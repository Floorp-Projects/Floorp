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
#ifdef USE_XIM
#include "nsTextWidget.h"
#endif
#include "nsScrollbar.h"
#include "nsIFileWidget.h"
#include "nsGUIEvent.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"

#ifdef        USE_XIM
#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID,
                     NS_ICHARSETCONVERTERMANAGER_CID);
#endif /* USE_XIM */

#include "stdio.h"
#include "ctype.h"

#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define DEBUG_EVENTS 1

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
  { NS_VK_TAB,        GDK_ISO_Left_Tab },
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
  { NS_VK_RETURN,     GDK_KP_Enter },

  { NS_VK_COMMA,      GDK_comma },
  { NS_VK_PERIOD,     GDK_period },
  { NS_VK_SLASH,      GDK_slash },
  { NS_VK_BACK_SLASH, GDK_backslash },
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


  // First, try to handle alphanumeric input, not listed in nsKeycodes:
  // most likely, more letters will be getting typed in than things in
  // the key list, so we will look through these first.

  // since X has different key symbols for upper and lowercase letters and
  // mozilla does not, convert gdk's to mozilla's
  if (keysym >= GDK_a && keysym <= GDK_z)
    return keysym - GDK_a + NS_VK_A;
  if (keysym >= GDK_A && keysym <= GDK_Z)
    return keysym - GDK_A + NS_VK_A;

  // numbers
  if (keysym >= GDK_0 && keysym <= GDK_9)
    return keysym - GDK_0 + NS_VK_0;

  // keypad numbers
  if (keysym >= GDK_KP_0 && keysym <= GDK_KP_9)
    return keysym - GDK_KP_0 + NS_VK_NUMPAD0;

  // misc other things
  for (i = 0; i < length; i++) {
    if (nsKeycodes[i].keysym == keysym)
      return(nsKeycodes[i].vkCode);
  }

  // function keys
  if (keysym >= GDK_F1 && keysym <= GDK_F24)
    return keysym - GDK_F1 + NS_VK_F1;

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

  anEvent.eventStructType = NS_SIZE_EVENT;

  if (aAlloc != nsnull) {
    // HACK
    //    nsRect *foo = new nsRect(aAlloc->x, aAlloc->y, aAlloc->width, aAlloc->height);
    nsRect *foo = new nsRect(0, 0, aAlloc->width, aAlloc->height);
    anEvent.windowSize = foo;
    //    anEvent.point.x = aAlloc->x;
    //    anEvent.point.y = aAlloc->y;
    // HACK
    anEvent.point.x = 0;
    anEvent.point.y = 0;
    anEvent.mWinWidth = aAlloc->width;
    anEvent.mWinHeight = aAlloc->height;
  }

  anEvent.time = PR_IntervalNow();
}

