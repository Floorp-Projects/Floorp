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
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

static void gtk_mozarea_class_init (GtkMozAreaClass *klass);
static void gtk_mozarea_init       (GtkMozArea      *mozarea);
static void gtk_mozarea_realize    (GtkWidget       *widget);
static void gtk_mozarea_unrealize  (GtkWidget       *widget);
static void gtk_mozarea_size_allocate (GtkWidget    *widget,
                                       GtkAllocation *allocation);

static void
attach_toplevel_listener(GtkMozArea *mozarea);

static Window
get_real_toplevel(Window aWindow);

static GdkFilterReturn
toplevel_window_filter(GdkXEvent   *aGdkXEvent,
                       GdkEvent    *aEvent,
                       gpointer     data);

GtkWidgetClass *parent_class = NULL;

enum {
  TOPLEVEL_FOCUS_IN,
  TOPLEVEL_FOCUS_OUT,
  LAST_SIGNAL
};

static guint mozarea_signals[LAST_SIGNAL] = { 0 };

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
  GtkObjectClass *object_class;

  widget_class = GTK_WIDGET_CLASS (klass);
  object_class = GTK_OBJECT_CLASS (klass);

  widget_class->realize = gtk_mozarea_realize;
  widget_class->unrealize = gtk_mozarea_unrealize;
  widget_class->size_allocate = gtk_mozarea_size_allocate;

  parent_class = gtk_type_class(gtk_widget_get_type());

  /* set up our signals */

  mozarea_signals[TOPLEVEL_FOCUS_IN] =
    gtk_signal_new("toplevel_focus_in",
                   GTK_RUN_FIRST,
                   object_class->type,
                   GTK_SIGNAL_OFFSET(GtkMozAreaClass, toplevel_focus_in),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);
  mozarea_signals[TOPLEVEL_FOCUS_OUT] =
    gtk_signal_new("toplevel_focus_out",
                   GTK_RUN_FIRST,
                   object_class->type,
                   GTK_SIGNAL_OFFSET(GtkMozAreaClass, toplevel_focus_out),
                   gtk_marshal_NONE__NONE,
                   GTK_TYPE_NONE, 0);
  gtk_object_class_add_signals(object_class, mozarea_signals, LAST_SIGNAL);

}

static void
gtk_mozarea_init (GtkMozArea *mozarea)
{
  mozarea->superwin = NULL;
  mozarea->toplevel_focus = FALSE;
}

static void 
gtk_mozarea_realize (GtkWidget *widget)
{
  GtkMozArea *mozarea;
  
  g_return_if_fail (GTK_IS_MOZAREA (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  mozarea = GTK_MOZAREA (widget);

  mozarea->superwin = gdk_superwin_new (gtk_widget_get_parent_window (widget),
                                        widget->allocation.x,
                                        widget->allocation.y,
                                        widget->allocation.width,
                                        widget->allocation.height);
  gdk_window_set_user_data (mozarea->superwin->shell_window, mozarea);
  widget->window = mozarea->superwin->shell_window;
  widget->style = gtk_style_attach (widget->style, widget->window);
  /* make sure that we add a reference to this window.
     if we don't then it will be destroyed by both the superwin
     destroy method and the widget class destructor */
  gdk_window_ref(widget->window);

  /* attach the toplevel X listener */
  attach_toplevel_listener(mozarea);
}

static void
gtk_mozarea_unrealize(GtkWidget *widget)
{
  GtkMozArea *mozarea;
  
  g_return_if_fail(GTK_IS_MOZAREA(widget));

  GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);

  mozarea = GTK_MOZAREA(widget);
  
  if (mozarea->superwin) {
    gtk_object_unref(GTK_OBJECT(mozarea->superwin));
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

gboolean
gtk_mozarea_get_toplevel_focus(GtkMozArea *area)
{
  g_return_val_if_fail(GTK_IS_MOZAREA(area), FALSE);
  
  return area->toplevel_focus;
}
                               
/* this function will attach a listener to this widget's real toplevel
   window */
static void
attach_toplevel_listener(GtkMozArea *mozarea)
{
  /* get the native window for this widget */
  GtkWidget *widget = GTK_WIDGET(mozarea);
  Window window = GDK_WINDOW_XWINDOW(widget->window);

  Window toplevel = get_real_toplevel(window);

  /* check to see if this is an already registered window with the
     type system. */
  GdkWindow *gdk_window = gdk_window_lookup(toplevel);

  /* This isn't our window?  It is now, fool! */
  if (!gdk_window) {
    /* import it into the type system */
    gdk_window = gdk_window_foreign_new(toplevel);
    /* make sure that we are listening for the right events on it. */
    gdk_window_set_events(gdk_window, GDK_FOCUS_CHANGE_MASK);
  }

  /* attach our passive filter.  when the window is destroyed it will
     automatically be removed. */

  gdk_window_add_filter(gdk_window, toplevel_window_filter, mozarea);
}

/* this function will try to find the real toplevel for a gdk window. */
static Window
get_real_toplevel(Window aWindow)
{
  Window current = aWindow;
  Atom   atom;

  /* get the atom for the WM_STATE variable that you get on WM
     managed windows. */

  atom = XInternAtom(GDK_DISPLAY(), "WM_STATE", FALSE);

  while (current) {
    Atom type = None;
    int format;
    unsigned long nitems, after;
    unsigned char *data;

    Window root_return;
    Window parent_return;
    Window *children_return = NULL;
    unsigned int nchildren_return;

    /* check for the atom on this window */
    XGetWindowProperty(GDK_DISPLAY(), current, atom,
		       0, 0, /* offsets */
		       False, /* don't delete */
		       AnyPropertyType,
		       &type, &format, &nitems, &after, &data);

    /* did we get something? */
    if (type != None) {
      XFree(data);
      data = NULL;
      /* ok, this is the toplevel window since it has the property set
         on it. */
      break;
    }
    
    /* what is the parent? */
    XQueryTree(GDK_DISPLAY(), current, &root_return,
	       &parent_return, &children_return,
	       &nchildren_return);

    if (children_return)
      XFree(children_return);

    /* If the parent window of this window is the root window then
     there is no window manager running so the current window is the
     toplevel window. */
    if (parent_return == root_return)
      break;

    current = parent_return;
  }

  return current;
}

static GdkFilterReturn
toplevel_window_filter(GdkXEvent   *aGdkXEvent,
                       GdkEvent    *aEvent,
                       gpointer     data)
{
  XEvent       *xevent = (XEvent *)aGdkXEvent;
  GtkMozArea   *mozarea = (GtkMozArea *)data;

  switch (xevent->xany.type) {
  case FocusIn:
    mozarea->toplevel_focus = TRUE;
    gtk_signal_emit(GTK_OBJECT(mozarea), mozarea_signals[TOPLEVEL_FOCUS_IN]);
    break;
  case FocusOut:
    mozarea->toplevel_focus = FALSE;
    gtk_signal_emit(GTK_OBJECT(mozarea), mozarea_signals[TOPLEVEL_FOCUS_OUT]);
    break;
  default:
    break;
  }

  return GDK_FILTER_CONTINUE;
}
