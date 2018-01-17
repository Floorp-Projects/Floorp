/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozcontainer.h"
#include <gtk/gtk.h>
#ifdef MOZ_WAYLAND
#include <gdk/gdkx.h>
#include <gdk/gdkwayland.h>
#endif
#include <stdio.h>
#include <dlfcn.h>

#ifdef ACCESSIBILITY
#include <atk/atk.h>
#include "maiRedundantObjectFactory.h"
#endif

/* init methods */
static void moz_container_class_init          (MozContainerClass *klass);
static void moz_container_init                (MozContainer      *container);

/* widget class methods */
static void moz_container_map                 (GtkWidget         *widget);
static void moz_container_unmap               (GtkWidget         *widget);
static void moz_container_realize             (GtkWidget         *widget);
static void moz_container_size_allocate       (GtkWidget         *widget,
                                               GtkAllocation     *allocation);

/* container class methods */
static void moz_container_remove      (GtkContainer      *container,
                                       GtkWidget         *child_widget);
static void moz_container_forall      (GtkContainer      *container,
                                       gboolean           include_internals,
                                       GtkCallback        callback,
                                       gpointer           callback_data);
static void moz_container_add         (GtkContainer      *container,
                                        GtkWidget        *widget);

typedef struct _MozContainerChild MozContainerChild;

struct _MozContainerChild {
    GtkWidget *widget;
    gint x;
    gint y;
};

static void moz_container_allocate_child (MozContainer      *container,
                                          MozContainerChild *child);
static MozContainerChild *
moz_container_get_child (MozContainer *container, GtkWidget *child);

/* public methods */

GType
moz_container_get_type(void)
{
    static GType moz_container_type = 0;

    if (!moz_container_type) {
        static GTypeInfo moz_container_info = {
            sizeof(MozContainerClass), /* class_size */
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc) moz_container_class_init, /* class_init */
            NULL, /* class_destroy */
            NULL, /* class_data */
            sizeof(MozContainer), /* instance_size */
            0, /* n_preallocs */
            (GInstanceInitFunc) moz_container_init, /* instance_init */
            NULL, /* value_table */
        };

        moz_container_type = g_type_register_static (GTK_TYPE_CONTAINER,
                                                     "MozContainer",
                                                     &moz_container_info,
                                                     static_cast<GTypeFlags>(0));
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

GtkWidget *
moz_container_new (void)
{
    MozContainer *container;

    container = static_cast<MozContainer*>(g_object_new (MOZ_CONTAINER_TYPE, nullptr));

    return GTK_WIDGET(container);
}

void
moz_container_put (MozContainer *container, GtkWidget *child_widget,
                   gint x, gint y)
{
    MozContainerChild *child;

    child = g_new (MozContainerChild, 1);

    child->widget = child_widget;
    child->x = x;
    child->y = y;

    /*  printf("moz_container_put %p %p %d %d\n", (void *)container,
        (void *)child_widget, x, y); */

    container->children = g_list_append (container->children, child);

    /* we assume that the caller of this function will have already set
       the parent GdkWindow because we can have many anonymous children. */
    gtk_widget_set_parent(child_widget, GTK_WIDGET(container));
}

void
moz_container_move (MozContainer *container, GtkWidget *child_widget,
                    gint x, gint y, gint width, gint height)
{
    MozContainerChild *child;
    GtkAllocation new_allocation;

    child = moz_container_get_child (container, child_widget);

    child->x = x;
    child->y = y;

    new_allocation.x = x;
    new_allocation.y = y;
    new_allocation.width = width;
    new_allocation.height = height;

    /* printf("moz_container_move %p %p will allocate to %d %d %d %d\n",
       (void *)container, (void *)child_widget,
       new_allocation.x, new_allocation.y,
       new_allocation.width, new_allocation.height); */

    gtk_widget_size_allocate(child_widget, &new_allocation);
}

/* static methods */

void
moz_container_class_init (MozContainerClass *klass)
{
    /*GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
      GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass); */
    GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    widget_class->map = moz_container_map;
    widget_class->unmap = moz_container_unmap;
    widget_class->realize = moz_container_realize;
    widget_class->size_allocate = moz_container_size_allocate;

    container_class->remove = moz_container_remove;
    container_class->forall = moz_container_forall;
    container_class->add = moz_container_add;
}

#if defined(MOZ_WAYLAND)
static void
registry_handle_global (void *data,
                        struct wl_registry *registry,
                        uint32_t name,
                        const char *interface,
                        uint32_t version)
{
    MozContainer *container = MOZ_CONTAINER(data);
    if(strcmp(interface, "wl_subcompositor") == 0) {
        container->subcompositor =
            static_cast<wl_subcompositor*>(wl_registry_bind(registry,
                                           name,
                                           &wl_subcompositor_interface,
                                           1));
    }
}

static void
registry_handle_global_remove (void *data,
                               struct wl_registry *registry,
                               uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};
#endif

void
moz_container_init (MozContainer *container)
{
    gtk_widget_set_can_focus(GTK_WIDGET(container), TRUE);
    gtk_container_set_resize_mode(GTK_CONTAINER(container), GTK_RESIZE_IMMEDIATE);
    gtk_widget_set_redraw_on_allocate(GTK_WIDGET(container), FALSE);

#if defined(MOZ_WAYLAND)
    {
      GdkDisplay *gdk_display = gtk_widget_get_display(GTK_WIDGET(container));
      if (GDK_IS_WAYLAND_DISPLAY (gdk_display)) {
          // Available as of GTK 3.8+
          static auto sGdkWaylandDisplayGetWlDisplay =
              (wl_display *(*)(GdkDisplay *))
              dlsym(RTLD_DEFAULT, "gdk_wayland_display_get_wl_display");

          wl_display* display = sGdkWaylandDisplayGetWlDisplay(gdk_display);
          wl_registry* registry = wl_display_get_registry(display);
          wl_registry_add_listener(registry, &registry_listener, container);
          wl_display_dispatch(display);
          wl_display_roundtrip(display);
        }
    }
#endif
}

#if defined(MOZ_WAYLAND)
/* We want to draw to GdkWindow owned by mContainer from Compositor thread but
 * Gtk+ can be used in main thread only. So we create wayland wl_surface
 * and attach it as an overlay to GdkWindow.
 *
 * see gtk_clutter_embed_ensure_subsurface() at gtk-clutter-embed.c
*  for reference.
 */
static gboolean
moz_container_map_surface(MozContainer *container)
{
    // Available as of GTK 3.8+
    static auto sGdkWaylandDisplayGetWlCompositor =
        (wl_compositor *(*)(GdkDisplay *))
        dlsym(RTLD_DEFAULT, "gdk_wayland_display_get_wl_compositor");
    static auto sGdkWaylandWindowGetWlSurface =
        (wl_surface *(*)(GdkWindow *))
        dlsym(RTLD_DEFAULT, "gdk_wayland_window_get_wl_surface");

    GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(container));
    if (GDK_IS_X11_DISPLAY(display))
        return false;

    if (container->subsurface && container->surface)
        return true;

    if (!container->surface) {
        struct wl_compositor *compositor;
        compositor = sGdkWaylandDisplayGetWlCompositor(display);
        container->surface = wl_compositor_create_surface(compositor);
    }

    if (!container->subsurface) {
        GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
        wl_surface* gtk_surface = sGdkWaylandWindowGetWlSurface(window);
        if (!gtk_surface) {
          // We requested the underlying wl_surface too early when container
          // is not realized yet. We'll try again before first rendering
          // to mContainer.
          return false;
        }

        container->subsurface =
          wl_subcompositor_get_subsurface (container->subcompositor,
                                           container->surface,
                                           gtk_surface);
        gint x, y;
        gdk_window_get_position(window, &x, &y);
        wl_subsurface_set_position(container->subsurface, x, y);
        wl_subsurface_set_desync(container->subsurface);

        // Route input to parent wl_surface owned by Gtk+ so we get input
        // events from Gtk+.
        GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET (container));
        wl_compositor* compositor = sGdkWaylandDisplayGetWlCompositor(display);
        wl_region* region = wl_compositor_create_region(compositor);
        wl_surface_set_input_region(container->surface, region);
        wl_region_destroy(region);
    }
    return true;
}

