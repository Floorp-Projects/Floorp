/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Makoto Kato  <m_kato@ga2.so-net.ne.jp>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Thomas K. Dyas <tdyas@zecador.org> (simple gestures support)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsGUIEvent_h__
#define nsGUIEvent_h__

#include "nsPoint.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "nsEvent.h"
#include "nsStringGlue.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMDataTransfer.h"
#include "nsWeakPtr.h"
#include "nsIWidget.h"
#include "nsTArray.h"
#include "nsTraceRefcnt.h"

class nsIRenderingContext;
class nsIRegion;
class nsIMenuItem;
class nsIAccessible;
class nsIContent;
class nsIURI;
class nsIDOMEvent;
class nsHashKey;

/**
 * Event Struct Types
 */
#define NS_EVENT                           1
#define NS_GUI_EVENT                       2
#define NS_SIZE_EVENT                      3
#define NS_SIZEMODE_EVENT                  4
#define NS_ZLEVEL_EVENT                    5
#define NS_PAINT_EVENT                     6
#define NS_SCROLLBAR_EVENT                 7
#define NS_INPUT_EVENT                     8
#define NS_KEY_EVENT                       9
#define NS_MOUSE_EVENT                    10
#define NS_MENU_EVENT                     11
#define NS_SCRIPT_ERROR_EVENT             12
#define NS_TEXT_EVENT                     13
#define NS_COMPOSITION_EVENT              14
#define NS_MOUSE_SCROLL_EVENT             16
#define NS_SCROLLPORT_EVENT               18
#define NS_MUTATION_EVENT                 19 // |nsMutationEvent| in content
#define NS_ACCESSIBLE_EVENT               20
#define NS_FORM_EVENT                     21
#define NS_FOCUS_EVENT                    22
#define NS_POPUP_EVENT                    23
#define NS_COMMAND_EVENT                  24
#define NS_POPUPBLOCKED_EVENT             25
#define NS_BEFORE_PAGE_UNLOAD_EVENT       26
#define NS_UI_EVENT                       27
#define NS_PAGETRANSITION_EVENT           29
#ifdef MOZ_SVG
#define NS_SVG_EVENT                      30
#define NS_SVGZOOM_EVENT                  31
#endif // MOZ_SVG
#define NS_XUL_COMMAND_EVENT              32
#define NS_QUERY_CONTENT_EVENT            33
#ifdef MOZ_MEDIA
#define NS_MEDIA_EVENT                    34
#endif // MOZ_MEDIA
#define NS_DRAG_EVENT                     35
#define NS_NOTIFYPAINT_EVENT              36
#define NS_SIMPLE_GESTURE_EVENT           37

// These flags are sort of a mess. They're sort of shared between event
// listener flags and event flags, but only some of them. You've been
// warned!
#define NS_EVENT_FLAG_NONE                0x0000
#define NS_EVENT_FLAG_TRUSTED             0x0001
#define NS_EVENT_FLAG_BUBBLE              0x0002
#define NS_EVENT_FLAG_CAPTURE             0x0004
#define NS_EVENT_FLAG_STOP_DISPATCH       0x0008
#define NS_EVENT_FLAG_NO_DEFAULT          0x0010
#define NS_EVENT_FLAG_CANT_CANCEL         0x0020
#define NS_EVENT_FLAG_CANT_BUBBLE         0x0040
#define NS_PRIV_EVENT_FLAG_SCRIPT         0x0080
#define NS_EVENT_FLAG_NO_CONTENT_DISPATCH 0x0100
#define NS_EVENT_FLAG_SYSTEM_EVENT        0x0200
// Event has been dispatched at least once
#define NS_EVENT_DISPATCHED               0x0400
#define NS_EVENT_FLAG_DISPATCHING         0x0800

#define NS_PRIV_EVENT_UNTRUSTED_PERMITTED 0x8000

#define NS_EVENT_CAPTURE_MASK             (~(NS_EVENT_FLAG_BUBBLE | NS_EVENT_FLAG_NO_CONTENT_DISPATCH))
#define NS_EVENT_BUBBLE_MASK              (~(NS_EVENT_FLAG_CAPTURE | NS_EVENT_FLAG_NO_CONTENT_DISPATCH))

#define NS_EVENT_TYPE_NULL                   0

/**
 * GUI MESSAGES
 */
 //@{
#define NS_EVENT_NULL                   0


#define NS_WINDOW_START                 100

// Widget is being created
#define NS_CREATE                       (NS_WINDOW_START)
// Widget may be destroyed
#define NS_XUL_CLOSE                    (NS_WINDOW_START + 1)
// Widget is being destroyed
#define NS_DESTROY                      (NS_WINDOW_START + 2)
// Widget was resized
#define NS_SIZE                         (NS_WINDOW_START + 3)
// Widget size mode was changed
#define NS_SIZEMODE                     (NS_WINDOW_START + 4)
// Widget gained focus
#define NS_GOTFOCUS                     (NS_WINDOW_START + 5)
// Widget lost focus
#define NS_LOSTFOCUS                    (NS_WINDOW_START + 6)
// Widget got activated
#define NS_ACTIVATE                     (NS_WINDOW_START + 7)
// Widget got deactivated
#define NS_DEACTIVATE                   (NS_WINDOW_START + 8)
// top-level window z-level change request
#define NS_SETZLEVEL                    (NS_WINDOW_START + 9)
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

#define NS_OS_TOOLBAR                   (NS_WINDOW_START + 36)

// Menu item selected
#define NS_MENU_SELECTED                (NS_WINDOW_START + 38)

// Form control changed: currently == combo box selection changed
// but could be expanded to mean textbox, checkbox changed, etc.
// This is a GUI specific event that does not necessarily correspond
// directly to a mouse click or a key press.
#define NS_CONTROL_CHANGE                (NS_WINDOW_START + 39)

// Indicates the display has changed depth
#define NS_DISPLAYCHANGED                (NS_WINDOW_START + 40)

// Indicates a theme change has occurred
#define NS_THEMECHANGED                 (NS_WINDOW_START + 41)

// Indicates a System color has changed. It is the platform
// toolkits responsibility to invalidate the window to 
// ensure that it is drawn using the current system colors.
#define NS_SYSCOLORCHANGED              (NS_WINDOW_START + 42)

#define NS_RESIZE_EVENT                 (NS_WINDOW_START + 60)
#define NS_SCROLL_EVENT                 (NS_WINDOW_START + 61)

#define NS_PLUGIN_ACTIVATE               (NS_WINDOW_START + 62)

#define NS_OFFLINE                       (NS_WINDOW_START + 63)
#define NS_ONLINE                        (NS_WINDOW_START + 64)

#define NS_MOUSE_MESSAGE_START          300
#define NS_MOUSE_MOVE                   (NS_MOUSE_MESSAGE_START)
#define NS_MOUSE_BUTTON_UP              (NS_MOUSE_MESSAGE_START + 1)
#define NS_MOUSE_BUTTON_DOWN            (NS_MOUSE_MESSAGE_START + 2)
#define NS_MOUSE_ENTER                  (NS_MOUSE_MESSAGE_START + 22)
#define NS_MOUSE_EXIT                   (NS_MOUSE_MESSAGE_START + 23)
#define NS_MOUSE_DOUBLECLICK            (NS_MOUSE_MESSAGE_START + 24)
#define NS_MOUSE_CLICK                  (NS_MOUSE_MESSAGE_START + 27)
#define NS_MOUSE_ACTIVATE               (NS_MOUSE_MESSAGE_START + 30)
#define NS_MOUSE_ENTER_SYNTH            (NS_MOUSE_MESSAGE_START + 31)
#define NS_MOUSE_EXIT_SYNTH             (NS_MOUSE_MESSAGE_START + 32)

