#ifndef WLR_SCREENCOPY_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define WLR_SCREENCOPY_UNSTABLE_V1_CLIENT_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <wayland-client-core.h>

struct zwlr_screencopy_manager_v1;
struct zwlr_screencopy_frame_v1;
struct wl_output;
struct wl_buffer;

extern const struct wl_interface zwlr_screencopy_manager_v1_interface;
extern const struct wl_interface zwlr_screencopy_frame_v1_interface;

#define ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT 0
#define ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT_REGION 1
#define ZWLR_SCREENCOPY_MANAGER_V1_DESTROY 2

#define ZWLR_SCREENCOPY_FRAME_V1_COPY 0
#define ZWLR_SCREENCOPY_FRAME_V1_DESTROY 1

struct zwlr_screencopy_manager_v1_listener {
    void (*dummy)(void);
};

struct zwlr_screencopy_frame_v1_listener {
    void (*buffer)(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride);
    void (*buffer_done)(void *data, struct zwlr_screencopy_frame_v1 *frame);
    void (*linux_dmabuf)(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width, uint32_t height, uint32_t modifier_hi, uint32_t modifier_lo);
    void (*ready)(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec, uint32_t flags);
    void (*failed)(void *data, struct zwlr_screencopy_frame_v1 *frame);
    void (*damage)(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
};

static inline struct zwlr_screencopy_frame_v1 *
zwlr_screencopy_manager_v1_capture_output(struct zwlr_screencopy_manager_v1 *manager, uint32_t overlay_cursor, struct wl_output *output) {
    return (struct zwlr_screencopy_frame_v1 *) wl_proxy_marshal_constructor((struct wl_proxy *) manager, ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT, &zwlr_screencopy_frame_v1_interface, NULL, overlay_cursor, output);
}

static inline struct zwlr_screencopy_frame_v1 *
zwlr_screencopy_manager_v1_capture_output_region(struct zwlr_screencopy_manager_v1 *manager, uint32_t overlay_cursor, struct wl_output *output, int32_t x, int32_t y, int32_t width, int32_t height) {
    return (struct zwlr_screencopy_frame_v1 *) wl_proxy_marshal_constructor((struct wl_proxy *) manager, ZWLR_SCREENCOPY_MANAGER_V1_CAPTURE_OUTPUT_REGION, &zwlr_screencopy_frame_v1_interface, NULL, overlay_cursor, output, x, y, width, height);
}

static inline void zwlr_screencopy_frame_v1_add_listener(struct zwlr_screencopy_frame_v1 *frame, const struct zwlr_screencopy_frame_v1_listener *listener, void *data) {
    wl_proxy_add_listener((struct wl_proxy *) frame, (void (**)(void)) listener, data);
}

static inline void zwlr_screencopy_manager_v1_destroy(struct zwlr_screencopy_manager_v1 *manager) {
    wl_proxy_marshal((struct wl_proxy *) manager, ZWLR_SCREENCOPY_MANAGER_V1_DESTROY);
    wl_proxy_destroy((struct wl_proxy *) manager);
}

static inline void zwlr_screencopy_frame_v1_copy(struct zwlr_screencopy_frame_v1 *frame, struct wl_buffer *buffer) {
    wl_proxy_marshal((struct wl_proxy *) frame, ZWLR_SCREENCOPY_FRAME_V1_COPY, buffer);
}

static inline void zwlr_screencopy_frame_v1_destroy(struct zwlr_screencopy_frame_v1 *frame) {
    wl_proxy_marshal((struct wl_proxy *) frame, ZWLR_SCREENCOPY_FRAME_V1_DESTROY);
    wl_proxy_destroy((struct wl_proxy *) frame);
}

#ifdef __cplusplus
}
#endif

#endif
