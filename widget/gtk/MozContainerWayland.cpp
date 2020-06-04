/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozContainer.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "nsWaylandDisplay.h"
#include "gfxPlatformGtk.h"
#include <wayland-egl.h>
#include <stdio.h>
#include <dlfcn.h>

#undef LOG
#ifdef MOZ_LOGGING

#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetWaylandLog;
#  define LOGWAYLAND(args) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, args)
#else
#  define LOGWAYLAND(args)
#endif /* MOZ_LOGGING */

using namespace mozilla;
using namespace mozilla::widget;

// Declaration from nsWindow, we don't want to include whole nsWindow.h file
// here just for it.
wl_region* CreateOpaqueRegionWayland(int aX, int aY, int aWidth, int aHeight,
                                     bool aSubtractCorners);

/* init methods */
static void moz_container_wayland_destroy(GtkWidget* widget);

/* widget class methods */
static void moz_container_wayland_map(GtkWidget* widget);
static gboolean moz_container_wayland_map_event(GtkWidget* widget,
                                                GdkEventAny* event);
static void moz_container_wayland_unmap(GtkWidget* widget);
static void moz_container_wayland_size_allocate(GtkWidget* widget,
                                                GtkAllocation* allocation);

// Imlemented in MozContainer.cpp
void moz_container_realize(GtkWidget* widget);

static void moz_container_wayland_move_locked(MozContainer* container, int dx,
                                              int dy) {
  LOGWAYLAND(("moz_container_wayland_move_locked [%p] %d,%d\n",
              (void*)container, dx, dy));

  MozContainerWayland* wl_container = &container->wl_container;

  wl_container->subsurface_dx = dx;
  wl_container->subsurface_dy = dy;
  wl_container->surface_position_needs_update = true;

  // Wayland subsurface is not created yet.
  if (!wl_container->subsurface) {
    return;
  }

  // wl_subsurface_set_position is actually property of parent surface
  // which is effective when parent surface is commited.
  wl_subsurface_set_position(wl_container->subsurface,
                             wl_container->subsurface_dx,
                             wl_container->subsurface_dy);
  wl_container->surface_position_needs_update = false;

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  if (window) {
    GdkRectangle rect = (GdkRectangle){0, 0, gdk_window_get_width(window),
                                       gdk_window_get_height(window)};
    gdk_window_invalidate_rect(window, &rect, false);
  }
}

static void moz_container_wayland_move(MozContainer* container, int dx,
                                       int dy) {
  MutexAutoLock lock(*container->wl_container.container_lock);
  LOGWAYLAND(
      ("moz_container_wayland_move [%p] %d,%d\n", (void*)container, dx, dy));
  moz_container_wayland_move_locked(container, dx, dy);
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
  container->frame_callback_handler_surface_id = -1;
  container->ready_to_draw = false;
  container->opaque_region_needs_update = false;
  container->opaque_region_subtract_corners = false;
  container->opaque_region_fullscreen = false;
  container->surface_needs_clear = true;
  container->subsurface_dx = 0;
  container->subsurface_dy = 0;
  container->surface_position_needs_update = 0;
  container->initial_draw_cbs.clear();
  container->container_lock = new mozilla::Mutex("MozContainer lock");
}

static void moz_container_wayland_destroy(GtkWidget* widget) {
  MozContainerWayland* container = &MOZ_CONTAINER(widget)->wl_container;
  delete container->container_lock;
  container->container_lock = nullptr;
}

void moz_container_wayland_add_initial_draw_callback(
    MozContainer* container, const std::function<void(void)>& initial_draw_cb) {
  container->wl_container.initial_draw_cbs.push_back(initial_draw_cb);
}

