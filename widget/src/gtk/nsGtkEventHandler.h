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

gint nsGtkWidget_KeyPressMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
gint nsGtkWidget_KeyReleaseMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
gint nsGtkWidget_ExposureMask_EventHandler(GtkWidget * w, GdkEventExpose * event, gpointer p);
gint nsGtkWidget_ButtonPressMask_EventHandler(GtkWidget * w, GdkEvent * event, gpointer p);
gint nsGtkWidget_ButtonReleaseMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
gint nsGtkWidget_ButtonMotionMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
gint nsGtkWidget_MotionMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
gint nsGtkWidget_EnterMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
gint nsGtkWidget_LeaveMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
gint nsGtkWidget_Resize_EventHandler(GtkWidget *w, GtkAllocation *allocation, gpointer data);

//----------------------------------------------------

gint nsGtkWidget_FSBCancel_Callback(GtkWidget *w, gpointer p);
gint nsGtkWidget_FSBOk_Callback(GtkWidget *w, gpointer p);

//----------------------------------------------------
gint nsGtkWidget_Focus_Callback(GtkWidget *w, gpointer p);
gint nsGtkWidget_Scrollbar_Callback(GtkWidget *w, gpointer p);

gint CheckButton_Toggle_Callback(GtkWidget *w, gpointer p);

gint nsGtkWidget_RadioButton_ArmCallback(GtkWidget *w, gpointer p);
gint nsGtkWidget_RadioButton_DisArmCallback(GtkWidget *w, gpointer p);

gint nsGtkWidget_Text_Callback(GtkWidget *w, GdkEvent* event, gpointer p);
gint nsGtkWidget_Expose_Callback(GtkWidget *w, gpointer p);

gint nsGtkWidget_Refresh_Callback(gpointer call_data);

gint nsGtkWidget_Menu_Callback(GtkWidget *w, gpointer p);

#endif  // __nsGtkEventHandler.h
