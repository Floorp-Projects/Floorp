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

#include "gtk/gtk.h"
#include "nsGtkEventHandler.h"

#include "nsWindow.h"
#include "nsTextWidget.h"
#include "nsCheckButton.h"
#include "nsRadioButton.h"
#include "nsScrollbar.h"
#include "nsFileWidget.h"
#include "nsGUIEvent.h"
#include "nsIMenuItem.h"

#include "stdio.h"

#define DBG 0

#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

struct nsKeyConverter {
  int vkCode; // Platform independent key code
  int keysym; // GDK keysym key code
};

struct nsKeyConverter nsKeycodes[] = {
  NS_VK_CANCEL,     GDK_Cancel,
  NS_VK_BACK,       GDK_BackSpace,
  NS_VK_TAB,        GDK_Tab,
  NS_VK_CLEAR,      GDK_Clear,
  NS_VK_RETURN,     GDK_Return,
  NS_VK_SHIFT,      GDK_Shift_L,
  NS_VK_SHIFT,      GDK_Shift_R,
  NS_VK_CONTROL,    GDK_Control_L,
  NS_VK_CONTROL,    GDK_Control_R,
  NS_VK_ALT,        GDK_Alt_L,
  NS_VK_ALT,        GDK_Alt_R,
  NS_VK_PAUSE,      GDK_Pause,
  NS_VK_CAPS_LOCK,  GDK_Caps_Lock,
  NS_VK_ESCAPE,     GDK_Escape,
  NS_VK_SPACE,      GDK_space,
  NS_VK_PAGE_UP,    GDK_Page_Up,
  NS_VK_PAGE_DOWN,  GDK_Page_Down,
  NS_VK_END,        GDK_End,
  NS_VK_HOME,       GDK_Home,
  NS_VK_LEFT,       GDK_Left,
  NS_VK_UP,         GDK_Up,
  NS_VK_RIGHT,      GDK_Right,
  NS_VK_DOWN,       GDK_Down,
  NS_VK_PRINTSCREEN, GDK_Print,
  NS_VK_INSERT,     GDK_Insert,
  NS_VK_DELETE,     GDK_Delete,

  NS_VK_NUMPAD0,    GDK_KP_0,
  NS_VK_NUMPAD1,    GDK_KP_1,
  NS_VK_NUMPAD2,    GDK_KP_2,
  NS_VK_NUMPAD3,    GDK_KP_3,
  NS_VK_NUMPAD4,    GDK_KP_4,
  NS_VK_NUMPAD5,    GDK_KP_5,
  NS_VK_NUMPAD6,    GDK_KP_6,
  NS_VK_NUMPAD7,    GDK_KP_7,
  NS_VK_NUMPAD8,    GDK_KP_8,
  NS_VK_NUMPAD9,    GDK_KP_9,

  NS_VK_MULTIPLY,   GDK_KP_Multiply,
  NS_VK_ADD,        GDK_KP_Add,
  NS_VK_SEPARATOR,  GDK_KP_Separator,
  NS_VK_SUBTRACT,   GDK_KP_Subtract,
  NS_VK_DECIMAL,    GDK_KP_Decimal,
  NS_VK_DIVIDE,     GDK_KP_Divide,
  NS_VK_F1,         GDK_F1,
  NS_VK_F2,         GDK_F2,
  NS_VK_F3,         GDK_F3,
  NS_VK_F4,         GDK_F4,
  NS_VK_F5,         GDK_F5,
  NS_VK_F6,         GDK_F6,
  NS_VK_F7,         GDK_F7,
  NS_VK_F8,         GDK_F8,
  NS_VK_F9,         GDK_F9,
  NS_VK_F10,        GDK_F10,
  NS_VK_F11,        GDK_F11,
  NS_VK_F12,        GDK_F12,
  NS_VK_F13,        GDK_F13,
  NS_VK_F14,        GDK_F14,
  NS_VK_F15,        GDK_F15,
  NS_VK_F16,        GDK_F16,
  NS_VK_F17,        GDK_F17,
  NS_VK_F18,        GDK_F18,
  NS_VK_F19,        GDK_F19,
  NS_VK_F20,        GDK_F20,
  NS_VK_F21,        GDK_F21,
  NS_VK_F22,        GDK_F22,
  NS_VK_F23,        GDK_F23,
  NS_VK_F24,        GDK_F24,

