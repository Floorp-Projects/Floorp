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

#ifndef __nsGtkEventHandler_h
#define __nsGtkEventHandler_h

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "gdksuperwin.h"

class nsIWidget;

gboolean handle_configure_event(GtkWidget *w, GdkEventConfigure *conf, gpointer p);
void handle_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer p);
gint handle_expose_event(GtkWidget *w, GdkEventExpose *event, gpointer p);

gint handle_key_release_event_for_text(GtkObject *w, GdkEventKey* event, gpointer p);
gint handle_key_press_event_for_text(GtkObject *w, GdkEventKey* event, gpointer p);

gint handle_key_release_event(GtkObject *w, GdkEventKey* event, gpointer p);
gint handle_key_press_event(GtkObject *w, GdkEventKey* event, gpointer p);

void handle_scrollbar_value_changed(GtkAdjustment *adjustment, gpointer p);

//----------------------------------------------------

void handle_xlib_shell_event(GdkSuperWin *superwin, XEvent *event, gpointer p);
void handle_superwin_paint(gint aX, gint aY,
                           gint aWidth, gint aHeight, gpointer aData);
void handle_superwin_flush(gpointer aData);
void handle_gdk_event (GdkEvent *event, gpointer data);

#endif  // __nsGtkEventHandler.h
