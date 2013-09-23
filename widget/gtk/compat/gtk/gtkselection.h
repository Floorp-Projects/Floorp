#ifndef GTKSELECTION_WRAPPER_H
#define GTKSELECTION_WRAPPER_H

#include_next <gtk/gtkselection.h>
#include <gtk/gtkversion.h>

#if !GTK_CHECK_VERSION(2, 16, 0)
static inline GdkAtom
gtk_selection_data_get_selection(GtkSelectionData *selection_data)
{
  return selection_data->selection;
}
#endif

#if !GTK_CHECK_VERSION(2, 14, 0)
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
#endif

#endif /* GTKSELECTION_WRAPPER_H */
