/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Wayland compatibility header, it makes Firefox build with
   wayland-1.2 and Gtk+ 3.10.
*/

#ifndef __MozWayland_h_
#define __MozWayland_h_

#include "mozilla/Types.h"
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

#ifdef __cplusplus
extern "C" {
#endif

MOZ_EXPORT int wl_display_roundtrip_queue(struct wl_display* display,
                                          struct wl_event_queue* queue);
MOZ_EXPORT uint32_t wl_proxy_get_version(struct wl_proxy* proxy);
MOZ_EXPORT struct wl_proxy* wl_proxy_marshal_constructor(
    struct wl_proxy* proxy, uint32_t opcode,
    const struct wl_interface* interface, ...);

MOZ_EXPORT void* wl_proxy_create_wrapper(void* proxy);
MOZ_EXPORT void wl_proxy_wrapper_destroy(void* proxy_wrapper);

/* We need implement some missing functions from wayland-client-protocol.h
 */
#ifndef WL_DATA_DEVICE_MANAGER_DND_ACTION_ENUM
enum wl_data_device_manager_dnd_action {
  WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE = 0,
  WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY = 1,
  WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE = 2,
  WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK = 4
};
#endif

#ifndef WL_DATA_OFFER_SET_ACTIONS
#  define WL_DATA_OFFER_SET_ACTIONS 4

struct moz_wl_data_offer_listener {
  void (*offer)(void* data, struct wl_data_offer* wl_data_offer,
                const char* mime_type);
  void (*source_actions)(void* data, struct wl_data_offer* wl_data_offer,
                         uint32_t source_actions);
  void (*action)(void* data, struct wl_data_offer* wl_data_offer,
                 uint32_t dnd_action);
};

static inline void wl_data_offer_set_actions(
    struct wl_data_offer* wl_data_offer, uint32_t dnd_actions,
    uint32_t preferred_action) {
  wl_proxy_marshal((struct wl_proxy*)wl_data_offer, WL_DATA_OFFER_SET_ACTIONS,
                   dnd_actions, preferred_action);
}
#else
typedef struct wl_data_offer_listener moz_wl_data_offer_listener;
#endif

#ifndef WL_SUBCOMPOSITOR_GET_SUBSURFACE
#  define WL_SUBCOMPOSITOR_GET_SUBSURFACE 1
struct wl_subcompositor;

// Emulate what mozilla header wrapper does - make the
// wl_subcompositor_interface always visible.
#  pragma GCC visibility push(default)
extern const struct wl_interface wl_subsurface_interface;
extern const struct wl_interface wl_subcompositor_interface;
#  pragma GCC visibility pop

#  define WL_SUBSURFACE_DESTROY 0
#  define WL_SUBSURFACE_SET_POSITION 1
#  define WL_SUBSURFACE_PLACE_ABOVE 2
#  define WL_SUBSURFACE_PLACE_BELOW 3
#  define WL_SUBSURFACE_SET_SYNC 4
#  define WL_SUBSURFACE_SET_DESYNC 5

static inline struct wl_subsurface* wl_subcompositor_get_subsurface(
    struct wl_subcompositor* wl_subcompositor, struct wl_surface* surface,
    struct wl_surface* parent) {
  struct wl_proxy* id;

  id = wl_proxy_marshal_constructor(
      (struct wl_proxy*)wl_subcompositor, WL_SUBCOMPOSITOR_GET_SUBSURFACE,
      &wl_subsurface_interface, NULL, surface, parent);

  return (struct wl_subsurface*)id;
}

static inline void wl_subsurface_set_position(
    struct wl_subsurface* wl_subsurface, int32_t x, int32_t y) {
  wl_proxy_marshal((struct wl_proxy*)wl_subsurface, WL_SUBSURFACE_SET_POSITION,
                   x, y);
}

static inline void wl_subsurface_set_desync(
    struct wl_subsurface* wl_subsurface) {
  wl_proxy_marshal((struct wl_proxy*)wl_subsurface, WL_SUBSURFACE_SET_DESYNC);
}

static inline void wl_subsurface_destroy(struct wl_subsurface* wl_subsurface) {
  wl_proxy_marshal((struct wl_proxy*)wl_subsurface, WL_SUBSURFACE_DESTROY);

  wl_proxy_destroy((struct wl_proxy*)wl_subsurface);
}
#endif

#ifndef WL_SURFACE_DAMAGE_BUFFER
#  define WL_SURFACE_DAMAGE_BUFFER 9

static inline void wl_surface_damage_buffer(struct wl_surface* wl_surface,
                                            int32_t x, int32_t y, int32_t width,
                                            int32_t height) {
  wl_proxy_marshal((struct wl_proxy*)wl_surface, WL_SURFACE_DAMAGE_BUFFER, x, y,
                   width, height);
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MozWayland_h_ */
