/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Owen Taylor <otaylor@redhat.com> and Christopher Blizzard <blizzard@redhat.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#include "gdksuperwin.h"

static void gdk_superwin_class_init(GdkSuperWinClass *klass);
static void gdk_superwin_init(GdkSuperWin *superwin);
static void gdk_superwin_expose_area  (GdkSuperWin *superwin,
                                       gint         x,
                                       gint         y,
                                       gint         width,
                                       gint         height);
static void gdk_superwin_flush(GdkSuperWin *superwin);
static void gdk_superwin_destroy(GtkObject *object);

static void gdk_superwin_add_translation(GdkSuperWin *superwin, unsigned long serial,
                                         gint dx, gint dy);
static void gdk_superwin_add_antiexpose (GdkSuperWin *superwin, unsigned long serial,
                                         gint x, gint y,
                                         gint width, gint height);

static void gdk_superwin_handle_expose (GdkSuperWin *superwin, XEvent *xevent,
                                        GdkRegion **region, gboolean dont_recurse);

static Bool gdk_superwin_expose_predicate(Display  *display,
                                          XEvent   *xevent,
                                          XPointer  arg);

typedef struct _GdkSuperWinTranslate GdkSuperWinTranslate;

enum {
  GDK_SUPERWIN_TRANSLATION = 1,
  GDK_SUPERWIN_ANTIEXPOSE
};
  

struct _GdkSuperWinTranslate
{
  int type;
  unsigned long serial;
  union {
    struct {
      gint dx;
      gint dy;
    } translation;
    struct {
      GdkRectangle rect;
    } antiexpose;
  } data;
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
  object_class->destroy = gdk_superwin_destroy;

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
  superwin->paint_func = NULL;
  superwin->flush_func = NULL;
  superwin->func_data = NULL;
  superwin->notify = NULL;

  /* Create the shell (clipping) window */
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = x;
  attributes.y = y;
  attributes.width = width;
  attributes.height = height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.colormap = gdk_rgb_get_cmap();
  attributes.visual = gdk_rgb_get_visual();
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

  attributes_mask = GDK_WA_VISUAL | GDK_WA_X | GDK_WA_Y | GDK_WA_COLORMAP;

  superwin->shell_window = gdk_window_new (parent_window,
					   &attributes, attributes_mask);

  /* set the back pixmap to None so that you don't end up with the gtk
     default which is BlackPixel */
  gdk_window_set_back_pixmap (superwin->shell_window, NULL, FALSE);

  /* if we failed to create a window, die a horrible death */
  g_assert((superwin->shell_window));

  /* Create the bin window for drawing */

  attributes.x = 0;
  attributes.y = 0;
  attributes.event_mask = GDK_EXPOSURE_MASK;

  superwin->bin_window = gdk_window_new (superwin->shell_window,
                                         &attributes, attributes_mask);

  /* set the back pixmap to None so that you don't end up with the gtk
     default which is BlackPixel */
  gdk_window_set_back_pixmap (superwin->bin_window, NULL, FALSE);

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

void gdk_superwin_destroy(GtkObject *object)
{
  
  GdkSuperWin *superwin = NULL;

  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_IS_OBJECT(object));
  g_return_if_fail(GTK_OBJECT_CONSTRUCTED(object));
  g_return_if_fail(GDK_IS_SUPERWIN(object));

  superwin = GDK_SUPERWIN(object);

  gdk_window_remove_filter(superwin->shell_window,
                           gdk_superwin_shell_filter,
                           superwin);
  gdk_window_remove_filter(superwin->bin_window,
                           gdk_superwin_bin_filter,
                           superwin);
  gdk_window_destroy(superwin->bin_window);
  gdk_window_destroy(superwin->shell_window);

  if (superwin->translate_queue) {
    GSList *tmp_list = superwin->translate_queue;
    while (tmp_list) {
      g_free(tmp_list->data);
      tmp_list = tmp_list->next;
    }
    g_slist_free(superwin->translate_queue);
  }
}

