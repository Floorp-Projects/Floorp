/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * MozContainerWayland is a wrapper over MozContainer which provides
 * wl_surface for MozContainer widget.
 *
 * The widget scheme looks like:
 *
 *   ---------------------------------------------------------
 *  |  mShell Gtk widget (contains wl_surface owned by Gtk+)  |
 *  |                                                         |
 *  |  ---------------------------------------------------    |
 *  | | mContainer (contains wl_surface owned by Gtk+)    |   |
 *  | |                                                   |   |
 *  | |  ---------------------------------------------    |   |
 *  | | | wl_subsurface (attached to wl_surface       |   |   |
 *  | | |                of mContainer)               |   |   |
 *  | | |                                             |   |   |
 *  | | |                                             |   |   |
 *  | |  ---------------------------------------------    |   |
 *  |  ---------------------------------------------------    |
 *   ---------------------------------------------------------
 *
 *  We draw to wl_subsurface owned by MozContainerWayland.
 *  We need to wait until wl_surface of mContainer is created
 *  and then we create and attach our wl_subsurface to it.
 *
 *  First wl_subsurface creation has these steps:
 *
 *  1) moz_container_wayland_size_allocate() handler is called when
 *     mContainer size/position is known.
 *     It calls moz_container_wayland_surface_create_locked(), registers
 *     a frame callback handler
 *     moz_container_wayland_frame_callback_handler().
 *
 *  2) moz_container_wayland_frame_callback_handler() is called
 *     when wl_surface owned by mozContainer is ready.
 *     We call initial_draw_cbs() handler and we can create our wl_subsurface
 *     on top of wl_surface owned by mozContainer.
 *
 *  When MozContainer hides/show again, moz_container_wayland_size_allocate()
 *  handler may not be called as MozContainer size is set. So after first
 *  show/hide sequence use moz_container_wayland_map_event() to create
 *  wl_subsurface of MozContainer.
 */

#include "MozContainer.h"

#include <dlfcn.h>
#include <glib.h>
#include <stdio.h>
#include <wayland-egl.h>

#include "mozilla/gfx/gfxVars.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsGtkUtils.h"
#include "nsWaylandDisplay.h"
#include "base/task.h"

#ifdef MOZ_LOGGING

#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
#  include "nsWindow.h"
extern mozilla::LazyLogModule gWidgetWaylandLog;
extern mozilla::LazyLogModule gWidgetLog;
#  define LOGWAYLAND(...) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#  define LOGCONTAINER(...) \
    MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#else
#  define LOGWAYLAND(...)
#  define LOGCONTAINER(...)
#endif /* MOZ_LOGGING */

using namespace mozilla;
using namespace mozilla::widget;

static bool moz_container_wayland_surface_create_locked(
    const MutexAutoLock& aProofOfLock, MozContainer* container);
static void moz_container_wayland_set_opaque_region_locked(
    const MutexAutoLock& aProofOfLock, MozContainer* container);

// Lock mozcontainer and get wayland surface of it. You need to pair with
// moz_container_wayland_surface_unlock() even
// if moz_container_wayland_surface_lock() fails and returns nullptr.
static struct wl_surface* moz_container_wayland_surface_lock(
    MozContainer* container);
static void moz_container_wayland_surface_unlock(MozContainer* container,
                                                 struct wl_surface** surface);

MozContainerSurfaceLock::MozContainerSurfaceLock(MozContainer* aContainer) {
  mContainer = aContainer;
  mSurface = moz_container_wayland_surface_lock(aContainer);
}
MozContainerSurfaceLock::~MozContainerSurfaceLock() {
  moz_container_wayland_surface_unlock(mContainer, &mSurface);
}
struct wl_surface* MozContainerSurfaceLock::GetSurface() { return mSurface; }

// Invalidate gtk wl_surface to commit changes to wl_subsurface.
// wl_subsurface changes are effective when parent surface is commited.
static void moz_container_wayland_invalidate(MozContainer* container) {
  LOGWAYLAND("moz_container_wayland_invalidate [%p]\n",
             (void*)moz_container_get_nsWindow(container));

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  if (!window) {
    LOGWAYLAND("    Failed - missing GdkWindow!\n");
    return;
  }
  gdk_window_invalidate_rect(window, nullptr, true);
}

