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
                                GtkWidget *w, gpointer p, GdkEvent * event);

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
void nsGtkWidget_InitNSEvent(GdkEvent *aGev,
                            gpointer   p,
                            nsGUIEvent &anEvent,
                            PRUint32   aEventType)
{
  GdkEventButton *anXEv = (GdkEventButton*)aGev;

  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;
  anEvent.eventStructType = NS_GUI_EVENT;

  if (anXEv != NULL) {
    anEvent.point.x = nscoord(anXEv->x);
    anEvent.point.y = nscoord(anXEv->y);
  }

  anEvent.time = anXEv->time;
}

//==============================================================
void nsGtkWidget_InitNSMouseEvent(GdkEvent *aGev,
                                 gpointer     p,
                                 nsMouseEvent &anEvent,
                                 PRUint32     aEventType)
{
  GdkEventButton *anXEv = (GdkEventButton*)aGev;

  // Do base initialization
  nsGtkWidget_InitNSEvent(aGev, p, anEvent, aEventType);

  if (anXEv != NULL) { // Do Mouse Event specific intialization
    anEvent.time       = anXEv->time;
    anEvent.isShift    = (anXEv->state & ShiftMask) ? PR_TRUE : PR_FALSE;
    anEvent.isControl  = (anXEv->state & ControlMask) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt      = (anXEv->state & Mod1Mask) ? PR_TRUE : PR_FALSE;
//    anEvent.clickCount = anXEv->button; //XXX Fix for double-clicks
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
#if 0
static Bool checkForExpose(Display *dpy, XEvent *evt, XtPointer client_data)
{
    CollapseInfo *cinfo = (CollapseInfo*)client_data;

    if ((evt->type == Expose && evt->xexpose.window == cinfo->win &&
         INTERSECTS(cinfo->r->x, cinfo->r->width, cinfo->r->y, cinfo->r->height,
                    evt->xexpose.x, evt->xexpose.y,
                    evt->xexpose.x + evt->xexpose.width,
                    evt->xexpose.y + evt->xexpose.height)) ||
         (evt->type == GraphicsExpose && evt->xgraphicsexpose.drawable == cinfo->win &&
         INTERSECTS(cinfo->r->x, cinfo->r->width, cinfo->r->y, cinfo->r->height,
                    evt->xgraphicsexpose.x, evt->xgraphicsexpose.y,
                    evt->xgraphicsexpose.x + evt->xgraphicsexpose.width,
                    evt->xgraphicsexpose.y + evt->xgraphicsexpose.height))) {

        return True;
    }
    return False;
}
#endif

//==============================================================
gint nsGtkWidget_ExposureMask_EventHandler(GtkWidget *w, GdkEventExpose *event, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow *)p;

  nsPaintEvent pevent;
  nsRect       rect;
  nsGtkWidget_InitNSEvent((GdkEvent*)event, p, pevent, NS_PAINT);
  pevent.rect = (nsRect *)&rect;

  rect.x      = event->area.x;
  rect.y      = event->area.y;
  rect.width  = event->area.width;
  rect.height = event->area.height;

  if (event->type == GDK_NO_EXPOSE) {
    return FALSE;
  }
/* FIXME
  Display* display = XtDisplay(w);
  Window   window = XtWindow(w);
  XEvent xev;

  XSync(display, FALSE);

  while (XCheckTypedWindowEvent(display, window, Expose, &xev) == TRUE) {
      rect.x      = xev.xexpose.x;
      rect.y      = xev.xexpose.y;
      rect.width  = xev.xexpose.width;
      rect.height = xev.xexpose.height;
  }
*/
  widgetWindow->OnPaint(pevent);

  return FALSE;
}

//==============================================================
gint nsGtkWidget_ButtonPressMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow *)p;
  nsMouseEvent mevent;
  nsGtkWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_LEFT_BUTTON_DOWN);
  widgetWindow->DispatchMouseEvent(mevent);

  return FALSE;
}

//==============================================================
gint nsGtkWidget_ButtonReleaseMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow *)p;
  nsMouseEvent mevent;
  nsGtkWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_LEFT_BUTTON_UP);
  widgetWindow->DispatchMouseEvent(mevent);

  return FALSE;
}

