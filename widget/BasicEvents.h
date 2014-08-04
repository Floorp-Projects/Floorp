/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BasicEvents_h__
#define mozilla_BasicEvents_h__

#include <stdint.h>

#include "mozilla/dom/EventTarget.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsISupportsImpl.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "Units.h"

/******************************************************************************
 * Messages
 *
 * TODO: Make them enum.
 ******************************************************************************/

#define NS_EVENT_NULL                   0

// This is a dummy event message for all event listener implementation in
// EventListenerManager.
#define NS_EVENT_ALL                    1

#define NS_WINDOW_START                 100

// Widget may be destroyed
#define NS_XUL_CLOSE                    (NS_WINDOW_START + 1)
// Key is pressed within a window
#define NS_KEY_PRESS                    (NS_WINDOW_START + 31)
// Key is released within a window
#define NS_KEY_UP                       (NS_WINDOW_START + 32)
// Key is pressed within a window
#define NS_KEY_DOWN                     (NS_WINDOW_START + 33)

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

// NS_BEFORERESIZE_EVENT used to be here (NS_WINDOW_START + 66)

// Indicates that the user is either idle or active
#define NS_MOZ_USER_IDLE                 (NS_WINDOW_START + 67)
#define NS_MOZ_USER_ACTIVE               (NS_WINDOW_START + 68)

// The resolution at which a plugin should draw has changed, for
// example as the result of changing from a HiDPI mode to a non-
// HiDPI mode.
#define NS_PLUGIN_RESOLUTION_CHANGED     (NS_WINDOW_START + 69)

#define NS_LANGUAGECHANGE                (NS_WINDOW_START + 70)

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
#define NS_MOUSE_MOZLONGTAP             (NS_MOUSE_MESSAGE_START + 36)

// Pointer spec events
#define NS_POINTER_EVENT_START          4400
#define NS_POINTER_MOVE                 (NS_POINTER_EVENT_START)
#define NS_POINTER_UP                   (NS_POINTER_EVENT_START + 1)
#define NS_POINTER_DOWN                 (NS_POINTER_EVENT_START + 2)
#define NS_POINTER_OVER                 (NS_POINTER_EVENT_START + 22)
#define NS_POINTER_OUT                  (NS_POINTER_EVENT_START + 23)
#define NS_POINTER_ENTER                (NS_POINTER_EVENT_START + 24)
#define NS_POINTER_LEAVE                (NS_POINTER_EVENT_START + 25)
#define NS_POINTER_CANCEL               (NS_POINTER_EVENT_START + 26)
#define NS_POINTER_GOT_CAPTURE          (NS_POINTER_EVENT_START + 27)
#define NS_POINTER_LOST_CAPTURE         (NS_POINTER_EVENT_START + 28)

#define NS_CONTEXTMENU_MESSAGE_START    500
#define NS_CONTEXTMENU                  (NS_CONTEXTMENU_MESSAGE_START)

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
#define NS_FORM_INVALID                 (NS_FORM_EVENT_START + 4)

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

#define NS_MUTATION_START                    1800
#define NS_MUTATION_SUBTREEMODIFIED          (NS_MUTATION_START)
#define NS_MUTATION_NODEINSERTED             (NS_MUTATION_START+1)
#define NS_MUTATION_NODEREMOVED              (NS_MUTATION_START+2)
#define NS_MUTATION_NODEREMOVEDFROMDOCUMENT  (NS_MUTATION_START+3)
#define NS_MUTATION_NODEINSERTEDINTODOCUMENT (NS_MUTATION_START+4)
#define NS_MUTATION_ATTRMODIFIED             (NS_MUTATION_START+5)
#define NS_MUTATION_CHARACTERDATAMODIFIED    (NS_MUTATION_START+6)
#define NS_MUTATION_END                      (NS_MUTATION_START+6)

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
// rect.  The point is specified by Event::refPoint.
#define NS_QUERY_CHARACTER_AT_POINT     (NS_QUERY_CONTENT_EVENT_START + 8)
// Query if the DOM element under Event::refPoint belongs to our widget
// or not.
#define NS_QUERY_DOM_WIDGET_HITTEST     (NS_QUERY_CONTENT_EVENT_START + 9)

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
#define NS_NEED_KEY            (NS_MEDIA_EVENT_START+20)