  NS_VK_COMMA,      GDK_comma,
  NS_VK_PERIOD,     GDK_period,
  NS_VK_SLASH,      GDK_slash,
//XXX: How do you get a BACK_QUOTE?  NS_VK_BACK_QUOTE, GDK_backquote,
  NS_VK_OPEN_BRACKET, GDK_bracketleft,
  NS_VK_CLOSE_BRACKET, GDK_bracketright,
  NS_VK_QUOTE, GDK_quotedbl

};

void nsGtkWidget_InitNSKeyEvent(int aEventType, nsKeyEvent& aKeyEvent,
                                GtkWidget *w, gpointer p, GdkEventKey * event);

int nsConvertKey(int keysym)
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
void InitAllocationEvent(GtkAllocation *aAlloc,
                            gpointer   p,
                            nsSizeEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;
  NS_ADDREF(anEvent.widget);

  anEvent.eventStructType = NS_SIZE_EVENT;

  if (aAlloc != NULL) {
    nsRect *foo = new nsRect(aAlloc->x, aAlloc->y, aAlloc->width, aAlloc->height);
    anEvent.windowSize = foo;
    anEvent.point.x = aAlloc->x;
    anEvent.point.y = aAlloc->y;
    anEvent.mWinWidth = aAlloc->width;
    anEvent.mWinHeight = aAlloc->height;
  }
// this usually returns 0
  anEvent.time = 0;
}

