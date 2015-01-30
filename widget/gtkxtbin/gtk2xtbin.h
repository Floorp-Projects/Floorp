/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set expandtab shiftwidth=2 tabstop=2: */
 
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __GTK_XTBIN_H__
#define __GTK_XTBIN_H__

#include <gtk/gtk.h>
#include <X11/Intrinsic.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#ifdef MOZILLA_CLIENT
#include "mozilla/Types.h"
#ifdef _IMPL_GTKXTBIN_API
#define GTKXTBIN_API(type) MOZ_EXPORT type
#else
#define GTKXTBIN_API(type) MOZ_IMPORT_API type
#endif
#else
#define GTKXTBIN_API(type) type
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _XtClient XtClient;

struct _XtClient {
  Display	*xtdisplay;
  Widget	top_widget;    /* The toplevel widget */
  Widget	child_widget;  /* The embedded widget */
  Visual	*xtvisual;
  int		xtdepth;
  Colormap	xtcolormap;
  Window	oldwindow;
};

#if (GTK_MAJOR_VERSION == 2)
typedef struct _GtkXtBin GtkXtBin;
typedef struct _GtkXtBinClass GtkXtBinClass;

#define GTK_TYPE_XTBIN                  (gtk_xtbin_get_type ())
#define GTK_XTBIN(obj)                  (G_TYPE_CHECK_INSTANCE_CAST  ((obj), \
                                         GTK_TYPE_XTBIN, GtkXtBin))
#define GTK_XTBIN_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                         GTK_TYPE_XTBIN, GtkXtBinClass))
#define GTK_IS_XTBIN(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                         GTK_TYPE_XTBIN))
#define GTK_IS_XTBIN_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                         GTK_TYPE_XTBIN))

struct _GtkXtBin
{
  GtkSocket      gsocket;
  GdkWindow     *parent_window;
  Display       *xtdisplay;        /* Xt Toolkit Display */

  Window         xtwindow;         /* Xt Toolkit XWindow */
  XtClient	 xtclient;         /* Xt Client for XEmbed */
};
  
struct _GtkXtBinClass
{
  GtkSocketClass parent_class;
};

GTKXTBIN_API(GType)       gtk_xtbin_get_type (void);
GTKXTBIN_API(GtkWidget *) gtk_xtbin_new (GdkWindow *parent_window, String *f);
#endif

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

/* Exported functions, used by Xt plugins */
void xt_client_create(XtClient * xtclient, Window embeder, int height, int width);
void xt_client_unrealize(XtClient* xtclient);
void xt_client_destroy(XtClient* xtclient);
void xt_client_init(XtClient * xtclient, Visual *xtvisual, Colormap xtcolormap, int xtdepth);
void xt_client_xloop_create(void);
void xt_client_xloop_destroy(void);
Display * xt_client_get_display(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __GTK_XTBIN_H__ */

