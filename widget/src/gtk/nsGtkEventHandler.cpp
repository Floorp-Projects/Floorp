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

#include "nsWidget.h"
#include "nsWindow.h"
#include "nsTextWidget.h"
#include "nsCheckButton.h"
#include "nsRadioButton.h"
#include "nsScrollbar.h"
#include "nsFileWidget.h"
#include "nsGUIEvent.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"

#include "stdio.h"

#define DBG 0

#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static NS_DEFINE_IID(kCWindow,        NS_WINDOW_CID);
static NS_DEFINE_IID(kCChild,         NS_CHILD_CID);

struct EventInfo {
  nsWidget *widget;  // the widget
  nsRect   *rect;    // the rect
};

struct nsKeyConverter {
  int vkCode; // Platform independent key code
  int keysym; // GDK keysym key code
};

struct nsKeyConverter nsKeycodes[] = {
  { NS_VK_CANCEL,     GDK_Cancel },
  { NS_VK_BACK,       GDK_BackSpace },
  { NS_VK_TAB,        GDK_Tab },
  { NS_VK_CLEAR,      GDK_Clear },
  { NS_VK_RETURN,     GDK_Return },
  { NS_VK_SHIFT,      GDK_Shift_L },
  { NS_VK_SHIFT,      GDK_Shift_R },
  { NS_VK_CONTROL,    GDK_Control_L },
  { NS_VK_CONTROL,    GDK_Control_R },
  { NS_VK_ALT,        GDK_Alt_L },
  { NS_VK_ALT,        GDK_Alt_R },
  { NS_VK_PAUSE,      GDK_Pause },
  { NS_VK_CAPS_LOCK,  GDK_Caps_Lock },
  { NS_VK_ESCAPE,     GDK_Escape },
  { NS_VK_SPACE,      GDK_space },
  { NS_VK_PAGE_UP,    GDK_Page_Up },
  { NS_VK_PAGE_DOWN,  GDK_Page_Down },
  { NS_VK_END,        GDK_End },
  { NS_VK_HOME,       GDK_Home },
  { NS_VK_LEFT,       GDK_Left },
  { NS_VK_UP,         GDK_Up },
  { NS_VK_RIGHT,      GDK_Right },
  { NS_VK_DOWN,       GDK_Down },
  { NS_VK_PRINTSCREEN, GDK_Print },
  { NS_VK_INSERT,     GDK_Insert },
  { NS_VK_DELETE,     GDK_Delete },

  { NS_VK_MULTIPLY,   GDK_KP_Multiply },
  { NS_VK_ADD,        GDK_KP_Add },
  { NS_VK_SEPARATOR,  GDK_KP_Separator },
  { NS_VK_SUBTRACT,   GDK_KP_Subtract },
  { NS_VK_DECIMAL,    GDK_KP_Decimal },
  { NS_VK_DIVIDE,     GDK_KP_Divide },
  { NS_VK_RETURN,      GDK_KP_Enter },

  { NS_VK_COMMA,      GDK_comma },
  { NS_VK_PERIOD,     GDK_period },
  { NS_VK_SLASH,      GDK_slash },
//XXX: How do you get a BACK_QUOTE?  NS_VK_BACK_QUOTE, GDK_backquote,
  { NS_VK_OPEN_BRACKET, GDK_bracketleft },
  { NS_VK_CLOSE_BRACKET, GDK_bracketright },
  { NS_VK_QUOTE, GDK_quotedbl }

};

void nsGtkWidget_InitNSKeyEvent(int aEventType, nsKeyEvent& aKeyEvent,
                                GtkWidget *w, gpointer p, GdkEventKey * event);

// Input keysym is in gtk format; output is in NS_VK format
int nsConvertKey(int keysym)
{
  int i;
  int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);
  for (i = 0; i < length; i++) {
    if (nsKeycodes[i].keysym == keysym)
      return(nsKeycodes[i].vkCode);
  }

  return((int)0);
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
void InitConfigureEvent(GdkEventConfigure *aConf,
                            gpointer   p,
                            nsSizeEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;
  NS_ADDREF(anEvent.widget);

  anEvent.eventStructType = NS_SIZE_EVENT;

  if (aConf != NULL) {
  /* do we accually need to alloc a new rect, or can we just set the
     current one */
    nsRect *foo = new nsRect(aConf->x, aConf->y, aConf->width, aConf->height);
    anEvent.windowSize = foo;
    anEvent.point.x = aConf->x;
    anEvent.point.y = aConf->y;
    anEvent.mWinWidth = aConf->width;
    anEvent.mWinHeight = aConf->height;
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
      case GDK_3BUTTON_PRESS:  /* Clamp to double-click */
        anEvent.clickCount = 2;
        break;
      default:
        anEvent.clickCount = 1;
    }

  }
}

