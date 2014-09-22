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

#endif /* GTKWIDGET_WRAPPER_H */