wl_surface* moz_gtk_widget_get_wl_surface(GtkWidget* aWidget) {
  static auto sGdkWaylandWindowGetWlSurface = (wl_surface * (*)(GdkWindow*))
      dlsym(RTLD_DEFAULT, "gdk_wayland_window_get_wl_surface");

  GdkWindow* window = gtk_widget_get_window(aWidget);
  wl_surface* surface = sGdkWaylandWindowGetWlSurface(window);

  LOGWAYLAND(("moz_gtk_widget_get_wl_surface [%p] wl_surface %p ID %d\n",
              (void*)aWidget, (void*)surface,
              surface ? wl_proxy_get_id((struct wl_proxy*)surface) : -1));

  return surface;
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
  wl_container->frame_callback_handler_surface_id = -1;

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

static void moz_container_wayland_request_parent_frame_callback(
    MozContainer* container) {
  MozContainerWayland* wl_container = &container->wl_container;

  wl_surface* gtk_container_surface =
      moz_gtk_widget_get_wl_surface(GTK_WIDGET(container));
  int gtk_container_surface_id =
      gtk_container_surface
          ? wl_proxy_get_id((struct wl_proxy*)gtk_container_surface)
          : -1;

  LOGWAYLAND(
      ("%s [%p] frame_callback_handler %p "
       "frame_callback_handler_surface_id %d\n",
       __FUNCTION__, (void*)container, wl_container->frame_callback_handler,
       wl_container->frame_callback_handler_surface_id));

  if (wl_container->frame_callback_handler &&
      wl_container->frame_callback_handler_surface_id ==
          gtk_container_surface_id) {
    return;
  }

  // If there's pending frame callback, delete it.
  if (wl_container->frame_callback_handler) {
    g_clear_pointer(&wl_container->frame_callback_handler, wl_callback_destroy);
  }

  if (gtk_container_surface) {
    wl_container->frame_callback_handler_surface_id = gtk_container_surface_id;
    wl_container->frame_callback_handler =
        wl_surface_frame(gtk_container_surface);
    wl_callback_add_listener(wl_container->frame_callback_handler,
                             &moz_container_frame_listener, container);
  } else {
    wl_container->frame_callback_handler_surface_id = -1;
  }
}

static gboolean moz_container_wayland_map_event(GtkWidget* widget,
                                                GdkEventAny* event) {
  MozContainerWayland* wl_container = &MOZ_CONTAINER(widget)->wl_container;

  LOGWAYLAND(("%s begin [%p] ready_to_draw %d\n", __FUNCTION__,
              (void*)MOZ_CONTAINER(widget), wl_container->ready_to_draw));

  if (wl_container->ready_to_draw) {
    return FALSE;
  }

  moz_container_wayland_request_parent_frame_callback(MOZ_CONTAINER(widget));
  return FALSE;
}

static void moz_container_wayland_unmap_internal(MozContainer* container) {
  MozContainerWayland* wl_container = &container->wl_container;
  MutexAutoLock lock(*wl_container->container_lock);

  g_clear_pointer(&wl_container->eglwindow, wl_egl_window_destroy);
  g_clear_pointer(&wl_container->subsurface, wl_subsurface_destroy);
  g_clear_pointer(&wl_container->surface, wl_surface_destroy);
  g_clear_pointer(&wl_container->frame_callback_handler, wl_callback_destroy);
  wl_container->frame_callback_handler_surface_id = -1;

  wl_container->surface_needs_clear = true;
  wl_container->ready_to_draw = false;

  LOGWAYLAND(("%s [%p]\n", __FUNCTION__, (void*)container));
}

void moz_container_wayland_map(GtkWidget* widget) {
  g_return_if_fail(IS_MOZ_CONTAINER(widget));
  gtk_widget_set_mapped(widget, TRUE);

  if (gtk_widget_get_has_window(widget)) {
    gdk_window_show(gtk_widget_get_window(widget));
    moz_container_wayland_map_event(widget, nullptr);
  }
}

void moz_container_wayland_unmap(GtkWidget* widget) {
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
    if (gfxPlatformGtk::GetPlatform()->IsWaylandDisplay()) {
      moz_container_wayland_move(MOZ_CONTAINER(widget), allocation->x,
                                 allocation->y);
    }
  }
}

static void moz_container_wayland_set_opaque_region_locked(
    MozContainer* container) {
  MozContainerWayland* wl_container = &container->wl_container;

  if (!wl_container->opaque_region_needs_update || !wl_container->surface) {
    return;
  }

  GtkAllocation allocation;
  gtk_widget_get_allocation(GTK_WIDGET(container), &allocation);

  // Set region to mozcontainer for normal state only
  if (!wl_container->opaque_region_fullscreen) {
    wl_region* region =
        CreateOpaqueRegionWayland(0, 0, allocation.width, allocation.height,
                                  wl_container->opaque_region_subtract_corners);
    wl_surface_set_opaque_region(wl_container->surface, region);
    wl_region_destroy(region);
  } else {
    wl_surface_set_opaque_region(wl_container->surface, nullptr);
  }

  wl_container->opaque_region_needs_update = false;
}

static void moz_container_wayland_set_opaque_region(MozContainer* container) {
  MutexAutoLock lock(*container->wl_container.container_lock);
  moz_container_wayland_set_opaque_region_locked(container);
}

static int moz_gtk_widget_get_scale_factor(MozContainer* container) {
  static auto sGtkWidgetGetScaleFactor =
      (gint(*)(GtkWidget*))dlsym(RTLD_DEFAULT, "gtk_widget_get_scale_factor");
  return sGtkWidgetGetScaleFactor
             ? sGtkWidgetGetScaleFactor(GTK_WIDGET(container))
             : 1;
}

static void moz_container_wayland_set_scale_factor_locked(
    MozContainer* container) {
  if (!container->wl_container.surface) {
    return;
  }
  wl_surface_set_buffer_scale(container->wl_container.surface,
                              moz_gtk_widget_get_scale_factor(container));
}

