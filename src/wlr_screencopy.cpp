#include "wlr_screencopy.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <linux/memfd.h>
#include <sys/syscall.h>
#include <cstring>
#include <iostream>

static int create_shm_file(size_t size) {
    int fd = syscall(SYS_memfd_create, "wlr-screencopy", MFD_CLOEXEC);
    if (fd < 0) {
        perror("memfd_create");
        return -1;
    }
    if (ftruncate(fd, size) < 0) {
        perror("ftruncate");
        close(fd);
        return -1;
    }
    return fd;
}

static const wl_registry_listener registry_listener = {
    .global = WlrScreencopy::registry_global,
    .global_remove = WlrScreencopy::registry_global_remove,
};

static const zxdg_output_v1_listener xdg_output_listener = {
    .logical_position = WlrScreencopy::xdg_output_logical_position,
    .logical_size = WlrScreencopy::xdg_output_logical_size,
    .done = WlrScreencopy::xdg_output_done,
    .name = WlrScreencopy::xdg_output_name,
    .description = WlrScreencopy::xdg_output_description,
};

static const zwlr_screencopy_frame_v1_listener frame_listener = {
    .buffer = WlrScreencopy::frame_buffer,
    .buffer_done = WlrScreencopy::frame_buffer_done,
    .linux_dmabuf = WlrScreencopy::frame_linux_dmabuf,
    .ready = WlrScreencopy::frame_ready,
    .failed = WlrScreencopy::frame_failed,
    .damage = WlrScreencopy::frame_damage,
};

WlrScreencopy::WlrScreencopy() {}

WlrScreencopy::~WlrScreencopy() { shutdown(); }

bool WlrScreencopy::init(const std::string &output_name, int target_fps) {
    desired_output_name = output_name;
    frame_interval_ms = 1000.0 / static_cast<double>(target_fps);

    if (!setup_wayland()) {
        return false;
    }

    running = true;
    worker = std::thread(&WlrScreencopy::capture_loop, this);
    initialized = true;
    return true;
}

void WlrScreencopy::shutdown() {
    running = false;
    if (display) {
        wl_display_flush(display);
    }
    if (worker.joinable()) {
        worker.join();
    }
    destroy_buffer();
    destroy_wayland();
}

bool WlrScreencopy::has_frame() const { return frame_available; }

void WlrScreencopy::upload_to_texture(GLuint texture_id) {
    std::vector<uint8_t> copy;
    int w = 0, h = 0;
    int stride = 0;
    bool do_dump = false;
    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        if (!frame_available)
            return;
        copy = frame_data;
        w = frame_width;
        h = frame_height;
        stride = frame_stride;
        if (!first_frame_dumped) {
            first_frame_dumped = true;
            do_dump = true;
        }
        frame_available = false;
    }
    if (do_dump) {
        dump_first_frame(copy, w, h, stride);
    }
    if (w <= 0 || h <= 0 || stride <= 0)
        return;

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, copy.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

bool WlrScreencopy::setup_wayland() {
    display = wl_display_connect(nullptr);
    if (!display)
        return false;

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);

    if (!shm || !screencopy_manager || !xdg_output_manager) {
        return false;
    }

    // Ensure target output exists
    if (!target_output.output) {
        // fall back to the first enumerated output if the requested
        // name was not found.
        for (const auto &entry : output_map) {
            if (entry.second) {
                target_output.output = entry.second;
                break;
            }
        }
    }

    if (!target_output.output) {
        return false;
    }

    return true;
}

void WlrScreencopy::destroy_wayland() {
    if (target_output.xdg_output) {
        zxdg_output_v1_destroy(target_output.xdg_output);
        target_output.xdg_output = nullptr;
    }
    if (target_output.output) {
        wl_output_destroy(target_output.output);
        target_output.output = nullptr;
    }
    if (screencopy_manager) {
        zwlr_screencopy_manager_v1_destroy(screencopy_manager);
        screencopy_manager = nullptr;
    }
    if (xdg_output_manager) {
        zxdg_output_manager_v1_destroy(xdg_output_manager);
        xdg_output_manager = nullptr;
    }
    if (shm) {
        wl_shm_destroy(shm);
        shm = nullptr;
    }
    if (registry) {
        wl_registry_destroy(registry);
        registry = nullptr;
    }
    if (display) {
        wl_display_disconnect(display);
        display = nullptr;
    }
}