// paint notification events
#define NS_NOTIFYPAINT_START    3400
#define NS_AFTERPAINT           (NS_NOTIFYPAINT_START)

// Simple gesture events
#define NS_SIMPLE_GESTURE_EVENT_START    3500
#define NS_SIMPLE_GESTURE_SWIPE_START    (NS_SIMPLE_GESTURE_EVENT_START)
#define NS_SIMPLE_GESTURE_SWIPE_UPDATE   (NS_SIMPLE_GESTURE_EVENT_START+1)
#define NS_SIMPLE_GESTURE_SWIPE_END      (NS_SIMPLE_GESTURE_EVENT_START+2)
#define NS_SIMPLE_GESTURE_SWIPE          (NS_SIMPLE_GESTURE_EVENT_START+3)
#define NS_SIMPLE_GESTURE_MAGNIFY_START  (NS_SIMPLE_GESTURE_EVENT_START+4)
#define NS_SIMPLE_GESTURE_MAGNIFY_UPDATE (NS_SIMPLE_GESTURE_EVENT_START+5)
#define NS_SIMPLE_GESTURE_MAGNIFY        (NS_SIMPLE_GESTURE_EVENT_START+6)
#define NS_SIMPLE_GESTURE_ROTATE_START   (NS_SIMPLE_GESTURE_EVENT_START+7)
#define NS_SIMPLE_GESTURE_ROTATE_UPDATE  (NS_SIMPLE_GESTURE_EVENT_START+8)
#define NS_SIMPLE_GESTURE_ROTATE         (NS_SIMPLE_GESTURE_EVENT_START+9)
#define NS_SIMPLE_GESTURE_TAP            (NS_SIMPLE_GESTURE_EVENT_START+10)
#define NS_SIMPLE_GESTURE_PRESSTAP       (NS_SIMPLE_GESTURE_EVENT_START+11)
#define NS_SIMPLE_GESTURE_EDGE_STARTED   (NS_SIMPLE_GESTURE_EVENT_START+12)
#define NS_SIMPLE_GESTURE_EDGE_CANCELED  (NS_SIMPLE_GESTURE_EVENT_START+13)
#define NS_SIMPLE_GESTURE_EDGE_COMPLETED (NS_SIMPLE_GESTURE_EVENT_START+14)

// These are used to send native events to plugins.
#define NS_PLUGIN_EVENT_START            3600
#define NS_PLUGIN_INPUT_EVENT            (NS_PLUGIN_EVENT_START)
#define NS_PLUGIN_FOCUS_EVENT            (NS_PLUGIN_EVENT_START+1)

// Events to manipulate selection (WidgetSelectionEvent)
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

#define NS_WEBAUDIO_EVENT_START      4350
#define NS_AUDIO_PROCESS             (NS_WEBAUDIO_EVENT_START)
#define NS_AUDIO_COMPLETE            (NS_WEBAUDIO_EVENT_START + 1)

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
#define NS_TOUCH_CANCEL              (NS_TOUCH_EVENT_START+3)

// Pointerlock DOM API
#define NS_POINTERLOCK_START         5300
#define NS_POINTERLOCKCHANGE         (NS_POINTERLOCK_START)
#define NS_POINTERLOCKERROR          (NS_POINTERLOCK_START + 1)

#define NS_WHEEL_EVENT_START         5400
#define NS_WHEEL_WHEEL               (NS_WHEEL_EVENT_START)
#define NS_WHEEL_START               (NS_WHEEL_EVENT_START + 1)
#define NS_WHEEL_STOP                (NS_WHEEL_EVENT_START + 2)

//System time is changed
#define NS_MOZ_TIME_CHANGE_EVENT     5500

// Network packet events.
#define NS_NETWORK_EVENT_START       5600
#define NS_NETWORK_UPLOAD_EVENT      (NS_NETWORK_EVENT_START + 1)
#define NS_NETWORK_DOWNLOAD_EVENT    (NS_NETWORK_EVENT_START + 2)

