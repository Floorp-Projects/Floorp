/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MouseEvents_h__
#define mozilla_MouseEvents_h__

#include <stdint.h>

#include "mozilla/BasicEvents.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/dom/DataTransfer.h"
#include "nsCOMPtr.h"
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
 * mozilla::WidgetPointerHelper
 ******************************************************************************/

class WidgetPointerHelper
{
public:
  uint32_t pointerId;
  uint32_t tiltX;
  uint32_t tiltY;
  bool convertToPointer;
  bool retargetedByPointerCapture;

  WidgetPointerHelper()
    : pointerId(0)
    , tiltX(0)
    , tiltY(0)
    , convertToPointer(true)
    , retargetedByPointerCapture(false)
  {
  }

  WidgetPointerHelper(uint32_t aPointerId, uint32_t aTiltX, uint32_t aTiltY)
    : pointerId(aPointerId)
    , tiltX(aTiltX)
    , tiltY(aTiltY)
    , convertToPointer(true)
    , retargetedByPointerCapture(false)
  {
  }

  void AssignPointerHelperData(const WidgetPointerHelper& aEvent)
  {
    pointerId = aEvent.pointerId;
    tiltX = aEvent.tiltX;
    tiltY = aEvent.tiltY;
    convertToPointer = aEvent.convertToPointer;
    retargetedByPointerCapture = aEvent.retargetedByPointerCapture;
  }
};

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
    : button(0)
    , buttons(0)
    , pressure(0)
    , hitCluster(false)
    , inputSource(nsIDOMMouseEvent::MOZ_SOURCE_MOUSE)
  {
  }

  WidgetMouseEventBase(bool aIsTrusted, EventMessage aMessage,
                       nsIWidget* aWidget, EventClassID aEventClassID)
    : WidgetInputEvent(aIsTrusted, aMessage, aWidget, aEventClassID)
    , button(0)
    , buttons(0)
    , pressure(0)
    , hitCluster(false)
    , inputSource(nsIDOMMouseEvent::MOZ_SOURCE_MOUSE)
 {
 }

public:
  virtual WidgetMouseEventBase* AsMouseEventBase() override { return this; }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_CRASH("WidgetMouseEventBase must not be most-subclass");
  }

  /// The possible related target
  nsCOMPtr<nsISupports> relatedTarget;

  enum buttonType
  {
    eNoButton     = -1,
    eLeftButton   = 0,
    eMiddleButton = 1,
    eRightButton  = 2
  };
  // Pressed button ID of mousedown or mouseup event.
  // This is set only when pressing a button causes the event.
  int16_t button;

  enum buttonsFlag {
    eNoButtonFlag     = 0x00,
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
  // Touch near a cluster of links (true)
  bool hitCluster;

  // Possible values at nsIDOMMouseEvent
  uint16_t inputSource;

  // ID of the canvas HitRegion
  nsString region;

  bool IsLeftButtonPressed() const { return !!(buttons & eLeftButtonFlag); }
  bool IsRightButtonPressed() const { return !!(buttons & eRightButtonFlag); }
  bool IsMiddleButtonPressed() const { return !!(buttons & eMiddleButtonFlag); }
  bool Is4thButtonPressed() const { return !!(buttons & e4thButtonFlag); }
  bool Is5thButtonPressed() const { return !!(buttons & e5thButtonFlag); }

  void AssignMouseEventBaseData(const WidgetMouseEventBase& aEvent,
                                bool aCopyTargets)
  {
    AssignInputEventData(aEvent, aCopyTargets);

    relatedTarget = aCopyTargets ? aEvent.relatedTarget : nullptr;
    button = aEvent.button;
    buttons = aEvent.buttons;
    pressure = aEvent.pressure;
    hitCluster = aEvent.hitCluster;
    inputSource = aEvent.inputSource;
  }

  /**
   * Returns true if left click event.
   */
  bool IsLeftClickEvent() const
  {
    return mMessage == eMouseClick && button == eLeftButton;
  }
};

/******************************************************************************
 * mozilla::WidgetMouseEvent
 ******************************************************************************/