bool WlrScreencopy::ensure_buffer(int32_t width, int32_t height, int32_t stride, uint32_t format, int32_t offset) {
    if (buffer.buffer && buffer.width == width && buffer.height == height && buffer.stride == stride && buffer.format == format && buffer.offset == offset) {
        return true;
    }

    destroy_buffer();

    size_t size = static_cast<size_t>(offset) + static_cast<size_t>(height) * stride;
    int fd = create_shm_file(size);
    if (fd < 0)
        return false;

    void *data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return false;
    }

    wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    wl_buffer *wlbuf = wl_shm_pool_create_buffer(pool, offset, width, height, stride, format);

    buffer.pool = pool;
    buffer.buffer = wlbuf;
    buffer.fd = fd;
    buffer.size = size;
    buffer.width = width;
    buffer.height = height;
    buffer.stride = stride;
    buffer.offset = offset;
    buffer.format = format;
    buffer.data = data;
    return true;
}

void WlrScreencopy::destroy_buffer() {
    if (buffer.buffer) {
        wl_buffer_destroy(buffer.buffer);
        buffer.buffer = nullptr;
    }
    if (buffer.pool) {
        wl_shm_pool_destroy(buffer.pool);
        buffer.pool = nullptr;
    }
    if (buffer.data && buffer.data != MAP_FAILED) {
        munmap(buffer.data, buffer.size);
        buffer.data = nullptr;
    }
    if (buffer.fd >= 0) {
        close(buffer.fd);
        buffer.fd = -1;
    }
    buffer.size = 0;
    buffer.width = buffer.height = buffer.stride = 0;
    buffer.offset = 0;
    buffer.format = 0;
}

void WlrScreencopy::request_frame() {
    if (pending_frame || !running.load())
        return;
    pending_frame = true;
    zwlr_screencopy_frame_v1 *frame = zwlr_screencopy_manager_v1_capture_output(screencopy_manager, 0, target_output.output);
    zwlr_screencopy_frame_v1_add_listener(frame, &frame_listener, this);
    wl_display_flush(display);
}

void WlrScreencopy::handle_ready(uint32_t, uint32_t, uint32_t) {
    if (!buffer.buffer)
        return;

    size_t size = static_cast<size_t>(buffer.height) * buffer.stride;
    {
        std::lock_guard<std::mutex> lock(frame_mutex);
        uint8_t *start = static_cast<uint8_t *>(buffer.data) + buffer.offset;
        frame_data.assign(start, start + size);
        frame_width = buffer.width;
        frame_height = buffer.height;
        frame_stride = buffer.stride;
        frame_available = true;
    }
    pending_frame = false;
}

void WlrScreencopy::handle_failed() {
    pending_frame = false;
}

void WlrScreencopy::capture_loop() {
    request_frame();
    auto next_time = std::chrono::steady_clock::now();
    while (running.load()) {
        if (wl_display_dispatch(display) < 0) {
            break;
        }

        if (!pending_frame.load()) {
            auto now = std::chrono::steady_clock::now();
            if (now >= next_time) {
                request_frame();
                next_time = now + std::chrono::milliseconds(static_cast<int>(frame_interval_ms));
            }
        }
    }
}

void WlrScreencopy::registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    auto *self = static_cast<WlrScreencopy *>(data);
    if (strcmp(interface, wl_shm_interface.name) == 0) {
        self->shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        self->screencopy_manager = static_cast<zwlr_screencopy_manager_v1 *>(wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, 1));
    } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        self->xdg_output_manager = static_cast<zxdg_output_manager_v1 *>(wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 3));
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        wl_output *output = static_cast<wl_output *>(wl_registry_bind(registry, name, &wl_output_interface, version >= 3 ? 3 : version));
        if (!self->xdg_output_manager)
            return;
        zxdg_output_v1 *xdg_output = zxdg_output_manager_v1_get_xdg_output(self->xdg_output_manager, output);
        zxdg_output_v1_add_listener(xdg_output, &xdg_output_listener, self);
        self->output_map[xdg_output] = output;
    }
}