// Route input to parent wl_surface owned by Gtk+ so we get input
// events from Gtk+.
static void moz_container_clear_input_region(MozContainer* container) {
  struct wl_compositor* compositor = WaylandDisplayGet()->GetCompositor();
  MozContainerWayland* wl_container = &container->data.wl_container;
  wl_region* region = wl_compositor_create_region(compositor);
  wl_surface_set_input_region(wl_container->surface, region);
  wl_region_destroy(region);
}

static void moz_container_wayland_move_locked(const MutexAutoLock& aProofOfLock,
                                              MozContainer* container, int dx,
                                              int dy) {
  LOGCONTAINER("moz_container_wayland_move [%p] %d,%d\n",
               (void*)moz_container_get_nsWindow(container), dx, dy);

  MozContainerWayland* wl_container = &container->data.wl_container;
  if (!wl_container->subsurface || (wl_container->subsurface_dx == dx &&
                                    wl_container->subsurface_dy == dy)) {
    return;
  }

  wl_container->subsurface_dx = dx;
  wl_container->subsurface_dy = dy;
  wl_subsurface_set_position(wl_container->subsurface,
                             wl_container->subsurface_dx,
                             wl_container->subsurface_dy);
}

// This is called from layout/compositor code only with
// size equal to GL rendering context.

// Return false if scale factor doesn't match buffer size.
// We need to skip painting in such case do avoid Wayland compositor freaking.
bool moz_container_wayland_egl_window_set_size(MozContainer* container,
                                               nsIntSize aSize, int aScale) {
  MozContainerWayland* wl_container = &container->data.wl_container;
  MutexAutoLock lock(wl_container->container_lock);
  if (!wl_container->eglwindow) {
    return false;
  }

  if (wl_container->buffer_scale != aScale) {
    moz_container_wayland_set_scale_factor_locked(lock, container, aScale);
  }

  LOGCONTAINER(
      "moz_container_wayland_egl_window_set_size [%p] %d x %d scale %d "
      "(unscaled %d x %d)",
      (void*)moz_container_get_nsWindow(container), aSize.width, aSize.height,
      aScale, aSize.width / aScale, aSize.height / aScale);
  wl_egl_window_resize(wl_container->eglwindow, aSize.width, aSize.height, 0,
                       0);

  return moz_container_wayland_size_matches_scale_factor_locked(
      lock, container, aSize.width, aSize.height);
}

void moz_container_wayland_add_initial_draw_callback_locked(
    MozContainer* container, const std::function<void(void)>& initial_draw_cb) {
  MozContainerWayland* wl_container = &container->data.wl_container;

  if (wl_container->ready_to_draw && !wl_container->surface) {
    NS_WARNING(
        "moz_container_wayland_add_or_fire_initial_draw_callback:"
        " ready to draw without wayland surface!");
  }
  MOZ_DIAGNOSTIC_ASSERT(!wl_container->ready_to_draw || !wl_container->surface);
  wl_container->initial_draw_cbs.push_back(initial_draw_cb);
}

void moz_container_wayland_add_or_fire_initial_draw_callback(
    MozContainer* container, const std::function<void(void)>& initial_draw_cb) {
  MozContainerWayland* wl_container = &container->data.wl_container;
  {
    MutexAutoLock lock(wl_container->container_lock);
    if (wl_container->ready_to_draw && !wl_container->surface) {
      NS_WARNING(
          "moz_container_wayland_add_or_fire_initial_draw_callback: ready to "
          "draw "
          "without wayland surface!");
    }
    if (!wl_container->ready_to_draw || !wl_container->surface) {
      wl_container->initial_draw_cbs.push_back(initial_draw_cb);
      return;
    }
  }

  // We're ready to draw as
  // wl_container->ready_to_draw && wl_container->surface
  // call the callback directly instead of store them.
  initial_draw_cb();
}

