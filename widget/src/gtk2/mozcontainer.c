/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code mozilla.org code.
 *
 * The Initial Developer of the Original Code Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozcontainer.h"
#include <gtk/gtkprivate.h>
#include <stdio.h>

/* init methods */
static void moz_container_class_init          (MozContainerClass *klass);
static void moz_container_init                (MozContainer      *container);

/* widget class methods */
static void moz_container_map                 (GtkWidget         *widget);
static void moz_container_unmap               (GtkWidget         *widget);
static void moz_container_realize             (GtkWidget         *widget);
static void moz_container_size_request        (GtkWidget         *widget,
					       GtkRequisition    *requisition);
static void moz_container_size_allocate       (GtkWidget         *widget,
					       GtkAllocation     *allocation);

/* container class methods */
static void moz_container_add         (GtkContainer      *container,
				       GtkWidget         *widget);
static void moz_container_remove      (GtkContainer      *container,
				       GtkWidget         *widget);
static void moz_container_forall      (GtkContainer      *container,
				       gboolean           include_internals,
				       GtkCallback        callback,
				       gpointer           callback_data);

static GtkContainerClass *parent_class = NULL;

GtkType
moz_container_get_type(void)
{
  static GtkType moz_container_type = 0;

  if (!moz_container_type)
    {
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
						   &moz_container_info, 0);
    }

  return moz_container_type;
}

GtkWidget *
moz_container_new (void)
{
  MozContainer *container;

  container = gtk_type_new (MOZ_CONTAINER_TYPE);

  return GTK_WIDGET(container);
}

void
moz_container_class_init (MozContainerClass *klass)
{
  /*GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass); */
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  widget_class->map = moz_container_map;
  widget_class->unmap = moz_container_unmap;
  widget_class->realize = moz_container_realize;
  widget_class->size_request = moz_container_size_request;
  widget_class->size_allocate = moz_container_size_allocate;

  container_class->add = moz_container_add;
  container_class->remove = moz_container_remove;
  container_class->forall = moz_container_forall;
}

void
moz_container_init (MozContainer *container)
{
  container->container.resize_mode = GTK_RESIZE_IMMEDIATE;
  gtk_widget_set_redraw_on_allocate(GTK_WIDGET(container),
				    FALSE);
}

void
moz_container_map (GtkWidget *widget)
{
  MozContainer *container;
  GList *tmp_list;
  GtkWidget *tmp_child;

  g_return_if_fail (IS_MOZ_CONTAINER(widget));
  container = MOZ_CONTAINER (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  tmp_list = container->children;
  while (tmp_list)
    {
      tmp_child = GTK_WIDGET(tmp_list->data);

      if (GTK_WIDGET_VISIBLE(tmp_child))
	{
	  if (!GTK_WIDGET_MAPPED(tmp_child))
	    gtk_widget_map(tmp_child);
	}
      tmp_list = tmp_list->next;
    }

  gdk_window_show (widget->window);
}

void
moz_container_unmap (GtkWidget *widget)
{
  MozContainer *container;

  g_return_if_fail (IS_MOZ_CONTAINER (widget));
  container = MOZ_CONTAINER (widget);
  
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  gdk_window_hide (widget->window);
}

void
moz_container_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask = 0;
  MozContainer *container;

  g_return_if_fail(IS_MOZ_CONTAINER(widget));

  container = MOZ_CONTAINER(widget);

  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

  /* create the shell window */

  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |=  (GDK_EXPOSURE_MASK | GDK_STRUCTURE_MASK);
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.window_type = GDK_WINDOW_CHILD;

  attributes_mask |= GDK_WA_VISUAL | GDK_WA_COLORMAP |
    GDK_WA_X | GDK_WA_Y;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  printf("widget->window is %p\n", widget->window);
  gdk_window_set_user_data (widget->window, container);

  widget->style = gtk_style_attach (widget->style, widget->window);

  /* set the back pixmap to None so that you don't end up with the gtk
     default which is BlackPixel */
  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);

}

void
moz_container_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  /* always request our current size */
  requisition->width = widget->allocation.width;
  requisition->height = widget->allocation.height;
}

void
moz_container_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  MozContainer *container;
  GList        *tmp_list;
  GtkAllocation  tmp_allocation;
  GtkRequisition tmp_requisition;

  g_return_if_fail (IS_MOZ_CONTAINER (widget));

  printf("moz_container_size_allocate %p %d %d %d %d\n",
	 widget,
	 allocation->x,
	 allocation->y,
	 allocation->width,
	 allocation->height);

  /* short circuit if you can */
  if (widget->allocation.x == allocation->x &&
      widget->allocation.y == allocation->y &&
      widget->allocation.width == allocation->width &&
      widget->allocation.height == allocation->height) {
    return;
  }

  container = MOZ_CONTAINER (widget);

  widget->allocation = *allocation;

  tmp_list = container->children;
  while (tmp_list)
    {
      gtk_widget_size_request (GTK_WIDGET(tmp_list->data), &tmp_requisition);
      tmp_allocation.x = 0;
      tmp_allocation.y = 0;
      tmp_allocation.width = tmp_requisition.width;
      tmp_allocation.height = tmp_requisition.height;
      gtk_widget_size_allocate (GTK_WIDGET(tmp_list->data), &tmp_allocation);
      tmp_list = tmp_list->next;
    }

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize(widget->window,
			     widget->allocation.x,
			     widget->allocation.y,
			     widget->allocation.width,
			     widget->allocation.height);
    }
}

void
moz_container_add (GtkContainer *container, GtkWidget *widget)
{
  MozContainer *moz_container;

  g_return_if_fail(IS_MOZ_CONTAINER(container));

  moz_container = MOZ_CONTAINER(container);

  moz_container->children = g_list_append(moz_container->children, widget);

  gtk_widget_set_parent (widget, GTK_WIDGET(moz_container));
}

void
moz_container_remove (GtkContainer *container, GtkWidget *widget)
{
  GList *tmp_list;

  MozContainer *moz_container;

  g_return_if_fail(IS_MOZ_CONTAINER(container));

  moz_container = MOZ_CONTAINER(container);

  tmp_list = g_list_find(moz_container->children, widget);
  if (tmp_list)
    {
      gtk_widget_unparent (widget);
      moz_container->children = g_list_remove_link (moz_container->children,
						    tmp_list);
      g_list_free_1 (tmp_list);
    }
}

static void
moz_container_forall (GtkContainer *container, gboolean include_internals,
		      GtkCallback  callback, gpointer callback_data)
{
  MozContainer *moz_container;
  GList *tmp_list;
  
  g_return_if_fail (IS_MOZ_CONTAINER(container));
  g_return_if_fail (callback != NULL);

  moz_container = MOZ_CONTAINER(container);

  tmp_list = moz_container->children;
  while (tmp_list)
    {
      (* callback) (GTK_WIDGET(tmp_list->data), callback_data);
      tmp_list = tmp_list->next;
    }
}
