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

#include "mozdrawingarea.h"

/* init methods */
static void moz_drawingarea_class_init          (MozDrawingareaClass *klass);
static void moz_drawingarea_init                (MozDrawingarea *drawingarea);

/* static methods */
static void moz_drawingarea_create_windows      (MozDrawingarea *drawingarea,
                                                 GdkWindow *parent,
                                                 GtkWidget *widget,
                                                 GdkVisual *visual);

static void moz_drawingarea_finalize            (GObject *object);

static GObjectClass *parent_class = NULL;

GtkType
moz_drawingarea_get_type(void)
{
    static GtkType moz_drawingarea_type = 0;

   if (!moz_drawingarea_type) {
       static GTypeInfo moz_drawingarea_info = {
           sizeof(MozDrawingareaClass), /* class size */
           NULL, /* base_init */
           NULL, /* base_finalize */
           (GClassInitFunc) moz_drawingarea_class_init, /* class_init */
           NULL, /* class_destroy */
           NULL, /* class_data */
           sizeof(MozDrawingarea), /* instance_size */
           0, /* n_preallocs */
           (GInstanceInitFunc) moz_drawingarea_init, /* instance_init */
           NULL, /* value_table */
       };
       moz_drawingarea_type =
           g_type_register_static (G_TYPE_OBJECT,
                                   "MozDrawingarea",
                                   &moz_drawingarea_info, 0);
   }

   return moz_drawingarea_type;
}

MozDrawingarea *
moz_drawingarea_new (MozDrawingarea *parent, MozContainer *widget_parent,
                     GdkVisual *visual)
{
    MozDrawingarea *drawingarea;

    drawingarea = g_object_new(MOZ_DRAWINGAREA_TYPE, NULL);

    drawingarea->parent = parent;

    if (!parent)
        moz_drawingarea_create_windows(drawingarea,
                                       GTK_WIDGET(widget_parent)->window,
                                       GTK_WIDGET(widget_parent),
                                       visual);
    else
        moz_drawingarea_create_windows(drawingarea,
                                       parent->inner_window, 
                                       GTK_WIDGET(widget_parent),
                                       visual);

    return drawingarea;
}

void
moz_drawingarea_class_init (MozDrawingareaClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = moz_drawingarea_finalize;

    parent_class = g_type_class_peek_parent(klass);
}

void
moz_drawingarea_init (MozDrawingarea *drawingarea)
{

}

void
moz_drawingarea_reparent (MozDrawingarea *drawingarea, GdkWindow *aNewParent)
{
    gdk_window_reparent(drawingarea->clip_window,
                        aNewParent, 0, 0);
}

void
moz_drawingarea_create_windows (MozDrawingarea *drawingarea, GdkWindow *parent,
                                GtkWidget *widget, GdkVisual *visual)
{
    GdkWindowAttr attributes;
    gint          attributes_mask = 0;

    /* create the clipping window */
    attributes.event_mask = 0;
    attributes.x = 0;
    attributes.y = 0;
    attributes.width = 1;
    attributes.height = 1;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    if (!visual) {
        attributes.visual = gtk_widget_get_visual (widget);
        attributes.colormap = gtk_widget_get_colormap (widget);
    } else {
        attributes.visual = visual;
        attributes.colormap = gdk_colormap_new(visual, 0);
    }

    attributes_mask |= GDK_WA_VISUAL | GDK_WA_COLORMAP |
        GDK_WA_X | GDK_WA_Y;

    drawingarea->clip_window = gdk_window_new (parent, &attributes,
                                               attributes_mask);
    gdk_window_set_user_data(drawingarea->clip_window, widget);

    /* set the default pixmap to None so that you don't end up with the
       gtk default which is BlackPixel. */
    gdk_window_set_back_pixmap(drawingarea->clip_window, NULL, FALSE);

    attributes.event_mask = (GDK_EXPOSURE_MASK | GDK_STRUCTURE_MASK |
                             GDK_VISIBILITY_NOTIFY_MASK |
                             GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                             GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK);
    /* create the inner window */
    drawingarea->inner_window = gdk_window_new (drawingarea->clip_window,
                                                &attributes, attributes_mask);
    gdk_window_set_user_data(drawingarea->inner_window, widget);

    /* set the default pixmap to None so that you don't end up with the
       gtk default which is BlackPixel. */
    gdk_window_set_back_pixmap(drawingarea->inner_window, NULL, FALSE);

    if (visual) {
        g_object_unref(attributes.colormap);
    }
}

void
moz_drawingarea_finalize (GObject *object)
{
    MozDrawingarea *drawingarea;

    g_return_if_fail(IS_MOZ_DRAWINGAREA(object));

    drawingarea = MOZ_DRAWINGAREA(object);

    gdk_window_set_user_data(drawingarea->inner_window, NULL);
    gdk_window_destroy(drawingarea->inner_window);
    gdk_window_set_user_data(drawingarea->clip_window, NULL);
    gdk_window_destroy(drawingarea->clip_window);

    (* parent_class->finalize) (object);
}

void
moz_drawingarea_move (MozDrawingarea *drawingarea,
                      gint x, gint y)
{
    gdk_window_move(drawingarea->clip_window, x, y);
}

void
moz_drawingarea_resize (MozDrawingarea *drawingarea,
                        gint width, gint height)
{
    gdk_window_resize(drawingarea->clip_window, width, height);
    gdk_window_resize(drawingarea->inner_window, width, height);
}

void
moz_drawingarea_move_resize (MozDrawingarea *drawingarea,
                             gint x, gint y, gint width, gint height)
{
    gdk_window_resize(drawingarea->inner_window, width, height);
    gdk_window_move_resize(drawingarea->clip_window, x, y, width, height);
}

void
moz_drawingarea_set_visibility (MozDrawingarea *drawingarea,
                                gboolean visibility)
{
    if (visibility) {
        gdk_window_show_unraised(drawingarea->inner_window);
        gdk_window_show_unraised(drawingarea->clip_window);
    }
    else    {
        gdk_window_hide(drawingarea->clip_window);
        gdk_window_hide(drawingarea->inner_window);
    }
}

void
moz_drawingarea_scroll (MozDrawingarea *drawingarea,
                        gint x, gint y)
{
    gdk_window_scroll(drawingarea->inner_window, x, y);
}
