/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsXtEventHandler_h      
#define __nsXtEventHandler_h

#include <Xm/Xm.h>

class nsIWidget;
class nsIMenuItem;

void nsXtWidget_KeyPressMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b);
void nsXtWidget_KeyReleaseMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b);
void nsXtWidget_ExposureMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b);
void nsXtWidget_ButtonPressMask_EventHandler(Widget w,XtPointer p, XEvent * event, Boolean * b);
void nsXtWidget_ButtonReleaseMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b);
void nsXtWidget_ButtonMotionMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b);
void nsXtWidget_MotionMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b);
void nsXtWidget_EnterMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b);
void nsXtWidget_LeaveMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b);
//----------------------------------------------------

void nsXtWidget_FSBCancel_Callback(Widget w, XtPointer p, XtPointer call_data);
void nsXtWidget_FSBOk_Callback(Widget w, XtPointer p, XtPointer call_data);

//----------------------------------------------------
void nsXtWidget_Focus_Callback(Widget w, XtPointer p, XtPointer call_data);
void nsXtWidget_Scrollbar_Callback(Widget w, XtPointer p, XtPointer call_data);
void nsXtWidget_Toggle_ArmCallback(Widget w, XtPointer p, XtPointer call_data);
void nsXtWidget_Toggle_DisArmCallback(Widget w, XtPointer p, XtPointer call_data);
void nsXtWidget_RadioButton_ArmCallback(Widget w, XtPointer p, XtPointer call_data);
void nsXtWidget_RadioButton_DisArmCallback(Widget w, XtPointer p, XtPointer call_data);

void nsXtWidget_Text_Callback(Widget w, XtPointer p, XtPointer call_data);
void nsXtWidget_Resize_Callback(Widget w, XtPointer p, XtPointer call_data);
void nsXtWidget_Expose_Callback(Widget w, XtPointer p, XtPointer call_data);

void nsXtWidget_Refresh_Callback(XtPointer call_data);

void nsXtWidget_ResetResize_Callback(XtPointer call_data);

void nsXtWidget_Menu_Callback(Widget w, XtPointer p, XtPointer call_data);

#endif  // __nsXtEventHandler.h