class WidgetMouseEvent : public WidgetMouseEventBase
                       , public WidgetPointerHelper
{
private:
  friend class dom::PBrowserParent;
  friend class dom::PBrowserChild;

public:
  typedef bool ReasonType;
  enum Reason : ReasonType
  {
    eReal,
    eSynthesized
  };

  typedef bool ContextMenuTriggerType;
  enum ContextMenuTrigger : ContextMenuTriggerType
  {
    eNormal,
    eContextMenuKey
  };

  typedef bool ExitFromType;
  enum ExitFrom : ExitFromType
  {
    eChild,
    eTopLevel
  };

protected:
  WidgetMouseEvent()
    : mReason(eReal)
    , mContextMenuTrigger(eNormal)
    , mExitFrom(eChild)
    , mIgnoreRootScrollFrame(false)
    , mClickCount(0)
  {
  }

  WidgetMouseEvent(bool aIsTrusted,
                   EventMessage aMessage,
                   nsIWidget* aWidget,
                   EventClassID aEventClassID,
                   Reason aReason)
    : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget, aEventClassID)
    , mReason(aReason)
    , mContextMenuTrigger(eNormal)
    , mExitFrom(eChild)
    , mIgnoreRootScrollFrame(false)
    , mClickCount(0)
  {
  }

public:
  virtual WidgetMouseEvent* AsMouseEvent() override { return this; }

  WidgetMouseEvent(bool aIsTrusted,
                   EventMessage aMessage,
                   nsIWidget* aWidget,
                   Reason aReason,
                   ContextMenuTrigger aContextMenuTrigger = eNormal)
    : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget, eMouseEventClass)
    , mReason(aReason)
    , mContextMenuTrigger(aContextMenuTrigger)
    , mExitFrom(eChild)
    , mIgnoreRootScrollFrame(false)
    , mClickCount(0)
  {
    if (aMessage == eContextMenu) {
      button = (mContextMenuTrigger == eNormal) ? eRightButton : eLeftButton;
    }
  }

#ifdef DEBUG
  virtual ~WidgetMouseEvent()
  {
    NS_WARNING_ASSERTION(
      mMessage != eContextMenu ||
      button == ((mContextMenuTrigger == eNormal) ? eRightButton : eLeftButton),
      "Wrong button set to eContextMenu event?");
  }
#endif

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eMouseEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetMouseEvent* result =
      new WidgetMouseEvent(false, mMessage, nullptr,
                           mReason, mContextMenuTrigger);
    result->AssignMouseEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  // mReason indicates the reason why the event is fired:
  // - Representing mouse operation.
  // - Synthesized for emulating mousemove event when the content under the
  //   mouse cursor is scrolled.
  Reason mReason;

  // mContextMenuTrigger is valid only when mMessage is eContextMenu.
  // This indicates if the context menu event is caused by context menu key or
  // other reasons (typically, a click of right mouse button).
  ContextMenuTrigger mContextMenuTrigger;

  // mExitFrom is valid only when mMessage is eMouseExitFromWidget.
  // This indicates if the mouse cursor exits from a top level widget or
  // a child widget.
  ExitFrom mExitFrom;

  // Whether the event should ignore scroll frame bounds during dispatch.
  bool mIgnoreRootScrollFrame;

  // mClickCount may be non-zero value when mMessage is eMouseDown, eMouseUp,
  // eMouseClick or eMouseDoubleClick. The number is count of mouse clicks.
  // Otherwise, this must be 0.
  uint32_t mClickCount;

  void AssignMouseEventData(const WidgetMouseEvent& aEvent, bool aCopyTargets)
  {
    AssignMouseEventBaseData(aEvent, aCopyTargets);
    AssignPointerHelperData(aEvent);

    mIgnoreRootScrollFrame = aEvent.mIgnoreRootScrollFrame;
    mClickCount = aEvent.mClickCount;
  }

  /**
   * Returns true if the event is a context menu event caused by key.
   */
  bool IsContextMenuKeyEvent() const
  {
    return mMessage == eContextMenu && mContextMenuTrigger == eContextMenuKey;
  }

  /**
   * Returns true if the event is a real mouse event.  Otherwise, i.e., it's
   * a synthesized event by scroll or something, returns false.
   */
  bool IsReal() const
  {
    return mReason == eReal;
  }
};

/******************************************************************************
 * mozilla::WidgetDragEvent
 ******************************************************************************/

