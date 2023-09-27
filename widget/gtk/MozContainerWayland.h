/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
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
  struct wl_surface* surface = nullptr;
  struct wl_subsurface* subsurface = nullptr;
  int subsurface_dx = 0;
  int subsurface_dy = 0;
  struct wl_egl_window* eglwindow = nullptr;
  struct wl_callback* frame_callback_handler = nullptr;
  struct wp_viewport* viewport = nullptr;
  struct wp_fractional_scale_v1* fractional_scale = nullptr;
  gboolean opaque_region_needs_updates = false;
  int opaque_region_corner_radius = 0;
  gboolean opaque_region_used = false;
  gboolean ready_to_draw = false;
  gboolean commit_to_parent = false;
  gboolean before_first_size_alloc = false;
  gboolean waiting_to_show = false;
  // Zero means no fractional scale set.
  double current_fractional_scale = 0.0;
  int buffer_scale = 1;
  std::vector<std::function<void(void)>> initial_draw_cbs;
  // mozcontainer is used from Compositor and Rendering threads so we need to
  // control access to mozcontainer where wayland internals are used directly.
  mozilla::Mutex container_lock{"MozContainerWayland::container_lock"};
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

void moz_container_wayland_map(GtkWidget*);
gboolean moz_container_wayland_map_event(GtkWidget*, GdkEventAny*);
void moz_container_wayland_size_allocate(GtkWidget*, GtkAllocation*);
void moz_container_wayland_unmap(GtkWidget*);

struct wl_egl_window* moz_container_wayland_get_egl_window(
    MozContainer* container, double scale);

gboolean moz_container_wayland_has_egl_window(MozContainer* container);
bool moz_container_wayland_egl_window_set_size(MozContainer* container,
                                               nsIntSize aSize, int aScale);
void moz_container_wayland_set_scale_factor_locked(
    const mozilla::MutexAutoLock& aProofOfLock, MozContainer* container,
    int aScale);
bool moz_container_wayland_size_matches_scale_factor_locked(
    const mozilla::MutexAutoLock& aProofOfLock, MozContainer* container,
    int aWidth, int aHeight);

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
double moz_container_wayland_get_fractional_scale(MozContainer* container);
void moz_container_wayland_set_commit_to_parent(MozContainer* container);
bool moz_container_wayland_is_commiting_to_parent(MozContainer* container);
bool moz_container_wayland_is_waiting_to_show(MozContainer* container);
void moz_container_wayland_clear_waiting_to_show_flag(MozContainer* container);

#endif /* __MOZ_CONTAINER_WAYLAND_H__ */
