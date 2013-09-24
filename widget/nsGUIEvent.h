/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGUIEvent_h__
#define nsGUIEvent_h__

#include "mozilla/BasicEvents.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/TextEvents.h"

#include "nsPoint.h"
#include "nsRect.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMWheelEvent.h"
#include "nsIDOMDataTransfer.h"
#include "nsWeakPtr.h"
#include "nsITransferable.h"
#include "nsAutoPtr.h"
#include "mozilla/dom/Touch.h"

namespace mozilla {
namespace dom {
  class PBrowserParent;
  class PBrowserChild;
}
namespace plugins {
  class PPluginInstanceChild;
}
}

class nsRenderingContext;
class nsIMenuItem;
class nsIContent;
class nsIURI;
class nsHashKey;

#define NS_EVENT_TYPE_NULL                   0
#define NS_EVENT_TYPE_ALL                  1 // Not a real event type

/**
 * Script error event
 */

class nsScriptErrorEvent : public nsEvent
{
public:
  nsScriptErrorEvent(bool isTrusted, uint32_t msg)
    : nsEvent(isTrusted, msg, NS_SCRIPT_ERROR_EVENT),
      lineNr(0), errorMsg(nullptr), fileName(nullptr)
  {
  }

  int32_t           lineNr;
  const PRUnichar*  errorMsg;
  const PRUnichar*  fileName;

  // XXX Not tested by test_assign_event_data.html
  void AssignScriptErrorEventData(const nsScriptErrorEvent& aEvent,
                                  bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    lineNr = aEvent.lineNr;

    // We don't copy errorMsg and fileName.  If it's necessary, perhaps, this
    // should duplicate the characters and free them at destructing.
    errorMsg = nullptr;
    fileName = nullptr;
  }
};

class nsScrollPortEvent : public nsGUIEvent
{
public:
  enum orientType {
    vertical   = 0,
    horizontal = 1,
    both       = 2
  };

  nsScrollPortEvent(bool isTrusted, uint32_t msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_SCROLLPORT_EVENT),
      orient(vertical)
  {
  }

  orientType orient;

  void AssignScrollPortEventData(const nsScrollPortEvent& aEvent,
                                 bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    orient = aEvent.orient;
  }
};

class nsScrollAreaEvent : public nsGUIEvent
{
public:
  nsScrollAreaEvent(bool isTrusted, uint32_t msg, nsIWidget *w)
    : nsGUIEvent(isTrusted, msg, w, NS_SCROLLAREA_EVENT)
  {
  }

  nsRect mArea;

  void AssignScrollAreaEventData(const nsScrollAreaEvent& aEvent,
                                 bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    mArea = aEvent.mArea;
  }
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

  nsMouseEvent_base(bool isTrusted, uint32_t msg, nsIWidget *w,
                    nsEventStructType type)
    : nsInputEvent(isTrusted, msg, w, type), button(0), buttons(0),
      pressure(0), inputSource(nsIDOMMouseEvent::MOZ_SOURCE_MOUSE) {}

  /// The possible related target
  nsCOMPtr<nsISupports> relatedTarget;

  int16_t               button;
  int16_t               buttons;

  // Finger or touch pressure of event
  // ranges between 0.0 and 1.0
  float                 pressure;

  // Possible values at nsIDOMMouseEvent
  uint16_t              inputSource;

  void AssignMouseEventBaseData(const nsMouseEvent_base& aEvent,
                                bool aCopyTargets)
  {
    AssignInputEventData(aEvent, aCopyTargets);

    relatedTarget = aCopyTargets ? aEvent.relatedTarget : nullptr;
    button = aEvent.button;
    buttons = aEvent.buttons;
    pressure = aEvent.pressure;
    inputSource = aEvent.inputSource;
  }
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
  nsMouseEvent(bool isTrusted, uint32_t msg, nsIWidget *w,
               nsEventStructType structType, reasonType aReason)
    : nsMouseEvent_base(isTrusted, msg, w, structType),
      acceptActivation(false), ignoreRootScrollFrame(false),
      reason(aReason), context(eNormal), exit(eChild), clickCount(0)
  {
    switch (msg) {
      case NS_MOUSE_MOVE:
        mFlags.mCancelable = false;
        break;
      case NS_MOUSEENTER:
      case NS_MOUSELEAVE:
        mFlags.mBubbles = false;
        mFlags.mCancelable = false;
        break;
      default:
        break;
    }
  }

public:

