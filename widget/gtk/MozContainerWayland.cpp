/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
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

#include "nsGtkUtils.h"
#include "nsWaylandDisplay.h"

#undef LOG
#ifdef MOZ_LOGGING

#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
#  include "nsWindow.h"
extern mozilla::LazyLogModule gWidgetWaylandLog;
#  define LOGWAYLAND(args) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, args)
#else
#  define LOGWAYLAND(args)
#endif /* MOZ_LOGGING */

using namespace mozilla;
using namespace mozilla::widget;

/* init methods */
static void moz_container_wayland_destroy(GtkWidget* widget);

/* widget class methods */
static void moz_container_wayland_map(GtkWidget* widget);
static gboolean moz_container_wayland_map_event(GtkWidget* widget,
                                                GdkEventAny* event);
static void moz_container_wayland_unmap(GtkWidget* widget);
static void moz_container_wayland_size_allocate(GtkWidget* widget,
                                                GtkAllocation* allocation);
static bool moz_container_wayland_surface_create_locked(
    MozContainer* container);
static void moz_container_wayland_set_scale_factor_locked(
    MozContainer* container);
static void moz_container_wayland_set_opaque_region_locked(
    MozContainer* container);

static nsWindow* moz_container_get_nsWindow(MozContainer* container) {
  gpointer user_data = g_object_get_data(G_OBJECT(container), "nsWindow");
  return static_cast<nsWindow*>(user_data);
}

// Imlemented in MozContainer.cpp
void moz_container_realize(GtkWidget* widget);

// Invalidate gtk wl_surface to commit changes to wl_subsurface.
// wl_subsurface changes are effective when parent surface is commited.
static void moz_container_wayland_invalidate(MozContainer* container) {
  LOGWAYLAND(("moz_container_wayland_invalidate [%p]\n", (void*)container));

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  if (!window) {
    LOGWAYLAND(("    Failed - missing GdkWindow!\n"));
    return;
  }

  GdkRectangle rect = (GdkRectangle){0, 0, gdk_window_get_width(window),
                                     gdk_window_get_height(window)};
  gdk_window_invalidate_rect(window, &rect, true);
}

static void moz_container_wayland_move_locked(MozContainer* container, int dx,
                                              int dy) {
  LOGWAYLAND(
      ("moz_container_wayland_move [%p] %d,%d\n", (void*)container, dx, dy));

  MozContainerWayland* wl_container = &container->wl_container;

  if (wl_container->subsurface_dx == dx && wl_container->subsurface_dy == dy) {
    return;
  }

  wl_container->subsurface_dx = dx;
  wl_container->subsurface_dy = dy;
  wl_subsurface_set_position(wl_container->subsurface,
                             wl_container->subsurface_dx,
                             wl_container->subsurface_dy);
}

// This is called from layout/compositor code only with
// size equal to GL rendering context. Otherwise there are
// rendering artifacts as wl_egl_window size does not match
// GL rendering pipeline setup.
void moz_container_wayland_egl_window_set_size(MozContainer* container,
                                               int width, int height) {
  MozContainerWayland* wl_container = &container->wl_container;
  MutexAutoLock lock(*wl_container->container_lock);
  if (wl_container->eglwindow) {
    wl_egl_window_resize(wl_container->eglwindow, width, height, 0, 0);
  }
}

void moz_container_wayland_class_init(MozContainerClass* klass) {
  /*GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass); */
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

  widget_class->map = moz_container_wayland_map;
  widget_class->map_event = moz_container_wayland_map_event;
  widget_class->destroy = moz_container_wayland_destroy;
  widget_class->unmap = moz_container_wayland_unmap;
  widget_class->realize = moz_container_realize;
  widget_class->size_allocate = moz_container_wayland_size_allocate;
}

void moz_container_wayland_init(MozContainerWayland* container) {
  container->surface = nullptr;
  container->subsurface = nullptr;
  container->eglwindow = nullptr;
  container->frame_callback_handler = nullptr;
  container->viewport = nullptr;
  container->ready_to_draw = false;
  container->opaque_region_needs_updates = false;
  container->opaque_region_subtract_corners = false;
  container->opaque_region_used = false;
  container->surface_needs_clear = true;
  container->subsurface_dx = 0;
  container->subsurface_dy = 0;
  container->before_first_size_alloc = true;
  container->buffer_scale = 1;
  container->initial_draw_cbs.clear();
  container->container_lock = new mozilla::Mutex("MozContainer lock");
}

