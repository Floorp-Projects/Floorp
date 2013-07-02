/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MetroInput.h"

// Moz headers (alphabetical)
#include "MetroUtils.h" // Logging, POINT_CEIL_*, ActivateGenericInstance, etc
#include "MetroWidget.h" // MetroInput::mWidget
#include "mozilla/dom/Touch.h"  // Touch
#include "nsTArray.h" // Touch lists
#include "nsIDOMSimpleGestureEvent.h" // Constants for gesture events

// System headers (alphabetical)
#include <windows.ui.core.h> // ABI::Window::UI::Core namespace
#include <windows.ui.input.h> // ABI::Window::UI::Input namespace

//#define DEBUG_INPUT

// Using declarations
using namespace ABI::Windows; // UI, System, Foundation namespaces
using namespace Microsoft; // WRL namespace (ComPtr, possibly others)
using namespace mozilla::widget::winrt;
using namespace mozilla::dom;

// File-scoped statics (unnamed namespace)
namespace {
  // XXX: Set these min values appropriately
  const double SWIPE_MIN_DISTANCE = 5.0;
  const double SWIPE_MIN_VELOCITY = 5.0;

  const double WHEEL_DELTA_DOUBLE = static_cast<double>(WHEEL_DELTA);

  // Convenience typedefs for event handler types
  typedef Foundation::__FITypedEventHandler_2_Windows__CUI__CInput__CEdgeGesture_Windows__CUI__CInput__CEdgeGestureEventArgs_t EdgeGestureHandler;
  typedef Foundation::__FITypedEventHandler_2_Windows__CUI__CCore__CCoreDispatcher_Windows__CUI__CCore__CAcceleratorKeyEventArgs_t AcceleratorKeyActivatedHandler;
  typedef Foundation::__FITypedEventHandler_2_Windows__CUI__CCore__CCoreWindow_Windows__CUI__CCore__CPointerEventArgs_t PointerEventHandler;
  typedef Foundation::__FITypedEventHandler_2_Windows__CUI__CInput__CGestureRecognizer_Windows__CUI__CInput__CTappedEventArgs_t TappedEventHandler;
  typedef Foundation::__FITypedEventHandler_2_Windows__CUI__CInput__CGestureRecognizer_Windows__CUI__CInput__CRightTappedEventArgs_t RightTappedEventHandler;
  typedef Foundation::__FITypedEventHandler_2_Windows__CUI__CInput__CGestureRecognizer_Windows__CUI__CInput__CManipulationStartedEventArgs_t ManipulationStartedEventHandler;
  typedef Foundation::__FITypedEventHandler_2_Windows__CUI__CInput__CGestureRecognizer_Windows__CUI__CInput__CManipulationUpdatedEventArgs_t ManipulationUpdatedEventHandler;
  typedef Foundation::__FITypedEventHandler_2_Windows__CUI__CInput__CGestureRecognizer_Windows__CUI__CInput__CManipulationCompletedEventArgs_t ManipulationCompletedEventHandler;

  // Other convenience typedefs
  typedef ABI::Windows::UI::Core::ICoreAcceleratorKeys ICoreAcceleratorKeys;

  /**
   * Creates and returns a new {@link Touch} from the given
   * ABI::Windows::UI::Input::IPointerPoint.  Note that the caller is
   * responsible for freeing the memory for the Touch returned from
   * this function.
   *
   * @param aPoint the ABI::Windows::UI::Input::IPointerPoint containing the
   *               metadata from which to create our new {@link Touch}
   * @return a new {@link Touch} representing the touch point. The caller
   *         is responsible for freeing the memory for this touch point.
   */
  Touch*
  CreateDOMTouch(UI::Input::IPointerPoint* aPoint) {
    WRL::ComPtr<UI::Input::IPointerPointProperties> props;
    Foundation::Point position;
    uint32_t pointerId;
    Foundation::Rect contactRect;
    float pressure;

    aPoint->get_Properties(props.GetAddressOf());
    aPoint->get_Position(&position);
    aPoint->get_PointerId(&pointerId);
    props->get_ContactRect(&contactRect);
    props->get_Pressure(&pressure);

    nsIntPoint touchPoint = MetroUtils::LogToPhys(position);
    nsIntPoint touchRadius;
    touchRadius.x = MetroUtils::LogToPhys(contactRect.Width) / 2;
    touchRadius.y = MetroUtils::LogToPhys(contactRect.Height) / 2;
    return new Touch(pointerId,
                     touchPoint,
                     // Rotation radius and angle.
                     // W3C touch events v1 do not use these.
                     // The draft for W3C touch events v2 explains that
                     // radius and angle should describe the ellipse that
                     // most closely circumscribes the touching area.  Since
                     // Windows gives us a bounding rectangle rather than an
                     // ellipse, we provide the ellipse that is most closely
                     // circumscribed by the bounding rectangle that Windows
                     // gave us.
                     touchRadius,
                     0.0f,
                     // Pressure
                     // W3C touch events v1 do not use this.
                     // The current draft for W3C touch events v2 says that
                     // this should be a value between 0.0 and 1.0, which is
                     // consistent with what Windows provides us here.
                     // XXX: Windows defaults to 0.5, but the current W3C
                     // draft says that the value should be 0.0 if no value
                     // known.
                     pressure);
  }

  /**
   * Converts from the Devices::Input::PointerDeviceType enumeration
   * to a nsIDOMMouseEvent::MOZ_SOURCE_* value.
   *
   * @param aDeviceType the value to convert
   * @param aMozInputSource the converted value
   */
  void
  MozInputSourceFromDeviceType(
              Devices::Input::PointerDeviceType const& aDeviceType,
              unsigned short& aMozInputSource) {
    if (Devices::Input::PointerDeviceType::PointerDeviceType_Mouse
                  == aDeviceType) {
      aMozInputSource = nsIDOMMouseEvent::MOZ_SOURCE_MOUSE;
    } else if (Devices::Input::PointerDeviceType::PointerDeviceType_Touch
                  == aDeviceType) {
      aMozInputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
    } else if (Devices::Input::PointerDeviceType::PointerDeviceType_Pen
                  == aDeviceType) {
      aMozInputSource = nsIDOMMouseEvent::MOZ_SOURCE_PEN;
    }
  }

