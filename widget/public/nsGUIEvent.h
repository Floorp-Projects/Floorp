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
class nsIMenuItem;

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
 * General event
 */

struct nsEvent {
  /// See event struct types
  PRUint8     eventStructType;
  /// See GUI MESSAGES,
  PRUint32    message;              
  /// in widget relative coordinates
  nsPoint     point;               
  /// elapsed time, in milliseconds, from the time the system was started to the time the message was created
  PRUint32    time;                                                
};

/**
 * General graphic user interface event
 */

struct nsGUIEvent : public nsEvent {
                /// Originator of the event
  nsIWidget*  widget;           
                /// Internal platform specific message.
  void*     nativeMsg;        
};

/**
 * Window resize event
 */

struct nsSizeEvent : public nsGUIEvent {
                /// x,y width, height in pixels (client area)
    nsRect          *windowSize;    
                /// width of entire window (in pixels)
    PRInt32         mWinWidth;    
                /// height of entire window (in pixels)
    PRInt32         mWinHeight;    
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


struct nsInputEvent : public nsGUIEvent {
                /// PR_TRUE indicates the shift key in down
    PRBool          isShift;        
                /// PR_TRUE indicates the control key in down
    PRBool          isControl;      
                /// PR_TRUE indicates the alt key in down
    PRBool          isAlt;          
#ifdef XP_MAC
                /// PR_TRUE indicates the command key in down
                /// For now, it's only used in Widget: not for export
                /// in nsIDOMEvent.h or nsJSEvent.cpp (later maybe)
    PRBool          isCommand;          
#endif
};

/**
 * Mouse event
 */

struct nsMouseEvent : public nsInputEvent {
                /// The number of mouse clicks
    PRUint32        clickCount;          
};

/**
 * Keyboard event
 */

struct nsKeyEvent : public nsInputEvent {
                /// see NS_VK codes
    PRUint32        keyCode;   
                /// OS translated Unicode char
    PRUint32        charCode;
};

/**
 * Tooltip event
 */

struct nsTextEvent : public nsInputEvent {
	PRUnichar*			theText;
	PRBool				commitText;
};

struct nsTooltipEvent : public nsGUIEvent {
                /// Index of tooltip area which generated the event. @see SetTooltips in nsIWidget
    PRUint32        tipIndex;           
};

/**
 * MenuItem event
 * 
 * When this event occurs the widget field in nsGUIEvent holds the "target"
 * for the event
 */

struct nsMenuEvent : public nsGUIEvent {
  nsIMenuItem * mMenuItem;
  PRUint32      mCommand;           
};

/**
 * Event status for D&D Event
 */

enum nsDragDropEventStatus {  
    /// The event is a enter
  nsDragDropEventStatus_eDragEntered,            
    /// The event is exit
  nsDragDropEventStatus_eDragExited, 
    /// The event is drop
  nsDragDropEventStatus_eDrop  
};


/**
 * Drag & Drop event
 * 
 * When this event occurs the widget field in nsGUIEvent holds the "target"
 * for the event
 */

struct nsDragDropEvent : public nsGUIEvent {
  nsDragDropEventStatus mType;
  PRBool                mIsFileURL;
  PRUnichar *           mURL;           
};

/**
 * Event Struct Types
 */
#define NS_EVENT            1
#define NS_GUI_EVENT        2
#define NS_SIZE_EVENT       3
#define NS_PAINT_EVENT      4
#define NS_SCROLLBAR_EVENT  5
#define NS_INPUT_EVENT      6
#define NS_KEY_EVENT        7
#define NS_MOUSE_EVENT      8
#define NS_TOOLTIP_EVENT    9
#define NS_MENU_EVENT       10
#define NS_DRAGDROP_EVENT   11
#define NS_TEXT_EVENT		12
 
