/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

// Moz headers (alphabetical)
#include "APZController.h"
#include "keyboardlayout.h"   // mModifierKeyState
#include "nsBaseHashtable.h"  // mTouches
#include "nsHashKeys.h"       // type of key for mTouches
#include "mozwrlbase.h"
#include "nsDeque.h"
#include "mozilla/EventForwards.h"
#include "mozilla/layers/APZCTreeManager.h"

// System headers (alphabetical)
#include <EventToken.h>     // EventRegistrationToken
#include <stdint.h>         // uint32_t
#include <wrl\client.h>     // Microsoft::WRL::ComPtr class
#include <wrl\implements.h> // Microsoft::WRL::InspectableClass macro

// Moz forward declarations
class MetroWidget;
struct nsIntPoint;

namespace mozilla {
namespace dom {
class Touch;
}
}

// Windows forward declarations
namespace ABI {
  namespace Windows {
    namespace Devices {
      namespace Input {
        enum PointerDeviceType;
      }
    };
    namespace Foundation {
      struct Point;
    };
    namespace UI {
      namespace Core {
        struct ICoreWindow;
        struct IAcceleratorKeyEventArgs;
        struct IKeyEventArgs;
        struct IPointerEventArgs;
      };
      namespace Input {
        struct IEdgeGesture;
        struct IEdgeGestureEventArgs;
        struct IGestureRecognizer;
        struct IManipulationCompletedEventArgs;
        struct IManipulationStartedEventArgs;
        struct IManipulationUpdatedEventArgs;
        struct IPointerPoint;
        struct IRightTappedEventArgs;
        struct ITappedEventArgs;
        struct ManipulationDelta;
      };
    };
  };
};

