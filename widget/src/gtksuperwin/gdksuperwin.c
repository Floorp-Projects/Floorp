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

#include "gdksuperwin.h"

static void gdk_superwin_class_init(GdkSuperWinClass *klass);
static void gdk_superwin_init(GdkSuperWin *superwin);

typedef struct _GdkSuperWinTranslate GdkSuperWinTranslate;

struct _GdkSuperWinTranslate
{
  unsigned long serial;
  gint dx;
  gint dy;
};

GtkType
gdk_superwin_get_type(void)
{
  static GtkType superwin_type = 0;
  
  if (!superwin_type)
    {
      static const GtkTypeInfo superwin_info =
      {
        "GtkSuperWin",
          sizeof(GdkSuperWin),
          sizeof(GdkSuperWinClass),
          (GtkClassInitFunc) gdk_superwin_class_init,
          (GtkObjectInitFunc) gdk_superwin_init,
          /* reserved_1 */ NULL,
          /* reserved_2 */ NULL,
          (GtkClassInitFunc) NULL
      };
      
      superwin_type = gtk_type_unique (gtk_object_get_type(), &superwin_info);
    }
  return superwin_type;
}

static void
gdk_superwin_class_init(GdkSuperWinClass *klass)
{
  GtkObjectClass *object_class;

  object_class = GTK_OBJECT_CLASS(klass);

  /* XXX do we need a finalize in here to destroy the
     window? */
}

static void
gdk_superwin_init(GdkSuperWin *superwin)
{
  
}

static GdkFilterReturn  gdk_superwin_bin_filter   (GdkXEvent *gdk_xevent,
                                                   GdkEvent  *event,
                                                   gpointer   data);
static GdkFilterReturn  gdk_superwin_shell_filter (GdkXEvent *gdk_xevent,
                                                   GdkEvent  *event,
                                                   gpointer   data);
static void             gdk_superwin_expose_area  (GdkSuperWin *superwin,
                                                   gint         x,
                                                   gint         y,
                                                   gint         width,
                                                   gint         height);
static gboolean gravity_works;

GdkSuperWin *
gdk_superwin_new (GdkWindow *parent_window,
                  guint      x,
                  guint      y,
                  guint      width,
                  guint      height)
{
  GdkWindowAttr         attributes;
  gint                  attributes_mask;
  Window                bin_xwindow;
  Display              *xdisplay;
  XSetWindowAttributes  xattr;
  unsigned long         xattr_mask;

  GdkSuperWin *superwin = gtk_type_new(GDK_TYPE_SUPERWIN);

  superwin->translate_queue = NULL;

  superwin->shell_func = NULL;
  superwin->bin_func = NULL;
  superwin->func_data = NULL;
  superwin->notify = NULL;

  /* Create the shell (clipping) window */
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = x;
  attributes.y = y;
  attributes.width = width;
  attributes.height = height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gdk_rgb_get_visual();
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_VISUAL | GDK_WA_X | GDK_WA_Y;

  superwin->shell_window = gdk_window_new (parent_window,
					   &attributes, attributes_mask);

  /* if we failed to create a window, die a horrible death */
  g_assert(superwin->shell_window);

  /* Create the bin window for drawing */

  attributes.x = 0;
  attributes.y = 0;
  attributes.event_mask = GDK_EXPOSURE_MASK;

  superwin->bin_window = gdk_window_new (superwin->shell_window,
                                         &attributes, attributes_mask);

  /* set the backing store for the bin window */
  bin_xwindow = GDK_WINDOW_XWINDOW(superwin->bin_window);
  xdisplay = GDK_WINDOW_XDISPLAY(superwin->bin_window);
  /* XXX should we make this Always? */
  xattr.backing_store = WhenMapped;
  xattr_mask = CWBackingStore;
  /* XChangeWindowAttributes(xdisplay, bin_xwindow, xattr_mask, &xattr); */

  gdk_window_show (superwin->bin_window);

  gdk_window_add_filter (superwin->shell_window, gdk_superwin_shell_filter, superwin);
  gdk_window_add_filter (superwin->bin_window, gdk_superwin_bin_filter, superwin);

  gravity_works = gdk_window_set_static_gravities (superwin->bin_window, TRUE);

  return superwin;
}

/* XXX this should really be part of the object... */
/* XXX and it should chain up to the object's destructor */

void gdk_superwin_destroy(GdkSuperWin *superwin)
{
  gdk_window_remove_filter(superwin->shell_window,
                           gdk_superwin_shell_filter,
                           superwin);
  gdk_window_remove_filter(superwin->bin_window,
                           gdk_superwin_bin_filter,
                           superwin);
  gdk_window_destroy(superwin->bin_window);
  gdk_window_destroy(superwin->shell_window);
}

