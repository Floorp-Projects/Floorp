/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MouseEvents_h__
#define mozilla_MouseEvents_h__

#include <stdint.h>

#include "mozilla/BasicEvents.h"
#include "mozilla/MathAlgorithms.h"
#include "nsCOMPtr.h"
#include "nsIDOMDataTransfer.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMWheelEvent.h"

/******************************************************************************
 * nsDragDropEventStatus
 ******************************************************************************/

enum nsDragDropEventStatus
{  
  // The event is a enter
  nsDragDropEventStatus_eDragEntered,
  // The event is exit
  nsDragDropEventStatus_eDragExited,
  // The event is drop
  nsDragDropEventStatus_eDrop
};

namespace mozilla {

namespace dom {
  class PBrowserParent;
  class PBrowserChild;
} // namespace dom

/******************************************************************************
 * mozilla::WidgetMouseEventBase
 ******************************************************************************/

class WidgetMouseEventBase : public WidgetInputEvent
{
private:
  friend class dom::PBrowserParent;
  friend class dom::PBrowserChild;

protected:
  WidgetMouseEventBase()
  {
  }

  WidgetMouseEventBase(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget,
                       nsEventStructType aStructType) :
    WidgetInputEvent(aIsTrusted, aMessage, aWidget, aStructType),
    button(0), buttons(0), pressure(0),
    inputSource(nsIDOMMouseEvent::MOZ_SOURCE_MOUSE)
 {
 }

public:
  virtual WidgetMouseEventBase* AsMouseEventBase() MOZ_OVERRIDE { return this; }

  /// The possible related target
  nsCOMPtr<nsISupports> relatedTarget;

  enum buttonType
  {
    eLeftButton   = 0,
    eMiddleButton = 1,
    eRightButton  = 2
  };
  // Pressed button ID of mousedown or mouseup event.
  // This is set only when pressing a button causes the event.
  int16_t button;

  enum buttonsFlag {
    eLeftButtonFlag   = 0x01,
    eRightButtonFlag  = 0x02,
    eMiddleButtonFlag = 0x04,
    // typicall, "back" button being left side of 5-button
    // mice, see "buttons" attribute document of DOM3 Events.
    e4thButtonFlag    = 0x08,
    // typicall, "forward" button being right side of 5-button
    // mice, see "buttons" attribute document of DOM3 Events.
    e5thButtonFlag    = 0x10
  };

  // Flags of all pressed buttons at the event fired.
  // This is set at any mouse event, don't be confused with |button|.
  int16_t buttons;

  // Finger or touch pressure of event. It ranges between 0.0 and 1.0.
  float pressure;

  // Possible values at nsIDOMMouseEvent
  uint16_t inputSource;

  void AssignMouseEventBaseData(const WidgetMouseEventBase& aEvent,
                                bool aCopyTargets)
  {
    AssignInputEventData(aEvent, aCopyTargets);

    relatedTarget = aCopyTargets ? aEvent.relatedTarget : nullptr;
    button = aEvent.button;
    buttons = aEvent.buttons;
    pressure = aEvent.pressure;
    inputSource = aEvent.inputSource;
  }

  /**
   * Returns true if left click event.
   */
  bool IsLeftClickEvent() const
  {
    return message == NS_MOUSE_CLICK && button == eLeftButton;
  }
};

/******************************************************************************
 * mozilla::WidgetMouseEvent
 ******************************************************************************/

class WidgetMouseEvent : public WidgetMouseEventBase
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

public:
  enum reasonType
  {
    eReal,
    eSynthesized
  };

  enum contextType
  {
    eNormal,
    eContextMenuKey
  };

  enum exitType
  {
    eChild,
    eTopLevel
  };

protected:
  WidgetMouseEvent()
  {
  }

  WidgetMouseEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget,
                   nsEventStructType aStructType, reasonType aReason) :
    WidgetMouseEventBase(aIsTrusted, aMessage, aWidget, aStructType),
    acceptActivation(false), ignoreRootScrollFrame(false),
    reason(aReason), context(eNormal), exit(eChild), clickCount(0)
  {
    switch (aMessage) {
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
  virtual WidgetMouseEvent* AsMouseEvent() MOZ_OVERRIDE { return this; }

  WidgetMouseEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget,
                   reasonType aReason, contextType aContext = eNormal) :
    WidgetMouseEventBase(aIsTrusted, aMessage, aWidget, NS_MOUSE_EVENT),
    acceptActivation(false), ignoreRootScrollFrame(false),
    reason(aReason), context(aContext), exit(eChild), clickCount(0)
  {
    switch (aMessage) {
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
  virtual ~WidgetMouseEvent()
  {
    NS_WARN_IF_FALSE(message != NS_CONTEXTMENU ||
                     button ==
                       ((context == eNormal) ? eRightButton : eLeftButton),
                     "Wrong button set to NS_CONTEXTMENU event?");
  }
#endif

  // Special return code for MOUSE_ACTIVATE to signal.
  // If the target accepts activation (1), or denies it (0).
  bool acceptActivation;
  // Whether the event should ignore scroll frame bounds during dispatch.
  bool ignoreRootScrollFrame;

  reasonType reason : 4;
  contextType context : 4;
  exitType exit;

  /// The number of mouse clicks.
  uint32_t clickCount;

  void AssignMouseEventData(const WidgetMouseEvent& aEvent, bool aCopyTargets)
  {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    acceptActivation = aEvent.acceptActivation;
    ignoreRootScrollFrame = aEvent.ignoreRootScrollFrame;
    clickCount = aEvent.clickCount;
  }

  /**
   * Returns true if the event is a context menu event caused by key.
   */
  bool IsContextMenuKeyEvent() const
  {
    return message == NS_CONTEXTMENU && context == eContextMenuKey;
  }
};

/******************************************************************************
 * mozilla::WidgetDragEvent
 ******************************************************************************/

class WidgetDragEvent : public WidgetMouseEvent
{
public:
  virtual WidgetDragEvent* AsDragEvent() MOZ_OVERRIDE { return this; }

  WidgetDragEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget) :
    WidgetMouseEvent(aIsTrusted, aMessage, aWidget, NS_DRAG_EVENT, eReal),
    userCancelled(false), mDefaultPreventedOnContent(false)
  {
    mFlags.mCancelable =
      (aMessage != NS_DRAGDROP_EXIT_SYNTH &&
       aMessage != NS_DRAGDROP_LEAVE_SYNTH &&
       aMessage != NS_DRAGDROP_END);
  }

  // The dragging data.
  nsCOMPtr<nsIDOMDataTransfer> dataTransfer;

  // If this is true, user has cancelled the drag operation.
  bool userCancelled;
  // If this is true, the drag event's preventDefault() is called on content.
  bool mDefaultPreventedOnContent;

  // XXX Not tested by test_assign_event_data.html
  void AssignDragEventData(const WidgetDragEvent& aEvent, bool aCopyTargets)
  {
    AssignMouseEventData(aEvent, aCopyTargets);

    dataTransfer = aEvent.dataTransfer;
    // XXX userCancelled isn't copied, is this instentionally?
    userCancelled = false;
    mDefaultPreventedOnContent = aEvent.mDefaultPreventedOnContent;
  }
};

/******************************************************************************
 * mozilla::WidgetMouseScrollEvent
 *
 * This is used for legacy DOM mouse scroll events, i.e.,
 * DOMMouseScroll and MozMousePixelScroll event.  These events are NOT hanbled
 * by ESM even if widget dispatches them.  Use new WidgetWheelEvent instead.
 ******************************************************************************/

class WidgetMouseScrollEvent : public WidgetMouseEventBase
{
private:
  WidgetMouseScrollEvent()
  {
  }

public:
  virtual WidgetMouseScrollEvent* AsMouseScrollEvent() MOZ_OVERRIDE
  {
    return this;
  }

  WidgetMouseScrollEvent(bool aIsTrusted, uint32_t aMessage,
                         nsIWidget* aWidget) :
    WidgetMouseEventBase(aIsTrusted, aMessage, aWidget, NS_MOUSE_SCROLL_EVENT),
    delta(0), isHorizontal(false)
  {
  }

  // The delta value of mouse scroll event.
  // If the event message is NS_MOUSE_SCROLL, the value indicates scroll amount
  // in lines.  However, if the value is nsIDOMUIEvent::SCROLL_PAGE_UP or
  // nsIDOMUIEvent::SCROLL_PAGE_DOWN, the value inducates one page scroll.
  // If the event message is NS_MOUSE_PIXEL_SCROLL, the value indicates scroll
  // amount in pixels.
  int32_t delta;

  // If this is true, it may cause to scroll horizontally.
  // Otherwise, vertically.
  bool isHorizontal;

  void AssignMouseScrollEventData(const WidgetMouseScrollEvent& aEvent,
                                  bool aCopyTargets)
  {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    delta = aEvent.delta;
    isHorizontal = aEvent.isHorizontal;
  }
};

/******************************************************************************
 * mozilla::WidgetWheelEvent
 ******************************************************************************/

class WidgetWheelEvent : public WidgetMouseEventBase
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  WidgetWheelEvent()
  {
  }

public:
  virtual WidgetWheelEvent* AsWheelEvent() MOZ_OVERRIDE { return this; }

  WidgetWheelEvent(bool aIsTrusted, uint32_t aMessage, nsIWidget* aWidget) :
    WidgetMouseEventBase(aIsTrusted, aMessage, aWidget, NS_WHEEL_EVENT),
    deltaX(0.0), deltaY(0.0), deltaZ(0.0),
    deltaMode(nsIDOMWheelEvent::DOM_DELTA_PIXEL),
    customizedByUserPrefs(false), isMomentum(false), isPixelOnlyDevice(false),
    lineOrPageDeltaX(0), lineOrPageDeltaY(0), scrollType(SCROLL_DEFAULT),
    overflowDeltaX(0.0), overflowDeltaY(0.0),
    mViewPortIsOverscrolled(false)
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
  enum ScrollType
  {
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

  // Whether or not the parent of the currently overscrolled frame is the
  // ViewPort. This is false in situations when an element on the page is being
  // overscrolled (such as a text field), but true when the 'page' is being
  // overscrolled.
  bool mViewPortIsOverscrolled;

  void AssignWheelEventData(const WidgetWheelEvent& aEvent, bool aCopyTargets)
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
    mViewPortIsOverscrolled = aEvent.mViewPortIsOverscrolled;
  }
};

