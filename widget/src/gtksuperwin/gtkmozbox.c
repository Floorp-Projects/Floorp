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

#include "gtkmozbox.h"
#include <gtk/gtkmain.h>
#include <X11/Xlib.h>

static void            gtk_mozbox_class_init      (GtkMozBoxClass *klass);
static void            gtk_mozbox_init            (GtkMozBox      *mozbox);
static void            gtk_mozbox_realize         (GtkWidget      *widget);
static void            gtk_mozbox_set_focus       (GtkWindow      *window,
                                                   GtkWidget      *focus);
static gint            gtk_mozbox_key_press_event (GtkWidget   *widget,
                                                   GdkEventKey *event);
static GdkFilterReturn gtk_mozbox_filter          (GdkXEvent      *xevent,
                                                   GdkEvent       *event,
                                                   gpointer        data);
static GtkWindow *     gtk_mozbox_get_parent_gtkwindow (GtkMozBox *mozbox);

void (*parent_class_set_focus)(GtkWindow *, GtkWidget *);

GtkType
gtk_mozbox_get_type (void)
{
  static GtkType mozbox_type = 0;

  if (!mozbox_type)
    {
      static const GtkTypeInfo mozbox_info =
      {
        "GtkMozBox",
        sizeof (GtkMozBox),
        sizeof (GtkMozBoxClass),
        (GtkClassInitFunc) gtk_mozbox_class_init,
        (GtkObjectInitFunc) gtk_mozbox_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

      mozbox_type = gtk_type_unique (GTK_TYPE_WINDOW, &mozbox_info);
    }

  return mozbox_type;
}

static void
gtk_mozbox_class_init (GtkMozBoxClass *klass)
{
  GtkWidgetClass *widget_class;
  GtkWindowClass *window_class;

  widget_class = GTK_WIDGET_CLASS (klass);
  window_class = GTK_WINDOW_CLASS (klass);

  widget_class->realize         = gtk_mozbox_realize;
  widget_class->key_press_event = gtk_mozbox_key_press_event;

  /* save the parent class' set_focus method */
  parent_class_set_focus = window_class->set_focus;
  window_class->set_focus       = gtk_mozbox_set_focus;
  
}

static void
gtk_mozbox_init (GtkMozBox *mozbox)
{
  mozbox->parent_window = NULL;
  mozbox->x = 0;
  mozbox->y = 0;
}

static void
gtk_mozbox_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkMozBox *mozbox;

  g_return_if_fail (GTK_IS_MOZBOX (widget));

  mozbox = GTK_MOZBOX (widget);

  /* GtkWindow checks against premature realization here. Just
   * don't do it.
   */
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = mozbox->x;
  attributes.y = mozbox->y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= GDK_EXPOSURE_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (mozbox->parent_window,
				   &attributes, attributes_mask);

  /* set the back pixmap to None so that you don't end up with the gtk
     default which is BlackPixel */
  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);

  gdk_window_set_user_data (widget->window, mozbox);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  gdk_window_add_filter (widget->window, gtk_mozbox_filter, mozbox);
}

static void
gtk_mozbox_set_focus (GtkWindow      *window,
                      GtkWidget      *focus)
{
  GtkMozBox *mozbox;
  GdkWindow *tmpWindow;
  GtkWindow *parentGtkWindow;

  g_return_if_fail(window != NULL);
  g_return_if_fail(GTK_IS_MOZBOX(window));

  mozbox = GTK_MOZBOX(window);

  parentGtkWindow = gtk_mozbox_get_parent_gtkwindow (mozbox);

  if (parentGtkWindow) {
    parent_class_set_focus(parentGtkWindow, focus);
    return;
  }

  /* as a fall back just hand it off to our parent class */
  parent_class_set_focus(window, focus);

}

static gint
gtk_mozbox_key_press_event (GtkWidget   *widget,
                            GdkEventKey *event)
{
  GtkWindow *window;
  GtkMozBox *mozbox;
  GtkWindow *parentWindow;
  gboolean   handled = FALSE;

  window = GTK_WINDOW (widget);
  mozbox = GTK_MOZBOX (widget);

  parentWindow = gtk_mozbox_get_parent_gtkwindow(mozbox);

  /* give the focus window the chance to handle the event first. */
  if (parentWindow && parentWindow->focus_widget) {
    handled = gtk_widget_event (parentWindow->focus_widget, (GdkEvent*) event);
  }

  /* pass it off to the parent widget */
  if (!handled) {
    gdk_window_unref(event->window);
    event->window = mozbox->parent_window;
    gdk_window_ref(event->window);
    gdk_event_put((GdkEvent *)event);
  }

  return TRUE;
}

static GdkFilterReturn 
gtk_mozbox_filter (GdkXEvent *xevent,
		   GdkEvent *event,
		   gpointer  data)
{
  XEvent *xev = xevent;
  GtkWidget *widget = data;

  switch (xev->xany.type)
    {
    case ConfigureNotify:
      event->configure.type = GDK_CONFIGURE;
      event->configure.window = widget->window;
      event->configure.x = 0;
      event->configure.y = 0;
      event->configure.width = xev->xconfigure.width;
      event->configure.height = xev->xconfigure.height;
      return GDK_FILTER_TRANSLATE;
    default:
      return GDK_FILTER_CONTINUE;
    }
}

static GtkWindow *
gtk_mozbox_get_parent_gtkwindow (GtkMozBox *mozbox)
{
  GdkWindow *tmpWindow;
  /* look for a parent GtkWindow in the heirarchy */
  tmpWindow = mozbox->parent_window;
  while (tmpWindow) {
    gpointer data = NULL;
    gdk_window_get_user_data(tmpWindow, &data);
    if (data && GTK_IS_WINDOW(data)) {
      return GTK_WINDOW(data);
    }
    tmpWindow = gdk_window_get_parent(tmpWindow);
  }
  return NULL;
}

GtkWidget*
gtk_mozbox_new (GdkWindow *parent_window)
{
  GtkMozBox *mozbox;

  mozbox = gtk_type_new (GTK_TYPE_MOZBOX);
  mozbox->parent_window = parent_window;

  return GTK_WIDGET (mozbox);
}

void
gtk_mozbox_set_position (GtkMozBox *mozbox,
			 gint       x,
			 gint       y)
{
  mozbox->x = x;
  mozbox->y = y;

  if (GTK_WIDGET_REALIZED (mozbox))
    gdk_window_move (GTK_WIDGET (mozbox)->window, x, y);
}