#define NS_CONTEXTMENU_MESSAGE_START    500
#define NS_CONTEXTMENU                  (NS_CONTEXTMENU_MESSAGE_START)

#define NS_SCROLLBAR_MESSAGE_START      1000
#define NS_SCROLLBAR_POS                (NS_SCROLLBAR_MESSAGE_START)
#define NS_SCROLLBAR_PAGE_NEXT          (NS_SCROLLBAR_MESSAGE_START + 1)
#define NS_SCROLLBAR_PAGE_PREV          (NS_SCROLLBAR_MESSAGE_START + 2)
#define NS_SCROLLBAR_LINE_NEXT          (NS_SCROLLBAR_MESSAGE_START + 3)
#define NS_SCROLLBAR_LINE_PREV          (NS_SCROLLBAR_MESSAGE_START + 4)

#define NS_STREAM_EVENT_START           1100
#define NS_LOAD                         (NS_STREAM_EVENT_START)
#define NS_PAGE_UNLOAD                  (NS_STREAM_EVENT_START + 1)
#define NS_IMAGE_ABORT                  (NS_STREAM_EVENT_START + 3)
#define NS_LOAD_ERROR                   (NS_STREAM_EVENT_START + 4)
#define NS_BEFORE_PAGE_UNLOAD           (NS_STREAM_EVENT_START + 6)
#define NS_PAGE_RESTORE                 (NS_STREAM_EVENT_START + 7)
 
#define NS_FORM_EVENT_START             1200
#define NS_FORM_SUBMIT                  (NS_FORM_EVENT_START)
#define NS_FORM_RESET                   (NS_FORM_EVENT_START + 1)
#define NS_FORM_CHANGE                  (NS_FORM_EVENT_START + 2)
#define NS_FORM_SELECTED                (NS_FORM_EVENT_START + 3)
#define NS_FORM_INPUT                   (NS_FORM_EVENT_START + 4)

//Need separate focus/blur notifications for non-native widgets
#define NS_FOCUS_EVENT_START            1300
#define NS_FOCUS_CONTENT                (NS_FOCUS_EVENT_START)
#define NS_BLUR_CONTENT                 (NS_FOCUS_EVENT_START + 1)

#define NS_DRAGDROP_EVENT_START         1400
#define NS_DRAGDROP_ENTER               (NS_DRAGDROP_EVENT_START)
#define NS_DRAGDROP_OVER                (NS_DRAGDROP_EVENT_START + 1)
#define NS_DRAGDROP_EXIT                (NS_DRAGDROP_EVENT_START + 2)
#define NS_DRAGDROP_DRAGDROP            (NS_DRAGDROP_EVENT_START + 3)
#define NS_DRAGDROP_GESTURE             (NS_DRAGDROP_EVENT_START + 4)
#define NS_DRAGDROP_DRAG                (NS_DRAGDROP_EVENT_START + 5)
#define NS_DRAGDROP_END                 (NS_DRAGDROP_EVENT_START + 6)
#define NS_DRAGDROP_START               (NS_DRAGDROP_EVENT_START + 7)
#define NS_DRAGDROP_DROP                (NS_DRAGDROP_EVENT_START + 8)
#define NS_DRAGDROP_OVER_SYNTH          (NS_DRAGDROP_EVENT_START + 1)
#define NS_DRAGDROP_EXIT_SYNTH          (NS_DRAGDROP_EVENT_START + 2)
#define NS_DRAGDROP_LEAVE_SYNTH         (NS_DRAGDROP_EVENT_START + 9)

// Events for popups
#define NS_XUL_EVENT_START            1500
#define NS_XUL_POPUP_SHOWING          (NS_XUL_EVENT_START)
#define NS_XUL_POPUP_SHOWN            (NS_XUL_EVENT_START+1)
#define NS_XUL_POPUP_HIDING           (NS_XUL_EVENT_START+2)
#define NS_XUL_POPUP_HIDDEN           (NS_XUL_EVENT_START+3)
// NS_XUL_COMMAND used to be here     (NS_XUL_EVENT_START+4)
#define NS_XUL_BROADCAST              (NS_XUL_EVENT_START+5)
#define NS_XUL_COMMAND_UPDATE         (NS_XUL_EVENT_START+6)
//@}

// Scroll events
#define NS_MOUSE_SCROLL_START         1600
#define NS_MOUSE_SCROLL               (NS_MOUSE_SCROLL_START)
#define NS_MOUSE_PIXEL_SCROLL         (NS_MOUSE_SCROLL_START + 1)

#define NS_SCROLLPORT_START           1700
#define NS_SCROLLPORT_UNDERFLOW       (NS_SCROLLPORT_START)
#define NS_SCROLLPORT_OVERFLOW        (NS_SCROLLPORT_START+1)
#define NS_SCROLLPORT_OVERFLOWCHANGED (NS_SCROLLPORT_START+2)

// Mutation events defined elsewhere starting at 1800

// accessible events
#define NS_ACCESSIBLE_START           1900
#define NS_GETACCESSIBLE              (NS_ACCESSIBLE_START)

#define NS_USER_DEFINED_EVENT         2000
 
// composition events
#define NS_COMPOSITION_EVENT_START    2200
#define NS_COMPOSITION_START          (NS_COMPOSITION_EVENT_START)
#define NS_COMPOSITION_END            (NS_COMPOSITION_EVENT_START + 1)
#define NS_COMPOSITION_QUERY          (NS_COMPOSITION_EVENT_START + 2)

// text events
#define NS_TEXT_START                 2400
#define NS_TEXT_TEXT                  (NS_TEXT_START)

// UI events
#define NS_UI_EVENT_START          2500
// this is not to be confused with NS_ACTIVATE!
#define NS_UI_ACTIVATE             (NS_UI_EVENT_START)
#define NS_UI_FOCUSIN              (NS_UI_EVENT_START + 1)
#define NS_UI_FOCUSOUT             (NS_UI_EVENT_START + 2)

// pagetransition events
#define NS_PAGETRANSITION_START    2700
#define NS_PAGE_SHOW               (NS_PAGETRANSITION_START + 1)
#define NS_PAGE_HIDE               (NS_PAGETRANSITION_START + 2)

#ifdef MOZ_SVG
// SVG events
#define NS_SVG_EVENT_START              2800
#define NS_SVG_LOAD                     (NS_SVG_EVENT_START)
#define NS_SVG_UNLOAD                   (NS_SVG_EVENT_START + 1)
#define NS_SVG_ABORT                    (NS_SVG_EVENT_START + 2)
#define NS_SVG_ERROR                    (NS_SVG_EVENT_START + 3)
#define NS_SVG_RESIZE                   (NS_SVG_EVENT_START + 4)
#define NS_SVG_SCROLL                   (NS_SVG_EVENT_START + 5)