/******************************************************************************
 * mozilla::WidgetPointerEvent
 ******************************************************************************/

class WidgetPointerEvent : public WidgetMouseEvent
{
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  WidgetPointerEvent()
  {
  }

public:
  virtual WidgetPointerEvent* AsPointerEvent() MOZ_OVERRIDE { return this; }

  WidgetPointerEvent(bool aIsTrusted, uint32_t aMsg, nsIWidget* w)
    : WidgetMouseEvent(aIsTrusted, aMsg, w, NS_POINTER_EVENT, eReal)
    , pointerId(0)
    , width(0)
    , height(0)
    , tiltX(0)
    , tiltY(0)
    , isPrimary(true)
  {
  }

  WidgetPointerEvent(const WidgetMouseEvent& aEvent)
    : WidgetMouseEvent(aEvent)
    , pointerId(0)
    , width(0)
    , height(0)
    , tiltX(0)
    , tiltY(0)
    , isPrimary(true)
  {
    eventStructType = NS_POINTER_EVENT;
  }

  WidgetPointerEvent(bool aIsTrusted, uint32_t aMsg, nsIWidget* w,
                     uint32_t aPointerId,
                     uint32_t aWidth, uint32_t aHeight,
                     uint32_t aTiltX, uint32_t aTiltY, bool aIsPrimary)
    : WidgetMouseEvent(aIsTrusted, aMsg, w, NS_POINTER_EVENT, eReal)
    , pointerId(aPointerId)
    , width(aWidth)
    , height(aHeight)
    , tiltX(aTiltX)
    , tiltY(aTiltY)
    , isPrimary(aIsPrimary)
  {
  }

  uint32_t pointerId;
  uint32_t width;
  uint32_t height;
  uint32_t tiltX;
  uint32_t tiltY;
  bool isPrimary;
};

} // namespace mozilla

#endif // mozilla_MouseEvents_h__