class WidgetDragEvent : public WidgetMouseEvent
{
private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;
protected:
  WidgetDragEvent()
    : mUserCancelled(false)
    , mDefaultPreventedOnContent(false)
  {
  }
public:
  virtual WidgetDragEvent* AsDragEvent() override { return this; }

  WidgetDragEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget)
    : WidgetMouseEvent(aIsTrusted, aMessage, aWidget, eDragEventClass, eReal)
    , mUserCancelled(false)
    , mDefaultPreventedOnContent(false)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eDragEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetDragEvent* result = new WidgetDragEvent(false, mMessage, nullptr);
    result->AssignDragEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  // The dragging data.
  nsCOMPtr<dom::DataTransfer> mDataTransfer;

  // If this is true, user has cancelled the drag operation.
  bool mUserCancelled;
  // If this is true, the drag event's preventDefault() is called on content.
  bool mDefaultPreventedOnContent;

  // XXX Not tested by test_assign_event_data.html
  void AssignDragEventData(const WidgetDragEvent& aEvent, bool aCopyTargets)
  {
    AssignMouseEventData(aEvent, aCopyTargets);

    mDataTransfer = aEvent.mDataTransfer;
    // XXX mUserCancelled isn't copied, is this intentionally?
    mUserCancelled = false;
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
    : mDelta(0)
    , mIsHorizontal(false)
  {
  }

public:
  virtual WidgetMouseScrollEvent* AsMouseScrollEvent() override
  {
    return this;
  }

  WidgetMouseScrollEvent(bool aIsTrusted, EventMessage aMessage,
                         nsIWidget* aWidget)
    : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget,
                           eMouseScrollEventClass)
    , mDelta(0)
    , mIsHorizontal(false)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eMouseScrollEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetMouseScrollEvent* result =
      new WidgetMouseScrollEvent(false, mMessage, nullptr);
    result->AssignMouseScrollEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  // The delta value of mouse scroll event.
  // If the event message is eLegacyMouseLineOrPageScroll, the value indicates
  // scroll amount in lines.  However, if the value is
  // nsIDOMUIEvent::SCROLL_PAGE_UP or nsIDOMUIEvent::SCROLL_PAGE_DOWN, the
  // value inducates one page scroll.  If the event message is
  // eLegacyMousePixelScroll, the value indicates scroll amount in pixels.
  int32_t mDelta;

  // If this is true, it may cause to scroll horizontally.
  // Otherwise, vertically.
  bool mIsHorizontal;

  void AssignMouseScrollEventData(const WidgetMouseScrollEvent& aEvent,
                                  bool aCopyTargets)
  {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    mDelta = aEvent.mDelta;
    mIsHorizontal = aEvent.mIsHorizontal;
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
    : mDeltaX(0.0)
    , mDeltaY(0.0)
    , mDeltaZ(0.0)
    , mOverflowDeltaX(0.0)
    , mOverflowDeltaY(0.0)
    , mDeltaMode(nsIDOMWheelEvent::DOM_DELTA_PIXEL)
    , mLineOrPageDeltaX(0)
    , mLineOrPageDeltaY(0)
    , mScrollType(SCROLL_DEFAULT)
    , mCustomizedByUserPrefs(false)
    , mIsMomentum(false)
    , mIsNoLineOrPageDelta(false)
    , mViewPortIsOverscrolled(false)
    , mCanTriggerSwipe(false)
    , mAllowToOverrideSystemScrollSpeed(false)
  {
  }

public:
  virtual WidgetWheelEvent* AsWheelEvent() override { return this; }

  WidgetWheelEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget)
    : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget, eWheelEventClass)
    , mDeltaX(0.0)
    , mDeltaY(0.0)
    , mDeltaZ(0.0)
    , mOverflowDeltaX(0.0)
    , mOverflowDeltaY(0.0)
    , mDeltaMode(nsIDOMWheelEvent::DOM_DELTA_PIXEL)
    , mLineOrPageDeltaX(0)
    , mLineOrPageDeltaY(0)
    , mScrollType(SCROLL_DEFAULT)
    , mCustomizedByUserPrefs(false)
    , mMayHaveMomentum(false)
    , mIsMomentum(false)
    , mIsNoLineOrPageDelta(false)
    , mViewPortIsOverscrolled(false)
    , mCanTriggerSwipe(false)
    , mAllowToOverrideSystemScrollSpeed(true)
  {
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == eWheelEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetWheelEvent* result = new WidgetWheelEvent(false, mMessage, nullptr);
    result->AssignWheelEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  // On OS X, scroll gestures that start at the edge of the scrollable range
  // can result in a swipe gesture. For the first wheel event of such a
  // gesture, call TriggersSwipe() after the event has been processed
  // in order to find out whether a swipe should be started.
  bool TriggersSwipe() const
  {
    return mCanTriggerSwipe && mViewPortIsOverscrolled &&
           this->mOverflowDeltaX != 0.0;
  }

  // NOTE: mDeltaX, mDeltaY and mDeltaZ may be customized by
  //       mousewheel.*.delta_multiplier_* prefs which are applied by
  //       EventStateManager.  So, after widget dispatches this event,
  //       these delta values may have different values than before.
  double mDeltaX;
  double mDeltaY;
  double mDeltaZ;

  // overflowed delta values for scroll, these values are set by
  // nsEventStateManger.  If the default action of the wheel event isn't scroll,
  // these values always zero.  Otherwise, remaning delta values which are
  // not used by scroll are set.
  // NOTE: mDeltaX, mDeltaY and mDeltaZ may be modified by EventStateManager.
  //       However, mOverflowDeltaX and mOverflowDeltaY indicate unused original
  //       delta values which are not applied the delta_multiplier prefs.
  //       So, if widget wanted to know the actual direction to be scrolled,
  //       it would need to check the mDeltaX and mDeltaY.
  double mOverflowDeltaX;
  double mOverflowDeltaY;

  // Should be one of nsIDOMWheelEvent::DOM_DELTA_*
  uint32_t mDeltaMode;

  // If widget sets mLineOrPageDelta, EventStateManager will dispatch
  // eLegacyMouseLineOrPageScroll event for compatibility.  Note that the delta
  // value means pages if the mDeltaMode is DOM_DELTA_PAGE, otherwise, lines.
  int32_t mLineOrPageDeltaX;
  int32_t mLineOrPageDeltaY;

  // When the default action for an wheel event is moving history or zooming,
  // need to chose a delta value for doing it.
  int32_t GetPreferredIntDelta()
  {
    if (!mLineOrPageDeltaX && !mLineOrPageDeltaY) {
      return 0;
    }
    if (mLineOrPageDeltaY && !mLineOrPageDeltaX) {
      return mLineOrPageDeltaY;
    }
    if (mLineOrPageDeltaX && !mLineOrPageDeltaY) {
      return mLineOrPageDeltaX;
    }
    if ((mLineOrPageDeltaX < 0 && mLineOrPageDeltaY > 0) ||
        (mLineOrPageDeltaX > 0 && mLineOrPageDeltaY < 0)) {
      return 0; // We cannot guess the answer in this case.
    }
    return (Abs(mLineOrPageDeltaX) > Abs(mLineOrPageDeltaY)) ?
             mLineOrPageDeltaX : mLineOrPageDeltaY;
  }

  // Scroll type
  // The default value is SCROLL_DEFAULT, which means EventStateManager will
  // select preferred scroll type automatically.
  enum ScrollType : uint8_t
  {
    SCROLL_DEFAULT,
    SCROLL_SYNCHRONOUSLY,
    SCROLL_ASYNCHRONOUSELY,
    SCROLL_SMOOTHLY
  };
  ScrollType mScrollType;

  // If the delta values are computed from prefs, this value is true.
  // Otherwise, i.e., they are computed from native events, false.
  bool mCustomizedByUserPrefs;

  // true if the momentum events directly tied to this event may follow it.
  bool mMayHaveMomentum;
  // true if the event is caused by momentum.
  bool mIsMomentum;

  // If device event handlers don't know when they should set mLineOrPageDeltaX
  // and mLineOrPageDeltaY, this is true.  Otherwise, false.
  // If mIsNoLineOrPageDelta is true, ESM will generate
  // eLegacyMouseLineOrPageScroll events when accumulated delta values reach
  // a line height.
  bool mIsNoLineOrPageDelta;

  // Whether or not the parent of the currently overscrolled frame is the
  // ViewPort. This is false in situations when an element on the page is being
  // overscrolled (such as a text field), but true when the 'page' is being
  // overscrolled.
  bool mViewPortIsOverscrolled;

  // The wheel event can trigger a swipe to start if it's overscrolling the
  // viewport.
  bool mCanTriggerSwipe;

  // If mAllowToOverrideSystemScrollSpeed is true, the scroll speed may be
  // overridden.  Otherwise, the scroll speed won't be overridden even if
  // it's enabled by the pref.
  bool mAllowToOverrideSystemScrollSpeed;

  void AssignWheelEventData(const WidgetWheelEvent& aEvent, bool aCopyTargets)
  {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    mDeltaX = aEvent.mDeltaX;
    mDeltaY = aEvent.mDeltaY;
    mDeltaZ = aEvent.mDeltaZ;
    mDeltaMode = aEvent.mDeltaMode;
    mCustomizedByUserPrefs = aEvent.mCustomizedByUserPrefs;
    mMayHaveMomentum = aEvent.mMayHaveMomentum;
    mIsMomentum = aEvent.mIsMomentum;
    mIsNoLineOrPageDelta = aEvent.mIsNoLineOrPageDelta;
    mLineOrPageDeltaX = aEvent.mLineOrPageDeltaX;
    mLineOrPageDeltaY = aEvent.mLineOrPageDeltaY;
    mScrollType = aEvent.mScrollType;
    mOverflowDeltaX = aEvent.mOverflowDeltaX;
    mOverflowDeltaY = aEvent.mOverflowDeltaY;
    mViewPortIsOverscrolled = aEvent.mViewPortIsOverscrolled;
    mCanTriggerSwipe = aEvent.mCanTriggerSwipe;
    mAllowToOverrideSystemScrollSpeed =
      aEvent.mAllowToOverrideSystemScrollSpeed;
  }

  // System scroll speed settings may be too slow at using Gecko.  In such
  // case, we should override the scroll speed computed with system settings.
  // Following methods return preferred delta values which are multiplied by
  // factors specified by prefs.  If system scroll speed shouldn't be
  // overridden (e.g., this feature is disabled by pref), they return raw
  // delta values.
  double OverriddenDeltaX() const;
  double OverriddenDeltaY() const;

  // Compute the overridden delta value.  This may be useful for suppressing
  // too fast scroll by system scroll speed overriding when widget sets
  // mAllowToOverrideSystemScrollSpeed.
  static double ComputeOverriddenDelta(double aDelta, bool aIsForVertical);

private:
  static bool sInitialized;
  static bool sIsSystemScrollSpeedOverrideEnabled;
  static int32_t sOverrideFactorX;
  static int32_t sOverrideFactorY;
  static void Initialize();
};

