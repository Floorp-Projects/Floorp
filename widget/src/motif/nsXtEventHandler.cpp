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

#include <Xm/Xm.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include "nsXtEventHandler.h"

#include "nsWindow.h"
#include "nsTextWidget.h"
#include "nsCheckButton.h"
#include "nsGUIEvent.h"
#include "nsIMenuItem.h"

#include "stdio.h"

#define DBG 0

struct nsKeyConverter {
  int vkCode; // Platform independent key code
  XID keysym; // X keysym key code
};

struct nsKeyConverter nsKeycodes[] = {
  { NS_VK_CANCEL,     XK_Cancel },
  { NS_VK_BACK,       XK_BackSpace },
  { NS_VK_TAB,        XK_Tab },
#ifdef  XK_ISO_Left_Tab
  { NS_VK_TAB,        XK_ISO_Left_Tab }, // SunOs 5.5.1 doesnt have it
#endif
  { NS_VK_CLEAR,      XK_Clear },
  { NS_VK_RETURN,     XK_Return },
  { NS_VK_SHIFT,      XK_Shift_L },
  { NS_VK_SHIFT,      XK_Shift_R },
  { NS_VK_CONTROL,    XK_Control_L },
  { NS_VK_CONTROL,    XK_Control_R },
  { NS_VK_ALT,        XK_Alt_L },
  { NS_VK_ALT,        XK_Alt_R },
  { NS_VK_PAUSE,      XK_Pause },
  { NS_VK_CAPS_LOCK,  XK_Caps_Lock },
  { NS_VK_ESCAPE,     XK_Escape },
  { NS_VK_SPACE,      XK_space },
  { NS_VK_PAGE_UP,    XK_Page_Up },
  { NS_VK_PAGE_DOWN,  XK_Page_Down },
  { NS_VK_END,        XK_End },
  { NS_VK_HOME,       XK_Home },
  { NS_VK_LEFT,       XK_Left },
  { NS_VK_UP,         XK_Up },
  { NS_VK_RIGHT,      XK_Right },
  { NS_VK_DOWN,       XK_Down },
  { NS_VK_PRINTSCREEN, XK_Print },
  { NS_VK_INSERT,     XK_Insert },
  { NS_VK_DELETE,     XK_Delete },

  { NS_VK_NUMPAD0,    XK_KP_0 },
  { NS_VK_NUMPAD1,    XK_KP_1 },
  { NS_VK_NUMPAD2,    XK_KP_2 },
  { NS_VK_NUMPAD3,    XK_KP_3 },
  { NS_VK_NUMPAD4,    XK_KP_4 },
  { NS_VK_NUMPAD5,    XK_KP_5 },
  { NS_VK_NUMPAD6,    XK_KP_6 },
  { NS_VK_NUMPAD7,    XK_KP_7 },
  { NS_VK_NUMPAD8,    XK_KP_8 },
  { NS_VK_NUMPAD9,    XK_KP_9 },

  { NS_VK_MULTIPLY,   XK_KP_Multiply },
  { NS_VK_ADD,        XK_KP_Add },
  { NS_VK_SEPARATOR,  XK_KP_Separator },
  { NS_VK_SUBTRACT,   XK_KP_Subtract },
  { NS_VK_DECIMAL,    XK_KP_Decimal },
  { NS_VK_DIVIDE,     XK_KP_Divide },
  { NS_VK_F1,         XK_F1 },
  { NS_VK_F2,         XK_F2 },
  { NS_VK_F3,         XK_F3 },
  { NS_VK_F4,         XK_F4 },
  { NS_VK_F5,         XK_F5 },
  { NS_VK_F6,         XK_F6 },
  { NS_VK_F7,         XK_F7 },
  { NS_VK_F8,         XK_F8 },
  { NS_VK_F9,         XK_F9 },
  { NS_VK_F10,        XK_F10 },
  { NS_VK_F11,        XK_F11 },
  { NS_VK_F12,        XK_F12 },
  { NS_VK_F13,        XK_F13 },
  { NS_VK_F14,        XK_F14 },
  { NS_VK_F15,        XK_F15 },
  { NS_VK_F16,        XK_F16 },
  { NS_VK_F17,        XK_F17 },
  { NS_VK_F18,        XK_F18 },
  { NS_VK_F19,        XK_F19 },
  { NS_VK_F20,        XK_F20 },
  { NS_VK_F21,        XK_F21 },
  { NS_VK_F22,        XK_F22 },
  { NS_VK_F23,        XK_F23 },
  { NS_VK_F24,        XK_F24 },

