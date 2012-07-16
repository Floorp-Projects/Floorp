/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGUIEvent_h__
#define nsGUIEvent_h__

#include "nsCOMArray.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "nsEvent.h"
#include "nsStringGlue.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMDataTransfer.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMTouchEvent.h"
#include "nsWeakPtr.h"
#include "nsIWidget.h"
#include "nsTArray.h"
#include "nsTraceRefcnt.h"
#include "nsITransferable.h"
#include "nsIVariant.h"
#include "nsStyleConsts.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {
  class PBrowserParent;
  class PBrowserChild;
}
namespace plugins {
  class PPluginInstanceChild;
}
}

#ifdef ACCESSIBILITY
class Accessible;
#endif
class nsRenderingContext;
class nsIMenuItem;
class nsIContent;
class nsIURI;
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
#define NS_SCROLLAREA_EVENT               25
#define NS_TRANSITION_EVENT               26
#define NS_ANIMATION_EVENT                27

#define NS_UI_EVENT                       28
#define NS_SVG_EVENT                      30
#define NS_SVGZOOM_EVENT                  31
#define NS_SMIL_TIME_EVENT                32

#define NS_QUERY_CONTENT_EVENT            33

#define NS_DRAG_EVENT                     35
#define NS_NOTIFYPAINT_EVENT              36
#define NS_SIMPLE_GESTURE_EVENT           37
#define NS_SELECTION_EVENT                38
#define NS_CONTENT_COMMAND_EVENT          39
#define NS_GESTURENOTIFY_EVENT            40
#define NS_UISTATECHANGE_EVENT            41
#define NS_MOZTOUCH_EVENT                 42
#define NS_PLUGIN_EVENT                   43
#define NS_TOUCH_EVENT                    44

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
// When an event is synthesized for testing, this flag will be set.
// Note that this is currently used only with mouse events, because this
// flag is not needed on other events now.  It could be added to other
// events.
#define NS_EVENT_FLAG_SYNTHETIC_TEST_EVENT 0x1000

// Use this flag if the event should be dispatched only to chrome.
#define NS_EVENT_FLAG_ONLY_CHROME_DISPATCH 0x2000

// A flag for drag&drop handling.
#define NS_EVENT_FLAG_NO_DEFAULT_CALLED_IN_CONTENT 0x4000

#define NS_PRIV_EVENT_UNTRUSTED_PERMITTED 0x8000

#define NS_EVENT_FLAG_EXCEPTION_THROWN    0x10000

#define NS_EVENT_FLAG_PREVENT_MULTIPLE_ACTIONS 0x20000

#define NS_EVENT_RETARGET_TO_NON_NATIVE_ANONYMOUS 0x40000

#define NS_EVENT_FLAG_STOP_DISPATCH_IMMEDIATELY 0x80000

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
// Widget got activated
#define NS_ACTIVATE                     (NS_WINDOW_START + 7)
// Widget got deactivated
#define NS_DEACTIVATE                   (NS_WINDOW_START + 8)
// top-level window z-level change request
#define NS_SETZLEVEL                    (NS_WINDOW_START + 9)
// Widget was repainted (dispatched when it's safe to move widgets, but
// only on some platforms (including GTK2 and Windows))
#define NS_DID_PAINT                   (NS_WINDOW_START + 28)
// Widget will need to be painted
#define NS_WILL_PAINT                   (NS_WINDOW_START + 29)
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

// Indicates the display has changed depth
#define NS_DISPLAYCHANGED                (NS_WINDOW_START + 40)

// Indicates a theme change has occurred
#define NS_THEMECHANGED                 (NS_WINDOW_START + 41)

// Indicates a System color has changed. It is the platform
// toolkits responsibility to invalidate the window to 
// ensure that it is drawn using the current system colors.
#define NS_SYSCOLORCHANGED              (NS_WINDOW_START + 42)

// Indicates that the ui state such as whether to show focus or
// keyboard accelerator indicators has changed.
#define NS_UISTATECHANGED               (NS_WINDOW_START + 43)

// Done sizing or moving a window, so ensure that the mousedown state was cleared.
#define NS_DONESIZEMOVE                 (NS_WINDOW_START + 44)

#define NS_RESIZE_EVENT                 (NS_WINDOW_START + 60)
#define NS_SCROLL_EVENT                 (NS_WINDOW_START + 61)

// A plugin was clicked or otherwise focused. NS_PLUGIN_ACTIVATE should be
// used when the window is not active. NS_PLUGIN_FOCUS should be used when
// the window is active. In the latter case, the dispatcher of the event
// is expected to ensure that the plugin's widget is focused beforehand.
#define NS_PLUGIN_ACTIVATE               (NS_WINDOW_START + 62)
#define NS_PLUGIN_FOCUS                  (NS_WINDOW_START + 63)

#define NS_OFFLINE                       (NS_WINDOW_START + 64)
#define NS_ONLINE                        (NS_WINDOW_START + 65)

// Indicates a resize will occur
#define NS_BEFORERESIZE_EVENT            (NS_WINDOW_START + 66)

// Indicates that the user is either idle or active
#define NS_MOZ_USER_IDLE                 (NS_WINDOW_START + 67)
#define NS_MOZ_USER_ACTIVE               (NS_WINDOW_START + 68)

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
#define NS_MOUSE_MOZHITTEST             (NS_MOUSE_MESSAGE_START + 33)
#define NS_MOUSEENTER                   (NS_MOUSE_MESSAGE_START + 34)
#define NS_MOUSELEAVE                   (NS_MOUSE_MESSAGE_START + 35)

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
#define NS_HASHCHANGE                   (NS_STREAM_EVENT_START + 2)
#define NS_IMAGE_ABORT                  (NS_STREAM_EVENT_START + 3)
#define NS_LOAD_ERROR                   (NS_STREAM_EVENT_START + 4)
#define NS_POPSTATE                     (NS_STREAM_EVENT_START + 5)
#define NS_BEFORE_PAGE_UNLOAD           (NS_STREAM_EVENT_START + 6)
#define NS_PAGE_RESTORE                 (NS_STREAM_EVENT_START + 7)
#define NS_READYSTATECHANGE             (NS_STREAM_EVENT_START + 8)
 
