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
#include "nsCheckButton.h"
#include "nsFileWidget.h"
#include "nsGUIEvent.h"

#include "stdio.h"

#define DBG 0

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
  if (DBG) fprintf(stderr, "In nsXtWidget_ExposureMask_EventHandler\n");
  nsPaintEvent pevent ;
  nsWindow * widgetWindow = (nsWindow *) p ;

  if (event->xexpose.count != 0)
    return ;

  nsXtWidget_InitNSEvent(event, p, pevent, NS_PAINT);

  widgetWindow->OnPaint(pevent);

  if (DBG) fprintf(stderr, "Out nsXtWidget_ExposureMask_EventHandler\n");
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
  //if (DBG) fprintf(stderr, "nsXtWidget_ButtonMotionMask_EventHandler\n");
  nsPaintEvent pevent ;
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_MOVE);
  widgetWindow->DispatchMouseEvent(mevent);

}

//==============================================================
void nsXtWidget_MotionMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  //if (DBG) fprintf(stderr, "nsXtWidget_ButtonMotionMask_EventHandler\n");
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_MOVE);
  widgetWindow->DispatchMouseEvent(mevent);

}

//==============================================================
void nsXtWidget_EnterMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_ENTER);
  widgetWindow->DispatchMouseEvent(mevent);
}

//==============================================================
void nsXtWidget_LeaveMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  if (DBG) fprintf(stderr, "***************** nsXtWidget_LeaveMask_EventHandler\n");
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_EXIT);
  widgetWindow->DispatchMouseEvent(mevent);
}

//==============================================================
void nsXtWidget_Toggle_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  if (DBG) fprintf(stderr, "***************** nsXtWidget_Scrollbar_Callback\n");

  nsScrollbarEvent sevent;

  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  
  if (DBG) fprintf(stderr, "Callback struct 0x%x\n", cbs);fflush(stderr);

  /*nsGUIEvent event;
  nsXtWidget_InitNSEvent(event, p, event, NS_MOUSE_MOVE);
  widgetWindow->DispatchMouseEvent(mevent);
  */
}

//==============================================================
void nsXtWidget_Toggle_ArmCallback(Widget w, XtPointer p, XtPointer call_data)
{
  nsCheckButton * checkBtn = (nsCheckButton *) p ;

  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  
  if (DBG) fprintf(stderr, "Callback struct 0x%x\n", cbs);fflush(stderr);
  checkBtn->Armed();

}

//==============================================================
void nsXtWidget_Toggle_DisArmCallback(Widget w, XtPointer p, XtPointer call_data)
{
  if (DBG) fprintf(stderr, "In ***************** nsXtWidget_Scrollbar_Callback\n");
  nsCheckButton * checkBtn = (nsCheckButton *) p ;

  nsScrollbarEvent sevent;

  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  
  if (DBG) fprintf(stderr, "Callback struct 0x%x\n", cbs);fflush(stderr);

  checkBtn->DisArmed();
  if (DBG) fprintf(stderr, "Out ***************** nsXtWidget_Scrollbar_Callback\n");
  
}

//==============================================================
void nsXtWidget_Scrollbar_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  if (DBG) fprintf(stderr, "***************** nsXtWidget_Scrollbar_Callback\n");

  nsScrollbarEvent sevent;

  XmScrollBarCallbackStruct * cbs = (XmScrollBarCallbackStruct*) call_data;
  if (DBG) fprintf(stderr, "Callback struct 0x%x\n", cbs);fflush(stderr);

  sevent.widget  = (nsWindow *) p;
  if (cbs->event != nsnull) {
    sevent.point.x = cbs->event->xbutton.x;
    sevent.point.y = cbs->event->xbutton.y;
  } else {
    sevent.point.x = 0;
    sevent.point.y = 0;
  }
  sevent.time    = 0; //TBD

  switch (cbs->reason) {

    case XmCR_INCREMENT:
      sevent.message = NS_SCROLLBAR_LINE_NEXT;
      break;

    case XmCR_DECREMENT:
      sevent.message = NS_SCROLLBAR_LINE_PREV;
      break;

    case XmCR_PAGE_INCREMENT:
      sevent.message = NS_SCROLLBAR_PAGE_NEXT;
      break;

    case XmCR_PAGE_DECREMENT:
      sevent.message = NS_SCROLLBAR_PAGE_PREV;
      break;

    case XmCR_DRAG:
      sevent.message = NS_SCROLLBAR_POS;
      break;

    case XmCR_VALUE_CHANGED:
      sevent.message = NS_SCROLLBAR_POS;
      break;

    default:
      if (DBG) fprintf(stderr, "In Default processing for scrollbar reason is [%d]\n", cbs->reason);
      break;
  }
  widgetWindow->OnScroll(sevent, cbs->value);
}



//==============================================================
void nsXtWidget_Resize_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  //if (DBG) 
fprintf(stderr, "In nsXtWidget_Resize_Callback 0x%x\n", p);

  nsWindow * widgetWindow = (nsWindow *) p ;
  if (widgetWindow == nsnull) {
    return;
  }

  XmDrawingAreaCallbackStruct * cbs = (XmDrawingAreaCallbackStruct *)call_data;
  if (cbs->reason == XmCR_RESIZE) {
    nsSizeEvent event;
    nsRect rect;
    event.message = NS_SIZE;
    event.widget  = (nsWindow *) p;
    if (cbs->event != NULL) {
      event.point.x = cbs->event->xbutton.x;
      event.point.y = cbs->event->xbutton.y;
    }
    event.time    = 0; //TBD
    event.windowSize = (nsRect *)&rect;
    widgetWindow->GetBounds(rect);
    widgetWindow->OnResize(event);
  }
}

//==============================================================
void nsXtWidget_Text_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  if (DBG) fprintf(stderr, "In nsXtWidget_Text_Callback\n");
  nsWindow * widgetWindow = (nsWindow *) p ;
  static char * passwd;
  char * newStr;
  int len;

  XmTextVerifyCallbackStruct *cbs = (XmTextVerifyCallbackStruct *) call_data;

  if (cbs->reason == XmCR_ACTIVATE) {
      //printf ("Password: %s\n", passwd);
      return;
  }

  if (cbs->startPos < cbs->currInsert) {   /* backspace */
      cbs->endPos = strlen (passwd);       /* delete from here to end */
      passwd[cbs->startPos] = 0;           /* backspace--terminate */
      return;
  }

  if (cbs->text->length > 1) {
      cbs->doit = False;  /* don't allow "paste" operations */
      return;             /* make the user *type* the password! */
  }

  newStr = XtMalloc (cbs->endPos + 2); /* new char + NULL terminator */
  if (passwd) {
      strcpy (newStr, passwd);
      XtFree (passwd);
  } else
      newStr[0] = 0;
  passwd = newStr;
  strncat (passwd, cbs->text->ptr, cbs->text->length);
  passwd[cbs->endPos + cbs->text->length] = 0;

  for (len = 0; len < cbs->text->length; len++)
      cbs->text->ptr[len] = '*';
  if (DBG) fprintf(stderr, "Out nsXtWidget_Text_Callback\n");
}

//==============================================================
void nsXtWidget_FSBCancel_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  nsFileWidget * widgetWindow = (nsFileWidget *) p ;
  if (p != nsnull) {
    printf("OnCancel\n");
    widgetWindow->OnCancel();
  }
}

//==============================================================
void nsXtWidget_FSBOk_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  nsFileWidget * widgetWindow = (nsFileWidget *) p ;
  if (p != nsnull) {
    widgetWindow->OnOk();
  }
}
