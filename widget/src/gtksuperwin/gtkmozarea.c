/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Initial Developers of this code under the MPL are Owen Taylor
 * <otaylor@redhat.com> and Christopher Blizzard <blizzard@redhat.com>.
 * Portions created by the Initial Developers are Copyright (C) 1999
 * Owen Taylor and Christopher Blizzard.  All Rights Reserved.  */

#include "gtkmozarea.h"
#include <X11/Xlib.h>

static void gtk_mozarea_class_init (GtkMozAreaClass *klass);
static void gtk_mozarea_init       (GtkMozArea      *mozarea);
static void gtk_mozarea_realize    (GtkWidget       *widget);
static void gtk_mozarea_unrealize  (GtkWidget       *widget);
static void gtk_mozarea_size_allocate (GtkWidget    *widget, GtkAllocation *allocation);        

GtkWidgetClass *parent_class = NULL;

GtkType
gtk_mozarea_get_type (void)
{
  static GtkType mozarea_type = 0;
  
  if (!mozarea_type)
    {
      static const GtkTypeInfo mozarea_info =
      {
        "GtkMozArea",
        sizeof (GtkMozArea),
        sizeof (GtkMozAreaClass),
        (GtkClassInitFunc) gtk_mozarea_class_init,
        (GtkObjectInitFunc) gtk_mozarea_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };
      
      mozarea_type = gtk_type_unique (GTK_TYPE_WIDGET, &mozarea_info);
    }

  return mozarea_type;
}

static void
gtk_mozarea_class_init (GtkMozAreaClass *klass)
{
  GtkWidgetClass *widget_class;

  widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->realize = gtk_mozarea_realize;
  widget_class->unrealize = gtk_mozarea_unrealize;
  widget_class->size_allocate = gtk_mozarea_size_allocate;

  parent_class = gtk_type_class(gtk_widget_get_type());

}

static void
gtk_mozarea_init (GtkMozArea *mozarea)
{
  mozarea->superwin = NULL;
}

static void 
gtk_mozarea_realize (GtkWidget *widget)
{
  GtkMozArea *mozarea;
  
  g_return_if_fail (GTK_IS_MOZAREA (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  mozarea = GTK_MOZAREA (widget);

  mozarea->superwin = gdk_superwin_new (gtk_widget_get_parent_window (widget),
                                        widget->allocation.x, widget->allocation.y,
                                        widget->allocation.width, widget->allocation.height);
  gdk_window_set_user_data (mozarea->superwin->shell_window, mozarea);
  widget->window = mozarea->superwin->shell_window;
  widget->style = gtk_style_attach (widget->style, widget->window);
  /* make sure that we add a reference to this window.
     if we don't then it will be destroyed by both the superwin
     destroy method and the widget class destructor */
  gdk_window_ref(widget->window);
}

static void
gtk_mozarea_unrealize(GtkWidget *widget)
{
  GtkMozArea *mozarea;
  
  g_return_if_fail(GTK_IS_MOZAREA(widget));

  GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);

  mozarea = GTK_MOZAREA(widget);
  
  if (mozarea->superwin) {
    gdk_superwin_destroy(mozarea->superwin);
    mozarea->superwin = NULL;
  }
  GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}

static void 
gtk_mozarea_size_allocate (GtkWidget    *widget,
			   GtkAllocation *allocation)
{
  GtkMozArea *mozarea;
  
  g_return_if_fail (GTK_IS_MOZAREA (widget));

  mozarea = GTK_MOZAREA (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move (mozarea->superwin->shell_window,
		       allocation->x, allocation->y);
      gdk_superwin_resize (mozarea->superwin,
			   allocation->width, allocation->height);
    }
}

GtkWidget*
gtk_mozarea_new (GdkWindow *parent_window)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_MOZAREA));
}

