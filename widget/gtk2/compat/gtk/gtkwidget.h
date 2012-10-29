/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GTKWIDGET_WRAPPER_H
#define GTKWIDGET_WRAPPER_H

#define gtk_widget_set_mapped gtk_widget_set_mapped_
#define gtk_widget_get_mapped gtk_widget_get_mapped_
#define gtk_widget_set_realized gtk_widget_set_realized_
#define gtk_widget_get_realized gtk_widget_get_realized_
#include_next <gtk/gtkwidget.h>
#undef gtk_widget_set_mapped
#undef gtk_widget_get_mapped
#undef gtk_widget_set_realized
#undef gtk_widget_get_realized

#include <gtk/gtkversion.h>

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

static inline gboolean
gtk_widget_get_has_window(GtkWidget *widget)
{
  return !GTK_WIDGET_NO_WINDOW(widget);
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

static inline void
gtk_widget_set_can_focus(GtkWidget *widget, gboolean can_focus)
{
  if (can_focus)
    GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
}

static inline void
gtk_widget_set_has_window(GtkWidget *widget, gboolean has_window)
{
  if (has_window)
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_NO_WINDOW);
  else
    GTK_WIDGET_SET_FLAGS (widget, GTK_NO_WINDOW);
}

static inline void
gtk_widget_set_window(GtkWidget *widget, GdkWindow *window)
{
  widget->window = window;
}

static inline gboolean
gtk_widget_is_toplevel(GtkWidget *widget)
{
  return GTK_WIDGET_TOPLEVEL(widget);
}
#endif

#if !GTK_CHECK_VERSION(2, 14, 0)
static inline GdkWindow*
gtk_widget_get_window(GtkWidget *widget)
{
  return widget->window;
}
#endif

#endif /* GTKWIDGET_WRAPPER_H */
