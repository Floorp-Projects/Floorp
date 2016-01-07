/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputData.h"

#include "mozilla/dom/Touch.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"
#include "UnitTransforms.h"

namespace mozilla {

using namespace dom;

already_AddRefed<Touch> SingleTouchData::ToNewDOMTouch() const
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only create dom::Touch instances on main thread");
  RefPtr<Touch> touch = new Touch(mIdentifier,
                                  LayoutDeviceIntPoint(mScreenPoint.x, mScreenPoint.y),
                                  LayoutDeviceIntPoint(mRadius.width, mRadius.height),
                                  mRotationAngle,
                                  mForce);
  return touch.forget();
}

MouseInput::MouseInput(const WidgetMouseEventBase& aMouseEvent)
  : InputData(MOUSE_INPUT, aMouseEvent.time, aMouseEvent.timeStamp,
              aMouseEvent.modifiers)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only copy from WidgetTouchEvent on main thread");

  mButtonType = NONE;

  switch (aMouseEvent.button) {
    case WidgetMouseEventBase::eLeftButton:
      mButtonType = MouseInput::LEFT_BUTTON;
      break;
    case WidgetMouseEventBase::eMiddleButton:
      mButtonType = MouseInput::MIDDLE_BUTTON;
      break;
    case WidgetMouseEventBase::eRightButton:
      mButtonType = MouseInput::RIGHT_BUTTON;
      break;
  }

  switch (aMouseEvent.mMessage) {
    case eMouseMove:
      mType = MOUSE_MOVE;
      break;
    case eMouseUp:
      mType = MOUSE_UP;
      break;
    case eMouseDown:
      mType = MOUSE_DOWN;
      break;
    case eDragStart:
      mType = MOUSE_DRAG_START;
      break;
    case eDragEnd:
      mType = MOUSE_DRAG_END;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Mouse event type not supported");
      break;
  }
}

bool
MouseInput::TransformToLocal(const ScreenToParentLayerMatrix4x4& aTransform)
{
  Maybe<ParentLayerPoint> point = UntransformBy(aTransform, mOrigin);
  if (!point) {
    return false;
  }
  mLocalOrigin = *point;

  return true;
}

MultiTouchInput::MultiTouchInput(const WidgetTouchEvent& aTouchEvent)
  : InputData(MULTITOUCH_INPUT, aTouchEvent.time, aTouchEvent.timeStamp,
              aTouchEvent.modifiers)
  , mHandledByAPZ(aTouchEvent.mFlags.mHandledByAPZ)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only copy from WidgetTouchEvent on main thread");

  switch (aTouchEvent.mMessage) {
    case eTouchStart:
      mType = MULTITOUCH_START;
      break;
    case eTouchMove:
      mType = MULTITOUCH_MOVE;
      break;
    case eTouchEnd:
      mType = MULTITOUCH_END;
      break;
    case eTouchCancel:
      mType = MULTITOUCH_CANCEL;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Did not assign a type to a MultiTouchInput");
      break;
  }

  for (size_t i = 0; i < aTouchEvent.touches.Length(); i++) {
    const Touch* domTouch = aTouchEvent.touches[i];

    // Extract data from weird interfaces.
    int32_t identifier = domTouch->Identifier();
    int32_t radiusX = domTouch->RadiusX();
    int32_t radiusY = domTouch->RadiusY();
    float rotationAngle = domTouch->RotationAngle();
    float force = domTouch->Force();

    SingleTouchData data(identifier,
                         ViewAs<ScreenPixel>(domTouch->mRefPoint,
                                             PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent),
                         ScreenSize(radiusX, radiusY),
                         rotationAngle,
                         force);

    mTouches.AppendElement(data);
  }
}

WidgetTouchEvent
MultiTouchInput::ToWidgetTouchEvent(nsIWidget* aWidget) const
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only convert To WidgetTouchEvent on main thread");

  EventMessage touchEventMessage = eVoidEvent;
  switch (mType) {
  case MULTITOUCH_START:
    touchEventMessage = eTouchStart;
    break;
  case MULTITOUCH_MOVE:
    touchEventMessage = eTouchMove;
    break;
  case MULTITOUCH_END:
    touchEventMessage = eTouchEnd;
    break;
  case MULTITOUCH_CANCEL:
    touchEventMessage = eTouchCancel;
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("Did not assign a type to WidgetTouchEvent in MultiTouchInput");
    break;
  }

  WidgetTouchEvent event(true, touchEventMessage, aWidget);
  if (touchEventMessage == eVoidEvent) {
    return event;
  }

  event.modifiers = this->modifiers;
  event.time = this->mTime;
  event.timeStamp = this->mTimeStamp;
  event.mFlags.mHandledByAPZ = mHandledByAPZ;

  for (size_t i = 0; i < mTouches.Length(); i++) {
    *event.touches.AppendElement() = mTouches[i].ToNewDOMTouch();
  }

  return event;
}

