/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "mozilla/Types.h"
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <gdk/gdkwayland.h>

union wl_argument;

/* Those structures are just placeholders and will be replaced by
 * real symbols from libwayland during run-time linking. We need to make
 * them explicitly visible.
 */
#pragma GCC visibility push(default)
const struct wl_interface wl_buffer_interface;
const struct wl_interface wl_callback_interface;
const struct wl_interface wl_data_device_interface;
const struct wl_interface wl_data_device_manager_interface;
const struct wl_interface wl_keyboard_interface;
const struct wl_interface wl_pointer_interface;
const struct wl_interface wl_region_interface;
const struct wl_interface wl_registry_interface;
const struct wl_interface wl_shm_interface;
const struct wl_interface wl_shm_pool_interface;
const struct wl_interface wl_seat_interface;
const struct wl_interface wl_surface_interface;
const struct wl_interface wl_subsurface_interface;
const struct wl_interface wl_compositor_interface;
const struct wl_interface wl_subcompositor_interface;
const struct wl_interface wl_output_interface;
#pragma GCC visibility pop

MOZ_EXPORT void wl_event_queue_destroy(struct wl_event_queue* queue) {}

MOZ_EXPORT void wl_proxy_marshal(struct wl_proxy* p, uint32_t opcode, ...) {}

MOZ_EXPORT void wl_proxy_marshal_array(struct wl_proxy* p, uint32_t opcode,
                                       union wl_argument* args) {}

MOZ_EXPORT struct wl_proxy* wl_proxy_create(
    struct wl_proxy* factory, const struct wl_interface* interface) {
  return NULL;
}

MOZ_EXPORT void* wl_proxy_create_wrapper(void* proxy) { return NULL; }

MOZ_EXPORT void wl_proxy_wrapper_destroy(void* proxy_wrapper) {}

MOZ_EXPORT struct wl_proxy* wl_proxy_marshal_constructor(
    struct wl_proxy* proxy, uint32_t opcode,
    const struct wl_interface* interface, ...) {
  return NULL;
}

MOZ_EXPORT struct wl_proxy* wl_proxy_marshal_constructor_versioned(
    struct wl_proxy* proxy, uint32_t opcode,
    const struct wl_interface* interface, uint32_t version, ...) {
  return NULL;
}

MOZ_EXPORT struct wl_proxy* wl_proxy_marshal_array_constructor(
    struct wl_proxy* proxy, uint32_t opcode, union wl_argument* args,
    const struct wl_interface* interface) {
  return NULL;
}

MOZ_EXPORT struct wl_proxy* wl_proxy_marshal_array_constructor_versioned(
    struct wl_proxy* proxy, uint32_t opcode, union wl_argument* args,
    const struct wl_interface* interface, uint32_t version) {
  return NULL;
}

MOZ_EXPORT void wl_proxy_destroy(struct wl_proxy* proxy) {}

MOZ_EXPORT int wl_proxy_add_listener(struct wl_proxy* proxy,
                                     void (**implementation)(void),
                                     void* data) {
  return -1;
}

MOZ_EXPORT const void* wl_proxy_get_listener(struct wl_proxy* proxy) {
  return NULL;
}

typedef int (*wl_dispatcher_func_t)(const void*, void*, uint32_t,
                                    const struct wl_message*,
                                    union wl_argument*);

MOZ_EXPORT int wl_proxy_add_dispatcher(struct wl_proxy* proxy,
                                       wl_dispatcher_func_t dispatcher_func,
                                       const void* dispatcher_data,
                                       void* data) {
  return -1;
}

MOZ_EXPORT void wl_proxy_set_user_data(struct wl_proxy* proxy,
                                       void* user_data) {}

MOZ_EXPORT void* wl_proxy_get_user_data(struct wl_proxy* proxy) { return NULL; }

MOZ_EXPORT uint32_t wl_proxy_get_version(struct wl_proxy* proxy) { return -1; }