  /**
   * This function is for use with mTouches.Enumerate.  It will
   * append each element it encounters to the {@link nsTArray}
   * of {@link mozilla::dom::Touch}es passed in through the third (void*)
   * parameter.
   *
   * NOTE: This function will set the `mChanged` member of each
   * element it encounters to `false`, since this function is only
   * used to populate a touchlist that is about to be dispatched
   * in a gecko touch event.
   *
   * @param aKey the key of the current element being enumerated
   * @param aData the value of the current element being enumerated
   * @param aTouchList the {@link nsTArray} to append to
   */
  PLDHashOperator
  AppendToTouchList(const unsigned int& aKey,
                    nsRefPtr<Touch>& aData,
                    void *aTouchList)
  {
    nsTArray<nsRefPtr<Touch> > *touches =
              static_cast<nsTArray<nsRefPtr<Touch> > *>(aTouchList);
    touches->AppendElement(aData);
    aData->mChanged = false;
    return PL_DHASH_NEXT;
  }
}

namespace mozilla {
namespace widget {
namespace winrt {

MetroInput::MetroInput(MetroWidget* aWidget,
                       UI::Core::ICoreWindow* aWindow,
                       UI::Core::ICoreDispatcher* aDispatcher)
              : mWidget(aWidget),
                mWindow(aWindow),
                mDispatcher(aDispatcher),
                mTouchEvent(true, NS_TOUCH_MOVE, aWidget)
{
  LogFunction();
  NS_ASSERTION(aWidget, "Attempted to create MetroInput for null widget!");
  NS_ASSERTION(aWindow, "Attempted to create MetroInput for null window!");

  mTokenPointerPressed.value = 0;
  mTokenPointerReleased.value = 0;
  mTokenPointerMoved.value = 0;
  mTokenPointerEntered.value = 0;
  mTokenPointerExited.value = 0;
  mTokenPointerWheelChanged.value = 0;
  mTokenEdgeStarted.value = 0;
  mTokenEdgeCanceled.value = 0;
  mTokenEdgeCompleted.value = 0;
  mTokenManipulationStarted.value = 0;
  mTokenManipulationUpdated.value = 0;
  mTokenManipulationCompleted.value = 0;
  mTokenTapped.value = 0;
  mTokenRightTapped.value = 0;

  mTouches.Init();

  // Create our Gesture Recognizer
  ActivateGenericInstance(RuntimeClass_Windows_UI_Input_GestureRecognizer,
                          mGestureRecognizer);
  NS_ASSERTION(mGestureRecognizer, "Failed to create GestureRecognizer!");

  RegisterInputEvents();
}

MetroInput::~MetroInput()
{
  LogFunction();
  UnregisterInputEvents();
}

/**
 * When the user swipes her/his finger in from the top of the screen,
 * we receive this event.
 *
 * @param sender the CoreDispatcher that fired this event
 * @param aArgs the event-specific args we use when processing this event
 * @returns S_OK
 */
HRESULT
MetroInput::OnEdgeGestureStarted(UI::Input::IEdgeGesture* sender,
                                 UI::Input::IEdgeGestureEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif
  nsSimpleGestureEvent geckoEvent(true,
                                  NS_SIMPLE_GESTURE_EDGE_STARTED,
                                  mWidget.Get(),
                                  0,
                                  0.0);
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(geckoEvent);
  geckoEvent.time = ::GetMessageTime();

  geckoEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;

  DispatchEventIgnoreStatus(&geckoEvent);
  return S_OK;
}

/**
 * This event can be received if the user swipes her/his finger back to
 * the top of the screen, or continues moving her/his finger such that
 * the movement is interpreted as a "grab this window" gesture
 *
 * @param sender the CoreDispatcher that fired this event
 * @param aArgs the event-specific args we use when processing this event
 * @returns S_OK
 */
HRESULT
MetroInput::OnEdgeGestureCanceled(UI::Input::IEdgeGesture* sender,
                                  UI::Input::IEdgeGestureEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif
  nsSimpleGestureEvent geckoEvent(true,
                                  NS_SIMPLE_GESTURE_EDGE_CANCELED,
                                  mWidget.Get(),
                                  0,
                                  0.0);
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(geckoEvent);
  geckoEvent.time = ::GetMessageTime();

  geckoEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;

  DispatchEventIgnoreStatus(&geckoEvent);
  return S_OK;
}

/**
 * This event is received if the user presses ctrl+Z or lifts her/his
 * finger after causing an EdgeGestureStarting event to fire.
 *
 * @param sender the CoreDispatcher that fired this event
 * @param aArgs the event-specific args we use when processing this event
 * @returns S_OK
 */
HRESULT
MetroInput::OnEdgeGestureCompleted(UI::Input::IEdgeGesture* sender,
                                   UI::Input::IEdgeGestureEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif
  nsSimpleGestureEvent geckoEvent(true,
                                  NS_SIMPLE_GESTURE_EDGE_COMPLETED,
                                  mWidget.Get(),
                                  0,
                                  0.0);
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(geckoEvent);
  geckoEvent.time = ::GetMessageTime();

  UI::Input::EdgeGestureKind value;
  aArgs->get_Kind(&value);

  if (value == UI::Input::EdgeGestureKind::EdgeGestureKind_Keyboard) {
    geckoEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;
  } else {
    geckoEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  }

  DispatchEventIgnoreStatus(&geckoEvent);
  return S_OK;
}

// This event is received when the user rotates a mouse wheel.  MSDN does not
// seem to indicate that this event can be triggered from other types of input
// (i.e. pen, touch).
HRESULT
MetroInput::OnPointerWheelChanged(UI::Core::ICoreWindow* aSender,
                                  UI::Core::IPointerEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif
  WRL::ComPtr<UI::Input::IPointerPoint> currentPoint;
  WRL::ComPtr<UI::Input::IPointerPointProperties> props;
  Foundation::Point position;
  uint64_t timestamp;
  float pressure;
  boolean horzEvent;
  int32_t delta;

  aArgs->get_CurrentPoint(currentPoint.GetAddressOf());
  currentPoint->get_Position(&position);
  currentPoint->get_Timestamp(&timestamp);
  currentPoint->get_Properties(props.GetAddressOf());
  props->get_Pressure(&pressure);
  props->get_IsHorizontalMouseWheel(&horzEvent);
  props->get_MouseWheelDelta(&delta);

  WheelEvent wheelEvent(true, NS_WHEEL_WHEEL, mWidget.Get());
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(wheelEvent);
  wheelEvent.refPoint = MetroUtils::LogToPhys(position);
  wheelEvent.time = timestamp;
  wheelEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_MOUSE;
  wheelEvent.pressure = pressure;
  wheelEvent.deltaMode = nsIDOMWheelEvent::DOM_DELTA_LINE;

  static int previousVertLeftOverDelta = 0;
  static int previousHorLeftOverDelta = 0;
  // Since we have chosen DOM_DELTA_LINE as our deltaMode, deltaX or deltaY
  // should be the number of lines that we want to scroll.  Windows has given
  // us delta, which is a more precise value, and the constant WHEEL_DELTA,
  // which defines the threshold of wheel movement before an action should
  // be taken.
  if (horzEvent) {
    wheelEvent.deltaX = delta / WHEEL_DELTA_DOUBLE;
    if ((delta > 0 && previousHorLeftOverDelta < 0)
     || (delta < 0 && previousHorLeftOverDelta > 0)) {
      previousHorLeftOverDelta = 0;
    }
    previousHorLeftOverDelta += delta;
    wheelEvent.lineOrPageDeltaX = previousHorLeftOverDelta / WHEEL_DELTA;
    previousHorLeftOverDelta %= WHEEL_DELTA;
  } else {
    int mouseWheelDelta = -1 * delta;
    wheelEvent.deltaY = mouseWheelDelta / WHEEL_DELTA_DOUBLE;
    if ((mouseWheelDelta > 0 && previousVertLeftOverDelta < 0)
     || (mouseWheelDelta < 0 && previousVertLeftOverDelta > 0)) {
      previousVertLeftOverDelta = 0;
    }
    previousVertLeftOverDelta += mouseWheelDelta;
    wheelEvent.lineOrPageDeltaY = previousVertLeftOverDelta / WHEEL_DELTA;
    previousVertLeftOverDelta %= WHEEL_DELTA;
  }

  DispatchEventIgnoreStatus(&wheelEvent);

  WRL::ComPtr<UI::Input::IPointerPoint> point;
  aArgs->get_CurrentPoint(point.GetAddressOf());
  mGestureRecognizer->ProcessMouseWheelEvent(point.Get(),
                                             wheelEvent.IsShift(),
                                             wheelEvent.IsControl());
  return S_OK;
}

/**
 * This helper function is used by our processing of PointerPressed,
 * PointerReleased, and PointerMoved events.
 * It dispatches a gecko event in response to the input received.  This
 * function should only be called for non-touch (i.e. pen or mouse) input
 * events.
 *
 * @param aPoint the PointerPoint for the input event
 */
void
MetroInput::OnPointerNonTouch(UI::Input::IPointerPoint* aPoint) {
  WRL::ComPtr<UI::Input::IPointerPointProperties> props;
  UI::Input::PointerUpdateKind pointerUpdateKind;

  aPoint->get_Properties(props.GetAddressOf());
  props->get_PointerUpdateKind(&pointerUpdateKind);

  nsMouseEvent mouseEvent(true,
                          NS_MOUSE_MOVE,
                          mWidget.Get(),
                          nsMouseEvent::eReal,
                          nsMouseEvent::eNormal);

  switch (pointerUpdateKind) {
    case UI::Input::PointerUpdateKind::PointerUpdateKind_LeftButtonPressed:
      // We don't bother setting mouseEvent.button because it is already
      // set to nsMouseEvent::buttonType::eLeftButton whose value is 0.
      mouseEvent.message = NS_MOUSE_BUTTON_DOWN;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_MiddleButtonPressed:
      mouseEvent.button = nsMouseEvent::buttonType::eMiddleButton;
      mouseEvent.message = NS_MOUSE_BUTTON_DOWN;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_RightButtonPressed:
      mouseEvent.button = nsMouseEvent::buttonType::eRightButton;
      mouseEvent.message = NS_MOUSE_BUTTON_DOWN;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_LeftButtonReleased:
      // We don't bother setting mouseEvent.button because it is already
      // set to nsMouseEvent::buttonType::eLeftButton whose value is 0.
      mouseEvent.message = NS_MOUSE_BUTTON_UP;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_MiddleButtonReleased:
      mouseEvent.button = nsMouseEvent::buttonType::eMiddleButton;
      mouseEvent.message = NS_MOUSE_BUTTON_UP;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_RightButtonReleased:
      mouseEvent.button = nsMouseEvent::buttonType::eRightButton;
      mouseEvent.message = NS_MOUSE_BUTTON_UP;
      break;
  }
  InitGeckoMouseEventFromPointerPoint(mouseEvent, aPoint);
  DispatchEventIgnoreStatus(&mouseEvent);
  return;
}

// This event is raised when the user pushes the left mouse button, presses a
// pen to the surface, or presses a touch screen.
HRESULT
MetroInput::OnPointerPressed(UI::Core::ICoreWindow* aSender,
                             UI::Core::IPointerEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif

  WRL::ComPtr<UI::Input::IPointerPoint> currentPoint;
  WRL::ComPtr<Devices::Input::IPointerDevice> device;
  Devices::Input::PointerDeviceType deviceType;

  aArgs->get_CurrentPoint(currentPoint.GetAddressOf());
  currentPoint->get_PointerDevice(device.GetAddressOf());
  device->get_PointerDeviceType(&deviceType);

  // For mouse and pen input, simply call our helper function
  if (deviceType !=
          Devices::Input::PointerDeviceType::PointerDeviceType_Touch) {
    OnPointerNonTouch(currentPoint.Get());
    mGestureRecognizer->ProcessDownEvent(currentPoint.Get());
    return S_OK;
  }

  // This is touch input.
  // Create the new touch point and add it to our event.
  uint32_t pointerId;
  currentPoint->get_PointerId(&pointerId);
  nsRefPtr<Touch> touch = CreateDOMTouch(currentPoint.Get());
  touch->mChanged = true;
  mTouches.Put(pointerId, touch);
  mTouchEvent.message = NS_TOUCH_START;

  // If this is the first touchstart of a touch session,
  // dispatch it now so we can see if preventDefault gets called on it.
  if (mTouches.Count() == 1) {
    nsEventStatus status;
    DispatchPendingTouchEvent(status);
    mTouchStartDefaultPrevented = (nsEventStatus_eConsumeNoDefault == status);
    // If the first touchstart event has preventDefault called on it, then
    // we will not perform any default actions associated with any touch
    // events for this session, including touchmove events.
    // Thus, mTouchStartDefaultPrevented implies mTouchMoveDefaultPrevented.
    mTouchMoveDefaultPrevented = mTouchStartDefaultPrevented;
    mIsFirstTouchMove = !mTouchStartDefaultPrevented;
  }

  // If the first touchstart of this touch session had its preventDefault
  // called on it, we will not perform any default actions for any of the
  // touches in this touch session.
  if (!mTouchStartDefaultPrevented) {
    mGestureRecognizer->ProcessDownEvent(currentPoint.Get());
  }

  return S_OK;
}

// This event is raised when the user lifts the left mouse button, lifts a
// pen from the surface, or lifts her/his finger from a touch screen.
HRESULT
MetroInput::OnPointerReleased(UI::Core::ICoreWindow* aSender,
                              UI::Core::IPointerEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif

  WRL::ComPtr<UI::Input::IPointerPoint> currentPoint;
  WRL::ComPtr<Devices::Input::IPointerDevice> device;
  Devices::Input::PointerDeviceType deviceType;

  aArgs->get_CurrentPoint(currentPoint.GetAddressOf());
  currentPoint->get_PointerDevice(device.GetAddressOf());
  device->get_PointerDeviceType(&deviceType);

  // For mouse and pen input, simply call our helper function
  if (deviceType !=
          Devices::Input::PointerDeviceType::PointerDeviceType_Touch) {
    OnPointerNonTouch(currentPoint.Get());
    mGestureRecognizer->ProcessUpEvent(currentPoint.Get());
    return S_OK;
  }

  // This is touch input.
  // Get the touch associated with this touch point.
  uint32_t pointerId;
  currentPoint->get_PointerId(&pointerId);
  nsRefPtr<Touch> touch = mTouches.Get(pointerId);

  // We are about to dispatch a touchend.  Before we do that, we should make
  // sure that we don't have a touchmove or touchstart sitting around for this
  // point.
  if (touch->mChanged) {
    DispatchPendingTouchEvent();
  }
  mTouches.Remove(pointerId);

  // touchend events only have a single touch; the touch that has been removed
  mTouchEvent.message = NS_TOUCH_END;
  mTouchEvent.touches.Clear();
  mTouchEvent.touches.AppendElement(CreateDOMTouch(currentPoint.Get()));
  mTouchEvent.time = ::GetMessageTime();
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(mTouchEvent);
  DispatchEventIgnoreStatus(&mTouchEvent);
  // mTouchEvent.message should always be set to NS_TOUCH_MOVE
  mTouchEvent.message = NS_TOUCH_MOVE;

  // If the first touchstart of this touch session had its preventDefault
  // called on it, we will not perform any default actions for any of the
  // touches in this touch session.  Note that we don't check
  // mTouchMoveDefaultPrevented here.  The reason is that, even if
  // preventDefault was called on the first touchmove event, we might still
  // want to dispatch a click (mousemove, mousedown, mouseup) in response to
  // this touch.
  if (!mTouchStartDefaultPrevented) {
    mGestureRecognizer->ProcessUpEvent(currentPoint.Get());
  }

  return S_OK;
}

// This event is raised when the user moves the mouse, moves a pen that is
// in contact with the surface, or moves a finger that is in contact with
// a touch screen.
HRESULT
MetroInput::OnPointerMoved(UI::Core::ICoreWindow* aSender,
                           UI::Core::IPointerEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif

  WRL::ComPtr<UI::Input::IPointerPoint> currentPoint;
  WRL::ComPtr<Devices::Input::IPointerDevice> device;
  Devices::Input::PointerDeviceType deviceType;

  aArgs->get_CurrentPoint(currentPoint.GetAddressOf());
  currentPoint->get_PointerDevice(device.GetAddressOf());
  device->get_PointerDeviceType(&deviceType);

  // For mouse and pen input, simply call our helper function
  if (deviceType !=
          Devices::Input::PointerDeviceType::PointerDeviceType_Touch) {
    OnPointerNonTouch(currentPoint.Get());
    WRL::ComPtr<Foundation::Collections::IVector<UI::Input::PointerPoint*>>
        pointerPoints;
    aArgs->GetIntermediatePoints(pointerPoints.GetAddressOf());
    mGestureRecognizer->ProcessMoveEvents(pointerPoints.Get());
    return S_OK;
  }

  // This is touch input.
  // Get the touch associated with this touch point.
  uint32_t pointerId;
  currentPoint->get_PointerId(&pointerId);
  nsRefPtr<Touch> touch = mTouches.Get(pointerId);

  // Some old drivers cause us to receive a PointerMoved event for a touchId
  // after we've already received a PointerReleased event for that touchId.
  // To work around those busted drivers, we simply ignore TouchMoved events
  // for touchIds that we are not currently tracking.  See bug 819223.
  if (!touch) {
    return S_OK;
  }

  // If we're modifying a touch entry that has a pending update, go through
  // with the update.
  if (touch->mChanged) {
    DispatchPendingTouchEvent();
  }

  touch = CreateDOMTouch(currentPoint.Get());
  touch->mChanged = true;
  mTouches.Put(pointerId, touch);

  // If this is the first touch move of our session, we should dispatch it
  // and store our mTouchMoveDefaultPrevented value
  if (mIsFirstTouchMove) {
    nsEventStatus status;
    DispatchPendingTouchEvent(status);
    mTouchMoveDefaultPrevented = (nsEventStatus_eConsumeNoDefault == status);
    mIsFirstTouchMove = false;
  }

  // We will perform default actions for touchmove events only if
  // preventDefault was not called on the first touchmove event and
  // preventDefault was not called on the first touchstart event.  Checking
  // mTouchMoveDefaultPrevented is enough here because it will be set if
  // mTouchStartDefaultPrevented is true.
  if (!mTouchMoveDefaultPrevented) {
    WRL::ComPtr<Foundation::Collections::IVector<UI::Input::PointerPoint*>>
        pointerPoints;
    aArgs->GetIntermediatePoints(pointerPoints.GetAddressOf());
    mGestureRecognizer->ProcessMoveEvents(pointerPoints.Get());
  }
  return S_OK;
}

void
MetroInput::InitGeckoMouseEventFromPointerPoint(
                                  nsMouseEvent& aEvent,
                                  UI::Input::IPointerPoint* aPointerPoint) {
  NS_ASSERTION(aPointerPoint, "InitGeckoMouseEventFromPointerPoint "
                              "called with null PointerPoint!");

  WRL::ComPtr<UI::Input::IPointerPointProperties> props;
  WRL::ComPtr<Devices::Input::IPointerDevice> device;
  Devices::Input::PointerDeviceType deviceType;
  Foundation::Point position;
  uint64_t timestamp;
  float pressure;
  boolean canBeDoubleTap;

  aPointerPoint->get_Position(&position);
  aPointerPoint->get_Timestamp(&timestamp);
  aPointerPoint->get_PointerDevice(device.GetAddressOf());
  device->get_PointerDeviceType(&deviceType);
  aPointerPoint->get_Properties(props.GetAddressOf());
  props->get_Pressure(&pressure);
  mGestureRecognizer->CanBeDoubleTap(aPointerPoint, &canBeDoubleTap);

  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(aEvent);
  aEvent.refPoint = MetroUtils::LogToPhys(position);
  aEvent.time = timestamp;

  if (!canBeDoubleTap) {
    aEvent.clickCount = 1;
  } else {
    aEvent.clickCount = 2;
  }
  aEvent.pressure = pressure;

  MozInputSourceFromDeviceType(deviceType, aEvent.inputSource);
}

// This event is raised when a precise pointer moves into the bounding box of
// our window.  For touch input, this will be raised before the PointerPressed
// event.
HRESULT
MetroInput::OnPointerEntered(UI::Core::ICoreWindow* aSender,
                             UI::Core::IPointerEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif

  WRL::ComPtr<UI::Input::IPointerPoint> currentPoint;
  WRL::ComPtr<Devices::Input::IPointerDevice> device;
  Devices::Input::PointerDeviceType deviceType;

  aArgs->get_CurrentPoint(currentPoint.GetAddressOf());
  currentPoint->get_PointerDevice(device.GetAddressOf());
  device->get_PointerDeviceType(&deviceType);

  // We only dispatch mouseenter and mouseexit events for mouse and pen input.
  if (deviceType !=
          Devices::Input::PointerDeviceType::PointerDeviceType_Touch) {
    nsMouseEvent mouseEvent(true,
                            NS_MOUSE_ENTER,
                            mWidget.Get(),
                            nsMouseEvent::eReal,
                            nsMouseEvent::eNormal);
    InitGeckoMouseEventFromPointerPoint(mouseEvent, currentPoint.Get());
    DispatchEventIgnoreStatus(&mouseEvent);
  }
  return S_OK;
}

// This event is raised when a precise pointer leaves the bounding box of
// our window.  For touch input, this will be raised before the
// PointerReleased event.
HRESULT
MetroInput::OnPointerExited(UI::Core::ICoreWindow* aSender,
                            UI::Core::IPointerEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif

  WRL::ComPtr<UI::Input::IPointerPoint> currentPoint;
  WRL::ComPtr<Devices::Input::IPointerDevice> device;
  Devices::Input::PointerDeviceType deviceType;

  aArgs->get_CurrentPoint(currentPoint.GetAddressOf());
  currentPoint->get_PointerDevice(device.GetAddressOf());
  device->get_PointerDeviceType(&deviceType);

  // We only dispatch mouseenter and mouseexit events for mouse and pen input.
  if (deviceType !=
          Devices::Input::PointerDeviceType::PointerDeviceType_Touch) {
    nsMouseEvent mouseEvent(true,
                            NS_MOUSE_EXIT,
                            mWidget.Get(),
                            nsMouseEvent::eReal,
                            nsMouseEvent::eNormal);
    InitGeckoMouseEventFromPointerPoint(mouseEvent, currentPoint.Get());
    DispatchEventIgnoreStatus(&mouseEvent);
  }
  return S_OK;
}

/**
 * This helper function is called by our processing of "manipulation events".
 * Manipulation events are how Windows sends us information about swipes,
 * magnification gestures, and rotation gestures.
 *
 * @param aDelta the gesture change since the last update
 * @param aPosition the position at which the gesture is taking place
 * @param aMagEventType the event type of the gecko magnification gesture to
 *                      send
 * @param aRotEventType the event type of the gecko rotation gesture to send
 */
void
MetroInput::ProcessManipulationDelta(
                            UI::Input::ManipulationDelta const& aDelta,
                            Foundation::Point const& aPosition,
                            uint32_t aMagEventType,
                            uint32_t aRotEventType) {
  // If we ONLY have translation (no rotation, no expansion), then this
  // gesture isn't a two-finger gesture.  We ignore it here, since the only
  // thing it could eventually be is a swipe, and we deal with swipes in
  // OnManipulationCompleted.
  if ((aDelta.Translation.X != 0.0f
    || aDelta.Translation.Y != 0.0f)
   && (aDelta.Rotation == 0.0f
    && aDelta.Expansion == 0.0f)) {
    return;
  }

  // Send a gecko event indicating the magnification since the last update.
  nsSimpleGestureEvent magEvent(true,
                                aMagEventType,
                                mWidget.Get(), 0, 0.0);
  magEvent.delta = aDelta.Expansion;
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(magEvent);
  magEvent.time = ::GetMessageTime();
  magEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  magEvent.refPoint = MetroUtils::LogToPhys(aPosition);
  DispatchEventIgnoreStatus(&magEvent);

  // Send a gecko event indicating the rotation since the last update.
  nsSimpleGestureEvent rotEvent(true,
                                aRotEventType,
                                mWidget.Get(), 0, 0.0);
  rotEvent.delta = aDelta.Rotation;
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(rotEvent);
  rotEvent.time = ::GetMessageTime();
  rotEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  rotEvent.refPoint = MetroUtils::LogToPhys(aPosition);
  if (rotEvent.delta >= 0) {
    rotEvent.direction = nsIDOMSimpleGestureEvent::ROTATION_COUNTERCLOCKWISE;
  } else {
    rotEvent.direction = nsIDOMSimpleGestureEvent::ROTATION_CLOCKWISE;
  }
  DispatchEventIgnoreStatus(&rotEvent);
}

// This event is raised when a gesture is detected to have started.  The
// data that is in "aArgs->Cumulative" represents the initial update, so
// it is equivalent to what we will receive later during ManipulationUpdated
// events.
HRESULT
MetroInput::OnManipulationStarted(
                      UI::Input::IGestureRecognizer* aSender,
                      UI::Input::IManipulationStartedEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif
  UI::Input::ManipulationDelta delta;
  Foundation::Point position;

  aArgs->get_Cumulative(&delta);
  aArgs->get_Position(&position);

  ProcessManipulationDelta(delta,
                           position,
                           NS_SIMPLE_GESTURE_MAGNIFY_START,
                           NS_SIMPLE_GESTURE_ROTATE_START);
  return S_OK;
}

// This event is raised to inform us of changes in the gesture
// that is occurring.  We simply pass "aArgs->Delta" (which gives us the
// changes since the last update or start event), and "aArgs->Position"
// to our helper function.
HRESULT
MetroInput::OnManipulationUpdated(
                        UI::Input::IGestureRecognizer* aSender,
                        UI::Input::IManipulationUpdatedEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif
  UI::Input::ManipulationDelta delta;
  Foundation::Point position;

  aArgs->get_Delta(&delta);
  aArgs->get_Position(&position);

  ProcessManipulationDelta(delta,
                           position,
                           NS_SIMPLE_GESTURE_MAGNIFY_UPDATE,
                           NS_SIMPLE_GESTURE_ROTATE_UPDATE);
  return S_OK;
}

// Gecko expects a "finished" event to be sent that has the cumulative
// changes since the gesture began.  The idea is that consumers could hook
// only this last event and still effectively support magnification and
// rotation. We accomplish sending this "finished" event by calling our
// helper function with a cumulative "delta" value.
//
// After sending the "finished" event, this function detects and sends
// swipe gestures.
HRESULT
MetroInput::OnManipulationCompleted(
                        UI::Input::IGestureRecognizer* aSender,
                        UI::Input::IManipulationCompletedEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif

  UI::Input::ManipulationDelta delta;
  Foundation::Point position;
  Devices::Input::PointerDeviceType deviceType;

  aArgs->get_Position(&position);
  aArgs->get_Cumulative(&delta);
  aArgs->get_PointerDeviceType(&deviceType);

  // Send the "finished" events.  Note that we are setting
  // delta to the cumulative ManipulationDelta.
  ProcessManipulationDelta(delta,
                           position,
                           NS_SIMPLE_GESTURE_MAGNIFY,
                           NS_SIMPLE_GESTURE_ROTATE);

  // If any rotation or expansion has occurred, we know we're not dealing
  // with a swipe gesture, so let's bail early.  Also, the GestureRecognizer
  // will send us Manipulation events even for mouse input under certain
  // conditions.  I was able to initiate swipe events consistently by
  // clicking as I threw the mouse from one side of the screen to the other.
  // Thus the check for mouse input here.
  if (delta.Rotation != 0.0f
   || delta.Expansion != 0.0f
   || deviceType ==
              Devices::Input::PointerDeviceType::PointerDeviceType_Mouse) {
    return S_OK;
  }

  // No rotation or expansion occurred, so it is possible that we have a
  // swipe gesture.  We must check that the distance the user's finger
  // traveled and the velocity with which it traveled exceed our thresholds
  // for classifying the movement as a swipe.
  UI::Input::ManipulationVelocities velocities;
  aArgs->get_Velocities(&velocities);

  bool isHorizontalSwipe =
            abs(velocities.Linear.X) >= SWIPE_MIN_VELOCITY
         && abs(delta.Translation.X) >= SWIPE_MIN_DISTANCE;
  bool isVerticalSwipe =
            abs(velocities.Linear.Y) >= SWIPE_MIN_VELOCITY
         && abs(delta.Translation.Y) >= SWIPE_MIN_DISTANCE;

  // If our thresholds were exceeded for both a vertical and a horizontal
  // swipe, it means the user is flinging her/his finger around and we
  // should just ignore the input.
  if (isHorizontalSwipe && isVerticalSwipe) {
    return S_OK;
  }

  if (isHorizontalSwipe) {
    nsSimpleGestureEvent swipeEvent(true, NS_SIMPLE_GESTURE_SWIPE,
                                    mWidget.Get(), 0, 0.0);
    swipeEvent.direction = delta.Translation.X > 0
                         ? nsIDOMSimpleGestureEvent::DIRECTION_RIGHT
                         : nsIDOMSimpleGestureEvent::DIRECTION_LEFT;
    swipeEvent.delta = delta.Translation.X;
    mModifierKeyState.Update();
    mModifierKeyState.InitInputEvent(swipeEvent);
    swipeEvent.time = ::GetMessageTime();
    swipeEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
    swipeEvent.refPoint = MetroUtils::LogToPhys(position);
    DispatchEventIgnoreStatus(&swipeEvent);
  }

  if (isVerticalSwipe) {
    nsSimpleGestureEvent swipeEvent(true, NS_SIMPLE_GESTURE_SWIPE,
                                    mWidget.Get(), 0, 0.0);
    swipeEvent.direction = delta.Translation.Y > 0
                         ? nsIDOMSimpleGestureEvent::DIRECTION_DOWN
                         : nsIDOMSimpleGestureEvent::DIRECTION_UP;
    swipeEvent.delta = delta.Translation.Y;
    mModifierKeyState.Update();
    mModifierKeyState.InitInputEvent(swipeEvent);
    swipeEvent.time = ::GetMessageTime();
    swipeEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
    swipeEvent.refPoint = MetroUtils::LogToPhys(position);
    DispatchEventIgnoreStatus(&swipeEvent);
  }

  return S_OK;
}

// This event is raised when a sequence of pointer events has been
// interpreted by the GestureRecognizer as a tap (this could be a mouse
// click, a pen tap, or a tap on a touch surface).
HRESULT
MetroInput::OnTapped(UI::Input::IGestureRecognizer* aSender,
                     UI::Input::ITappedEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif

  Devices::Input::PointerDeviceType deviceType;

  aArgs->get_PointerDeviceType(&deviceType);

  // For mouse and pen input, we send mousedown/mouseup/mousemove
  // events as soon as we detect the input event.  For touch input, a set of
  // mousedown/mouseup events will be sent only once a tap has been detected.
  if (deviceType ==
              Devices::Input::PointerDeviceType::PointerDeviceType_Touch) {
    Foundation::Point position;
    aArgs->get_Position(&position);

    // Set up the mouse event that we'll reuse for mousemove, mousedown, and
    // mouseup
    nsMouseEvent mouseEvent(true,
                            NS_MOUSE_MOVE,
                            mWidget.Get(),
                            nsMouseEvent::eReal,
                            nsMouseEvent::eNormal);
    mModifierKeyState.Update();
    mModifierKeyState.InitInputEvent(mouseEvent);
    mouseEvent.refPoint = MetroUtils::LogToPhys(position);
    mouseEvent.time = ::GetMessageTime();
    aArgs->get_TapCount(&mouseEvent.clickCount);
    mouseEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;

    // Send the mousemove
    DispatchEventIgnoreStatus(&mouseEvent);

    // Send the mousedown
    mouseEvent.message = NS_MOUSE_BUTTON_DOWN;
    mouseEvent.button = nsMouseEvent::buttonType::eLeftButton;
    DispatchEventIgnoreStatus(&mouseEvent);

    // Send the mouseup
    mouseEvent.message = NS_MOUSE_BUTTON_UP;
    DispatchEventIgnoreStatus(&mouseEvent);

    // Send one more mousemove to avoid getting a hover state.
    // In the Metro environment for any application, a tap does not imply a
    // mouse cursor move.  In desktop environment for any application a tap
    // does imply a cursor move.
    POINT point;
    if (GetCursorPos(&point)) {
      ScreenToClient((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW), &point);
      Foundation::Point oldMousePosition;
      oldMousePosition.X = static_cast<FLOAT>(point.x);
      oldMousePosition.Y = static_cast<FLOAT>(point.y);
      mouseEvent.refPoint = MetroUtils::LogToPhys(oldMousePosition);
      mouseEvent.message = NS_MOUSE_MOVE;
      mouseEvent.button = 0;

      DispatchEventIgnoreStatus(&mouseEvent);
    }
  }

  return S_OK;
}

// This event is raised when a sequence of pointer events has been
// interpreted by the GestureRecognizer as a right tap.
// This could be a mouse right-click, a right-click on a pen, or
// a tap-and-hold on a touch surface.
HRESULT
MetroInput::OnRightTapped(UI::Input::IGestureRecognizer* aSender,
                          UI::Input::IRightTappedEventArgs* aArgs)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif

  Devices::Input::PointerDeviceType deviceType;
  Foundation::Point position;

  aArgs->get_PointerDeviceType(&deviceType);
  aArgs->get_Position(&position);

  nsMouseEvent contextMenu(true,
                           NS_CONTEXTMENU,
                           mWidget.Get(),
                           nsMouseEvent::eReal,
                           nsMouseEvent::eNormal);
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(contextMenu);
  contextMenu.refPoint = MetroUtils::LogToPhys(position);
  contextMenu.time = ::GetMessageTime();
  MozInputSourceFromDeviceType(deviceType, contextMenu.inputSource);
  DispatchEventIgnoreStatus(&contextMenu);

  return S_OK;
}

/**
 * Implementation Details
 */
nsEventStatus MetroInput::sThrowawayStatus;

// This function allows us to call MetroWidget's DispatchEvent function
// without passing in a status.  It uses a static nsEventStatus whose value
// is never read.  This allows us to avoid the (admittedly small) overhead
// of creating a new nsEventStatus every time we dispatch an event.
void
MetroInput::DispatchEventIgnoreStatus(nsGUIEvent *aEvent) {
  mWidget->DispatchEvent(aEvent, sThrowawayStatus);
}

void
MetroInput::DispatchPendingTouchEvent(nsEventStatus& aStatus) {
  mTouchEvent.touches.Clear();
  mTouches.Enumerate(&AppendToTouchList,
                     static_cast<void*>(&mTouchEvent.touches));
  mTouchEvent.time = ::GetMessageTime();
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(mTouchEvent);
  mWidget->DispatchEvent(&mTouchEvent, aStatus);

  // mTouchEvent.message should always be set to NS_TOUCH_MOVE
  mTouchEvent.message = NS_TOUCH_MOVE;
}

void
MetroInput::DispatchPendingTouchEvent() {
  DispatchPendingTouchEvent(sThrowawayStatus);
}

void
MetroInput::UnregisterInputEvents() {
  // Unregister ourselves for the edge swipe event
  WRL::ComPtr<UI::Input::IEdgeGestureStatics> edgeStatics;
  if (SUCCEEDED(Foundation::GetActivationFactory(
        WRL::Wrappers::HStringReference(
              RuntimeClass_Windows_UI_Input_EdgeGesture).Get(),
      edgeStatics.GetAddressOf()))) {
    WRL::ComPtr<UI::Input::IEdgeGesture> edge;
    if (SUCCEEDED(edgeStatics->GetForCurrentView(edge.GetAddressOf()))) {
      edge->remove_Starting(mTokenEdgeStarted);
      edge->remove_Canceled(mTokenEdgeCanceled);
      edge->remove_Completed(mTokenEdgeCompleted);
    }
  }
  // Unregister ourselves from the window events. This is extremely important;
  // once this object is destroyed we don't want Windows to try to send events
  // to it.
  mWindow->remove_PointerPressed(mTokenPointerPressed);
  mWindow->remove_PointerReleased(mTokenPointerReleased);
  mWindow->remove_PointerMoved(mTokenPointerMoved);
  mWindow->remove_PointerEntered(mTokenPointerEntered);
  mWindow->remove_PointerExited(mTokenPointerExited);
  mWindow->remove_PointerWheelChanged(mTokenPointerWheelChanged);

  // Unregistering from the gesture recognizer events probably isn't as
  // necessary since we're about to destroy the gesture recognizer, but
  // it can't hurt.
  mGestureRecognizer->remove_ManipulationStarted(mTokenManipulationStarted);
  mGestureRecognizer->remove_ManipulationUpdated(mTokenManipulationUpdated);
  mGestureRecognizer->remove_ManipulationCompleted(
                                        mTokenManipulationCompleted);
  mGestureRecognizer->remove_Tapped(mTokenTapped);
  mGestureRecognizer->remove_RightTapped(mTokenRightTapped);
}

void
MetroInput::RegisterInputEvents()
{
  NS_ASSERTION(mWindow, "Must have a window to register for input events!");
  NS_ASSERTION(mGestureRecognizer,
               "Must have a GestureRecognizer for input events!");
  NS_ASSERTION(mDispatcher,
               "Must have a CoreDispatcher to register for input events!");
  // Register for edge swipe
  WRL::ComPtr<UI::Input::IEdgeGestureStatics> edgeStatics;
  Foundation::GetActivationFactory(
            WRL::Wrappers::HStringReference(
                    RuntimeClass_Windows_UI_Input_EdgeGesture)
            .Get(),
            edgeStatics.GetAddressOf());
  WRL::ComPtr<UI::Input::IEdgeGesture> edge;
  edgeStatics->GetForCurrentView(edge.GetAddressOf());

  edge->add_Starting(
      WRL::Callback<EdgeGestureHandler>(
                                  this,
                                  &MetroInput::OnEdgeGestureStarted).Get(),
      &mTokenEdgeStarted);

  edge->add_Canceled(
      WRL::Callback<EdgeGestureHandler>(
                                  this,
                                  &MetroInput::OnEdgeGestureCanceled).Get(),
      &mTokenEdgeCanceled);

  edge->add_Completed(
      WRL::Callback<EdgeGestureHandler>(
                                  this,
                                  &MetroInput::OnEdgeGestureCompleted).Get(),
      &mTokenEdgeCompleted);

  // Set up our Gesture Recognizer to raise events for the gestures we
  // care about
  mGestureRecognizer->put_GestureSettings(
            UI::Input::GestureSettings::GestureSettings_Tap
          | UI::Input::GestureSettings::GestureSettings_DoubleTap
          | UI::Input::GestureSettings::GestureSettings_RightTap
          | UI::Input::GestureSettings::GestureSettings_Hold
          | UI::Input::GestureSettings::GestureSettings_ManipulationTranslateX
          | UI::Input::GestureSettings::GestureSettings_ManipulationTranslateY
          | UI::Input::GestureSettings::GestureSettings_ManipulationScale
          | UI::Input::GestureSettings::GestureSettings_ManipulationRotate);

  // Register for the pointer events on our Window
  mWindow->add_PointerPressed(
      WRL::Callback<PointerEventHandler>(
        this,
        &MetroInput::OnPointerPressed).Get(),
      &mTokenPointerPressed);

  mWindow->add_PointerReleased(
      WRL::Callback<PointerEventHandler>(
        this,
        &MetroInput::OnPointerReleased).Get(),
      &mTokenPointerReleased);

  mWindow->add_PointerMoved(
      WRL::Callback<PointerEventHandler>(
        this,
        &MetroInput::OnPointerMoved).Get(),
      &mTokenPointerMoved);

  mWindow->add_PointerEntered(
      WRL::Callback<PointerEventHandler>(
        this,
        &MetroInput::OnPointerEntered).Get(),
      &mTokenPointerEntered);

  mWindow->add_PointerExited(
      WRL::Callback<PointerEventHandler>(
        this,
        &MetroInput::OnPointerExited).Get(),
      &mTokenPointerExited);

  mWindow->add_PointerWheelChanged(
      WRL::Callback<PointerEventHandler>(
        this,
        &MetroInput::OnPointerWheelChanged).Get(),
      &mTokenPointerWheelChanged);

  // Register for the events raised by our Gesture Recognizer
  mGestureRecognizer->add_Tapped(
      WRL::Callback<TappedEventHandler>(
        this,
        &MetroInput::OnTapped).Get(),
      &mTokenTapped);

  mGestureRecognizer->add_RightTapped(
      WRL::Callback<RightTappedEventHandler>(
        this,
        &MetroInput::OnRightTapped).Get(),
      &mTokenRightTapped);

  mGestureRecognizer->add_ManipulationStarted(
      WRL::Callback<ManipulationStartedEventHandler>(
       this,
       &MetroInput::OnManipulationStarted).Get(),
     &mTokenManipulationStarted);

  mGestureRecognizer->add_ManipulationUpdated(
      WRL::Callback<ManipulationUpdatedEventHandler>(
        this,
        &MetroInput::OnManipulationUpdated).Get(),
      &mTokenManipulationUpdated);

  mGestureRecognizer->add_ManipulationCompleted(
      WRL::Callback<ManipulationCompletedEventHandler>(
        this,
        &MetroInput::OnManipulationCompleted).Get(),
      &mTokenManipulationCompleted);
}

} } }