//==============================================================
void InitConfigureEvent(GdkEventConfigure *aConf,
                            gpointer   p,
                            nsSizeEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;

  anEvent.eventStructType = NS_SIZE_EVENT;

  if (aConf != nsnull) {
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
void InitExposeEvent(GdkEventExpose *aGEE,
                     gpointer   p,
                     nsPaintEvent &anEvent,
                     PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;

  anEvent.eventStructType = NS_PAINT_EVENT;

  if (aGEE != nsnull)
  {
#ifdef DEBUG_EVENTS
    g_print("expose event: x = %i , y = %i , w = %i , h = %i\n",
            aGEE->area.x, aGEE->area.y,
            aGEE->area.width, aGEE->area.height);
#endif
    anEvent.point.x = aGEE->area.x;
    anEvent.point.y = aGEE->area.y;

    nsRect *rect = new nsRect(aGEE->area.x, aGEE->area.y,
                              aGEE->area.width, aGEE->area.height);
    anEvent.rect = rect;
    anEvent.time = gdk_event_get_time((GdkEvent*)aGEE);
  }
}

//=============================================================
void UninitExposeEvent(GdkEventExpose *aGEE,
                       gpointer   p,
                       nsPaintEvent &anEvent,
                       PRUint32   aEventType)
{
  if (aGEE != nsnull) {
    delete anEvent.rect;
  }
}

//==============================================================

#if 1
PRUint32 nsConvertCharCodeToUnicode(GdkEventKey* aGEK)
{
  // ALT keys in gdk give the upper case character in string,
  // but we want the lower case char in char code
  // unless shift was also pressed.
  if (((aGEK->state & GDK_CONTROL_MASK) || (aGEK->state & GDK_MOD1_MASK))
      && !(aGEK->state & GDK_SHIFT_MASK)
      && isupper(aGEK->string[0]))
    return tolower(aGEK->string[0]);

  //
  // placeholder for something a little more interesting and correct
  //
  return aGEK->string[0];
}

//==============================================================
void InitKeyEvent(GdkEventKey *aGEK,
                            gpointer   p,
                            nsKeyEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;

  anEvent.eventStructType = NS_KEY_EVENT;

  if (aGEK != nsnull) {
    anEvent.keyCode = nsConvertKey(aGEK->keyval) & 0x00FF;
    anEvent.charCode = 0;
    anEvent.time = aGEK->time;
    anEvent.isShift = (aGEK->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGEK->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGEK->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.point.x = 0;
    anEvent.point.y = 0;
  }
}

void InitKeyPressEvent(GdkEventKey *aGEK,
                       gpointer p,
                       nsKeyEvent &anEvent)
{
  //
  // init the basic event fields
  //
  anEvent.eventStructType = NS_KEY_EVENT;
  anEvent.message = NS_KEY_PRESS;
  anEvent.widget = (nsWidget*)p;

  if (aGEK!=nsnull) {
    anEvent.keyCode = nsConvertKey(aGEK->keyval) & 0x00FF;
    anEvent.charCode = nsConvertCharCodeToUnicode(aGEK);
#ifdef DEBUG_pavlov
    g_print("%s\n", aGEK->string);
#endif
    anEvent.time = aGEK->time;
    anEvent.isShift = (aGEK->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGEK->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGEK->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.point.x = 0;
    anEvent.point.y = 0;
  }

}


#else
//==============================================================
void InitKeyEvent(GdkEventKey *aGEK,
                            gpointer   p,
                            nsKeyEvent &anEvent,
                            PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = (nsWidget *) p;

  anEvent.eventStructType = NS_KEY_EVENT;

  if (aGEK != nsnull) {
    anEvent.keyCode = nsConvertKey(aGEK->keyval) & 0x00FF;
    //
    // As per joki: as long as we're using ASCII rather than Unicode,
    // it's okay to set the keycode to the charcode because the ns
    // keycodes (e.g. VK_X) happen to map to ascii values;
    // however, when we move to unicode, we'll want to send the 
    // unicode translation in the charCode field.
    //
    anEvent.charCode = aGEK->keyval;
    anEvent.time = aGEK->time;
    anEvent.isShift = (aGEK->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGEK->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGEK->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.point.x = 0;
    anEvent.point.y = 0;
  }
}
#endif

//=============================================================
void UninitKeyEvent(GdkEventKey *aGEK,
                              gpointer   p,
                              nsKeyEvent &anEvent,
                              PRUint32   aEventType)
{
}

/*==============================================================
  ==============================================================
  =============================================================
  ==============================================================*/

void handle_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer p)
{
  nsWindow *widget = (nsWindow *)p;
  nsSizeEvent event;

  InitAllocationEvent(alloc, p, event, NS_SIZE);
  NS_ADDREF(widget);
  widget->OnResize(event);
  NS_RELEASE(widget);

  delete event.windowSize;
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
  win->AddRef();
  win->OnResize(sevent);
  win->Release();

  return PR_FALSE;
}
#endif

gint handle_expose_event(GtkWidget *w, GdkEventExpose *event, gpointer p)
{
  if (event->type == GDK_NO_EXPOSE)
    return PR_FALSE;

  nsPaintEvent pevent;
  InitExposeEvent(event, p, pevent, NS_PAINT);

  nsWindow *win = (nsWindow *)p;
  win->AddRef();
  win->OnPaint(pevent);
  win->Release();

  UninitExposeEvent(event, p, pevent, NS_PAINT);

  return PR_TRUE;
}

//==============================================================
void menu_item_activate_handler(GtkWidget *w, gpointer p)
{
  // g_print("menu_item_activate_handler\n");

  nsIMenuListener *menuListener = nsnull;
  nsIMenuItem *menuItem = (nsIMenuItem *)p;
  if (menuItem != nsnull) {
    nsMenuEvent mevent;
    mevent.message = NS_MENU_SELECTED;
    mevent.eventStructType = NS_MENU_EVENT;
    mevent.point.x = 0;
    mevent.point.y = 0;
    // mevent.widget = menuItem;
    mevent.widget = nsnull;
    menuItem->GetCommand(mevent.mCommand);

    mevent.mMenuItem = menuItem;
    mevent.time = PR_IntervalNow();

    // FIXME - THIS SHOULD WORK.  FIX EVENTS FOR XP CODE!!!!! (pav)
    //    nsEventStatus status;
    //    mevent.widget->DispatchEvent((nsGUIEvent *)&mevent, status);

    menuItem->QueryInterface(nsIMenuListener::GetIID(), (void**)&menuListener);
    if(menuListener) {
      menuListener->MenuItemSelected(mevent);
      NS_IF_RELEASE(menuListener);
    }
  }
}

//==============================================================
void menu_map_handler(GtkWidget *w, gpointer p)
{ 
  nsIMenuListener *menuListener = nsnull;
  nsIMenu *menu = (nsIMenu *)p;
  if (menu != nsnull) {
    nsMenuEvent mevent;
    mevent.message = NS_MENU_SELECTED;
    mevent.eventStructType = NS_MENU_EVENT;
    mevent.point.x = 0;
    mevent.point.y = 0;
    mevent.widget = nsnull;

    mevent.time = PR_IntervalNow();

    menu->QueryInterface(nsIMenuListener::GetIID(), (void**)&menuListener);

    if(menuListener) {
      menuListener->MenuConstruct(
        mevent,
      	nsnull,   //parent window
        nsnull,   //menuNode
        nsnull ); // webshell
      NS_IF_RELEASE(menuListener);
    }
  }
}

//==============================================================
void menu_unmap_handler(GtkWidget *w, gpointer p)
{
  nsIMenuListener *menuListener = nsnull;
  nsIMenu *menu = (nsIMenu *)p;
  if (menu != nsnull) {
    nsMenuEvent mevent;
    mevent.message = NS_MENU_SELECTED;
    mevent.eventStructType = NS_MENU_EVENT;
    mevent.point.x = 0;
    mevent.point.y = 0;
    mevent.widget = nsnull;

    mevent.time = PR_IntervalNow();

    menu->QueryInterface(nsIMenuListener::GetIID(), (void**)&menuListener);
    if(menuListener) {
      menuListener->MenuDestruct(mevent);
      NS_IF_RELEASE(menuListener);
    }
  }
}


//==============================================================
void handle_scrollbar_value_changed(GtkAdjustment *adj, gpointer p)
{
  nsScrollbar *widget = (nsScrollbar*) p;
  nsScrollbarEvent sevent;

  sevent.message = NS_SCROLLBAR_POS;
  sevent.widget  = (nsWidget *) p;
  sevent.eventStructType = NS_SCROLLBAR_EVENT;

  GdkWindow *win = (GdkWindow *)widget->GetNativeData(NS_NATIVE_WINDOW);
  gdk_window_get_pointer(win, &sevent.point.x, &sevent.point.y, nsnull);

#ifdef NS_GTK_REF
  widget->ReleaseNativeData(NS_NATIVE_WINDOW);
#endif

  widget->AddRef();
  widget->OnScroll(sevent, adj->value);
  widget->Release();

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

#if 1
//==============================================================
gint handle_key_release_event(GtkWidget *w, GdkEventKey* event, gpointer p)
{
  // Don't pass shift, control and alt as key release events
  if (event->keyval == GDK_Shift_L
      || event->keyval == GDK_Shift_R
      || event->keyval == GDK_Control_L
      || event->keyval == GDK_Control_R)
    return PR_TRUE;

  nsKeyEvent kevent;
  InitKeyEvent(event, p, kevent, NS_KEY_UP);

  nsWindow * win = (nsWindow *) p;
  win->AddRef();
  win->OnKey(kevent);
  win->Release();

  gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_release_event");

  return PR_TRUE;
}

#ifdef USE_XIM
static gint composition_start(GdkEventKey *aEvent, nsWindow *aWin,
                              nsEventStatus *aStatus) {
  nsCompositionEvent compEvent;

  compEvent.widget = (nsWidget*)aWin;
  compEvent.point.x = 0;
  compEvent.point.y = 0;
  compEvent.time = aEvent->time;
  compEvent.message = NS_COMPOSITION_START;
  compEvent.eventStructType = NS_COMPOSITION_START;
  compEvent.compositionMessage = NS_COMPOSITION_START;
  aWin->DispatchEvent(&compEvent, *aStatus);

  return PR_TRUE;
}

static gint composition_draw(GdkEventKey *aEvent, nsWindow *aWin,
                             nsIUnicodeDecoder *aDecoder,
                             nsEventStatus *aStatus) {
  if (!aWin->mIMECompositionUniString) {
    aWin->mIMECompositionUniStringSize = 128;
    aWin->mIMECompositionUniString =
      new PRUnichar[aWin->mIMECompositionUniStringSize];
  }
  PRUnichar *uniChar;
  PRInt32 uniCharSize;
  PRInt32 srcLen = aEvent->length;
  for (;;) {
    uniChar = aWin->mIMECompositionUniString;
    uniCharSize = aWin->mIMECompositionUniStringSize - 1;
    aDecoder->Convert((char*)aEvent->string, uniChar, &uniCharSize,
                      &srcLen);
    if (srcLen == aEvent->length &&
        uniCharSize < aWin->mIMECompositionUniStringSize - 1) {
      break;
    }
    aWin->mIMECompositionUniStringSize += 32;
    aWin->mIMECompositionUniString =
      new PRUnichar[aWin->mIMECompositionUniStringSize];
  }
  aWin->mIMECompositionUniString[uniCharSize] = 0;

  nsTextEvent textEvent;
  textEvent.message = NS_TEXT_EVENT;
  textEvent.widget = (nsWidget*)aWin;
  textEvent.time = aEvent->time;
  textEvent.point.x = 0;
  textEvent.point.y = 0;
  textEvent.theText = aWin->mIMECompositionUniString;
  textEvent.rangeCount = 0;
  textEvent.rangeArray = nsnull;
  textEvent.isShift = (aEvent->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
  textEvent.isControl = (aEvent->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
  textEvent.isAlt = (aEvent->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
  textEvent.eventStructType = NS_TEXT_EVENT;
  aWin->DispatchEvent(&textEvent, *aStatus);

  aWin->SetXICSpotLocation(textEvent.theReply.mCursorPosition);
  return True;
}

static gint composition_end(GdkEventKey *aEvent, nsWindow *aWin,
                            nsEventStatus *aStatus) {
  nsCompositionEvent compEvent;

  compEvent.widget = (nsWidget*)aWin;
  compEvent.point.x = 0;
  compEvent.point.y = 0;
  compEvent.time = aEvent->time;
  compEvent.message = NS_COMPOSITION_END;
  compEvent.eventStructType = NS_COMPOSITION_END;
  compEvent.compositionMessage = NS_COMPOSITION_END;
  aWin->DispatchEvent(&compEvent, *aStatus);

  return PR_TRUE;
}

static nsIUnicodeDecoder*
open_unicode_decoder(void) {
  nsresult result;
  nsIUnicodeDecoder *decoder = nsnull;
  NS_WITH_SERVICE(nsIPlatformCharset, platform, NS_PLATFORMCHARSET_PROGID,
                  &result);
  if (platform && NS_SUCCEEDED(result)) {
    nsAutoString charset("");
    result = platform->GetCharset(kPlatformCharsetSel_Menu, charset);
    if (NS_FAILED(result) || (charset.Length() == 0)) {
      charset = "ISO-8859-1";   // default
    }
    nsICharsetConverterManager* manager = nsnull;
    nsresult res = nsServiceManager::
      GetService(kCharsetConverterManagerCID,
                 nsCOMTypeInfo<nsICharsetConverterManager>::GetIID(),
                 (nsISupports**)&manager);
    if (NS_SUCCEEDED(res)) {
      manager->GetUnicodeDecoder(&charset, &decoder);
      nsServiceManager::ReleaseService(kCharsetConverterManagerCID, manager);
    }
  }
  return decoder;
}

gint handle_key_press_event_for_text(GtkWidget *w, GdkEventKey* event,
                                     gpointer p)
{
  nsKeyEvent kevent;
  nsTextWidget* win = (nsTextWidget*)p;
  int isModifier;

  // work around for annoying things.
  if (event->keyval == GDK_Tab)
    if (event->state & GDK_CONTROL_MASK)
      if (event->state & GDK_MOD1_MASK)
        return PR_FALSE;

  // Don't pass shift, control and alt as key press events
  if (event->keyval == GDK_Shift_L
      || event->keyval == GDK_Shift_R
      || event->keyval == GDK_Control_L
      || event->keyval == GDK_Control_R)
    return PR_TRUE;

  win->AddRef();
  InitKeyEvent(event, p, kevent, NS_KEY_DOWN);
  win->OnKey(kevent);

  //
  // Second, dispatch the Key event as a key press event w/ a Unicode
  //  character code.  Note we have to check for modifier keys, since
  // gtk returns a character value for them
  //
  isModifier = event->state &(GDK_CONTROL_MASK|GDK_MOD1_MASK);
#ifdef DEBUG_EVENTS
  if (isModifier) printf("isModifier\n");
#endif
#ifdef USE_XIM
  if (event->length || !isModifier) {
    static nsIUnicodeDecoder *decoder = nsnull;
    if (!decoder) {
      decoder = open_unicode_decoder();
    }
    if (decoder) {
      nsEventStatus status;
      composition_start(event, (nsWindow*) win, &status);
      composition_draw(event, (nsWindow*) win, decoder, &status);
      composition_end(event, (nsWindow*) win, &status);
    } else {
      InitKeyPressEvent(event,p, kevent);
      win->OnKey(kevent);
    }
  }
#else
  if (event->length && !isModifier) {
     InitKeyPressEvent(event,p,kevent);
     win->OnKey(kevent);
  }
#endif
  win->Release();
  gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_press_event");

  return PR_TRUE;
}

gint handle_key_release_event_for_text(GtkWidget *w, GdkEventKey* event,
                                       gpointer p)
{
  nsKeyEvent kevent;
  nsTextWidget* win = (nsTextWidget*)p;

  // Don't pass shift, control and alt as key release events
  if (event->keyval == GDK_Shift_L
      || event->keyval == GDK_Shift_R
      || event->keyval == GDK_Control_L
      || event->keyval == GDK_Control_R)
    return PR_TRUE;

  InitKeyEvent(event, p, kevent, NS_KEY_UP);
  win->AddRef();
  win->OnKey(kevent);
  win->Release();

  gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_release_event");

  return PR_TRUE;
}

#endif

//==============================================================
gint handle_key_press_event(GtkWidget *w, GdkEventKey* event, gpointer p)
{
  nsKeyEvent kevent;
  nsWindow* win = (nsWindow*)p;

  // work around for annoying things.
  if (event->keyval == GDK_Tab)
    if (event->state & GDK_CONTROL_MASK)
      if (event->state & GDK_MOD1_MASK)
        return PR_FALSE;

  // Don't pass shift, control and alt as key press events
  if (event->keyval == GDK_Shift_L
      || event->keyval == GDK_Shift_R
      || event->keyval == GDK_Control_L
      || event->keyval == GDK_Control_R)
    return PR_TRUE;

  win->AddRef();
  //
  // First, dispatch the Key event as a virtual key down event
  //
  InitKeyEvent(event, p, kevent, NS_KEY_DOWN);
  win->OnKey(kevent);
 


  //
  // Second, dispatch the Key event as a key press event w/ a Unicode
  //  character code.  Note we have to check for modifier keys, since
  // gtk returns a character value for them
  //
  if (event->length) {
     InitKeyPressEvent(event,p,kevent);
     win->OnKey(kevent);
   }

  win->Release();
  gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_press_event");
  
  return PR_TRUE;
}
#else
//==============================================================
gint handle_key_release_event(GtkWidget *w, GdkEventKey* event, gpointer p)
{
  // Don't pass shift, control and alt as key release events
  if (event->keyval == GDK_Shift_L
      || event->keyval == GDK_Shift_R
      || event->keyval == GDK_Control_L
      || event->keyval == GDK_Control_R)
    return PR_TRUE;

  nsKeyEvent kevent;
  InitKeyEvent(event, p, kevent, NS_KEY_UP);

  nsWindow * win = (nsWindow *) p;
  win->AddRef();
  win->OnKey(kevent);
  win->Release();

  gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_release_event");

  return PR_TRUE;
}


//==============================================================
gint handle_key_press_event(GtkWidget *w, GdkEventKey* event, gpointer p)
{
  // Don't pass shift, control and alt as key press events
  if (event->keyval == GDK_Shift_L
      || event->keyval == GDK_Shift_R
      || event->keyval == GDK_Control_L
      || event->keyval == GDK_Control_R)
    return PR_TRUE;

  nsKeyEvent kevent;
  InitKeyEvent(event, p, kevent, NS_KEY_DOWN);

  nsWindow * win = (nsWindow *) p;
  win->AddRef();
  win->OnKey(kevent);
  win->Release();

  gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_press_event");
  
  return PR_TRUE;
}
#endif

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
