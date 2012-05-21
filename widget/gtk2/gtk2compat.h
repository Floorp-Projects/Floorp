/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

static inline gboolean
gtk_widget_has_grab(GtkWidget *widget)
{
  return GTK_WIDGET_HAS_GRAB(widget);
}

static inline void
gtk_widget_get_allocation(GtkWidget *widget, GtkAllocation *allocation)
{
  *allocation = widget->allocation;
}

static inline void
gtk_widget_set_allocation(GtkWidget *widget, const GtkAllocation *allocation)
{
  widget->allocation = *allocation;
}

static inline gboolean
gdk_window_is_destroyed(GdkWindow *window)
{
  return GDK_WINDOW_OBJECT(window)->destroyed;
}

static inline void 
gtk_widget_set_can_focus(GtkWidget *widget, gboolean can_focus)
{
  if (can_focus)
    GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
}

static inline void
gtk_widget_set_window(GtkWidget *widget, GdkWindow *window)
{
  widget->window = window;
}
#endif

#if !GTK_CHECK_VERSION(2, 20, 0)
static inline void
gtk_widget_set_mapped(GtkWidget *widget, gboolean mapped)
{
  if (mapped)
    GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
}

static inline gboolean
gtk_widget_get_mapped(GtkWidget *widget)
{
  return GTK_WIDGET_MAPPED (widget);
}

static inline void
gtk_widget_set_realized(GtkWidget *widget, gboolean realized)
{
  if (realized)
    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
  else
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
}

static inline gboolean
gtk_widget_get_realized(GtkWidget *widget)
{
  return GTK_WIDGET_REALIZED (widget);
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

static inline GtkWidget *
gtk_window_group_get_current_grab(GtkWindowGroup *window_group)
{
  if (!window_group->grabs)
    return NULL;

  return GTK_WIDGET(window_group->grabs->data);
}
#endif

#if !GTK_CHECK_VERSION(2, 24, 0)
#ifdef GDK_WINDOW_XDISPLAY
static inline GdkWindow *
gdk_x11_window_lookup_for_display(GdkDisplay *display, Window window)
{
  return gdk_window_lookup_for_display(display, window);
}
#endif
static inline GdkDisplay *
gdk_window_get_display(GdkWindow *window)
{
  return gdk_drawable_get_display(GDK_DRAWABLE(window));
}

static inline GdkScreen*
gdk_window_get_screen (GdkWindow *window)
{
  return gdk_drawable_get_screen (window);
}
#endif

#ifdef GDK_WINDOW_XWINDOW
static inline Window 
gdk_x11_window_get_xid(GdkWindow *window)
{
  return(GDK_WINDOW_XWINDOW(window));
}
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GTK2_COMPAT_H_ */