void gdk_superwin_reparent(GdkSuperWin *superwin,
                           GdkWindow   *parent_window)
{
  gdk_window_reparent(superwin->shell_window,
                      parent_window, 0, 0);
}

void         
gdk_superwin_scroll (GdkSuperWin *superwin,
                     gint dx,
                     gint dy)
{
  gint width, height;

  gint first_resize_x = 0;
  gint first_resize_y = 0;
  gint first_resize_width = 0;
  gint first_resize_height = 0;

  unsigned long first_resize_serial = 0;
  unsigned long move_serial = 0;
  unsigned long last_resize_serial = 0;
  gint move_x = 0;
  gint move_y = 0;

  /* get the current dimensions of the window */
  gdk_window_get_size (superwin->shell_window, &width, &height);

  /* calculate the first resize. */

  /* for the first move resize, the width + height default to the
     width and height of the window. */
  first_resize_width = width;
  first_resize_height = height;

  /* if scrolling left ( dx < 0 ) need to extend window right by
     ABS(dx) and left side of window won't move. */
  if (dx < 0) {
    /* left side of the window doesn't move. */
    first_resize_x = 0;
    /* right side of window will be the width + the offset of the
       scroll */
    first_resize_width = width + ABS(dx);
  }

  /* if scrolling right ( dx > 0 ) need to extend window left by
     ABS(dx) and right side of the window doesn't move */
  if (dx > 0) {
    /* left side of the window will be offset to the left by ABS(dx) (
       x will be < 0 ) */
    first_resize_x = -dx;
    /* right side of the window won't move.  We have to add the offset
       to the width so that it doesn't. */
    first_resize_width = width + dx;
  }

  /* if scrolling down ( dy < 0 ) need to extend window down by
     ABS(dy) and top of window won't move */
  if (dy < 0) {
    /* top of window not moving */
    first_resize_y = 0;
    /* bottom of window will be the height + the offset of the scroll */
    first_resize_height = height + ABS(dy);
  }

  /* if scrolling up ( dy > 0 ) need to extend window up by ABS(dy)
     and bottom of window won't move. */
  if (dy > 0) {
    /* top of the window will be moved up by ABS(dy) ( y will be < 0 ) */
    first_resize_y = -dy;
    /* this will cause the bottom of the window not to move since
       we're moving y by the offset up. */
    first_resize_height = height + dy; 
  }

  /* calculate our move offsets  */

  /* if scrolling left ( dx < 0 ) we need to move the window left by
     ABS(dx) */
  if (dx < 0) {
    /* dx will be negative - this will move it left */
    move_x = dx;
  }

  /* if scrolling right ( dx > 0 ) we need to move the window right by
     ABS(dx).  Because we already resized the window to -dx by moving
     it to zero we are actually moving the window to the right. */
  if (dx > 0) {
    move_x = 0;
  }

  /* If scrolling down ( dy < 0 ) we need to move the window up by
     ABS(dy) */
  if (dy < 0) {
    /* dy will be negative - this will move it up */
    move_y = dy;
  }

  /* If scrolling up ( dy > 0 ) we need to move the window down by
     ABS(dy).  In this case we already resized the top of the window
     to -dy so by moving it to zero we are actually moving the window
     down. */
  if (dy > 0) {
    move_y = 0;
  }

  /* save the serial of the first request */
  first_resize_serial = NextRequest(GDK_DISPLAY());

  /* move our window */
  gdk_window_move_resize(superwin->bin_window,
                         first_resize_x, first_resize_y,
                         first_resize_width, first_resize_height);

  /* window move - this move will cause all of the exposes to happen
     and is where all the translation magic has to be applied.  we
     need to save the serial of this operation since any expose events
     with serial request numbers lower than it will have to have their
     coordinates translated into the new coordinate system. */

  move_serial = NextRequest(GDK_DISPLAY());

  gdk_window_move(superwin->bin_window,
                  move_x, move_y);

  /* last resize.  This will resize the window to its original
     position. */

  /* save the request of the last resize */
  last_resize_serial = NextRequest(GDK_DISPLAY());

  gdk_window_move_resize(superwin->bin_window,
                         0, 0, width, height);

  /* now that we have moved everything, repaint the damaged areas */

  /* if scrolling left ( dx < 0 ) moving window left so expose area at
     the right of the window.  Clip the width of the exposure to the
     width of the window or the offset, whichever is smaller.  Also
     clip the start to the origin ( 0 ) if the width minus the
     absolute value of the offset is < 0 to handle > 1 page
     scrolls. */
  if (dx < 0) {
    gdk_superwin_expose_area(superwin, MAX(0, width - ABS(dx)), 0,
                             MIN(width, ABS(dx)), height);
    /* If we've exposed this area add an antiexpose for it.  When the
       window was moved left the expose will be offset by ABS(dx).
       For greated than one page scrolls, the expose will be offset by
       the actual offset of the move, hence the MAX(height,
       ABS(dy)). */
    gdk_superwin_add_antiexpose(superwin, move_serial,
                                MAX(width, ABS(dx)),
                                0, MIN(width, ABS(dx)), height);
  }

  /* if scrolling right ( dx > 0 ) moving window right so expose area
     at the left of the window.  Clip the width of the exposure to the
     width of the window or the offset, whichever is smaller. */
  if (dx > 0) {
    gdk_superwin_expose_area(superwin, 0, 0, 
                             MIN(width, ABS(dx)), height);
    /* If we've exposed this area add an antiexpose for it. */
    gdk_superwin_add_antiexpose(superwin, move_serial,
                                0, 0, MIN(width, ABS(dx)), height);
  }

  /* if scrolling down ( dy < 0 ) moving window up so expose area at
     the bottom of the window.  Clip the exposed area to the height of
     the window or the offset, whichever is smaller.  Also clip the
     start to the origin ( 0 ) if the the height minus the absolute
     value of the offset is < 0 to handle > 1 page scrolls. */

  if (dy < 0) {
    gdk_superwin_expose_area(superwin, 0, MAX(0, height - ABS(dy)),
                             width, MIN(height, ABS(dy)));
    /* If we've exposed this area add an antiexpose for it.  When the
       was moved up before the second move the expose will be offset
       by ABS(dy).  For greater than one page scrolls, the expose will
       be offset by actual offset of the move, hence the MAX(height,
       ABS(dy)).  */
        gdk_superwin_add_antiexpose(superwin, move_serial,
                                    0,
                                    MAX(height, ABS(dy)),
                                    width, MIN(height, ABS(dy)));
  }

  /* if scrolling up ( dy > 0 ) moving window down so expose area at
     the top of the window.  Clip the exposed area to the height or
     the offset, whichever is smaller. */
  if (dy > 0) {
    gdk_superwin_expose_area(superwin, 0, 0,
                             width, MIN(height, ABS(dy)));
    /* if we've exposed this area add an antiexpose for it. */
    gdk_superwin_add_antiexpose(superwin, move_serial,
                                0, 0, width, MIN(height, ABS(dy)));
  }

  /* if we are scrolling right or down ( dx > 0 or dy > 0 ) we need to
     add our translation before the fist move_resize.  Once we do this
     all previous expose events will be translated into the new
     coordinate space */
  if (dx > 0 || dy > 0) {
    gdk_superwin_add_translation(superwin, first_resize_serial,
                                 MAX(0, dx), MAX(0, dy));
  }

  /* If we are scrolling left or down ( x < 0 or y < 0 ) we need to
     add our translation before the last move_resize.  Once we do this
     all previous expose events will be translated in the new
     coordinate space. */
  if (dx < 0 || dy < 0) {
    gdk_superwin_add_translation(superwin, last_resize_serial,
                                 MIN(0, dx), MIN(0, dy));
  }

  /* sync so we get the windows moved now */
  XSync(GDK_DISPLAY(), False);
}