void         
gdk_superwin_scroll (GdkSuperWin *superwin,
		     gint dx,
		     gint dy)
{
  GdkSuperWinTranslate *translate;
  gint width, height;

  gdk_window_get_size (superwin->shell_window, &width, &height);

  if (dx > 0 || dy > 0) 
    {
      translate = g_new (GdkSuperWinTranslate, 1);
      translate->dx = MAX (0, dx);
      translate->dy = MAX (0, dy);
      translate->serial = NextRequest (GDK_DISPLAY());
      superwin->translate_queue = g_list_append (superwin->translate_queue, translate);
    }


  gdk_window_move_resize (superwin->bin_window,
			  MIN (0, -dx), MIN (0, -dy),
			  width + ABS(dx), height + ABS(dy));

  gdk_window_move (superwin->bin_window,
		   MIN (0, -dx) + dx, MIN (0, -dy) + dy);  

  if (dx < 0 || dy < 0) 
    {
      translate = g_new (GdkSuperWinTranslate, 1);
      translate->dx = MIN (0, dx);
      translate->dy = MIN (0, dy);
      translate->serial = NextRequest (GDK_DISPLAY());
      superwin->translate_queue = g_list_append (superwin->translate_queue, translate);
    }

  gdk_window_move_resize (superwin->bin_window,
			  0, 0, width, height);

  if (dx < 0)
    gdk_superwin_expose_area (superwin,
			      MAX (width + dx, 0), 0,
			      MIN (-dx, width),    height);
  else
    gdk_superwin_expose_area (superwin,
			      0,                   0,
			      MIN (dx, width),     height);

  if (dy < 0)
    gdk_superwin_expose_area (superwin, 
			      MIN (dx, 0),              MAX (height + dy, 0),
			      MIN (width - ABS(dx), 0), MIN (-dy, height));
  else
    gdk_superwin_expose_area (superwin, 
			      MIN (dx, 0),              0,
			      MIN (width - ABS(dx), 0), MIN (dy, height));
  
}

void  
gdk_superwin_set_event_funcs (GdkSuperWin    *superwin,
                              GdkSuperWinFunc shell_func,
                              GdkSuperWinFunc bin_func,
                              gpointer        func_data,
                              GDestroyNotify  notify)
  
{
  if (superwin->notify && superwin->func_data)
    superwin->notify (superwin->func_data);
  
  superwin->shell_func = shell_func;
  superwin->bin_func = bin_func;
  superwin->func_data = func_data;
  superwin->notify = notify;

}

void gdk_superwin_resize (GdkSuperWin *superwin,
			  gint         width,
			  gint         height)
{
  gdk_window_resize (superwin->bin_window, width, height);
  gdk_window_resize (superwin->shell_window, width, height);
}

static void
gdk_superwin_expose_area  (GdkSuperWin *superwin,
                           gint         x,
                           gint         y,
                           gint         width,
                           gint         height)
{
  
}

static GdkFilterReturn 
gdk_superwin_bin_filter (GdkXEvent *gdk_xevent,
			 GdkEvent  *event,
			 gpointer   data)
{
  XEvent *xevent = (XEvent *)gdk_xevent;
  GdkSuperWin *superwin = data;
  GdkSuperWinTranslate *translate;
  GList *tmp_list;
  
  switch (xevent->xany.type) {
  case Expose:
    tmp_list = superwin->translate_queue;
    while (tmp_list) {
      translate = tmp_list->data;
      
      xevent->xexpose.x += translate->dx;
      xevent->xexpose.y += translate->dy;
      tmp_list = tmp_list->next;
    }
    
    if (superwin->bin_func) {
      superwin->bin_func (superwin, xevent, superwin->func_data);
    }

    return GDK_FILTER_REMOVE;
    break;

  case ConfigureNotify:
    gdk_superwin_clear_translate_queue(superwin, xevent->xany.serial);
    return GDK_FILTER_REMOVE;
    break;
  }

  return GDK_FILTER_CONTINUE;
}

static GdkFilterReturn 
gdk_superwin_shell_filter (GdkXEvent *gdk_xevent,
			  GdkEvent  *event,
			  gpointer   data)
{
  XEvent *xevent = (XEvent *)gdk_xevent;
  GdkSuperWin *superwin = data;
  
  if (xevent->type == VisibilityNotify)
    {
      switch (xevent->xvisibility.state)
        {
        case VisibilityFullyObscured:
          superwin->visibility = GDK_VISIBILITY_FULLY_OBSCURED;
          break;
          
        case VisibilityPartiallyObscured:
          superwin->visibility = GDK_VISIBILITY_PARTIAL;
          break;
          
        case VisibilityUnobscured:
          superwin->visibility = GDK_VISIBILITY_UNOBSCURED;
          break;
        }
      
      return GDK_FILTER_REMOVE;
    }
  
  if (superwin->shell_func) {
    /* g_print("calling func %p with arg %p in win %p\n",
       superwin->event_func, superwin->func_data, superwin); */
    superwin->shell_func (superwin, xevent, superwin->func_data);
  }

  return GDK_FILTER_CONTINUE;
}

void
gdk_superwin_clear_translate_queue(GdkSuperWin *superwin, unsigned long serial)
{
  GdkSuperWinTranslate *translate;
  GList *tmp_list;
  GList *link_to_remove = NULL;

  if (superwin->translate_queue) {
    tmp_list = superwin->translate_queue;
    while (tmp_list) {
      translate = tmp_list->data;
      if (serial == translate->serial) {
        g_free (tmp_list->data);
        superwin->translate_queue = g_list_remove_link (superwin->translate_queue, tmp_list);
        link_to_remove = tmp_list;
      }
      tmp_list = tmp_list->next;
      if (link_to_remove) {
        g_list_free_1(link_to_remove);
        link_to_remove = NULL;
      }
    }
  }
}