#define NS_FORM_EVENT_START             1200
#define NS_FORM_SUBMIT                  (NS_FORM_EVENT_START)
#define NS_FORM_RESET                   (NS_FORM_EVENT_START + 1)
#define NS_FORM_CHANGE                  (NS_FORM_EVENT_START + 2)
#define NS_FORM_SELECTED                (NS_FORM_EVENT_START + 3)
#define NS_FORM_INPUT                   (NS_FORM_EVENT_START + 4)
#define NS_FORM_INVALID                 (NS_FORM_EVENT_START + 5)

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
#define NS_COMPOSITION_UPDATE         (NS_COMPOSITION_EVENT_START + 2)

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
// Returns the entire text if requested length > actual length.
#define NS_QUERY_TEXT_CONTENT           (NS_QUERY_CONTENT_EVENT_START + 1)
// Query for the caret rect of nth insertion point. The offset of the result is
// relative position from the top level widget.
#define NS_QUERY_CARET_RECT             (NS_QUERY_CONTENT_EVENT_START + 3)
// Query for the bounding rect of a range of characters. This works on any
// valid character range given offset and length. Result is relative to top
// level widget coordinates
#define NS_QUERY_TEXT_RECT              (NS_QUERY_CONTENT_EVENT_START + 4)
// Query for the bounding rect of the current focused frame. Result is relative
// to top level widget coordinates
#define NS_QUERY_EDITOR_RECT            (NS_QUERY_CONTENT_EVENT_START + 5)
// Query for the current state of the content. The particular members of
// mReply that are set for each query content event will be valid on success.
#define NS_QUERY_CONTENT_STATE          (NS_QUERY_CONTENT_EVENT_START + 6)
// Query for the selection in the form of a nsITransferable.
#define NS_QUERY_SELECTION_AS_TRANSFERABLE (NS_QUERY_CONTENT_EVENT_START + 7)
// Query for character at a point.  This returns the character offset and its
// rect.  The point is specified by nsEvent::refPoint.
#define NS_QUERY_CHARACTER_AT_POINT     (NS_QUERY_CONTENT_EVENT_START + 8)
// Query if the DOM element under nsEvent::refPoint belongs to our widget
// or not.
#define NS_QUERY_DOM_WIDGET_HITTEST     (NS_QUERY_CONTENT_EVENT_START + 9)
// Query for some information about mouse wheel event's target
// XXX This is used only for supporting high resolution mouse scroll on Windows
//     and it's going to be reimplemented with another approach.  At that time,
//     this even is going to be removed. Therefore,  DON'T use this event for
//     other purpose.
#define NS_QUERY_SCROLL_TARGET_INFO     (NS_QUERY_CONTENT_EVENT_START + 99)

// Video events
#define NS_MEDIA_EVENT_START            3300
#define NS_LOADSTART           (NS_MEDIA_EVENT_START)
#define NS_PROGRESS            (NS_MEDIA_EVENT_START+1)
#define NS_SUSPEND             (NS_MEDIA_EVENT_START+2)
#define NS_EMPTIED             (NS_MEDIA_EVENT_START+3)
#define NS_STALLED             (NS_MEDIA_EVENT_START+4)
#define NS_PLAY                (NS_MEDIA_EVENT_START+5)
#define NS_PAUSE               (NS_MEDIA_EVENT_START+6)
#define NS_LOADEDMETADATA      (NS_MEDIA_EVENT_START+7)
#define NS_LOADEDDATA          (NS_MEDIA_EVENT_START+8)
#define NS_WAITING             (NS_MEDIA_EVENT_START+9)
#define NS_PLAYING             (NS_MEDIA_EVENT_START+10)
#define NS_CANPLAY             (NS_MEDIA_EVENT_START+11)
#define NS_CANPLAYTHROUGH      (NS_MEDIA_EVENT_START+12)
#define NS_SEEKING             (NS_MEDIA_EVENT_START+13)
#define NS_SEEKED              (NS_MEDIA_EVENT_START+14)
#define NS_TIMEUPDATE          (NS_MEDIA_EVENT_START+15)
#define NS_ENDED               (NS_MEDIA_EVENT_START+16)
#define NS_RATECHANGE          (NS_MEDIA_EVENT_START+17)
#define NS_DURATIONCHANGE      (NS_MEDIA_EVENT_START+18)
#define NS_VOLUMECHANGE        (NS_MEDIA_EVENT_START+19)
#define NS_MOZAUDIOAVAILABLE   (NS_MEDIA_EVENT_START+20)

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
#define NS_SIMPLE_GESTURE_TAP            (NS_SIMPLE_GESTURE_EVENT_START+7)
#define NS_SIMPLE_GESTURE_PRESSTAP       (NS_SIMPLE_GESTURE_EVENT_START+8)
#define NS_SIMPLE_GESTURE_EDGEUI         (NS_SIMPLE_GESTURE_EVENT_START+9)

// These are used to send native events to plugins.
#define NS_PLUGIN_EVENT_START            3600
#define NS_PLUGIN_INPUT_EVENT            (NS_PLUGIN_EVENT_START)
#define NS_PLUGIN_FOCUS_EVENT            (NS_PLUGIN_EVENT_START+1)

// Events to manipulate selection (nsSelectionEvent)
#define NS_SELECTION_EVENT_START        3700
// Clear any previous selection and set the given range as the selection
#define NS_SELECTION_SET                (NS_SELECTION_EVENT_START)

// Events of commands for the contents
#define NS_CONTENT_COMMAND_EVENT_START  3800
#define NS_CONTENT_COMMAND_CUT          (NS_CONTENT_COMMAND_EVENT_START)
#define NS_CONTENT_COMMAND_COPY         (NS_CONTENT_COMMAND_EVENT_START+1)
#define NS_CONTENT_COMMAND_PASTE        (NS_CONTENT_COMMAND_EVENT_START+2)
#define NS_CONTENT_COMMAND_DELETE       (NS_CONTENT_COMMAND_EVENT_START+3)
#define NS_CONTENT_COMMAND_UNDO         (NS_CONTENT_COMMAND_EVENT_START+4)
#define NS_CONTENT_COMMAND_REDO         (NS_CONTENT_COMMAND_EVENT_START+5)
#define NS_CONTENT_COMMAND_PASTE_TRANSFERABLE (NS_CONTENT_COMMAND_EVENT_START+6)
// NS_CONTENT_COMMAND_SCROLL scrolls the nearest scrollable element to the
// currently focused content or latest DOM selection. This would normally be
// the same element scrolled by keyboard scroll commands, except that this event
// will scroll an element scrollable in either direction.  I.e., if the nearest
// scrollable ancestor element can only be scrolled vertically, and horizontal
// scrolling is requested using this event, no scrolling will occur.
#define NS_CONTENT_COMMAND_SCROLL       (NS_CONTENT_COMMAND_EVENT_START+7)

// Event to gesture notification
#define NS_GESTURENOTIFY_EVENT_START 3900

#define NS_ORIENTATION_EVENT         4000

#define NS_SCROLLAREA_EVENT_START    4100
#define NS_SCROLLEDAREACHANGED       (NS_SCROLLAREA_EVENT_START)

#define NS_TRANSITION_EVENT_START    4200
#define NS_TRANSITION_END            (NS_TRANSITION_EVENT_START)

#define NS_ANIMATION_EVENT_START     4250
#define NS_ANIMATION_START           (NS_ANIMATION_EVENT_START)
#define NS_ANIMATION_END             (NS_ANIMATION_EVENT_START + 1)
#define NS_ANIMATION_ITERATION       (NS_ANIMATION_EVENT_START + 2)