void  
gdk_superwin_set_event_funcs (GdkSuperWin               *superwin,
                              GdkSuperWinFunc            shell_func,
                              GdkSuperWinPaintFunc       paint_func,
                              GdkSuperWinPaintFlushFunc  flush_func,
                              GdkSuperWinKeyPressFunc    keyprs_func,
                              GdkSuperWinKeyReleaseFunc  keyrel_func,
                              gpointer                   func_data,
                              GDestroyNotify             notify)
{
  if (superwin->notify && superwin->func_data)
    superwin->notify (superwin->func_data);
  
  superwin->shell_func = shell_func;
  superwin->paint_func = paint_func;
  superwin->flush_func = flush_func;
  superwin->keyprs_func = keyprs_func;
  superwin->keyrel_func = keyrel_func;
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
  if (superwin->paint_func)
    superwin->paint_func(x, y, width, height, superwin->func_data);
}

static void
gdk_superwin_flush(GdkSuperWin *superwin)
{
  if (superwin->flush_func)
    superwin->flush_func(superwin->func_data);
}

static GdkFilterReturn 
gdk_superwin_bin_filter (GdkXEvent *gdk_xevent,
                         GdkEvent  *event,
                         gpointer   data)
{
  XEvent *xevent = (XEvent *)gdk_xevent;
  GdkSuperWin *superwin = data;
  GdkFilterReturn retval = GDK_FILTER_CONTINUE;
  GdkRegion *region = NULL;

  switch (xevent->xany.type) {
  case Expose:
    region = gdk_region_new();
    retval = GDK_FILTER_REMOVE;
    gdk_superwin_handle_expose(superwin, xevent, &region, FALSE);
    gdk_region_destroy(region);
    break;
  case KeyPress:
    if (superwin->keyprs_func)
      superwin->keyprs_func(&xevent->xkey);
    break;
  case KeyRelease:
    if (superwin->keyrel_func)
      superwin->keyrel_func(&xevent->xkey);
    break;
  default:
    break;
  }
  return retval;
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
    superwin->shell_func (superwin, xevent, superwin->func_data);
  }
  return GDK_FILTER_CONTINUE;
}

