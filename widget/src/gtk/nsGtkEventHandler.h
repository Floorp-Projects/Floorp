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

#ifndef __nsGtkEventHandler_h
#define __nsGtkEventHandler_h

#include <gtk/gtk.h>

class nsIWidget;
class nsIMenuItem;

gint handle_configure_event(GtkWidget *w, GdkEventConfigure *conf, gpointer p);
void handle_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer p);
gint handle_expose_event(GtkWidget *w, GdkEventExpose *event, gpointer p);
gint handle_button_press_event(GtkWidget *w, GdkEventButton * event, gpointer p);
gint handle_button_release_event(GtkWidget *w, GdkEventButton * event, gpointer p);
gint handle_key_release_event(GtkWidget *w, GdkEventKey* event, gpointer p);
gint handle_key_press_event(GtkWidget *w, GdkEventKey* event, gpointer p);

gint handle_focus_in_event(GtkWidget *w, GdkEventFocus * event, gpointer p);
gint handle_focus_out_event(GtkWidget *w, GdkEventFocus * event, gpointer p);

void handle_scrollbar_value_changed(GtkAdjustment *adjustment, gpointer p);

void menu_item_activate_handler(GtkWidget *w, gpointer p);

//----------------------------------------------------

gint nsGtkWidget_FSBCancel_Callback(GtkWidget *w, gpointer p);
gint nsGtkWidget_FSBOk_Callback(GtkWidget *w, gpointer p);

//----------------------------------------------------
gint nsGtkWidget_Focus_Callback(GtkWidget *w, gpointer p);



gint CheckButton_Toggle_Callback(GtkWidget *w, gpointer p);

gint nsGtkWidget_RadioButton_ArmCallback(GtkWidget *w, gpointer p);
gint nsGtkWidget_RadioButton_DisArmCallback(GtkWidget *w, gpointer p);

gint nsGtkWidget_Text_Callback(GtkWidget *w, GdkEventKey* event, gpointer p);
gint nsGtkWidget_Expose_Callback(GtkWidget *w, gpointer p);

gint nsGtkWidget_Refresh_Callback(gpointer call_data);


#endif  // __nsGtkEventHandler.h