// SVG Zoom events
#define NS_SVGZOOM_EVENT_START          2900
#define NS_SVG_ZOOM                     (NS_SVGZOOM_EVENT_START)
#endif // MOZ_SVG

// XUL command events
#define NS_XULCOMMAND_EVENT_START       3000
#define NS_XUL_COMMAND                  (NS_XULCOMMAND_EVENT_START)

// Cut, copy, paste events
#define NS_CUTCOPYPASTE_EVENT_START     3100
#define NS_COPY             (NS_CUTCOPYPASTE_EVENT_START)
#define NS_CUT              (NS_CUTCOPYPASTE_EVENT_START + 1)
#define NS_PASTE            (NS_CUTCOPYPASTE_EVENT_START + 2)

// Query the content information
#define NS_QUERY_CONTENT_EVENT_START    3200
// Query for the selected text information, it return the selection offset,
// selection length and selected text.
#define NS_QUERY_SELECTED_TEXT          (NS_QUERY_CONTENT_EVENT_START)
// Query for the text content of specified range, it returns actual lengh (if
// the specified range is too long) and the text of the specified range.
#define NS_QUERY_TEXT_CONTENT           (NS_QUERY_CONTENT_EVENT_START + 1)
// Query for the character rect of nth character. If there is no character at
// the offset, the query will be failed. The offset of the result is relative
// position from the top level widget.
#define NS_QUERY_CHARACTER_RECT         (NS_QUERY_CONTENT_EVENT_START + 2)
// Query for the caret rect of nth insertion point. The offset of the result is
// relative position from the top level widget.
#define NS_QUERY_CARET_RECT             (NS_QUERY_CONTENT_EVENT_START + 3)

// Video events
#ifdef MOZ_MEDIA
#define NS_MEDIA_EVENT_START            3300
#define NS_LOADSTART           (NS_MEDIA_EVENT_START)
#define NS_PROGRESS            (NS_MEDIA_EVENT_START+1)
#define NS_LOADEDMETADATA      (NS_MEDIA_EVENT_START+2)
#define NS_LOADEDDATA          (NS_MEDIA_EVENT_START+3)
#define NS_EMPTIED             (NS_MEDIA_EVENT_START+4)
#define NS_STALLED             (NS_MEDIA_EVENT_START+5)
#define NS_PLAY                (NS_MEDIA_EVENT_START+6)
#define NS_PAUSE               (NS_MEDIA_EVENT_START+7)
#define NS_WAITING             (NS_MEDIA_EVENT_START+8)
#define NS_SEEKING             (NS_MEDIA_EVENT_START+9)
#define NS_SEEKED              (NS_MEDIA_EVENT_START+10)
#define NS_TIMEUPDATE          (NS_MEDIA_EVENT_START+11)
#define NS_ENDED               (NS_MEDIA_EVENT_START+12)
#define NS_CANPLAY             (NS_MEDIA_EVENT_START+13)
#define NS_CANPLAYTHROUGH      (NS_MEDIA_EVENT_START+14)
#define NS_RATECHANGE          (NS_MEDIA_EVENT_START+15)
#define NS_DURATIONCHANGE      (NS_MEDIA_EVENT_START+16)
#define NS_VOLUMECHANGE        (NS_MEDIA_EVENT_START+17)
#define NS_MEDIA_ABORT         (NS_MEDIA_EVENT_START+18)
#define NS_MEDIA_ERROR         (NS_MEDIA_EVENT_START+19)
#endif // MOZ_MEDIA

// paint notification events
#define NS_NOTIFYPAINT_START    3400
#define NS_AFTERPAINT           (NS_NOTIFYPAINT_START)

// Simple gesture events
#define NS_SIMPLE_GESTURE_EVENT_START    3500
#define NS_SIMPLE_GESTURE_SWIPE          (NS_SIMPLE_GESTURE_EVENT_START)
#define NS_SIMPLE_GESTURE_MAGNIFY_START  (NS_SIMPLE_GESTURE_EVENT_START+1)
#define NS_SIMPLE_GESTURE_MAGNIFY_UPDATE (NS_SIMPLE_GESTURE_EVENT_START+2)
#define NS_SIMPLE_GESTURE_MAGNIFY        (NS_SIMPLE_GESTURE_EVENT_START+3)
#define NS_SIMPLE_GESTURE_ROTATE_START   (NS_SIMPLE_GESTURE_EVENT_START+4)
#define NS_SIMPLE_GESTURE_ROTATE_UPDATE  (NS_SIMPLE_GESTURE_EVENT_START+5)
#define NS_SIMPLE_GESTURE_ROTATE         (NS_SIMPLE_GESTURE_EVENT_START+6)

/**
 * Return status for event processors, nsEventStatus, is defined in
 * nsEvent.h.
 */

/**
 * sizemode is an adjunct to widget size
 */
enum nsSizeMode {
  nsSizeMode_Normal = 0,
  nsSizeMode_Minimized,
  nsSizeMode_Maximized
};

/**
 * different types of (top-level) window z-level positioning
 */
enum nsWindowZ {
  nsWindowZTop = 0,   // on top
  nsWindowZBottom,    // on bottom
  nsWindowZRelative   // just below some specified widget
};

/**
 * General event
 */

class nsEvent
{
protected:
  nsEvent(PRBool isTrusted, PRUint32 msg, PRUint8 structType)
    : eventStructType(structType),
      message(msg),
      refPoint(0, 0),
      time(0),
      flags(isTrusted ? NS_EVENT_FLAG_TRUSTED : NS_EVENT_FLAG_NONE),
      userType(0)
  {
    MOZ_COUNT_CTOR(nsEvent);
  }

public:
  nsEvent(PRBool isTrusted, PRUint32 msg)
    : eventStructType(NS_EVENT),
      message(msg),
      refPoint(0, 0),
      time(0),
      flags(isTrusted ? NS_EVENT_FLAG_TRUSTED : NS_EVENT_FLAG_NONE),
      userType(0)
  {
    MOZ_COUNT_CTOR(nsEvent);
  }

  ~nsEvent()
  {
    MOZ_COUNT_DTOR(nsEvent);
  }

  // See event struct types
  PRUint8     eventStructType;
  // See GUI MESSAGES,
  PRUint32    message;
  // In widget relative coordinates, not modified by layout code.
  nsPoint     refPoint;
  // Elapsed time, in milliseconds, from a platform-specific zero time
  // to the time the message was created
  PRUint32    time;
  // Flags to hold event flow stage and capture/bubble cancellation
  // status. This is used also to indicate whether the event is trusted.
  PRUint32    flags;
  // Additional type info for user defined events
  nsCOMPtr<nsIAtom>     userType;
  // Event targets, needed by DOM Events
  // Using nsISupports, not nsIDOMEventTarget because in some cases
  // nsIDOMEventTarget is implemented as a tearoff.
  nsCOMPtr<nsISupports> target;
  nsCOMPtr<nsISupports> currentTarget;
  nsCOMPtr<nsISupports> originalTarget;
};

/**
 * General graphic user interface event
 */

class nsGUIEvent : public nsEvent
{
protected:
  nsGUIEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w, PRUint8 structType)
    : nsEvent(isTrusted, msg, structType),
      widget(w), nativeMsg(nsnull)
  {
  }