// MediaRecorder events.
#define NS_MEDIARECORDER_EVENT_START 5700
#define NS_MEDIARECORDER_DATAAVAILABLE  (NS_MEDIARECORDER_EVENT_START + 1)
#define NS_MEDIARECORDER_WARNING        (NS_MEDIARECORDER_EVENT_START + 2)
#define NS_MEDIARECORDER_STOP           (NS_MEDIARECORDER_EVENT_START + 3)

// SpeakerManager events
#define NS_SPEAKERMANAGER_EVENT_START 5800
#define NS_SPEAKERMANAGER_SPEAKERFORCEDCHANGE (NS_SPEAKERMANAGER_EVENT_START + 1)

#ifdef MOZ_GAMEPAD
// Gamepad input events
#define NS_GAMEPAD_START         6000
#define NS_GAMEPAD_BUTTONDOWN    (NS_GAMEPAD_START)
#define NS_GAMEPAD_BUTTONUP      (NS_GAMEPAD_START+1)
#define NS_GAMEPAD_AXISMOVE      (NS_GAMEPAD_START+2)
#define NS_GAMEPAD_CONNECTED     (NS_GAMEPAD_START+3)
#define NS_GAMEPAD_DISCONNECTED  (NS_GAMEPAD_START+4)
// Keep this defined to the same value as the event above
#define NS_GAMEPAD_END           (NS_GAMEPAD_START+4)
#endif

// input and beforeinput events.
#define NS_EDITOR_EVENT_START    6100
#define NS_EDITOR_INPUT          (NS_EDITOR_EVENT_START)

namespace IPC {
template<typename T>
struct ParamTraits;
}

namespace mozilla {

/******************************************************************************
 * mozilla::BaseEventFlags
 *
 * BaseEventFlags must be a POD struct for safe to use memcpy (including
 * in ParamTraits<BaseEventFlags>).  So don't make virtual methods, constructor,
 * destructor and operators.
 * This is necessary for VC which is NOT C++0x compiler.
 ******************************************************************************/

struct BaseEventFlags
{
public:
  // If mIsTrusted is true, the event is a trusted event.  Otherwise, it's
  // an untrusted event.
  bool    mIsTrusted : 1;
  // If mInBubblingPhase is true, the event is in bubbling phase or target
  // phase.
  bool    mInBubblingPhase : 1;
  // If mInCapturePhase is true, the event is in capture phase or target phase.
  bool    mInCapturePhase : 1;
  // If mInSystemGroup is true, the event is being dispatched in system group.
  bool    mInSystemGroup: 1;
  // If mCancelable is true, the event can be consumed.  I.e., calling
  // dom::Event::PreventDefault() can prevent the default action.
  bool    mCancelable : 1;
  // If mBubbles is true, the event can bubble.  Otherwise, cannot be handled
  // in bubbling phase.
  bool    mBubbles : 1;
  // If mPropagationStopped is true, dom::Event::StopPropagation() or
  // dom::Event::StopImmediatePropagation() has been called.
  bool    mPropagationStopped : 1;
  // If mImmediatePropagationStopped is true,
  // dom::Event::StopImmediatePropagation() has been called.
  // Note that mPropagationStopped must be true when this is true.
  bool    mImmediatePropagationStopped : 1;
  // If mDefaultPrevented is true, the event has been consumed.
  // E.g., dom::Event::PreventDefault() has been called or
  // the default action has been performed.
  bool    mDefaultPrevented : 1;
  // If mDefaultPreventedByContent is true, the event has been
  // consumed by content.
  // Note that mDefaultPrevented must be true when this is true.
  bool    mDefaultPreventedByContent : 1;
  // mMultipleActionsPrevented may be used when default handling don't want to
  // be prevented, but only one of the event targets should handle the event.
  // For example, when a <label> element is in another <label> element and
  // the first <label> element is clicked, that one may set this true.
  // Then, the second <label> element won't handle the event.
  bool    mMultipleActionsPrevented : 1;
  // If mIsBeingDispatched is true, the DOM event created from the event is
  // dispatching into the DOM tree and not completed.
  bool    mIsBeingDispatched : 1;
  // If mDispatchedAtLeastOnce is true, the event has been dispatched
  // as a DOM event and the dispatch has been completed.
  bool    mDispatchedAtLeastOnce : 1;
  // If mIsSynthesizedForTests is true, the event has been synthesized for
  // automated tests or something hacky approach of an add-on.
  bool    mIsSynthesizedForTests : 1;
  // If mExceptionHasBeenRisen is true, one of the event handlers has risen an
  // exception.
  bool    mExceptionHasBeenRisen : 1;
  // If mRetargetToNonNativeAnonymous is true and the target is in a non-native
  // native anonymous subtree, the event target is set to originalTarget.
  bool    mRetargetToNonNativeAnonymous : 1;
  // If mNoCrossProcessBoundaryForwarding is true, the event is not allowed to
  // cross process boundary.
  bool    mNoCrossProcessBoundaryForwarding : 1;
  // If mNoContentDispatch is true, the event is never dispatched to the
  // event handlers which are added to the contents, onfoo attributes and
  // properties.  Note that this flag is ignored when
  // EventChainPreVisitor::mForceContentDispatch is set true.  For exapmle,
  // window and document object sets it true.  Therefore, web applications
  // can handle the event if they add event listeners to the window or the
  // document.
  bool    mNoContentDispatch : 1;
  // If mOnlyChromeDispatch is true, the event is dispatched to only chrome.
  bool    mOnlyChromeDispatch : 1;
  // If mWantReplyFromContentProcess is true, the event will be redispatched
  // in the parent process after the content process has handled it. Useful
  // for when the parent process need the know first how the event was used
  // by content before handling it itself.
  bool mWantReplyFromContentProcess : 1;

