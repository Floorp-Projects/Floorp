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

namespace mozilla {

namespace dom {
class PBrowserParent;
class PBrowserChild;
class PBrowserBridgeParent;
}  // namespace dom

class WidgetPointerEvent;
}  // namespace mozilla

namespace mozilla {
class WidgetPointerEventHolder final {
 public:
  nsTArray<WidgetPointerEvent> mEvents;
  NS_INLINE_DECL_REFCOUNTING(WidgetPointerEventHolder)

 private:
  virtual ~WidgetPointerEventHolder() = default;
};

/******************************************************************************
 * mozilla::WidgetPointerHelper
 ******************************************************************************/

class WidgetPointerHelper {
 public:
  uint32_t pointerId;
  uint32_t tiltX;
  uint32_t tiltY;
  uint32_t twist;
  float tangentialPressure;
  bool convertToPointer;
  RefPtr<WidgetPointerEventHolder> mCoalescedWidgetEvents;

  WidgetPointerHelper()
      : pointerId(0),
        tiltX(0),
        tiltY(0),
        twist(0),
        tangentialPressure(0),
        convertToPointer(true) {}

  WidgetPointerHelper(uint32_t aPointerId, uint32_t aTiltX, uint32_t aTiltY,
                      uint32_t aTwist = 0, float aTangentialPressure = 0)
      : pointerId(aPointerId),
        tiltX(aTiltX),
        tiltY(aTiltY),
        twist(aTwist),
        tangentialPressure(aTangentialPressure),
        convertToPointer(true) {}

  explicit WidgetPointerHelper(const WidgetPointerHelper& aHelper) = default;

  void AssignPointerHelperData(const WidgetPointerHelper& aEvent,
                               bool aCopyCoalescedEvents = false) {
    pointerId = aEvent.pointerId;
    tiltX = aEvent.tiltX;
    tiltY = aEvent.tiltY;
    twist = aEvent.twist;
    tangentialPressure = aEvent.tangentialPressure;
    convertToPointer = aEvent.convertToPointer;
    if (aCopyCoalescedEvents) {
      mCoalescedWidgetEvents = aEvent.mCoalescedWidgetEvents;
    }
  }
};

/******************************************************************************
 * mozilla::WidgetMouseEventBase
 ******************************************************************************/

class WidgetMouseEventBase : public WidgetInputEvent {
 private:
  friend class dom::PBrowserParent;
  friend class dom::PBrowserChild;
  friend class dom::PBrowserBridgeParent;

 protected:
  WidgetMouseEventBase()
      : mPressure(0),
        mButton(0),
        mButtons(0),
        mInputSource(/* MouseEvent_Binding::MOZ_SOURCE_MOUSE = */ 1) {}
  // Including MouseEventBinding.h here leads to an include loop, so
  // we have to hardcode MouseEvent_Binding::MOZ_SOURCE_MOUSE.

  WidgetMouseEventBase(bool aIsTrusted, EventMessage aMessage,
                       nsIWidget* aWidget, EventClassID aEventClassID)
      : WidgetInputEvent(aIsTrusted, aMessage, aWidget, aEventClassID),
        mPressure(0),
        mButton(0),
        mButtons(0),
        mInputSource(/* MouseEvent_Binding::MOZ_SOURCE_MOUSE = */ 1) {}
  // Including MouseEventBinding.h here leads to an include loop, so
  // we have to hardcode MouseEvent_Binding::MOZ_SOURCE_MOUSE.

 public:
  virtual WidgetMouseEventBase* AsMouseEventBase() override { return this; }

  virtual WidgetEvent* Duplicate() const override {
    MOZ_CRASH("WidgetMouseEventBase must not be most-subclass");
  }

  // Finger or touch pressure of event. It ranges between 0.0 and 1.0.
  float mPressure;

  // Pressed button ID of mousedown or mouseup event.
  // This is set only when pressing a button causes the event.
  int16_t mButton;

  // Flags of all pressed buttons at the event fired.
  // This is set at any mouse event, don't be confused with |mButton|.
  int16_t mButtons;

  // Possible values a in MouseEvent
  uint16_t mInputSource;

