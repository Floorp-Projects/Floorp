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
 * Owen Taylor <otaylor@redhat.com> and Christopher Blizzard 
 * <blizzard@redhat.com>.
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

#ifndef __GDK_SUPERWIN_H__
#define __GDK_SUPERWIN_H__

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtkobject.h>
#ifdef MOZILLA_CLIENT
#include "nscore.h"
#ifdef _IMPL_GTKSUPERWIN_API
#define GTKSUPERWIN_API(type) NS_EXPORT_(type)
#else
#define GTKSUPERWIN_API(type) NS_IMPORT_(type)
#endif
#else
#define GTKSUPERWIN_API(type) type
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GdkSuperWin GdkSuperWin;
typedef struct _GdkSuperWinClass GdkSuperWinClass;

#define GDK_TYPE_SUPERWIN            (gdk_superwin_get_type())
#define GDK_SUPERWIN(obj)            (GTK_CHECK_CAST((obj), GDK_TYPE_SUPERWIN, GdkSuperWin))
#define GDK_SUPERWIN_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), GDK_TYPE_SUPERWIN, GdkSuperWinClass))
#define GDK_IS_SUPERWIN(obj)         (GTK_CHECK_TYPE((obj), GDK_TYPE_SUPERWIN))
#define GDK_IS_SUPERWIN_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDK_TYPE_SUPERWIN))

typedef void (*GdkSuperWinFunc) (GdkSuperWin *super_win,
                                 XEvent      *event,
                                 gpointer     data);

typedef void (*GdkSuperWinPaintFunc) (gint x, gint y,
                                      gint width, gint height,
                                      gpointer data);
typedef void (*GdkSuperWinPaintFlushFunc) (gpointer data);

typedef void (*GdkSuperWinKeyPressFunc) (XKeyEvent *event);
typedef void (*GdkSuperWinKeyReleaseFunc) (XKeyEvent *event);

struct _GdkSuperWin
{
  GtkObject object;
  GdkWindow *shell_window;
  GdkWindow *bin_window;

  /* Private */
  GSList                    *translate_queue;
  GdkSuperWinFunc            shell_func;
  GdkSuperWinPaintFunc       paint_func;
  GdkSuperWinPaintFlushFunc  flush_func;
  GdkSuperWinKeyPressFunc    keyprs_func;
  GdkSuperWinKeyReleaseFunc  keyrel_func;
  gpointer                   func_data;
  GDestroyNotify             notify;

  GdkVisibilityState         visibility;
};

struct _GdkSuperWinClass
{
  GtkObjectClass object_class;
};

GTKSUPERWIN_API(GtkType) gdk_superwin_get_type(void);

GTKSUPERWIN_API(GdkSuperWin*) gdk_superwin_new (GdkWindow      *parent_window,
                                                guint           x,
                                                guint           y,
                                                guint           width,
                                                guint           height);

GTKSUPERWIN_API(void)  gdk_superwin_reparent(GdkSuperWin *superwin,
                                             GdkWindow   *parent_window);

GTKSUPERWIN_API(void)  
gdk_superwin_set_event_funcs (GdkSuperWin               *superwin,
                              GdkSuperWinFunc            shell_func,
                              GdkSuperWinPaintFunc       paint_func,
                              GdkSuperWinPaintFlushFunc  flush_func,
                              GdkSuperWinKeyPressFunc    keyprs_func,
                              GdkSuperWinKeyReleaseFunc  keyrel_func,
                              gpointer                   func_data,
                              GDestroyNotify             notify);

GTKSUPERWIN_API(void) gdk_superwin_scroll (GdkSuperWin *superwin,
                                           gint         dx,
                                           gint         dy);
GTKSUPERWIN_API(void) gdk_superwin_resize (GdkSuperWin *superwin,
                                           gint         width,
                                           gint         height);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GDK_SUPERWIN_H__ */

