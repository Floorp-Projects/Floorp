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

#include "Xm/Xm.h"
#include "nsXtEventHandler.h"

#include "nsWindow.h"
#include "nsGUIEvent.h"

#include "stdio.h"


void nsXtWidget_InitNSEvent(XEvent   * anXEv,
                            XtPointer  p,
                            nsGUIEvent &anEvent,
                            PRUint32   aEventType) 
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWindow *) p;

  anEvent.point.x = anXEv->xbutton.x;
  anEvent.point.y = anXEv->xbutton.y;

  anEvent.time    = 0; //TBD

}

void nsXtWidget_InitNSMouseEvent(XEvent   * anXEv,
                                 XtPointer  p,
                                 nsMouseEvent &anEvent,
                                 PRUint32   aEventType) 
{
  // Do base initialization
  nsXtWidget_InitNSEvent(anXEv, p, anEvent, aEventType);

  // Do Mouse Event specific intialization
  anEvent.time      = anXEv->xbutton.time;
  anEvent.isShift   = anXEv->xbutton.state | ShiftMask;
  anEvent.isControl = anXEv->xbutton.state | ControlMask;

  //anEvent.isAlt      = GetKeyState(VK_LMENU) < 0    || GetKeyState(VK_RMENU) < 0;
  ////anEvent.clickCount = (aEventType == NS_MOUSE_LEFT_DOUBLECLICK ||
                      //aEventType == NS_MOUSE_LEFT_DOUBLECLICK)? 2:1;

}

void nsXtWidget_ExposureMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsPaintEvent pevent ;
  nsWindow * widgetWindow = (nsWindow *) p ;

  if (event->xexpose.count != 0)
    return ;

  nsXtWidget_InitNSEvent(event, p, pevent, NS_PAINT);

  widgetWindow->OnPaint(pevent);

}

//==============================================================
void nsXtWidget_ButtonPressMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_LEFT_BUTTON_DOWN);
  widgetWindow->DispatchMouseEvent(mevent);
}

//==============================================================
void nsXtWidget_ButtonReleaseMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_LEFT_BUTTON_UP);
  widgetWindow->DispatchMouseEvent(mevent);
}

//==============================================================
void nsXtWidget_ButtonMotionMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  //fprintf(stderr, "nsXtWidget_ButtonMotionMask_EventHandler\n");
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_LEFT_BUTTON_UP);
  widgetWindow->DispatchMouseEvent(mevent);
}

//==============================================================
void nsXtWidget_MotionMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  //fprintf(stderr, "nsXtWidget_MotionMask_EventHandler\n");
  nsGUIEvent mevent;
  nsXtWidget_InitNSEvent(event, p, mevent, NS_MOUSE_MOVE);
  widgetWindow->DispatchEvent(&mevent);
}

//==============================================================
void nsXtWidget_EnterMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  fprintf(stderr, "***************** nsXtWidget_EnterMask_EventHandler\n");
  nsGUIEvent mevent;
  nsXtWidget_InitNSEvent(event, p, mevent, NS_MOUSE_ENTER);
  widgetWindow->DispatchEvent(&mevent);
}

//==============================================================
void nsXtWidget_LeaveMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  fprintf(stderr, "***************** nsXtWidget_LeaveMask_EventHandler\n");
  nsGUIEvent mevent;
  nsXtWidget_InitNSEvent(event, p, mevent, NS_MOUSE_EXIT);
  widgetWindow->DispatchEvent(&mevent);
}