  bool IsLeftButtonPressed() const {
    return !!(mButtons & MouseButtonsFlag::ePrimaryFlag);
  }
  bool IsRightButtonPressed() const {
    return !!(mButtons & MouseButtonsFlag::eSecondaryFlag);
  }
  bool IsMiddleButtonPressed() const {
    return !!(mButtons & MouseButtonsFlag::eMiddleFlag);
  }
  bool Is4thButtonPressed() const {
    return !!(mButtons & MouseButtonsFlag::e4thFlag);
  }
  bool Is5thButtonPressed() const {
    return !!(mButtons & MouseButtonsFlag::e5thFlag);
  }

  void AssignMouseEventBaseData(const WidgetMouseEventBase& aEvent,
                                bool aCopyTargets) {
    AssignInputEventData(aEvent, aCopyTargets);

    mButton = aEvent.mButton;
    mButtons = aEvent.mButtons;
    mPressure = aEvent.mPressure;
    mInputSource = aEvent.mInputSource;
  }

  /**
   * Returns true if left click event.
   */
  bool IsLeftClickEvent() const {
    return mMessage == eMouseClick && mButton == MouseButton::ePrimary;
  }
};

/******************************************************************************
 * mozilla::WidgetMouseEvent
 ******************************************************************************/

class WidgetMouseEvent : public WidgetMouseEventBase,
                         public WidgetPointerHelper {
 private:
  friend class dom::PBrowserParent;
  friend class dom::PBrowserChild;
  friend class dom::PBrowserBridgeParent;

 public:
  typedef bool ReasonType;
  enum Reason : ReasonType { eReal, eSynthesized };

  typedef uint8_t ContextMenuTriggerType;
  enum ContextMenuTrigger : ContextMenuTriggerType {
    eNormal,
    eContextMenuKey,
    eControlClick
  };

  typedef uint8_t ExitFromType;
  enum ExitFrom : ExitFromType {
    ePlatformChild,
    ePlatformTopLevel,
    ePuppet,
    ePuppetParentToPuppetChild
  };

 protected:
  WidgetMouseEvent()
      : mReason(eReal),
        mContextMenuTrigger(eNormal),
        mClickCount(0),
        mIgnoreRootScrollFrame(false),
        mUseLegacyNonPrimaryDispatch(false),
        mClickEventPrevented(false) {}

  WidgetMouseEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget,
                   EventClassID aEventClassID, Reason aReason)
      : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget, aEventClassID),
        mReason(aReason),
        mContextMenuTrigger(eNormal),
        mClickCount(0),
        mIgnoreRootScrollFrame(false),
        mUseLegacyNonPrimaryDispatch(false),
        mClickEventPrevented(false) {}

#ifdef DEBUG
  void AssertContextMenuEventButtonConsistency() const;
#endif

 public:
  virtual WidgetMouseEvent* AsMouseEvent() override { return this; }

  WidgetMouseEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget,
                   Reason aReason,
                   ContextMenuTrigger aContextMenuTrigger = eNormal)
      : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget, eMouseEventClass),
        mReason(aReason),
        mContextMenuTrigger(aContextMenuTrigger),
        mClickCount(0),
        mIgnoreRootScrollFrame(false),
        mUseLegacyNonPrimaryDispatch(false),
        mClickEventPrevented(false) {
    if (aMessage == eContextMenu) {
      mButton = (mContextMenuTrigger == eNormal) ? MouseButton::eSecondary
                                                 : MouseButton::ePrimary;
    }
  }

#ifdef DEBUG
  virtual ~WidgetMouseEvent() { AssertContextMenuEventButtonConsistency(); }
