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

#ifndef __nsXtEventHandler_h      
#define __nsXtEventHandler_h

#include <gtk/gtk.h>
#include <gdk/gdk.h>

class nsIWidget;
class nsIMenuItem;

void nsGtkWidget_KeyPressMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
void nsGtkWidget_KeyReleaseMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
void nsGtkWidget_ExposureMask_EventHandler(GtkWidget * w, GdkEventExpose * event, gpointer p);
void nsGtkWidget_ButtonPressMask_EventHandler(GtkWidget * w, GdkEvent * event, gpointer p);
void nsGtkWidget_ButtonReleaseMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
void nsGtkWidget_ButtonMotionMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
void nsGtkWidget_MotionMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
void nsGtkWidget_EnterMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
void nsGtkWidget_LeaveMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p);
//----------------------------------------------------

void nsGtkWidget_FSBCancel_Callback(GtkWidget *w, gpointer p);
void nsGtkWidget_FSBOk_Callback(GtkWidget *w, gpointer p);

//----------------------------------------------------
void nsGtkWidget_Focus_Callback(GtkWidget *w, gpointer p);
void nsGtkWidget_Scrollbar_Callback(GtkWidget *w, gpointer p);

void CheckButton_Toggle_Callback(GtkWidget *w, gpointer p);

void nsGtkWidget_RadioButton_ArmCallback(GtkWidget *w, gpointer p);
void nsGtkWidget_RadioButton_DisArmCallback(GtkWidget *w, gpointer p);

void nsGtkWidget_Text_Callback(GtkWidget *w, gpointer p);
void nsGtkWidget_Resize_Callback(GtkWidget *w, gpointer p);
void nsGtkWidget_Expose_Callback(GtkWidget *w, gpointer p);

void nsGtkWidget_Refresh_Callback(gpointer call_data);

void nsGtkWidget_ResetResize_Callback(GtkWidget *w, gpointer p);

void nsGtkWidget_Menu_Callback(GtkWidget *w, gpointer p);

#endif  // __nsXtEventHandler.h





