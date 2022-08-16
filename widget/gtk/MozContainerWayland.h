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
#include "WindowSurface.h"

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
  struct wp_viewport* viewport;
  gboolean opaque_region_needs_updates;
  int opaque_region_corner_radius;
  gboolean opaque_region_used;
  gboolean ready_to_draw;
  gboolean commit_to_parent;
  gboolean before_first_size_alloc;
  gboolean waiting_to_show;
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

class MozContainerSurfaceLock {
  MozContainer* mContainer;
  struct wl_surface* mSurface;

 public:
  explicit MozContainerSurfaceLock(MozContainer* aContainer);
  ~MozContainerSurfaceLock();
  struct wl_surface* GetSurface();
};

void moz_container_wayland_class_init(MozContainerClass* klass);
void moz_container_wayland_init(MozContainerWayland* container);

struct wl_egl_window* moz_container_wayland_get_egl_window(
    MozContainer* container, double scale);

gboolean moz_container_wayland_has_egl_window(MozContainer* container);
void moz_container_wayland_egl_window_set_size(MozContainer* container,
                                               int width, int height);
void moz_container_wayland_set_scale_factor(MozContainer* container);
void moz_container_wayland_set_scale_factor_locked(MozContainer* container);

void moz_container_wayland_add_initial_draw_callback_locked(
    MozContainer* container, const std::function<void(void)>& initial_draw_cb);
void moz_container_wayland_add_or_fire_initial_draw_callback(
    MozContainer* container, const std::function<void(void)>& initial_draw_cb);
void moz_container_wayland_clear_initial_draw_callback(MozContainer* container);

wl_surface* moz_gtk_widget_get_wl_surface(GtkWidget* aWidget);
void moz_container_wayland_update_opaque_region(MozContainer* container,
                                                int corner_radius);
gboolean moz_container_wayland_can_draw(MozContainer* container);
double moz_container_wayland_get_scale(MozContainer* container);
void moz_container_wayland_set_commit_to_parent(MozContainer* container);
bool moz_container_wayland_is_commiting_to_parent(MozContainer* container);
bool moz_container_wayland_is_waiting_to_show(MozContainer* container);
void moz_container_wayland_clear_waiting_to_show_flag(MozContainer* container);

#endif /* __MOZ_CONTAINER_WAYLAND_H__ */