/* this function is used to check whether or not a particular event is
   for a specific superwin and is a ConfigureNotify or Expose
   event. */

Bool gdk_superwin_expose_predicate(Display  *display,
                                   XEvent   *xevent,
                                   XPointer  arg) {
  GdkSuperWin *superwin = (GdkSuperWin *)arg;
  if (xevent->xany.window != GDK_WINDOW_XWINDOW(superwin->bin_window))
    return False;
  switch (xevent->xany.type) {
  case Expose:
    return True;
    break;
  default:
    return False;
    break;
  }
}

/* static */
void gdk_superwin_add_translation(GdkSuperWin *superwin, unsigned long serial,
                                  gint dx, gint dy)
{
  GdkSuperWinTranslate *translate = g_new(GdkSuperWinTranslate, 1);
  translate->type = GDK_SUPERWIN_TRANSLATION;
  translate->serial = serial;
  translate->data.translation.dx = dx;
  translate->data.translation.dy = dy;
  superwin->translate_queue = g_slist_append(superwin->translate_queue, translate);
}

/* static */
void gdk_superwin_add_antiexpose (GdkSuperWin *superwin, unsigned long serial,
                                  gint x, gint y,
                                  gint width, gint height)

{
  GdkSuperWinTranslate *translate = g_new(GdkSuperWinTranslate, 1);
  translate->type = GDK_SUPERWIN_ANTIEXPOSE;
  translate->serial = serial;
  translate->data.antiexpose.rect.x = x;
  translate->data.antiexpose.rect.y = y;
  translate->data.antiexpose.rect.width = width;
  translate->data.antiexpose.rect.height = height;
  superwin->translate_queue = g_slist_append(superwin->translate_queue, translate);
}

