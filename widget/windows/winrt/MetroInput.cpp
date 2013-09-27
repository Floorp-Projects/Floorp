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
    touches->AppendElement(aData);
    aData->mChanged = false;
    return PL_DHASH_NEXT;
  }

  // Helper for making sure event ptrs get freed.
  class AutoDeleteEvent
  {
  public:
    AutoDeleteEvent(nsGUIEvent* aPtr) :
      mPtr(aPtr) {}
    ~AutoDeleteEvent() {
      if (mPtr) {
        delete mPtr;
      }
    }
    nsGUIEvent* mPtr;
  };
}

namespace mozilla {
namespace widget {
namespace winrt {

MetroInput::MetroInput(MetroWidget* aWidget,
                       UI::Core::ICoreWindow* aWindow)
              : mWidget(aWidget),
                mChromeHitTestCacheForTouch(false),
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
  mTokenManipulationStarted.value = 0;
  mTokenManipulationUpdated.value = 0;
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
  nsSimpleGestureEvent geckoEvent(true,
                                  NS_SIMPLE_GESTURE_EDGE_CANCELED,
                                  mWidget.Get(),
                                  0,
                                  0.0);
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(geckoEvent);
  geckoEvent.time = ::GetMessageTime();

  geckoEvent.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;

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

  nsMouseEvent* event =
    new nsMouseEvent(true,
                     NS_MOUSE_MOVE,
                     mWidget.Get(),
                     nsMouseEvent::eReal,
                     nsMouseEvent::eNormal);

  switch (pointerUpdateKind) {
    case UI::Input::PointerUpdateKind::PointerUpdateKind_LeftButtonPressed:
      // We don't bother setting mouseEvent.button because it is already
      // set to nsMouseEvent::buttonType::eLeftButton whose value is 0.
      event->message = NS_MOUSE_BUTTON_DOWN;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_MiddleButtonPressed:
      event->button = nsMouseEvent::buttonType::eMiddleButton;
      event->message = NS_MOUSE_BUTTON_DOWN;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_RightButtonPressed:
      event->button = nsMouseEvent::buttonType::eRightButton;
      event->message = NS_MOUSE_BUTTON_DOWN;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_LeftButtonReleased:
      // We don't bother setting mouseEvent.button because it is already
      // set to nsMouseEvent::buttonType::eLeftButton whose value is 0.
      event->message = NS_MOUSE_BUTTON_UP;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_MiddleButtonReleased:
      event->button = nsMouseEvent::buttonType::eMiddleButton;
      event->message = NS_MOUSE_BUTTON_UP;
      break;
    case UI::Input::PointerUpdateKind::PointerUpdateKind_RightButtonReleased:
      event->button = nsMouseEvent::buttonType::eRightButton;
      event->message = NS_MOUSE_BUTTON_UP;
      break;
  }
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

  WidgetTouchEvent* touchEvent =
    new WidgetTouchEvent(true, NS_TOUCH_START, mWidget.Get());

  if (mTouches.Count() == 1) {
    // If this is the first touchstart of a touch session reset some
    // tracking flags and dispatch the event with a custom callback
    // so we can check preventDefault result.
    mTouchStartDefaultPrevented = false;
    mTouchMoveDefaultPrevented = false;
    mIsFirstTouchMove = true;
    mCancelable = true;
    mTouchCancelSent = false;
    InitTouchEventTouchList(touchEvent);
    DispatchAsyncTouchEventWithCallback(touchEvent, &MetroInput::OnPointerPressedCallback);
  } else {
    InitTouchEventTouchList(touchEvent);
    DispatchAsyncTouchEventIgnoreStatus(touchEvent);
  }

