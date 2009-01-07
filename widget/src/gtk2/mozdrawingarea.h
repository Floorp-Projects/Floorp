/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
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

#ifndef __MOZ_DRAWINGAREA_H__
#define __MOZ_DRAWINGAREA_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "mozcontainer.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MOZ_DRAWINGAREA_TYPE            (moz_drawingarea_get_type())
#define MOZ_DRAWINGAREA(obj)            (GTK_CHECK_CAST((obj), MOZ_DRAWINGAREA_TYPE, MozDrawingarea))
#define MOZ_DRAWINGAREA_CLASS(klass)    (GTK_CHECK_CLASS_CAST((klass), MOZ_DRAWINGAREA_TYPE, MozDrawingareaClass))
#define IS_MOZ_DRAWINGAREA(obj)         (GTK_CHECK_TYPE((obj), MOZ_DRAWINGAREA_TYPE))
#define IS_MOZ_DRAWINGAREA_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), MOZ_DRAWINGAREA_TYPE))
#define MOZ_DRAWINGAREA_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS((obj), MOZ_DRAWINGAREA_TYPE, MozDrawingareaClass))

#if (GTK_CHECK_VERSION(2, 12, 0) || \
    (GTK_CHECK_VERSION(2, 10, 0) && defined(MOZ_PLATFORM_HILDON)))
#define HAVE_GTK_MOTION_HINTS
#endif

typedef struct _MozDrawingarea      MozDrawingarea;
typedef struct _MozDrawingareaClass MozDrawingareaClass;

struct _MozDrawingarea
{
    GObject         parent_instance;
    GdkWindow      *clip_window;
    GdkWindow      *inner_window;
    MozDrawingarea *parent;
};

struct _MozDrawingareaClass
{
    GObjectClass parent_class;
};

GtkType         moz_drawingarea_get_type       (void);
MozDrawingarea *moz_drawingarea_new            (MozDrawingarea *parent,
                                                MozContainer *widget_parent,
                                                GdkVisual *visual);
void            moz_drawingarea_reparent       (MozDrawingarea *drawingarea,
                                                GdkWindow *aNewParent);
void            moz_drawingarea_move           (MozDrawingarea *drawingarea,
                                                gint x, gint y);
void            moz_drawingarea_resize         (MozDrawingarea *drawingarea,
                                                gint width, gint height);
void            moz_drawingarea_move_resize    (MozDrawingarea *drawingarea,
                                                gint x, gint y,
                                                gint width, gint height);
void            moz_drawingarea_set_visibility (MozDrawingarea *drawingarea,
                                                gboolean visibility);
void            moz_drawingarea_scroll         (MozDrawingarea *drawingarea,
                                                gint x, gint y);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MOZ_DRAWINGAREA_H__ */
