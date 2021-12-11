/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MOZ_CONTAINER_H__
#define __MOZ_CONTAINER_H__

#ifdef MOZ_WAYLAND
#  include "MozContainerWayland.h"
#endif

#include <gtk/gtk.h>
#include <functional>

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
 *
 *   - It provides a container parent for GtkWidgets.  The only GtkWidgets
 *     that need this in Mozilla are the GtkSockets for windowed plugins (Xt
 *     and XEmbed).
 *
 * Note that the window hierarchy in Mozilla differs from conventional
 * GtkWidget hierarchies.
 *
 * Mozilla's hierarchy exists through the GdkWindow hierarchy, and all child
 * GdkWindows (within a child nsIWidget hierarchy) belong to one MozContainer
 * GtkWidget.  If the MozContainer is unrealized or its GdkWindows are
 * destroyed for some other reason, then the hierarchy no longer exists.  (In
 * conventional GTK clients, the hierarchy is recorded by the GtkWidgets, and
 * so can be re-established after destruction of the GdkWindows.)
 *
 * One consequence of this is that the MozContainer does not know which of its
 * GdkWindows should parent child GtkWidgets.  (Conventional GtkContainers
 * determine which GdkWindow to assign child GtkWidgets.)
 *
 * Therefore, when adding a child GtkWidget to a MozContainer,
 * gtk_widget_set_parent_window should be called on the child GtkWidget before
 * it is realized.
 */

#define MOZ_CONTAINER_TYPE (moz_container_get_type())
#define MOZ_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MOZ_CONTAINER_TYPE, MozContainer))
#define MOZ_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), MOZ_CONTAINER_TYPE, MozContainerClass))
#define IS_MOZ_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOZ_CONTAINER_TYPE))
#define IS_MOZ_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), MOZ_CONTAINER_TYPE))
#define MOZ_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), MOZ_CONTAINER_TYPE, MozContainerClass))

typedef struct _MozContainer MozContainer;
typedef struct _MozContainerClass MozContainerClass;

struct _MozContainer {
  GtkContainer container;
  GList* children;
  gboolean force_default_visual;
#ifdef MOZ_WAYLAND
  MozContainerWayland wl_container;
#endif
};

struct _MozContainerClass {
  GtkContainerClass parent_class;
};

GType moz_container_get_type(void);
GtkWidget* moz_container_new(void);
void moz_container_put(MozContainer* container, GtkWidget* child_widget, gint x,
                       gint y);
void moz_container_force_default_visual(MozContainer* container);

class nsWindow;
nsWindow* moz_container_get_nsWindow(MozContainer* container);

#endif /* __MOZ_CONTAINER_H__ */