static void moz_container_wayland_clear_initial_draw_callback_locked(
    const MutexAutoLock& aProofOfLock, MozContainer* container) {
  MozContainerWayland* wl_container = &container->data.wl_container;
  MozClearPointer(wl_container->frame_callback_handler, wl_callback_destroy);
  wl_container->initial_draw_cbs.clear();
}

void moz_container_wayland_clear_initial_draw_callback(
    MozContainer* container) {
  MutexAutoLock lock(container->data.wl_container.container_lock);
  moz_container_wayland_clear_initial_draw_callback_locked(lock, container);
}

static void moz_container_wayland_frame_callback_handler(
    void* data, struct wl_callback* callback, uint32_t time) {
  MozContainerWayland* wl_container = MOZ_WL_CONTAINER(data);

  LOGWAYLAND(
      "%s [%p] frame_callback_handler %p ready_to_draw %d (set to true)"
      " initial_draw callback %zd\n",
      __FUNCTION__, (void*)moz_container_get_nsWindow(MOZ_CONTAINER(data)),
      (void*)wl_container->frame_callback_handler, wl_container->ready_to_draw,
      wl_container->initial_draw_cbs.size());

  std::vector<std::function<void(void)>> cbs;
  {
    // Protect mozcontainer internals changes by container_lock.
    MutexAutoLock lock(wl_container->container_lock);
    MozClearPointer(wl_container->frame_callback_handler, wl_callback_destroy);
    // It's possible that container is already unmapped so quit in such case.
    if (!wl_container->surface) {
      LOGWAYLAND("  container is unmapped, quit.");
      if (!wl_container->initial_draw_cbs.empty()) {
        NS_WARNING("Unmapping MozContainer with active draw callback!");
        wl_container->initial_draw_cbs.clear();
      }
      return;
    }
    if (wl_container->ready_to_draw) {
      return;
    }
    wl_container->ready_to_draw = true;
    cbs = std::move(wl_container->initial_draw_cbs);
  }

  // Call the callbacks registered by
  // moz_container_wayland_add_or_fire_initial_draw_callback().
  // and we can't do that under mozcontainer lock.
  for (auto const& cb : cbs) {
    cb();
  }
}

static const struct wl_callback_listener moz_container_frame_listener = {
    moz_container_wayland_frame_callback_handler};

static void after_frame_clock_after_paint(GdkFrameClock* clock,
                                          MozContainer* container) {
  MozContainerSurfaceLock lock(container);
  struct wl_surface* surface = lock.GetSurface();
  if (surface) {
    wl_surface_commit(surface);
  }
}

static bool moz_gdk_wayland_window_add_frame_callback_surface_locked(
    const MutexAutoLock& aProofOfLock, MozContainer* container) {
  static auto sGdkWaylandWindowAddCallbackSurface =
      (void (*)(GdkWindow*, struct wl_surface*))dlsym(
          RTLD_DEFAULT, "gdk_wayland_window_add_frame_callback_surface");

  if (!StaticPrefs::widget_wayland_opaque_region_enabled_AtStartup() ||
      !sGdkWaylandWindowAddCallbackSurface) {
    return false;
  }

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  MozContainerWayland* wl_container = &container->data.wl_container;

  sGdkWaylandWindowAddCallbackSurface(window, wl_container->surface);

  GdkFrameClock* frame_clock = gdk_window_get_frame_clock(window);
  g_signal_connect_after(frame_clock, "after-paint",
                         G_CALLBACK(after_frame_clock_after_paint), container);
  return true;
}

static void moz_gdk_wayland_window_remove_frame_callback_surface_locked(
    const MutexAutoLock& aProofOfLock, MozContainer* container) {
  static auto sGdkWaylandWindowRemoveCallbackSurface =
      (void (*)(GdkWindow*, struct wl_surface*))dlsym(
          RTLD_DEFAULT, "gdk_wayland_window_remove_frame_callback_surface");

  if (!sGdkWaylandWindowRemoveCallbackSurface) {
    return;
  }

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  MozContainerWayland* wl_container = &container->data.wl_container;

  if (wl_container->surface) {
    sGdkWaylandWindowRemoveCallbackSurface(window, wl_container->surface);
  }

  GdkFrameClock* frame_clock = gdk_window_get_frame_clock(window);
  g_signal_handlers_disconnect_by_func(
      frame_clock, FuncToGpointer(after_frame_clock_after_paint), container);
}

