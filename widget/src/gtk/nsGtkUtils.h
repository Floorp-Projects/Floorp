/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __nsGtkUtils_h
#define __nsGtkUtils_h

#include <gtk/gtk.h>

struct nsGtkUtils
{
  //
  // Wrapper for XQueryPointer
  //
#if 0
  static gint gdk_query_pointer(GdkWindow * window,
                                gint *      x_out,
                                gint *      y_out);
#endif

  //
  // Change a widget's background
  //
  // flags isa bit mask of the following bits:
  //
  //   GTK_RC_FG
  //   GTK_RC_BG
  //   GTK_RC_TEXT
  //   GTK_RC_BASE
  //
  // state is an enum:
  //
  //   GTK_STATE_NORMAL,
  //   GTK_STATE_ACTIVE,
  //   GTK_STATE_PRELIGHT,
  //   GTK_STATE_SELECTED,
  //   GTK_STATE_INSENSITIVE
  //
  static void gtk_widget_set_color(GtkWidget *  widget,
                                   GtkRcFlags   flags,
                                   GtkStateType state,
                                   GdkColor *   color);

  /**
   * Return the current keyboard modifier state.
   *
   * @return the current keyboard modifier state.
   *
   */
  static GdkModifierType gdk_keyboard_get_modifiers();

  /**
   * Flash an area within a GDK window (or the whole window)
   *
   * @param aGdkWindow   The GDK window to flash.
   * @param aTimes       Number of times to flash the area.
   * @param aInterval    Interval between flashes in milliseconds.
   * @param aArea        The area to flash.  The whole window if NULL.
   *
   */
  static void gdk_window_flash(GdkWindow *     aGdkWindow,
                               unsigned int    aTimes,
                               unsigned long   aInterval,
                               GdkRectangle *  aArea);
};

#endif  // __nsGtkEventHandler.h
