/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GDKWINDOW_WRAPPER_H
#define GDKWINDOW_WRAPPER_H

#define gdk_window_get_display gdk_window_get_display_
#define gdk_window_get_screen gdk_window_get_screen_
#include_next <gdk/gdkwindow.h>
#undef gdk_window_get_display
#undef gdk_window_get_screen

static inline GdkDisplay *
gdk_window_get_display(GdkWindow *window)
{
  return gdk_drawable_get_display(GDK_DRAWABLE(window));
}

static inline GdkScreen*
gdk_window_get_screen(GdkWindow *window)
{
  return gdk_drawable_get_screen (window);
}
#endif /* GDKWINDOW_WRAPPER_H */