#endif

  virtual WidgetEvent* Duplicate() const override {
    MOZ_ASSERT(mClass == eMouseEventClass,
               "Duplicate() must be overridden by sub class");
    // Not copying widget, it is a weak reference.
    WidgetMouseEvent* result = new WidgetMouseEvent(
        false, mMessage, nullptr, mReason, mContextMenuTrigger);
    result->AssignMouseEventData(*this, true);
    result->mFlags = mFlags;
    return result;
  }

  // If during mouseup handling we detect that click event might need to be
  // dispatched, this is setup to be the target of the click event.
  nsCOMPtr<dom::EventTarget> mClickTarget;

  // mReason indicates the reason why the event is fired:
  // - Representing mouse operation.
  // - Synthesized for emulating mousemove event when the content under the
  //   mouse cursor is scrolled.
  Reason mReason;

  // mContextMenuTrigger is valid only when mMessage is eContextMenu.
  // This indicates if the context menu event is caused by context menu key or
  // other reasons (typically, a click of right mouse button).
  ContextMenuTrigger mContextMenuTrigger;

  // mExitFrom contains a value only when mMessage is eMouseExitFromWidget.
  // This indicates if the mouse cursor exits from a top level platform widget,
  // a child widget or a puppet widget.
  Maybe<ExitFrom> mExitFrom;

  // mClickCount may be non-zero value when mMessage is eMouseDown, eMouseUp,
  // eMouseClick or eMouseDoubleClick. The number is count of mouse clicks.
  // Otherwise, this must be 0.
  uint32_t mClickCount;

  // Whether the event should ignore scroll frame bounds during dispatch.
  bool mIgnoreRootScrollFrame;

  // Indicates whether the event should dispatch click events for non-primary
  // mouse buttons on window and document.
  bool mUseLegacyNonPrimaryDispatch;

  // Whether the event shouldn't cause click event.
  bool mClickEventPrevented;

  void AssignMouseEventData(const WidgetMouseEvent& aEvent, bool aCopyTargets) {
    AssignMouseEventBaseData(aEvent, aCopyTargets);
    AssignPointerHelperData(aEvent, /* aCopyCoalescedEvents */ true);

    mExitFrom = aEvent.mExitFrom;
    mClickCount = aEvent.mClickCount;
    mIgnoreRootScrollFrame = aEvent.mIgnoreRootScrollFrame;
    mUseLegacyNonPrimaryDispatch = aEvent.mUseLegacyNonPrimaryDispatch;
    mClickEventPrevented = aEvent.mClickEventPrevented;
  }

  /**
   * Returns true if the event is a context menu event caused by key.
   */
  bool IsContextMenuKeyEvent() const {
    return mMessage == eContextMenu && mContextMenuTrigger == eContextMenuKey;
  }

  /**
   * Returns true if the event is a real mouse event.  Otherwise, i.e., it's
   * a synthesized event by scroll or something, returns false.
   */
  bool IsReal() const { return mReason == eReal; }

  /**
   * Returns true if middle click paste is enabled.
   */
  static bool IsMiddleClickPasteEnabled();
};

/******************************************************************************
 * mozilla::WidgetDragEvent
 ******************************************************************************/

class WidgetDragEvent : public WidgetMouseEvent {
 private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

 protected:
  WidgetDragEvent()
      : mUserCancelled(false), mDefaultPreventedOnContent(false) {}

 public:
  virtual WidgetDragEvent* AsDragEvent() override { return this; }

  WidgetDragEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget)
      : WidgetMouseEvent(aIsTrusted, aMessage, aWidget, eDragEventClass, eReal),
        mUserCancelled(false),
        mDefaultPreventedOnContent(false) {}

  virtual WidgetEvent* Duplicate() const override {
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
  void AssignDragEventData(const WidgetDragEvent& aEvent, bool aCopyTargets) {
    AssignMouseEventData(aEvent, aCopyTargets);

    mDataTransfer = aEvent.mDataTransfer;
    // XXX mUserCancelled isn't copied, is this intentionally?
    mUserCancelled = false;
    mDefaultPreventedOnContent = aEvent.mDefaultPreventedOnContent;
  }

  /**
   * Should be called before dispatching the DOM tree if this event is
   * synthesized for tests because drop effect is initialized before
   * dispatching from widget if it's not synthesized event, but synthesized
   * events are not initialized in the path.
   */
  void InitDropEffectForTests();
};

/******************************************************************************
 * mozilla::WidgetMouseScrollEvent
 *
 * This is used for legacy DOM mouse scroll events, i.e.,
 * DOMMouseScroll and MozMousePixelScroll event.  These events are NOT hanbled
 * by ESM even if widget dispatches them.  Use new WidgetWheelEvent instead.
 ******************************************************************************/

class WidgetMouseScrollEvent : public WidgetMouseEventBase {
 private:
  WidgetMouseScrollEvent() : mDelta(0), mIsHorizontal(false) {}

 public:
  virtual WidgetMouseScrollEvent* AsMouseScrollEvent() override { return this; }