  // If the event is being handled in target phase, returns true.
  inline bool InTargetPhase() const
  {
    return (mInBubblingPhase && mInCapturePhase);
  }

  inline void Clear()
  {
    SetRawFlags(0);
  }
  // Get if either the instance's bit or the aOther's bit is true, the
  // instance's bit becomes true.  In other words, this works like:
  // eventFlags |= aOther;
  inline void Union(const BaseEventFlags& aOther)
  {
    RawFlags rawFlags = GetRawFlags() | aOther.GetRawFlags();
    SetRawFlags(rawFlags);
  }

private:
  typedef uint32_t RawFlags;

  inline void SetRawFlags(RawFlags aRawFlags)
  {
    static_assert(sizeof(BaseEventFlags) <= sizeof(RawFlags),
      "mozilla::EventFlags must not be bigger than the RawFlags");
    memcpy(this, &aRawFlags, sizeof(BaseEventFlags));
  }
  inline RawFlags GetRawFlags() const
  {
    RawFlags result = 0;
    memcpy(&result, this, sizeof(BaseEventFlags));
    return result;
  }
};

/******************************************************************************
 * mozilla::EventFlags
 ******************************************************************************/

struct EventFlags : public BaseEventFlags
{
  EventFlags()
  {
    Clear();
  }
};

/******************************************************************************
 * mozilla::WidgetEvent
 ******************************************************************************/

class WidgetEvent
{
protected:
  WidgetEvent(bool aIsTrusted, uint32_t aMessage, EventClassID aEventClassID)
    : mClass(aEventClassID)
    , message(aMessage)
    , refPoint(0, 0)
    , lastRefPoint(0, 0)
    , time(0)
    , timeStamp(TimeStamp::Now())
    , userType(nullptr)
  {
    MOZ_COUNT_CTOR(WidgetEvent);
    mFlags.Clear();
    mFlags.mIsTrusted = aIsTrusted;
    mFlags.mCancelable = true;
    mFlags.mBubbles = true;
  }

