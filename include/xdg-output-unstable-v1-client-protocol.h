#ifndef XDG_OUTPUT_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define XDG_OUTPUT_UNSTABLE_V1_CLIENT_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <wayland-client-core.h>

struct wl_output;
struct zxdg_output_manager_v1;
struct zxdg_output_v1;

extern const struct wl_interface zxdg_output_manager_v1_interface;
extern const struct wl_interface zxdg_output_v1_interface;

#define ZXDG_OUTPUT_MANAGER_V1_DESTROY 0
#define ZXDG_OUTPUT_MANAGER_V1_GET_XDG_OUTPUT 1

#define ZXDG_OUTPUT_V1_DESTROY 0

struct zxdg_output_v1_listener {
    void (*logical_position)(void *data, struct zxdg_output_v1 *zxdg_output_v1, int32_t x, int32_t y);
    void (*logical_size)(void *data, struct zxdg_output_v1 *zxdg_output_v1, int32_t width, int32_t height);
    void (*done)(void *data, struct zxdg_output_v1 *zxdg_output_v1);
    void (*name)(void *data, struct zxdg_output_v1 *zxdg_output_v1, const char *name);
    void (*description)(void *data, struct zxdg_output_v1 *zxdg_output_v1, const char *description);
};

static inline void zxdg_output_v1_add_listener(struct zxdg_output_v1 *output, const struct zxdg_output_v1_listener *listener, void *data) {
    wl_proxy_add_listener((struct wl_proxy *) output, (void (**)(void)) listener, data);
}

static inline void zxdg_output_manager_v1_destroy(struct zxdg_output_manager_v1 *manager) {
    wl_proxy_marshal((struct wl_proxy *) manager, ZXDG_OUTPUT_MANAGER_V1_DESTROY);
    wl_proxy_destroy((struct wl_proxy *) manager);
}

static inline struct zxdg_output_v1 *
zxdg_output_manager_v1_get_xdg_output(struct zxdg_output_manager_v1 *manager, struct wl_output *output) {
    return (struct zxdg_output_v1 *) wl_proxy_marshal_constructor((struct wl_proxy *) manager, ZXDG_OUTPUT_MANAGER_V1_GET_XDG_OUTPUT, &zxdg_output_v1_interface, NULL, output);
}

static inline void zxdg_output_v1_destroy(struct zxdg_output_v1 *output) {
    wl_proxy_marshal((struct wl_proxy *) output, ZXDG_OUTPUT_V1_DESTROY);
    wl_proxy_destroy((struct wl_proxy *) output);
}

#ifdef __cplusplus
}
#endif

#endif
