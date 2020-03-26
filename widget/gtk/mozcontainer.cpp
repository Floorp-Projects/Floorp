/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozcontainer.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#ifdef MOZ_WAYLAND
#  include "nsWaylandDisplay.h"
#  include "gfxPlatformGtk.h"
#  include <wayland-egl.h>
#endif
#include <stdio.h>
#include <dlfcn.h>

#ifdef ACCESSIBILITY
#  include <atk/atk.h>
#  include "maiRedundantObjectFactory.h"
#endif

#undef LOG
#ifdef MOZ_LOGGING

#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetLog;
extern mozilla::LazyLogModule gWidgetWaylandLog;
#  define LOG(args) MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, args)
#  define LOGWAYLAND(args) \
    MOZ_LOG(gWidgetWaylandLog, mozilla::LogLevel::Debug, args)
#else
#  define LOG(args)
#  define LOGWAYLAND(args)
#endif /* MOZ_LOGGING */

#ifdef MOZ_WAYLAND
using namespace mozilla;
using namespace mozilla::widget;
#endif

#ifdef MOZ_WAYLAND
// Declaration from nsWindow, we don't want to include whole nsWindow.h file
// here just for it.
wl_region* CreateOpaqueRegionWayland(int aX, int aY, int aWidth, int aHeight,
                                     bool aSubtractCorners);
#endif

/* init methods */
static void moz_container_class_init(MozContainerClass* klass);
static void moz_container_init(MozContainer* container);

/* widget class methods */
static void moz_container_map(GtkWidget* widget);
#if defined(MOZ_WAYLAND)
static gboolean moz_container_map_wayland(GtkWidget* widget,
                                          GdkEventAny* event);
#endif
static void moz_container_unmap(GtkWidget* widget);
static void moz_container_realize(GtkWidget* widget);
static void moz_container_size_allocate(GtkWidget* widget,
                                        GtkAllocation* allocation);

/* container class methods */
static void moz_container_remove(GtkContainer* container,
                                 GtkWidget* child_widget);
static void moz_container_forall(GtkContainer* container,
                                 gboolean include_internals,
                                 GtkCallback callback, gpointer callback_data);
static void moz_container_add(GtkContainer* container, GtkWidget* widget);

typedef struct _MozContainerChild MozContainerChild;

struct _MozContainerChild {
  GtkWidget* widget;
  gint x;
  gint y;
};

static void moz_container_allocate_child(MozContainer* container,
                                         MozContainerChild* child);
static MozContainerChild* moz_container_get_child(MozContainer* container,
                                                  GtkWidget* child);

/* public methods */

GType moz_container_get_type(void) {
  static GType moz_container_type = 0;

  if (!moz_container_type) {
    static GTypeInfo moz_container_info = {
        sizeof(MozContainerClass),                /* class_size */
        NULL,                                     /* base_init */
        NULL,                                     /* base_finalize */
        (GClassInitFunc)moz_container_class_init, /* class_init */
        NULL,                                     /* class_destroy */
        NULL,                                     /* class_data */
        sizeof(MozContainer),                     /* instance_size */
        0,                                        /* n_preallocs */
        (GInstanceInitFunc)moz_container_init,    /* instance_init */
        NULL,                                     /* value_table */
    };

    moz_container_type =
        g_type_register_static(GTK_TYPE_CONTAINER, "MozContainer",
                               &moz_container_info, static_cast<GTypeFlags>(0));
#ifdef ACCESSIBILITY
    /* Set a factory to return accessible object with ROLE_REDUNDANT for
     * MozContainer, so that gail won't send focus notification for it */
    atk_registry_set_factory_type(atk_get_default_registry(),
                                  moz_container_type,
                                  mai_redundant_object_factory_get_type());
#endif
  }

  return moz_container_type;
}

GtkWidget* moz_container_new(void) {
  MozContainer* container;

  container =
      static_cast<MozContainer*>(g_object_new(MOZ_CONTAINER_TYPE, nullptr));

  return GTK_WIDGET(container);
}

