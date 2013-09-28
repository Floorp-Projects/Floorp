/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

// Moz headers (alphabetical)
#include "keyboardlayout.h"   // mModifierKeyState
#include "nsBaseHashtable.h"  // mTouches
#include "nsHashKeys.h"       // type of key for mTouches
#include "mozwrlbase.h"
#include "nsDeque.h"
#include "mozilla/EventForwards.h"

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

class MetroInput : public Microsoft::WRL::RuntimeClass<IInspectable>
{
  InspectableClass(L"MetroInput", BaseTrust);

private:
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

  // These events are raised by our GestureRecognizer in response to input
  // events that we forward to it.  The ManipulationStarted,
  // ManipulationUpdated, and ManipulationEnded events are sent during
  // complex input gestures including pinch, swipe, and rotate.  Note that
  // all three gestures can occur simultaneously.
  HRESULT OnManipulationStarted(IGestureRecognizer* aSender,
                                IManipulationStartedEventArgs* aArgs);
  HRESULT OnManipulationUpdated(IGestureRecognizer* aSender,
                                IManipulationUpdatedEventArgs* aArgs);
  HRESULT OnManipulationCompleted(IGestureRecognizer* aSender,
                                  IManipulationCompletedEventArgs* aArgs);
  HRESULT OnTapped(IGestureRecognizer* aSender, ITappedEventArgs* aArgs);
  HRESULT OnRightTapped(IGestureRecognizer* aSender,
                        IRightTappedEventArgs* aArgs);

  void HandleSingleTap(const Point& aPoint);
  void HandleLongTap(const Point& aPoint);

private:
  Microsoft::WRL::ComPtr<ICoreWindow> mWindow;
  Microsoft::WRL::ComPtr<MetroWidget> mWidget;
  Microsoft::WRL::ComPtr<IGestureRecognizer> mGestureRecognizer;

  ModifierKeyState mModifierKeyState;

  // Initialization/Uninitialization helpers
  void RegisterInputEvents();
  void UnregisterInputEvents();

  // Hit testing for chrome content
  bool mChromeHitTestCacheForTouch;
  bool HitTestChrome(const LayoutDeviceIntPoint& pt);

  // Event processing helpers.  See function definitions for more info.
  void TransformRefPoint(const Point& aPosition,
                         LayoutDeviceIntPoint& aRefPointOut);
  void OnPointerNonTouch(IPointerPoint* aPoint);
  void AddPointerMoveDataToRecognizer(IPointerEventArgs* aArgs);
  void InitGeckoMouseEventFromPointerPoint(nsMouseEvent* aEvent,
                                           IPointerPoint* aPoint);
  void ProcessManipulationDelta(ManipulationDelta const& aDelta,
                                Point const& aPosition,
                                uint32_t aMagEventType,
                                uint32_t aRotEventType);

  // The W3C spec states that "whether preventDefault has been called" should
  // be tracked on a per-touchpoint basis, but it also states that touchstart
  // and touchmove events can contain multiple changed points.  At the time of
  // this writing, W3C touch events are in the process of being abandoned in
  // favor of W3C pointer events, so it is unlikely that this ambiguity will
  // be resolved.  Additionally, nsPresShell tracks "whether preventDefault
  // has been called" on a per-touch-session basis.  We will follow a similar
  // approach here.
  //
  // Specifically:
  //   If preventDefault is called on the FIRST touchstart event of a touch
  //   session, then no default actions associated with any touchstart,
  //   touchmove, or touchend events will be taken.  This means that no
  //   mousedowns, mousemoves, mouseups, clicks, swipes, rotations,
  //   magnifications, etc will be dispatched during this touch session;
  //   only touchstart, touchmove, and touchend.
  //
  //   If preventDefault is called on the FIRST touchmove event of a touch
  //   session, then no default actions associated with the _touchmove_ events
  //   will be dispatched.  However, it is still possible that additional
  //   events will be generated based on the touchstart and touchend events.
  //   For example, a set of mousemove, mousedown, and mouseup events might
  //   be sent if a tap is detected.
  bool mTouchStartDefaultPrevented;
  bool mTouchMoveDefaultPrevented;
  bool mIsFirstTouchMove;
  bool mCancelable;
  bool mTouchCancelSent;

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
  EventRegistrationToken mTokenManipulationStarted;
  EventRegistrationToken mTokenManipulationUpdated;
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
  void DispatchAsyncEventIgnoreStatus(nsInputEvent* aEvent);
  void DispatchAsyncTouchEventIgnoreStatus(WidgetTouchEvent* aEvent);
  void DispatchAsyncTouchEventWithCallback(WidgetTouchEvent* aEvent,
                                           void (MetroInput::*Callback)());

  // Async event callbacks
  void DeliverNextQueuedEventIgnoreStatus();
  nsEventStatus DeliverNextQueuedTouchEvent();

  // Misc. specialty async callbacks
  void OnPointerPressedCallback();
  void OnFirstPointerMoveCallback();

  // Sync event dispatching
  void DispatchEventIgnoreStatus(nsGUIEvent *aEvent);
  void DispatchTouchCancel();

  nsDeque mInputEventQueue;
  static nsEventStatus sThrowawayStatus;
};

} } }