public:
  nsGUIEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsEvent(isTrusted, msg, NS_GUI_EVENT),
      widget(w), nativeMsg(nsnull)
  {
  }

  /// Originator of the event
  nsCOMPtr<nsIWidget> widget;

  /// Internal platform specific message.
  void* nativeMsg;
};

/**
 * Script error event
 */

class nsScriptErrorEvent : public nsEvent
{
public:
  nsScriptErrorEvent(PRBool isTrusted, PRUint32 msg)
    : nsEvent(isTrusted, msg, NS_SCRIPT_ERROR_EVENT),
      lineNr(0), errorMsg(nsnull), fileName(nsnull)
  {
  }

  PRInt32           lineNr;
  const PRUnichar*  errorMsg;
  const PRUnichar*  fileName;
};

class nsBeforePageUnloadEvent : public nsEvent
{
public:
  nsBeforePageUnloadEvent(PRBool isTrusted, PRUint32 msg)
    : nsEvent(isTrusted, msg, NS_BEFORE_PAGE_UNLOAD_EVENT)
  {
  }

  nsString text;
};

/**
 * Window resize event
 */

class nsSizeEvent : public nsGUIEvent
{
public:
  nsSizeEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_SIZE_EVENT),
      windowSize(nsnull), mWinWidth(0), mWinHeight(0)
  {
  }

  /// x,y width, height in pixels (client area)
  nsRect          *windowSize;    
  /// width of entire window (in pixels)
  PRInt32         mWinWidth;    
  /// height of entire window (in pixels)
  PRInt32         mWinHeight;    
};

/**
 * Window size mode event
 */

class nsSizeModeEvent : public nsGUIEvent
{
public:
  nsSizeModeEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_SIZEMODE_EVENT),
      mSizeMode(nsSizeMode_Normal)
  {
  }

  nsSizeMode      mSizeMode;
};

/**
 * Window z-level event
 */

class nsZLevelEvent : public nsGUIEvent
{
public:
  nsZLevelEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_ZLEVEL_EVENT),
      mPlacement(nsWindowZTop), mReqBelow(nsnull), mActualBelow(nsnull),
      mImmediate(PR_FALSE), mAdjusted(PR_FALSE)
  {
  }

  nsWindowZ  mPlacement;
  nsIWidget *mReqBelow,    // widget we request being below, if any
            *mActualBelow; // widget to be below, returned by handler
  PRBool     mImmediate,   // handler should make changes immediately
             mAdjusted;    // handler changed placement
};

/**
 * Window repaint event
 */

class nsPaintEvent : public nsGUIEvent
{
public:
  nsPaintEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_PAINT_EVENT),
      renderingContext(nsnull), region(nsnull), rect(nsnull)
  {
  }

  /// Context to paint in.
  nsIRenderingContext *renderingContext;
  /// area to paint  (should be used instead of rect)
  nsIRegion           *region;
  /// x,y, width, height in pixels of area to paint
  nsRect              *rect;      
};

/**
 * Scrollbar event
 */

class nsScrollbarEvent : public nsGUIEvent
{
public:
  nsScrollbarEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_SCROLLBAR_EVENT),
      position(0)
  {
  }

  /// ranges between scrollbar 0 and (maxRange - thumbSize). See nsIScrollbar
  PRUint32        position; 
};

class nsScrollPortEvent : public nsGUIEvent
{
public:
  enum orientType {
    vertical   = 0,
    horizontal = 1,
    both       = 2
  };

  nsScrollPortEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_SCROLLPORT_EVENT),
      orient(vertical)
  {
  }

  orientType orient;
};

class nsInputEvent : public nsGUIEvent
{
protected:
  nsInputEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w,
               PRUint8 structType)
    : nsGUIEvent(isTrusted, msg, w, structType),
      isShift(PR_FALSE), isControl(PR_FALSE), isAlt(PR_FALSE), isMeta(PR_FALSE)
  {
  }

public:
  nsInputEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_INPUT_EVENT),
      isShift(PR_FALSE), isControl(PR_FALSE), isAlt(PR_FALSE), isMeta(PR_FALSE)
  {
  }

  /// PR_TRUE indicates the shift key is down
  PRBool          isShift;        
  /// PR_TRUE indicates the control key is down
  PRBool          isControl;      
  /// PR_TRUE indicates the alt key is down
  PRBool          isAlt;          
  /// PR_TRUE indicates the meta key is down (or, on Mac, the Command key)
  PRBool          isMeta;
};

/**
 * Mouse event
 */

class nsMouseEvent_base : public nsInputEvent
{
public:
  nsMouseEvent_base(PRBool isTrusted, PRUint32 msg, nsIWidget *w, PRUint8 type)
  : nsInputEvent(isTrusted, msg, w, type), button(0), pressure(0) {}

  /// The possible related target
  nsCOMPtr<nsISupports> relatedTarget;

  PRInt16               button;

  // Finger or touch pressure of event
  // ranges between 0.0 and 1.0
  float                 pressure;
};

class nsMouseEvent : public nsMouseEvent_base
{
public:
  enum buttonType  { eLeftButton = 0, eMiddleButton = 1, eRightButton = 2 };
  enum reasonType  { eReal, eSynthesized };
  enum contextType { eNormal, eContextMenuKey };
  enum exitType    { eChild, eTopLevel };

protected:
  nsMouseEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w,
               PRUint8 structType, reasonType aReason)
    : nsMouseEvent_base(isTrusted, msg, w, structType),
      acceptActivation(PR_FALSE), ignoreRootScrollFrame(PR_FALSE),
      reason(aReason), context(eNormal), exit(eChild), clickCount(0)
  {
    if (msg == NS_MOUSE_MOVE) {
      flags |= NS_EVENT_FLAG_CANT_CANCEL;
    }
  }

public:

  nsMouseEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w,
               reasonType aReason, contextType aContext = eNormal)
    : nsMouseEvent_base(isTrusted, msg, w, NS_MOUSE_EVENT),
      acceptActivation(PR_FALSE), ignoreRootScrollFrame(PR_FALSE),
      reason(aReason), context(aContext), exit(eChild), clickCount(0)
  {
    if (msg == NS_MOUSE_MOVE) {
      flags |= NS_EVENT_FLAG_CANT_CANCEL;
    } else if (msg == NS_CONTEXTMENU) {
      button = (context == eNormal) ? eRightButton : eLeftButton;
    }
  }
#ifdef NS_DEBUG
  ~nsMouseEvent() {
    NS_WARN_IF_FALSE(message != NS_CONTEXTMENU ||
                     button ==
                       ((context == eNormal) ? eRightButton : eLeftButton),
                     "Wrong button set to NS_CONTEXTMENU event?");
  }
#endif

  /// Special return code for MOUSE_ACTIVATE to signal
  /// if the target accepts activation (1), or denies it (0)
  PRPackedBool acceptActivation;
  // Whether the event should ignore scroll frame bounds
  // during dispatch.
  PRPackedBool ignoreRootScrollFrame;

  reasonType   reason : 4;
  contextType  context : 4;
  exitType     exit;

  /// The number of mouse clicks
  PRUint32     clickCount;
};