  WidgetMouseScrollEvent(bool aIsTrusted, EventMessage aMessage,
                         nsIWidget* aWidget)
      : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget,
                             eMouseScrollEventClass),
        mDelta(0),
        mIsHorizontal(false) {}

  virtual WidgetEvent* Duplicate() const override {
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
  // UIEvent::SCROLL_PAGE_UP or UIEvent::SCROLL_PAGE_DOWN, the
  // value inducates one page scroll.  If the event message is
  // eLegacyMousePixelScroll, the value indicates scroll amount in pixels.
  int32_t mDelta;

  // If this is true, it may cause to scroll horizontally.
  // Otherwise, vertically.
  bool mIsHorizontal;

  void AssignMouseScrollEventData(const WidgetMouseScrollEvent& aEvent,
                                  bool aCopyTargets) {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    mDelta = aEvent.mDelta;
    mIsHorizontal = aEvent.mIsHorizontal;
  }
};

/******************************************************************************
 * mozilla::WidgetWheelEvent
 ******************************************************************************/

class WidgetWheelEvent : public WidgetMouseEventBase {
 private:
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

  WidgetWheelEvent()
      : mDeltaX(0.0),
        mDeltaY(0.0),
        mDeltaZ(0.0),
        mOverflowDeltaX(0.0),
        mOverflowDeltaY(0.0)
        // Including WheelEventBinding.h here leads to an include loop, so
        // we have to hardcode WheelEvent_Binding::DOM_DELTA_PIXEL.
        ,
        mDeltaMode(/* WheelEvent_Binding::DOM_DELTA_PIXEL = */ 0),
        mLineOrPageDeltaX(0),
        mLineOrPageDeltaY(0),
        mScrollType(SCROLL_DEFAULT),
        mCustomizedByUserPrefs(false),
        mMayHaveMomentum(false),
        mIsMomentum(false),
        mIsNoLineOrPageDelta(false),
        mViewPortIsOverscrolled(false),
        mCanTriggerSwipe(false),
        mAllowToOverrideSystemScrollSpeed(false),
        mDeltaValuesHorizontalizedForDefaultHandler(false) {}

 public:
  virtual WidgetWheelEvent* AsWheelEvent() override { return this; }

  WidgetWheelEvent(bool aIsTrusted, EventMessage aMessage, nsIWidget* aWidget)
      : WidgetMouseEventBase(aIsTrusted, aMessage, aWidget, eWheelEventClass),
        mDeltaX(0.0),
        mDeltaY(0.0),
        mDeltaZ(0.0),
        mOverflowDeltaX(0.0),
        mOverflowDeltaY(0.0)
        // Including WheelEventBinding.h here leads to an include loop, so
        // we have to hardcode WheelEvent_Binding::DOM_DELTA_PIXEL.
        ,
        mDeltaMode(/* WheelEvent_Binding::DOM_DELTA_PIXEL = */ 0),
        mLineOrPageDeltaX(0),
        mLineOrPageDeltaY(0),
        mScrollType(SCROLL_DEFAULT),
        mCustomizedByUserPrefs(false),
        mMayHaveMomentum(false),
        mIsMomentum(false),
        mIsNoLineOrPageDelta(false),
        mViewPortIsOverscrolled(false),
        mCanTriggerSwipe(false),
        mAllowToOverrideSystemScrollSpeed(true),
        mDeltaValuesHorizontalizedForDefaultHandler(false) {}