  nsMouseEvent(bool isTrusted, uint32_t msg, nsIWidget *w,
               reasonType aReason, contextType aContext = eNormal)
    : nsMouseEvent_base(isTrusted, msg, w, NS_MOUSE_EVENT),
      acceptActivation(false), ignoreRootScrollFrame(false),
      reason(aReason), context(aContext), exit(eChild), clickCount(0)
  {
    switch (msg) {
      case NS_MOUSE_MOVE:
        mFlags.mCancelable = false;
        break;
      case NS_MOUSEENTER:
      case NS_MOUSELEAVE:
        mFlags.mBubbles = false;
        mFlags.mCancelable = false;
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
  uint32_t     clickCount;

  void AssignMouseEventData(const nsMouseEvent& aEvent, bool aCopyTargets)
  {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    acceptActivation = aEvent.acceptActivation;
    ignoreRootScrollFrame = aEvent.ignoreRootScrollFrame;
    clickCount = aEvent.clickCount;
  }
};

/**
 * Drag event
 */

class nsDragEvent : public nsMouseEvent
{
public:
  nsDragEvent(bool isTrusted, uint32_t msg, nsIWidget *w)
    : nsMouseEvent(isTrusted, msg, w, NS_DRAG_EVENT, eReal),
      userCancelled(false)
  {
    mFlags.mCancelable =
      (msg != NS_DRAGDROP_EXIT_SYNTH &&
       msg != NS_DRAGDROP_LEAVE_SYNTH &&
       msg != NS_DRAGDROP_END);
  }

  nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
  bool userCancelled;

  // XXX Not tested by test_assign_event_data.html
  void AssignDragEventData(const nsDragEvent& aEvent, bool aCopyTargets)
  {
    AssignMouseEventData(aEvent, aCopyTargets);

    dataTransfer = aEvent.dataTransfer;
    // XXX userCancelled isn't copied, is this instentionally?
    userCancelled = false;
  }
};

/**
 * nsMouseScrollEvent is used for legacy DOM mouse scroll events, i.e.,
 * DOMMouseScroll and MozMousePixelScroll event.  These events are NOT hanbled
 * by ESM even if widget dispatches them.  Use new WheelEvent instead.
 */

class nsMouseScrollEvent : public nsMouseEvent_base
{
private:
  nsMouseScrollEvent()
  {
  }

public:
  nsMouseScrollEvent(bool isTrusted, uint32_t msg, nsIWidget *w)
    : nsMouseEvent_base(isTrusted, msg, w, NS_MOUSE_SCROLL_EVENT),
      delta(0), isHorizontal(false)
  {
  }

  int32_t               delta;
  bool                  isHorizontal;

  void AssignMouseScrollEventData(const nsMouseScrollEvent& aEvent,
                                  bool aCopyTargets)
  {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    delta = aEvent.delta;
    isHorizontal = aEvent.isHorizontal;
  }
};

/**
 * WheelEvent is used only for DOM Level 3 WheelEvent (dom::DOMWheelEvent).
 */

namespace mozilla {

class WheelEvent : public nsMouseEvent_base
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  WheelEvent()
  {
  }

public:
  WheelEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget) :
    nsMouseEvent_base(aIsTrusted, aMessage, aWidget, NS_WHEEL_EVENT),
    deltaX(0.0), deltaY(0.0), deltaZ(0.0),
    deltaMode(nsIDOMWheelEvent::DOM_DELTA_PIXEL),
    customizedByUserPrefs(false), isMomentum(false), isPixelOnlyDevice(false),
    lineOrPageDeltaX(0), lineOrPageDeltaY(0), scrollType(SCROLL_DEFAULT),
    overflowDeltaX(0.0), overflowDeltaY(0.0)
  {
  }

  // NOTE: deltaX, deltaY and deltaZ may be customized by
  //       mousewheel.*.delta_multiplier_* prefs which are applied by
  //       nsEventStateManager.  So, after widget dispatches this event,
  //       these delta values may have different values than before.
  double deltaX;
  double deltaY;
  double deltaZ;

  // Should be one of nsIDOMWheelEvent::DOM_DELTA_*
  uint32_t deltaMode;

  // Following members are for internal use only, not for DOM event.

  // If the delta values are computed from prefs, this value is true.
  // Otherwise, i.e., they are computed from native events, false.
  bool customizedByUserPrefs;

  // true if the event is caused by momentum.
  bool isMomentum;

  // If device event handlers don't know when they should set lineOrPageDeltaX
  // and lineOrPageDeltaY, this is true.  Otherwise, false.
  // If isPixelOnlyDevice is true, ESM will generate NS_MOUSE_SCROLL events
  // when accumulated pixel delta values reach a line height.
  bool isPixelOnlyDevice;

  // If widget sets lineOrPageDelta, nsEventStateManager will dispatch
  // NS_MOUSE_SCROLL event for compatibility.  Note that the delta value means
  // pages if the deltaMode is DOM_DELTA_PAGE, otherwise, lines.
  int32_t lineOrPageDeltaX;
  int32_t lineOrPageDeltaY;

  // When the default action for an wheel event is moving history or zooming,
  // need to chose a delta value for doing it.
  int32_t GetPreferredIntDelta()
  {
    if (!lineOrPageDeltaX && !lineOrPageDeltaY) {
      return 0;
    }
    if (lineOrPageDeltaY && !lineOrPageDeltaX) {
      return lineOrPageDeltaY;
    }
    if (lineOrPageDeltaX && !lineOrPageDeltaY) {
      return lineOrPageDeltaX;
    }
    if ((lineOrPageDeltaX < 0 && lineOrPageDeltaY > 0) ||
        (lineOrPageDeltaX > 0 && lineOrPageDeltaY < 0)) {
      return 0; // We cannot guess the answer in this case.
    }
    return (Abs(lineOrPageDeltaX) > Abs(lineOrPageDeltaY)) ?
             lineOrPageDeltaX : lineOrPageDeltaY;
  }

  // Scroll type
  // The default value is SCROLL_DEFAULT, which means nsEventStateManager will
  // select preferred scroll type automatically.
  enum ScrollType {
    SCROLL_DEFAULT,
    SCROLL_SYNCHRONOUSLY,
    SCROLL_ASYNCHRONOUSELY,
    SCROLL_SMOOTHLY
  };
  ScrollType scrollType;

  // overflowed delta values for scroll, these values are set by
  // nsEventStateManger.  If the default action of the wheel event isn't scroll,
  // these values always zero.  Otherwise, remaning delta values which are
  // not used by scroll are set.
  // NOTE: deltaX, deltaY and deltaZ may be modified by nsEventStateManager.
  //       However, overflowDeltaX and overflowDeltaY indicate unused original
  //       delta values which are not applied the delta_multiplier prefs.
  //       So, if widget wanted to know the actual direction to be scrolled,
  //       it would need to check the deltaX and deltaY.
  double overflowDeltaX;
  double overflowDeltaY;

  void AssignWheelEventData(const WheelEvent& aEvent, bool aCopyTargets)
  {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    deltaX = aEvent.deltaX;
    deltaY = aEvent.deltaY;
    deltaZ = aEvent.deltaZ;
    deltaMode = aEvent.deltaMode;
    customizedByUserPrefs = aEvent.customizedByUserPrefs;
    isMomentum = aEvent.isMomentum;
    isPixelOnlyDevice = aEvent.isPixelOnlyDevice;
    lineOrPageDeltaX = aEvent.lineOrPageDeltaX;
    lineOrPageDeltaY = aEvent.lineOrPageDeltaY;
    scrollType = aEvent.scrollType;
    overflowDeltaX = aEvent.overflowDeltaX;
    overflowDeltaY = aEvent.overflowDeltaY;
  }
};

} // namespace mozilla

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
  
  nsGestureNotifyEvent(bool aIsTrusted, uint32_t aMsg, nsIWidget *aWidget):
    nsGUIEvent(aIsTrusted, aMsg, aWidget, NS_GESTURENOTIFY_EVENT),
    panDirection(ePanNone),
    displayPanFeedback(false)
  {
  }
};

