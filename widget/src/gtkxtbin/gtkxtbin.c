/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2: */
 
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
 * The Original Code is the GtkXtBin Widget Implementation.
 *
 * The Initial Developer of the Original Code is
 * Intel Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
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
 
/*
 * The GtkXtBin widget allows for Xt toolkit code to be used
 * inside a GTK application.  
 */

#include "gtkxtbin.h"
#include <gtk/gtkcontainer.h>
#include <gtk/gtkmain.h>
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

/* uncomment this if you want debugging information about widget
   creation and destruction */
/* #define DEBUG_XTBIN */

#define XTBIN_MAX_EVENTS 30

static void            gtk_xtbin_class_init (GtkXtBinClass *klass);
static void            gtk_xtbin_init       (GtkXtBin      *xtbin);
static void            gtk_xtbin_realize    (GtkWidget      *widget);
static void            gtk_xtbin_destroy    (GtkObject      *object);
static void            gtk_xtbin_shutdown   (GtkObject      *object);
static void            gtk_xtbin_show       (GtkWidget *widget);

static GtkWidgetClass *parent_class = NULL;

static String          *fallback = NULL;
static gboolean         xt_is_initialized = FALSE;
static gint             num_widgets = 0;

static GPollFD          xt_event_poll_fd;

static gboolean
xt_event_prepare (gpointer  source_data,
                   GTimeVal *current_time,
                   gint     *timeout,
                   gpointer  user_data)
{   
  int mask;

  GDK_THREADS_ENTER();
  mask = XPending((Display *)user_data);
  GDK_THREADS_LEAVE();

  return (gboolean)mask;
}

static gboolean
xt_event_check (gpointer  source_data,
                 GTimeVal *current_time,
                 gpointer  user_data)
{
  GDK_THREADS_ENTER ();

  if (xt_event_poll_fd.revents & G_IO_IN) {
    int mask;

    mask = XPending((Display *)user_data);
    
    GDK_THREADS_LEAVE ();

    return (gboolean)mask;
  }

  GDK_THREADS_LEAVE ();
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
  int i = 0;

  display = (Display *)user_data;
  ac = XtDisplayToApplicationContext(display);

  GDK_THREADS_ENTER ();

  /* Process only real X traffic here.  We only look for data on the
   * pipe, limit it to XTBIN_MAX_EVENTS and only call
   * XtAppProcessEvent so that it will look for X events.  There's no
   * timer processing here since we already have a timer callback that
   * does it.  */
  for (i=0; i < XTBIN_MAX_EVENTS && XPending(display); i++) {
    XtAppProcessEvent(ac, XtIMXEvent);
  }

  GDK_THREADS_LEAVE ();

  return TRUE;  
}

static GSourceFuncs xt_event_funcs = {
  xt_event_prepare,
  xt_event_check,
  xt_event_dispatch,
  (GDestroyNotify)g_free
};

static gint             xt_polling_timer_id = 0;

static gboolean
xt_event_polling_timer_callback(gpointer user_data)
{
  Display * display;
  XtAppContext ac;

  display = (Display *)user_data;
  ac = XtDisplayToApplicationContext(display);

  /* don't starve the primary event queue - just process one event */
  if (XtAppPending(ac))
    XtAppProcessEvent(ac, XtIMAll);

  /* restart the timer */
  return TRUE;
}

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

      xtbin_type = gtk_type_unique (GTK_TYPE_WIDGET, &xtbin_info);
    }

  return xtbin_type;
}

static void
gtk_xtbin_class_init (GtkXtBinClass *klass)
{
  GtkWidgetClass *widget_class;
  GtkObjectClass *object_class;

  parent_class = gtk_type_class (gtk_widget_get_type ());

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gtk_xtbin_realize;
  widget_class->show = gtk_xtbin_show;

  object_class = GTK_OBJECT_CLASS (klass);
  object_class->shutdown= gtk_xtbin_shutdown;
  object_class->destroy = gtk_xtbin_destroy;
}

static void
gtk_xtbin_init (GtkXtBin *xtbin)
{
  xtbin->xtdisplay = NULL;
  xtbin->xtwidget = NULL;
  xtbin->parent_window = NULL;
  xtbin->xtwindow = 0;
  xtbin->x = 0;
  xtbin->y = 0;
}

static void
gtk_xtbin_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  gint          attributes_mask, n;
  GtkXtBin     *xtbin;
  Arg           args[5];
  gint          width, height;
  Widget        top_widget;
  Window        win;
  Widget        embeded;

#ifdef DEBUG_XTBIN
  printf("gtk_xtbin_realize()\n");
