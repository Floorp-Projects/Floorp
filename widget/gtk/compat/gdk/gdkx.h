/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GDKX_WRAPPER_H
#define GDKX_WRAPPER_H

#include <gtk/gtkversion.h>

#define gdk_x11_window_foreign_new_for_display \
  gdk_x11_window_foreign_new_for_display_
#define gdk_x11_window_lookup_for_display gdk_x11_window_lookup_for_display_
#define gdk_x11_window_get_xid gdk_x11_window_get_xid_
#if !GTK_CHECK_VERSION(2, 24, 0)
#  define gdk_x11_set_sm_client_id gdk_x11_set_sm_client_id_
#endif
#include_next <gdk/gdkx.h>
#undef gdk_x11_window_foreign_new_for_display
#undef gdk_x11_window_lookup_for_display
#undef gdk_x11_window_get_xid

static inline GdkWindow* gdk_x11_window_foreign_new_for_display(
    GdkDisplay* display, Window window) {
  return gdk_window_foreign_new_for_display(display, window);
}

static inline GdkWindow* gdk_x11_window_lookup_for_display(GdkDisplay* display,
                                                           Window window) {
  return gdk_window_lookup_for_display(display, window);
}

static inline Window gdk_x11_window_get_xid(GdkWindow* window) {
  return (GDK_WINDOW_XWINDOW(window));
}

#ifndef GDK_IS_X11_DISPLAY
#  define GDK_IS_X11_DISPLAY(a) (true)
#endif

#if !GTK_CHECK_VERSION(2, 24, 0)
#  undef gdk_x11_set_sm_client_id
static inline void gdk_x11_set_sm_client_id(const gchar* sm_client_id) {
  gdk_set_sm_client_id(sm_client_id);
}
#endif
#endif /* GDKX_WRAPPER_H */