static void moz_container_wayland_destroy(GtkWidget* widget) {
  MozContainerWayland* container = &MOZ_CONTAINER(widget)->wl_container;
  delete container->container_lock;
  container->container_lock = nullptr;
  container->initial_draw_cbs.clear();
}

void moz_container_wayland_add_initial_draw_callback(
    MozContainer* container, const std::function<void(void)>& initial_draw_cb) {
  container->wl_container.initial_draw_cbs.push_back(initial_draw_cb);
}

static void moz_container_wayland_frame_callback_handler(
    void* data, struct wl_callback* callback, uint32_t time) {
  MozContainerWayland* wl_container = &MOZ_CONTAINER(data)->wl_container;

  LOGWAYLAND(
      ("%s [%p] frame_callback_handler %p ready_to_draw %d (set to true)"
       " initial_draw callback %zd\n",
       __FUNCTION__, (void*)MOZ_CONTAINER(data),
       (void*)wl_container->frame_callback_handler, wl_container->ready_to_draw,
       wl_container->initial_draw_cbs.size()));

  g_clear_pointer(&wl_container->frame_callback_handler, wl_callback_destroy);

  if (!wl_container->ready_to_draw) {
    wl_container->ready_to_draw = true;
    for (auto const& cb : wl_container->initial_draw_cbs) {
      cb();
    }
    wl_container->initial_draw_cbs.clear();
  }
}

static const struct wl_callback_listener moz_container_frame_listener = {
    moz_container_wayland_frame_callback_handler};

static void after_frame_clock_after_paint(GdkFrameClock* clock,
                                          MozContainer* container) {
  struct wl_surface* surface = moz_container_wayland_surface_lock(container);
  if (surface) {
    wl_surface_commit(surface);
    moz_container_wayland_surface_unlock(container, &surface);
  }
}

static bool moz_gdk_wayland_window_add_frame_callback_surface_locked(
    MozContainer* container) {
  static auto sGdkWaylandWindowAddCallbackSurface =
      (void (*)(GdkWindow*, struct wl_surface*))dlsym(
          RTLD_DEFAULT, "gdk_wayland_window_add_frame_callback_surface");

  if (!StaticPrefs::widget_wayland_opaque_region_enabled() ||
      !sGdkWaylandWindowAddCallbackSurface) {
    return false;
  }

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  MozContainerWayland* wl_container = &container->wl_container;

  sGdkWaylandWindowAddCallbackSurface(window, wl_container->surface);

  GdkFrameClock* frame_clock = gdk_window_get_frame_clock(window);
  g_signal_connect_after(frame_clock, "after-paint",
                         G_CALLBACK(after_frame_clock_after_paint), container);
  return true;
}

static void moz_gdk_wayland_window_remove_frame_callback_surface_locked(
    MozContainer* container) {
  static auto sGdkWaylandWindowRemoveCallbackSurface =
      (void (*)(GdkWindow*, struct wl_surface*))dlsym(
          RTLD_DEFAULT, "gdk_wayland_window_remove_frame_callback_surface");

  if (!sGdkWaylandWindowRemoveCallbackSurface) {
    return;
  }

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  MozContainerWayland* wl_container = &container->wl_container;

  sGdkWaylandWindowRemoveCallbackSurface(window, wl_container->surface);

  GdkFrameClock* frame_clock = gdk_window_get_frame_clock(window);
  g_signal_handlers_disconnect_by_func(
      frame_clock, FuncToGpointer(after_frame_clock_after_paint), container);
}