#define NS_SMIL_TIME_EVENT_START     4300
#define NS_SMIL_BEGIN                (NS_SMIL_TIME_EVENT_START)
#define NS_SMIL_END                  (NS_SMIL_TIME_EVENT_START + 1)
#define NS_SMIL_REPEAT               (NS_SMIL_TIME_EVENT_START + 2)

#define NS_MOZTOUCH_EVENT_START      4400
#define NS_MOZTOUCH_DOWN             (NS_MOZTOUCH_EVENT_START)
#define NS_MOZTOUCH_MOVE             (NS_MOZTOUCH_EVENT_START+1)
#define NS_MOZTOUCH_UP               (NS_MOZTOUCH_EVENT_START+2)

// script notification events
#define NS_NOTIFYSCRIPT_START        4500
#define NS_BEFORE_SCRIPT_EXECUTE     (NS_NOTIFYSCRIPT_START)
#define NS_AFTER_SCRIPT_EXECUTE      (NS_NOTIFYSCRIPT_START+1)

#define NS_PRINT_EVENT_START         4600
#define NS_BEFOREPRINT               (NS_PRINT_EVENT_START)
#define NS_AFTERPRINT                (NS_PRINT_EVENT_START + 1)

#define NS_MESSAGE_EVENT_START       4700
#define NS_MESSAGE                   (NS_MESSAGE_EVENT_START)

// Open and close events
#define NS_OPENCLOSE_EVENT_START     4800
#define NS_OPEN                      (NS_OPENCLOSE_EVENT_START)
#define NS_CLOSE                     (NS_OPENCLOSE_EVENT_START+1)

// Device motion and orientation
#define NS_DEVICE_ORIENTATION_START  4900
#define NS_DEVICE_ORIENTATION        (NS_DEVICE_ORIENTATION_START)
#define NS_DEVICE_MOTION             (NS_DEVICE_ORIENTATION_START+1)
#define NS_DEVICE_PROXIMITY          (NS_DEVICE_ORIENTATION_START+2)
#define NS_USER_PROXIMITY            (NS_DEVICE_ORIENTATION_START+3)
#define NS_DEVICE_LIGHT              (NS_DEVICE_ORIENTATION_START+4)

#define NS_SHOW_EVENT                5000

// Fullscreen DOM API
#define NS_FULL_SCREEN_START         5100
#define NS_FULLSCREENCHANGE          (NS_FULL_SCREEN_START)
#define NS_FULLSCREENERROR           (NS_FULL_SCREEN_START + 1)

#define NS_TOUCH_EVENT_START         5200
#define NS_TOUCH_START               (NS_TOUCH_EVENT_START)
#define NS_TOUCH_MOVE                (NS_TOUCH_EVENT_START+1)
#define NS_TOUCH_END                 (NS_TOUCH_EVENT_START+2)
#define NS_TOUCH_ENTER               (NS_TOUCH_EVENT_START+3)
#define NS_TOUCH_LEAVE               (NS_TOUCH_EVENT_START+4)
#define NS_TOUCH_CANCEL              (NS_TOUCH_EVENT_START+5)

// Pointerlock DOM API
#define NS_POINTERLOCK_START         5300
#define NS_POINTERLOCKCHANGE         (NS_POINTERLOCK_START)
#define NS_POINTERLOCKERROR          (NS_POINTERLOCK_START + 1)

/**
 * Return status for event processors, nsEventStatus, is defined in
 * nsEvent.h.
 */

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
  nsEvent(bool isTrusted, PRUint32 msg, PRUint8 structType)
    : eventStructType(structType),
      message(msg),
      refPoint(0, 0),
      lastRefPoint(0, 0),
      time(0),
      flags(isTrusted ? NS_EVENT_FLAG_TRUSTED : NS_EVENT_FLAG_NONE),
      userType(0)
  {
    MOZ_COUNT_CTOR(nsEvent);
  }

  nsEvent()
  {
    MOZ_COUNT_CTOR(nsEvent);
  }

public:
  nsEvent(bool isTrusted, PRUint32 msg)
    : eventStructType(NS_EVENT),
      message(msg),
      refPoint(0, 0),
      lastRefPoint(0, 0),
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

  nsEvent(const nsEvent& aOther)
  {
    MOZ_COUNT_CTOR(nsEvent);
    *this = aOther;
  }

  // See event struct types
  PRUint8     eventStructType;
  // See GUI MESSAGES,
  PRUint32    message;
  // Relative to the widget of the event, or if there is no widget then it is
  // in screen coordinates. Not modified by layout code.
  nsIntPoint  refPoint;
  // The previous refPoint, if known, used to calculate mouse movement deltas.
  nsIntPoint  lastRefPoint;
  // Elapsed time, in milliseconds, from a platform-specific zero time
  // to the time the message was created
  PRUint64    time;
  // Flags to hold event flow stage and capture/bubble cancellation
  // status. This is used also to indicate whether the event is trusted.
  PRUint32    flags;
  // Additional type info for user defined events
  nsCOMPtr<nsIAtom>     userType;
  // Event targets, needed by DOM Events
  nsCOMPtr<nsIDOMEventTarget> target;
  nsCOMPtr<nsIDOMEventTarget> currentTarget;
  nsCOMPtr<nsIDOMEventTarget> originalTarget;
};

/**
 * General graphic user interface event
 */

class nsGUIEvent : public nsEvent
{
protected:
  nsGUIEvent(bool isTrusted, PRUint32 msg, nsIWidget *w, PRUint8 structType)
    : nsEvent(isTrusted, msg, structType),
      widget(w), pluginEvent(nsnull)
  {
  }

  nsGUIEvent()
    : pluginEvent(nsnull)
  {
  }

public:
  nsGUIEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsEvent(isTrusted, msg, NS_GUI_EVENT),
      widget(w), pluginEvent(nsnull)
  {
  }

  /// Originator of the event
  nsCOMPtr<nsIWidget> widget;

  /// Event for NPAPI plugin
  void* pluginEvent;
};

/**
 * Script error event
 */

class nsScriptErrorEvent : public nsEvent
{
public:
  nsScriptErrorEvent(bool isTrusted, PRUint32 msg)
    : nsEvent(isTrusted, msg, NS_SCRIPT_ERROR_EVENT),
      lineNr(0), errorMsg(nsnull), fileName(nsnull)
  {
  }

  PRInt32           lineNr;
  const PRUnichar*  errorMsg;
  const PRUnichar*  fileName;
};

/**
 * Window resize event
 */

class nsSizeEvent : public nsGUIEvent
{
public:
  nsSizeEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_SIZE_EVENT),
      windowSize(nsnull), mWinWidth(0), mWinHeight(0)
  {
  }

  /// x,y width, height in pixels (client area)
  nsIntRect       *windowSize;
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
  nsSizeModeEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
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
  nsZLevelEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_ZLEVEL_EVENT),
      mPlacement(nsWindowZTop), mReqBelow(nsnull), mActualBelow(nsnull),
      mImmediate(false), mAdjusted(false)
  {
  }

  nsWindowZ  mPlacement;
  nsIWidget *mReqBelow,    // widget we request being below, if any
            *mActualBelow; // widget to be below, returned by handler
  bool       mImmediate,   // handler should make changes immediately
             mAdjusted;    // handler changed placement
};