//==============================================================
void InitMouseEvent(GdkEventButton *aGEB,
                            gpointer   p,
                            nsMouseEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;
  NS_ADDREF(anEvent.widget);

  anEvent.eventStructType = NS_MOUSE_EVENT;

  if (aGEB != NULL) {
    anEvent.point.x = nscoord(aGEB->x);
    anEvent.point.y = nscoord(aGEB->y);

    anEvent.isShift = (aGEB->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGEB->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGEB->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.time = aGEB->time;

    switch(aGEB->type)
    {
      case GDK_BUTTON_PRESS:
        anEvent.clickCount = 1;
	break;
      case GDK_2BUTTON_PRESS:
        anEvent.clickCount = 2;
	break;
      default:
        anEvent.clickCount = 1;
    }

  }
}

//==============================================================
void InitExposeEvent(GdkEventExpose *aGEE,
                            gpointer   p,
                            nsPaintEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;
  NS_ADDREF(anEvent.widget);

  anEvent.eventStructType = NS_PAINT_EVENT;

  if (aGEE != NULL) {
    nsRect rect(aGEE->area.x, aGEE->area.y, aGEE->area.width, aGEE->area.height);
    anEvent.rect = &rect;
    anEvent.time = gdk_event_get_time((GdkEvent*)aGEE);
  }
}

//==============================================================
void InitMotionEvent(GdkEventMotion *aGEM,
                            gpointer   p,
                            nsMouseEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;
  NS_ADDREF(anEvent.widget);

  anEvent.eventStructType = NS_MOUSE_EVENT;

  if (aGEM != NULL) {
    anEvent.point.x = nscoord(aGEM->x);
    anEvent.point.y = nscoord(aGEM->y);
    anEvent.time = aGEM->time;
  }
}

//==============================================================
void InitCrossingEvent(GdkEventCrossing *aGEC,
                            gpointer   p,
                            nsMouseEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;
  NS_ADDREF(anEvent.widget);

  anEvent.eventStructType = NS_MOUSE_EVENT;

  if (aGEC != NULL) {
    anEvent.point.x = nscoord(aGEC->x);
    anEvent.point.y = nscoord(aGEC->y);
    anEvent.time = aGEC->time;
  }
}

//==============================================================
void InitKeyEvent(GdkEventKey *aGEK,
                            gpointer   p,
                            nsKeyEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;
  NS_ADDREF(anEvent.widget);

  anEvent.eventStructType = NS_KEY_EVENT;

  if (aGEK != NULL) {
    anEvent.keyCode = nsConvertKey(aGEK->keyval) & 0x00FF;
    anEvent.time = aGEK->time;
    anEvent.isShift = (aGEK->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGEK->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGEK->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.time = aGEK->time;
  }
}

/*==============================================================
  ==============================================================
  =============================================================
  ==============================================================*/

void handle_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer p)
{
  nsSizeEvent sevent;
  InitAllocationEvent(alloc, p, sevent, NS_SIZE);

  nsWindow *win = (nsWindow *)p;

  win->OnResize(sevent);
}

gint handle_expose_event(GtkWidget *w, GdkEventExpose *event, gpointer p)
{
  if (event->type == GDK_NO_EXPOSE)
    return PR_FALSE;

  nsPaintEvent pevent;
  InitExposeEvent(event, p, pevent, NS_PAINT);

  nsWindow *win = (nsWindow *)p;

  win->OnPaint(pevent);

  return PR_FALSE;
}

//==============================================================
gint handle_button_press_event(GtkWidget *w, GdkEventButton * event, gpointer p)
{
  nsMouseEvent mevent;
  int b = 0;

  switch (event->button)
  {
    case 1:
      b = NS_MOUSE_LEFT_BUTTON_DOWN;
      break;
    case 2:
      b = NS_MOUSE_MIDDLE_BUTTON_DOWN;
      break;
    case 3:
      b = NS_MOUSE_RIGHT_BUTTON_DOWN;
      break;
    default:
      b = NS_MOUSE_LEFT_BUTTON_DOWN;
      break;
  }
  InitMouseEvent(event, p, mevent, b);

  nsWindow *win = (nsWindow *)p;
  win->DispatchMouseEvent(mevent);

  return PR_FALSE;
}

//==============================================================
gint handle_button_release_event(GtkWidget *w, GdkEventButton * event, gpointer p)
{
  nsMouseEvent mevent;
  int b = 0;

  switch (event->button)
  {
    case 1:
      b = NS_MOUSE_LEFT_BUTTON_UP;
      break;
    case 2:
      b = NS_MOUSE_MIDDLE_BUTTON_UP;
      break;
    case 3:
      b = NS_MOUSE_RIGHT_BUTTON_UP;
      break;
    default:
      b = NS_MOUSE_LEFT_BUTTON_UP;
      break;
  }
  InitMouseEvent(event, p, mevent, b);

  nsWindow *win = (nsWindow *)p;
  win->DispatchMouseEvent(mevent);

  return PR_FALSE;
}

//==============================================================
gint handle_motion_notify_event(GtkWidget *w, GdkEventMotion * event, gpointer p)
{
  nsMouseEvent mevent;
  InitMotionEvent(event, p, mevent, NS_MOUSE_MOVE);

  nsWindow *win = (nsWindow *)p;
  win->DispatchMouseEvent(mevent);

  return PR_FALSE;
}

//==============================================================
gint handle_enter_notify_event(GtkWidget *w, GdkEventCrossing * event, gpointer p)
{
  nsMouseEvent mevent;
  InitCrossingEvent(event, p, mevent, NS_MOUSE_ENTER);

  nsWindow *win = (nsWindow *)p;
  win->DispatchMouseEvent(mevent);

  return PR_FALSE;
}

//==============================================================
gint handle_leave_notify_event(GtkWidget *w, GdkEventCrossing * event, gpointer p)
{
  nsMouseEvent mevent;
  InitCrossingEvent(event, p, mevent, NS_MOUSE_EXIT);

  nsWindow *win = (nsWindow *)p;
  win->DispatchMouseEvent(mevent);

  return PR_FALSE;
}

#if 0
//==============================================================
gint nsGtkWidget_Focus_Callback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
  nsWindow * widgetWindow = (nsWindow *) p ;

  XmAnyCallbackStruct * cbs = (XmAnyCallbackStruct*)call_data;
  nsGUIEvent event;
  nsGtkWidget_InitNSEvent(cbs->event, p, event,
                         cbs->reason == XmCR_FOCUS?NS_GOTFOCUS:NS_LOSTFOCUS);
  widgetWindow->DispatchFocus(event);

  return PR_FALSE;
}
#endif

