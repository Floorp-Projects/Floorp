/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * GtkXtBin Widget Implementation
 * Rusty Lynch - 02/27/2000
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */    

/*
 * The GtkXtBin widget allows for Xt toolkit code to be used
 * inside a GTK application.  
 */

#include "gtkxtbin.h"
#include <gdk/gdkx.h>
#include <glib.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Xlib/Xt stuff */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Shell.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

static void            gtk_xtbin_class_init (GtkXtBinClass *klass);
static void            gtk_xtbin_init       (GtkXtBin      *xtbin);
static void            gtk_xtbin_realize    (GtkWidget      *widget);

static String          *fallback = NULL;
static int              xt_is_initialized = 0;

static gboolean
xt_event_prepare (gpointer  source_data,
                   GTimeVal *current_time,
                   gint     *timeout,
                   gpointer  user_data)
{   
  XtAppContext ac;
  XtInputMask mask;  

  ac = XtDisplayToApplicationContext((Display *)user_data);

  GDK_THREADS_ENTER();
  mask = XtAppPending(ac);
  GDK_THREADS_LEAVE();

  if (mask)
	return TRUE;
  else
	return FALSE;
}

static gboolean
xt_event_check (gpointer  source_data,
                 GTimeVal *current_time,
                 gpointer  user_data)
{
  Display * display;
  XtAppContext ac;
  XtInputMask mask;

  GDK_THREADS_ENTER ();

  display = (Display *)user_data;
  ac = XtDisplayToApplicationContext(display);

  mask = XtAppPending(ac);

  GDK_THREADS_LEAVE ();

  if (mask)
    return TRUE;
  else
    return FALSE;
}   

static gboolean
xt_event_dispatch (gpointer  source_data,
                    GTimeVal *current_time,
                    gpointer  user_data)
{
  XEvent event;
  Display * display;
  XtAppContext ac;
  XtInputMask mask;

  display = (Display *)user_data;
  ac = XtDisplayToApplicationContext(display);

  GDK_THREADS_ENTER ();
  while((mask = XtAppPending(ac)) > 0)
    XtAppProcessEvent(ac, mask);
  GDK_THREADS_LEAVE ();

  return TRUE;  
}

static GSourceFuncs xt_event_funcs = {
  xt_event_prepare,
  xt_event_check,
  xt_event_dispatch,
  (GDestroyNotify)g_free
};  