/**
 * Window repaint event
 */

class nsPaintEvent : public nsGUIEvent
{
public:
  nsPaintEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_PAINT_EVENT),
      willSendDidPaint(false),
      didSendWillPaint(false)
  {
  }

  // area that needs repainting
  nsIntRegion region;
  bool willSendDidPaint;
  bool didSendWillPaint;
};

/**
 * Scrollbar event
 */

class nsScrollbarEvent : public nsGUIEvent
{
public:
  nsScrollbarEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
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

  nsScrollPortEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_SCROLLPORT_EVENT),
      orient(vertical)
  {
  }

  orientType orient;
};

class nsScrollAreaEvent : public nsGUIEvent
{
public:
  nsScrollAreaEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_SCROLLAREA_EVENT)
  {
  }

  nsRect mArea;
};

class nsInputEvent : public nsGUIEvent
{
protected:
  nsInputEvent(bool isTrusted, PRUint32 msg, nsIWidget *w,
               PRUint8 structType)
    : nsGUIEvent(isTrusted, msg, w, structType),
      modifiers(0)
  {
  }

  nsInputEvent()
  {
  }

public:
  nsInputEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_INPUT_EVENT),
      modifiers(0)
  {
  }

  // true indicates the shift key is down
  bool IsShift() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_SHIFT) != 0);
  }
  // true indicates the control key is down
  bool IsControl() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_CONTROL) != 0);
  }
  // true indicates the alt key is down
  bool IsAlt() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_ALT) != 0);
  }
  // true indicates the meta key is down (or, on Mac, the Command key)
  bool IsMeta() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_META) != 0);
  }
  // true indicates the win key is down on Windows. Or the Super or Hyper key
  // is down on Linux.
  bool IsOS() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_OS) != 0);
  }
  // true indicates the alt graph key is down
  // NOTE: on Mac, the option key press causes both IsAlt() and IsAltGrpah()
  //       return true.
  bool IsAltGraph() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_ALTGRAPH) != 0);
  }
  // true indeicates the CapLock LED is turn on.
  bool IsCapsLocked() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_CAPSLOCK) != 0);
  }
  // true indeicates the NumLock LED is turn on.
  bool IsNumLocked() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_NUMLOCK) != 0);
  }
  // true indeicates the ScrollLock LED is turn on.
  bool IsScrollLocked() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_SCROLLLOCK) != 0);
  }

  // true indeicates the Fn key is down, but this is not supported by native
  // key event on any platform.
  bool IsFn() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_FN) != 0);
  }
  // true indeicates the ScrollLock LED is turn on.
  bool IsSymbolLocked() const
  {
    return ((modifiers & mozilla::widget::MODIFIER_SYMBOLLOCK) != 0);
  }

  void InitBasicModifiers(bool aCtrlKey,
                          bool aAltKey,
                          bool aShiftKey,
                          bool aMetaKey)
  {
    modifiers = 0;
    if (aCtrlKey) {
      modifiers |= mozilla::widget::MODIFIER_CONTROL;
    }
    if (aAltKey) {
      modifiers |= mozilla::widget::MODIFIER_ALT;
    }
    if (aShiftKey) {
      modifiers |= mozilla::widget::MODIFIER_SHIFT;
    }
    if (aMetaKey) {
      modifiers |= mozilla::widget::MODIFIER_META;
    }
  }

  mozilla::widget::Modifiers modifiers;
};

/**
 * Mouse event
 */

class nsMouseEvent_base : public nsInputEvent
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

public:

  nsMouseEvent_base()
  {
  }

  nsMouseEvent_base(bool isTrusted, PRUint32 msg, nsIWidget *w, PRUint8 type)
    : nsInputEvent(isTrusted, msg, w, type), button(0), buttons(0),
      pressure(0), inputSource(nsIDOMMouseEvent::MOZ_SOURCE_MOUSE) {}

  /// The possible related target
  nsCOMPtr<nsISupports> relatedTarget;

  PRInt16               button;
  PRInt16               buttons;

  // Finger or touch pressure of event
  // ranges between 0.0 and 1.0
  float                 pressure;

  // Possible values at nsIDOMMouseEvent
  PRUint16              inputSource;
};

class nsMouseEvent : public nsMouseEvent_base
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

public:
  enum buttonType  { eLeftButton = 0, eMiddleButton = 1, eRightButton = 2 };
  enum buttonsFlag { eLeftButtonFlag   = 0x01,
                     eRightButtonFlag  = 0x02,
                     eMiddleButtonFlag = 0x04,
                     // typicall, "back" button being left side of 5-button
                     // mice, see "buttons" attribute document of DOM3 Events.
                     e4thButtonFlag    = 0x08,
                     // typicall, "forward" button being right side of 5-button
                     // mice, see "buttons" attribute document of DOM3 Events.
                     e5thButtonFlag    = 0x10 };
  enum reasonType  { eReal, eSynthesized };
  enum contextType { eNormal, eContextMenuKey };
  enum exitType    { eChild, eTopLevel };

  nsMouseEvent()
  {
  }

protected:
  nsMouseEvent(bool isTrusted, PRUint32 msg, nsIWidget *w,
               PRUint8 structType, reasonType aReason)
    : nsMouseEvent_base(isTrusted, msg, w, structType),
      acceptActivation(false), ignoreRootScrollFrame(false),
      reason(aReason), context(eNormal), exit(eChild), clickCount(0)
  {
    switch (msg) {
      case NS_MOUSE_MOVE:
        flags |= NS_EVENT_FLAG_CANT_CANCEL;
        break;
      case NS_MOUSEENTER:
      case NS_MOUSELEAVE:
        flags |= (NS_EVENT_FLAG_CANT_CANCEL & NS_EVENT_FLAG_CANT_BUBBLE);
        break;
      default:
        break;
    }
  }

public:

  nsMouseEvent(bool isTrusted, PRUint32 msg, nsIWidget *w,
               reasonType aReason, contextType aContext = eNormal)
    : nsMouseEvent_base(isTrusted, msg, w, NS_MOUSE_EVENT),
      acceptActivation(false), ignoreRootScrollFrame(false),
      reason(aReason), context(aContext), exit(eChild), clickCount(0)
  {
    switch (msg) {
      case NS_MOUSE_MOVE:
        flags |= NS_EVENT_FLAG_CANT_CANCEL;
        break;
      case NS_MOUSEENTER:
      case NS_MOUSELEAVE:
        flags |= (NS_EVENT_FLAG_CANT_CANCEL | NS_EVENT_FLAG_CANT_BUBBLE);
        break;
      case NS_CONTEXTMENU:
        button = (context == eNormal) ? eRightButton : eLeftButton;
        break;
      default:
        break;
    }
  }