class nsContentCommandEvent : public nsGUIEvent
{
public:
  nsContentCommandEvent(bool aIsTrusted, uint32_t aMsg, nsIWidget *aWidget,
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

    int32_t      mAmount;                                  // [in]
    uint8_t      mUnit;                                    // [in]
    bool mIsHorizontal;                            // [in]
  } mScroll;

  bool mOnlyEnabledCheck;                          // [in]

  bool mSucceeded;                                 // [out]
  bool mIsEnabled;                                 // [out]
};

class nsTouchEvent : public nsInputEvent
{
public:
  nsTouchEvent()
  {
  }
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
  nsTouchEvent(bool isTrusted, uint32_t msg, nsIWidget* w)
    : nsInputEvent(isTrusted, msg, w, NS_TOUCH_EVENT)
  {
    MOZ_COUNT_CTOR(nsTouchEvent);
  }
  ~nsTouchEvent()
  {
    MOZ_COUNT_DTOR(nsTouchEvent);
  }

  nsTArray< nsRefPtr<mozilla::dom::Touch> > touches;

  void AssignTouchEventData(const nsTouchEvent& aEvent, bool aCopyTargets)
  {
    AssignInputEventData(aEvent, aCopyTargets);

    // Currently, we don't need to copy touches.
  }
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
  nsFormEvent(bool isTrusted, uint32_t msg)
    : nsEvent(isTrusted, msg, NS_FORM_EVENT),
      originator(nullptr)
  {
  }

