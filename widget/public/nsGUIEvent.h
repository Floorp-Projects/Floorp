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

#ifndef nsGUIEvent_h__
#define nsGUIEvent_h__

#include "nsPoint.h"
#include "nsRect.h"

class nsIRenderingContext;
class nsIWidget;

/**
 * Return status for event processors.
 */

enum nsEventStatus {  
    /// The event is ignored, do default processing
  nsEventStatus_eIgnore,            
    /// The event is consumed, don't do default processing
  nsEventStatus_eConsumeNoDefault, 
    /// The event is consumed, but do default processing
  nsEventStatus_eConsumeDoDefault  
};

/**
 * General graphic user interface event
 */

struct nsGUIEvent {
                /// See GUI MESSAGES,
  PRUint32    message;              
                /// Originator of the event
  nsIWidget*  widget;           
                /// in widget relative coordinates
  nsPoint     point;               
                /// elapsed time, in milliseconds, from the time the system was started to the time the message was created
  PRUint32    time;                                                
                /// Internal platform specific message.
  void*     nativeMsg;        
};

/**
 * Window resize event
 */

struct nsSizeEvent : public nsGUIEvent {
                /// x,y width, height in pixels
    nsRect          *windowSize;    
};

/**
 * Window repaint event
 */

struct nsPaintEvent : public nsGUIEvent {
                /// Context to paint in.
    nsIRenderingContext *renderingContext;
                /// x,y, width, height in pixels of area to paint
    nsRect              *rect;      
};

/**
 * Scrollbar event
 */

struct nsScrollbarEvent : public nsGUIEvent {
                /// ranges between scrollbar 0 and (maxRange - thumbSize). See nsIScrollbar
    PRUint32        position; 
};


/**
 * Keyboard event
 */

struct nsKeyEvent : public nsGUIEvent {
                /// see NS_VK codes
    PRUint32        keyCode;   
                /// PR_TRUE indicates the shift key in down
    PRBool          isShift;        
                /// PR_TRUE indicates the control key in down
    PRBool          isControl;      
                /// PR_TRUE indicates the alt key in down
    PRBool          isAlt;          
};

/**
 * GUI MESSAGES
 */
 //@{

#define NS_WINDOW_START                 100

// Sent when window is created
#define NS_CREATE                       (NS_WINDOW_START)
// Sent when window is destroyed
#define NS_DESTROY                      (NS_WINDOW_START + 1)
// Sent when the window is resized
#define NS_SIZE                         (NS_WINDOW_START + 2)
// Sent when window gains focus
#define NS_GOTFOCUS                     (NS_WINDOW_START + 3)
// Sent when window loses focus
#define NS_LOSTFOCUS                    (NS_WINDOW_START + 4)
// Sent when the window needs to be repainted
#define NS_PAINT                        (NS_WINDOW_START + 30)
// Sent when a key is pressed down within a window
#define NS_KEY_UP                       (NS_WINDOW_START + 32)
// Sent when a key is released within a window
#define NS_KEY_DOWN                     (NS_WINDOW_START + 33)
// Sent when the window has been moved to a new location.
// The events point contains the x, y location in screen coordinates
#define NS_MOVE                         (NS_WINDOW_START + 34) 

// Sent when a tab control's selected tab has changed
#define NS_TABCHANGE                    (NS_WINDOW_START + 35)

#define NS_MOUSE_MESSAGE_START          300
#define NS_MOUSE_MOVE                   (NS_MOUSE_MESSAGE_START)
#define NS_MOUSE_LEFT_BUTTON_UP         (NS_MOUSE_MESSAGE_START + 1)
#define NS_MOUSE_LEFT_BUTTON_DOWN       (NS_MOUSE_MESSAGE_START + 2)
#define NS_MOUSE_MIDDLE_BUTTON_UP       (NS_MOUSE_MESSAGE_START + 10)
#define NS_MOUSE_MIDDLE_BUTTON_DOWN     (NS_MOUSE_MESSAGE_START + 11)
#define NS_MOUSE_RIGHT_BUTTON_UP        (NS_MOUSE_MESSAGE_START + 20)
#define NS_MOUSE_RIGHT_BUTTON_DOWN      (NS_MOUSE_MESSAGE_START + 21)
#define NS_MOUSE_ENTER                  (NS_MOUSE_MESSAGE_START + 22)
#define NS_MOUSE_EXIT                   (NS_MOUSE_MESSAGE_START + 23)

#define NS_SCROLLBAR_MESSAGE_START      1000
#define NS_SCROLLBAR_POS                (NS_SCROLLBAR_MESSAGE_START)
#define NS_SCROLLBAR_PAGE_NEXT          (NS_SCROLLBAR_MESSAGE_START + 1)
#define NS_SCROLLBAR_PAGE_PREV          (NS_SCROLLBAR_MESSAGE_START + 2)
#define NS_SCROLLBAR_LINE_NEXT          (NS_SCROLLBAR_MESSAGE_START + 3)
#define NS_SCROLLBAR_LINE_PREV          (NS_SCROLLBAR_MESSAGE_START + 4)
//@}