  { NS_VK_COMMA,      XK_comma },
  { NS_VK_PERIOD,     XK_period },
  { NS_VK_SLASH,      XK_slash },
//XXX: How do you get a BACK_QUOTE?  NS_VK_BACK_QUOTE, XK_backquote, 
  { NS_VK_OPEN_BRACKET, XK_bracketleft },
  { NS_VK_CLOSE_BRACKET, XK_bracketright },
  { NS_VK_QUOTE, XK_quotedbl }

};

int nsConvertKey(XID keysym)
{
 int i;
 int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);
 for (i = 0; i < length; i++) {
   if (nsKeycodes[i].keysym == keysym)
     return(nsKeycodes[i].vkCode);
 }

 return((int)keysym);
}


//==============================================================
void nsXtWidget_InitNSEvent(XEvent   * anXEv,
                            XtPointer  p,
                            nsGUIEvent &anEvent,
                            PRUint32   aEventType) 
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWindow *) p;
  anEvent.eventStructType = NS_GUI_EVENT;

  if (anXEv != NULL) {
    anEvent.point.x = anXEv->xbutton.x;
    anEvent.point.y = anXEv->xbutton.y;
  }

  anEvent.time = 0; //XXX: Implement this

}

//==============================================================
void nsXtWidget_InitNSMouseEvent(XEvent   * anXEv,
                                 XtPointer  p,
                                 nsMouseEvent &anEvent,
                                 PRUint32   aEventType) 
{
  // Do base initialization
  nsXtWidget_InitNSEvent(anXEv, p, anEvent, aEventType);

  if (anXEv != NULL) { // Do Mouse Event specific intialization
    anEvent.time       = anXEv->xbutton.time;
    anEvent.isShift    = (anXEv->xbutton.state & ShiftMask) ? PR_TRUE : PR_FALSE;
    anEvent.isControl  = (anXEv->xbutton.state & ControlMask) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt      = (anXEv->xbutton.state & Mod1Mask) ? PR_TRUE : PR_FALSE;
    anEvent.clickCount = 1; //XXX Fix for double-clicks
    anEvent.eventStructType = NS_MOUSE_EVENT;

  }

}

//==============================================================
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define INTERSECTS(r1_x1,r1_x2,r1_y1,r1_y2,r2_x1,r2_x2,r2_y1,r2_y2) \
        !((r2_x2 <= r1_x1) ||\
          (r2_y2 <= r1_y1) ||\
          (r2_x1 >= r1_x2) ||\
          (r2_y1 >= r1_y2))

//==============================================================
typedef struct COLLAPSE_INFO {
    Window win;
    nsRect *r;
} CollapseInfo;