void moz_container_wayland_unmap(GtkWidget* widget) {
  MozContainer* container = MOZ_CONTAINER(widget);
  MozContainerWayland* wl_container = &container->data.wl_container;
  MutexAutoLock lock(wl_container->container_lock);

  LOGCONTAINER("%s [%p]\n", __FUNCTION__,
               (void*)moz_container_get_nsWindow(container));

  moz_container_wayland_clear_initial_draw_callback_locked(lock, container);

  if (wl_container->opaque_region_used) {
    moz_gdk_wayland_window_remove_frame_callback_surface_locked(lock,
                                                                container);
  }
  if (wl_container->commit_to_parent) {
    wl_container->surface = nullptr;
  }

  MozClearPointer(wl_container->eglwindow, wl_egl_window_destroy);
  MozClearPointer(wl_container->subsurface, wl_subsurface_destroy);
  MozClearPointer(wl_container->surface, wl_surface_destroy);
  MozClearPointer(wl_container->viewport, wp_viewport_destroy);
  MozClearPointer(wl_container->fractional_scale,
                  wp_fractional_scale_v1_destroy);

  wl_container->ready_to_draw = false;
  wl_container->buffer_scale = 1;
  wl_container->current_fractional_scale = 0.0;
}

gboolean moz_container_wayland_map_event(GtkWidget* widget,
                                         GdkEventAny* event) {
  MozContainerWayland* wl_container = &MOZ_CONTAINER(widget)->data.wl_container;

  LOGCONTAINER("%s [%p]\n", __FUNCTION__,
               (void*)moz_container_get_nsWindow(MOZ_CONTAINER(widget)));

  // We need to mark MozContainer as mapped to make sure
  // moz_container_wayland_unmap() is called on hide/withdraw.
  gtk_widget_set_mapped(widget, TRUE);

  // Make sure we're on main thread as we can't lock mozContainer here
  // due to moz_container_wayland_add_or_fire_initial_draw_callback() call
  // below.
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  // Set waiting_to_show flag. It means the mozcontainer is cofigured/mapped
  // and it's supposed to be visible. *But* it's really visible when we get
  // moz_container_wayland_add_or_fire_initial_draw_callback() which means
  // wayland compositor makes it live.
  wl_container->waiting_to_show = true;
  MozContainer* container = MOZ_CONTAINER(widget);
  moz_container_wayland_add_or_fire_initial_draw_callback(
      container, [container]() -> void {
        LOGCONTAINER(
            "[%p] moz_container_wayland_add_or_fire_initial_draw_callback set "
            "visible",
            moz_container_get_nsWindow(container));
        moz_container_wayland_clear_waiting_to_show_flag(container);
      });

  MutexAutoLock lock(wl_container->container_lock);

  // Don't create wl_subsurface in map_event when it's already created or
  // if we create it for the first time.
  if (wl_container->ready_to_draw || wl_container->before_first_size_alloc) {
    return FALSE;
  }

  if (!wl_container->surface) {
    if (!moz_container_wayland_surface_create_locked(lock,
                                                     MOZ_CONTAINER(widget))) {
      return FALSE;
    }
  }

  nsWindow* window = moz_container_get_nsWindow(MOZ_CONTAINER(widget));
  moz_container_wayland_set_scale_factor_locked(lock, MOZ_CONTAINER(widget),
                                                window->GdkCeiledScaleFactor());
  moz_container_wayland_set_opaque_region_locked(lock, MOZ_CONTAINER(widget));
  moz_container_clear_input_region(MOZ_CONTAINER(widget));
  moz_container_wayland_invalidate(MOZ_CONTAINER(widget));
  return FALSE;
}