  virtual WidgetEvent* Duplicate() const override {
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
  bool TriggersSwipe() const {
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

  // The mousewheel tick counts.
  double mWheelTicksX = 0.0;
  double mWheelTicksY = 0.0;

  enum class DeltaModeCheckingState : uint8_t {
    // Neither deltaMode nor the delta values have been accessed.
    Unknown,
    // The delta values have been accessed, without checking deltaMode first.
    Unchecked,
    // The deltaMode has been checked.
    Checked,
  };

  // For compat reasons, we might expose a DOM_DELTA_LINE event as
  // DOM_DELTA_PIXEL instead. Whether we do that depends on whether the event
  // has been asked for the deltaMode before the deltas. If it has, we assume
  // that the page will correctly handle DOM_DELTA_LINE. This variable tracks
  // that state. See bug 1392460.
  DeltaModeCheckingState mDeltaModeCheckingState =
      DeltaModeCheckingState::Unknown;

  // The amount of scrolling per line or page, without accounting for mouse
  // wheel transactions etc.
  //
  // Computed by EventStateManager::DeltaAccumulator::InitLineOrPageDelta.
  nsSize mScrollAmount;

  // overflowed delta values for scroll, these values are set by
  // EventStateManger.  If the default action of the wheel event isn't scroll,
  // these values are always zero.  Otherwise, remaining delta values which are
  // not used by scroll are set.
  // NOTE: mDeltaX, mDeltaY and mDeltaZ may be modified by EventStateManager.
  //       However, mOverflowDeltaX and mOverflowDeltaY indicate unused original
  //       delta values which are not applied the delta_multiplier prefs.
  //       So, if widget wanted to know the actual direction to be scrolled,
  //       it would need to check the mDeltaX and mDeltaY.
  double mOverflowDeltaX;
  double mOverflowDeltaY;

  // Should be one of WheelEvent_Binding::DOM_DELTA_*
  uint32_t mDeltaMode;

  // If widget sets mLineOrPageDelta, EventStateManager will dispatch
  // eLegacyMouseLineOrPageScroll event for compatibility.  Note that the delta
  // value means pages if the mDeltaMode is DOM_DELTA_PAGE, otherwise, lines.
  int32_t mLineOrPageDeltaX;
  int32_t mLineOrPageDeltaY;

  // When the default action for an wheel event is moving history or zooming,
  // need to chose a delta value for doing it.
  int32_t GetPreferredIntDelta() {
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
      return 0;  // We cannot guess the answer in this case.
    }
    return (Abs(mLineOrPageDeltaX) > Abs(mLineOrPageDeltaY))
               ? mLineOrPageDeltaX
               : mLineOrPageDeltaY;
  }

  // Scroll type
  // The default value is SCROLL_DEFAULT, which means EventStateManager will
  // select preferred scroll type automatically.
  enum ScrollType : uint8_t {
    SCROLL_DEFAULT,
    SCROLL_SYNCHRONOUSLY,
    SCROLL_ASYNCHRONOUSLY,
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

  // After the event's default action handler has adjusted its delta's values
  // for horizontalizing a vertical wheel scroll, this variable will be set to
  // true.
  bool mDeltaValuesHorizontalizedForDefaultHandler;

  void AssignWheelEventData(const WidgetWheelEvent& aEvent, bool aCopyTargets) {
    AssignMouseEventBaseData(aEvent, aCopyTargets);

    mDeltaX = aEvent.mDeltaX;
    mDeltaY = aEvent.mDeltaY;
    mDeltaZ = aEvent.mDeltaZ;
    mDeltaMode = aEvent.mDeltaMode;
    mScrollAmount = aEvent.mScrollAmount;
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
    mDeltaValuesHorizontalizedForDefaultHandler =
        aEvent.mDeltaValuesHorizontalizedForDefaultHandler;
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

class WidgetPointerEvent : public WidgetMouseEvent {
  friend class mozilla::dom::PBrowserParent;
  friend class mozilla::dom::PBrowserChild;

 public:
  virtual WidgetPointerEvent* AsPointerEvent() override { return this; }

  WidgetPointerEvent(bool aIsTrusted, EventMessage aMsg, nsIWidget* w)
      : WidgetMouseEvent(aIsTrusted, aMsg, w, ePointerEventClass, eReal),
        mWidth(1),
        mHeight(1),
        mIsPrimary(true),
        mFromTouchEvent(false) {}

  explicit WidgetPointerEvent(const WidgetMouseEvent& aEvent)
      : WidgetMouseEvent(aEvent),
        mWidth(1),
        mHeight(1),
        mIsPrimary(true),
        mFromTouchEvent(false) {
    mClass = ePointerEventClass;
  }

  virtual WidgetEvent* Duplicate() const override {
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
  bool mFromTouchEvent;

  // XXX Not tested by test_assign_event_data.html
  void AssignPointerEventData(const WidgetPointerEvent& aEvent,
                              bool aCopyTargets) {
    AssignMouseEventData(aEvent, aCopyTargets);

    mWidth = aEvent.mWidth;
    mHeight = aEvent.mHeight;
    mIsPrimary = aEvent.mIsPrimary;
    mFromTouchEvent = aEvent.mFromTouchEvent;
  }
};

}  // namespace mozilla

#endif  // mozilla_MouseEvents_h__