/******************************************************************************
 * mozilla::WidgetPointerEvent
 ******************************************************************************/

class WidgetPointerEvent : public WidgetMouseEvent
{
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  WidgetPointerEvent()
    : mWidth(1)
    , mHeight(1)
    , mIsPrimary(true)
  {
  }

public:
  virtual WidgetPointerEvent* AsPointerEvent() override { return this; }

  WidgetPointerEvent(bool aIsTrusted, EventMessage aMsg, nsIWidget* w)
    : WidgetMouseEvent(aIsTrusted, aMsg, w, ePointerEventClass, eReal)
    , mWidth(1)
    , mHeight(1)
    , mIsPrimary(true)
  {
  }

  explicit WidgetPointerEvent(const WidgetMouseEvent& aEvent)
    : WidgetMouseEvent(aEvent)
    , mWidth(1)
    , mHeight(1)
    , mIsPrimary(true)
  {
    mClass = ePointerEventClass;
  }

  virtual WidgetEvent* Duplicate() const override
  {
    MOZ_ASSERT(mClass == ePointerEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetPointerEvent* result =
      new WidgetPointerEvent(false, mMessage, nullptr);
    result->AssignPointerEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  uint32_t mWidth;
  uint32_t mHeight;
  bool mIsPrimary;

  // XXX Not tested by test_assign_event_data.html
  void AssignPointerEventData(const WidgetPointerEvent& aEvent,
                              bool aCopyTargets)
  {
    AssignMouseEventData(aEvent, aCopyTargets);

    mWidth = aEvent.mWidth;
    mHeight = aEvent.mHeight;
    mIsPrimary = aEvent.mIsPrimary;
  }
};

} // namespace mozilla

#endif // mozilla_MouseEvents_h__