//==============================================================
void InitDrawEvent(GdkRectangle *area,
                            gpointer   p,
                            nsPaintEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;
  NS_ADDREF(anEvent.widget);

  anEvent.eventStructType = NS_PAINT_EVENT;

  if (area != NULL) {
    nsRect *rect = new nsRect(area->x, area->y, area->width, area->height);
    anEvent.rect = rect;
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
    nsRect *rect = new nsRect(aGEE->area.x, aGEE->area.y,
                              aGEE->area.width, aGEE->area.height);
    anEvent.rect = rect;
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
    anEvent.charCode = aGEK->keyval;
    anEvent.time = aGEK->time;
    anEvent.isShift = (aGEK->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGEK->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGEK->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.point.x = 0;
    anEvent.point.y = 0;
  }
}

//==============================================================
void InitFocusEvent(GdkEventFocus *aGEF,
                            gpointer   p,
                            nsGUIEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;
  NS_ADDREF(anEvent.widget);

  anEvent.eventStructType = NS_GUI_EVENT;

  anEvent.time = 0;
  anEvent.point.x = 0;
  anEvent.point.y = 0;
}

/*==============================================================
  ==============================================================
  =============================================================
  ==============================================================*/

// this function will clear the queue of any resize
// or move events
static gint
idle_resize_cb(gpointer data)
{
  EventInfo *info = (EventInfo *)data;
  //  g_print("idle_resize_cb: sending event for %p\n",
  //          info->widget);
  info->widget->OnResize(*info->rect);
  NS_RELEASE(info->widget);
  // FIXME free info->rect
  g_free(info);
  // this will return 0 if the list is
  // empty.  that will remove this idle timeout.
  // if it's > 1 then it will be restarted and
  // this will be run again.
  return FALSE;
}

void handle_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer p)
{
  nsWindow *widget = (nsWindow *)p;
  EventInfo *eventinfo = 0;
  GtkAllocation *old_size = 0;
  PRBool send_event = PR_FALSE;

  old_size = (GtkAllocation *) gtk_object_get_data(GTK_OBJECT(w), "mozilla.old_size");
  // see if we need to allocate this - this may be the first time
  // the size allocation has happened.
  if (!old_size) { 
    old_size = (GtkAllocation *) g_malloc(sizeof(GtkAllocation));
    old_size->x = alloc->x;
    old_size->y = alloc->y;
    old_size->width = alloc->width;
    old_size->height = alloc->height;
    gtk_object_set_data(GTK_OBJECT(w), "mozilla.old_size", old_size);
    send_event = PR_TRUE;
  }
  // only send an event if we've actually changed sizes.
  else if ((old_size->x != alloc->x) ||
           (old_size->y != alloc->y) ||
           (old_size->width != alloc->width) ||
           (old_size->height != alloc->height)) {
    old_size->x = alloc->x;
    old_size->y = alloc->y;
    old_size->width = alloc->width;
    old_size->height = alloc->height;
    send_event = PR_TRUE;
  }
  // send it out.
  if (send_event == PR_TRUE) {
    eventinfo = (EventInfo *)g_malloc(sizeof(struct EventInfo));
    eventinfo->rect = new nsRect();
    eventinfo->rect->x = alloc->x;
    eventinfo->rect->y = alloc->y;
    eventinfo->rect->width = alloc->width;
    eventinfo->rect->height = alloc->height;
    eventinfo->widget = widget;
    NS_ADDREF(widget);
    gtk_idle_add(idle_resize_cb, eventinfo);
  }
}

#if 0
gint handle_configure_event(GtkWidget *w, GdkEventConfigure *conf, gpointer p)
{
  g_print("configure_event: {x=%i, y=%i, w=%i, h=%i}\n", conf->x,
           conf->y,
	   conf->width,
	   conf->height);
  
  nsSizeEvent sevent;
  InitConfigureEvent(conf, p, sevent, NS_SIZE);

  nsWindow *win = (nsWindow *)p;

  win->OnResize(sevent);

  return PR_FALSE;
}
#endif

gint handle_draw_event(GtkWidget *w, GdkRectangle *area, gpointer p)
{
  nsPaintEvent pevent;
  InitDrawEvent(area, p, pevent, NS_PAINT);

  nsWindow *win = (nsWindow *)p;

  win->OnPaint(pevent);

  return PR_TRUE;
}

gint handle_expose_event(GtkWidget *w, GdkEventExpose *event, gpointer p)
{
  if (event->type == GDK_NO_EXPOSE)
    return PR_FALSE;

  nsPaintEvent pevent;
  InitExposeEvent(event, p, pevent, NS_PAINT);

  nsWindow *win = (nsWindow *)p;

  win->OnPaint(pevent);

  return PR_TRUE;
}