  WidgetEvent()
  {
    MOZ_COUNT_CTOR(WidgetEvent);
  }

public:
  WidgetEvent(bool aIsTrusted, uint32_t aMessage)
    : mClass(eBasicEventClass)
    , message(aMessage)
    , refPoint(0, 0)
    , lastRefPoint(0, 0)
    , time(0)
    , timeStamp(TimeStamp::Now())
    , userType(nullptr)
  {
    MOZ_COUNT_CTOR(WidgetEvent);
    mFlags.Clear();
    mFlags.mIsTrusted = aIsTrusted;
    mFlags.mCancelable = true;
    mFlags.mBubbles = true;
  }

  virtual ~WidgetEvent()
  {
    MOZ_COUNT_DTOR(WidgetEvent);
  }

  WidgetEvent(const WidgetEvent& aOther)
  {
    MOZ_COUNT_CTOR(WidgetEvent);
    *this = aOther;
  }

  virtual WidgetEvent* Duplicate() const
  {
    MOZ_ASSERT(mClass == eBasicEventClass,
               "Duplicate() must be overridden by sub class");
    WidgetEvent* result = new WidgetEvent(false, message);
    result->AssignEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  EventClassID mClass;
  // See GUI MESSAGES,
  uint32_t message;
  // Relative to the widget of the event, or if there is no widget then it is
  // in screen coordinates. Not modified by layout code.
  LayoutDeviceIntPoint refPoint;
  // The previous refPoint, if known, used to calculate mouse movement deltas.
  LayoutDeviceIntPoint lastRefPoint;
  // Elapsed time, in milliseconds, from a platform-specific zero time
  // to the time the message was created
  uint64_t time;
  // Timestamp when the message was created. Set in parallel to 'time' until we
  // determine if it is safe to drop 'time' (see bug 77992).
  mozilla::TimeStamp timeStamp;
  // See BaseEventFlags definition for the detail.
  BaseEventFlags mFlags;

  // Additional type info for user defined events
  nsCOMPtr<nsIAtom> userType;

  nsString typeString; // always set on non-main-thread events

  // Event targets, needed by DOM Events
  nsCOMPtr<dom::EventTarget> target;
  nsCOMPtr<dom::EventTarget> currentTarget;
  nsCOMPtr<dom::EventTarget> originalTarget;

  void AssignEventData(const WidgetEvent& aEvent, bool aCopyTargets)
  {
    // mClass should be initialized with the constructor.
    // message should be initialized with the constructor.
    refPoint = aEvent.refPoint;
    // lastRefPoint doesn't need to be copied.
    time = aEvent.time;
    timeStamp = aEvent.timeStamp;
    // mFlags should be copied manually if it's necessary.
    userType = aEvent.userType;
    // typeString should be copied manually if it's necessary.
    target = aCopyTargets ? aEvent.target : nullptr;
    currentTarget = aCopyTargets ? aEvent.currentTarget : nullptr;
    originalTarget = aCopyTargets ? aEvent.originalTarget : nullptr;
  }

  /**
   * Utils for checking event types
   */

  /**
   * As*Event() returns the pointer of the instance only when the instance is
   * the class or one of its derived class.
   */
#define NS_ROOT_EVENT_CLASS(aPrefix, aName)
#define NS_EVENT_CLASS(aPrefix, aName) \
  virtual aPrefix##aName* As##aName(); \
  const aPrefix##aName* As##aName() const;

#include "mozilla/EventClassList.h"

#undef NS_EVENT_CLASS
#undef NS_ROOT_EVENT_CLASS

  /**
   * Returns true if the event is a query content event.
   */
  bool IsQueryContentEvent() const;
  /**
   * Returns true if the event is a selection event.
   */
  bool IsSelectionEvent() const;
  /**
   * Returns true if the event is a content command event.
   */
  bool IsContentCommandEvent() const;
  /**
   * Returns true if the event is a native event deliverer event for plugin.
   */
  bool IsNativeEventDelivererForPlugin() const;

  /**
   * Returns true if the event message is one of mouse events.
   */
  bool HasMouseEventMessage() const;
  /**
   * Returns true if the event message is one of drag events.
   */
  bool HasDragEventMessage() const;
  /**
   * Returns true if the event message is one of key events.
   */
  bool HasKeyEventMessage() const;
  /**
   * Returns true if the event message is one of composition events or text
   * event.
   */
  bool HasIMEEventMessage() const;
  /**
   * Returns true if the event message is one of plugin activation events.
   */
  bool HasPluginActivationEventMessage() const;

  /**
   * Returns true if the event is native event deliverer event for plugin and
   * it should be retarted to focused document.
   */
  bool IsRetargetedNativeEventDelivererForPlugin() const;
  /**
   * Returns true if the event is native event deliverer event for plugin and
   * it should NOT be retarted to focused document.
   */
  bool IsNonRetargetedNativeEventDelivererForPlugin() const;
  /**
   * Returns true if the event is related to IME handling.  It includes
   * IME events, query content events and selection events.
   * Be careful when you use this.
   */
  bool IsIMERelatedEvent() const;

  /**
   * Whether the event should be handled by the frame of the mouse cursor
   * position or not.  When it should be handled there (e.g., the mouse events),
   * this returns true.
   */
  bool IsUsingCoordinates() const;
  /**
   * Whether the event should be handled by the focused DOM window in the
   * same top level window's or not.  E.g., key events, IME related events
   * (including the query content events, they are used in IME transaction)
   * should be handled by the (last) focused window rather than the dispatched
   * window.
   *
   * NOTE: Even if this returns true, the event isn't going to be handled by the
   * application level active DOM window which is on another top level window.
   * So, when the event is fired on a deactive window, the event is going to be
   * handled by the last focused DOM window in the last focused window.
   */
  bool IsTargetedAtFocusedWindow() const;
  /**
   * Whether the event should be handled by the focused content or not.  E.g.,
   * key events, IME related events and other input events which are not handled
   * by the frame of the mouse cursor position.
   *
   * NOTE: Even if this returns true, the event isn't going to be handled by the
   * application level active DOM window which is on another top level window.
   * So, when the event is fired on a deactive window, the event is going to be
   * handled by the last focused DOM element of the last focused DOM window in
   * the last focused window.
   */
  bool IsTargetedAtFocusedContent() const;
  /**
   * Whether the event should cause a DOM event.
   */
  bool IsAllowedToDispatchDOMEvent() const;
};

/******************************************************************************
 * mozilla::WidgetGUIEvent
 ******************************************************************************/

class WidgetGUIEvent : public WidgetEvent
{
protected:
  WidgetGUIEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget,
                 EventClassID aEventClassID)
    : WidgetEvent(aIsTrusted, aMessage, aEventClassID)
    , widget(aWidget)
  {
  }