void moz_container_wayland_set_scale_factor(MozContainer* container) {
  MutexAutoLock lock(*container->wl_container.container_lock);
  moz_container_wayland_set_scale_factor_locked(container);
}

static struct wl_surface* moz_container_wayland_get_surface_locked(
    MozContainer* container, nsWaylandDisplay* aWaylandDisplay) {
  MozContainerWayland* wl_container = &container->wl_container;

  LOGWAYLAND(("%s [%p] surface %p ready_to_draw %d\n", __FUNCTION__,
              (void*)container, (void*)wl_container->surface,
              wl_container->ready_to_draw));

  if (!wl_container->surface) {
    if (!wl_container->ready_to_draw) {
      moz_container_wayland_request_parent_frame_callback(container);
      return nullptr;
    }
    wl_surface* parent_surface =
        moz_gtk_widget_get_wl_surface(GTK_WIDGET(container));
    if (!parent_surface) {
      return nullptr;
    }

    // Available as of GTK 3.8+
    struct wl_compositor* compositor = aWaylandDisplay->GetCompositor();
    wl_container->surface = wl_compositor_create_surface(compositor);
    if (!wl_container->surface) {
      return nullptr;
    }

    wl_container->subsurface =
        wl_subcompositor_get_subsurface(aWaylandDisplay->GetSubcompositor(),
                                        wl_container->surface, parent_surface);
    if (!wl_container->subsurface) {
      g_clear_pointer(&wl_container->surface, wl_surface_destroy);
      return nullptr;
    }

    GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
    gint x, y;
    gdk_window_get_position(window, &x, &y);
    moz_container_wayland_move_locked(container, x, y);
    wl_subsurface_set_desync(wl_container->subsurface);

    // Route input to parent wl_surface owned by Gtk+ so we get input
    // events from Gtk+.
    wl_region* region = wl_compositor_create_region(compositor);
    wl_surface_set_input_region(wl_container->surface, region);
    wl_region_destroy(region);

    wl_surface_commit(wl_container->surface);
    wl_display_flush(aWaylandDisplay->GetDisplay());

    LOGWAYLAND(("%s [%p] created surface %p\n", __FUNCTION__, (void*)container,
                (void*)wl_container->surface));
  }

  if (wl_container->surface_position_needs_update) {
    moz_container_wayland_move_locked(container, wl_container->subsurface_dx,
                                      wl_container->subsurface_dy);
  }

  moz_container_wayland_set_opaque_region_locked(container);
  moz_container_wayland_set_scale_factor_locked(container);

  return wl_container->surface;
}

struct wl_surface* moz_container_wayland_surface_lock(MozContainer* container) {
  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(container));
  nsWaylandDisplay* waylandDisplay = WaylandDisplayGet(display);

  LOGWAYLAND(("%s [%p] surface %p\n", __FUNCTION__, (void*)container,
              (void*)container->wl_container.surface));

  container->wl_container.container_lock->Lock();
  struct wl_surface* surface =
      moz_container_wayland_get_surface_locked(container, waylandDisplay);
  if (surface == nullptr) {
    container->wl_container.container_lock->Unlock();
  }
  return surface;
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
    MozContainer* container, int scale) {
  GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(container));
  nsWaylandDisplay* waylandDisplay = WaylandDisplayGet(display);
  MozContainerWayland* wl_container = &container->wl_container;

  LOGWAYLAND(("%s [%p] eglwindow %p\n", __FUNCTION__, (void*)container,
              (void*)wl_container->eglwindow));

  MutexAutoLock lock(*wl_container->container_lock);

  // Always call moz_container_get_wl_surface() to ensure underlying
  // container->surface has correct scale and position.
  wl_surface* surface =
      moz_container_wayland_get_surface_locked(container, waylandDisplay);
  if (!surface) {
    return nullptr;
  }
  if (!wl_container->eglwindow) {
    GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
    wl_container->eglwindow =
        wl_egl_window_create(surface, gdk_window_get_width(window) * scale,
                             gdk_window_get_height(window) * scale);

    LOGWAYLAND(("%s [%p] created eglwindow %p\n", __FUNCTION__,
                (void*)container, (void*)wl_container->eglwindow));
  }

  return wl_container->eglwindow;
}

gboolean moz_container_wayland_has_egl_window(MozContainer* container) {
  return container->wl_container.eglwindow ? true : false;
}

gboolean moz_container_wayland_surface_needs_clear(MozContainer* container) {
  int ret = container->wl_container.surface_needs_clear;
  container->wl_container.surface_needs_clear = false;
  return ret;
}

void moz_container_wayland_update_opaque_region(MozContainer* container,
                                                bool aSubtractCorners,
                                                bool aFullScreen) {
  MozContainerWayland* wl_container = &container->wl_container;
  wl_container->opaque_region_needs_update = true;
  wl_container->opaque_region_subtract_corners = aSubtractCorners;
  wl_container->opaque_region_fullscreen = aFullScreen;

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