void moz_container_put(MozContainer* container, GtkWidget* child_widget, gint x,
                       gint y) {
  MozContainerChild* child;

  child = g_new(MozContainerChild, 1);

  child->widget = child_widget;
  child->x = x;
  child->y = y;

  /*  printf("moz_container_put %p %p %d %d\n", (void *)container,
      (void *)child_widget, x, y); */

  container->children = g_list_append(container->children, child);

  /* we assume that the caller of this function will have already set
     the parent GdkWindow because we can have many anonymous children. */
  gtk_widget_set_parent(child_widget, GTK_WIDGET(container));
}

#if defined(MOZ_WAYLAND)
void moz_container_move(MozContainer* container, int dx, int dy) {
  LOGWAYLAND(("moz_container_move [%p] %d,%d\n", (void*)container, dx, dy));

  container->subsurface_dx = dx;
  container->subsurface_dy = dy;
  container->surface_position_needs_update = true;

  // Wayland subsurface is not created yet.
  if (!container->subsurface) {
    return;
  }

  // wl_subsurface_set_position is actually property of parent surface
  // which is effective when parent surface is commited.
  wl_subsurface_set_position(container->subsurface, container->subsurface_dx,
                             container->subsurface_dy);
  container->surface_position_needs_update = false;

  GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
  if (window) {
    GdkRectangle rect = (GdkRectangle){0, 0, gdk_window_get_width(window),
                                       gdk_window_get_height(window)};
    gdk_window_invalidate_rect(window, &rect, false);
  }
}

// This is called from layout/compositor code only with
// size equal to GL rendering context. Otherwise there are
// rendering artifacts as wl_egl_window size does not match
// GL rendering pipeline setup.
void moz_container_egl_window_set_size(MozContainer* container, int width,
                                       int height) {
  if (container->eglwindow) {
    wl_egl_window_resize(container->eglwindow, width, height, 0, 0);
  }
}
#endif

void moz_container_class_init(MozContainerClass* klass) {
  /*GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass); */
  GtkContainerClass* container_class = GTK_CONTAINER_CLASS(klass);
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

  widget_class->map = moz_container_map;
#if defined(MOZ_WAYLAND)
  if (gfxPlatformGtk::GetPlatform()->IsWaylandDisplay()) {
    widget_class->map_event = moz_container_map_wayland;
  }
#endif
  widget_class->unmap = moz_container_unmap;
  widget_class->realize = moz_container_realize;
  widget_class->size_allocate = moz_container_size_allocate;

  container_class->remove = moz_container_remove;
  container_class->forall = moz_container_forall;
  container_class->add = moz_container_add;
}

void moz_container_init(MozContainer* container) {
  gtk_widget_set_can_focus(GTK_WIDGET(container), TRUE);
  gtk_container_set_resize_mode(GTK_CONTAINER(container), GTK_RESIZE_IMMEDIATE);
  gtk_widget_set_redraw_on_allocate(GTK_WIDGET(container), FALSE);

#if defined(MOZ_WAYLAND)
  container->surface = nullptr;
  container->subsurface = nullptr;
  container->eglwindow = nullptr;
  container->frame_callback_handler = nullptr;
  container->frame_callback_handler_surface_id = -1;
  // We can draw to x11 window any time.
  container->ready_to_draw = gfxPlatformGtk::GetPlatform()->IsX11Display();
  container->opaque_region_needs_update = false;
  container->opaque_region_subtract_corners = false;
  container->opaque_region_fullscreen = false;
  container->surface_needs_clear = true;
  container->subsurface_dx = 0;
  container->subsurface_dy = 0;
  container->surface_position_needs_update = 0;
  container->initial_draw_cbs.clear();
#endif

  LOG(("%s [%p]\n", __FUNCTION__, (void*)container));
}

