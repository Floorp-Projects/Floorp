/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsWidget.h"
#include "nsWindow.h"

#include "nsScrollbar.h"
#include "nsIFileWidget.h"
#include "nsGUIEvent.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"

#include "nsTextWidget.h"
#include "nsGtkIMEHelper.h"


#include "stdio.h"
#include "ctype.h"

#include "gtk/gtk.h"
#include "nsGtkEventHandler.h"

#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef DEBUG_pavlov
//#define DEBUG_EVENTS 1
#endif

struct EventInfo {
  nsWidget *widget;  // the widget
  nsRect   *rect;    // the rect
};

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

struct nsKeyConverter {
  int vkCode; // Platform independent key code
  int keysym; // GDK keysym key code
};

//
// Netscape keycodes are defined in widget/public/nsGUIEvent.h
// GTK keycodes are defined in <gdk/gdkkeysyms.h>
//
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

  // NS doesn't have dash or equals distinct from the numeric keypad ones,
  // so we'll use those for now.  See bug 17008:
  { NS_VK_SUBTRACT, GDK_minus },
  { NS_VK_EQUALS, GDK_equal },
  // and we don't have a single-quote symbol either:
  { NS_VK_QUOTE, GDK_apostrophe },

  { NS_VK_COMMA,      GDK_comma },
  { NS_VK_PERIOD,     GDK_period },
  { NS_VK_SLASH,      GDK_slash },
  { NS_VK_BACK_SLASH, GDK_backslash },
  { NS_VK_BACK_QUOTE, GDK_grave },
  { NS_VK_OPEN_BRACKET, GDK_bracketleft },
  { NS_VK_CLOSE_BRACKET, GDK_bracketright },
  { NS_VK_QUOTE, GDK_quotedbl },

  // Some shifted keys, see bug 15463.
  // These should be subject to different keyboard mappings;
  // how do we do that in gtk?
  { NS_VK_SEMICOLON, GDK_colon },
  { NS_VK_BACK_QUOTE, GDK_asciitilde },
  { NS_VK_COMMA, GDK_less },
  { NS_VK_PERIOD, GDK_greater },
  { NS_VK_SLASH,      GDK_question },
  { NS_VK_1, GDK_exclam },
  { NS_VK_2, GDK_at },
  { NS_VK_3, GDK_numbersign },
  { NS_VK_4, GDK_dollar },
  { NS_VK_5, GDK_percent },
  { NS_VK_6, GDK_asciicircum },
  { NS_VK_7, GDK_ampersand },
  { NS_VK_8, GDK_asterisk },
  { NS_VK_9, GDK_parenleft },
  { NS_VK_0, GDK_parenright },
  { NS_VK_SUBTRACT, GDK_underscore },
  { NS_VK_EQUALS, GDK_plus }
};

void nsGtkWidget_InitNSKeyEvent(int aEventType, nsKeyEvent& aKeyEvent,
                                GtkWidget *w, gpointer p, GdkEventKey * event);

//==============================================================

// Input keysym is in gtk format; output is in NS_VK format
int nsPlatformToDOMKeyCode(GdkEventKey *aGEK)
{
  int i;
  int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);

  int keysym = aGEK->keyval;

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

#if defined(DEBUG_akkana) || defined(DEBUG_ftang)
  printf("No match in nsPlatformToDOMKeyCode: keysym is 0x%x, string is %s\n", keysym, aGEK->string);
#endif

  return((int)0);
}

//==============================================================