MOZ_EXPORT uint32_t wl_proxy_get_id(struct wl_proxy* proxy) { return -1; }

MOZ_EXPORT const char* wl_proxy_get_class(struct wl_proxy* proxy) {
  return NULL;
}

MOZ_EXPORT void wl_proxy_set_queue(struct wl_proxy* proxy,
                                   struct wl_event_queue* queue) {}

MOZ_EXPORT struct wl_display* wl_display_connect(const char* name) {
  return NULL;
}

MOZ_EXPORT struct wl_display* wl_display_connect_to_fd(int fd) { return NULL; }

MOZ_EXPORT void wl_display_disconnect(struct wl_display* display) {}

MOZ_EXPORT int wl_display_get_fd(struct wl_display* display) { return -1; }

MOZ_EXPORT int wl_display_dispatch(struct wl_display* display) { return -1; }

MOZ_EXPORT int wl_display_dispatch_queue(struct wl_display* display,
                                         struct wl_event_queue* queue) {
  return -1;
}

MOZ_EXPORT int wl_display_dispatch_queue_pending(struct wl_display* display,
                                                 struct wl_event_queue* queue) {
  return -1;
}

MOZ_EXPORT int wl_display_dispatch_pending(struct wl_display* display) {
  return -1;
}

MOZ_EXPORT int wl_display_get_error(struct wl_display* display) { return -1; }

MOZ_EXPORT uint32_t wl_display_get_protocol_error(
    struct wl_display* display, const struct wl_interface** interface,
    uint32_t* id) {
  return -1;
}

MOZ_EXPORT int wl_display_flush(struct wl_display* display) { return -1; }

MOZ_EXPORT int wl_display_roundtrip_queue(struct wl_display* display,
                                          struct wl_event_queue* queue) {
  return -1;
}

MOZ_EXPORT int wl_display_roundtrip(struct wl_display* display) { return -1; }

MOZ_EXPORT struct wl_event_queue* wl_display_create_queue(
    struct wl_display* display) {
  return NULL;
}

MOZ_EXPORT int wl_display_prepare_read_queue(struct wl_display* display,
                                             struct wl_event_queue* queue) {
  return -1;
}

MOZ_EXPORT int wl_display_prepare_read(struct wl_display* display) {
  return -1;
}

MOZ_EXPORT void wl_display_cancel_read(struct wl_display* display) {}

MOZ_EXPORT int wl_display_read_events(struct wl_display* display) { return -1; }

MOZ_EXPORT void wl_log_set_handler_client(wl_log_func_t handler) {}

MOZ_EXPORT struct wl_egl_window* wl_egl_window_create(
    struct wl_surface* surface, int width, int height) {
  return NULL;
}

MOZ_EXPORT void wl_egl_window_destroy(struct wl_egl_window* egl_window) {}

MOZ_EXPORT void wl_egl_window_resize(struct wl_egl_window* egl_window,
                                     int width, int height, int dx, int dy) {}

MOZ_EXPORT void wl_egl_window_get_attached_size(
    struct wl_egl_window* egl_window, int* width, int* height) {}

MOZ_EXPORT void wl_list_init(struct wl_list* list) {}

MOZ_EXPORT void wl_list_insert(struct wl_list* list, struct wl_list* elm) {}

MOZ_EXPORT void wl_list_remove(struct wl_list* elm) {}

MOZ_EXPORT int wl_list_length(const struct wl_list* list) { return -1; }

MOZ_EXPORT int wl_list_empty(const struct wl_list* list) { return -1; }

MOZ_EXPORT void wl_list_insert_list(struct wl_list* list,
                                    struct wl_list* other) {}

MOZ_EXPORT struct wl_proxy* wl_proxy_marshal_flags(
    struct wl_proxy* proxy, uint32_t opcode,
    const struct wl_interface* interface, uint32_t version, uint32_t flags,
    ...) {
  return NULL;
}