  if (!mTouchStartDefaultPrevented) {
    mGestureRecognizer->ProcessDownEvent(currentPoint.Get());
  }
  return S_OK;
}

void
MetroInput::OnPointerPressedCallback()
{
  nsEventStatus status = DeliverNextQueuedTouchEvent();
  mTouchStartDefaultPrevented = (nsEventStatus_eConsumeNoDefault == status);
  if (mTouchStartDefaultPrevented) {
    // If content canceled the first touchstart don't generate any gesture based
    // input - clear the recognizer state without sending any events.
    mGestureRecognizer->CompleteGesture();
    // Let the apz know content wants to consume touch events.
    mWidget->ApzContentConsumingTouch();
  }
}

void
MetroInput::AddPointerMoveDataToRecognizer(UI::Core::IPointerEventArgs* aArgs)
{
  // Only feed move input to the recognizer if the first touchstart and
  // subsequent touchmove return results were not eConsumeNoDefault.
  if (!mTouchStartDefaultPrevented && !mTouchMoveDefaultPrevented) {
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

  // If the point hasn't moved, filter it out per the spec. Pres shell does
  // this as well, but we need to know when our first touchmove is going to
  // get delivered so we can check the result.
  if (!HasPointMoved(touch, currentPoint.Get())) {
    // The recognizer needs the intermediate data otherwise it acts flaky
    AddPointerMoveDataToRecognizer(aArgs);
    return S_OK;
  }

  // If we've accumulated a batch of pointer moves and we're now on a new batch
  // at a new position send the previous batch. (perf opt)
  if (!mIsFirstTouchMove && touch->mChanged) {
    WidgetTouchEvent* touchEvent =
      new WidgetTouchEvent(true, NS_TOUCH_MOVE, mWidget.Get());
    InitTouchEventTouchList(touchEvent);
    DispatchAsyncTouchEventIgnoreStatus(touchEvent);
  }

  touch = CreateDOMTouch(currentPoint.Get());
  touch->mChanged = true;
  // replacing old touch point in mTouches map
  mTouches.Put(pointerId, touch);

  WidgetTouchEvent* touchEvent =
    new WidgetTouchEvent(true, NS_TOUCH_MOVE, mWidget.Get());

  // If this is the first touch move of our session, we should check the result.
  // Note we may lose some touch move data here for the recognizer since we want
  // to wait until we have the result of the first touchmove dispatch. For gesture
  // based events this shouldn't break anything.
  if (mIsFirstTouchMove) {
    InitTouchEventTouchList(touchEvent);
    DispatchAsyncTouchEventWithCallback(touchEvent, &MetroInput::OnFirstPointerMoveCallback);
    mIsFirstTouchMove = false;
  }

  AddPointerMoveDataToRecognizer(aArgs);

  return S_OK;
}

void
MetroInput::OnFirstPointerMoveCallback()
{
  nsEventStatus status = DeliverNextQueuedTouchEvent();
  mCancelable = false;
  mTouchMoveDefaultPrevented = (nsEventStatus_eConsumeNoDefault == status);
  // Let the apz know whether content wants to consume touch events
  if (mTouchMoveDefaultPrevented) {
    mWidget->ApzContentConsumingTouch();
    // reset the recognizer
    mGestureRecognizer->CompleteGesture();
  } else if (!mTouchMoveDefaultPrevented && !mTouchStartDefaultPrevented) {
    mWidget->ApzContentIgnoringTouch();
  }
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

  // Purge any pending moves for this pointer
  if (touch->mChanged) {
    WidgetTouchEvent* touchEvent =
      new WidgetTouchEvent(true, NS_TOUCH_MOVE, mWidget.Get());
    InitTouchEventTouchList(touchEvent);
    DispatchAsyncTouchEventIgnoreStatus(touchEvent);
  }

  // Remove this touch point from our map. Eventually all touch points are
  // removed for this session since we receive released events for every
  // point. 
  mTouches.Remove(pointerId);

  // touchend events only have a single touch; the touch that has been removed
  WidgetTouchEvent* touchEvent =
    new WidgetTouchEvent(true, NS_TOUCH_END, mWidget.Get());
  touchEvent->touches.AppendElement(CreateDOMTouch(currentPoint.Get()));
  DispatchAsyncTouchEventIgnoreStatus(touchEvent);

  // If content didn't cancel the first touchstart feed touchend data to the
  // recognizer.
  if (!mTouchStartDefaultPrevented && !mTouchMoveDefaultPrevented) {
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
  nsMouseEvent hittest(true, NS_MOUSE_MOZHITTEST, mWidget.Get(), nsMouseEvent::eReal, nsMouseEvent::eNormal);
  hittest.refPoint = pt;
  nsEventStatus status;
  mWidget->DispatchEvent(&hittest, status);
  return (status == nsEventStatus_eConsumeNoDefault);
}

void
MetroInput::TransformRefPoint(const Foundation::Point& aPosition, LayoutDeviceIntPoint& aRefPointOut)
{
  // If this event is destined for content we need to transform our ref point through
  // the apz so that zoom can be accounted for.
  LayoutDeviceIntPoint pt = LayoutDeviceIntPoint::FromUntyped(MetroUtils::LogToPhys(aPosition));
  aRefPointOut = pt;
  // This is currently a general contained rect hit test, it may produce a false positive for
  // overlay chrome elements.
  bool apzIntersect = mWidget->HitTestAPZC(mozilla::ScreenPoint(pt.x, pt.y));
  if (apzIntersect && HitTestChrome(pt)) {
    return;
  }
  nsMouseEvent event(true,
                     NS_MOUSE_MOVE,
                     mWidget.Get(),
                     nsMouseEvent::eReal,
                     nsMouseEvent::eNormal);
  event.refPoint = aRefPointOut;
  mWidget->ApzReceiveInputEvent(&event);
  aRefPointOut = event.refPoint;
}

void
MetroInput::InitGeckoMouseEventFromPointerPoint(
                                  nsMouseEvent* aEvent,
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
    nsMouseEvent* event = new nsMouseEvent(true,
                                           NS_MOUSE_ENTER,
                                           mWidget.Get(),
                                           nsMouseEvent::eReal,
                                           nsMouseEvent::eNormal);
    InitGeckoMouseEventFromPointerPoint(event, currentPoint.Get());
    DispatchAsyncEventIgnoreStatus(event);
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
    nsMouseEvent* event = new nsMouseEvent(true,
                                           NS_MOUSE_EXIT,
                                           mWidget.Get(),
                                           nsMouseEvent::eReal,
                                           nsMouseEvent::eNormal);
    InitGeckoMouseEventFromPointerPoint(event, currentPoint.Get());
    DispatchAsyncEventIgnoreStatus(event);
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
  nsSimpleGestureEvent* magEvent =
    new nsSimpleGestureEvent(true, aMagEventType, mWidget.Get(), 0, 0.0);

  magEvent->delta = aDelta.Expansion;
  magEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  magEvent->refPoint = LayoutDeviceIntPoint::FromUntyped(MetroUtils::LogToPhys(aPosition));
  DispatchAsyncEventIgnoreStatus(magEvent);

  // Send a gecko event indicating the rotation since the last update.
  nsSimpleGestureEvent* rotEvent =
    new nsSimpleGestureEvent(true, aRotEventType, mWidget.Get(), 0, 0.0);

  rotEvent->delta = aDelta.Rotation;
  rotEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  rotEvent->refPoint = LayoutDeviceIntPoint::FromUntyped(MetroUtils::LogToPhys(aPosition));
  if (rotEvent->delta >= 0) {
    rotEvent->direction = nsIDOMSimpleGestureEvent::ROTATION_COUNTERCLOCKWISE;
  } else {
    rotEvent->direction = nsIDOMSimpleGestureEvent::ROTATION_CLOCKWISE;
  }
  DispatchAsyncEventIgnoreStatus(rotEvent);
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
    nsSimpleGestureEvent* swipeEvent =
      new nsSimpleGestureEvent(true, NS_SIMPLE_GESTURE_SWIPE,
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
    nsSimpleGestureEvent* swipeEvent =
      new nsSimpleGestureEvent(true, NS_SIMPLE_GESTURE_SWIPE,
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

  // For mouse and pen input, we send mousedown/mouseup/mousemove
  // events as soon as we detect the input event.  For touch input, a set of
  // mousedown/mouseup events will be sent only once a tap has been detected.
  if (deviceType != Devices::Input::PointerDeviceType::PointerDeviceType_Touch) {
    return S_OK;
  }

  Foundation::Point position;
  aArgs->get_Position(&position);
  HandleSingleTap(position);
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
MetroInput::HandleSingleTap(const Foundation::Point& aPoint)
{
#ifdef DEBUG_INPUT
  LogFunction();
#endif
  
  LayoutDeviceIntPoint refPoint;
  TransformRefPoint(aPoint, refPoint);

  // send mousemove
  nsMouseEvent* mouseEvent = new nsMouseEvent(true,
                                              NS_MOUSE_MOVE,
                                              mWidget.Get(),
                                              nsMouseEvent::eReal,
                                              nsMouseEvent::eNormal);
  mouseEvent->refPoint = refPoint;
  mouseEvent->clickCount = 1;
  mouseEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  DispatchAsyncEventIgnoreStatus(mouseEvent);

  // Send the mousedown
  mouseEvent = new nsMouseEvent(true,
                                NS_MOUSE_BUTTON_DOWN,
                                mWidget.Get(),
                                nsMouseEvent::eReal,
                                nsMouseEvent::eNormal);
  mouseEvent->refPoint = refPoint;
  mouseEvent->clickCount = 1;
  mouseEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  mouseEvent->button = nsMouseEvent::buttonType::eLeftButton;
  DispatchAsyncEventIgnoreStatus(mouseEvent);

  mouseEvent = new nsMouseEvent(true,
                                NS_MOUSE_BUTTON_UP,
                                mWidget.Get(),
                                nsMouseEvent::eReal,
                                nsMouseEvent::eNormal);
  mouseEvent->refPoint = refPoint;
  mouseEvent->clickCount = 1;
  mouseEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  mouseEvent->button = nsMouseEvent::buttonType::eLeftButton;
  DispatchAsyncEventIgnoreStatus(mouseEvent);

  // Send one more mousemove to avoid getting a hover state.
  // In the Metro environment for any application, a tap does not imply a
  // mouse cursor move.  In desktop environment for any application a tap
  // does imply a cursor move.
  POINT point;
  if (GetCursorPos(&point)) {
    ScreenToClient((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW), &point);
    mouseEvent = new nsMouseEvent(true,
                                  NS_MOUSE_MOVE,
                                  mWidget.Get(),
                                  nsMouseEvent::eReal,
                                  nsMouseEvent::eNormal);
    mouseEvent->refPoint = LayoutDeviceIntPoint(point.x, point.y);
    mouseEvent->clickCount = 1;
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

  nsMouseEvent* contextEvent = new nsMouseEvent(true,
                                                NS_CONTEXTMENU,
                                                mWidget.Get(),
                                                nsMouseEvent::eReal,
                                                nsMouseEvent::eNormal);
  contextEvent->refPoint = refPoint;
  contextEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  DispatchAsyncEventIgnoreStatus(contextEvent);
}

/**
 * Implementation Details
 */
nsEventStatus MetroInput::sThrowawayStatus;

void
MetroInput::DispatchAsyncEventIgnoreStatus(nsInputEvent* aEvent)
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
  nsGUIEvent* event = static_cast<nsGUIEvent*>(mInputEventQueue.PopFront());
  MOZ_ASSERT(event);
  DispatchEventIgnoreStatus(event);
  delete event;
}

void
MetroInput::DispatchAsyncTouchEventIgnoreStatus(WidgetTouchEvent* aEvent)
{
  aEvent->time = ::GetMessageTime();
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(*aEvent);
  mInputEventQueue.Push(aEvent);
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &MetroInput::DeliverNextQueuedTouchEvent);
  NS_DispatchToCurrentThread(runnable);
}

nsEventStatus
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
   * 1) delivering first touchpoint touchstart or its first touchmove
   *  Our callers (OnFirstPointerMoveCallback, OnPointerPressedCallback) will
   *  check our result and set mTouchStartDefaultPrevented or
   *  mTouchMoveDefaultPrevented appropriately. Deliver touch events to the apz
   *  (ignoring return result) and to content and return the content event
   *  status result to our caller.
   * 2) mTouchStartDefaultPrevented or mTouchMoveDefaultPrevented are true
   *  Deliver touch to content after transforming through the apz. Our callers
   *  handle calling cancel for the touch sequence on the apz.
   * 3) mTouchStartDefaultPrevented and mTouchMoveDefaultPrevented are false
   *  Deliver events to the apz. If the apz returns eConsumeNoDefault dispatch
   *  a touchcancel to content and do not deliver any additional events there.
   *  (If the apz is doing something with the events we can save ourselves
   *  the overhead of delivering dom events.)
   */

  // Test for chrome vs. content target. To do this we only use the first touch
  // point since that will be the input batch target. Cache this for touch events
  // since HitTestChrome has to send a dom event.
  if (event->message == NS_TOUCH_START) {
    nsRefPtr<Touch> touch = event->touches[0];
    LayoutDeviceIntPoint pt = LayoutDeviceIntPoint::FromUntyped(touch->mRefPoint);
    bool apzIntersect = mWidget->HitTestAPZC(mozilla::ScreenPoint(pt.x, pt.y));
    mChromeHitTestCacheForTouch = (apzIntersect && HitTestChrome(pt));
  }

  // Check if content called preventDefault on touchstart or first touchmove. If so
  // and the event is destined for chrome, send the event. If destined for content,
  // translate coordinates through the apz then send.
  if (mTouchStartDefaultPrevented || mTouchMoveDefaultPrevented) {
    if (!mChromeHitTestCacheForTouch) {
      // ContentReceivedTouch has already been called so this shouldn't cause
      // the apz to react. We still need to transform our coordinates though.
      mWidget->ApzReceiveInputEvent(event);
    }
    mWidget->DispatchEvent(event, status);
    return status;
  }

  // Forward event data to apz. If the apz consumes the event, don't forward to
  // content if this is not a cancelable event.
  WidgetTouchEvent transformedEvent(*event);
  status = mWidget->ApzReceiveInputEvent(event, &transformedEvent);
  if (!mCancelable && status == nsEventStatus_eConsumeNoDefault) {
    if (!mTouchCancelSent) {
      mTouchCancelSent = true;
      DispatchTouchCancel();
    }
    return status;
  }

  // Deliver event. If this is destined for chrome, use the untransformed event
  // data, if it's destined for content, use the transformed event.
  mWidget->DispatchEvent(!mChromeHitTestCacheForTouch ? &transformedEvent : event, status);
  return status;
}

void
MetroInput::DispatchTouchCancel()
{
  LogFunction();
  // From the spec: The touch point or points that were removed must be
  // included in the changedTouches attribute of the TouchEvent, and must
  // not be included in the touches and targetTouches attributes. 
  // (We are 'removing' all touch points that have been sent to content
  // thus far.)
  WidgetTouchEvent touchEvent(true, NS_TOUCH_CANCEL, mWidget.Get());
  InitTouchEventTouchList(&touchEvent);
  mWidget->DispatchEvent(&touchEvent, sThrowawayStatus);
}

void
MetroInput::DispatchAsyncTouchEventWithCallback(WidgetTouchEvent* aEvent,
                                                void (MetroInput::*Callback)())
{
  aEvent->time = ::GetMessageTime();
  mModifierKeyState.Update();
  mModifierKeyState.InitInputEvent(*aEvent);
  mInputEventQueue.Push(aEvent);
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, Callback);
  NS_DispatchToCurrentThread(runnable);
}

void
MetroInput::DispatchEventIgnoreStatus(nsGUIEvent *aEvent) {
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