/**
 * Drag event
 */

class nsDragEvent : public nsMouseEvent
{
public:
  nsDragEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsMouseEvent(isTrusted, msg, w, NS_DRAG_EVENT, eReal)
  {
    if (msg == NS_DRAGDROP_EXIT_SYNTH ||
        msg == NS_DRAGDROP_LEAVE_SYNTH ||
        msg == NS_DRAGDROP_END) {
      flags |= NS_EVENT_FLAG_CANT_CANCEL;
    }
  }

  nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
};

/**
 * Accessible event
 */

class nsAccessibleEvent : public nsInputEvent
{
public:
  nsAccessibleEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsInputEvent(isTrusted, msg, w, NS_ACCESSIBLE_EVENT),
      accessible(nsnull)
  {
  }

  nsIAccessible*  accessible;     
};

/**
 * Keyboard event
 */

struct nsAlternativeCharCode {
  nsAlternativeCharCode(PRUint32 aUnshiftedCharCode,
                        PRUint32 aShiftedCharCode) :
    mUnshiftedCharCode(aUnshiftedCharCode), mShiftedCharCode(aShiftedCharCode)
  {
  }
  PRUint32 mUnshiftedCharCode;
  PRUint32 mShiftedCharCode;
};

class nsKeyEvent : public nsInputEvent
{
public:
  nsKeyEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsInputEvent(isTrusted, msg, w, NS_KEY_EVENT),
      keyCode(0), charCode(0), isChar(0)
  {
  }

  /// see NS_VK codes
  PRUint32        keyCode;   
  /// OS translated Unicode char
  PRUint32        charCode;
  // OS translated Unicode chars which are used for accesskey and accelkey
  // handling. The handlers will try from first character to last character.
  nsTArray<nsAlternativeCharCode> alternativeCharCodes;
  // indicates whether the event signifies a printable character
  PRBool          isChar;
};

/**
 * IME Related Events
 */
struct nsTextRange
{
  nsTextRange()
    : mStartOffset(0), mEndOffset(0), mRangeType(0)
  {
  }

  PRUint32 mStartOffset;
  PRUint32 mEndOffset;
  PRUint32 mRangeType;
};

typedef nsTextRange* nsTextRangeArray;

struct nsTextEventReply
{
  nsTextEventReply()
    : mCursorIsCollapsed(PR_FALSE), mReferenceWidget(nsnull)
  {
  }

  nsRect mCursorPosition;
  PRBool mCursorIsCollapsed;
  nsIWidget* mReferenceWidget;
};

typedef struct nsTextEventReply nsTextEventReply;

class nsTextEvent : public nsInputEvent
{
public:
  nsTextEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsInputEvent(isTrusted, msg, w, NS_TEXT_EVENT),
      theText(nsnull), rangeCount(0), rangeArray(nsnull), isChar(PR_FALSE)
  {
  }

  const PRUnichar*  theText;
  nsTextEventReply  theReply;
  PRUint32          rangeCount;
  nsTextRangeArray  rangeArray;
  PRBool            isChar;
};

class nsCompositionEvent : public nsInputEvent
{
public:
  nsCompositionEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsInputEvent(isTrusted, msg, w, NS_COMPOSITION_EVENT)
  {
  }

  nsTextEventReply theReply;
};

/* Mouse Scroll Events: Line Scrolling, Pixel Scrolling and Common Event Flows
 *
 * There are two common event flows:
 *  (1) Normal line scrolling:
 *      1. An NS_MOUSE_SCROLL event without kHasPixels is dispatched to Gecko.
 *      2. A DOMMouseScroll event is sent into the DOM.
 *      3. A MozMousePixelScroll event is sent into the DOM.
 *      4. If neither event has been consumed, the default handling of the
 *         NS_MOUSE_SCROLL event is executed.
 *
 *  (2) Pixel scrolling:
 *      1. An NS_MOUSE_SCROLL event with kHasPixels is dispatched to Gecko.
 *      2. A DOMMouseScroll event is sent into the DOM.
 *      3. No scrolling takes place in the default handler.
 *      4. An NS_MOUSE_PIXEL_SCROLL event is dispatched to Gecko.
 *      5. A MozMousePixelScroll event is sent into the DOM.
 *      6. If neither the NS_MOUSE_PIXELSCROLL event nor the preceding
 *         NS_MOUSE_SCROLL event have been consumed, the default handler scrolls.
 *      7. Steps 4.-6. are repeated for every pixel scroll that belongs to
 *         the announced line scroll. Once enough pixels have been sent to
 *         complete a line, a new NS_MOUSE_SCROLL event is sent (goto step 1.).
 *
 * If a DOMMouseScroll event has been preventDefaulted, the associated
 * following MozMousePixelScroll events are still sent - they just don't result
 * in any scrolling (their default handler isn't executed).
 *
 * How many pixel scrolls make up one line scroll is decided in the widget layer
 * where the NS_MOUSE(_PIXEL)_SCROLL events are created.
 *
 * This event flow model satisfies several requirements:
 *  - DOMMouseScroll handlers don't need to be aware of the existence of pixel
 *    scrolling.
 *  - preventDefault on a DOMMouseScroll event results in no scrolling.
 *  - DOMMouseScroll events aren't polluted with a kHasPixels flag.
 *  - You can make use of pixel scroll DOM events (MozMousePixelScroll).
 */

class nsMouseScrollEvent : public nsMouseEvent_base
{
public:
  enum nsMouseScrollFlags {
    kIsFullPage =   1 << 0,
    kIsVertical =   1 << 1,
    kIsHorizontal = 1 << 2,
    kHasPixels =    1 << 3  // Marks line scroll events that are provided as
                            // a fallback for pixel scroll events.
                            // These scroll events are used by things that can't
                            // be scrolled pixel-wise, like trees. You should
                            // ignore them when processing pixel scroll events
                            // to avoid double-processing the same scroll gesture.
                            // When kHasPixels is set, the event is guaranteed to
                            // be followed up by an event that contains pixel
                            // scrolling information.
  };

  nsMouseScrollEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsMouseEvent_base(isTrusted, msg, w, NS_MOUSE_SCROLL_EVENT),
      scrollFlags(0), delta(0)
  {
  }

  PRInt32               scrollFlags;
  PRInt32               delta;
};

class nsQueryContentEvent : public nsGUIEvent
{
public:
  nsQueryContentEvent(PRBool aIsTrusted, PRUint32 aMsg, nsIWidget *aWidget) :
    nsGUIEvent(aIsTrusted, aMsg, aWidget, NS_QUERY_CONTENT_EVENT),
    mSucceeded(PR_FALSE)
  {
  }