//==============================================================
void nsXtWidget_ExposureMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsWindow * widgetWindow = (nsWindow *) p ;

  nsPaintEvent pevent;
  nsRect       rect;
  nsXtWidget_InitNSEvent(event, p, pevent, NS_PAINT);
  pevent.rect = (nsRect *)&rect;
  XEvent xev;

  rect.x      = event->xexpose.x;
  rect.y      = event->xexpose.y;
  rect.width  = event->xexpose.width;
  rect.height = event->xexpose.height;


  if (widgetWindow->GetResized())
   return;

  if (event->type == NoExpose) {
    return;
  }

  Display* display = XtDisplay(w);
  Window   window = XtWindow(w);

  XSync(display, FALSE);

  while (XCheckTypedWindowEvent(display, window, Expose, &xev) == TRUE) {
      rect.x      = xev.xexpose.x;
      rect.y      = xev.xexpose.y;
      rect.width  = xev.xexpose.width;
      rect.height = xev.xexpose.height;
  }

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
  nsWindow * widgetWindow = (nsWindow *) p;
  nsMouseEvent mevent;
  nsXtWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_MOVE);
  widgetWindow->DispatchMouseEvent(mevent);
}

//==============================================================
void nsXtWidget_MotionMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
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
void nsXtWidget_Focus_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  nsWindow * widgetWindow = (nsWindow *) p ;

  XmAnyCallbackStruct * cbs = (XmAnyCallbackStruct*)call_data;
  nsGUIEvent event;
  nsXtWidget_InitNSEvent(cbs->event, p, event, 
                         cbs->reason == XmCR_FOCUS?NS_GOTFOCUS:NS_LOSTFOCUS);
  widgetWindow->DispatchFocus(event);
}

//==============================================================
void nsXtWidget_Toggle_ArmCallback(Widget w, XtPointer p, XtPointer call_data)
{
  nsCheckButton * checkBtn = (nsCheckButton *) p;

  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  
  if (DBG) fprintf(stderr, "Callback struct 0x%x\n", cbs);fflush(stderr);
  checkBtn->Armed();

}

//==============================================================
void nsXtWidget_Toggle_DisArmCallback(Widget w, XtPointer p, XtPointer call_data)
{
  nsCheckButton * checkBtn = (nsCheckButton *) p;
  checkBtn->DisArmed();
}

//==============================================================
void nsXtWidget_Scrollbar_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  nsScrollbarEvent sevent;
  XmScrollBarCallbackStruct * cbs = (XmScrollBarCallbackStruct*) call_data;
  sevent.widget  = (nsWindow *) p;
  if (cbs->event != nsnull) {
    sevent.point.x = cbs->event->xbutton.x;
    sevent.point.y = cbs->event->xbutton.y;
  } else {
    sevent.point.x = 0;
    sevent.point.y = 0;
  }
  sevent.time    = 0; //XXX Implement this

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
      break;
  }
  widgetWindow->OnScroll(sevent, cbs->value);
}



//==============================================================
void nsXtWidget_Expose_Callback(Widget w, XtPointer p, XtPointer call_data)
{

  nsWindow * widgetWindow = (nsWindow *) p ;
  if (widgetWindow == nsnull) {
    return;
  }

  XmDrawingAreaCallbackStruct * cbs = (XmDrawingAreaCallbackStruct *)call_data;
  XEvent * event = cbs->event;
  nsPaintEvent pevent;
  nsRect       rect;
  nsXtWidget_InitNSEvent(event, p, pevent, NS_PAINT);
  pevent.rect = (nsRect *)&rect;
  widgetWindow->OnPaint(pevent);
}

//==============================================================
void nsXtWidget_Resize_Callback(Widget w, XtPointer p, XtPointer call_data)
{
}