static void moz_container_wayland_unmap_internal(MozContainer* container) {
  MozContainerWayland* wl_container = &container->wl_container;
  MutexAutoLock lock(*wl_container->container_lock);

  if (wl_container->opaque_region_used) {
    moz_gdk_wayland_window_remove_frame_callback_surface_locked(container);
  }

  g_clear_pointer(&wl_container->eglwindow, wl_egl_window_destroy);
  g_clear_pointer(&wl_container->subsurface, wl_subsurface_destroy);
  g_clear_pointer(&wl_container->surface, wl_surface_destroy);
  g_clear_pointer(&wl_container->viewport, wp_viewport_destroy);
  g_clear_pointer(&wl_container->frame_callback_handler, wl_callback_destroy);

  wl_container->surface_needs_clear = true;
  wl_container->ready_to_draw = false;
  wl_container->buffer_scale = 1;

  LOGWAYLAND(("%s [%p]\n", __FUNCTION__, (void*)container));
}

static gboolean moz_container_wayland_map_event(GtkWidget* widget,
                                                GdkEventAny* event) {
  MozContainerWayland* wl_container = &MOZ_CONTAINER(widget)->wl_container;
  LOGWAYLAND(("%s [%p]\n", __FUNCTION__, (void*)MOZ_CONTAINER(widget)));

  // Don't create wl_subsurface in map_event when it's already created or
  // if we create it for the first time.
  if (wl_container->ready_to_draw || wl_container->before_first_size_alloc) {
    return FALSE;
  }

  MutexAutoLock lock(*wl_container->container_lock);
  if (!wl_container->surface) {
    if (!moz_container_wayland_surface_create_locked(MOZ_CONTAINER(widget))) {
      return FALSE;
    }
  }

  moz_container_wayland_set_scale_factor_locked(MOZ_CONTAINER(widget));
  moz_container_wayland_set_opaque_region_locked(MOZ_CONTAINER(widget));
  moz_container_wayland_invalidate(MOZ_CONTAINER(widget));
  return FALSE;
}

void moz_container_wayland_map(GtkWidget* widget) {
  LOGWAYLAND(("%s [%p]\n", __FUNCTION__, (void*)widget));

  g_return_if_fail(IS_MOZ_CONTAINER(widget));
  gtk_widget_set_mapped(widget, TRUE);

  if (gtk_widget_get_has_window(widget)) {
    gdk_window_show(gtk_widget_get_window(widget));
  }
}

void moz_container_wayland_unmap(GtkWidget* widget) {
  LOGWAYLAND(("%s [%p]\n", __FUNCTION__, (void*)widget));

  g_return_if_fail(IS_MOZ_CONTAINER(widget));
  gtk_widget_set_mapped(widget, FALSE);

  if (gtk_widget_get_has_window(widget)) {
    gdk_window_hide(gtk_widget_get_window(widget));
    moz_container_wayland_unmap_internal(MOZ_CONTAINER(widget));
  }
}

void moz_container_wayland_size_allocate(GtkWidget* widget,
                                         GtkAllocation* allocation) {
  MozContainer* container;
  GtkAllocation tmp_allocation;

  g_return_if_fail(IS_MOZ_CONTAINER(widget));

  LOGWAYLAND(("moz_container_wayland_size_allocate [%p] %d,%d -> %d x %d\n",
              (void*)widget, allocation->x, allocation->y, allocation->width,
              allocation->height));

  /* short circuit if you can */
  container = MOZ_CONTAINER(widget);
  gtk_widget_get_allocation(widget, &tmp_allocation);
  if (!container->children && tmp_allocation.x == allocation->x &&
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
    MutexAutoLock lock(*container->wl_container.container_lock);
    if (!container->wl_container.surface) {
      if (!moz_container_wayland_surface_create_locked(container)) {
        return;
      }
    }
    moz_container_wayland_set_scale_factor_locked(container);
    moz_container_wayland_set_opaque_region_locked(container);
    moz_container_wayland_move_locked(container, allocation->x, allocation->y);
    moz_container_wayland_invalidate(MOZ_CONTAINER(widget));
    container->wl_container.before_first_size_alloc = false;
  }
}

static wl_region* moz_container_wayland_create_opaque_region(
    int aX, int aY, int aWidth, int aHeight, bool aSubtractCorners) {
  struct wl_compositor* compositor = WaylandDisplayGet()->GetCompositor();
  wl_region* region = wl_compositor_create_region(compositor);
  wl_region_add(region, aX, aY, aWidth, aHeight);
  if (aSubtractCorners) {
    wl_region_subtract(region, aX, aY, TITLEBAR_SHAPE_MASK_HEIGHT,
                       TITLEBAR_SHAPE_MASK_HEIGHT);
    wl_region_subtract(region, aX + aWidth - TITLEBAR_SHAPE_MASK_HEIGHT, aY,
                       TITLEBAR_SHAPE_MASK_HEIGHT, TITLEBAR_SHAPE_MASK_HEIGHT);
  }
  return region;
}