  void InitForQueryTextContent(PRUint32 aOffset, PRUint32 aLength)
  {
    NS_ASSERTION(message == NS_QUERY_TEXT_CONTENT,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
  }

  void InitForQueryCharacterRect(PRUint32 aOffset)
  {
    NS_ASSERTION(message == NS_QUERY_CHARACTER_RECT,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
  }

  void InitForQueryCaretRect(PRUint32 aOffset)
  {
    NS_ASSERTION(message == NS_QUERY_CARET_RECT,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
  }

  PRBool mSucceeded;
  struct {
    PRUint32 mOffset;
    PRUint32 mLength;
  } mInput;
  struct {
    void* mContentsRoot;
    PRUint32 mOffset;
    nsString mString;
    nsRect mRect; // Finally, the coordinates is system coordinates.
    // The return widget has the caret. This is set at all query events.
    nsIWidget* mFocusedWidget;
  } mReply;
};

/**
 * MenuItem event
 * 
 * When this event occurs the widget field in nsGUIEvent holds the "target"
 * for the event
 */

class nsMenuEvent : public nsGUIEvent
{
public:
  nsMenuEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_MENU_EVENT),
      mMenuItem(nsnull), mCommand(0)
  {
  }

  nsIMenuItem * mMenuItem;
  PRUint32      mCommand;
};

/**
 * Form event
 * 
 * We hold the originating form control for form submit and reset events.
 * originator is a weak pointer (does not hold a strong reference).
 */

class nsFormEvent : public nsEvent
{
public:
  nsFormEvent(PRBool isTrusted, PRUint32 msg)
    : nsEvent(isTrusted, msg, NS_FORM_EVENT),
      originator(nsnull)
  {
  }

  nsIContent *originator;
};

/**
* Focus event
*/
class nsFocusEvent : public nsGUIEvent
{
public:
  nsFocusEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_FOCUS_EVENT),
      isMozWindowTakingFocus(PR_FALSE)
  {
  }

  PRBool isMozWindowTakingFocus;
};

/**
 * Command event
 *
 * Custom commands for example from the operating system.
 */

class nsCommandEvent : public nsGUIEvent
{
public:
  nsCommandEvent(PRBool isTrusted, nsIAtom* aEventType,
                 nsIAtom* aCommand, nsIWidget* w)
    : nsGUIEvent(isTrusted, NS_USER_DEFINED_EVENT, w, NS_COMMAND_EVENT)
  {
    userType = aEventType;
    command = aCommand;
  }

  nsCOMPtr<nsIAtom> command;
};

/**
 * blocked popup window event
 */
class nsPopupBlockedEvent : public nsEvent
{
public:
  nsPopupBlockedEvent(PRBool isTrusted, PRUint32 msg)
    : nsEvent(isTrusted, msg, NS_POPUPBLOCKED_EVENT),
      mPopupWindowURI(nsnull)
  {
  }

  nsWeakPtr mRequestingWindow;
  nsIURI* mPopupWindowURI;      // owning reference
  nsString mPopupWindowFeatures;
  nsString mPopupWindowName;
};

/**
 * DOM UIEvent
 */
class nsUIEvent : public nsEvent
{
public:
  nsUIEvent(PRBool isTrusted, PRUint32 msg, PRInt32 d)
    : nsEvent(isTrusted, msg, NS_UI_EVENT),
      detail(d)
  {
  }

  PRInt32 detail;
};

/**
 * NotifyPaint event
 */
class nsNotifyPaintEvent : public nsEvent
{
public:
  nsNotifyPaintEvent(PRBool isTrusted, PRUint32 msg,
                     const nsRegion& aSameDocRegion, const nsRegion& aCrossDocRegion)
    : nsEvent(isTrusted, msg, NS_NOTIFYPAINT_EVENT),
      sameDocRegion(aSameDocRegion), crossDocRegion(aCrossDocRegion)
  {
  }

  nsRegion sameDocRegion;
  nsRegion crossDocRegion;
};

/**
 * PageTransition event
 */
class nsPageTransitionEvent : public nsEvent
{
public:
  nsPageTransitionEvent(PRBool isTrusted, PRUint32 msg, PRBool p)
    : nsEvent(isTrusted, msg, NS_PAGETRANSITION_EVENT),
      persisted(p)
  {
  }

  PRBool persisted;
};

/**
 * XUL command event
 */
class nsXULCommandEvent : public nsInputEvent
{
public:
  nsXULCommandEvent(PRBool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsInputEvent(isTrusted, msg, w, NS_XUL_COMMAND_EVENT)
  {
  }

  nsCOMPtr<nsIDOMEvent> sourceEvent;
};

/**
 * Simple gesture event
 */
class nsSimpleGestureEvent : public nsInputEvent
{
public:
  nsSimpleGestureEvent(PRBool isTrusted, PRUint32 msg, nsIWidget* w,
                         PRUint32 directionArg, PRFloat64 deltaArg)
    : nsInputEvent(isTrusted, msg, w, NS_SIMPLE_GESTURE_EVENT),
      direction(directionArg), delta(deltaArg)
  {
  }