static void
moz_container_unmap_surface(MozContainer *container)
{
    g_clear_pointer(&container->subsurface, wl_subsurface_destroy);
    g_clear_pointer(&container->surface, wl_surface_destroy);
}

#endif

void
moz_container_map (GtkWidget *widget)
{
    MozContainer *container;
    GList *tmp_list;
    GtkWidget *tmp_child;

    g_return_if_fail (IS_MOZ_CONTAINER(widget));
    container = MOZ_CONTAINER (widget);

    gtk_widget_set_mapped(widget, TRUE);

    tmp_list = container->children;
    while (tmp_list) {
        tmp_child = ((MozContainerChild *)tmp_list->data)->widget;

        if (gtk_widget_get_visible(tmp_child)) {
            if (!gtk_widget_get_mapped(tmp_child))
                gtk_widget_map(tmp_child);
        }
        tmp_list = tmp_list->next;
    }

    if (gtk_widget_get_has_window (widget)) {
        gdk_window_show (gtk_widget_get_window(widget));
#if defined(MOZ_WAYLAND)
        moz_container_map_surface(MOZ_CONTAINER(widget));
#endif
    }
}

void
moz_container_unmap (GtkWidget *widget)
{
    g_return_if_fail (IS_MOZ_CONTAINER (widget));

    gtk_widget_set_mapped(widget, FALSE);

    if (gtk_widget_get_has_window (widget)) {
        gdk_window_hide (gtk_widget_get_window(widget));
#if defined(MOZ_WAYLAND)
        moz_container_unmap_surface(MOZ_CONTAINER(widget));
#endif
    }
}