#endif

  g_return_if_fail (GTK_IS_XTBIN (widget));

  gdk_flush();
  xtbin = GTK_XTBIN (widget);

  if (widget->allocation.x == -1 &&
      widget->allocation.y == -1 &&
      widget->allocation.width == 1 &&
      widget->allocation.height == 1)
  {
    GtkRequisition requisition;
    GtkAllocation allocation = { 0, 0, 200, 200 };

    gtk_widget_size_request (widget, &requisition);
    if (requisition.width || requisition.height)
    {
      /* non-empty window */
      allocation.width = requisition.width;
      allocation.height = requisition.height;
    }
    gtk_widget_size_allocate (widget, &allocation);
  }

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type  = GDK_WINDOW_CHILD;
  attributes.x            = xtbin->x;
  attributes.y            = xtbin->y;
  attributes.width        = widget->allocation.width;
  attributes.height       = widget->allocation.height;
  attributes.wclass       = GDK_INPUT_OUTPUT;
  attributes.visual       = gdk_window_get_visual (xtbin->parent_window);
  attributes.colormap     = gdk_window_get_colormap (xtbin->parent_window);
  attributes.event_mask   = gdk_window_get_events (xtbin->parent_window);
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

  /* ensure all the outgoing events are flushed */
  /* before we try this crazy dual toolkit stuff */
  gdk_flush();


  /***
   * I'm sure there is a better way, but for now I'm 
   * just creating a new application shell (since it doesn't
   * need a parent widget,) and then swapping out the XWindow
   * from under it.  This seems to mostly work, but it's
   * an ugly hack.
   */

  top_widget = XtAppCreateShell("drawingArea", "Wrapper", 
                                applicationShellWidgetClass, xtbin->xtdisplay, 
                                NULL, 0);

  xtbin->xtwidget = top_widget;

  /* set size of Xt window */
  n = 0;
  XtSetArg(args[n], XtNheight, xtbin->height);n++;
  XtSetArg(args[n], XtNwidth,  xtbin->width);n++;
  XtSetValues(top_widget, args, n);

  embeded = XtVaCreateWidget("form", compositeWidgetClass, top_widget, NULL);

  n = 0;
  XtSetArg(args[n], XtNheight, xtbin->height);n++;
  XtSetArg(args[n], XtNwidth,  xtbin->width);n++;
  XtSetArg(args[n], XtNvisual, GDK_VISUAL_XVISUAL(gdk_window_get_visual( xtbin->parent_window )) ); n++;
  XtSetArg(args[n], XtNdepth, gdk_window_get_visual( xtbin->parent_window )->depth ); n++;
  XtSetArg(args[n], XtNcolormap, GDK_COLORMAP_XCOLORMAP(gdk_window_get_colormap( xtbin->parent_window)) ); n++;
  XtSetValues(embeded, args, n);

  /* Ok, here is the dirty little secret on how I am */
  /* switching the widget's XWindow to the GdkWindow's XWindow. */
  /* I should be ashamed of myself! */
  gtk_object_set_data(GTK_OBJECT(widget), "oldwindow",
                      GUINT_TO_POINTER(top_widget->core.window)); /* save it off so we can get it during destroy */

  top_widget->core.window = GDK_WINDOW_XWINDOW(widget->window);
  

  /* this little trick seems to finish initializing the widget */
#if XlibSpecificationRelease >= 6
  XtRegisterDrawable(xtbin->xtdisplay, 
                     GDK_WINDOW_XWINDOW(widget->window),
                     top_widget);
#else
  _XtRegisterWindow( GDK_WINDOW_XWINDOW(widget->window),
                     top_widget);
#endif
  
  XtRealizeWidget(embeded);
#ifdef DEBUG_XTBIN
  printf("embeded window = %li\n", XtWindow(embeded));
#endif
  XtManageChild(embeded);

  /* now fill out the xtbin info */
  xtbin->xtwindow  = XtWindow(embeded);

  /* listen to all Xt events */
  XSelectInput(xtbin->xtdisplay, 
               XtWindow(top_widget), 
               0x0FFFFF);
  XSelectInput(xtbin->xtdisplay, 
               XtWindow(embeded), 
               0x0FFFFF);
  XFlush(xtbin->xtdisplay);
}