WidgetMouseEvent
MultiTouchInput::ToWidgetMouseEvent(nsIWidget* aWidget) const
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only convert To WidgetMouseEvent on main thread");

  EventMessage mouseEventMessage = eVoidEvent;
  switch (mType) {
    case MultiTouchInput::MULTITOUCH_START:
      mouseEventMessage = eMouseDown;
      break;
    case MultiTouchInput::MULTITOUCH_MOVE:
      mouseEventMessage = eMouseMove;
      break;
    case MultiTouchInput::MULTITOUCH_CANCEL:
    case MultiTouchInput::MULTITOUCH_END:
      mouseEventMessage = eMouseUp;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Did not assign a type to WidgetMouseEvent");
      break;
  }

  WidgetMouseEvent event(true, mouseEventMessage, aWidget,
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);

  const SingleTouchData& firstTouch = mTouches[0];
  event.refPoint.x = firstTouch.mScreenPoint.x;
  event.refPoint.y = firstTouch.mScreenPoint.y;

  event.time = mTime;
  event.button = WidgetMouseEvent::eLeftButton;
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  event.modifiers = modifiers;
  event.mFlags.mHandledByAPZ = mHandledByAPZ;

  if (mouseEventMessage != eMouseMove) {
    event.clickCount = 1;
  }

  return event;
}

int32_t
MultiTouchInput::IndexOfTouch(int32_t aTouchIdentifier)
{
  for (size_t i = 0; i < mTouches.Length(); i++) {
    if (mTouches[i].mIdentifier == aTouchIdentifier) {
      return (int32_t)i;
    }
  }
  return -1;
}

// This conversion from WidgetMouseEvent to MultiTouchInput is needed because on
// the B2G emulator we can only receive mouse events, but we need to be able
// to pan correctly. To do this, we convert the events into a format that the
// panning code can handle. This code is very limited and only supports
// SingleTouchData. It also sends garbage for the identifier, radius, force
// and rotation angle.
MultiTouchInput::MultiTouchInput(const WidgetMouseEvent& aMouseEvent)
  : InputData(MULTITOUCH_INPUT, aMouseEvent.time, aMouseEvent.timeStamp,
              aMouseEvent.modifiers)
  , mHandledByAPZ(aMouseEvent.mFlags.mHandledByAPZ)
{
  MOZ_ASSERT(NS_IsMainThread(),
             "Can only copy from WidgetMouseEvent on main thread");
  switch (aMouseEvent.mMessage) {
  case eMouseDown:
    mType = MULTITOUCH_START;
    break;
  case eMouseMove:
    mType = MULTITOUCH_MOVE;
    break;
  case eMouseUp:
    mType = MULTITOUCH_END;
    break;
  // The mouse pointer has been interrupted in an implementation-specific
  // manner, such as a synchronous event or action cancelling the touch, or a
  // touch point leaving the document window and going into a non-document
  // area capable of handling user interactions.
  case eMouseExitFromWidget:
    mType = MULTITOUCH_CANCEL;
    break;
  default:
    NS_WARNING("Did not assign a type to a MultiTouchInput");
    break;
  }

  mTouches.AppendElement(SingleTouchData(0,
                                         ViewAs<ScreenPixel>(aMouseEvent.refPoint,
                                                             PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent),
                                         ScreenSize(1, 1),
                                         180.0f,
                                         1.0f));
}

bool
MultiTouchInput::TransformToLocal(const ScreenToParentLayerMatrix4x4& aTransform)
{
  for (size_t i = 0; i < mTouches.Length(); i++) {
    Maybe<ParentLayerIntPoint> point = UntransformBy(aTransform, mTouches[i].mScreenPoint);
    if (!point) { 
      return false;
    }
    mTouches[i].mLocalScreenPoint = *point;
  }
  return true;
}

bool
PanGestureInput::IsMomentum() const
{
  switch (mType) {
    case PanGestureInput::PANGESTURE_MOMENTUMSTART:
    case PanGestureInput::PANGESTURE_MOMENTUMPAN:
    case PanGestureInput::PANGESTURE_MOMENTUMEND:
      return true;
    default:
      return false;
  }
}

WidgetWheelEvent
PanGestureInput::ToWidgetWheelEvent(nsIWidget* aWidget) const
{
  WidgetWheelEvent wheelEvent(true, eWheel, aWidget);
  wheelEvent.modifiers = this->modifiers;
  wheelEvent.time = mTime;
  wheelEvent.timeStamp = mTimeStamp;
  wheelEvent.refPoint =
    RoundedToInt(ViewAs<LayoutDevicePixel>(mPanStartPoint,
      PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent));
  wheelEvent.buttons = 0;
  wheelEvent.deltaMode = nsIDOMWheelEvent::DOM_DELTA_PIXEL;
  wheelEvent.mayHaveMomentum = true; // pan inputs may have momentum
  wheelEvent.isMomentum = IsMomentum();
  wheelEvent.lineOrPageDeltaX = mLineOrPageDeltaX;
  wheelEvent.lineOrPageDeltaY = mLineOrPageDeltaY;
  wheelEvent.deltaX = mPanDisplacement.x;
  wheelEvent.deltaY = mPanDisplacement.y;
  wheelEvent.mFlags.mHandledByAPZ = mHandledByAPZ;
  return wheelEvent;
}