/* static */
void gdk_superwin_handle_expose (GdkSuperWin *superwin, XEvent *xevent,
                                 GdkRegion **region, gboolean dont_recurse)
{
  GSList *tmp_list;
  gboolean send_event = TRUE;
  unsigned long serial = xevent->xany.serial;
  XEvent extra_event;
  GdkRectangle rect;
  GdkRegion *tmp_region = NULL;
  gboolean   is_special = TRUE;

  /* set up our rect for the damaged area */
  rect.x = xevent->xexpose.x;
  rect.y = xevent->xexpose.y;
  rect.width = xevent->xexpose.width;
  rect.height = xevent->xexpose.height;

  /* try to see if this is a special event that matches an antiexpose */
  tmp_list = superwin->translate_queue;
  while (tmp_list) {
    GdkSuperWinTranslate *xlate = tmp_list->data;
    if (xlate->type == GDK_SUPERWIN_ANTIEXPOSE && serial == xlate->serial) {
      GdkRegion *antiexpose_region = gdk_region_new();
      tmp_region = gdk_region_union_with_rect(antiexpose_region, 
                                              &xlate->data.antiexpose.rect);
      gdk_region_destroy(antiexpose_region);
      antiexpose_region = tmp_region;
      /* if the rect of the expose event is contained in the
         antiexpose then we should just drop it on the floor. */
      if (gdk_region_rect_in(antiexpose_region, &rect) == GDK_OVERLAP_RECTANGLE_IN) {
        gdk_region_destroy(antiexpose_region);
        goto end;
      }
      gdk_region_destroy(antiexpose_region);

    }
    tmp_list = tmp_list->next;
  }

  /* we walk the list looking for any transformations */
  tmp_list = superwin->translate_queue;
  while (tmp_list) {
    GdkSuperWinTranslate *xlate = tmp_list->data;
    /* apply translations to this event if we can. */
    if (xlate->type == GDK_SUPERWIN_TRANSLATION && serial < xlate->serial ) {
      rect.x += xlate->data.translation.dx;
      rect.y += xlate->data.translation.dy;
    }
    tmp_list = tmp_list->next;
  }

  /* add this expose area to our damaged rect */

  tmp_region = gdk_region_union_with_rect(*region, &rect);
  gdk_region_destroy(*region);
  *region = tmp_region;

 end:

  /* remove any events from the queue that are old */
  tmp_list = superwin->translate_queue;
  while (tmp_list) {
    GdkSuperWinTranslate *xlate = tmp_list->data;
    if (serial > xlate->serial) {
      GSList *tmp_link = tmp_list;
      tmp_list = tmp_list->next;
      superwin->translate_queue = g_slist_remove_link(superwin->translate_queue,
                                                      tmp_link);
      g_free(tmp_link->data);
      g_slist_free_1(tmp_link);
    }
    else {
      tmp_list = tmp_list->next;
    }
  }

  /* if we're not supposed to recurse or paint then return now */
  if (dont_recurse)
    return;

  /* try to do any expose event compression we can */
  while (XCheckTypedWindowEvent(xevent->xany.display,
                                xevent->xany.window,
                                Expose,
                                &extra_event) == True) {
    gdk_superwin_handle_expose(superwin, &extra_event, region, TRUE);
  }

  /* if the region isn't empty, send the paint event */
  if (gdk_region_empty(*region) == FALSE) {
      GdkRectangle clip_box;
      gdk_region_get_clipbox(*region, &clip_box);
      if (superwin->paint_func)
        superwin->paint_func(clip_box.x, clip_box.y,
                             clip_box.width, clip_box.height,
                             superwin->func_data);
  }

}