#if defined(MOZ_WAYLAND)
void moz_container_add_initial_draw_callback(
    MozContainer* container, const std::function<void(void)>& initial_draw_cb) {
  container->initial_draw_cbs.push_back(initial_draw_cb);
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

static void moz_container_frame_callback_handler(void* data,
                                                 struct wl_callback* callback,
                                                 uint32_t time) {
  MozContainer* container = MOZ_CONTAINER(data);

  LOGWAYLAND(
      ("%s [%p] frame_callback_handler %p ready_to_draw %d (set to true)"
       " initial_draw callback %zd\n",
       __FUNCTION__, (void*)container, (void*)container->frame_callback_handler,
       container->ready_to_draw, container->initial_draw_cbs.size()));

  g_clear_pointer(&container->frame_callback_handler, wl_callback_destroy);
  container->frame_callback_handler_surface_id = -1;

  if (!container->ready_to_draw) {
    container->ready_to_draw = true;
    for (auto const& cb : container->initial_draw_cbs) {
      cb();
    }
    container->initial_draw_cbs.clear();
  }
}

static const struct wl_callback_listener moz_container_frame_listener = {
    moz_container_frame_callback_handler};

static void moz_container_request_parent_frame_callback(
    MozContainer* container) {
  wl_surface* gtk_container_surface =
      moz_gtk_widget_get_wl_surface(GTK_WIDGET(container));
  int gtk_container_surface_id =
      gtk_container_surface
          ? wl_proxy_get_id((struct wl_proxy*)gtk_container_surface)
          : -1;

  LOGWAYLAND(
      ("%s [%p] frame_callback_handler %p "
       "frame_callback_handler_surface_id %d\n",
       __FUNCTION__, (void*)container, container->frame_callback_handler,
       container->frame_callback_handler_surface_id));

  if (container->frame_callback_handler &&
      container->frame_callback_handler_surface_id ==
          gtk_container_surface_id) {
    return;
  }

  // If there's pending frame callback, delete it.
  if (container->frame_callback_handler) {
    g_clear_pointer(&container->frame_callback_handler, wl_callback_destroy);
  }

  if (gtk_container_surface) {
    container->frame_callback_handler_surface_id = gtk_container_surface_id;
    container->frame_callback_handler = wl_surface_frame(gtk_container_surface);
    wl_callback_add_listener(container->frame_callback_handler,
                             &moz_container_frame_listener, container);
  } else {
    container->frame_callback_handler_surface_id = -1;
  }
}

static gboolean moz_container_map_wayland(GtkWidget* widget,
                                          GdkEventAny* event) {
  MozContainer* container = MOZ_CONTAINER(widget);

  LOGWAYLAND(("%s begin [%p] ready_to_draw %d\n", __FUNCTION__,
              (void*)container, container->ready_to_draw));

  if (container->ready_to_draw) {
    return FALSE;
  }

  moz_container_request_parent_frame_callback(MOZ_CONTAINER(widget));
  return FALSE;
}

static void moz_container_unmap_wayland(MozContainer* container) {
  g_clear_pointer(&container->eglwindow, wl_egl_window_destroy);
  g_clear_pointer(&container->subsurface, wl_subsurface_destroy);
  g_clear_pointer(&container->surface, wl_surface_destroy);
  g_clear_pointer(&container->frame_callback_handler, wl_callback_destroy);
  container->frame_callback_handler_surface_id = -1;

  container->surface_needs_clear = true;
  container->ready_to_draw = false;

  LOGWAYLAND(("%s [%p]\n", __FUNCTION__, (void*)container));
}
#endif

void moz_container_map(GtkWidget* widget) {
  MozContainer* container;
  GList* tmp_list;
  GtkWidget* tmp_child;

  g_return_if_fail(IS_MOZ_CONTAINER(widget));
  container = MOZ_CONTAINER(widget);

  gtk_widget_set_mapped(widget, TRUE);

  tmp_list = container->children;
  while (tmp_list) {
    tmp_child = ((MozContainerChild*)tmp_list->data)->widget;

    if (gtk_widget_get_visible(tmp_child)) {
      if (!gtk_widget_get_mapped(tmp_child)) gtk_widget_map(tmp_child);
    }
    tmp_list = tmp_list->next;
  }

  if (gtk_widget_get_has_window(widget)) {
    gdk_window_show(gtk_widget_get_window(widget));
#if defined(MOZ_WAYLAND)
    if (gfxPlatformGtk::GetPlatform()->IsWaylandDisplay()) {
      moz_container_map_wayland(widget, nullptr);
    }
#endif
  }
}

void moz_container_unmap(GtkWidget* widget) {
  g_return_if_fail(IS_MOZ_CONTAINER(widget));

  gtk_widget_set_mapped(widget, FALSE);

  if (gtk_widget_get_has_window(widget)) {
    gdk_window_hide(gtk_widget_get_window(widget));
#if defined(MOZ_WAYLAND)
    if (gfxPlatformGtk::GetPlatform()->IsWaylandDisplay()) {
      moz_container_unmap_wayland(MOZ_CONTAINER(widget));
    }
#endif
  }
}

void moz_container_realize(GtkWidget* widget) {
  GdkWindow* parent = gtk_widget_get_parent_window(widget);
  GdkWindow* window;

  gtk_widget_set_realized(widget, TRUE);

  if (gtk_widget_get_has_window(widget)) {
    GdkWindowAttr attributes;
    gint attributes_mask = GDK_WA_VISUAL | GDK_WA_X | GDK_WA_Y;
    GtkAllocation allocation;

    gtk_widget_get_allocation(widget, &allocation);
    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    MozContainer* container = MOZ_CONTAINER(widget);
    attributes.visual =
        container->force_default_visual
            ? gdk_screen_get_system_visual(gtk_widget_get_screen(widget))
            : gtk_widget_get_visual(widget);

    window = gdk_window_new(parent, &attributes, attributes_mask);

    LOG(("moz_container_realize() [%p] GdkWindow %p\n", (void*)container,
         (void*)window));

    gdk_window_set_user_data(window, widget);
  } else {
    window = parent;
    g_object_ref(window);
  }

  gtk_widget_set_window(widget, window);
}

void moz_container_size_allocate(GtkWidget* widget, GtkAllocation* allocation) {
  MozContainer* container;
  GList* tmp_list;
  GtkAllocation tmp_allocation;

  g_return_if_fail(IS_MOZ_CONTAINER(widget));

  LOG(("moz_container_size_allocate [%p] %d,%d -> %d x %d\n", (void*)widget,
       allocation->x, allocation->y, allocation->width, allocation->height));

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

  tmp_list = container->children;

  while (tmp_list) {
    MozContainerChild* child = static_cast<MozContainerChild*>(tmp_list->data);

    moz_container_allocate_child(container, child);

    tmp_list = tmp_list->next;
  }

  if (gtk_widget_get_has_window(widget) && gtk_widget_get_realized(widget)) {
    gdk_window_move_resize(gtk_widget_get_window(widget), allocation->x,
                           allocation->y, allocation->width,
                           allocation->height);
#if defined(MOZ_WAYLAND)
    // We need to position our subsurface according to GdkWindow
    // when offset changes (GdkWindow is maximized for instance).
    // see gtk-clutter-embed.c for reference.
    moz_container_move(MOZ_CONTAINER(widget), allocation->x, allocation->y);
#endif
  }
}

void moz_container_remove(GtkContainer* container, GtkWidget* child_widget) {
  MozContainerChild* child;
  MozContainer* moz_container;
  GdkWindow* parent_window;

  g_return_if_fail(IS_MOZ_CONTAINER(container));
  g_return_if_fail(GTK_IS_WIDGET(child_widget));

  moz_container = MOZ_CONTAINER(container);

  child = moz_container_get_child(moz_container, child_widget);
  g_return_if_fail(child);

  /* gtk_widget_unparent will remove the parent window (as well as the
   * parent widget), but, in Mozilla's window hierarchy, the parent window
   * may need to be kept because it may be part of a GdkWindow sub-hierarchy
   * that is being moved to another MozContainer.
   *
   * (In a conventional GtkWidget hierarchy, GdkWindows being reparented
   * would have their own GtkWidget and that widget would be the one being
   * reparented.  In Mozilla's hierarchy, the parent_window needs to be
   * retained so that the GdkWindow sub-hierarchy is maintained.)
   */
  parent_window = gtk_widget_get_parent_window(child_widget);
  if (parent_window) g_object_ref(parent_window);

  gtk_widget_unparent(child_widget);

  if (parent_window) {
    /* The child_widget will always still exist because g_signal_emit,
     * which invokes this function, holds a reference.
     *
     * If parent_window is the container's root window then it will not be
     * the parent_window if the child_widget is placed in another
     * container.
     */
    if (parent_window != gtk_widget_get_window(GTK_WIDGET(container)))
      gtk_widget_set_parent_window(child_widget, parent_window);

    g_object_unref(parent_window);
  }

  moz_container->children = g_list_remove(moz_container->children, child);
  g_free(child);
}

void moz_container_forall(GtkContainer* container, gboolean include_internals,
                          GtkCallback callback, gpointer callback_data) {
  MozContainer* moz_container;
  GList* tmp_list;

  g_return_if_fail(IS_MOZ_CONTAINER(container));
  g_return_if_fail(callback != NULL);

  moz_container = MOZ_CONTAINER(container);

  tmp_list = moz_container->children;
  while (tmp_list) {
    MozContainerChild* child;
    child = static_cast<MozContainerChild*>(tmp_list->data);
    tmp_list = tmp_list->next;
    (*callback)(child->widget, callback_data);
  }
}

static void moz_container_allocate_child(MozContainer* container,
                                         MozContainerChild* child) {
  GtkAllocation allocation;

  gtk_widget_get_allocation(child->widget, &allocation);
  allocation.x = child->x;
  allocation.y = child->y;

  gtk_widget_size_allocate(child->widget, &allocation);
}

MozContainerChild* moz_container_get_child(MozContainer* container,
                                           GtkWidget* child_widget) {
  GList* tmp_list;

  tmp_list = container->children;
  while (tmp_list) {
    MozContainerChild* child;

    child = static_cast<MozContainerChild*>(tmp_list->data);
    tmp_list = tmp_list->next;

    if (child->widget == child_widget) return child;
  }

  return NULL;
}

static void moz_container_add(GtkContainer* container, GtkWidget* widget) {
  moz_container_put(MOZ_CONTAINER(container), widget, 0, 0);
}

#ifdef MOZ_WAYLAND
static void moz_container_set_opaque_region(MozContainer* container) {
  if (!container->opaque_region_needs_update || !container->surface) {
    return;
  }

  GtkAllocation allocation;
  gtk_widget_get_allocation(GTK_WIDGET(container), &allocation);

  // Set region to mozcontainer for normal state only
  if (!container->opaque_region_fullscreen) {
    wl_region* region =
        CreateOpaqueRegionWayland(0, 0, allocation.width, allocation.height,
                                  container->opaque_region_subtract_corners);
    wl_surface_set_opaque_region(container->surface, region);
    wl_region_destroy(region);
  } else {
    wl_surface_set_opaque_region(container->surface, nullptr);
  }

  container->opaque_region_needs_update = false;
}

static int moz_gtk_widget_get_scale_factor(MozContainer* container) {
  static auto sGtkWidgetGetScaleFactor =
      (gint(*)(GtkWidget*))dlsym(RTLD_DEFAULT, "gtk_widget_get_scale_factor");
  return sGtkWidgetGetScaleFactor
             ? sGtkWidgetGetScaleFactor(GTK_WIDGET(container))
             : 1;
}

void moz_container_set_scale_factor(MozContainer* container) {
  if (!container->surface) {
    return;
  }
  wl_surface_set_buffer_scale(container->surface,
                              moz_gtk_widget_get_scale_factor(container));
}

struct wl_surface* moz_container_get_wl_surface(MozContainer* container) {
  LOGWAYLAND(("%s [%p] surface %p ready_to_draw %d\n", __FUNCTION__,
              (void*)container, (void*)container->surface,
              container->ready_to_draw));

  if (!container->surface) {
    if (!container->ready_to_draw) {
      moz_container_request_parent_frame_callback(container);
      return nullptr;
    }
    GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(container));
    nsWaylandDisplay* waylandDisplay = WaylandDisplayGet(display);

    // Available as of GTK 3.8+
    struct wl_compositor* compositor = waylandDisplay->GetCompositor();
    container->surface = wl_compositor_create_surface(compositor);
    wl_surface* parent_surface =
        moz_gtk_widget_get_wl_surface(GTK_WIDGET(container));
    if (!container->surface || !parent_surface) {
      return nullptr;
    }

    container->subsurface = wl_subcompositor_get_subsurface(
        waylandDisplay->GetSubcompositor(), container->surface, parent_surface);

    GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
    gint x, y;
    gdk_window_get_position(window, &x, &y);
    moz_container_move(container, x, y);
    wl_subsurface_set_desync(container->subsurface);

    // Route input to parent wl_surface owned by Gtk+ so we get input
    // events from Gtk+.
    wl_region* region = wl_compositor_create_region(compositor);
    wl_surface_set_input_region(container->surface, region);
    wl_region_destroy(region);

    wl_surface_commit(container->surface);
    wl_display_flush(waylandDisplay->GetDisplay());

    LOGWAYLAND(("%s [%p] created surface %p\n", __FUNCTION__, (void*)container,
                (void*)container->surface));
  }

  if (container->surface_position_needs_update) {
    moz_container_move(container, container->subsurface_dx,
                       container->subsurface_dy);
  }

  moz_container_set_opaque_region(container);
  moz_container_set_scale_factor(container);

  return container->surface;
}