  WidgetGUIEvent()
  {
  }

public:
  virtual WidgetGUIEvent* AsGUIEvent() MOZ_OVERRIDE { return this; }

  WidgetGUIEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget) :
    WidgetEvent(aIsTrusted, aMessage, eGUIEventClass),
    widget(aWidget)
  {
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(mClass == eGUIEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetGUIEvent* result = new WidgetGUIEvent(false, message, nullptr);
    result->AssignGUIEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  /// Originator of the event
  nsCOMPtr<nsIWidget> widget;

  /*
   * Explanation for this PluginEvent class:
   *
   * WidgetGUIEvent's mPluginEvent member used to be a void* pointer,
   * used to reference external, OS-specific data structures.
   *
   * That void* pointer wasn't serializable by itself, causing
   * certain plugin events not to function in e10s. See bug 586656.
   *
   * To make this serializable, we changed this void* pointer into
   * a proper buffer, and copy these external data structures into this
   * buffer.
   *
   * That buffer is PluginEvent::mBuffer below.
   *
   * We wrap this in that PluginEvent class providing operators to
   * be compatible with existing code that was written around
   * the old void* field.
   *
   * Ideally though, we wouldn't allow arbitrary reinterpret_cast'ing here;
   * instead, we would at least store type information here so that
   * this class can't be used to reinterpret one structure type into another.
   * We can also wonder if it would be possible to properly extend
   * WidgetGUIEvent and other Event classes to remove the need for this
   * mPluginEvent field.
   */
  class PluginEvent MOZ_FINAL
  {
    nsTArray<uint8_t> mBuffer;

    friend struct IPC::ParamTraits<mozilla::WidgetGUIEvent>;

  public:

    MOZ_EXPLICIT_CONVERSION operator bool() const
    {
      return !mBuffer.IsEmpty();
    }

    template<typename T>
    MOZ_EXPLICIT_CONVERSION operator const T*() const
    {
      return mBuffer.IsEmpty()
             ? nullptr
             : reinterpret_cast<const T*>(mBuffer.Elements());
    }

    template <typename T>
    void Copy(const T& other)
    {
      static_assert(!mozilla::IsPointer<T>::value, "Don't want a pointer!");
      mBuffer.SetLength(sizeof(T));
      memcpy(mBuffer.Elements(), &other, mBuffer.Length());
    }

    void Clear()
    {
      mBuffer.Clear();
    }
  };

  /// Event for NPAPI plugin
  PluginEvent mPluginEvent;

  void AssignGUIEventData(const WidgetGUIEvent& aEvent, bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    // widget should be initialized with the constructor.

    mPluginEvent = aEvent.mPluginEvent;
  }
};

/******************************************************************************
 * mozilla::Modifier
 *
 * All modifier keys should be defined here.  This is used for managing
 * modifier states for DOM Level 3 or later.
 ******************************************************************************/

enum Modifier
{
  MODIFIER_NONE       = 0x0000,
  MODIFIER_ALT        = 0x0001,
  MODIFIER_ALTGRAPH   = 0x0002,
  MODIFIER_CAPSLOCK   = 0x0004,
  MODIFIER_CONTROL    = 0x0008,
  MODIFIER_FN         = 0x0010,
  MODIFIER_META       = 0x0020,
  MODIFIER_NUMLOCK    = 0x0040,
  MODIFIER_SCROLLLOCK = 0x0080,
  MODIFIER_SHIFT      = 0x0100,
  MODIFIER_SYMBOLLOCK = 0x0200,
  MODIFIER_OS         = 0x0400
};

/******************************************************************************
 * Modifier key names.
 ******************************************************************************/

#define NS_DOM_KEYNAME_ALT        "Alt"
#define NS_DOM_KEYNAME_ALTGRAPH   "AltGraph"
#define NS_DOM_KEYNAME_CAPSLOCK   "CapsLock"
#define NS_DOM_KEYNAME_CONTROL    "Control"
#define NS_DOM_KEYNAME_FN         "Fn"
#define NS_DOM_KEYNAME_META       "Meta"
#define NS_DOM_KEYNAME_NUMLOCK    "NumLock"
#define NS_DOM_KEYNAME_SCROLLLOCK "ScrollLock"
#define NS_DOM_KEYNAME_SHIFT      "Shift"
#define NS_DOM_KEYNAME_SYMBOLLOCK "SymbolLock"
#define NS_DOM_KEYNAME_OS         "OS"

/******************************************************************************
 * mozilla::Modifiers
 ******************************************************************************/

typedef uint16_t Modifiers;

/******************************************************************************
 * mozilla::WidgetInputEvent
 ******************************************************************************/

class WidgetInputEvent : public WidgetGUIEvent
{
protected:
  WidgetInputEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget,
                   EventClassID aEventClassID)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, aEventClassID)
    , modifiers(0)
  {
  }