//==============================================================
gint nsGtkWidget_ButtonMotionMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p)
{
  nsPaintEvent pevent;
  nsWindow *widgetWindow = (nsWindow *)p;
  nsMouseEvent mevent;
  nsGtkWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_MOVE);
  widgetWindow->DispatchMouseEvent(mevent);

  return FALSE;
}

//==============================================================
gint nsGtkWidget_MotionMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow *)p;
  nsMouseEvent mevent;
  nsGtkWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_MOVE);
  widgetWindow->DispatchMouseEvent(mevent);

  return FALSE;
}

//==============================================================
gint nsGtkWidget_EnterMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow *)p;
  nsMouseEvent mevent;
  nsGtkWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_ENTER);
  widgetWindow->DispatchMouseEvent(mevent);

  return FALSE;
}

//==============================================================
gint nsGtkWidget_LeaveMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p)
{
  if (DBG) fprintf(stderr, "***************** nsGtkWidget_LeaveMask_EventHandler\n");
  nsWindow * widgetWindow = (nsWindow *)p;
  nsMouseEvent mevent;
  nsGtkWidget_InitNSMouseEvent(event, p, mevent, NS_MOUSE_EXIT);
  widgetWindow->DispatchMouseEvent(mevent);

  return FALSE;
}

//==============================================================
gint nsGtkWidget_Focus_Callback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
#if 0
  nsWindow * widgetWindow = (nsWindow *) p ;

  XmAnyCallbackStruct * cbs = (XmAnyCallbackStruct*)call_data;
  nsGUIEvent event;
  nsGtkWidget_InitNSEvent(cbs->event, p, event,
                         cbs->reason == XmCR_FOCUS?NS_GOTFOCUS:NS_LOSTFOCUS);
  widgetWindow->DispatchFocus(event);
#endif

  return FALSE;
}

//==============================================================
gint nsGtkWidget_Toggle_Callback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
#if 0
  nsWindow * widgetWindow = (nsWindow *) p ;
  if (DBG) fprintf(stderr, "***************** nsGtkWidget_Scrollbar_Callback\n");

  nsScrollbarEvent sevent;
  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
#endif

  return FALSE;
}

//==============================================================
gint nsGtkWidget_CheckButton_Toggle_Callback(GtkWidget *w, gpointer p)
{
  nsCheckButton *checkBtn = (nsCheckButton*)gtk_object_get_user_data(GTK_OBJECT(w));
  if (GTK_TOGGLE_BUTTON(w)->active)
    checkBtn->Armed();
  else
    checkBtn->DisArmed();

  return FALSE;
}

//==============================================================
gint nsGtkWidget_RadioButton_ArmCallback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
#if 0
  nsRadioButton * radioBtn = (nsRadioButton *) p ;
  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  radioBtn->Armed();
  nsMouseEvent mevent;
  nsGtkWidget_InitNSMouseEvent(cbs->event, p, mevent, NS_MOUSE_LEFT_BUTTON_DOWN);
  radioBtn->DispatchMouseEvent(mevent);
#endif

  return FALSE;
}

//==============================================================
gint nsGtkWidget_RadioButton_DisArmCallback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
#if 0
  nsRadioButton * radioBtn = (nsRadioButton *) p ;
  nsScrollbarEvent sevent;
  XmToggleButtonCallbackStruct * cbs = (XmToggleButtonCallbackStruct*)call_data;
  radioBtn->DisArmed();
  nsMouseEvent mevent;
  nsGtkWidget_InitNSMouseEvent(cbs->event, p, mevent, NS_MOUSE_LEFT_BUTTON_UP);
  radioBtn->DispatchMouseEvent(mevent);
#endif

  return FALSE;
}


//==============================================================
gint nsGtkWidget_Scrollbar_Callback(GtkWidget *w, gpointer p)
{
  //  nsScrollbar *widget = (nsScrollbar*)gtk_object_get_user_data(GTK_OBJECT(w));
  nsScrollbar *widget = (nsScrollbar*) p;
  nsScrollbarEvent sevent;

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
  sevent.message = NS_SCROLLBAR_POS;
  sevent.widget  = (nsWidget *) p;
  widget->OnScroll(sevent, GTK_ADJUSTMENT(w)->value);
  return 0; /* XXX */
}



