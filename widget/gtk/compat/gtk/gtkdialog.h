#ifndef GTKDIALOG_WRAPPER_H
#define GTKDIALOG_WRAPPER_H

#include_next <gtk/gtkdialog.h>
#include <gtk/gtkversion.h>

#if !GTK_CHECK_VERSION(2, 14, 0)
static inline GtkWidget *
gtk_dialog_get_content_area(GtkDialog *dialog)
{
  return dialog->vbox;
}
#endif

#endif /* GTKDIALOG_WRAPPER_H */