#if 0
//==============================================================
gint nsGtkWidget_Toggle_Callback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
  nsWindow * widgetWindow = (nsWindow *) p ;
  if (DBG) fprintf(stderr, "***************** nsGtkWidget_Scrollbar_Callback\n");

  nsScrollbarEvent sevent;
  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;

  return PR_FALSE;
}
#endif

#if 0
//==============================================================
gint nsGtkWidget_CheckButton_Toggle_Callback(GtkWidget *w, gpointer p)
{
  nsCheckButton *checkBtn = (nsCheckButton*)gtk_object_get_user_data(GTK_OBJECT(w));
  if (GTK_TOGGLE_BUTTON(w)->active)
    checkBtn->Armed();
  else
    checkBtn->DisArmed();

  return PR_FALSE;
}
#endif

#if 0
//==============================================================
gint nsGtkWidget_RadioButton_ArmCallback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
  nsRadioButton * radioBtn = (nsRadioButton *) p ;
  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  radioBtn->Armed();
  nsMouseEvent mevent;
  nsGtkWidget_InitNSMouseEvent(cbs->event, p, mevent, NS_MOUSE_LEFT_BUTTON_DOWN);
  radioBtn->DispatchMouseEvent(mevent);

  return PR_FALSE;
}
#endif

#if 0
//==============================================================
gint nsGtkWidget_RadioButton_DisArmCallback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
  nsRadioButton * radioBtn = (nsRadioButton *) p ;
  nsScrollbarEvent sevent;
  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  radioBtn->DisArmed();
  nsMouseEvent mevent;
  nsGtkWidget_InitNSMouseEvent(cbs->event, p, mevent, NS_MOUSE_LEFT_BUTTON_UP);
  radioBtn->DispatchMouseEvent(mevent);

  return PR_FALSE;
}
#endif


//==============================================================
void handle_scrollbar_value_changed(GtkAdjustment *adj, gpointer p)
{
  nsScrollbar *widget = (nsScrollbar*) p;
  nsScrollbarEvent sevent;
  sevent.message = NS_SCROLLBAR_POS;
  sevent.widget  = (nsWidget *) p;
  sevent.eventStructType = NS_SCROLLBAR_EVENT;
  widget->OnScroll(sevent, adj->value);

/* FIXME we need to set point.* from the event stuff. */
#if 0
  nsWindow * widgetWindow = (nsWindow *) p ;
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
#endif
}

//==============================================================
gint handle_key_release_event(GtkWidget *w, GdkEventKey* event, gpointer p)
{
  nsKeyEvent kevent;
  InitKeyEvent(event, p, kevent, NS_KEY_UP);

  nsWindow * win = (nsWindow *) p;
  win->OnKey(kevent);

  return PR_FALSE;
}

//==============================================================
gint handle_key_press_event(GtkWidget *w, GdkEventKey* event, gpointer p)
{
  nsKeyEvent kevent;
  InitKeyEvent(event, p, kevent, NS_KEY_DOWN);

  nsWindow * win = (nsWindow *) p;
  win->OnKey(kevent);

  return PR_FALSE;
}

//==============================================================
gint nsGtkWidget_FSBCancel_Callback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
#if 0
  nsFileWidget * widgetWindow = (nsFileWidget *) p ;
  if (p != nsnull) {
    widgetWindow->OnCancel();
  }
#endif

  return PR_FALSE;
}

//==============================================================
gint nsGtkWidget_FSBOk_Callback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
#if 0
  nsFileWidget * widgetWindow = (nsFileWidget *) p;
  if (p != nsnull) {
    widgetWindow->OnOk();
  }
#endif

  return PR_FALSE;
}

//==============================================================
gint nsGtkWidget_Menu_Callback(GtkWidget *w, gpointer p)
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

  return PR_FALSE;
}