PRUint32 nsConvertCharCodeToUnicode(GdkEventKey* aGEK)
{
  // For control chars, GDK sets string to be the actual ascii value.
  // Map that to what nsKeyEvent wants, which currently --
  // TEMPORARILY (the spec has changed and will be switched over
  // when the tree opens for M11) --
  // is the ascii for the actual event (e.g. 1 for control-a).
  // This is only true for control chars; for alt chars, send the
  // ascii for the key, i.e. a for alt-a.
  if (aGEK->state & GDK_CONTROL_MASK)
  {
    if (aGEK->state & GDK_SHIFT_MASK)
      return aGEK->string[0] + 'A' - 1;
    else
      return aGEK->string[0] + 'a' - 1;
  }

  // For now (obviously this will need to change for IME),
  // only set a char code if the result is printable:
  if (!isprint(aGEK->string[0]))
    return 0;

  // ALT keys in gdk give the upper case character in string,
  // but we want the lower case char in char code
  // unless shift was also pressed.
  if (((aGEK->state & GDK_MOD1_MASK))
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
    anEvent.keyCode = nsPlatformToDOMKeyCode(aGEK);
    anEvent.charCode = 0;
    anEvent.time = aGEK->time;
    anEvent.isShift = (aGEK->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGEK->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGEK->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    // XXX
    anEvent.isMeta = PR_FALSE; //(aGEK->state & GDK_MOD2_MASK) ? PR_TRUE : PR_FALSE;
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

  if (aGEK!=nsnull)
  {
    anEvent.isShift = (aGEK->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGEK->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGEK->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    // XXX
    anEvent.isMeta = PR_FALSE; //(aGEK->state & GDK_MOD2_MASK) ? PR_TRUE : PR_FALSE;

    if(aGEK->length)
       anEvent.charCode = nsConvertCharCodeToUnicode(aGEK);
    else 
       anEvent.charCode = 0;

    if (anEvent.charCode) {
      anEvent.keyCode = 0;
      anEvent.isShift = PR_FALSE;
    } else
      anEvent.keyCode = nsPlatformToDOMKeyCode(aGEK);

#if defined(DEBUG_akkana) || defined(DEBUG_pavlov) || defined (DEBUG_ftang)
    printf("Key Press event: keyCode = 0x%x, char code = '%c'",
           anEvent.keyCode, anEvent.charCode);
    if (anEvent.isShift)
      printf(" [shift]");
    if (anEvent.isControl)
      printf(" [ctrl]");
    if (anEvent.isAlt)
      printf(" [alt]");
    if (anEvent.isMeta)
      printf(" [meta]");
    printf("\n");
#endif

    anEvent.time = aGEK->time;
    anEvent.point.x = 0;
    anEvent.point.y = 0;
  }
}

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

gint handle_expose_event(GtkWidget *w, GdkEventExpose *event, gpointer p)
{
  if (event->type == GDK_NO_EXPOSE)
    return PR_FALSE;

  nsPaintEvent pevent;
  InitExposeEvent(event, p, pevent, NS_PAINT);

  nsWindow *win = (nsWindow *)p;
  win->AddRef();
  win->OnExpose(pevent);
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

  // set SpotLocation
  aWin->SetXICSpotLocation(compEvent.theReply.mCursorPosition);

  return PR_TRUE;
}

static gint composition_draw(GdkEventKey *aEvent, nsWindow *aWin,
                             nsEventStatus *aStatus) {
  nsresult res= NS_OK;
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
    res = nsGtkIMEHelper::GetSingleton()->ConvertToUnicode(
            (char*)aEvent->string, &srcLen, uniChar, &uniCharSize);
    if(NS_ERROR_ABORT == res)
      return FALSE;
    if (srcLen == aEvent->length &&
        uniCharSize < aWin->mIMECompositionUniStringSize - 1) {
      break;
    }
    aWin->mIMECompositionUniStringSize += 32;
    if(aWin->mIMECompositionUniString)
	delete [] aWin->mIMECompositionUniString;
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
  // XXX
  textEvent.isMeta = PR_FALSE; //(aEvent->state & GDK_MOD2_MASK) ? PR_TRUE : PR_FALSE;
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

// GTK's text widget already does XIM, so we don't want to do this again
gint handle_key_press_event_for_text(GtkObject *w, GdkEventKey* event,
                                     gpointer p)
{
  nsKeyEvent kevent;
  nsTextWidget* win = (nsTextWidget*)p;

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
  InitKeyPressEvent(event,p, kevent);
  win->OnKey(kevent);

  win->Release();
  if (w)
  {
    gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_press_event");
  }

  return PR_TRUE;
}

// GTK's text widget already does XIM, so we don't want to do this again
gint handle_key_release_event_for_text(GtkObject *w, GdkEventKey* event,
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
  
  if (w)
  {
    gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_release_event");
  }

  return PR_TRUE;
}


//==============================================================
gint handle_key_press_event(GtkObject *w, GdkEventKey* event, gpointer p)
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
    if (nsGtkIMEHelper::GetSingleton() && (!kevent.keyCode)) {
      nsEventStatus status;
      composition_start(event, win, &status);
      composition_draw(event, win, &status);
      composition_end(event, win, &status);
    } else {
      InitKeyPressEvent(event,p, kevent);
      win->OnKey(kevent);

#if 0 // this will break editor Undo/Redo Text Txn system
      nsEventStatus status;
      composition_start(event, win, &status);
      composition_end(event, win, &status);
#endif

    }
  } else { // for Home/End/Up/Down/Left/Right/PageUp/PageDown key
    InitKeyPressEvent(event,p, kevent);
    win->OnKey(kevent);
  }

  win->Release();
  if (w)
  {
    gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_press_event");
  }

  return PR_TRUE;
}

//==============================================================
gint handle_key_release_event(GtkObject *w, GdkEventKey* event, gpointer p)
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

  if (w)
  {
    gtk_signal_emit_stop_by_name (GTK_OBJECT(w), "key_release_event");
  }

  return PR_TRUE;
}

//==============================================================
void 
handle_gdk_event (GdkEvent *event, gpointer data)
{
  GtkObject *object = nsnull;

  if (event->any.window)
    gdk_window_get_user_data (event->any.window, (void **)&object);

  if (object != nsnull &&
      GDK_IS_SUPERWIN (object))
    {
      // It was an event on one of our superwindows

      nsWindow *window = (nsWindow *)gtk_object_get_data (object, "nsWindow");
      GtkWidget *current_grab = gtk_grab_get_current();
      if (window->GrabInProgress()) {
        goto handle_as_superwin;
      }

      if (current_grab) {
        // walk up the list of our parents looking for the widget.
        // if it's there, then don't rewrite the event
        GtkWidget *this_widget = GTK_WIDGET(window->GetMozArea());
        while (this_widget) {
          // if this widget is the grab widget then let the event pass
          // through as a superwin.  this takes care of
          // superwin windows inside of a modal window.
          if (this_widget == current_grab)
            goto handle_as_superwin;
          this_widget = this_widget->parent;
        }
        // A GTK+ grab is in effect. Rewrite the event to point to
        // our toplevel, and pass it through.
        // XXX: We should actually translate the coordinates
        
        gdk_window_unref (event->any.window);
        event->any.window = GTK_WIDGET (window->GetMozArea())->window;
        gdk_window_ref (event->any.window);
      }
      else
        {
          // Handle it ourselves.
        handle_as_superwin:
          switch (event->type)
            {
            case GDK_KEY_PRESS:
              handle_key_press_event (NULL, &event->key, window);
              break;
            case GDK_KEY_RELEASE:
              handle_key_release_event (NULL, &event->key, window);
              break;
            default:
              window->HandleEvent (event);
            }
          return;
        }
    }

  gtk_main_do_event (event);
}

//==============================================================
void
handle_xlib_shell_event(GdkSuperWin *superwin, XEvent *event, gpointer p)
{
  nsWindow *window = (nsWindow *)p;
  switch(event->xany.type) {
  case ConfigureNotify:
    window->HandleXlibConfigureNotifyEvent(event);
    break;
  default:
    break;
  }
}

//==============================================================
void
handle_xlib_bin_event(GdkSuperWin *superwin, XEvent *event, gpointer p)
{
  nsWindow *window = (nsWindow *)p;

  switch(event->xany.type) {
  case Expose:
    window->HandleXlibExposeEvent(event);
    break;
  case ButtonPress:
  case ButtonRelease:
    window->HandleXlibButtonEvent((XButtonEvent *)event);
    break;
  case MotionNotify:
    window->HandleXlibMotionNotifyEvent((XMotionEvent *) event);
    break;
  case EnterNotify:
  case LeaveNotify:
    window->HandleXlibCrossingEvent((XCrossingEvent *) event);
    break;
  default:
    break;
  }
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
