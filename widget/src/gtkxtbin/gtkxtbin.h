/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * GtkXtBin Widget Declaration
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

#ifndef __GTK_XTBIN_H__
#define __GTK_XTBIN_H__

#include <gtk/gtkwidget.h>
#include <X11/Intrinsic.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkXtBin GtkXtBin;
typedef struct _GtkXtBinClass GtkXtBinClass;

#define GTK_TYPE_XTBIN                  (gtk_xtbin_get_type ())
#define GTK_XTBIN(obj)                  (GTK_CHECK_CAST ((obj), \
                                         GTK_TYPE_XTBIN, GtkXtBin))
#define GTK_XTBIN_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), \
                                         GTK_TYPE_XTBIN, GtkXtBinClass))
#define GTK_IS_XTBIN(obj)               (GTK_CHECK_TYPE ((obj), \
                                         GTK_TYPE_XTBIN))
#define GTK_IS_XTBIN_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), \
                                         GTK_TYPE_XTBIN))

struct _GtkXtBin
{
  GtkWidget      widget;
  GdkWindow     *parent_window;
  Display       *xtdisplay;        /* Xt Toolkit Display */

  Widget         xtwidget;         /* Xt Toolkit Widget */

  Window         xtwindow;         /* Xt Toolkit XWindow */
  gint           x, y;
  gint           width, height;
};
  
struct _GtkXtBinClass
{
  GtkWidgetClass widget_class;
};

GtkType    gtk_xtbin_get_type (void);
GtkWidget *gtk_xtbin_new (GdkWindow *parent_window, String *f);
void       gtk_xtbin_set_position (GtkXtBin *xtbin,
                                   gint       x,
                                   gint       y);
void       gtk_xtbin_resize (GtkWidget *widget,
                             gint       width,
                             gint       height);

typedef struct _XtTMRec {
    XtTranslations  translations;       /* private to Translation Manager    */
    XtBoundActions  proc_table;         /* procedure bindings for actions    */
    struct _XtStateRec *current_state;  /* Translation Manager state ptr     */
    unsigned long   lastEventTime;
} XtTMRec, *XtTM;   

typedef struct _CorePart {
    Widget          self;               /* pointer to widget itself          */
    WidgetClass     widget_class;       /* pointer to Widget's ClassRec      */
    Widget          parent;             /* parent widget                     */
    XrmName         xrm_name;           /* widget resource name quarkified   */
    Boolean         being_destroyed;    /* marked for destroy                */
    XtCallbackList  destroy_callbacks;  /* who to call when widget destroyed */
    XtPointer       constraints;        /* constraint record                 */
    Position        x, y;               /* window position                   */
    Dimension       width, height;      /* window dimensions                 */
    Dimension       border_width;       /* window border width               */
    Boolean         managed;            /* is widget geometry managed?       */
    Boolean         sensitive;          /* is widget sensitive to user events*/
    Boolean         ancestor_sensitive; /* are all ancestors sensitive?      */
    XtEventTable    event_table;        /* private to event dispatcher       */
    XtTMRec         tm;                 /* translation management            */
    XtTranslations  accelerators;       /* accelerator translations          */
    Pixel           border_pixel;       /* window border pixel               */
    Pixmap          border_pixmap;      /* window border pixmap or NULL      */
    WidgetList      popup_list;         /* list of popups                    */
    Cardinal        num_popups;         /* how many popups                   */
    String          name;               /* widget resource name              */
    Screen          *screen;            /* window's screen                   */
    Colormap        colormap;           /* colormap                          */
    Window          window;             /* window ID                         */
    Cardinal        depth;              /* number of planes in window        */
    Pixel           background_pixel;   /* window background pixel           */
    Pixmap          background_pixmap;  /* window background pixmap or NULL  */
    Boolean         visible;            /* is window mapped and not occluded?*/
    Boolean         mapped_when_managed;/* map window if it's managed?       */
} CorePart;

typedef struct _WidgetRec {
    CorePart    core;
 } WidgetRec, CoreRec;   

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __GTK_XTBIN_H__ */