GtkWidget*
gtk_xtbin_new (GdkWindow *parent_window, String * f)
{
  static Display *xtdisplay = NULL;

  GtkXtBin *xtbin;
  assert(parent_window != NULL);

  xtbin = gtk_type_new (GTK_TYPE_XTBIN);

  if (!xtbin)
    return (GtkWidget*)NULL;

  /* Initialize the Xt toolkit */
  if (!xt_is_initialized) {
    char         *mArgv[1];
    int           mArgc = 0;
    XtAppContext  app_context;

#ifdef DEBUG_XTBIN
    printf("starting up Xt stuff\n");
#endif
    /*
     * Initialize Xt stuff
     */

    XtToolkitInitialize();
    app_context = XtCreateApplicationContext();
    if (fallback)
      XtAppSetFallbackResources(app_context, fallback);
  
    xtdisplay = XtOpenDisplay(app_context, gdk_get_display(), NULL, 
                              "Wrapper", NULL, 0, &mArgc, mArgv);

    if (!xtdisplay) {
      /* If XtOpenDisplay failed, we can't go any further.
       *  Bail out.
       */
#ifdef DEBUG_XTBIN
      printf("gtk_xtbin_init: XtOpenDisplay() returned NULL.\n");
#endif

      gtk_type_free (GTK_TYPE_XTBIN, xtbin);
      return (GtkWidget *)NULL;
    }

    xt_is_initialized = TRUE;
  }

  /* If this is the first running widget, hook this display into the
     mainloop */
  if (0 == num_widgets) {
    int           cnumber;
    /*
     * hook Xt event loop into the glib event loop.
     */

    /* the assumption is that gtk_init has already been called */
    g_source_add (GDK_PRIORITY_EVENTS, TRUE, 
                  &xt_event_funcs, NULL, xtdisplay, (GDestroyNotify)NULL);
    
#ifdef VMS
    cnumber = XConnectionNumber(xtdisplay);
#else
    cnumber = ConnectionNumber(xtdisplay);
#endif
    xt_event_poll_fd.fd = cnumber;
    xt_event_poll_fd.events = G_IO_IN; 
    xt_event_poll_fd.revents = 0;    /* hmm... is this correct? */

    g_main_add_poll (&xt_event_poll_fd, G_PRIORITY_LOW);

    /* add a timer so that we can poll and process Xt timers */
    xt_polling_timer_id =
      gtk_timeout_add(25,
                      (GtkFunction)xt_event_polling_timer_callback,
                      xtdisplay);
  }

  /* Bump up our usage count */
  num_widgets++;

  xtbin->xtdisplay = xtdisplay;
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

void
gtk_xtbin_resize (GtkWidget *widget,
                  gint       width,
                  gint       height)
{
  Arg args[2];
  GtkXtBin *xtbin = GTK_XTBIN (widget);
  Widget xtwidget = XtWindowToWidget(xtbin->xtdisplay, xtbin->xtwindow);

  XtSetArg(args[0], XtNheight, height);
  XtSetArg(args[1], XtNwidth,  width);
  XtSetValues(XtParent(xtwidget), args, 2);
}

static void
gtk_xtbin_shutdown (GtkObject *object)
{
  GtkXtBin *xtbin;
  GtkWidget *widget;

#ifdef DEBUG_XTBIN
  printf("gtk_xtbin_shutdown()\n");
#endif

  /* gtk_object_destroy() will already hold a refcount on object
   */
  xtbin = GTK_XTBIN(object);
  widget = GTK_WIDGET(object);

  if (widget->parent)
    gtk_container_remove (GTK_CONTAINER (widget->parent), widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
  if (GTK_WIDGET_REALIZED (widget)) {
#if XlibSpecificationRelease >= 6
    XtUnregisterDrawable(xtbin->xtdisplay,
                         GDK_WINDOW_XWINDOW(GTK_WIDGET(object)->window));
#else
    _XtUnregisterWindow(GDK_WINDOW_XWINDOW(GTK_WIDGET(object)->window),
                        XtWindowToWidget(xtbin->xtdisplay,
                        GDK_WINDOW_XWINDOW(GTK_WIDGET(object)->window)));
#endif

    /* flush the queue before we returning origin top_widget->core.window
       or we can get X error since the window is gone */
    XSync(xtbin->xtdisplay, False);

    xtbin->xtwidget->core.window = GPOINTER_TO_UINT(gtk_object_get_data(object, "oldwindow"));
    XtUnrealizeWidget(xtbin->xtwidget);

    gtk_widget_unrealize (widget);
  }


  gtk_object_remove_data(object, "oldwindow");

  GTK_OBJECT_CLASS(parent_class)->shutdown (object);
}


static void
gtk_xtbin_destroy (GtkObject *object)
{
  GtkXtBin *xtbin;

#ifdef DEBUG_XTBIN
  printf("gtk_xtbin_destroy()\n");
#endif

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_XTBIN (object));

  xtbin = GTK_XTBIN (object);

  XtDestroyWidget(xtbin->xtwidget);

  num_widgets--; /* reduce our usage count */

  /* If this is the last running widget, remove the Xt display
     connection from the mainloop */
  if (0 == num_widgets) {
    XtAppContext  ac;

#ifdef DEBUG_XTBIN
    printf("removing the Xt connection from the main loop\n");
#endif

    g_main_remove_poll(&xt_event_poll_fd);
    g_source_remove_by_user_data(xtbin->xtdisplay);

    gtk_timeout_remove(xt_polling_timer_id);
    xt_polling_timer_id = 0;

  }

  GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

static void
gtk_xtbin_show (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));

#ifdef DEBUG_XTBIN
  printf("gtk_xtbin_show()\n");
#endif

  if (!GTK_WIDGET_VISIBLE (widget))
  {
    GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);

    if (!GTK_WIDGET_MAPPED(widget))
      gtk_widget_map (widget);
  }
}