bool
PanGestureInput::TransformToLocal(const ScreenToParentLayerMatrix4x4& aTransform)
{ 
  Maybe<ParentLayerPoint> panStartPoint = UntransformBy(aTransform, mPanStartPoint);
  if (!panStartPoint) {
    return false;
  }
  mLocalPanStartPoint = *panStartPoint;
  
  Maybe<ParentLayerPoint> panDisplacement = UntransformVector(aTransform, mPanDisplacement, mPanStartPoint);
  if (!panDisplacement) {
    return false;
  }
  mLocalPanDisplacement = *panDisplacement;
  return true;
}

bool
PinchGestureInput::TransformToLocal(const ScreenToParentLayerMatrix4x4& aTransform)
{ 
  Maybe<ParentLayerPoint> point = UntransformBy(aTransform, mFocusPoint);
  if (!point) {
    return false;
  }
  mLocalFocusPoint = *point;
  return true;
}

bool
TapGestureInput::TransformToLocal(const ScreenToParentLayerMatrix4x4& aTransform)
{
  Maybe<ParentLayerIntPoint> point = UntransformBy(aTransform, mPoint);
  if (!point) {
    return false;
  }
  mLocalPoint = *point;
  return true;
}

static uint32_t
DeltaModeForDeltaType(ScrollWheelInput::ScrollDeltaType aDeltaType)
{
  switch (aDeltaType) {
    case ScrollWheelInput::SCROLLDELTA_LINE:
      return nsIDOMWheelEvent::DOM_DELTA_LINE;
    case ScrollWheelInput::SCROLLDELTA_PAGE:
      return nsIDOMWheelEvent::DOM_DELTA_PAGE;
    case ScrollWheelInput::SCROLLDELTA_PIXEL:
    default:
      return nsIDOMWheelEvent::DOM_DELTA_PIXEL;
  }
}

ScrollWheelInput::ScrollWheelInput(const WidgetWheelEvent& aWheelEvent) :
  InputData(SCROLLWHEEL_INPUT, aWheelEvent.time, aWheelEvent.timeStamp, aWheelEvent.modifiers),
  mDeltaType(DeltaTypeForDeltaMode(aWheelEvent.deltaMode)),
  mScrollMode(SCROLLMODE_INSTANT),
  mHandledByAPZ(aWheelEvent.mFlags.mHandledByAPZ),
  mDeltaX(aWheelEvent.deltaX),
  mDeltaY(aWheelEvent.deltaY),
  mLineOrPageDeltaX(aWheelEvent.lineOrPageDeltaX),
  mLineOrPageDeltaY(aWheelEvent.lineOrPageDeltaY),
  mUserDeltaMultiplierX(1.0),
  mUserDeltaMultiplierY(1.0),
  mMayHaveMomentum(aWheelEvent.mayHaveMomentum),
  mIsMomentum(aWheelEvent.isMomentum)
{
  mOrigin =
    ScreenPoint(ViewAs<ScreenPixel>(aWheelEvent.refPoint,
      PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent));
}

WidgetWheelEvent
ScrollWheelInput::ToWidgetWheelEvent(nsIWidget* aWidget) const
{
  WidgetWheelEvent wheelEvent(true, eWheel, aWidget);
  wheelEvent.modifiers = this->modifiers;
  wheelEvent.time = mTime;
  wheelEvent.timeStamp = mTimeStamp;
  wheelEvent.refPoint =
    RoundedToInt(ViewAs<LayoutDevicePixel>(mOrigin,
      PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent));
  wheelEvent.buttons = 0;
  wheelEvent.deltaMode = DeltaModeForDeltaType(mDeltaType);
  wheelEvent.mayHaveMomentum = mMayHaveMomentum;
  wheelEvent.isMomentum = mIsMomentum;
  wheelEvent.deltaX = mDeltaX;
  wheelEvent.deltaY = mDeltaY;
  wheelEvent.lineOrPageDeltaX = mLineOrPageDeltaX;
  wheelEvent.lineOrPageDeltaY = mLineOrPageDeltaY;
  wheelEvent.mFlags.mHandledByAPZ = mHandledByAPZ;
  return wheelEvent;
}

bool
ScrollWheelInput::TransformToLocal(const ScreenToParentLayerMatrix4x4& aTransform)
{
  Maybe<ParentLayerPoint> point = UntransformBy(aTransform, mOrigin);
  if (!point) {
    return false;
  }
  mLocalOrigin = *point;
  return true;
}

bool
ScrollWheelInput::IsCustomizedByUserPrefs() const
{
  return mUserDeltaMultiplierX != 1.0 ||
         mUserDeltaMultiplierY != 1.0;
}

} // namespace mozilla