static void moz_container_wayland_set_opaque_region_locked(
    MozContainer* container) {
  MozContainerWayland* wl_container = &container->wl_container;

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
      wl_container->opaque_region_subtract_corners);
  wl_surface_set_opaque_region(wl_container->surface, region);
  wl_region_destroy(region);
  wl_container->opaque_region_needs_updates = false;
}

static void moz_container_wayland_set_opaque_region(MozContainer* container) {
  MozContainerWayland* wl_container = &container->wl_container;
  MutexAutoLock lock(*wl_container->container_lock);
  if (wl_container->surface) {
    moz_container_wayland_set_opaque_region_locked(container);
  }
}

static void moz_container_wayland_set_scale_factor_locked(
    MozContainer* container) {
  MozContainerWayland* wl_container = &container->wl_container;
  nsWindow* window = moz_container_get_nsWindow(container);

  if (window && window->UseFractionalScale()) {
    if (!wl_container->viewport) {
      wl_container->viewport = wp_viewporter_get_viewport(
          WaylandDisplayGet()->GetViewporter(), wl_container->surface);
    }

    GdkWindow* gdkWindow = gtk_widget_get_window(GTK_WIDGET(container));
    wp_viewport_set_destination(wl_container->viewport,
                                gdk_window_get_width(gdkWindow),
                                gdk_window_get_height(gdkWindow));
  } else {
    int scale = window ? window->GdkCeiledScaleFactor() : 1;

    if (scale == wl_container->buffer_scale) {
      return;
    }

    LOGWAYLAND(("%s [%p] scale %d\n", __FUNCTION__, (void*)container, scale));
    wl_surface_set_buffer_scale(wl_container->surface, scale);
    wl_container->buffer_scale = scale;
  }
}

void moz_container_wayland_set_scale_factor(MozContainer* container) {
  MutexAutoLock lock(*container->wl_container.container_lock);
  if (container->wl_container.surface) {
    moz_container_wayland_set_scale_factor_locked(container);
  }
}

static bool moz_container_wayland_surface_create_locked(
    MozContainer* container) {
  MozContainerWayland* wl_container = &container->wl_container;

  LOGWAYLAND(("%s [%p]\n", __FUNCTION__, (void*)container));

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  wl_surface* parent_surface = gdk_wayland_window_get_wl_surface(window);
  if (!parent_surface) {
    LOGWAYLAND(("    Failed - missing parent surface!"));
    return false;
  }
  LOGWAYLAND(("    gtk wl_surface %p ID %d\n", (void*)parent_surface,
              wl_proxy_get_id((struct wl_proxy*)parent_surface)));

  // Available as of GTK 3.8+
  struct wl_compositor* compositor = WaylandDisplayGet()->GetCompositor();
  wl_container->surface = wl_compositor_create_surface(compositor);
  if (!wl_container->surface) {
    LOGWAYLAND(("    Failed - can't create surface!"));
    return false;
  }

  wl_container->subsurface =
      wl_subcompositor_get_subsurface(WaylandDisplayGet()->GetSubcompositor(),
                                      wl_container->surface, parent_surface);
  if (!wl_container->subsurface) {
    g_clear_pointer(&wl_container->surface, wl_surface_destroy);
    LOGWAYLAND(("    Failed - can't create sub-surface!"));
    return false;
  }
  wl_subsurface_set_desync(wl_container->subsurface);

  // Try to guess subsurface offset to avoid potential flickering.
  int dx, dy;
  if (moz_container_get_nsWindow(container)->GetCSDDecorationOffset(&dx, &dy)) {
    wl_container->subsurface_dx = dx;
    wl_container->subsurface_dy = dy;
    wl_subsurface_set_position(wl_container->subsurface, dx, dy);
    LOGWAYLAND(("    guessing subsurface position %d %d\n", dx, dy));
  }

  // Route input to parent wl_surface owned by Gtk+ so we get input
  // events from Gtk+.
  wl_region* region = wl_compositor_create_region(compositor);
  wl_surface_set_input_region(wl_container->surface, region);
  wl_region_destroy(region);

  // If there's pending frame callback it's for wrong parent surface,
  // so delete it.
  if (wl_container->frame_callback_handler) {
    g_clear_pointer(&wl_container->frame_callback_handler, wl_callback_destroy);
  }
  wl_container->frame_callback_handler = wl_surface_frame(parent_surface);
  wl_callback_add_listener(wl_container->frame_callback_handler,
                           &moz_container_frame_listener, container);
  LOGWAYLAND((
      "    created frame callback ID %d\n",
      wl_proxy_get_id((struct wl_proxy*)wl_container->frame_callback_handler)));

  wl_surface_commit(wl_container->surface);
  wl_display_flush(WaylandDisplayGet()->GetDisplay());

  wl_container->opaque_region_used =
      moz_gdk_wayland_window_add_frame_callback_surface_locked(container);

  LOGWAYLAND(("    created surface %p ID %d\n", (void*)wl_container->surface,
              wl_proxy_get_id((struct wl_proxy*)wl_container->surface)));
  return true;
}