  WidgetInputEvent()
  {
  }

public:
  virtual WidgetInputEvent* AsInputEvent() MOZ_OVERRIDE { return this; }

  WidgetInputEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, eInputEventClass)
    , modifiers(0)
  {
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(mClass == eInputEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetInputEvent* result = new WidgetInputEvent(false, message, nullptr);
    result->AssignInputEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }


  /**
   * Returns a modifier of "Accel" virtual modifier which is used for shortcut
   * key.
   */
  static Modifier AccelModifier();

  // true indicates the accel key on the environment is down
  bool IsAccel() const
  {
    return ((modifiers & AccelModifier()) != 0);
  }

  // true indicates the shift key is down
  bool IsShift() const
  {
    return ((modifiers & MODIFIER_SHIFT) != 0);
  }
  // true indicates the control key is down
  bool IsControl() const
  {
    return ((modifiers & MODIFIER_CONTROL) != 0);
  }
  // true indicates the alt key is down
  bool IsAlt() const
  {
    return ((modifiers & MODIFIER_ALT) != 0);
  }
  // true indicates the meta key is down (or, on Mac, the Command key)
  bool IsMeta() const
  {
    return ((modifiers & MODIFIER_META) != 0);
  }
  // true indicates the win key is down on Windows. Or the Super or Hyper key
  // is down on Linux.
  bool IsOS() const
  {
    return ((modifiers & MODIFIER_OS) != 0);
  }
  // true indicates the alt graph key is down
  // NOTE: on Mac, the option key press causes both IsAlt() and IsAltGrpah()
  //       return true.
  bool IsAltGraph() const
  {
    return ((modifiers & MODIFIER_ALTGRAPH) != 0);
  }
  // true indeicates the CapLock LED is turn on.
  bool IsCapsLocked() const
  {
    return ((modifiers & MODIFIER_CAPSLOCK) != 0);
  }
  // true indeicates the NumLock LED is turn on.
  bool IsNumLocked() const
  {
    return ((modifiers & MODIFIER_NUMLOCK) != 0);
  }
  // true indeicates the ScrollLock LED is turn on.
  bool IsScrollLocked() const
  {
    return ((modifiers & MODIFIER_SCROLLLOCK) != 0);
  }

  // true indeicates the Fn key is down, but this is not supported by native
  // key event on any platform.
  bool IsFn() const
  {
    return ((modifiers & MODIFIER_FN) != 0);
  }
  // true indeicates the ScrollLock LED is turn on.
  bool IsSymbolLocked() const
  {
    return ((modifiers & MODIFIER_SYMBOLLOCK) != 0);
  }

  void InitBasicModifiers(bool aCtrlKey,
                          bool aAltKey,
                          bool aShiftKey,
                          bool aMetaKey)
  {
    modifiers = 0;
    if (aCtrlKey) {
      modifiers |= MODIFIER_CONTROL;
    }
    if (aAltKey) {
      modifiers |= MODIFIER_ALT;
    }
    if (aShiftKey) {
      modifiers |= MODIFIER_SHIFT;
    }
    if (aMetaKey) {
      modifiers |= MODIFIER_META;
    }
  }

  Modifiers modifiers;

  void AssignInputEventData(const WidgetInputEvent& aEvent, bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    modifiers = aEvent.modifiers;
  }
};