//==============================================================
gint handle_button_press_event(GtkWidget *w, GdkEventButton * event, gpointer p)
{
  nsMouseEvent mevent;
  int b = 0;

  /* Switch on single, double, triple click. */
  switch (event->type) {
  case GDK_BUTTON_PRESS:   /* Single click. */
    switch (event->button)  /* Which button? */
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
        /* Single-click default. */
        b = NS_MOUSE_LEFT_BUTTON_DOWN;
        break;
      }
    break;

  case GDK_2BUTTON_PRESS:   /* Double click. */
    switch (event->button)  /* Which button? */
      {
      case 1:
        b = NS_MOUSE_LEFT_DOUBLECLICK;
        break;
      case 2:
        b = NS_MOUSE_MIDDLE_DOUBLECLICK;
        break;
      case 3:
        b = NS_MOUSE_RIGHT_DOUBLECLICK;
        break;
      default:
        /* Double-click default. */
        b = NS_MOUSE_LEFT_DOUBLECLICK;
        break;
      }
    break;

  case GDK_3BUTTON_PRESS:   /* Triple click. */
    /* Unhandled triple click. */
    break;

  default:
    break;
  }
  InitMouseEvent(event, p, mevent, b);

  nsWindow *win = (nsWindow *)p;
  win->DispatchMouseEvent(mevent);

  return PR_TRUE;
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

  return PR_TRUE;
}

//==============================================================
gint handle_motion_notify_event(GtkWidget *w, GdkEventMotion * event, gpointer p)
{
  nsMouseEvent mevent;
  InitMotionEvent(event, p, mevent, NS_MOUSE_MOVE);

  nsWindow *win = (nsWindow *)p;
  win->DispatchMouseEvent(mevent);

  return PR_TRUE;
}

//==============================================================
gint handle_enter_notify_event(GtkWidget *w, GdkEventCrossing * event, gpointer p)
{
  nsMouseEvent mevent;
  InitCrossingEvent(event, p, mevent, NS_MOUSE_ENTER);

  nsWindow *win = (nsWindow *)p;
  win->DispatchMouseEvent(mevent);

  return PR_TRUE;
}

//==============================================================
gint handle_leave_notify_event(GtkWidget *w, GdkEventCrossing * event, gpointer p)
{
  nsMouseEvent mevent;
  InitCrossingEvent(event, p, mevent, NS_MOUSE_EXIT);

  nsWindow *win = (nsWindow *)p;
  win->DispatchMouseEvent(mevent);

  return PR_TRUE;
}

//==============================================================
gint handle_focus_in_event(GtkWidget *w, GdkEventFocus * event, gpointer p)
{
  nsGUIEvent gevent;
  InitFocusEvent(event, p, gevent, NS_GOTFOCUS);

  nsWindow *win = (nsWindow *)p;
  win->DispatchFocus(gevent);

  return PR_TRUE;
}

//==============================================================
gint handle_focus_out_event(GtkWidget *w, GdkEventFocus * event, gpointer p)
{
  nsGUIEvent gevent;
  InitFocusEvent(event, p, gevent, NS_LOSTFOCUS);

  nsWindow *win = (nsWindow *)p;
  win->DispatchFocus(gevent);

  return PR_TRUE;
}

//==============================================================
void menu_item_activate_handler(GtkWidget *w, gpointer p)
{
  g_print("menu selected\n");

  nsIMenuListener *menuListener = nsnull;
  nsIMenuItem *menuItem = (nsIMenuItem *)p;
  
  if (menuItem != NULL) {
    nsMenuEvent mevent;
    mevent.message = NS_MENU_SELECTED;
    mevent.eventStructType = NS_MENU_EVENT;
    mevent.point.x = 0;
    mevent.point.y = 0;
    menuItem->GetTarget(mevent.widget);
    menuItem->GetCommand(mevent.mCommand);

    mevent.mMenuItem = menuItem;
    mevent.time = PR_IntervalNow();

    nsEventStatus status;
    // FIXME - THIS SHOULD WORK.  FIX EVENTS FOR XP CODE!!!!! (pav)
//    mevent.widget->DispatchEvent((nsGUIEvent *)&mevent, status);

    menuItem->QueryInterface(kIMenuListenerIID, (void**)&menuListener);
    if(menuListener) {
      menuListener->MenuSelected(mevent);
      NS_IF_RELEASE(menuListener);
    }
  }
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
  // Don't pass shift, control and alt as key release events
  if (event->state & GDK_SHIFT_MASK
      || event->state & GDK_CONTROL_MASK
      || event->state & GDK_MOD1_MASK)
    return PR_FALSE;

  nsKeyEvent kevent;
  InitKeyEvent(event, p, kevent, NS_KEY_UP);

  nsWindow * win = (nsWindow *) p;
  win->OnKey(kevent);

  return PR_TRUE;
}

//==============================================================
gint handle_key_press_event(GtkWidget *w, GdkEventKey* event, gpointer p)
{
  // Don't pass shift, control and alt as key press events
  if (event->state & GDK_SHIFT_MASK
      || event->state & GDK_CONTROL_MASK
      || event->state & GDK_MOD1_MASK)
    return PR_FALSE;

  nsKeyEvent kevent;
  InitKeyEvent(event, p, kevent, NS_KEY_DOWN);

  nsWindow * win = (nsWindow *) p;
  win->OnKey(kevent);

  return PR_TRUE;
}

//==============================================================
gint nsGtkWidget_FSBCancel_Callback(GtkWidget *w, gpointer p)
{
#if 0
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
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
#if 0
  nsWindow *widgetWindow = (nsWindow*)gtk_object_get_user_data(GTK_OBJECT(w));
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

  return PR_TRUE;
}