namespace mozilla {
namespace widget {
namespace winrt {

class MetroInput : public Microsoft::WRL::RuntimeClass<IInspectable>,
                   public APZPendingResponseFlusher
{
  InspectableClass(L"MetroInput", BaseTrust);

private:
  typedef mozilla::layers::AllowedTouchBehavior AllowedTouchBehavior;
  typedef uint32_t TouchBehaviorFlags;

  // Devices
  typedef ABI::Windows::Devices::Input::PointerDeviceType PointerDeviceType;

  // Foundation
  typedef ABI::Windows::Foundation::Point Point;

  // UI::Core
  typedef ABI::Windows::UI::Core::ICoreWindow ICoreWindow;
  typedef ABI::Windows::UI::Core::IAcceleratorKeyEventArgs \
                                  IAcceleratorKeyEventArgs;
  typedef ABI::Windows::UI::Core::IKeyEventArgs IKeyEventArgs;
  typedef ABI::Windows::UI::Core::IPointerEventArgs IPointerEventArgs;

  // UI::Input
  typedef ABI::Windows::UI::Input::IEdgeGesture IEdgeGesture;
  typedef ABI::Windows::UI::Input::IEdgeGestureEventArgs IEdgeGestureEventArgs;
  typedef ABI::Windows::UI::Input::IGestureRecognizer IGestureRecognizer;
  typedef ABI::Windows::UI::Input::IManipulationCompletedEventArgs \
                                   IManipulationCompletedEventArgs;
  typedef ABI::Windows::UI::Input::IManipulationStartedEventArgs \
                                   IManipulationStartedEventArgs;
  typedef ABI::Windows::UI::Input::IManipulationUpdatedEventArgs \
                                   IManipulationUpdatedEventArgs;
  typedef ABI::Windows::UI::Input::IPointerPoint IPointerPoint;
  typedef ABI::Windows::UI::Input::IRightTappedEventArgs IRightTappedEventArgs;
  typedef ABI::Windows::UI::Input::ITappedEventArgs ITappedEventArgs;
  typedef ABI::Windows::UI::Input::ManipulationDelta ManipulationDelta;

  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

public:
  MetroInput(MetroWidget* aWidget,
             ICoreWindow* aWindow);
  virtual ~MetroInput();

  // These input events are received from our window. These are basic
  // pointer and keyboard press events. MetroInput responds to them
  // by sending gecko events and forwarding these input events to its
  // GestureRecognizer to be processed into more complex input events
  // (tap, rightTap, rotate, etc)
  HRESULT OnPointerPressed(ICoreWindow* aSender,
                           IPointerEventArgs* aArgs);
  HRESULT OnPointerReleased(ICoreWindow* aSender,
                            IPointerEventArgs* aArgs);
  HRESULT OnPointerMoved(ICoreWindow* aSender,
                         IPointerEventArgs* aArgs);
  HRESULT OnPointerEntered(ICoreWindow* aSender,
                           IPointerEventArgs* aArgs);
  HRESULT OnPointerExited(ICoreWindow* aSender,
                          IPointerEventArgs* aArgs);

  // The Edge gesture event is special.  It does not come from our window
  // or from our GestureRecognizer.
  HRESULT OnEdgeGestureStarted(IEdgeGesture* aSender,
                               IEdgeGestureEventArgs* aArgs);
  HRESULT OnEdgeGestureCanceled(IEdgeGesture* aSender,
                                IEdgeGestureEventArgs* aArgs);
  HRESULT OnEdgeGestureCompleted(IEdgeGesture* aSender,
                                 IEdgeGestureEventArgs* aArgs);

  // Swipe gesture callback from the GestureRecognizer.
  HRESULT OnManipulationCompleted(IGestureRecognizer* aSender,
                                  IManipulationCompletedEventArgs* aArgs);

  // Tap gesture callback from the GestureRecognizer.
  HRESULT OnTapped(IGestureRecognizer* aSender, ITappedEventArgs* aArgs);
  HRESULT OnRightTapped(IGestureRecognizer* aSender,
                        IRightTappedEventArgs* aArgs);

  void HandleTap(const Point& aPoint, unsigned int aTapCount);
  void HandleLongTap(const Point& aPoint);

  // The APZPendingResponseFlusher implementation
  void FlushPendingContentResponse();

  static bool IsInputModeImprecise();

private:
  Microsoft::WRL::ComPtr<ICoreWindow> mWindow;
  Microsoft::WRL::ComPtr<MetroWidget> mWidget;
  Microsoft::WRL::ComPtr<IGestureRecognizer> mGestureRecognizer;

  ModifierKeyState mModifierKeyState;

  // Tracking input level
  enum InputPrecisionLevel {
    LEVEL_PRECISE,
    LEVEL_IMPRECISE
  };
  static InputPrecisionLevel sCurrentInputLevel;
  void UpdateInputLevel(InputPrecisionLevel aInputLevel);

  // Initialization/Uninitialization helpers
  void RegisterInputEvents();
  void UnregisterInputEvents();

  // Hit testing for apz content
  bool mNonApzTargetForTouch;
  bool HitTestChrome(const LayoutDeviceIntPoint& pt);

  // Event processing helpers.  See function definitions for more info.
  bool TransformRefPoint(const Point& aPosition,
                         LayoutDeviceIntPoint& aRefPointOut);
  void TransformTouchEvent(WidgetTouchEvent* aEvent);
  void OnPointerNonTouch(IPointerPoint* aPoint);
  void AddPointerMoveDataToRecognizer(IPointerEventArgs* aArgs);
  void InitGeckoMouseEventFromPointerPoint(WidgetMouseEvent* aEvent,
                                           IPointerPoint* aPoint);
  void ProcessManipulationDelta(ManipulationDelta const& aDelta,
                                Point const& aPosition,
                                uint32_t aMagEventType,
                                uint32_t aRotEventType);
  uint16_t ProcessInputTypeForGesture(IEdgeGestureEventArgs* aArgs);
  bool ShouldDeliverInputToRecognizer();

  // Returns array of allowed touch behaviors  for touch points of given TouchEvent.
  // Note: event argument should be transformed via apzc before supplying to this method.
  void GetAllowedTouchBehavior(WidgetTouchEvent* aTransformedEvent, nsTArray<TouchBehaviorFlags>& aOutBehaviors);

  // First, read the comment in gfx/layers/apz/src/TouchBlockState.h.
  // The following booleans track the following pieces of state:
  // mApzConsumingTouch - if events from the current touch block should be
  //    sent to the APZ. This is true unless the touchstart hit no APZ
  //    instances, in which case we don't need to send the rest of the touch
  //    block to the APZ code.
  // mCancelable - if we have not yet notified the APZ code about the prevent-
  //    default status of the current touch block. This is flipped from true
  //    to false when this notification happens (or would have happened, in
  //    the case that mApzConsumingTouch is false).
  // mRecognizerWantsEvents - If the gesture recognizer should be receiving
  //    events. This is normally true, but will be set to false if the APZ
  //    decides the touch block should be thrown away entirely, or if content
  //    consumes the touch block.
  //    XXX There is a hazard with mRecognizerWantsEvents because it is accessed
  //    both in the sync and async portions of the code.
  bool mApzConsumingTouch;
  bool mCancelable;
  bool mRecognizerWantsEvents;

  // In the old Win32 way of doing things, we would receive a WM_TOUCH event
  // that told us the state of every touchpoint on the touch surface.  If
  // multiple touchpoints had moved since the last update we would learn
  // about all their movement simultaneously.
  //
  // In the new WinRT way of doing things, we receive a separate
  // PointerPressed/PointerMoved/PointerReleased event for each touchpoint
  // that has changed.
  //
  // When we learn of touch input, we dispatch gecko events in response.
  // With the new WinRT way of doing things, we would end up sending many
  // more gecko events than we would using the Win32 mechanism.  E.g.,
  // for 5 active touchpoints, we would be sending 5 times as many gecko
  // events.  This caused performance to visibly degrade on modestly-powered
  // machines.  In response, we no longer send touch events immediately
  // upon receiving PointerPressed or PointerMoved.  Instead, we store
  // the updated touchpoint info and record the fact that the touchpoint
  // has changed.  If ever we try to update a touchpoint has already
  // changed, we dispatch a touch event containing all the changed touches.
  void InitTouchEventTouchList(WidgetTouchEvent* aEvent);
  nsBaseHashtable<nsUint32HashKey,
                  nsRefPtr<mozilla::dom::Touch>,
                  nsRefPtr<mozilla::dom::Touch> > mTouches;

  // These registration tokens are set when we register ourselves to receive
  // events from our window.  We must hold on to them for the entire duration
  // that we want to receive these events.  When we are done, we must
  // unregister ourself with the window using these tokens.
  EventRegistrationToken mTokenPointerPressed;
  EventRegistrationToken mTokenPointerReleased;
  EventRegistrationToken mTokenPointerMoved;
  EventRegistrationToken mTokenPointerEntered;
  EventRegistrationToken mTokenPointerExited;

  // When we register ourselves to handle edge gestures, we receive a
  // token. To we unregister ourselves, we must use the token we received.
  EventRegistrationToken mTokenEdgeStarted;
  EventRegistrationToken mTokenEdgeCanceled;
  EventRegistrationToken mTokenEdgeCompleted;

  // These registration tokens are set when we register ourselves to receive
  // events from our GestureRecognizer.  It's probably not a huge deal if we
  // don't unregister ourselves with our GestureRecognizer before destroying
  // the GestureRecognizer, but it can't hurt.
  EventRegistrationToken mTokenManipulationCompleted;
  EventRegistrationToken mTokenTapped;
  EventRegistrationToken mTokenRightTapped;

  // Due to a limitation added in 8.1 the ui thread can't re-enter the main
  // native event dispatcher in MetroAppShell. So all events delivered to us
  // on the ui thread via a native event dispatch call get bounced through
  // the gecko thread event queue using runnables. Most events can be sent
  // async without the need to see the status result. Those that do have
  // specialty callbacks. Note any event that arrives to us on the ui thread
  // that originates from another thread is safe to send sync.

  // Async event dispatching
  void DispatchAsyncEventIgnoreStatus(WidgetInputEvent* aEvent);
  void DispatchAsyncTouchEvent(WidgetTouchEvent* aEvent);

  // Async event callbacks
  void DeliverNextQueuedEventIgnoreStatus();
  void DeliverNextQueuedTouchEvent();

  void HandleTouchStartEvent(WidgetTouchEvent* aEvent);
  void HandleFirstTouchMoveEvent(WidgetTouchEvent* aEvent);
  void SendPendingResponseToApz();
  void CancelGesture();

  // Sync event dispatching
  void DispatchEventIgnoreStatus(WidgetGUIEvent* aEvent);

  nsDeque mInputEventQueue;
  mozilla::layers::ScrollableLayerGuid mTargetAPZCGuid;
  static nsEventStatus sThrowawayStatus;
};

} } }
