#ifndef GTKCOLORSELECTIONDIALOG_WRAPPER_H
#define GTKCOLORSELECTIONDIALOG_WRAPPER_H

#include_next <gtk/gtkselection.h>
#include <gtk/gtkversion.h>

#if !GTK_CHECK_VERSION(2, 14, 0)
static inline GtkWidget*
gtk_color_selection_dialog_get_color_selection(GtkColorSelectionDialog* colorseldialog)
{
  GtkWidget* colorsel;
  g_object_get(colorseldialog, "color-selection", &colorsel, NULL);
  return colorsel;
}
#endif // GTK_CHECK_VERSION

#endif // GTKCOLORSELECTIONDIALOG_WRAPPER_H