#ifdef DEBUG
  ~nsMouseEvent() {
    NS_WARN_IF_FALSE(message != NS_CONTEXTMENU ||
                     button ==
                       ((context == eNormal) ? eRightButton : eLeftButton),
                     "Wrong button set to NS_CONTEXTMENU event?");
  }
#endif

  /// Special return code for MOUSE_ACTIVATE to signal
  /// if the target accepts activation (1), or denies it (0)
  bool acceptActivation;
  // Whether the event should ignore scroll frame bounds
  // during dispatch.
  bool ignoreRootScrollFrame;

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
  nsDragEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsMouseEvent(isTrusted, msg, w, NS_DRAG_EVENT, eReal),
      userCancelled(false)
  {
    if (msg == NS_DRAGDROP_EXIT_SYNTH ||
        msg == NS_DRAGDROP_LEAVE_SYNTH ||
        msg == NS_DRAGDROP_END) {
      flags |= NS_EVENT_FLAG_CANT_CANCEL;
    }
  }

  nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
  bool userCancelled;
};

#ifdef ACCESSIBILITY
/**
 * Accessible event
 */

class nsAccessibleEvent : public nsInputEvent
{
public:
  nsAccessibleEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsInputEvent(isTrusted, msg, w, NS_ACCESSIBLE_EVENT),
      mAccessible(nsnull)
  {
  }

  Accessible *mAccessible;
};
#endif

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
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

public:
  nsKeyEvent()
  {
  }

  nsKeyEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsInputEvent(isTrusted, msg, w, NS_KEY_EVENT),
      keyCode(0), charCode(0),
      location(nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD), isChar(0)
  {
  }

  /// see NS_VK codes
  PRUint32        keyCode;   
  /// OS translated Unicode char
  PRUint32        charCode;
  // One of nsIDOMKeyEvent::DOM_KEY_LOCATION_*
  PRUint32        location;
  // OS translated Unicode chars which are used for accesskey and accelkey
  // handling. The handlers will try from first character to last character.
  nsTArray<nsAlternativeCharCode> alternativeCharCodes;
  // indicates whether the event signifies a printable character
  bool            isChar;
};

/**
 * IME Related Events
 */
 
struct nsTextRangeStyle
{
  enum {
    LINESTYLE_NONE   = NS_STYLE_TEXT_DECORATION_STYLE_NONE,
    LINESTYLE_SOLID  = NS_STYLE_TEXT_DECORATION_STYLE_SOLID,
    LINESTYLE_DOTTED = NS_STYLE_TEXT_DECORATION_STYLE_DOTTED,
    LINESTYLE_DASHED = NS_STYLE_TEXT_DECORATION_STYLE_DASHED,
    LINESTYLE_DOUBLE = NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE,
    LINESTYLE_WAVY   = NS_STYLE_TEXT_DECORATION_STYLE_WAVY
  };

  enum {
    DEFINED_NONE             = 0x00,
    DEFINED_LINESTYLE        = 0x01,
    DEFINED_FOREGROUND_COLOR = 0x02,
    DEFINED_BACKGROUND_COLOR = 0x04,
    DEFINED_UNDERLINE_COLOR  = 0x08
  };

  // Initialize all members, because nsTextRange instances may be compared by
  // memcomp.
  nsTextRangeStyle()
  {
    Clear();
  }

  void Clear()
  {
    mDefinedStyles = DEFINED_NONE;
    mLineStyle = LINESTYLE_NONE;
    mIsBoldLine = false;
    mForegroundColor = mBackgroundColor = mUnderlineColor = NS_RGBA(0, 0, 0, 0);
  }

  bool IsDefined() const { return mDefinedStyles != DEFINED_NONE; }

  bool IsLineStyleDefined() const
  {
    return (mDefinedStyles & DEFINED_LINESTYLE) != 0;
  }

  bool IsForegroundColorDefined() const
  {
    return (mDefinedStyles & DEFINED_FOREGROUND_COLOR) != 0;
  }

  bool IsBackgroundColorDefined() const
  {
    return (mDefinedStyles & DEFINED_BACKGROUND_COLOR) != 0;
  }

  bool IsUnderlineColorDefined() const
  {
    return (mDefinedStyles & DEFINED_UNDERLINE_COLOR) != 0;
  }

  bool IsNoChangeStyle() const
  {
    return !IsForegroundColorDefined() && !IsBackgroundColorDefined() &&
           IsLineStyleDefined() && mLineStyle == LINESTYLE_NONE;
  }

  bool Equals(const nsTextRangeStyle& aOther)
  {
    if (mDefinedStyles != aOther.mDefinedStyles)
      return false;
    if (IsLineStyleDefined() && (mLineStyle != aOther.mLineStyle ||
                                 !mIsBoldLine != !aOther.mIsBoldLine))
      return false;
    if (IsForegroundColorDefined() &&
        (mForegroundColor != aOther.mForegroundColor))
      return false;
    if (IsBackgroundColorDefined() &&
        (mBackgroundColor != aOther.mBackgroundColor))
      return false;
    if (IsUnderlineColorDefined() &&
        (mUnderlineColor != aOther.mUnderlineColor))
      return false;
    return true;
  }

  bool operator !=(const nsTextRangeStyle &aOther)
  {
    return !Equals(aOther);
  }

  bool operator ==(const nsTextRangeStyle &aOther)
  {
    return Equals(aOther);
  }

  PRUint8 mDefinedStyles;
  PRUint8 mLineStyle;        // DEFINED_LINESTYLE

  bool mIsBoldLine;  // DEFINED_LINESTYLE

  nscolor mForegroundColor;  // DEFINED_FOREGROUND_COLOR
  nscolor mBackgroundColor;  // DEFINED_BACKGROUND_COLOR
  nscolor mUnderlineColor;   // DEFINED_UNDERLINE_COLOR
};

struct nsTextRange
{
  nsTextRange()
    : mStartOffset(0), mEndOffset(0), mRangeType(0)
  {
  }

  PRUint32 mStartOffset;
  PRUint32 mEndOffset;
  PRUint32 mRangeType;

  nsTextRangeStyle mRangeStyle;
};

typedef nsTextRange* nsTextRangeArray;

class nsTextEvent : public nsInputEvent
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;
  friend class mozilla::plugins::PPluginInstanceChild;

  nsTextEvent()
  {
  }

public:
  PRUint32 seqno;

public:
  nsTextEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsInputEvent(isTrusted, msg, w, NS_TEXT_EVENT),
      rangeCount(0), rangeArray(nsnull), isChar(false)
  {
  }

  nsString          theText;
  PRUint32          rangeCount;
  // Note that the range array may not specify a caret position; in that
  // case there will be no range of type NS_TEXTRANGE_CARETPOSITION in the
  // array.
  nsTextRangeArray  rangeArray;
  bool              isChar;
};

