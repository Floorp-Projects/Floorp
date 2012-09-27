#ifndef GTKPLUG_WRAPPER_H
#define GTKPLUG_WRAPPER_H

#include_next <gtk/gtkplug.h>
#include <gtk/gtkversion.h>

#if !GTK_CHECK_VERSION(2, 14, 0)
static inline GdkWindow *
gtk_plug_get_socket_window(GtkPlug *plug)
{
  return plug->socket_window;
}
#endif

#endif /* GTKPLUG_WRAPPER_H */
