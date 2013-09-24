/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GDKX_WRAPPER_H
#define GDKX_WRAPPER_H

#define gdk_x11_window_lookup_for_display gdk_x11_window_lookup_for_display_
#define gdk_x11_window_get_xid gdk_x11_window_get_xid_
#include_next <gdk/gdkx.h>
#undef gdk_x11_window_lookup_for_display
#undef gdk_x11_window_get_xid

static inline GdkWindow *
gdk_x11_window_lookup_for_display(GdkDisplay *display, Window window)
{
  return gdk_window_lookup_for_display(display, window);
}

static inline Window
gdk_x11_window_get_xid(GdkWindow *window)
{
  return(GDK_WINDOW_XWINDOW(window));
}
#endif /* GDKX_WRAPPER_H */