GtkType
gtk_xtbin_get_type (void)
{
  static GtkType xtbin_type = 0;

  if (!xtbin_type)
    {
      static const GtkTypeInfo xtbin_info =
      {
        "GtkXtBin",
        sizeof (GtkXtBin),
        sizeof (GtkXtBinClass),
        (GtkClassInitFunc) gtk_xtbin_class_init,
        (GtkObjectInitFunc) gtk_xtbin_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

      xtbin_type = gtk_type_unique (GTK_TYPE_WINDOW, &xtbin_info);
    }

  return xtbin_type;
}

static void
gtk_xtbin_class_init (GtkXtBinClass *klass)
{
  GtkWidgetClass *widget_class;


  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gtk_xtbin_realize;
}

static void
gtk_xtbin_init (GtkXtBin *xtbin)
{

  xtbin->xtwidget = NULL;
  xtbin->parent_window = NULL;
  xtbin->x = 0;
  xtbin->y = 0;

  xtbin->xtdisplay = NULL;
  xtbin->xtcontext = 0;
  xtbin->xtwindow = 0;

}

static void
gtk_xtbin_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint          attributes_mask, n;
  GtkXtBin     *xtbin;
  Arg           args[20];
  char         *mArgv[1];
  int           mArgc = 0;
  gint          width, height;
  static Display      *xtdisplay;
  XtAppContext  app_context;
  Widget        top_widget;
  Window        win;
  Widget        embeded;


  g_return_if_fail (GTK_IS_XTBIN (widget));

  gdk_flush();
  xtbin = GTK_XTBIN (widget);

  /* GtkWindow checks against premature realization here. Just
   * don't do it.
   */
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type  = GDK_WINDOW_CHILD;
  attributes.x            = xtbin->x;
  attributes.y            = xtbin->y;
  attributes.width        = widget->allocation.width;
  attributes.height       = widget->allocation.height;
  attributes.wclass       = GDK_INPUT_OUTPUT;
  attributes.visual       = gtk_widget_get_visual (widget);
  attributes.colormap     = gtk_widget_get_colormap (widget);
  attributes.event_mask   = gtk_widget_get_events (widget);
  attributes.event_mask  |= GDK_EXPOSURE_MASK;
  attributes_mask         = GDK_WA_X      | GDK_WA_Y | 
                            GDK_WA_VISUAL | GDK_WA_COLORMAP;

  xtbin->width = attributes.width;
  xtbin->height = attributes.height;

  widget->window = gdk_window_new (xtbin->parent_window,
                                   &attributes, attributes_mask);

  /* Turn off any event catching for this window by */
  /* the Gtk/Gdk event loop... otherwise some strange */
  /* things happen */
  XSelectInput(GDK_WINDOW_XDISPLAY(widget->window), 
               GDK_WINDOW_XWINDOW(widget->window),
               0);

  gdk_window_set_user_data (widget->window, xtbin);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  xtbin->current_window = widget->window;

  /* ensure all the outgoing events are flushed */
  /* before we try this crazy dual toolkit stuff */
  gdk_flush();

  /* Initialize the Xt toolkit */
  if (!xt_is_initialized) {
    int            cnumber;
    static GPollFD xt_event_poll_fd;

    /****
     * Standard Xt initialization stuff
     */

    XtToolkitInitialize();
    app_context = XtCreateApplicationContext();
    if (fallback)
      XtAppSetFallbackResources(app_context, fallback);
  
    xtdisplay = XtOpenDisplay(app_context, NULL, NULL, 
                              "Wrapper", NULL, 0, &mArgc, mArgv);
    xt_is_initialized++;

    /****
     * hook Xt event loop into the glib event loop.
     */

    /* the assumption is that gtk_init has already been called */
    g_source_add (GDK_PRIORITY_EVENTS, TRUE, 
                  &xt_event_funcs, NULL, xtdisplay, NULL);	
    
    cnumber = ConnectionNumber(xtdisplay);
    xt_event_poll_fd.fd = cnumber;
    xt_event_poll_fd.events = G_IO_IN | G_IO_OUT; 
    xt_event_poll_fd.revents = 0;    /* hmm... is this correct? */

    g_main_add_poll (&xt_event_poll_fd, G_PRIORITY_DEFAULT);
  }

  /***
   * I'm sure there is a better way, but for now I'm 
   * just creating a new application shell (since it doesn't
   * need a parent widget,) and then swapping out the XWindow
   * from under it.  This seems to mostly work, but it's
   * an ugly hack.
   */

  top_widget = XtAppCreateShell("drawingArea", "Wrapper", 
                                applicationShellWidgetClass, xtdisplay, 
                                NULL, 0);


  /* set size of Xt window */
  n = 0;
  XtSetArg(args[n], XtNheight, xtbin->height);n++;
  XtSetArg(args[n], XtNwidth,  xtbin->width);n++;
  XtSetValues(top_widget, args, n);

  embeded = XtVaCreateWidget("form", compositeWidgetClass, top_widget);

  n = 0;
  XtSetArg(args[n], XtNheight, xtbin->height);n++;
  XtSetArg(args[n], XtNwidth,  xtbin->width);n++;
  XtSetValues(embeded, args, n);

  /* Ok, here is the dirty little secret on how I am */
  /* switching the widget's XWindow to the GdkWindow's XWindow. */
  /* I should be ashamed of myself! */
  top_widget->core.window = GDK_WINDOW_XWINDOW(widget->window);

  /* this little trick seems to finish initializing the widget */
  XtRegisterDrawable(xtdisplay, 
                     GDK_WINDOW_XWINDOW(widget->window),
                     top_widget);
  
  XtRealizeWidget(embeded);
  printf("embeded window = %p\n", XtWindow(embeded));

  XtManageChild(embeded);

  /* now fill out the xtbin info */
  xtbin->xtdisplay = xtdisplay;
  xtbin->xtwindow  = XtWindow(embeded);

  /* listen to all Xt events */
  XSelectInput(xtdisplay, 
               XtWindow(top_widget), 
               GDK_ALL_EVENTS_MASK);
  XSelectInput(xtdisplay, 
               XtWindow(embeded), 
               GDK_ALL_EVENTS_MASK);
  XFlush(xtdisplay);
}

GtkWidget*
gtk_xtbin_new (GdkWindow *parent_window, String * f)
{
  GtkXtBin *xtbin;

  assert(parent_window != NULL);

  xtbin = gtk_type_new (GTK_TYPE_XTBIN);
  xtbin->parent_window = parent_window;
  if (f)
    fallback = f;

  return GTK_WIDGET (xtbin);
}

void
gtk_xtbin_set_position (GtkXtBin *xtbin,
                        gint       x,
                        gint       y)
{
  xtbin->x = x;
  xtbin->y = y;

  if (GTK_WIDGET_REALIZED (xtbin))
    gdk_window_move (GTK_WIDGET (xtbin)->window, x, y);
}












