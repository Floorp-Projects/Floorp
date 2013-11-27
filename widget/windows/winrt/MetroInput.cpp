/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Moz headers (alphabetical)
#include "MetroInput.h"
#include "MetroUtils.h" // Logging, POINT_CEIL_*, ActivateGenericInstance, etc
#include "MetroWidget.h" // MetroInput::mWidget
#include "mozilla/dom/Touch.h"  // Touch
#include "nsTArray.h" // Touch lists
#include "nsIDOMSimpleGestureEvent.h" // Constants for gesture events
#include "InputData.h"
#include "UIABridgePrivate.h"
#include "MetroAppShell.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"

// System headers (alphabetical)
#include <windows.ui.core.h> // ABI::Window::UI::Core namespace
#include <windows.ui.input.h> // ABI::Window::UI::Input namespace

//#define DEBUG_INPUT

// Using declarations
using namespace ABI::Windows; // UI, System, Foundation namespaces
using namespace Microsoft; // WRL namespace (ComPtr, possibly others)
using namespace mozilla;
using namespace mozilla::widget::winrt;
using namespace mozilla::dom;

// File-scoped statics (unnamed namespace)
namespace {
  // XXX: Set these min values appropriately
  const double SWIPE_MIN_DISTANCE = 5.0;
  const double SWIPE_MIN_VELOCITY = 5.0;

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
   * Test if a touchpoint position has moved. See Touch.Equals for
   * criteria.
   *
   * @param aTouch previous touch point
   * @param aPoint new winrt touch point
   * @return true if the point has moved
   */
  bool
  HasPointMoved(Touch* aTouch, UI::Input::IPointerPoint* aPoint) {
    WRL::ComPtr<UI::Input::IPointerPointProperties> props;
    Foundation::Point position;
    Foundation::Rect contactRect;
    float pressure;

    aPoint->get_Properties(props.GetAddressOf());
    aPoint->get_Position(&position);
    props->get_ContactRect(&contactRect);
    props->get_Pressure(&pressure);
    nsIntPoint touchPoint = MetroUtils::LogToPhys(position);
    nsIntPoint touchRadius;
    touchRadius.x = MetroUtils::LogToPhys(contactRect.Width) / 2;
    touchRadius.y = MetroUtils::LogToPhys(contactRect.Height) / 2;

    // from Touch.Equals
    return touchPoint != aTouch->mRefPoint ||
           pressure != aTouch->Force() ||
           /* mRotationAngle == aTouch->RotationAngle() || */
           touchRadius.x != aTouch->RadiusX() ||
           touchRadius.y != aTouch->RadiusY();
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
    nsRefPtr<Touch> copy = new Touch(aData->mIdentifier,
               aData->mRefPoint,
               aData->mRadius,
               aData->mRotationAngle,
               aData->mForce);
    touches->AppendElement(copy);
    aData->mChanged = false;
    return PL_DHASH_NEXT;
  }

  // Helper for making sure event ptrs get freed.
  class AutoDeleteEvent
  {
  public:
    AutoDeleteEvent(WidgetGUIEvent* aPtr) :
      mPtr(aPtr) {}
    ~AutoDeleteEvent() {
      if (mPtr) {
        delete mPtr;
      }
    }
    WidgetGUIEvent* mPtr;
  };
}

