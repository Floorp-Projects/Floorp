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

#include "nsKeyCode.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "nsGUIEvent.h" // For NS_VK

struct nsKeyConverter 
{
  int          vkCode; // Platform independent key code
  unsigned int keysym; // XK keysym key code
};

#if 0
struct nsKeyConverter nsKeycodes[] = 
{
  { NS_VK_CANCEL,     XK_Cancel },
  { NS_VK_BACK,       XK_BackSpace },
  { NS_VK_TAB,        XK_Tab },
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

  { NS_VK_MULTIPLY,   XK_KP_Multiply },
  { NS_VK_ADD,        XK_KP_Add },
  { NS_VK_SEPARATOR,  XK_KP_Separator },
  { NS_VK_SUBTRACT,   XK_KP_Subtract },
  { NS_VK_DECIMAL,    XK_KP_Decimal },
  { NS_VK_DIVIDE,     XK_KP_Divide },
  { NS_VK_RETURN,     XK_KP_Enter },

  { NS_VK_COMMA,      XK_comma },
  { NS_VK_PERIOD,     XK_period },
  { NS_VK_SLASH,      XK_slash },
//XXX: How do you get a BACK_QUOTE?  NS_VK_BACK_QUOTE, XK_backquote,
  { NS_VK_OPEN_BRACKET, XK_bracketleft },
  { NS_VK_CLOSE_BRACKET, XK_bracketright },
  { NS_VK_QUOTE, XK_quotedbl }
};
#else
struct nsKeyConverter nsKeycodes[] = {
  { NS_VK_CANCEL,     XK_Cancel },
  { NS_VK_BACK,       XK_BackSpace },
  { NS_VK_TAB,        XK_Tab },
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
#endif

int
nsKeyCode::ConvertKeySymToVirtualKey(KeySym keysym)
{
#if 0
  unsigned int i;
  unsigned int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);
  
  // First, try to handle alphanumeric input, not listed in nsKeycodes:
  // most likely, more letters will be getting typed in than things in
  // the key list, so we will look through these first.

  // since X has different key symbols for upper and lowercase letters and
  // mozilla does not, convert gdk's to mozilla's
  if (keysym >= XK_a && keysym <= XK_z)
    return keysym - XK_a + NS_VK_A;
  if (keysym >= XK_A && keysym <= XK_Z)
    return keysym - XK_A + NS_VK_A;

  // numbers
  if (keysym >= XK_0 && keysym <= XK_9)
    return keysym - XK_0 + NS_VK_0;

  // keypad numbers
  if (keysym >= XK_KP_0 && keysym <= XK_KP_9)
    return keysym - XK_KP_0 + NS_VK_NUMPAD0;

  // misc other things
  for (i = 0; i < length; i++) {
    if (nsKeycodes[i].keysym == keysym)
      return(nsKeycodes[i].vkCode);
  }

  // function keys
  if (keysym >= XK_F1 && keysym <= XK_F24)
    return keysym - XK_F1 + NS_VK_F1;

  return (KeySym) 0;
#else

  unsigned int i;
  unsigned int length = sizeof(nsKeycodes) / sizeof(struct nsKeyConverter);

  for (i = 0; i < length; i++) 
  {
    if (nsKeycodes[i].keysym == keysym)
    {
      return(nsKeycodes[i].vkCode);
    }
  }
  
  return((int)keysym);
#endif
}