  PRUint32 direction;   // See nsIDOMSimpleGestureEvent for values
  PRFloat64 delta;      // Delta for magnify and rotate events
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


#define NS_IS_MOUSE_EVENT(evnt) \
       (((evnt)->message == NS_MOUSE_BUTTON_DOWN) || \
        ((evnt)->message == NS_MOUSE_BUTTON_UP) || \
        ((evnt)->message == NS_MOUSE_CLICK) || \
        ((evnt)->message == NS_MOUSE_DOUBLECLICK) || \
        ((evnt)->message == NS_MOUSE_ENTER) || \
        ((evnt)->message == NS_MOUSE_EXIT) || \
        ((evnt)->message == NS_MOUSE_ACTIVATE) || \
        ((evnt)->message == NS_MOUSE_ENTER_SYNTH) || \
        ((evnt)->message == NS_MOUSE_EXIT_SYNTH) || \
        ((evnt)->message == NS_MOUSE_MOVE))

#define NS_IS_MOUSE_LEFT_CLICK(evnt) \
       ((evnt)->eventStructType == NS_MOUSE_EVENT && \
        (evnt)->message == NS_MOUSE_CLICK && \
        static_cast<nsMouseEvent*>((evnt))->button == nsMouseEvent::eLeftButton)

#define NS_IS_CONTEXT_MENU_KEY(evnt) \
       ((evnt)->eventStructType == NS_MOUSE_EVENT && \
        (evnt)->message == NS_CONTEXTMENU && \
        static_cast<nsMouseEvent*>((evnt))->context == nsMouseEvent::eContextMenuKey)

#define NS_IS_DRAG_EVENT(evnt) \
       (((evnt)->message == NS_DRAGDROP_ENTER) || \
        ((evnt)->message == NS_DRAGDROP_OVER) || \
        ((evnt)->message == NS_DRAGDROP_EXIT) || \
        ((evnt)->message == NS_DRAGDROP_DRAGDROP) || \
        ((evnt)->message == NS_DRAGDROP_GESTURE) || \
        ((evnt)->message == NS_DRAGDROP_DRAG) || \
        ((evnt)->message == NS_DRAGDROP_END) || \
        ((evnt)->message == NS_DRAGDROP_START) || \
        ((evnt)->message == NS_DRAGDROP_DROP) || \
        ((evnt)->message == NS_DRAGDROP_LEAVE_SYNTH))

#define NS_IS_KEY_EVENT(evnt) \
       (((evnt)->message == NS_KEY_DOWN) ||  \
        ((evnt)->message == NS_KEY_PRESS) || \
        ((evnt)->message == NS_KEY_UP))

#define NS_IS_IME_EVENT(evnt) \
       (((evnt)->message == NS_TEXT_TEXT) ||  \
        ((evnt)->message == NS_COMPOSITION_START) ||  \
        ((evnt)->message == NS_COMPOSITION_END) || \
        ((evnt)->message == NS_COMPOSITION_QUERY))

#define NS_IS_FOCUS_EVENT(evnt) \
       (((evnt)->message == NS_GOTFOCUS) ||  \
        ((evnt)->message == NS_LOSTFOCUS) ||  \
        ((evnt)->message == NS_ACTIVATE) || \
        ((evnt)->message == NS_DEACTIVATE) || \
        ((evnt)->message == NS_PLUGIN_ACTIVATE))

#define NS_IS_QUERY_CONTENT_EVENT(evnt) \
       (((evnt)->message == NS_QUERY_SELECTED_TEXT) || \
        ((evnt)->message == NS_QUERY_TEXT_CONTENT) || \
        ((evnt)->message == NS_QUERY_CHARACTER_RECT) || \
        ((evnt)->message == NS_QUERY_CARET_RECT))

#define NS_IS_TRUSTED_EVENT(event) \
  (((event)->flags & NS_EVENT_FLAG_TRUSTED) != 0)

// Mark an event as being dispatching.
#define NS_MARK_EVENT_DISPATCH_STARTED(event) \
  (event)->flags |= NS_EVENT_FLAG_DISPATCHING;

#define NS_IS_EVENT_IN_DISPATCH(event) \
  (((event)->flags & NS_EVENT_FLAG_DISPATCHING) != 0)

// Mark an event as being done dispatching.
#define NS_MARK_EVENT_DISPATCH_DONE(event) \
  NS_ASSERTION(NS_IS_EVENT_IN_DISPATCH(event), \
               "Event never got marked for dispatch!"); \
  (event)->flags &= ~NS_EVENT_FLAG_DISPATCHING; \
  (event)->flags |= NS_EVENT_DISPATCHED;

/*
 * Virtual key bindings for keyboard events.
 * These come from nsIDOMKeyEvent.h, which is generated from MouseKeyEvent.idl.
 * Really, it would be better if we phased out the NS_VK symbols altogether
 * in favor of the DOM ones, but at least this way they'll be in sync.
 */

#define NS_VK_CANCEL         nsIDOMKeyEvent::DOM_VK_CANCEL
#define NS_VK_HELP           nsIDOMKeyEvent::DOM_VK_HELP
#define NS_VK_BACK           nsIDOMKeyEvent::DOM_VK_BACK_SPACE
#define NS_VK_TAB            nsIDOMKeyEvent::DOM_VK_TAB
#define NS_VK_CLEAR          nsIDOMKeyEvent::DOM_VK_CLEAR
#define NS_VK_RETURN         nsIDOMKeyEvent::DOM_VK_RETURN
#define NS_VK_ENTER          nsIDOMKeyEvent::DOM_VK_ENTER
#define NS_VK_SHIFT          nsIDOMKeyEvent::DOM_VK_SHIFT
#define NS_VK_CONTROL        nsIDOMKeyEvent::DOM_VK_CONTROL
#define NS_VK_ALT            nsIDOMKeyEvent::DOM_VK_ALT
#define NS_VK_PAUSE          nsIDOMKeyEvent::DOM_VK_PAUSE
#define NS_VK_CAPS_LOCK      nsIDOMKeyEvent::DOM_VK_CAPS_LOCK
#define NS_VK_ESCAPE         nsIDOMKeyEvent::DOM_VK_ESCAPE
#define NS_VK_SPACE          nsIDOMKeyEvent::DOM_VK_SPACE
#define NS_VK_PAGE_UP        nsIDOMKeyEvent::DOM_VK_PAGE_UP
#define NS_VK_PAGE_DOWN      nsIDOMKeyEvent::DOM_VK_PAGE_DOWN
#define NS_VK_END            nsIDOMKeyEvent::DOM_VK_END
#define NS_VK_HOME           nsIDOMKeyEvent::DOM_VK_HOME
#define NS_VK_LEFT           nsIDOMKeyEvent::DOM_VK_LEFT
#define NS_VK_UP             nsIDOMKeyEvent::DOM_VK_UP
#define NS_VK_RIGHT          nsIDOMKeyEvent::DOM_VK_RIGHT
#define NS_VK_DOWN           nsIDOMKeyEvent::DOM_VK_DOWN
#define NS_VK_PRINTSCREEN    nsIDOMKeyEvent::DOM_VK_PRINTSCREEN
#define NS_VK_INSERT         nsIDOMKeyEvent::DOM_VK_INSERT
#define NS_VK_DELETE         nsIDOMKeyEvent::DOM_VK_DELETE

// NS_VK_0 - NS_VK_9 match their ascii values
#define NS_VK_0              nsIDOMKeyEvent::DOM_VK_0
#define NS_VK_1              nsIDOMKeyEvent::DOM_VK_1
#define NS_VK_2              nsIDOMKeyEvent::DOM_VK_2
#define NS_VK_3              nsIDOMKeyEvent::DOM_VK_3
#define NS_VK_4              nsIDOMKeyEvent::DOM_VK_4
#define NS_VK_5              nsIDOMKeyEvent::DOM_VK_5
#define NS_VK_6              nsIDOMKeyEvent::DOM_VK_6
#define NS_VK_7              nsIDOMKeyEvent::DOM_VK_7
#define NS_VK_8              nsIDOMKeyEvent::DOM_VK_8
#define NS_VK_9              nsIDOMKeyEvent::DOM_VK_9

#define NS_VK_SEMICOLON      nsIDOMKeyEvent::DOM_VK_SEMICOLON
#define NS_VK_EQUALS         nsIDOMKeyEvent::DOM_VK_EQUALS

// NS_VK_A - NS_VK_Z match their ascii values
#define NS_VK_A              nsIDOMKeyEvent::DOM_VK_A
#define NS_VK_B              nsIDOMKeyEvent::DOM_VK_B
#define NS_VK_C              nsIDOMKeyEvent::DOM_VK_C
#define NS_VK_D              nsIDOMKeyEvent::DOM_VK_D
#define NS_VK_E              nsIDOMKeyEvent::DOM_VK_E
#define NS_VK_F              nsIDOMKeyEvent::DOM_VK_F
#define NS_VK_G              nsIDOMKeyEvent::DOM_VK_G
#define NS_VK_H              nsIDOMKeyEvent::DOM_VK_H
#define NS_VK_I              nsIDOMKeyEvent::DOM_VK_I
#define NS_VK_J              nsIDOMKeyEvent::DOM_VK_J
#define NS_VK_K              nsIDOMKeyEvent::DOM_VK_K
#define NS_VK_L              nsIDOMKeyEvent::DOM_VK_L
#define NS_VK_M              nsIDOMKeyEvent::DOM_VK_M
#define NS_VK_N              nsIDOMKeyEvent::DOM_VK_N
#define NS_VK_O              nsIDOMKeyEvent::DOM_VK_O
#define NS_VK_P              nsIDOMKeyEvent::DOM_VK_P
#define NS_VK_Q              nsIDOMKeyEvent::DOM_VK_Q
#define NS_VK_R              nsIDOMKeyEvent::DOM_VK_R
#define NS_VK_S              nsIDOMKeyEvent::DOM_VK_S
#define NS_VK_T              nsIDOMKeyEvent::DOM_VK_T
#define NS_VK_U              nsIDOMKeyEvent::DOM_VK_U
#define NS_VK_V              nsIDOMKeyEvent::DOM_VK_V
#define NS_VK_W              nsIDOMKeyEvent::DOM_VK_W
#define NS_VK_X              nsIDOMKeyEvent::DOM_VK_X
#define NS_VK_Y              nsIDOMKeyEvent::DOM_VK_Y
#define NS_VK_Z              nsIDOMKeyEvent::DOM_VK_Z

#define NS_VK_CONTEXT_MENU   nsIDOMKeyEvent::DOM_VK_CONTEXT_MENU

#define NS_VK_NUMPAD0        nsIDOMKeyEvent::DOM_VK_NUMPAD0
#define NS_VK_NUMPAD1        nsIDOMKeyEvent::DOM_VK_NUMPAD1
#define NS_VK_NUMPAD2        nsIDOMKeyEvent::DOM_VK_NUMPAD2
#define NS_VK_NUMPAD3        nsIDOMKeyEvent::DOM_VK_NUMPAD3
#define NS_VK_NUMPAD4        nsIDOMKeyEvent::DOM_VK_NUMPAD4
#define NS_VK_NUMPAD5        nsIDOMKeyEvent::DOM_VK_NUMPAD5
#define NS_VK_NUMPAD6        nsIDOMKeyEvent::DOM_VK_NUMPAD6
#define NS_VK_NUMPAD7        nsIDOMKeyEvent::DOM_VK_NUMPAD7
#define NS_VK_NUMPAD8        nsIDOMKeyEvent::DOM_VK_NUMPAD8
#define NS_VK_NUMPAD9        nsIDOMKeyEvent::DOM_VK_NUMPAD9
#define NS_VK_MULTIPLY       nsIDOMKeyEvent::DOM_VK_MULTIPLY
#define NS_VK_ADD            nsIDOMKeyEvent::DOM_VK_ADD
#define NS_VK_SEPARATOR      nsIDOMKeyEvent::DOM_VK_SEPARATOR
#define NS_VK_SUBTRACT       nsIDOMKeyEvent::DOM_VK_SUBTRACT
#define NS_VK_DECIMAL        nsIDOMKeyEvent::DOM_VK_DECIMAL
#define NS_VK_DIVIDE         nsIDOMKeyEvent::DOM_VK_DIVIDE
#define NS_VK_F1             nsIDOMKeyEvent::DOM_VK_F1
#define NS_VK_F2             nsIDOMKeyEvent::DOM_VK_F2
#define NS_VK_F3             nsIDOMKeyEvent::DOM_VK_F3
#define NS_VK_F4             nsIDOMKeyEvent::DOM_VK_F4
#define NS_VK_F5             nsIDOMKeyEvent::DOM_VK_F5
#define NS_VK_F6             nsIDOMKeyEvent::DOM_VK_F6
#define NS_VK_F7             nsIDOMKeyEvent::DOM_VK_F7
#define NS_VK_F8             nsIDOMKeyEvent::DOM_VK_F8
#define NS_VK_F9             nsIDOMKeyEvent::DOM_VK_F9
#define NS_VK_F10            nsIDOMKeyEvent::DOM_VK_F10
#define NS_VK_F11            nsIDOMKeyEvent::DOM_VK_F11
#define NS_VK_F12            nsIDOMKeyEvent::DOM_VK_F12
#define NS_VK_F13            nsIDOMKeyEvent::DOM_VK_F13
#define NS_VK_F14            nsIDOMKeyEvent::DOM_VK_F14
#define NS_VK_F15            nsIDOMKeyEvent::DOM_VK_F15
#define NS_VK_F16            nsIDOMKeyEvent::DOM_VK_F16
#define NS_VK_F17            nsIDOMKeyEvent::DOM_VK_F17
#define NS_VK_F18            nsIDOMKeyEvent::DOM_VK_F18
#define NS_VK_F19            nsIDOMKeyEvent::DOM_VK_F19
#define NS_VK_F20            nsIDOMKeyEvent::DOM_VK_F20
#define NS_VK_F21            nsIDOMKeyEvent::DOM_VK_F21
#define NS_VK_F22            nsIDOMKeyEvent::DOM_VK_F22
#define NS_VK_F23            nsIDOMKeyEvent::DOM_VK_F23
#define NS_VK_F24            nsIDOMKeyEvent::DOM_VK_F24

#define NS_VK_NUM_LOCK       nsIDOMKeyEvent::DOM_VK_NUM_LOCK
#define NS_VK_SCROLL_LOCK    nsIDOMKeyEvent::DOM_VK_SCROLL_LOCK

#define NS_VK_COMMA          nsIDOMKeyEvent::DOM_VK_COMMA
#define NS_VK_PERIOD         nsIDOMKeyEvent::DOM_VK_PERIOD
#define NS_VK_SLASH          nsIDOMKeyEvent::DOM_VK_SLASH
#define NS_VK_BACK_QUOTE     nsIDOMKeyEvent::DOM_VK_BACK_QUOTE
#define NS_VK_OPEN_BRACKET   nsIDOMKeyEvent::DOM_VK_OPEN_BRACKET
#define NS_VK_BACK_SLASH     nsIDOMKeyEvent::DOM_VK_BACK_SLASH
#define NS_VK_CLOSE_BRACKET  nsIDOMKeyEvent::DOM_VK_CLOSE_BRACKET
#define NS_VK_QUOTE          nsIDOMKeyEvent::DOM_VK_QUOTE

#define NS_VK_META           nsIDOMKeyEvent::DOM_VK_META

// IME Constants  -- keep in synch with nsIDOMTextRange.h
#define NS_TEXTRANGE_CARETPOSITION				0x01
#define NS_TEXTRANGE_RAWINPUT					0X02
#define NS_TEXTRANGE_SELECTEDRAWTEXT			0x03
#define NS_TEXTRANGE_CONVERTEDTEXT				0x04
#define NS_TEXTRANGE_SELECTEDCONVERTEDTEXT		0x05

inline PRBool NS_TargetUnfocusedEventToLastFocusedContent(nsEvent* aEvent)
{
#if defined(MOZ_X11) || defined(XP_MACOSX)
  // bug 52416 (MOZ_X11)
  // Lookup region (candidate window) of UNIX IME grabs
  // input focus from Mozilla but wants to send IME event
  // to redraw pre-edit (composed) string
  // If Mozilla does not have input focus and event is IME,
  // sends IME event to pre-focused element

  // bug 417315 (XP_MACOSX)
  // The commit event when the window is deactivating is sent after
  // the next focused widget getting the focus.
  // We need to send the commit event to last focused content.

  return NS_IS_IME_EVENT(aEvent);
#elif defined(XP_WIN)
  // bug 292263 (XP_WIN)
  // If software keyboard has focus, it may send the key messages and
  // the IME messages to pre-focused window. Therefore, if Mozilla
  // doesn't have focus and event is key event or IME event, we should
  // send the events to pre-focused element.

  return NS_IS_KEY_EVENT(aEvent) || NS_IS_IME_EVENT(aEvent);
#else
  return PR_FALSE;
#endif
}

#endif // nsGUIEvent_h__