void moz_container_wayland_map(GtkWidget* widget) {
  LOGCONTAINER("%s [%p]\n", __FUNCTION__,
               (void*)moz_container_get_nsWindow(MOZ_CONTAINER(widget)));

  g_return_if_fail(IS_MOZ_CONTAINER(widget));
  gtk_widget_set_mapped(widget, TRUE);

  if (gtk_widget_get_has_window(widget)) {
    gdk_window_show(gtk_widget_get_window(widget));
  }
}

void moz_container_wayland_size_allocate(GtkWidget* widget,
                                         GtkAllocation* allocation) {
  MozContainer* container;
  GtkAllocation tmp_allocation;

  g_return_if_fail(IS_MOZ_CONTAINER(widget));

  LOGCONTAINER("moz_container_wayland_size_allocate [%p] %d,%d -> %d x %d\n",
               (void*)moz_container_get_nsWindow(MOZ_CONTAINER(widget)),
               allocation->x, allocation->y, allocation->width,
               allocation->height);

  /* short circuit if you can */
  container = MOZ_CONTAINER(widget);
  gtk_widget_get_allocation(widget, &tmp_allocation);
  if (!container->data.children && tmp_allocation.x == allocation->x &&
      tmp_allocation.y == allocation->y &&
      tmp_allocation.width == allocation->width &&
      tmp_allocation.height == allocation->height) {
    return;
  }
  gtk_widget_set_allocation(widget, allocation);

  if (gtk_widget_get_has_window(widget) && gtk_widget_get_realized(widget)) {
    gdk_window_move_resize(gtk_widget_get_window(widget), allocation->x,
                           allocation->y, allocation->width,
                           allocation->height);
    // We need to position our subsurface according to GdkWindow
    // when offset changes (GdkWindow is maximized for instance).
    // see gtk-clutter-embed.c for reference.
    MutexAutoLock lock(container->data.wl_container.container_lock);
    if (!container->data.wl_container.surface) {
      if (!moz_container_wayland_surface_create_locked(lock, container)) {
        return;
      }
    }
    nsWindow* window = moz_container_get_nsWindow(container);
    moz_container_wayland_set_scale_factor_locked(
        lock, container, window->GdkCeiledScaleFactor());
    moz_container_wayland_set_opaque_region_locked(lock, container);
    moz_container_wayland_move_locked(lock, container, allocation->x,
                                      allocation->y);
    moz_container_clear_input_region(container);
    moz_container_wayland_invalidate(MOZ_CONTAINER(widget));
    container->data.wl_container.before_first_size_alloc = false;
  }
}

static wl_region* moz_container_wayland_create_opaque_region(
    int aX, int aY, int aWidth, int aHeight, int aCornerRadius) {
  struct wl_compositor* compositor = WaylandDisplayGet()->GetCompositor();
  wl_region* region = wl_compositor_create_region(compositor);
  wl_region_add(region, aX, aY, aWidth, aHeight);
  if (aCornerRadius) {
    wl_region_subtract(region, aX, aY, aCornerRadius, aCornerRadius);
    wl_region_subtract(region, aX + aWidth - aCornerRadius, aY, aCornerRadius,
                       aCornerRadius);
    wl_region_subtract(region, aX, aY + aHeight - aCornerRadius, aCornerRadius,
                       aCornerRadius);
    wl_region_subtract(region, aX + aWidth - aCornerRadius,
                       aY + aHeight - aCornerRadius, aCornerRadius,
                       aCornerRadius);
  }
  return region;
}

static void moz_container_wayland_set_opaque_region_locked(
    const MutexAutoLock& aProofOfLock, MozContainer* container) {
  MozContainerWayland* wl_container = &container->data.wl_container;

  if (!wl_container->opaque_region_needs_updates) {
    return;
  }

  if (!wl_container->opaque_region_used) {
    wl_container->opaque_region_needs_updates = false;
    return;
  }

  GtkAllocation allocation;
  gtk_widget_get_allocation(GTK_WIDGET(container), &allocation);

  wl_region* region = moz_container_wayland_create_opaque_region(
      0, 0, allocation.width, allocation.height,
      wl_container->opaque_region_corner_radius);
  wl_surface_set_opaque_region(wl_container->surface, region);
  wl_region_destroy(region);
  wl_container->opaque_region_needs_updates = false;
}

