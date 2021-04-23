/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MOZ_CONTAINER_WAYLAND_H__
#define __MOZ_CONTAINER_WAYLAND_H__

#include <gtk/gtk.h>
#include <functional>
#include <vector>
#include "mozilla/Mutex.h"

/*
 * MozContainer
 *
 * This class serves three purposes in the nsIWidget implementation.
 *
 *   - It provides objects to receive signals from GTK for events on native
 *     windows.
 *
 *   - It provides GdkWindow to draw content on Wayland or when Gtk+ renders
 *     client side decorations to mShell.
 */

/* Workaround for bug at wayland-util.h,
 * present in wayland-devel < 1.12
 */
struct wl_surface;
struct wl_subsurface;

struct MozContainerWayland {
  struct wl_surface* surface;
  struct wl_subsurface* subsurface;
  int subsurface_dx, subsurface_dy;
  struct wl_egl_window* eglwindow;
  struct wl_callback* frame_callback_handler;
  gboolean opaque_region_needs_updates;
  gboolean opaque_region_subtract_corners;
  gboolean opaque_region_used;
  gboolean surface_needs_clear;
  gboolean ready_to_draw;
  gboolean before_first_size_alloc;
  int buffer_scale;
  std::vector<std::function<void(void)>> initial_draw_cbs;
  // mozcontainer is used from Compositor and Rendering threads
  // so we need to control access to mozcontainer where wayland internals
  // are used directly.
  mozilla::Mutex* container_lock;
};

struct _MozContainer;
struct _MozContainerClass;
typedef struct _MozContainer MozContainer;
typedef struct _MozContainerClass MozContainerClass;

void moz_container_wayland_class_init(MozContainerClass* klass);
void moz_container_wayland_init(MozContainerWayland* container);

struct wl_surface* moz_container_wayland_surface_lock(MozContainer* container);
void moz_container_wayland_surface_unlock(MozContainer* container,
                                          struct wl_surface** surface);

struct wl_egl_window* moz_container_wayland_get_egl_window(
    MozContainer* container, int scale);

gboolean moz_container_wayland_has_egl_window(MozContainer* container);
gboolean moz_container_wayland_surface_needs_clear(MozContainer* container);
void moz_container_wayland_egl_window_set_size(MozContainer* container,
                                               int width, int height);
void moz_container_wayland_set_scale_factor(MozContainer* container);
void moz_container_wayland_add_initial_draw_callback(
    MozContainer* container, const std::function<void(void)>& initial_draw_cb);
wl_surface* moz_gtk_widget_get_wl_surface(GtkWidget* aWidget);
void moz_container_wayland_update_opaque_region(MozContainer* container,
                                                bool aSubtractCorners);
gboolean moz_container_wayland_can_draw(MozContainer* container);

#endif /* __MOZ_CONTAINER_WAYLAND_H__ */