struct wl_egl_window* moz_container_get_wl_egl_window(MozContainer* container,
                                                      int scale) {
  LOGWAYLAND(("%s [%p] eglwindow %p\n", __FUNCTION__, (void*)container,
              (void*)container->eglwindow));

  // Always call moz_container_get_wl_surface() to ensure underlying
  // container->surface has correct scale and position.
  wl_surface* surface = moz_container_get_wl_surface(container);
  if (!surface) {
    return nullptr;
  }
  if (!container->eglwindow) {
    GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
    container->eglwindow =
        wl_egl_window_create(surface, gdk_window_get_width(window) * scale,
                             gdk_window_get_height(window) * scale);

    LOGWAYLAND(("%s [%p] created eglwindow %p\n", __FUNCTION__,
                (void*)container, (void*)container->eglwindow));
  }

  return container->eglwindow;
}

gboolean moz_container_has_wl_egl_window(MozContainer* container) {
  return container->eglwindow ? true : false;
}

gboolean moz_container_surface_needs_clear(MozContainer* container) {
  int ret = container->surface_needs_clear;
  container->surface_needs_clear = false;
  return ret;
}

void moz_container_update_opaque_region(MozContainer* container,
                                        bool aSubtractCorners,
                                        bool aFullScreen) {
  container->opaque_region_needs_update = true;
  container->opaque_region_subtract_corners = aSubtractCorners;
  container->opaque_region_fullscreen = aFullScreen;

  // When GL compositor / WebRender is used,
  // moz_container_get_wl_egl_window() is called only once when window
  // is created or resized so update opaque region now.
  if (moz_container_has_wl_egl_window(container)) {
    moz_container_set_opaque_region(container);
  }
}
#endif

void moz_container_force_default_visual(MozContainer* container) {
  container->force_default_visual = true;
}