struct wl_surface* moz_container_wayland_surface_lock(MozContainer* container) {
  LOGWAYLAND(("%s [%p] surface %p ready_to_draw %d\n", __FUNCTION__,
              (void*)container, (void*)container->wl_container.surface,
              container->wl_container.ready_to_draw));

  if (!container->wl_container.surface ||
      !container->wl_container.ready_to_draw) {
    return nullptr;
  }

  container->wl_container.container_lock->Lock();

  moz_container_wayland_set_scale_factor_locked(container);
  return container->wl_container.surface;
}

void moz_container_wayland_surface_unlock(MozContainer* container,
                                          struct wl_surface** surface) {
  LOGWAYLAND(("%s [%p] surface %p\n", __FUNCTION__, (void*)container,
              (void*)container->wl_container.surface));
  if (*surface) {
    container->wl_container.container_lock->Unlock();
    *surface = nullptr;
  }
}

struct wl_egl_window* moz_container_wayland_get_egl_window(
    MozContainer* container, double scale) {
  MozContainerWayland* wl_container = &container->wl_container;

  LOGWAYLAND(("%s [%p] eglwindow %p\n", __FUNCTION__, (void*)container,
              (void*)wl_container->eglwindow));

  MutexAutoLock lock(*wl_container->container_lock);
  if (!wl_container->surface || !wl_container->ready_to_draw) {
    return nullptr;
  }
  if (!wl_container->eglwindow) {
    GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
    wl_container->eglwindow = wl_egl_window_create(
        wl_container->surface, (int)round(gdk_window_get_width(window) * scale),
        (int)round(gdk_window_get_height(window) * scale));

    LOGWAYLAND(("%s [%p] created eglwindow %p\n", __FUNCTION__,
                (void*)container, (void*)wl_container->eglwindow));
  }
  return wl_container->eglwindow;
}

gboolean moz_container_wayland_has_egl_window(MozContainer* container) {
  return container->wl_container.eglwindow != nullptr;
}

gboolean moz_container_wayland_surface_needs_clear(MozContainer* container) {
  int ret = container->wl_container.surface_needs_clear;
  container->wl_container.surface_needs_clear = false;
  return ret;
}

void moz_container_wayland_update_opaque_region(MozContainer* container,
                                                bool aSubtractCorners) {
  MozContainerWayland* wl_container = &container->wl_container;
  wl_container->opaque_region_needs_updates = true;
  wl_container->opaque_region_subtract_corners = aSubtractCorners;

  // When GL compositor / WebRender is used,
  // moz_container_wayland_get_egl_window() is called only once when window
  // is created or resized so update opaque region now.
  if (moz_container_wayland_has_egl_window(container)) {
    moz_container_wayland_set_opaque_region(container);
  }
}

gboolean moz_container_wayland_can_draw(MozContainer* container) {
  return container ? container->wl_container.ready_to_draw : false;
}
