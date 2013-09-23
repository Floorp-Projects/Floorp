/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GTKWINDOW_WRAPPER_H
#define GTKWINDOW_WRAPPER_H

#define gtk_window_group_get_current_grab gtk_window_group_get_current_grab_
#include_next <gtk/gtkwindow.h>
#undef gtk_window_group_get_current_grab

static inline GtkWidget *
gtk_window_group_get_current_grab(GtkWindowGroup *window_group)
{
  if (!window_group->grabs)
    return NULL;

  return GTK_WIDGET(window_group->grabs->data);
}
#endif /* GTKWINDOW_WRAPPER_H */
