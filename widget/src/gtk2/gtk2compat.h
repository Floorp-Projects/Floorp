/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is Martin Stransky
 * <stransky@redhat.com>.  Portions created by the Initial Developer
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

/**
 * gtk2compat.h: GTK2 compatibility routines
 *
 * gtk2compat provides an API which is not defined in GTK2.
 */

#ifndef _GTK2_COMPAT_H_
#define _GTK2_COMPAT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if !GTK_CHECK_VERSION(2, 14, 0)
static inline GdkWindow*
gtk_widget_get_window(GtkWidget *widget)
{
  return widget->window;
}

static inline const guchar *
gtk_selection_data_get_data(GtkSelectionData *selection_data)
{
  return selection_data->data;
}

static inline gint
gtk_selection_data_get_length(GtkSelectionData *selection_data)
{
  return selection_data->length;
}

static inline GdkAtom
gtk_selection_data_get_target(GtkSelectionData *selection_data)
{
  return selection_data->target;
}

static inline GtkWidget *
gtk_dialog_get_content_area(GtkDialog *dialog)
{
  return dialog->vbox;
}
#endif


#if !GTK_CHECK_VERSION(2, 16, 0)
static inline GdkAtom
gtk_selection_data_get_selection(GtkSelectionData *selection_data)
{
  return selection_data->selection;
}
#endif

#if !GTK_CHECK_VERSION(2, 18, 0)
static inline gboolean
gtk_widget_get_visible(GtkWidget *widget)
{
  return GTK_WIDGET_VISIBLE(widget);
}

static inline gboolean
gtk_widget_has_focus(GtkWidget *widget)
{
  return GTK_WIDGET_HAS_FOCUS(widget);
}

static inline void
gtk_widget_get_allocation(GtkWidget *widget, GtkAllocation *allocation)
{
  *allocation = widget->allocation;
}

static inline gboolean
gdk_window_is_destroyed(GdkWindow *window)
{
  return GDK_WINDOW_OBJECT(window)->destroyed;
}
#endif

#if !GTK_CHECK_VERSION(2, 22, 0)
static inline gint
gdk_visual_get_depth(GdkVisual *visual)
{
  return visual->depth;
}

static inline GdkDragAction
gdk_drag_context_get_actions(GdkDragContext *context)
{
  return context->actions;
}
#endif

#if !GTK_CHECK_VERSION(2, 24, 0)
static inline GdkWindow *
gdk_x11_window_lookup_for_display(GdkDisplay *display, Window window)
{
  return gdk_window_lookup_for_display(display, window);
}

static inline GdkDisplay *
gdk_window_get_display(GdkWindow *window)
{
  return gdk_drawable_get_display(GDK_DRAWABLE(window));
}
#endif

static inline Window 
gdk_x11_window_get_xid(GdkWindow *window)
{
  return(GDK_WINDOW_XWINDOW(window));
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // _GTK2_COMPAT_H_