class nsCompositionEvent : public nsGUIEvent
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  nsCompositionEvent()
  {
  }

public:
  PRUint32 seqno;

public:
  nsCompositionEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_COMPOSITION_EVENT)
  {
    // XXX compositionstart is cancelable in draft of DOM3 Events.
    //     However, it doesn't make sense for us, we cannot cancel composition
    //     when we send compositionstart event.
    flags |= NS_EVENT_FLAG_CANT_CANCEL;
  }

  nsString data;
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
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  nsMouseScrollEvent()
  {
  }

public:
  enum nsMouseScrollFlags {
    kIsFullPage =   1 << 0,
    kIsVertical =   1 << 1,
    kIsHorizontal = 1 << 2,
    kHasPixels =    1 << 3, // Marks line scroll events that are provided as
                            // a fallback for pixel scroll events.
                            // These scroll events are used by things that can't
                            // be scrolled pixel-wise, like trees. You should
                            // ignore them when processing pixel scroll events
                            // to avoid double-processing the same scroll gesture.
                            // When kHasPixels is set, the event is guaranteed to
                            // be followed up by an event that contains pixel
                            // scrolling information.
    kNoLines =      1 << 4, // Marks pixel scroll events that will not be
                            // followed by a line scroll events. EventStateManager
                            // will compute the appropriate height/width based on
                            // view lineHeight and generate line scroll events
                            // as needed.
    kNoDefer =      1 << 5, // For scrollable views, indicates scroll should not
                            // occur asynchronously.
    kIsMomentum =   1 << 6, // Marks scroll events that aren't controlled by the
                            // user but fire automatically as the result of a
                            // "momentum" scroll.
    kAllowSmoothScroll = 1 << 7, // Allow smooth scroll for the pixel scroll
                                 // event.
    kFromLines =    1 << 8  // For a pixels scroll event, indicates that it
                            // originated from a lines scroll event.
                            // (Only used on windows which generates "faked"
                            // pixel scroll events even for simple mouse wheel
                            // scroll)
};

  nsMouseScrollEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsMouseEvent_base(isTrusted, msg, w, NS_MOUSE_SCROLL_EVENT),
      scrollFlags(0), delta(0), scrollOverflow(0)
  {
  }

  PRInt32               scrollFlags;
  PRInt32               delta;
  PRInt32               scrollOverflow;
};

/*
 * Gesture Notify Event:
 *
 * This event is the first event generated when the user touches
 * the screen with a finger, and it's meant to decide what kind
 * of action we'll use for that touch interaction.
 *
 * The event is dispatched to the layout and based on what is underneath
 * the initial contact point it's then decided if we should pan
 * (finger scrolling) or drag the target element.
 */
class nsGestureNotifyEvent : public nsGUIEvent
{
public:
  enum ePanDirection {
    ePanNone,
    ePanVertical,
    ePanHorizontal,
    ePanBoth
  };
  
  ePanDirection panDirection;
  bool          displayPanFeedback;
  
  nsGestureNotifyEvent(bool aIsTrusted, PRUint32 aMsg, nsIWidget *aWidget):
    nsGUIEvent(aIsTrusted, aMsg, aWidget, NS_GESTURENOTIFY_EVENT),
    panDirection(ePanNone),
    displayPanFeedback(false)
  {
  }
};

class nsQueryContentEvent : public nsGUIEvent
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  nsQueryContentEvent()
  {
    mReply.mContentsRoot = nsnull;
    mReply.mFocusedWidget = nsnull;
  }

public:
  nsQueryContentEvent(bool aIsTrusted, PRUint32 aMsg, nsIWidget *aWidget) :
    nsGUIEvent(aIsTrusted, aMsg, aWidget, NS_QUERY_CONTENT_EVENT),
    mSucceeded(false), mWasAsync(false)
  {
  }

  void InitForQueryTextContent(PRUint32 aOffset, PRUint32 aLength)
  {
    NS_ASSERTION(message == NS_QUERY_TEXT_CONTENT,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
  }

  void InitForQueryCaretRect(PRUint32 aOffset)
  {
    NS_ASSERTION(message == NS_QUERY_CARET_RECT,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
  }

  void InitForQueryTextRect(PRUint32 aOffset, PRUint32 aLength)
  {
    NS_ASSERTION(message == NS_QUERY_TEXT_RECT,
                 "wrong initializer is called");
    mInput.mOffset = aOffset;
    mInput.mLength = aLength;
  }

  void InitForQueryDOMWidgetHittest(nsIntPoint& aPoint)
  {
    NS_ASSERTION(message == NS_QUERY_DOM_WIDGET_HITTEST,
                 "wrong initializer is called");
    refPoint = aPoint;
  }

  void InitForQueryScrollTargetInfo(nsMouseScrollEvent* aEvent)
  {
    NS_ASSERTION(message == NS_QUERY_SCROLL_TARGET_INFO,
                 "wrong initializer is called");
    mInput.mMouseScrollEvent = aEvent;
  }

  PRUint32 GetSelectionStart(void) const
  {
    NS_ASSERTION(message == NS_QUERY_SELECTED_TEXT,
                 "not querying selection");
    return mReply.mOffset + (mReply.mReversed ? mReply.mString.Length() : 0);
  }

  PRUint32 GetSelectionEnd(void) const
  {
    NS_ASSERTION(message == NS_QUERY_SELECTED_TEXT,
                 "not querying selection");
    return mReply.mOffset + (mReply.mReversed ? 0 : mReply.mString.Length());
  }

  bool mSucceeded;
  bool mWasAsync;
  struct {
    PRUint32 mOffset;
    PRUint32 mLength;
    // used by NS_QUERY_SCROLL_TARGET_INFO
    nsMouseScrollEvent* mMouseScrollEvent;
  } mInput;
  struct {
    void* mContentsRoot;
    PRUint32 mOffset;
    nsString mString;
    nsIntRect mRect; // Finally, the coordinates is system coordinates.
    // The return widget has the caret. This is set at all query events.
    nsIWidget* mFocusedWidget;
    bool mReversed; // true if selection is reversed (end < start)
    bool mHasSelection; // true if the selection exists
    bool mWidgetIsHit; // true if DOM element under mouse belongs to widget
    // used by NS_QUERY_SELECTION_AS_TRANSFERABLE
    nsCOMPtr<nsITransferable> mTransferable;
    // used by NS_QUERY_SCROLL_TARGET_INFO
    PRInt32 mLineHeight;
    PRInt32 mPageWidth;
    PRInt32 mPageHeight;
    // used by NS_QUERY_SCROLL_TARGET_INFO
    // the mouse wheel scrolling amount may be overridden by prefs or
    // overriding system scrolling speed mechanism.
    // If mMouseScrollEvent is a line scroll event, the unit of this value is
    // line.  If mMouseScrollEvent is a page scroll event, the unit of this
    // value is page.
    PRInt32 mComputedScrollAmount;
    PRInt32 mComputedScrollAction;
  } mReply;

  enum {
    NOT_FOUND = PR_UINT32_MAX
  };

  // values of mComputedScrollAction
  enum {
    SCROLL_ACTION_NONE,
    SCROLL_ACTION_LINE,
    SCROLL_ACTION_PAGE
  };
};

class nsFocusEvent : public nsEvent
{
public:
  nsFocusEvent(bool isTrusted, PRUint32 msg)
    : nsEvent(isTrusted, msg, NS_FOCUS_EVENT),
      fromRaise(false),
      isRefocus(false)
  {
  }

  bool fromRaise;
  bool isRefocus;
};

class nsSelectionEvent : public nsGUIEvent
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  nsSelectionEvent()
  {
  }

public:
  PRUint32 seqno;

public:
  nsSelectionEvent(bool aIsTrusted, PRUint32 aMsg, nsIWidget *aWidget) :
    nsGUIEvent(aIsTrusted, aMsg, aWidget, NS_SELECTION_EVENT),
    mExpandToClusterBoundary(true), mSucceeded(false)
  {
  }

  PRUint32 mOffset; // start offset of selection
  PRUint32 mLength; // length of selection
  bool mReversed; // selection "anchor" should be in front
  bool mExpandToClusterBoundary; // cluster-based or character-based
  bool mSucceeded;
};