/******************************************************************************
 * mozilla::InternalUIEvent
 *
 * XXX Why this inherits WidgetGUIEvent rather than WidgetEvent?
 ******************************************************************************/

class InternalUIEvent : public WidgetGUIEvent
{
protected:
  InternalUIEvent()
    : detail(0)
  {
  }

  InternalUIEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget,
                  EventClassID aEventClassID)
    : WidgetGUIEvent(aIsTrusted, aMessage, aWidget, aEventClassID)
    , detail(0)
  {
  }

  InternalUIEvent(bool aIsTrusted, uint32_t aMessage,
                  EventClassID aEventClassID)
    : WidgetGUIEvent(aIsTrusted, aMessage, nullptr, aEventClassID)
    , detail(0)
  {
  }

public:
  virtual InternalUIEvent* AsUIEvent() MOZ_OVERRIDE { return this; }

  InternalUIEvent(bool aIsTrusted, uint32_t aMessage)
    : WidgetGUIEvent(aIsTrusted, aMessage, nullptr, eUIEventClass)
    , detail(0)
  {
  }

  virtual WidgetEvent* Duplicate() const MOZ_OVERRIDE
  {
    MOZ_ASSERT(mClass == eUIEventClass,
               "Duplicate() must be overridden by sub class");
    InternalUIEvent* result = new InternalUIEvent(false, message);
    result->AssignUIEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  int32_t detail;

  void AssignUIEventData(const InternalUIEvent& aEvent, bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    detail = aEvent.detail;
  }
};

} // namespace mozilla

#endif // mozilla_BasicEvents_h__