void WlrScreencopy::registry_global_remove(void *, wl_registry *, uint32_t) {}

void WlrScreencopy::xdg_output_name(void *data, zxdg_output_v1 *output, const char *name) {
    auto *self = static_cast<WlrScreencopy *>(data);
    wl_output *wl_out = nullptr;
    auto it = self->output_map.find(output);
    if (it != self->output_map.end()) {
        wl_out = it->second;
    }

    if (self->desired_output_name == name) {
        self->target_output.output = wl_out;
        self->target_output.xdg_output = output;
    } else if (!self->target_output.output) {
        // default to the first enumerated output until a matching
        // one is found.
        self->target_output.output = wl_out;
        self->target_output.xdg_output = output;
    }

    if (self->target_output.xdg_output == output) {
        self->target_output.name = name;
    }
}

void WlrScreencopy::xdg_output_logical_position(void *, zxdg_output_v1 *, int32_t, int32_t) {}

void WlrScreencopy::xdg_output_logical_size(void *data, zxdg_output_v1 *output, int32_t width, int32_t height) {
    auto *self = static_cast<WlrScreencopy *>(data);
    if (self->target_output.xdg_output == output) {
        self->target_output.width = width;
        self->target_output.height = height;
    }
}

void WlrScreencopy::xdg_output_done(void *data, zxdg_output_v1 *output) {
    auto *self = static_cast<WlrScreencopy *>(data);
    if (self->target_output.xdg_output != output)
        return;
}

void WlrScreencopy::xdg_output_description(void *, zxdg_output_v1 *, const char *) {}

void WlrScreencopy::frame_buffer(void *data, zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
    auto *self = static_cast<WlrScreencopy *>(data);
    if (!self->ensure_buffer(width, height, stride, format, 0)) {
        zwlr_screencopy_frame_v1_destroy(frame);
        self->pending_frame = false;
        return;
    }
    zwlr_screencopy_frame_v1_copy(frame, self->buffer.buffer);
}

void WlrScreencopy::frame_linux_dmabuf(void *data, zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride, uint32_t, uint32_t) {
    auto *self = static_cast<WlrScreencopy *>(data);
    // Fallback to shm copy
    const uint32_t resolved_stride = stride ? stride : width * 4;
    if (!self->ensure_buffer(width, height, resolved_stride, format, 0)) {
        zwlr_screencopy_frame_v1_destroy(frame);
        self->pending_frame = false;
        return;
    }
    zwlr_screencopy_frame_v1_copy(frame, self->buffer.buffer);
}

void WlrScreencopy::frame_ready(void *data, zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec, uint32_t) {
    auto *self = static_cast<WlrScreencopy *>(data);
    self->handle_ready(tv_sec_hi, tv_sec_lo, tv_nsec);
    zwlr_screencopy_frame_v1_destroy(frame);
}

void WlrScreencopy::frame_failed(void *data, zwlr_screencopy_frame_v1 *frame) {
    auto *self = static_cast<WlrScreencopy *>(data);
    self->handle_failed();
    zwlr_screencopy_frame_v1_destroy(frame);
}

void WlrScreencopy::frame_damage(void *, zwlr_screencopy_frame_v1 *, uint32_t, uint32_t, uint32_t, uint32_t) {}

void WlrScreencopy::frame_buffer_done(void *, zwlr_screencopy_frame_v1 *) {}

void WlrScreencopy::dump_first_frame(const std::vector<uint8_t> &data, int width, int height, int stride) {
    if (first_frame_dumped || data.empty() || width <= 0 || height <= 0 || stride <= 0)
        return;

    FILE *file = fopen("wlr_first_frame.ppm", "wb");
    if (!file) {
        std::cerr << "Failed to write wlr_first_frame.ppm" << std::endl;
        first_frame_dumped = true;
        return;
    }

    fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (int y = 0; y < height; ++y) {
        const uint8_t *row = data.data() + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const uint8_t *px = row + static_cast<size_t>(x) * 4;
            unsigned char rgb[3] = {px[2], px[1], px[0]};
            fwrite(rgb, 1, 3, file);
        }
    }
    fclose(file);
    std::cerr << "Wrote first screencopy frame to wlr_first_frame.ppm (" << width << "x" << height << ")" << std::endl;
}