//==============================================================
void nsXtWidget_Text_Callback(Widget w, XtPointer p, XtPointer call_data)
{
  int len;
  XmTextVerifyCallbackStruct *cbs = (XmTextVerifyCallbackStruct *) call_data;
  PasswordData * data;
  XtVaGetValues(w, XmNuserData, &data, NULL);
  if (data == NULL || data->mIgnore) {
    return;
  }

  if (cbs->reason == XmCR_ACTIVATE) {
    char* password = data->mPassword.ToNewCString();
    printf ("Password: %s\n", password);
    delete[] password;
    return;
  }

  if (cbs->startPos < cbs->currInsert) {   /* backspace */
      cbs->endPos = data->mPassword.Length();  /* delete from here to end */
      data->mPassword.SetLength(cbs->startPos); /* backspace--terminate */
      return;
  }

  if (cbs->startPos == cbs->currInsert && cbs->currInsert < data->mPassword.Length()) {
    nsString insStr(cbs->text->ptr);
    data->mPassword.Insert(insStr, cbs->currInsert, strlen(cbs->text->ptr));
  } else if (cbs->startPos == cbs->currInsert && cbs->endPos != cbs->startPos) {
    data->mPassword.SetLength(cbs->startPos);
    printf("Setting Length [%s] at %d\n", cbs->text->ptr, cbs->currInsert);
  } else if (cbs->startPos == cbs->currInsert) {   /* backspace */
    data->mPassword.Append(cbs->text->ptr);
  } 
  
  for (len = 0; len < cbs->text->length; len++)
    cbs->text->ptr[len] = '*';

}

//==============================================================
void nsXtWidget_InitNSKeyEvent(int aEventType, nsKeyEvent& aKeyEvent, Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  Modifiers modout = 0;
  KeySym res;

  nsXtWidget_InitNSEvent(event, p, aKeyEvent, aEventType);
  XKeyEvent* xKeyEvent =  (XKeyEvent*)event; 

   // Get the modout to test for shift + control
  XtTranslateKeycode(xKeyEvent->display,xKeyEvent->keycode, xKeyEvent->state, &modout, &res); 
  res = XKeycodeToKeysym(xKeyEvent->display, xKeyEvent->keycode, 0);

  aKeyEvent.keyCode   = nsConvertKey(res) & 0x00FF;
  aKeyEvent.time      = xKeyEvent->time; 
  aKeyEvent.isShift   = (xKeyEvent->state & ShiftMask) ? PR_TRUE : PR_FALSE; 
  aKeyEvent.isControl = (xKeyEvent->state & ControlMask) ? PR_TRUE : PR_FALSE;
  aKeyEvent.isAlt     = (xKeyEvent->state & Mod1Mask) ? PR_TRUE : PR_FALSE;
}

//==============================================================
void nsXtWidget_KeyPressMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsKeyEvent kevent;
  nsXtWidget_InitNSKeyEvent(NS_KEY_DOWN, kevent, w, p, event, b);
  nsWindow * widgetWindow = (nsWindow *) p ;
  widgetWindow->OnKey(NS_KEY_DOWN, kevent.keyCode, &kevent);
}

//==============================================================
void nsXtWidget_KeyReleaseMask_EventHandler(Widget w, XtPointer p, XEvent * event, Boolean * b)
{
  nsKeyEvent kevent;
  nsXtWidget_InitNSKeyEvent(NS_KEY_UP, kevent, w, p, event, b);
  nsWindow * widgetWindow = (nsWindow *) p ;
  widgetWindow->OnKey(NS_KEY_UP, kevent.keyCode, &kevent);
}

//==============================================================
void nsXtWidget_ResetResize_Callback(XtPointer call_data)
{
    nsWindow* widgetWindow = (nsWindow*)call_data;
    widgetWindow->SetResized(PR_FALSE);
}

//==============================================================
void nsXtWidget_Menu_Callback(Widget w, XtPointer p, XtPointer call_data)
{

  nsIMenuItem * menuItem = (nsIMenuItem *)p;
  if (menuItem != NULL) {
    nsMenuEvent mevent;
    mevent.message = NS_MENU_SELECTED;
    mevent.eventStructType = NS_MENU_EVENT;
    mevent.point.x = 0;
    mevent.point.y = 0;
    menuItem->GetTarget(mevent.widget);
    menuItem->GetCommand(mevent.mCommand);
    mevent.mMenuItem = menuItem;
    mevent.time = 0; //XXX: Implement this
    nsEventStatus status;
    mevent.widget->DispatchEvent((nsGUIEvent *)&mevent, status);
  }
}
