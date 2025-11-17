#pragma once

#include <wayland-client.h>
#include <GL/glew.h>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <stdint.h>
#include <chrono>

#include "../include/wlr-screencopy-unstable-v1-client-protocol.h"
#include "../include/xdg-output-unstable-v1-client-protocol.h"

class WlrScreencopy {
public:
    WlrScreencopy();
    ~WlrScreencopy();

    bool init(const std::string &output_name, int target_fps);
    void shutdown();

    bool has_frame() const;
    void upload_to_texture(GLuint texture_id);
    int width() const { return frame_width; }
    int height() const { return frame_height; }

    static void registry_global(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void registry_global_remove(void *data, wl_registry *registry, uint32_t name);
    static void xdg_output_name(void *data, zxdg_output_v1 *output, const char *name);
    static void xdg_output_logical_position(void *data, zxdg_output_v1 *output, int32_t, int32_t);
    static void xdg_output_logical_size(void *data, zxdg_output_v1 *output, int32_t width, int32_t height);
    static void xdg_output_done(void *data, zxdg_output_v1 *output);
    static void xdg_output_description(void *data, zxdg_output_v1 *output, const char *description);

    static void frame_buffer(void *data, zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride);
    static void frame_linux_dmabuf(void *data, zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width, uint32_t height);
    static void frame_ready(void *data, zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
    static void frame_failed(void *data, zwlr_screencopy_frame_v1 *frame);
    static void frame_damage(void *data, zwlr_screencopy_frame_v1 *frame, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    static void frame_buffer_done(void *data, zwlr_screencopy_frame_v1 *frame);

private:
    struct OutputInfo {
        wl_output *output = nullptr;
        zxdg_output_v1 *xdg_output = nullptr;
        std::string name;
        int32_t width = 0;
        int32_t height = 0;
    };

    struct ShmBuffer {
        wl_shm_pool *pool = nullptr;
        wl_buffer *buffer = nullptr;
        int fd = -1;
        size_t size = 0;
        int32_t width = 0;
        int32_t height = 0;
        int32_t stride = 0;
        uint32_t format = 0;
        void *data = nullptr;
    };

    void capture_loop();
    bool setup_wayland();
    void destroy_wayland();

    bool ensure_buffer(int32_t width, int32_t height, int32_t stride, uint32_t format);
    void destroy_buffer();
    void request_frame();
    void handle_ready(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
    void handle_failed();

    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_shm *shm = nullptr;
    zxdg_output_manager_v1 *xdg_output_manager = nullptr;
    zwlr_screencopy_manager_v1 *screencopy_manager = nullptr;

    OutputInfo target_output;
    std::string desired_output_name = "DP-3";

    ShmBuffer buffer;

    std::thread worker;
    std::atomic<bool> running{false};
    std::atomic<bool> pending_frame{false};
    std::atomic<bool> initialized{false};

    std::mutex frame_mutex;
    std::vector<uint8_t> frame_data;
    bool frame_available = false;
    int frame_width = 0;
    int frame_height = 0;

    double frame_interval_ms = 1000.0 / 90.0;
};

