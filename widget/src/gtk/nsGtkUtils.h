/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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

  static void gdk_window_flash(GdkWindow * window,
                               unsigned int  times,  /* Number of times to flash */
                               unsigned long interval); /* Interval between flashes */
};

#endif  // __nsGtkEventHandler.h