namespace mozilla {
namespace widget {
namespace winrt {

MetroInput::MetroInput(MetroWidget* aWidget,
                       UI::Core::ICoreWindow* aWindow)
              : mWidget(aWidget),
                mChromeHitTestCacheForTouch(false),
                mCurrentInputLevel(LEVEL_IMPRECISE),
                mWindow(aWindow)
{
  LogFunction();
  NS_ASSERTION(aWidget, "Attempted to create MetroInput for null widget!");
  NS_ASSERTION(aWindow, "Attempted to create MetroInput for null window!");

  mTokenPointerPressed.value = 0;
  mTokenPointerReleased.value = 0;
  mTokenPointerMoved.value = 0;
  mTokenPointerEntered.value = 0;
  mTokenPointerExited.value = 0;
  mTokenEdgeStarted.value = 0;
  mTokenEdgeCanceled.value = 0;
  mTokenEdgeCompleted.value = 0;
  mTokenManipulationCompleted.value = 0;
  mTokenTapped.value = 0;
  mTokenRightTapped.value = 0;

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
 * Tracks the current input level (precise/imprecise) and fires an observer
 * when the mode changes.
 */
void
MetroInput::UpdateInputLevel(InputPrecisionLevel aInputLevel)
{
  // ignore mouse input if we have active touch input.
  if (aInputLevel == LEVEL_PRECISE && mTouches.Count() > 0) {
    return;
  }
  if (mCurrentInputLevel != aInputLevel) {
    mCurrentInputLevel = aInputLevel;
    MetroUtils::FireObserver(mCurrentInputLevel == LEVEL_PRECISE ?
                               "metro_precise_input" : "metro_imprecise_input");
  }
}

/**
 * Processes an IEdgeGestureEventArgs and returns the input source type
 * for the event. Also updates input level via UpdateInputLevel.
 */
uint16_t
MetroInput::ProcessInputTypeForGesture(UI::Input::IEdgeGestureEventArgs* aArgs)
{
  MOZ_ASSERT(aArgs);
  UI::Input::EdgeGestureKind kind;
  aArgs->get_Kind(&kind);
  switch(kind) {
    case UI::Input::EdgeGestureKind::EdgeGestureKind_Touch:
      UpdateInputLevel(LEVEL_IMPRECISE);
      return nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
    break;
    case UI::Input::EdgeGestureKind::EdgeGestureKind_Keyboard:
      return nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;
    break;
    case UI::Input::EdgeGestureKind::EdgeGestureKind_Mouse:
      UpdateInputLevel(LEVEL_PRECISE);
      return nsIDOMMouseEvent::MOZ_SOURCE_MOUSE;
    break;
  }
  return nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
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
  WidgetSimpleGestureEvent geckoEvent(true,
                                      NS_SIMPLE_GESTURE_EDGE_STARTED,
                                      mWidget.Get(),
                                      0,
                                      0.0);
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(geckoEvent);
  geckoEvent.time = ::GetMessageTime();
  geckoEvent.inputSource = ProcessInputTypeForGesture(aArgs);

  // Safe
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
  WidgetSimpleGestureEvent geckoEvent(true,
                                      NS_SIMPLE_GESTURE_EDGE_CANCELED,
                                      mWidget.Get(),
                                      0,
                                      0.0);
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(geckoEvent);
  geckoEvent.time = ::GetMessageTime();
  geckoEvent.inputSource = ProcessInputTypeForGesture(aArgs);

  // Safe
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
  WidgetSimpleGestureEvent geckoEvent(true,
                                      NS_SIMPLE_GESTURE_EDGE_COMPLETED,
                                      mWidget.Get(),
                                      0,
                                      0.0);
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(geckoEvent);
  geckoEvent.time = ::GetMessageTime();
  geckoEvent.inputSource = ProcessInputTypeForGesture(aArgs);

  // Safe
  DispatchEventIgnoreStatus(&geckoEvent);
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

  WidgetMouseEvent* event =
    new WidgetMouseEvent(true, NS_MOUSE_MOVE, mWidget.Get(),
                         WidgetMouseEvent::eReal,
                         WidgetMouseEvent::eNormal);

  switch (pointerUpdateKind) {
    case UI::Input::PointerUpdateKind::PointerUpdateKind_LeftButtonPressed:
      // We don't bother setting mouseEvent.button because it is already
      // set to WidgetMouseEvent::buttonType::eLeftButton whose value is 0.
      event->message = NS_MOUSE_BUTTON_DOWN;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_MiddleButtonPressed:
      event->button = WidgetMouseEvent::buttonType::eMiddleButton;
      event->message = NS_MOUSE_BUTTON_DOWN;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_RightButtonPressed:
      event->button = WidgetMouseEvent::buttonType::eRightButton;
      event->message = NS_MOUSE_BUTTON_DOWN;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_LeftButtonReleased:
      // We don't bother setting mouseEvent.button because it is already
      // set to WidgetMouseEvent::buttonType::eLeftButton whose value is 0.
      event->message = NS_MOUSE_BUTTON_UP;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_MiddleButtonReleased:
      event->button = WidgetMouseEvent::buttonType::eMiddleButton;
      event->message = NS_MOUSE_BUTTON_UP;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_RightButtonReleased:
      event->button = WidgetMouseEvent::buttonType::eRightButton;
      event->message = NS_MOUSE_BUTTON_UP;
      break;
  }
  UpdateInputLevel(LEVEL_PRECISE);
  InitGeckoMouseEventFromPointerPoint(event, aPoint);
  DispatchAsyncEventIgnoreStatus(event);
}

void
MetroInput::InitTouchEventTouchList(WidgetTouchEvent* aEvent)
{
  MOZ_ASSERT(aEvent);
  mTouches.Enumerate(&AppendToTouchList,
                      static_cast<void*>(&aEvent->touches));
}

bool
MetroInput::ShouldDeliverInputToRecognizer()
{
  return mRecognizerWantsEvents;
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
  UpdateInputLevel(LEVEL_IMPRECISE);

  // Create the new touch point and add it to our event.
  uint32_t pointerId;
  currentPoint->get_PointerId(&pointerId);
  nsRefPtr<Touch> touch = CreateDOMTouch(currentPoint.Get());
  touch->mChanged = true;
  mTouches.Put(pointerId, touch);

  WidgetTouchEvent* touchEvent =
    new WidgetTouchEvent(true, NS_TOUCH_START, mWidget.Get());

  if (mTouches.Count() == 1) {
    // If this is the first touchstart of a touch session reset some
    // tracking flags.
    mContentConsumingTouch = false;
    mApzConsumingTouch = false;
    mRecognizerWantsEvents = true;
    mCancelable = true;
    mCanceledIds.Clear();
  } else {
    // Only the first touchstart can be canceled.
    mCancelable = false;
  }

  InitTouchEventTouchList(touchEvent);
  DispatchAsyncTouchEvent(touchEvent);

  if (ShouldDeliverInputToRecognizer()) {
    mGestureRecognizer->ProcessDownEvent(currentPoint.Get());
  }
  return S_OK;
}

void
MetroInput::AddPointerMoveDataToRecognizer(UI::Core::IPointerEventArgs* aArgs)
{
  if (ShouldDeliverInputToRecognizer()) {
    WRL::ComPtr<Foundation::Collections::IVector<UI::Input::PointerPoint*>>
        pointerPoints;
    aArgs->GetIntermediatePoints(pointerPoints.GetAddressOf());
    mGestureRecognizer->ProcessMoveEvents(pointerPoints.Get());
  }
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
    AddPointerMoveDataToRecognizer(aArgs);
    return S_OK;
  }

  // This is touch input.
  UpdateInputLevel(LEVEL_IMPRECISE);

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

  AddPointerMoveDataToRecognizer(aArgs);

  // If the point hasn't moved, filter it out per the spec. Pres shell does
  // this as well, but we need to know when our first touchmove is going to
  // get delivered so we can check the result.
  if (!HasPointMoved(touch, currentPoint.Get())) {
    return S_OK;
  }

  touch = CreateDOMTouch(currentPoint.Get());
  touch->mChanged = true;
  // replacing old touch point in mTouches map
  mTouches.Put(pointerId, touch);

  WidgetTouchEvent* touchEvent =
    new WidgetTouchEvent(true, NS_TOUCH_MOVE, mWidget.Get());
  InitTouchEventTouchList(touchEvent);
  DispatchAsyncTouchEvent(touchEvent);

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
  UpdateInputLevel(LEVEL_IMPRECISE);

  // Get the touch associated with this touch point.
  uint32_t pointerId;
  currentPoint->get_PointerId(&pointerId);
  nsRefPtr<Touch> touch = mTouches.Get(pointerId);

  // Purge any pending moves for this pointer
  if (touch->mChanged) {
    WidgetTouchEvent* touchEvent =
      new WidgetTouchEvent(true, NS_TOUCH_MOVE, mWidget.Get());
    InitTouchEventTouchList(touchEvent);
    DispatchAsyncTouchEvent(touchEvent);
  }

  // Remove this touch point from our map. Eventually all touch points are
  // removed for this session since we receive released events for every
  // point. 
  mTouches.Remove(pointerId);

  // touchend events only have a single touch; the touch that has been removed
  WidgetTouchEvent* touchEvent =
    new WidgetTouchEvent(true, NS_TOUCH_END, mWidget.Get());
  touchEvent->touches.AppendElement(CreateDOMTouch(currentPoint.Get()));
  DispatchAsyncTouchEvent(touchEvent);

  if (ShouldDeliverInputToRecognizer()) {
    mGestureRecognizer->ProcessUpEvent(currentPoint.Get());
  }

  // Make sure all gecko events are dispatched and the dom is up to date
  // so that when ui automation comes in looking for focus info it gets
  // the right information.
  MetroAppShell::MarkEventQueueForPurge();

  return S_OK;
}

// Tests for chrome vs. content target so we know whether input coordinates need
// to be transformed through the apz. Eventually this hit testing should move
// into the apz (bug 918288).
bool
MetroInput::HitTestChrome(const LayoutDeviceIntPoint& pt)
{
  // Confirm this event targets content. We pick this up in browser's input.js.
  WidgetMouseEvent hittest(true, NS_MOUSE_MOZHITTEST, mWidget.Get(),
                           WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
  hittest.refPoint = pt;
  nsEventStatus status;
  mWidget->DispatchEvent(&hittest, status);
  return (status == nsEventStatus_eConsumeNoDefault);
}

/**
 * Returns true if the position is in chrome, false otherwise.
 */
bool
MetroInput::TransformRefPoint(const Foundation::Point& aPosition, LayoutDeviceIntPoint& aRefPointOut)
{
  // If this event is destined for content we need to transform our ref point through
  // the apz so that zoom can be accounted for.
  aRefPointOut = LayoutDeviceIntPoint::FromUntyped(MetroUtils::LogToPhys(aPosition));
  ScreenIntPoint spt;
  spt.x = aRefPointOut.x;
  spt.y = aRefPointOut.y;
  // This is currently a general contained rect hit test, it may produce a false positive for
  // overlay chrome elements.
  bool apzIntersect = mWidget->ApzHitTest(spt);
  if (!apzIntersect) {
    return true;
  }
  if (HitTestChrome(aRefPointOut)) {
    return true;
  }
  mWidget->ApzTransformGeckoCoordinate(spt, &aRefPointOut);
  return false;
}

void
MetroInput::TransformTouchEvent(WidgetTouchEvent* aEvent)
{
  nsTArray< nsRefPtr<dom::Touch> >& touches = aEvent->touches;
  for (uint32_t i = 0; i < touches.Length(); ++i) {
    dom::Touch* touch = touches[i];
    if (touch) {
      LayoutDeviceIntPoint lpt;
      ScreenIntPoint spt;
      spt.x = touch->mRefPoint.x;
      spt.y = touch->mRefPoint.y;
      mWidget->ApzTransformGeckoCoordinate(spt, &lpt);
      touch->mRefPoint.x = lpt.x;
      touch->mRefPoint.y = lpt.y;
    }
  }
}

void
MetroInput::InitGeckoMouseEventFromPointerPoint(
                                  WidgetMouseEvent* aEvent,
                                  UI::Input::IPointerPoint* aPointerPoint)
{
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

  TransformRefPoint(position, aEvent->refPoint);

  if (!canBeDoubleTap) {
    aEvent->clickCount = 1;
  } else {
    aEvent->clickCount = 2;
  }
  aEvent->pressure = pressure;

  MozInputSourceFromDeviceType(deviceType, aEvent->inputSource);
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
    WidgetMouseEvent* event =
      new WidgetMouseEvent(true, NS_MOUSE_ENTER, mWidget.Get(),
                           WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
    UpdateInputLevel(LEVEL_PRECISE);
    InitGeckoMouseEventFromPointerPoint(event, currentPoint.Get());
    DispatchAsyncEventIgnoreStatus(event);
    return S_OK;
  }
  UpdateInputLevel(LEVEL_IMPRECISE);
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
    WidgetMouseEvent* event =
      new WidgetMouseEvent(true, NS_MOUSE_EXIT, mWidget.Get(),
                           WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
    UpdateInputLevel(LEVEL_PRECISE);
    InitGeckoMouseEventFromPointerPoint(event, currentPoint.Get());
    DispatchAsyncEventIgnoreStatus(event);
    return S_OK;
  }
  UpdateInputLevel(LEVEL_IMPRECISE);
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

  Devices::Input::PointerDeviceType deviceType;
  aArgs->get_PointerDeviceType(&deviceType);
  if (deviceType ==
              Devices::Input::PointerDeviceType::PointerDeviceType_Mouse) {
    return S_OK;
  }

  UI::Input::ManipulationDelta delta;
  Foundation::Point position;

  aArgs->get_Position(&position);
  aArgs->get_Cumulative(&delta);

  // We check that the distance the user's finger traveled and the
  // velocity with which it traveled exceed our thresholds for
  // classifying the movement as a swipe.
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
    WidgetSimpleGestureEvent* swipeEvent =
      new WidgetSimpleGestureEvent(true, NS_SIMPLE_GESTURE_SWIPE,
                                   mWidget.Get(), 0, 0.0);
    swipeEvent->direction = delta.Translation.X > 0
                         ? nsIDOMSimpleGestureEvent::DIRECTION_RIGHT
                         : nsIDOMSimpleGestureEvent::DIRECTION_LEFT;
    swipeEvent->delta = delta.Translation.X;
    swipeEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
    swipeEvent->refPoint = LayoutDeviceIntPoint::FromUntyped(MetroUtils::LogToPhys(position));
    DispatchAsyncEventIgnoreStatus(swipeEvent);
  }

  if (isVerticalSwipe) {
    WidgetSimpleGestureEvent* swipeEvent =
      new WidgetSimpleGestureEvent(true, NS_SIMPLE_GESTURE_SWIPE,
                                   mWidget.Get(), 0, 0.0);
    swipeEvent->direction = delta.Translation.Y > 0
                         ? nsIDOMSimpleGestureEvent::DIRECTION_DOWN
                         : nsIDOMSimpleGestureEvent::DIRECTION_UP;
    swipeEvent->delta = delta.Translation.Y;
    swipeEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
    swipeEvent->refPoint = LayoutDeviceIntPoint::FromUntyped(MetroUtils::LogToPhys(position));
    DispatchAsyncEventIgnoreStatus(swipeEvent);
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

  unsigned int tapCount;
  aArgs->get_TapCount(&tapCount);

  // For mouse and pen input, we send mousedown/mouseup/mousemove
  // events as soon as we detect the input event.  For touch input, a set of
  // mousedown/mouseup events will be sent only once a tap has been detected.
  if (deviceType != Devices::Input::PointerDeviceType::PointerDeviceType_Touch) {
    return S_OK;
  }

  Foundation::Point position;
  aArgs->get_Position(&position);
  HandleTap(position, tapCount);
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
  aArgs->get_PointerDeviceType(&deviceType);

  Foundation::Point position;
  aArgs->get_Position(&position);
  HandleLongTap(position);

  return S_OK;
}

void
MetroInput::HandleTap(const Foundation::Point& aPoint, unsigned int aTapCount)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif

  LayoutDeviceIntPoint refPoint;
  bool hitTestChrome = TransformRefPoint(aPoint, refPoint);
  if (!hitTestChrome) {
    // Let APZC handle tap/doubletap detection for content.
    return;
  }

  // send mousemove
  WidgetMouseEvent* mouseEvent =
    new WidgetMouseEvent(true, NS_MOUSE_MOVE, mWidget.Get(),
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
  mouseEvent->refPoint = refPoint;
  mouseEvent->clickCount = aTapCount;
  mouseEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  DispatchAsyncEventIgnoreStatus(mouseEvent);

  // Send the mousedown
  mouseEvent =
    new WidgetMouseEvent(true, NS_MOUSE_BUTTON_DOWN, mWidget.Get(),
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
  mouseEvent->refPoint = refPoint;
  mouseEvent->clickCount = aTapCount;
  mouseEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  mouseEvent->button = WidgetMouseEvent::buttonType::eLeftButton;
  DispatchAsyncEventIgnoreStatus(mouseEvent);

  mouseEvent =
    new WidgetMouseEvent(true, NS_MOUSE_BUTTON_UP, mWidget.Get(),
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
  mouseEvent->refPoint = refPoint;
  mouseEvent->clickCount = aTapCount;
  mouseEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  mouseEvent->button = WidgetMouseEvent::buttonType::eLeftButton;
  DispatchAsyncEventIgnoreStatus(mouseEvent);

  // Send one more mousemove to avoid getting a hover state.
  // In the Metro environment for any application, a tap does not imply a
  // mouse cursor move.  In desktop environment for any application a tap
  // does imply a cursor move.
  POINT point;
  if (GetCursorPos(&point)) {
    ScreenToClient((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW), &point);
    mouseEvent =
      new WidgetMouseEvent(true, NS_MOUSE_MOVE, mWidget.Get(),
                           WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
    mouseEvent->refPoint = LayoutDeviceIntPoint(point.x, point.y);
    mouseEvent->clickCount = aTapCount;
    mouseEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
    DispatchAsyncEventIgnoreStatus(mouseEvent);
  }
}

void
MetroInput::HandleLongTap(const Foundation::Point& aPoint)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif
  LayoutDeviceIntPoint refPoint;
  TransformRefPoint(aPoint, refPoint);

  WidgetMouseEvent* contextEvent =
    new WidgetMouseEvent(true, NS_CONTEXTMENU, mWidget.Get(),
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
  contextEvent->refPoint = refPoint;
  contextEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  DispatchAsyncEventIgnoreStatus(contextEvent);
}

/**
 * Implementation Details
 */
nsEventStatus MetroInput::sThrowawayStatus;

void
MetroInput::DispatchAsyncEventIgnoreStatus(WidgetInputEvent* aEvent)
{
  aEvent->time = ::GetMessageTime();
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(*aEvent);
  mInputEventQueue.Push(aEvent);
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &MetroInput::DeliverNextQueuedEventIgnoreStatus);
  NS_DispatchToCurrentThread(runnable);
}

void
MetroInput::DeliverNextQueuedEventIgnoreStatus()
{
  WidgetGUIEvent* event =
    static_cast<WidgetGUIEvent*>(mInputEventQueue.PopFront());
  MOZ_ASSERT(event);
  DispatchEventIgnoreStatus(event);
  delete event;
}

void
MetroInput::DispatchAsyncTouchEvent(WidgetTouchEvent* aEvent)
{
  aEvent->time = ::GetMessageTime();
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(*aEvent);
  mInputEventQueue.Push(aEvent);
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &MetroInput::DeliverNextQueuedTouchEvent);
  NS_DispatchToCurrentThread(runnable);
}

static void DumpTouchIds(const char* aTarget, WidgetTouchEvent* aEvent)
{
  // comment out for touch moves
  if (aEvent->message == NS_TOUCH_MOVE) {
    return;
  }
  switch(aEvent->message) {
    case NS_TOUCH_START:
    WinUtils::Log("DumpTouchIds: NS_TOUCH_START block");
    break;
    case NS_TOUCH_MOVE:
    WinUtils::Log("DumpTouchIds: NS_TOUCH_MOVE block");
    break;
    case NS_TOUCH_END:
    WinUtils::Log("DumpTouchIds: NS_TOUCH_END block");
    break;
    case NS_TOUCH_CANCEL:
    WinUtils::Log("DumpTouchIds: NS_TOUCH_CANCEL block");
    break;
  }
  nsTArray< nsRefPtr<dom::Touch> >& touches = aEvent->touches;
  for (uint32_t i = 0; i < touches.Length(); ++i) {
    dom::Touch* touch = touches[i];
    if (!touch) {
      continue;
    }
    int32_t id = touch->Identifier();
    WinUtils::Log("   id=%d target=%s", id, aTarget);
  }
}

/*
 * nsPreShell's processing of WidgetTouchEvent events:
 *
 * NS_TOUCH_START:
 *  Interprets a single touch point as the first touch point of a block and will reset its
 *  queue when it receives this. For multiple touch points it sets all points in its queue
 *  and marks new points as changed.
 * NS_TOUCH_MOVE:
 *  Uses the equality tests in dom::Touch to test if a touch point has changed (moved).
 *  If a point has moved, keeps this touch point in the event, otherwise it removes
 *  the touch point. Note if no points have changed, it exits without sending a dom event.
 * NS_TOUCH_CANCEL/NS_TOUCH_END
 *  Assumes any point in touchEvent->touches has been removed or canceled.
*/

//#define DUMP_TOUCH_IDS(aTarget, aEvent) DumpTouchIds(aTarget, aEvent)
#define DUMP_TOUCH_IDS(...)

void
MetroInput::DeliverNextQueuedTouchEvent()
{
  nsEventStatus status;
  WidgetTouchEvent* event =
    static_cast<WidgetTouchEvent*>(mInputEventQueue.PopFront());
  MOZ_ASSERT(event);

  AutoDeleteEvent wrap(event);

  /*
   * We go through states here and make different decisions in each:
   *
   * 1) Hit test chrome on first touchstart
   *  If chrome is the target simplify event delivery from that point
   *  on by directing all input to chrome, bypassing the apz.
   * 2) Process first touchpoint touchstart and touchmove
   *  Check the result and set mContentConsumingTouch appropriately. Deliver
   *  touch events to the apz (ignoring return result) and to content.
   * 3) If mContentConsumingTouch is true: deliver touch to content after
   *  transforming through the apz. Also let the apz know content is
   *  consuming touch.
   * 4) If mContentConsumingTouch is false: check the result from the apz and
   *  set mApzConsumingTouch appropriately.
   * 5) If mApzConsumingTouch is true: send a touchcancel to content
   *  and deliver all events to the apz. If the apz is doing something with
   *  the events we can save ourselves the overhead of delivering dom events.
   *
   * Notes:
   * - never rely on the contents of mTouches here, since this is a delayed
   *   callback. mTouches will likely have been modified.
   */

  // Test for chrome vs. content target. To do this we only use the first touch
  // point since that will be the input batch target. Cache this for touch events
  // since HitTestChrome has to send a dom event.
  if (mCancelable && event->message == NS_TOUCH_START) {
    nsRefPtr<Touch> touch = event->touches[0];
    LayoutDeviceIntPoint pt = LayoutDeviceIntPoint::FromUntyped(touch->mRefPoint);
    bool apzIntersect = mWidget->ApzHitTest(mozilla::ScreenIntPoint(pt.x, pt.y));
    mChromeHitTestCacheForTouch = (apzIntersect && HitTestChrome(pt));
  }

  // If this event is destined for chrome, deliver it directly there bypassing
  // the apz.
  if (!mCancelable && mChromeHitTestCacheForTouch) {
    DUMP_TOUCH_IDS("DOM(1)", event);
    mWidget->DispatchEvent(event, status);
    return;
  }

  // If we have yet to deliver the first touch start and touch move, deliver the
  // event to both content and the apz. Ignore the apz's return result since we
  // give content the option of saying it wants to consume touch for both events.
  if (mCancelable) {
    WidgetTouchEvent transformedEvent(*event);
    DUMP_TOUCH_IDS("APZC(1)", event);
    mWidget->ApzReceiveInputEvent(event, &mTargetAPZCGuid, &transformedEvent);
    DUMP_TOUCH_IDS("DOM(2)", event);
    mWidget->DispatchEvent(mChromeHitTestCacheForTouch ? event : &transformedEvent, status);
    if (event->message == NS_TOUCH_START) {
      mContentConsumingTouch = (nsEventStatus_eConsumeNoDefault == status);
      // If we know content wants touch here, we can bail early on mCancelable
      // processing. This insures the apz gets an update when the user touches
      // the screen, but doesn't move soon after.
      if (mContentConsumingTouch) {
        mCancelable = false;
        mWidget->ApzContentConsumingTouch(mTargetAPZCGuid);
        DispatchTouchCancel(event);
      }
      // Disable gesture based events (taps, swipes, rotation) if
      // preventDefault is called on touchstart.
      mRecognizerWantsEvents = !(nsEventStatus_eConsumeNoDefault == status);
    } else if (event->message == NS_TOUCH_MOVE) {
      mCancelable = false;
      // Add this result to to our content comsuming flag
      if (!mContentConsumingTouch) {
        mContentConsumingTouch = (nsEventStatus_eConsumeNoDefault == status);
      }
      // Let the apz know if content wants to consume touch events, or cancel
      // the touch block for content.
      if (mContentConsumingTouch) {
        mWidget->ApzContentConsumingTouch(mTargetAPZCGuid);
        DispatchTouchCancel(event);
      } else {
        mWidget->ApzContentIgnoringTouch(mTargetAPZCGuid);
      }
    }
    // If content is consuming touch don't generate any gesture based
    // input - clear the recognizer state without sending any events.
    if (!ShouldDeliverInputToRecognizer()) {
      mGestureRecognizer->CompleteGesture();
    }
    return;
  }

  // If content is consuming touch, we may need to transform event coords
  // through the apzc before sending to the dom. Otherwise send the event
  // to apzc.
  if (mContentConsumingTouch) {
    // Only translate if we're dealing with web content that's transformed
    // by the apzc.
    if (!mChromeHitTestCacheForTouch) {
      TransformTouchEvent(event);
    }
    DUMP_TOUCH_IDS("DOM(3)", event);
    mWidget->DispatchEvent(event, status);
    return;
  }

  DUMP_TOUCH_IDS("APZC(2)", event);
  status = mWidget->ApzReceiveInputEvent(event, nullptr);

  // Send the event to content unless APZC is consuming it.
  if (!mApzConsumingTouch) {
    if (status == nsEventStatus_eConsumeNoDefault) {
      mApzConsumingTouch = true;
      DispatchTouchCancel(event);
      return;
    }
    if (!mChromeHitTestCacheForTouch) {
      TransformTouchEvent(event);
    }
    DUMP_TOUCH_IDS("DOM(4)", event);
    mWidget->DispatchEvent(event, status);
  }
}

void
MetroInput::DispatchTouchCancel(WidgetTouchEvent* aEvent)
{
  MOZ_ASSERT(aEvent);
  // Send a touchcancel for each pointer id we have a corresponding start
  // for. Note we can't rely on mTouches here since touchends remove points
  // from it.
  WidgetTouchEvent touchEvent(true, NS_TOUCH_CANCEL, mWidget.Get());
  nsTArray< nsRefPtr<dom::Touch> >& touches = aEvent->touches;
  for (uint32_t i = 0; i < touches.Length(); ++i) {
    dom::Touch* touch = touches[i];
    if (!touch) {
      continue;
    }
    int32_t id = touch->Identifier();
    if (mCanceledIds.Contains(id)) {
      continue;
    }
    mCanceledIds.AppendElement(id);
    touchEvent.touches.AppendElement(touch);
  }
  if (!touchEvent.touches.Length()) {
    return;
  }
  if (mContentConsumingTouch) {
    DUMP_TOUCH_IDS("APZC(3)", &touchEvent);
    mWidget->ApzReceiveInputEvent(&touchEvent, nullptr);
  } else {
    DUMP_TOUCH_IDS("DOM(5)", &touchEvent);
    mWidget->DispatchEvent(&touchEvent, sThrowawayStatus);
  }
}

void
MetroInput::DispatchEventIgnoreStatus(WidgetGUIEvent *aEvent)
{
  mWidget->DispatchEvent(aEvent, sThrowawayStatus);
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

  // Unregistering from the gesture recognizer events probably isn't as
  // necessary since we're about to destroy the gesture recognizer, but
  // it can't hurt.
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
          | UI::Input::GestureSettings::GestureSettings_ManipulationTranslateY);

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

  mGestureRecognizer->add_ManipulationCompleted(
      WRL::Callback<ManipulationCompletedEventHandler>(
        this,
        &MetroInput::OnManipulationCompleted).Get(),
      &mTokenManipulationCompleted);
}

} } }