  nsIContent *originator;

  void AssignFormEventData(const nsFormEvent& aEvent, bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    // Don't copy originator due to a weak pointer.
  }
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

  // XXX Not tested by test_assign_event_data.html
  void AssignCommandEventData(const nsCommandEvent& aEvent, bool aCopyTargets)
  {
    AssignGUIEventData(aEvent, aCopyTargets);

    // command must have been initialized with the constructor.
  }
};

/**
 * Clipboard event
 */
class nsClipboardEvent : public nsEvent
{
public:
  nsClipboardEvent(bool isTrusted, uint32_t msg)
    : nsEvent(isTrusted, msg, NS_CLIPBOARD_EVENT)
  {
  }

  nsCOMPtr<nsIDOMDataTransfer> clipboardData;

  void AssignClipboardEventData(const nsClipboardEvent& aEvent,
                                bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    clipboardData = aEvent.clipboardData;
  }
};

class nsFocusEvent : public nsUIEvent
{
public:
  nsFocusEvent(bool isTrusted, uint32_t msg)
    : nsUIEvent(isTrusted, msg, 0),
      fromRaise(false),
      isRefocus(false)
  {
    eventStructType = NS_FOCUS_EVENT;
  }

  /// The possible related target
  nsCOMPtr<mozilla::dom::EventTarget> relatedTarget;

  bool fromRaise;
  bool isRefocus;

  void AssignFocusEventData(const nsFocusEvent& aEvent, bool aCopyTargets)
  {
    AssignUIEventData(aEvent, aCopyTargets);

    relatedTarget = aCopyTargets ? aEvent.relatedTarget : nullptr;
    fromRaise = aEvent.fromRaise;
    isRefocus = aEvent.isRefocus;
  }
};

/**
 * Simple gesture event
 */
class nsSimpleGestureEvent : public nsMouseEvent_base
{
public:
  nsSimpleGestureEvent(bool isTrusted, uint32_t msg, nsIWidget* w,
                         uint32_t directionArg, double deltaArg)
    : nsMouseEvent_base(isTrusted, msg, w, NS_SIMPLE_GESTURE_EVENT),
      allowedDirections(0), direction(directionArg), delta(deltaArg),
      clickCount(0)
  {
  }

  nsSimpleGestureEvent(const nsSimpleGestureEvent& other)
    : nsMouseEvent_base(other.mFlags.mIsTrusted,
                        other.message, other.widget, NS_SIMPLE_GESTURE_EVENT),
      allowedDirections(other.allowedDirections), direction(other.direction),
      delta(other.delta), clickCount(0)
  {
  }
  uint32_t allowedDirections; // See nsIDOMSimpleGestureEvent for values
  uint32_t direction;         // See nsIDOMSimpleGestureEvent for values
  double delta;               // Delta for magnify and rotate events
  uint32_t clickCount;        // The number of taps for tap events

  // XXX Not tested by test_assign_event_data.html
  void AssignSimpleGestureEventData(const nsSimpleGestureEvent& aEvent,
                                    bool aCopyTargets)
  {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    // allowedDirections isn't copied
    direction = aEvent.direction;
    delta = aEvent.delta;
    clickCount = aEvent.clickCount;
  }
};