void
moz_container_realize (GtkWidget *widget)
{
    GdkWindow *parent = gtk_widget_get_parent_window (widget);
    GdkWindow *window;

    gtk_widget_set_realized(widget, TRUE);

    if (gtk_widget_get_has_window (widget)) {
        GdkWindowAttr attributes;
        gint attributes_mask = GDK_WA_VISUAL | GDK_WA_X | GDK_WA_Y;
        GtkAllocation allocation;

        gtk_widget_get_allocation (widget, &allocation);
        attributes.event_mask = gtk_widget_get_events (widget);
        attributes.x = allocation.x;
        attributes.y = allocation.y;
        attributes.width = allocation.width;
        attributes.height = allocation.height;
        attributes.wclass = GDK_INPUT_OUTPUT;
        attributes.visual = gtk_widget_get_visual (widget);
        attributes.window_type = GDK_WINDOW_CHILD;

        window = gdk_window_new (parent, &attributes, attributes_mask);
        gdk_window_set_user_data (window, widget);
    } else {
        window = parent;
        g_object_ref (window);
    }

    gtk_widget_set_window (widget, window);

}

void
moz_container_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
    MozContainer   *container;
    GList          *tmp_list;
    GtkAllocation   tmp_allocation;

    g_return_if_fail (IS_MOZ_CONTAINER (widget));

    /*  printf("moz_container_size_allocate %p %d %d %d %d\n",
        (void *)widget,
        allocation->x,
        allocation->y,
        allocation->width,
        allocation->height); */

    /* short circuit if you can */
    container = MOZ_CONTAINER (widget);
    gtk_widget_get_allocation(widget, &tmp_allocation);
    if (!container->children &&
        tmp_allocation.x == allocation->x &&
        tmp_allocation.y == allocation->y &&
        tmp_allocation.width == allocation->width &&
        tmp_allocation.height == allocation->height) {
        return;
    }

    gtk_widget_set_allocation(widget, allocation);

    tmp_list = container->children;

    while (tmp_list) {
        MozContainerChild *child = static_cast<MozContainerChild*>(tmp_list->data);

        moz_container_allocate_child (container, child);

        tmp_list = tmp_list->next;
    }

    if (gtk_widget_get_has_window (widget) &&
        gtk_widget_get_realized (widget)) {

        gdk_window_move_resize(gtk_widget_get_window(widget),
                               allocation->x,
                               allocation->y,
                               allocation->width,
                               allocation->height);
    }

#if defined(MOZ_WAYLAND)
    // We need to position our subsurface according to GdkWindow
    // when offset changes (GdkWindow is maximized for instance).
    // see gtk-clutter-embed.c for reference.
    if (container->subsurface) {
        gint x, y;
        gdk_window_get_position(gtk_widget_get_window(widget), &x, &y);
        wl_subsurface_set_position(container->subsurface, x, y);
    }
#endif
}

void
moz_container_remove (GtkContainer *container, GtkWidget *child_widget)
{
    MozContainerChild *child;
    MozContainer *moz_container;
    GdkWindow* parent_window;

    g_return_if_fail (IS_MOZ_CONTAINER(container));
    g_return_if_fail (GTK_IS_WIDGET(child_widget));

    moz_container = MOZ_CONTAINER(container);

    child = moz_container_get_child (moz_container, child_widget);
    g_return_if_fail (child);

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
    if (parent_window)
        g_object_ref(parent_window);

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

void
moz_container_forall (GtkContainer *container, gboolean include_internals,
                      GtkCallback  callback, gpointer callback_data)
{
    MozContainer *moz_container;
    GList *tmp_list;

    g_return_if_fail (IS_MOZ_CONTAINER(container));
    g_return_if_fail (callback != NULL);

    moz_container = MOZ_CONTAINER(container);

    tmp_list = moz_container->children;
    while (tmp_list) {
        MozContainerChild *child;
        child = static_cast<MozContainerChild*>(tmp_list->data);
        tmp_list = tmp_list->next;
        (* callback) (child->widget, callback_data);
    }
}

static void
moz_container_allocate_child (MozContainer *container,
                              MozContainerChild *child)
{
    GtkAllocation  allocation;

    gtk_widget_get_allocation (child->widget, &allocation);
    allocation.x = child->x;
    allocation.y = child->y;

    gtk_widget_size_allocate (child->widget, &allocation);
}

MozContainerChild *
moz_container_get_child (MozContainer *container, GtkWidget *child_widget)
{
    GList *tmp_list;

    tmp_list = container->children;
    while (tmp_list) {
        MozContainerChild *child;

        child = static_cast<MozContainerChild*>(tmp_list->data);
        tmp_list = tmp_list->next;

        if (child->widget == child_widget)
            return child;
    }

    return NULL;
}

static void
moz_container_add(GtkContainer *container, GtkWidget *widget)
{
    moz_container_put(MOZ_CONTAINER(container), widget, 0, 0);
}

#ifdef MOZ_WAYLAND
struct wl_surface*
moz_container_get_wl_surface(MozContainer *container)
{
    if (!container->subsurface || !container->surface) {
        GdkWindow* window = gtk_widget_get_window(GTK_WIDGET(container));
        if (!gdk_window_is_visible(window))
            return nullptr;

        moz_container_map_surface(container);
    }

    return container->surface;
}
#endif