class nsContentCommandEvent : public nsGUIEvent
{
public:
  nsContentCommandEvent(bool aIsTrusted, PRUint32 aMsg, nsIWidget *aWidget,
                        bool aOnlyEnabledCheck = false) :
    nsGUIEvent(aIsTrusted, aMsg, aWidget, NS_CONTENT_COMMAND_EVENT),
    mOnlyEnabledCheck(bool(aOnlyEnabledCheck)),
    mSucceeded(false), mIsEnabled(false)
  {
  }

  // NS_CONTENT_COMMAND_PASTE_TRANSFERABLE
  nsCOMPtr<nsITransferable> mTransferable;                 // [in]

  // NS_CONTENT_COMMAND_SCROLL
  // for mScroll.mUnit
  enum {
    eCmdScrollUnit_Line,
    eCmdScrollUnit_Page,
    eCmdScrollUnit_Whole
  };

  struct ScrollInfo {
    ScrollInfo() :
      mAmount(0), mUnit(eCmdScrollUnit_Line), mIsHorizontal(false)
    {
    }

    PRInt32      mAmount;                                  // [in]
    PRUint8      mUnit;                                    // [in]
    bool mIsHorizontal;                            // [in]
  } mScroll;

  bool mOnlyEnabledCheck;                          // [in]

  bool mSucceeded;                                 // [out]
  bool mIsEnabled;                                 // [out]
};

class nsMozTouchEvent : public nsMouseEvent_base
{
public:
  nsMozTouchEvent(bool isTrusted, PRUint32 msg, nsIWidget* w,
                  PRUint32 streamIdArg)
    : nsMouseEvent_base(isTrusted, msg, w, NS_MOZTOUCH_EVENT),
      streamId(streamIdArg)
  {
  }

  PRUint32 streamId;
};

class nsTouchEvent : public nsInputEvent
{
public:
  nsTouchEvent(bool isTrusted, nsTouchEvent *aEvent)
    : nsInputEvent(isTrusted,
                   aEvent->message,
                   aEvent->widget,
                   NS_TOUCH_EVENT)
  {
    modifiers = aEvent->modifiers;
    time = aEvent->time;
    touches.AppendElements(aEvent->touches);
    MOZ_COUNT_CTOR(nsTouchEvent);
  }
  nsTouchEvent(bool isTrusted, PRUint32 msg, nsIWidget* w)
    : nsInputEvent(isTrusted, msg, w, NS_TOUCH_EVENT)
  {
    MOZ_COUNT_CTOR(nsTouchEvent);
  }
  ~nsTouchEvent()
  {
    MOZ_COUNT_DTOR(nsTouchEvent);
  }

  nsTArray<nsCOMPtr<nsIDOMTouch> > touches;
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
  nsFormEvent(bool isTrusted, PRUint32 msg)
    : nsEvent(isTrusted, msg, NS_FORM_EVENT),
      originator(nsnull)
  {
  }

  nsIContent *originator;
};

/**
 * Command event
 *
 * Custom commands for example from the operating system.
 */

class nsCommandEvent : public nsGUIEvent
{
public:
  nsCommandEvent(bool isTrusted, nsIAtom* aEventType,
                 nsIAtom* aCommand, nsIWidget* w)
    : nsGUIEvent(isTrusted, NS_USER_DEFINED_EVENT, w, NS_COMMAND_EVENT)
  {
    userType = aEventType;
    command = aCommand;
  }

  nsCOMPtr<nsIAtom> command;
};

/**
 * DOM UIEvent
 */
class nsUIEvent : public nsEvent
{
public:
  nsUIEvent(bool isTrusted, PRUint32 msg, PRInt32 d)
    : nsEvent(isTrusted, msg, NS_UI_EVENT),
      detail(d)
  {
  }

  PRInt32 detail;
};

/**
 * Simple gesture event
 */
class nsSimpleGestureEvent : public nsMouseEvent_base
{
public:
  nsSimpleGestureEvent(bool isTrusted, PRUint32 msg, nsIWidget* w,
                         PRUint32 directionArg, PRFloat64 deltaArg)
    : nsMouseEvent_base(isTrusted, msg, w, NS_SIMPLE_GESTURE_EVENT),
      direction(directionArg), delta(deltaArg), clickCount(0)
  {
  }

  nsSimpleGestureEvent(const nsSimpleGestureEvent& other)
    : nsMouseEvent_base((other.flags & NS_EVENT_FLAG_TRUSTED) != 0,
                        other.message, other.widget, NS_SIMPLE_GESTURE_EVENT),
      direction(other.direction), delta(other.delta), clickCount(0)
  {
  }

  PRUint32 direction;   // See nsIDOMSimpleGestureEvent for values
  PRFloat64 delta;      // Delta for magnify and rotate events
  PRUint32 clickCount;  // The number of taps for tap events
};

class nsTransitionEvent : public nsEvent
{
public:
  nsTransitionEvent(bool isTrusted, PRUint32 msg,
                    const nsString &propertyNameArg, float elapsedTimeArg)
    : nsEvent(isTrusted, msg, NS_TRANSITION_EVENT),
      propertyName(propertyNameArg), elapsedTime(elapsedTimeArg)
  {
  }

  nsString propertyName;
  float elapsedTime;
};

class nsAnimationEvent : public nsEvent
{
public:
  nsAnimationEvent(bool isTrusted, PRUint32 msg,
                   const nsString &animationNameArg, float elapsedTimeArg)
    : nsEvent(isTrusted, msg, NS_ANIMATION_EVENT),
      animationName(animationNameArg), elapsedTime(elapsedTimeArg)
  {
  }

  nsString animationName;
  float elapsedTime;
};