 /**
 * GUI MESSAGES
 */
 //@{

#define NS_WINDOW_START                 100

// Widget is being created
#define NS_CREATE                       (NS_WINDOW_START)
// Widget is being destroyed
#define NS_DESTROY                      (NS_WINDOW_START + 1)
// Widget was resized
#define NS_SIZE                         (NS_WINDOW_START + 2)
// Widget gained focus
#define NS_GOTFOCUS                     (NS_WINDOW_START + 3)
// Widget lost focus
#define NS_LOSTFOCUS                    (NS_WINDOW_START + 4)
// Widget needs to be repainted
#define NS_PAINT                        (NS_WINDOW_START + 30)
// Key is pressed within a window
#define NS_KEY_PRESS                    (NS_WINDOW_START + 31)
// Key is released within a window
#define NS_KEY_UP                       (NS_WINDOW_START + 32)
// Key is pressed within a window
#define NS_KEY_DOWN                     (NS_WINDOW_START + 33)
// Window has been moved to a new location.
// The events point contains the x, y location in screen coordinates
#define NS_MOVE                         (NS_WINDOW_START + 34) 

// Tab control's selected tab has changed
#define NS_TABCHANGE                    (NS_WINDOW_START + 35)

// Tooltip should be shown
#define NS_SHOW_TOOLTIP                 (NS_WINDOW_START + 36)
// Tooltip should be hidden
#define NS_HIDE_TOOLTIP                 (NS_WINDOW_START + 37)
// Menu item selected
#define NS_MENU_SELECTED                (NS_WINDOW_START + 38)


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
#define NS_MOUSE_LEFT_DOUBLECLICK       (NS_MOUSE_MESSAGE_START + 24)
#define NS_MOUSE_MIDDLE_DOUBLECLICK     (NS_MOUSE_MESSAGE_START + 25)
#define NS_MOUSE_RIGHT_DOUBLECLICK      (NS_MOUSE_MESSAGE_START + 26)
#define NS_MOUSE_LEFT_CLICK             (NS_MOUSE_MESSAGE_START + 27)
#define NS_MOUSE_MIDDLE_CLICK           (NS_MOUSE_MESSAGE_START + 28)
#define NS_MOUSE_RIGHT_CLICK            (NS_MOUSE_MESSAGE_START + 29)

#define NS_SCROLLBAR_MESSAGE_START      1000
#define NS_SCROLLBAR_POS                (NS_SCROLLBAR_MESSAGE_START)
#define NS_SCROLLBAR_PAGE_NEXT          (NS_SCROLLBAR_MESSAGE_START + 1)
#define NS_SCROLLBAR_PAGE_PREV          (NS_SCROLLBAR_MESSAGE_START + 2)
#define NS_SCROLLBAR_LINE_NEXT          (NS_SCROLLBAR_MESSAGE_START + 3)
#define NS_SCROLLBAR_LINE_PREV          (NS_SCROLLBAR_MESSAGE_START + 4)

#define NS_STREAM_EVENT_START           1100
#define NS_PAGE_LOAD                    (NS_STREAM_EVENT_START)
#define NS_PAGE_UNLOAD                  (NS_STREAM_EVENT_START + 1)
#define NS_IMAGE_LOAD                   (NS_STREAM_EVENT_START + 2)
#define NS_IMAGE_ABORT                  (NS_STREAM_EVENT_START + 3)
#define NS_IMAGE_ERROR                  (NS_STREAM_EVENT_START + 4)

#define NS_FORM_EVENT_START             1200
#define NS_FORM_SUBMIT                  (NS_FORM_EVENT_START)
#define NS_FORM_RESET                   (NS_FORM_EVENT_START + 1)
#define NS_FORM_CHANGE                  (NS_FORM_EVENT_START + 2)

//Need separate focus/blur notifications for non-native widgets
#define NS_FOCUS_EVENT_START            1300
#define NS_FOCUS_CONTENT                (NS_FOCUS_EVENT_START)
#define NS_BLUR_CONTENT                 (NS_FOCUS_EVENT_START + 1)

//@}


#define NS_IS_MOUSE_EVENT(evnt) \
       (((evnt)->message == NS_MOUSE_LEFT_BUTTON_DOWN) || \
        ((evnt)->message == NS_MOUSE_LEFT_BUTTON_UP) || \
        ((evnt)->message == NS_MOUSE_MIDDLE_BUTTON_DOWN) || \
        ((evnt)->message == NS_MOUSE_MIDDLE_BUTTON_UP) || \
        ((evnt)->message == NS_MOUSE_RIGHT_BUTTON_DOWN) || \
        ((evnt)->message == NS_MOUSE_RIGHT_BUTTON_UP) || \
        ((evnt)->message == NS_MOUSE_ENTER) || \
        ((evnt)->message == NS_MOUSE_EXIT) || \
        ((evnt)->message == NS_MOUSE_MOVE))

#define NS_IS_KEY_EVENT(evnt) \
       (((evnt)->message == NS_KEY_DOWN) ||  \
        ((evnt)->message == NS_KEY_UP))

/*
 * Virtual key bindings for keyboard events
 * NOTE: These are repeated in nsIDOMEvent.h and must be kept in sync
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

#define NS_EVENT_FLAG_NONE          0x0000
#define NS_EVENT_FLAG_INIT          0x0001
#define NS_EVENT_FLAG_BUBBLE        0x0002
#define NS_EVENT_FLAG_CAPTURE       0x0004
#define NS_EVENT_FLAG_POST_PROCESS  0x0008

#endif // nsGUIEvent_h__

