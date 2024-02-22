/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozContainer.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include "mozilla/WidgetUtilsGtk.h"
#include "nsWindow.h"

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetLog;
#  define LOGCONTAINER(args) MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, args)
#else
#  define LOGCONTAINER(args)
#endif /* MOZ_LOGGING */

/* init methods */
void moz_container_class_init(MozContainerClass* klass);
static void moz_container_init(MozContainer* container);

/* widget class methods */
static void moz_container_map(GtkWidget* widget);
void moz_container_unmap(GtkWidget* widget);
static void moz_container_size_allocate(GtkWidget* widget,
                                        GtkAllocation* allocation);
static void moz_container_realize(GtkWidget* widget);
static void moz_container_unrealize(GtkWidget* widget);

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
  }

  return moz_container_type;
}

GtkWidget* moz_container_new(void) {
  MozContainer* container;

  container =
      static_cast<MozContainer*>(g_object_new(MOZ_CONTAINER_TYPE, nullptr));

  return GTK_WIDGET(container);
}

static void moz_container_destroy(GtkWidget* widget) {
  auto* container = MOZ_CONTAINER(widget);
  if (container->destroyed) {
    return;  // The destroy signal may run multiple times.
  }
  LOGCONTAINER(("moz_container_destroy() [%p]\n",
                (void*)moz_container_get_nsWindow(MOZ_CONTAINER(widget))));
  container->destroyed = TRUE;
  container->data.~Data();
}

void moz_container_class_init(MozContainerClass* klass) {
  /*GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass); */
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

  widget_class->realize = moz_container_realize;
  widget_class->unrealize = moz_container_unrealize;
  widget_class->destroy = moz_container_destroy;

#ifdef MOZ_WAYLAND
  if (mozilla::widget::GdkIsWaylandDisplay()) {
    widget_class->map = moz_container_wayland_map;
    widget_class->size_allocate = moz_container_wayland_size_allocate;
    widget_class->map_event = moz_container_wayland_map_event;
    widget_class->unmap = moz_container_wayland_unmap;
  } else {
#endif
    widget_class->map = moz_container_map;
    widget_class->size_allocate = moz_container_size_allocate;
    widget_class->unmap = moz_container_unmap;
#ifdef MOZ_WAYLAND
  }
#endif
}

void moz_container_init(MozContainer* container) {
  container->destroyed = FALSE;
  new (&container->data) MozContainer::Data();
  gtk_widget_set_can_focus(GTK_WIDGET(container), TRUE);
  gtk_widget_set_redraw_on_allocate(GTK_WIDGET(container), FALSE);
  LOGCONTAINER(("%s [%p]\n", __FUNCTION__,
                (void*)moz_container_get_nsWindow(container)));
}

void moz_container_map(GtkWidget* widget) {
  MozContainer* container;

  g_return_if_fail(IS_MOZ_CONTAINER(widget));
  container = MOZ_CONTAINER(widget);

  LOGCONTAINER(("moz_container_map() [%p]",
                (void*)moz_container_get_nsWindow(container)));

  gtk_widget_set_mapped(widget, TRUE);

  if (gtk_widget_get_has_window(widget)) {
    gdk_window_show(gtk_widget_get_window(widget));
  }
}

void moz_container_unmap(GtkWidget* widget) {
  g_return_if_fail(IS_MOZ_CONTAINER(widget));

  LOGCONTAINER(("moz_container_unmap() [%p]",
                (void*)moz_container_get_nsWindow(MOZ_CONTAINER(widget))));

  // Disable rendering to MozContainer before we unmap it.
  nsWindow* window = moz_container_get_nsWindow(MOZ_CONTAINER(widget));
  window->DisableRendering();

  gtk_widget_set_mapped(widget, FALSE);

  if (gtk_widget_get_has_window(widget)) {
    gdk_window_hide(gtk_widget_get_window(widget));
  }
}

void moz_container_realize(GtkWidget* widget) {
  GdkWindow* parent = gtk_widget_get_parent_window(widget);
  GdkWindow* window;

  gtk_widget_set_realized(widget, TRUE);

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
      container->data.force_default_visual
          ? gdk_screen_get_system_visual(gtk_widget_get_screen(widget))
          : gtk_widget_get_visual(widget);

  window = gdk_window_new(parent, &attributes, attributes_mask);

  LOGCONTAINER(("moz_container_realize() [%p] GdkWindow %p\n",
                (void*)moz_container_get_nsWindow(container), (void*)window));

  gtk_widget_register_window(widget, window);
  gtk_widget_set_window(widget, window);
}

void moz_container_unrealize(GtkWidget* widget) {
  GdkWindow* window = gtk_widget_get_window(widget);
  LOGCONTAINER(("moz_container_unrealize() [%p] GdkWindow %p\n",
                (void*)moz_container_get_nsWindow(MOZ_CONTAINER(widget)),
                (void*)window));

  if (gtk_widget_get_mapped(widget)) {
    gtk_widget_unmap(widget);
  }

  gtk_widget_unregister_window(widget, window);
  gtk_widget_set_window(widget, nullptr);
  gdk_window_destroy(window);
  gtk_widget_set_realized(widget, false);
}

void moz_container_size_allocate(GtkWidget* widget, GtkAllocation* allocation) {
  GtkAllocation tmp_allocation;

  g_return_if_fail(IS_MOZ_CONTAINER(widget));

  LOGCONTAINER(("moz_container_size_allocate [%p] %d,%d -> %d x %d\n",
                (void*)moz_container_get_nsWindow(MOZ_CONTAINER(widget)),
                allocation->x, allocation->y, allocation->width,
                allocation->height));

  /* short circuit if you can */
  gtk_widget_get_allocation(widget, &tmp_allocation);
  if (tmp_allocation.x == allocation->x && tmp_allocation.y == allocation->y &&
      tmp_allocation.width == allocation->width &&
      tmp_allocation.height == allocation->height) {
    return;
  }

  gtk_widget_set_allocation(widget, allocation);

  if (gtk_widget_get_has_window(widget) && gtk_widget_get_realized(widget)) {
    gdk_window_move_resize(gtk_widget_get_window(widget), allocation->x,
                           allocation->y, allocation->width,
                           allocation->height);
  }
}

void moz_container_force_default_visual(MozContainer* container) {
  container->data.force_default_visual = true;
}

nsWindow* moz_container_get_nsWindow(MozContainer* container) {
  gpointer user_data = g_object_get_data(G_OBJECT(container), "nsWindow");
  return static_cast<nsWindow*>(user_data);
}

#undef LOGCONTAINER