class nsUIStateChangeEvent : public nsGUIEvent
{
public:
  nsUIStateChangeEvent(bool isTrusted, PRUint32 msg, nsIWidget* w)
    : nsGUIEvent(isTrusted, msg, w, NS_UISTATECHANGE_EVENT),
      showAccelerators(UIStateChangeType_NoChange),
      showFocusRings(UIStateChangeType_NoChange)
  {
  }

  UIStateChangeType showAccelerators;
  UIStateChangeType showFocusRings;
};

/**
 * Native event pluginEvent for plugins.
 */

class nsPluginEvent : public nsGUIEvent
{
public:
  nsPluginEvent(bool isTrusted, PRUint32 msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_PLUGIN_EVENT),
      retargetToFocusedDocument(false)
  {
  }

  // If TRUE, this event needs to be retargeted to focused document.
  // Otherwise, never retargeted.
  // Defaults to false.
  bool retargetToFocusedDocument;
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
        ((evnt)->message == NS_MOUSE_MOZHITTEST) || \
        ((evnt)->message == NS_MOUSE_MOVE))

#define NS_IS_MOUSE_EVENT_STRUCT(evnt) \
       ((evnt)->eventStructType == NS_MOUSE_EVENT || \
        (evnt)->eventStructType == NS_DRAG_EVENT)

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
        ((evnt)->message == NS_COMPOSITION_UPDATE))

#define NS_IS_ACTIVATION_EVENT(evnt) \
       (((evnt)->message == NS_ACTIVATE) || \
        ((evnt)->message == NS_DEACTIVATE) || \
        ((evnt)->message == NS_PLUGIN_ACTIVATE) || \
        ((evnt)->message == NS_PLUGIN_FOCUS))

#define NS_IS_QUERY_CONTENT_EVENT(evnt) \
       ((evnt)->eventStructType == NS_QUERY_CONTENT_EVENT)

#define NS_IS_SELECTION_EVENT(evnt) \
       (((evnt)->message == NS_SELECTION_SET))

#define NS_IS_CONTENT_COMMAND_EVENT(evnt) \
       ((evnt)->eventStructType == NS_CONTENT_COMMAND_EVENT)

#define NS_IS_PLUGIN_EVENT(evnt) \
       (((evnt)->message == NS_PLUGIN_INPUT_EVENT) || \
        ((evnt)->message == NS_PLUGIN_FOCUS_EVENT))

#define NS_IS_RETARGETED_PLUGIN_EVENT(evnt) \
       (NS_IS_PLUGIN_EVENT(evnt) && \
        (static_cast<nsPluginEvent*>(evnt)->retargetToFocusedDocument))

#define NS_IS_NON_RETARGETED_PLUGIN_EVENT(evnt) \
       (NS_IS_PLUGIN_EVENT(evnt) && \
        !(static_cast<nsPluginEvent*>(evnt)->retargetToFocusedDocument))

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

// Be aware the query content events and the selection events are a part of IME
// processing.  So, you shouldn't use NS_IS_IME_EVENT macro directly in most
// cases, you should use NS_IS_IME_RELATED_EVENT instead.
#define NS_IS_IME_RELATED_EVENT(evnt) \
  (NS_IS_IME_EVENT(evnt) || \
   (NS_IS_QUERY_CONTENT_EVENT(evnt) && \
    evnt->message != NS_QUERY_SCROLL_TARGET_INFO) || \
   NS_IS_SELECTION_EVENT(evnt))

/*
 * Virtual key bindings for keyboard events.
 * These come from nsIDOMKeyEvent.h, which is generated from MouseKeyEvent.idl.
 * Really, it would be better if we phased out the NS_VK symbols altogether
 * in favor of the DOM ones, but at least this way they'll be in sync.
 */

enum {
#define NS_DEFINE_VK(aDOMKeyName, aDOMKeyCode) NS_##aDOMKeyName = aDOMKeyCode
#include "nsVKList.h"
#undef NS_DEFINE_VK
};

// IME Constants  -- keep in synch with nsIPrivateTextRange.h
#define NS_TEXTRANGE_CARETPOSITION         0x01
#define NS_TEXTRANGE_RAWINPUT              0x02
#define NS_TEXTRANGE_SELECTEDRAWTEXT       0x03
#define NS_TEXTRANGE_CONVERTEDTEXT         0x04
#define NS_TEXTRANGE_SELECTEDCONVERTEDTEXT 0x05

/**
 * Whether the event should be handled by the frame of the mouse cursor
 * position or not.  When it should be handled there (e.g., the mouse events),
 * this returns TRUE.
 */
inline bool NS_IsEventUsingCoordinates(nsEvent* aEvent)
{
  return !NS_IS_KEY_EVENT(aEvent) && !NS_IS_IME_RELATED_EVENT(aEvent) &&
         !NS_IS_CONTEXT_MENU_KEY(aEvent) && !NS_IS_ACTIVATION_EVENT(aEvent) &&
         !NS_IS_PLUGIN_EVENT(aEvent) &&
         !NS_IS_CONTENT_COMMAND_EVENT(aEvent) &&
         aEvent->eventStructType != NS_ACCESSIBLE_EVENT;
}

/**
 * Whether the event should be handled by the focused DOM window in the
 * same top level window's or not.  E.g., key events, IME related events
 * (including the query content events, they are used in IME transaction)
 * should be handled by the (last) focused window rather than the dispatched
 * window.
 *
 * NOTE: Even if this returns TRUE, the event isn't going to be handled by the
 * application level active DOM window which is on another top level window.
 * So, when the event is fired on a deactive window, the event is going to be
 * handled by the last focused DOM window in the last focused window.
 */
inline bool NS_IsEventTargetedAtFocusedWindow(nsEvent* aEvent)
{
  return NS_IS_KEY_EVENT(aEvent) || NS_IS_IME_RELATED_EVENT(aEvent) ||
         NS_IS_CONTEXT_MENU_KEY(aEvent) ||
         NS_IS_CONTENT_COMMAND_EVENT(aEvent) ||
         NS_IS_RETARGETED_PLUGIN_EVENT(aEvent);
}

/**
 * Whether the event should be handled by the focused content or not.  E.g.,
 * key events, IME related events and other input events which are not handled
 * by the frame of the mouse cursor position.
 *
 * NOTE: Even if this returns TRUE, the event isn't going to be handled by the
 * application level active DOM window which is on another top level window.
 * So, when the event is fired on a deactive window, the event is going to be
 * handled by the last focused DOM element of the last focused DOM window in
 * the last focused window.
 */
inline bool NS_IsEventTargetedAtFocusedContent(nsEvent* aEvent)
{
  return NS_IS_KEY_EVENT(aEvent) || NS_IS_IME_RELATED_EVENT(aEvent) ||
         NS_IS_CONTEXT_MENU_KEY(aEvent) ||
         NS_IS_RETARGETED_PLUGIN_EVENT(aEvent);
}

#endif // nsGUIEvent_h__