class nsTransitionEvent : public nsEvent
{
public:
  nsTransitionEvent(bool isTrusted, uint32_t msg,
                    const nsAString& propertyNameArg, float elapsedTimeArg,
                    const nsAString& pseudoElementArg)
    : nsEvent(isTrusted, msg, NS_TRANSITION_EVENT),
      propertyName(propertyNameArg), elapsedTime(elapsedTimeArg),
      pseudoElement(pseudoElementArg)
  {
  }

  nsString propertyName;
  float elapsedTime;
  nsString pseudoElement;

  void AssignTransitionEventData(const nsTransitionEvent& aEvent,
                                 bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    // propertyName, elapsedTime and pseudoElement must have been initialized
    // with the constructor.
  }
};

class nsAnimationEvent : public nsEvent
{
public:
  nsAnimationEvent(bool isTrusted, uint32_t msg,
                   const nsAString &animationNameArg, float elapsedTimeArg,
                   const nsAString &pseudoElementArg)
    : nsEvent(isTrusted, msg, NS_ANIMATION_EVENT),
      animationName(animationNameArg), elapsedTime(elapsedTimeArg),
      pseudoElement(pseudoElementArg)
  {
  }

  nsString animationName;
  float elapsedTime;
  nsString pseudoElement;

  void AssignAnimationEventData(const nsAnimationEvent& aEvent,
                                bool aCopyTargets)
  {
    AssignEventData(aEvent, aCopyTargets);

    // animationName, elapsedTime and pseudoElement must have been initialized
    // with the constructor.
  }
};

/**
 * Native event pluginEvent for plugins.
 */

class nsPluginEvent : public nsGUIEvent
{
public:
  nsPluginEvent(bool isTrusted, uint32_t msg, nsIWidget *w)
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

#define NS_IS_INPUT_EVENT(evnt) \
       (((evnt)->eventStructType == NS_INPUT_EVENT) || \
        ((evnt)->eventStructType == NS_MOUSE_EVENT) || \
        ((evnt)->eventStructType == NS_KEY_EVENT) || \
        ((evnt)->eventStructType == NS_TOUCH_EVENT) || \
        ((evnt)->eventStructType == NS_DRAG_EVENT) || \
        ((evnt)->eventStructType == NS_MOUSE_SCROLL_EVENT) || \
        ((evnt)->eventStructType == NS_WHEEL_EVENT) || \
        ((evnt)->eventStructType == NS_SIMPLE_GESTURE_EVENT))

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
        (((evnt)->message == NS_PLUGIN_ACTIVATE) || \
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

// Be aware the query content events and the selection events are a part of IME
// processing.  So, you shouldn't use NS_IS_IME_EVENT macro directly in most
// cases, you should use NS_IS_IME_RELATED_EVENT instead.
#define NS_IS_IME_RELATED_EVENT(evnt) \
  (NS_IS_IME_EVENT(evnt) || \
   NS_IS_QUERY_CONTENT_EVENT(evnt) || \
   NS_IS_SELECTION_EVENT(evnt))

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
         aEvent->message != NS_PLUGIN_RESOLUTION_CHANGED;
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

/**
 * Whether the event should cause a DOM event.
 */
inline bool NS_IsAllowedToDispatchDOMEvent(nsEvent* aEvent)
{
  switch (aEvent->eventStructType) {
    case NS_MOUSE_EVENT:
      // We want synthesized mouse moves to cause mouseover and mouseout
      // DOM events (nsEventStateManager::PreHandleEvent), but not mousemove
      // DOM events.
      // Synthesized button up events also do not cause DOM events because they
      // do not have a reliable refPoint.
      return static_cast<nsMouseEvent*>(aEvent)->reason == nsMouseEvent::eReal;

    case NS_WHEEL_EVENT: {
      // wheel event whose all delta values are zero by user pref applied, it
      // shouldn't cause a DOM event.
      mozilla::WheelEvent* wheelEvent =
        static_cast<mozilla::WheelEvent*>(aEvent);
      return wheelEvent->deltaX != 0.0 || wheelEvent->deltaY != 0.0 ||
             wheelEvent->deltaZ != 0.0;
    }

    default:
      return true;
  }
}

#endif // nsGUIEvent_h__
