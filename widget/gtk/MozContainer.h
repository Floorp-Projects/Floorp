/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
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
 * This class serves two purposes in the nsIWidget implementation.
 *
 *   - It provides objects to receive signals from GTK for events on native
 *     windows.
 *
 *   - It provides GdkWindow to draw content.
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
#ifdef MOZ_WAYLAND
#  define MOZ_WL_CONTAINER(obj) (&MOZ_CONTAINER(obj)->data.wl_container)
#endif

typedef struct _MozContainer MozContainer;
typedef struct _MozContainerClass MozContainerClass;

struct _MozContainer {
  GtkContainer container;
  gboolean destroyed;
  struct Data {
    gboolean force_default_visual = false;
#ifdef MOZ_WAYLAND
    MozContainerWayland wl_container;
#endif
  } data;
};

struct _MozContainerClass {
  GtkContainerClass parent_class;
};

GType moz_container_get_type(void);
GtkWidget* moz_container_new(void);
void moz_container_unmap(GtkWidget* widget);
void moz_container_put(MozContainer* container, GtkWidget* child_widget, gint x,
                       gint y);
void moz_container_force_default_visual(MozContainer* container);
void moz_container_class_init(MozContainerClass* klass);

class nsWindow;
nsWindow* moz_container_get_nsWindow(MozContainer* container);

#endif /* __MOZ_CONTAINER_H__ */