/*
 * Virtual key bindings for keyboard events
 */

#define NS_VK_CANCEL         0x03
#define NS_VK_BACK           0x08
#define NS_VK_TAB            0x09
#define NS_VK_CLEAR          0x0C
#define NS_VK_RETURN         0x0D
#define NS_VK_SHIFT          0x10
#define NS_VK_CONTROL        0x11
#define NS_VK_ALT            0x12
#define NS_VK_PAUSE          0x13
#define NS_VK_CAPS_LOCK      0x14
#define NS_VK_ESCAPE         0x1B
#define NS_VK_SPACE          0x20
#define NS_VK_PAGE_UP        0x21
#define NS_VK_PAGE_DOWN      0x22
#define NS_VK_END            0x23
#define NS_VK_HOME           0x24
#define NS_VK_LEFT           0x25
#define NS_VK_UP             0x26
#define NS_VK_RIGHT          0x27
#define NS_VK_DOWN           0x28
#define NS_VK_PRINTSCREEN    0x2C
#define NS_VK_INSERT         0x2D
#define NS_VK_DELETE         0x2E

// NS_VK_0 - NS_VK_9 match their ascii values
#define NS_VK_0              0x30
#define NS_VK_1              0x31
#define NS_VK_2              0x32
#define NS_VK_3              0x33
#define NS_VK_4              0x34
#define NS_VK_5              0x35
#define NS_VK_6              0x36
#define NS_VK_7              0x37
#define NS_VK_8              0x38
#define NS_VK_9              0x39

#define NS_VK_SEMICOLON      0x3B
#define NS_VK_EQUALS         0x3D

// NS_VK_A - NS_VK_Z match their ascii values
#define NS_VK_A              0x41
#define NS_VK_B              0x42
#define NS_VK_C              0x43
#define NS_VK_D              0x44
#define NS_VK_E              0x45
#define NS_VK_F              0x46
#define NS_VK_G              0x47
#define NS_VK_H              0x48
#define NS_VK_I              0x49
#define NS_VK_J              0x4A
#define NS_VK_K              0x4B
#define NS_VK_L              0x4C
#define NS_VK_M              0x4D
#define NS_VK_N              0x4E
#define NS_VK_O              0x4F
#define NS_VK_P              0x50
#define NS_VK_Q              0x51
#define NS_VK_R              0x52
#define NS_VK_S              0x53
#define NS_VK_T              0x54
#define NS_VK_U              0x55
#define NS_VK_V              0x56
#define NS_VK_W              0x57
#define NS_VK_X              0x58
#define NS_VK_Y              0x59
#define NS_VK_Z              0x5A

#define NS_VK_NUMPAD0        0x60
#define NS_VK_NUMPAD1        0x61
#define NS_VK_NUMPAD2        0x62
#define NS_VK_NUMPAD3        0x63
#define NS_VK_NUMPAD4        0x64
#define NS_VK_NUMPAD5        0x65
#define NS_VK_NUMPAD6        0x66
#define NS_VK_NUMPAD7        0x67
#define NS_VK_NUMPAD8        0x68
#define NS_VK_NUMPAD9        0x69
#define NS_VK_MULTIPLY       0x6A
#define NS_VK_ADD            0x6B
#define NS_VK_SEPARATOR      0x6C
#define NS_VK_SUBTRACT       0x6D
#define NS_VK_DECIMAL        0x6E
#define NS_VK_DIVIDE         0x6F
#define NS_VK_F1             0x70
#define NS_VK_F2             0x71
#define NS_VK_F3             0x72
#define NS_VK_F4             0x73
#define NS_VK_F5             0x74
#define NS_VK_F6             0x75
#define NS_VK_F7             0x76
#define NS_VK_F8             0x77
#define NS_VK_F9             0x78
#define NS_VK_F10            0x79
#define NS_VK_F11            0x7A
#define NS_VK_F12            0x7B
#define NS_VK_F13            0x7C
#define NS_VK_F14            0x7D
#define NS_VK_F15            0x7E
#define NS_VK_F16            0x7F
#define NS_VK_F17            0x80
#define NS_VK_F18            0x81
#define NS_VK_F19            0x82
#define NS_VK_F20            0x83
#define NS_VK_F21            0x84
#define NS_VK_F22            0x85
#define NS_VK_F23            0x86
#define NS_VK_F24            0x87

#define NS_VK_NUM_LOCK       0x90
#define NS_VK_SCROLL_LOCK    0x91

#define NS_VK_COMMA          0xBC
#define NS_VK_PERIOD         0xBE
#define NS_VK_SLASH          0xBF
#define NS_VK_BACK_QUOTE     0xC0
#define NS_VK_OPEN_BRACKET   0xDB
#define NS_VK_BACK_SLASH     0xDC
#define NS_VK_CLOSE_BRACKET  0xDD
#define NS_VK_QUOTE          0xDE


#endif // nsGUIEvent_h__

