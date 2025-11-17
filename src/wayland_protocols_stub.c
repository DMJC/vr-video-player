#include <wayland-util.h>
#include <wayland-client-protocol.h>
#include "../include/wlr-screencopy-unstable-v1-client-protocol.h"
#include "../include/xdg-output-unstable-v1-client-protocol.h"

static const struct wl_message zwlr_screencopy_manager_v1_requests[] = {
    {"capture_output", "nuo", (const struct wl_interface * const[]){&zwlr_screencopy_frame_v1_interface, NULL, &wl_output_interface}},
    {"capture_output_region", "nuoiiii", (const struct wl_interface * const[]){&zwlr_screencopy_frame_v1_interface, NULL, &wl_output_interface, NULL, NULL, NULL, NULL}},
    {"destroy", "", NULL},
};

const struct wl_interface zwlr_screencopy_manager_v1_interface = {
    "zwlr_screencopy_manager_v1", 3,
    3, zwlr_screencopy_manager_v1_requests,
    0, NULL
};

static const struct wl_message zwlr_screencopy_frame_v1_requests[] = {
    {"copy", "o", (const struct wl_interface * const[]){&wl_buffer_interface}},
    {"destroy", "", NULL},
};

static const struct wl_message zwlr_screencopy_frame_v1_events[] = {
    {"buffer", "uuuu", NULL},
    {"buffer_done", "", NULL},
    {"linux_dmabuf", "uiiuu", NULL},
    {"ready", "uuuu", NULL},
    {"failed", "", NULL},
    {"damage", "uuuu", NULL},
};

const struct wl_interface zwlr_screencopy_frame_v1_interface = {
    "zwlr_screencopy_frame_v1", 3,
    2, zwlr_screencopy_frame_v1_requests,
    6, zwlr_screencopy_frame_v1_events
};

static const struct wl_message zxdg_output_manager_v1_requests[] = {
    {"destroy", "", NULL},
    {"get_xdg_output", "no", (const struct wl_interface * const[]){&zxdg_output_v1_interface, &wl_output_interface}},
};

const struct wl_interface zxdg_output_manager_v1_interface = {
    "zxdg_output_manager_v1", 3,
    2, zxdg_output_manager_v1_requests,
    0, NULL
};

static const struct wl_message zxdg_output_v1_requests[] = {
    {"destroy", "", NULL},
};

static const struct wl_message zxdg_output_v1_events[] = {
    {"logical_position", "ii", NULL},
    {"logical_size", "ii", NULL},
    {"done", "", NULL},
    {"name", "s", NULL},
    {"description", "s", NULL},
};

const struct wl_interface zxdg_output_v1_interface = {
    "zxdg_output_v1", 3,
    1, zxdg_output_v1_requests,
    5, zxdg_output_v1_events
};