static void moz_container_wayland_set_opaque_region(MozContainer* container) {
  MozContainerWayland* wl_container = &container->data.wl_container;
  MutexAutoLock lock(wl_container->container_lock);
  if (wl_container->surface) {
    moz_container_wayland_set_opaque_region_locked(lock, container);
  }
}

static void moz_container_wayland_surface_set_scale_locked(
    const MutexAutoLock& aProofOfLock, MozContainerWayland* wl_container,
    int scale) {
  if (!wl_container->surface) {
    return;
  }
  if (wl_container->buffer_scale == scale) {
    return;
  }

  LOGCONTAINER("%s scale %d\n", __FUNCTION__, scale);

  // There is a chance that the attached wl_buffer has not yet been doubled
  // on the main thread when scale factor changed to 2. This leads to
  // crash with the following message:
  // Buffer size (AxB) must be an integer multiple of the buffer_scale (2)
  // Removing the possibly wrong wl_buffer to prevent that crash:
  wl_surface_attach(wl_container->surface, nullptr, 0, 0);
  wl_surface_set_buffer_scale(wl_container->surface, scale);
  wl_container->buffer_scale = scale;
}

static void fractional_scale_handle_preferred_scale(
    void* data, struct wp_fractional_scale_v1* info, uint32_t wire_scale) {
  MozContainer* container = MOZ_CONTAINER(data);
  MozContainerWayland* wl_container = &container->data.wl_container;
  wl_container->current_fractional_scale = wire_scale / 120.0;

  RefPtr<nsWindow> window = moz_container_get_nsWindow(container);
  LOGWAYLAND("%s [%p] scale: %f\n", __func__, window.get(),
             wl_container->current_fractional_scale);
  MOZ_DIAGNOSTIC_ASSERT(window);
  window->OnScaleChanged(/* aNotify = */ true);
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener =
    {
        .preferred_scale = fractional_scale_handle_preferred_scale,
};

void moz_container_wayland_set_scale_factor_locked(
    const MutexAutoLock& aProofOfLock, MozContainer* container, int aScale) {
  if (gfx::gfxVars::UseWebRenderCompositor()) {
    // the compositor backend handles scaling itself
    return;
  }

  MozContainerWayland* wl_container = &container->data.wl_container;
  wl_container->container_lock.AssertCurrentThreadOwns();

  if (StaticPrefs::widget_wayland_fractional_scale_enabled_AtStartup()) {
    if (!wl_container->fractional_scale) {
      if (auto* manager = WaylandDisplayGet()->GetFractionalScaleManager()) {
        wl_container->fractional_scale =
            wp_fractional_scale_manager_v1_get_fractional_scale(
                manager, wl_container->surface);
        wp_fractional_scale_v1_add_listener(wl_container->fractional_scale,
                                            &fractional_scale_listener,
                                            container);
      }
    }

    if (wl_container->fractional_scale) {
      if (!wl_container->viewport) {
        if (auto* viewporter = WaylandDisplayGet()->GetViewporter()) {
          wl_container->viewport =
              wp_viewporter_get_viewport(viewporter, wl_container->surface);
        }
      }
      if (wl_container->viewport) {
        GdkWindow* gdkWindow = gtk_widget_get_window(GTK_WIDGET(container));
        wp_viewport_set_destination(wl_container->viewport,
                                    gdk_window_get_width(gdkWindow),
                                    gdk_window_get_height(gdkWindow));
        return;
      }
    }
  }

  moz_container_wayland_surface_set_scale_locked(aProofOfLock, wl_container,
                                                 aScale);
}

bool moz_container_wayland_size_matches_scale_factor_locked(
    const MutexAutoLock& aProofOfLock, MozContainer* container, int aWidth,
    int aHeight) {
  return aWidth % container->data.wl_container.buffer_scale == 0 &&
         aHeight % container->data.wl_container.buffer_scale == 0;
}

static bool moz_container_wayland_surface_create_locked(
    const MutexAutoLock& aProofOfLock, MozContainer* container) {
  MozContainerWayland* wl_container = &container->data.wl_container;

  LOGWAYLAND("%s [%p]\n", __FUNCTION__,
             (void*)moz_container_get_nsWindow(container));

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  MOZ_DIAGNOSTIC_ASSERT(window);

  wl_surface* parent_surface = gdk_wayland_window_get_wl_surface(window);
  if (!parent_surface) {
    LOGWAYLAND("    Failed - missing parent surface!");
    return false;
  }
  LOGWAYLAND("    gtk wl_surface %p ID %d\n", (void*)parent_surface,
             wl_proxy_get_id((struct wl_proxy*)parent_surface));

  if (wl_container->commit_to_parent) {
    LOGWAYLAND("    commit to parent");
    wl_container->surface = parent_surface;
    NS_DispatchToCurrentThread(NewRunnableFunction(
        "moz_container_wayland_frame_callback_handler",
        &moz_container_wayland_frame_callback_handler, container, nullptr, 0));
    return true;
  }

  // Available as of GTK 3.8+
  struct wl_compositor* compositor = WaylandDisplayGet()->GetCompositor();
  wl_container->surface = wl_compositor_create_surface(compositor);
  if (!wl_container->surface) {
    LOGWAYLAND("    Failed - can't create surface!");
    return false;
  }

  wl_container->subsurface =
      wl_subcompositor_get_subsurface(WaylandDisplayGet()->GetSubcompositor(),
                                      wl_container->surface, parent_surface);
  if (!wl_container->subsurface) {
    MozClearPointer(wl_container->surface, wl_surface_destroy);
    LOGWAYLAND("    Failed - can't create sub-surface!");
    return false;
  }
  wl_subsurface_set_desync(wl_container->subsurface);

  // Try to guess subsurface offset to avoid potential flickering.
  int dx, dy;
  if (moz_container_get_nsWindow(container)->GetCSDDecorationOffset(&dx, &dy)) {
    wl_container->subsurface_dx = dx;
    wl_container->subsurface_dy = dy;
    wl_subsurface_set_position(wl_container->subsurface, dx, dy);
    LOGWAYLAND("    guessing subsurface position %d %d\n", dx, dy);
  }

  // If there's pending frame callback it's for wrong parent surface,
  // so delete it.
  if (wl_container->frame_callback_handler) {
    MozClearPointer(wl_container->frame_callback_handler, wl_callback_destroy);
  }
  wl_container->frame_callback_handler = wl_surface_frame(parent_surface);
  wl_callback_add_listener(wl_container->frame_callback_handler,
                           &moz_container_frame_listener, container);
  LOGWAYLAND(
      "    created frame callback ID %d\n",
      wl_proxy_get_id((struct wl_proxy*)wl_container->frame_callback_handler));

  wl_surface_commit(wl_container->surface);
  wl_display_flush(WaylandDisplayGet()->GetDisplay());

  wl_container->opaque_region_used =
      moz_gdk_wayland_window_add_frame_callback_surface_locked(aProofOfLock,
                                                               container);

  LOGWAYLAND("    created surface %p ID %d\n", (void*)wl_container->surface,
             wl_proxy_get_id((struct wl_proxy*)wl_container->surface));
  return true;
}

struct wl_surface* moz_container_wayland_surface_lock(MozContainer* container)
    MOZ_NO_THREAD_SAFETY_ANALYSIS {
  // LOGWAYLAND("%s [%p] surface %p ready_to_draw %d\n", __FUNCTION__,
  //           (void*)container, (void*)container->data.wl_container.surface,
  //           container->data.wl_container.ready_to_draw);
  container->data.wl_container.container_lock.Lock();
  if (!container->data.wl_container.surface ||
      !container->data.wl_container.ready_to_draw) {
    return nullptr;
  }
  return container->data.wl_container.surface;
}

void moz_container_wayland_surface_unlock(MozContainer* container,
                                          struct wl_surface** surface)
    MOZ_NO_THREAD_SAFETY_ANALYSIS {
  // Temporarily disabled to avoid log noise
  // LOGWAYLAND("%s [%p] surface %p\n", __FUNCTION__, (void*)container,
  //            (void*)container->data.wl_container.surface);
  if (*surface) {
    *surface = nullptr;
  }
  container->data.wl_container.container_lock.Unlock();
}

struct wl_egl_window* moz_container_wayland_get_egl_window(
    MozContainer* container, double scale) {
  MozContainerWayland* wl_container = &container->data.wl_container;

  LOGCONTAINER("%s [%p] eglwindow %p scale %d\n", __FUNCTION__,
               (void*)moz_container_get_nsWindow(container),
               (void*)wl_container->eglwindow, (int)scale);

  MutexAutoLock lock(wl_container->container_lock);
  if (!wl_container->surface || !wl_container->ready_to_draw) {
    LOGCONTAINER(
        "  quit, wl_container->surface %p wl_container->ready_to_draw %d\n",
        wl_container->surface, wl_container->ready_to_draw);
    return nullptr;
  }

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  nsIntSize requestedSize((int)round(gdk_window_get_width(window) * scale),
                          (int)round(gdk_window_get_height(window) * scale));

  if (!wl_container->eglwindow) {
    wl_container->eglwindow = wl_egl_window_create(
        wl_container->surface, requestedSize.width, requestedSize.height);

    LOGCONTAINER("%s [%p] created eglwindow %p size %d x %d (with scale %f)\n",
                 __FUNCTION__, (void*)moz_container_get_nsWindow(container),
                 (void*)wl_container->eglwindow, requestedSize.width,
                 requestedSize.height, scale);
  } else {
    nsIntSize recentSize;
    wl_egl_window_get_attached_size(wl_container->eglwindow, &recentSize.width,
                                    &recentSize.height);
    if (requestedSize != recentSize) {
      LOGCONTAINER("%s [%p] resized to %d x %d (with scale %f)\n", __FUNCTION__,
                   (void*)moz_container_get_nsWindow(container),
                   requestedSize.width, requestedSize.height, scale);
      wl_egl_window_resize(wl_container->eglwindow, requestedSize.width,
                           requestedSize.height, 0, 0);
    }
  }
  moz_container_wayland_surface_set_scale_locked(lock, wl_container,
                                                 static_cast<int>(scale));
  return wl_container->eglwindow;
}

gboolean moz_container_wayland_has_egl_window(MozContainer* container) {
  return !!container->data.wl_container.eglwindow;
}

void moz_container_wayland_update_opaque_region(MozContainer* container,
                                                int corner_radius) {
  MozContainerWayland* wl_container = &container->data.wl_container;
  wl_container->opaque_region_needs_updates = true;
  wl_container->opaque_region_corner_radius = corner_radius;

  // When GL compositor / WebRender is used,
  // moz_container_wayland_get_egl_window() is called only once when window
  // is created or resized so update opaque region now.
  if (moz_container_wayland_has_egl_window(container)) {
    moz_container_wayland_set_opaque_region(container);
  }
}

gboolean moz_container_wayland_can_draw(MozContainer* container) {
  MozContainerWayland* wl_container = &container->data.wl_container;
  MutexAutoLock lock(wl_container->container_lock);
  return wl_container->ready_to_draw;
}

double moz_container_wayland_get_fractional_scale(MozContainer* container) {
  return container->data.wl_container.current_fractional_scale;
}

double moz_container_wayland_get_scale(MozContainer* container) {
  nsWindow* window = moz_container_get_nsWindow(container);
  return window ? window->FractionalScaleFactor() : 1.0;
}

void moz_container_wayland_set_commit_to_parent(MozContainer* container) {
  MozContainerWayland* wl_container = &container->data.wl_container;
  MOZ_DIAGNOSTIC_ASSERT(!wl_container->surface);
  wl_container->commit_to_parent = true;
}

bool moz_container_wayland_is_commiting_to_parent(MozContainer* container) {
  return container->data.wl_container.commit_to_parent;
}

bool moz_container_wayland_is_waiting_to_show(MozContainer* container) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  return container->data.wl_container.waiting_to_show;
}

void moz_container_wayland_clear_waiting_to_show_flag(MozContainer* container) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  container->data.wl_container.waiting_to_show = false;
}