//==============================================================
gint nsGtkWidget_Expose_Callback(GtkWidget *w, gpointer p)
{
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
#if 0
  nsWindow * widgetWindow = (nsWindow *) p ;
  if (widgetWindow == nsnull) {
    return;
  }

  XmDrawingAreaCallbackStruct * cbs = (XmDrawingAreaCallbackStruct *)call_data;
  XEvent * event = cbs->event;
  nsPaintEvent pevent;
  nsRect       rect;
  nsGtkWidget_InitNSEvent(event, p, pevent, NS_PAINT);
  pevent.rect = (nsRect *)&rect;
  widgetWindow->OnPaint(pevent);
#endif
  return 0; /* XXX */
}

//==============================================================
gint nsGtkWidget_Text_Callback(GtkWidget *w, GdkEvent* event, gpointer p)
{
  nsKeyEvent kevent;
  nsGtkWidget_InitNSKeyEvent(NS_KEY_UP, kevent, w, p, event);
  nsWindow * widgetWindow = (nsWindow *) p ;
  widgetWindow->OnKey(NS_KEY_UP, kevent.keyCode, &kevent);

#if 0
  nsWindow * widgetWindow = (nsWindow *) p ;
  int len;
  XmTextVerifyCallbackStruct *cbs = (XmTextVerifyCallbackStruct *) call_data;
  PasswordData * data;
  XtVaGetValues(w, XmNuserData, &data, NULL);
  if (data == NULL || data->mIgnore) {
    return;
  }

  if (cbs->reason == XmCR_ACTIVATE) {
    printf ("Password: %s\n", data->mPassword.ToNewCString());
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
#endif
  return 0; /* XXX */
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

  return FALSE;
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

  return FALSE;
}

//==============================================================
void nsGtkWidget_InitNSKeyEvent(int aEventType, nsKeyEvent& aKeyEvent,
                                GtkWidget *w, gpointer p, GdkEvent * event)
{
  nsWindow * widgetWindow = (nsWindow *) p ;
  char *res;

  nsGtkWidget_InitNSEvent(event, p, aKeyEvent, aEventType);
  GdkEventKey *eventKey = (GdkEventKey*)event;

  aKeyEvent.keyCode   = nsConvertKey(eventKey->keyval) & 0x00FF;
  aKeyEvent.time      = eventKey->time;
  aKeyEvent.isShift   = (eventKey->state & ShiftMask) ? PR_TRUE : PR_FALSE;
  aKeyEvent.isControl = (eventKey->state & ControlMask) ? PR_TRUE : PR_FALSE;
  aKeyEvent.isAlt     = (eventKey->state & Mod1Mask) ? PR_TRUE : PR_FALSE;
}

//==============================================================
gint nsGtkWidget_KeyPressMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p)
{
  nsKeyEvent kevent;
  nsGtkWidget_InitNSKeyEvent(NS_KEY_DOWN, kevent, w, p, event);
  nsWindow * widgetWindow = (nsWindow *) p ;
  widgetWindow->OnKey(NS_KEY_DOWN, kevent.keyCode, &kevent);

  return FALSE;
}

//==============================================================
gint nsGtkWidget_KeyReleaseMask_EventHandler(GtkWidget *w, GdkEvent * event, gpointer p)
{
  nsKeyEvent kevent;
  nsGtkWidget_InitNSKeyEvent(NS_KEY_UP, kevent, w, p, event);
  nsWindow * widgetWindow = (nsWindow *) p ;
  widgetWindow->OnKey(NS_KEY_UP, kevent.keyCode, &kevent);

  return FALSE;
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

  return FALSE;
}

gint nsGtkWidget_Resize_EventHandler(GtkWidget *w, GtkAllocation *allocation, gpointer data)
{
  nsWindow *win = (nsWindow*)data;

  nsRect winBounds;
  win->GetBounds(winBounds);
  g_print("resize event handler:\n\tallocation->w=%d allocation->h=%d window.w=%d window.h=%d\n",
          allocation->width, allocation->height, winBounds.width, winBounds.height);
  if (winBounds.width != allocation->width ||
      winBounds.height != allocation->height) {
    g_print("\tAllocation != current window bounds.  Resize.\n");

    winBounds.width = allocation->width;
    winBounds.height = allocation->height;
    //    win->SetBounds(winBounds);

    nsPaintEvent pevent;
    pevent.message = NS_PAINT;
    pevent.widget = win;
    pevent.time = PR_IntervalNow();
    pevent.rect = (nsRect *)&winBounds;
    win->OnPaint(pevent);
  }
  return FALSE;
}
