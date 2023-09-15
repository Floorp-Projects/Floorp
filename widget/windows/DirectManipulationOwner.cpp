/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectManipulationOwner.h"
#include "nsWindow.h"
#include "WinModifierKeyState.h"
#include "InputData.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/SwipeTracker.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/VsyncDispatcher.h"

#include "directmanipulation.h"

namespace mozilla {
namespace widget {

class DManipEventHandler : public IDirectManipulationViewportEventHandler,
                           public IDirectManipulationInteractionEventHandler {
 public:
  typedef mozilla::LayoutDeviceIntRect LayoutDeviceIntRect;

  friend class DirectManipulationOwner;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DManipEventHandler)

  STDMETHODIMP QueryInterface(REFIID, void**) override;

  friend class DirectManipulationOwner;

  explicit DManipEventHandler(nsWindow* aWindow,
                              DirectManipulationOwner* aOwner,
                              const LayoutDeviceIntRect& aBounds);

  HRESULT STDMETHODCALLTYPE OnViewportStatusChanged(
      IDirectManipulationViewport* viewport, DIRECTMANIPULATION_STATUS current,
      DIRECTMANIPULATION_STATUS previous) override;

  HRESULT STDMETHODCALLTYPE
  OnViewportUpdated(IDirectManipulationViewport* viewport) override;

  HRESULT STDMETHODCALLTYPE
  OnContentUpdated(IDirectManipulationViewport* viewport,
                   IDirectManipulationContent* content) override;

  HRESULT STDMETHODCALLTYPE
  OnInteraction(IDirectManipulationViewport2* viewport,
                DIRECTMANIPULATION_INTERACTION_TYPE interaction) override;

  void Update();

  class VObserver final : public mozilla::VsyncObserver {
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DManipEventHandler::VObserver,
                                          override)

   public:
    void NotifyVsync(const mozilla::VsyncEvent& aVsync) override {
      if (mOwner) {
        mOwner->Update();
      }
    }
    explicit VObserver(DManipEventHandler* aOwner) : mOwner(aOwner) {}

    void ClearOwner() { mOwner = nullptr; }

   private:
    virtual ~VObserver() {}
    DManipEventHandler* mOwner;
  };

  enum class State { eNone, ePanning, eInertia, ePinching };
  void TransitionToState(State aNewState);

  enum class Phase { eStart, eMiddle, eEnd };
  // Return value indicates if we sent an event or not and hence if we should
  // update mLastScale. (We only want to send pinch events if the computed
  // deltaY for the corresponding WidgetWheelEvent would be non-zero.)
  bool SendPinch(Phase aPhase, float aScale);
  void SendPan(Phase aPhase, float x, float y, bool aIsInertia);
  static void SendPanCommon(nsWindow* aWindow, Phase aPhase,
                            ScreenPoint aPosition, double aDeltaX,
                            double aDeltaY, Modifiers aMods, bool aIsInertia);

  static void SynthesizeNativeTouchpadPan(
      nsWindow* aWindow, nsIWidget::TouchpadGesturePhase aEventPhase,
      LayoutDeviceIntPoint aPoint, double aDeltaX, double aDeltaY,
      int32_t aModifierFlags);

 private:
  virtual ~DManipEventHandler() = default;

  nsWindow* mWindow;
  DirectManipulationOwner* mOwner;
  RefPtr<VObserver> mObserver;
  float mLastScale;
  float mLastXOffset;
  float mLastYOffset;
  LayoutDeviceIntRect mBounds;
  bool mShouldSendPanStart;
  bool mShouldSendPinchStart;
  State mState = State::eNone;
};

DManipEventHandler::DManipEventHandler(nsWindow* aWindow,
                                       DirectManipulationOwner* aOwner,
                                       const LayoutDeviceIntRect& aBounds)
    : mWindow(aWindow),
      mOwner(aOwner),
      mLastScale(1.f),
      mLastXOffset(0.f),
      mLastYOffset(0.f),
      mBounds(aBounds),
      mShouldSendPanStart(false),
      mShouldSendPinchStart(false) {}

STDMETHODIMP
DManipEventHandler::QueryInterface(REFIID iid, void** ppv) {
  const IID IID_IDirectManipulationViewportEventHandler =
      __uuidof(IDirectManipulationViewportEventHandler);
  const IID IID_IDirectManipulationInteractionEventHandler =
      __uuidof(IDirectManipulationInteractionEventHandler);

  if ((IID_IUnknown == iid) ||
      (IID_IDirectManipulationViewportEventHandler == iid)) {
    *ppv = static_cast<IDirectManipulationViewportEventHandler*>(this);
    AddRef();
    return S_OK;
  }
  if (IID_IDirectManipulationInteractionEventHandler == iid) {
    *ppv = static_cast<IDirectManipulationInteractionEventHandler*>(this);
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

HRESULT
DManipEventHandler::OnViewportStatusChanged(
    IDirectManipulationViewport* viewport, DIRECTMANIPULATION_STATUS current,
    DIRECTMANIPULATION_STATUS previous) {
  if (current == previous) {
    return S_OK;
  }

  if (current == DIRECTMANIPULATION_INERTIA) {
    if (previous != DIRECTMANIPULATION_RUNNING || mState != State::ePanning) {
      // xxx transition to none?
      return S_OK;
    }

    TransitionToState(State::eInertia);
  }

  if (current == DIRECTMANIPULATION_RUNNING) {
    // INERTIA -> RUNNING, should start a new sequence.
    if (previous == DIRECTMANIPULATION_INERTIA) {
      TransitionToState(State::eNone);
    }
  }

  if (current != DIRECTMANIPULATION_ENABLED &&
      current != DIRECTMANIPULATION_READY) {
    return S_OK;
  }

  // A session has ended, reset the transform.
  if (mLastScale != 1.f || mLastXOffset != 0.f || mLastYOffset != 0.f) {
    HRESULT hr =
        viewport->ZoomToRect(0, 0, mBounds.width, mBounds.height, false);
    if (!SUCCEEDED(hr)) {
      NS_WARNING("ZoomToRect failed");
    }
  }
  mLastScale = 1.f;
  mLastXOffset = 0.f;
  mLastYOffset = 0.f;

  TransitionToState(State::eNone);

  return S_OK;
}

HRESULT
DManipEventHandler::OnViewportUpdated(IDirectManipulationViewport* viewport) {
  return S_OK;
}

void DManipEventHandler::TransitionToState(State aNewState) {
  if (mState == aNewState) {
    return;
  }

  State prevState = mState;
  mState = aNewState;

  // End the previous sequence.
  switch (prevState) {
    case State::ePanning: {
      // ePanning -> *: PanEnd
      SendPan(Phase::eEnd, 0.f, 0.f, false);
      break;
    }
    case State::eInertia: {
      // eInertia -> *: MomentumEnd
      SendPan(Phase::eEnd, 0.f, 0.f, true);
      break;
    }
    case State::ePinching: {
      MOZ_ASSERT(aNewState == State::eNone);
      // ePinching -> eNone: PinchEnd. ePinching should only transition to
      // eNone.
      // Only send a pinch end if we sent a pinch start.
      if (!mShouldSendPinchStart) {
        SendPinch(Phase::eEnd, 0.f);
      }
      mShouldSendPinchStart = false;
      break;
    }
    case State::eNone: {
      // eNone -> *: no cleanup is needed.
      break;
    }
    default:
      MOZ_ASSERT(false);
  }

  // Start the new sequence.
  switch (aNewState) {
    case State::ePanning: {
      // eInertia, eNone -> ePanning: PanStart.
      // We're being called from OnContentUpdated, it has the coords we need to
      // pass to SendPan(Phase::eStart), so set mShouldSendPanStart and when we
      // return OnContentUpdated will check it and call SendPan(Phase::eStart).
      mShouldSendPanStart = true;
      break;
    }
    case State::eInertia: {
      // Only ePanning can transition to eInertia.
      MOZ_ASSERT(prevState == State::ePanning);
      SendPan(Phase::eStart, 0.f, 0.f, true);
      break;
    }
    case State::ePinching: {
      // * -> ePinching: PinchStart.
      // Pinch gesture may begin with some scroll events.
      // We're being called from OnContentUpdated, it has the scale we need to
      // pass to SendPinch(Phase::eStart), so set mShouldSendPinchStart and when
      // we return OnContentUpdated will check it and call
      // SendPinch(Phase::eStart).
      mShouldSendPinchStart = true;
      break;
    }
    case State::eNone: {
      // * -> eNone: only cleanup is needed.
      break;
    }
    default:
      MOZ_ASSERT(false);
  }
}

HRESULT
DManipEventHandler::OnContentUpdated(IDirectManipulationViewport* viewport,
                                     IDirectManipulationContent* content) {
  float transform[6];
  HRESULT hr = content->GetContentTransform(transform, ARRAYSIZE(transform));
  if (!SUCCEEDED(hr)) {
    NS_WARNING("GetContentTransform failed");
    return S_OK;
  }

  float scale = transform[0];
  float xoffset = transform[4];
  float yoffset = transform[5];

  // Not different from last time.
  if (FuzzyEqualsMultiplicative(scale, mLastScale) && xoffset == mLastXOffset &&
      yoffset == mLastYOffset) {
    return S_OK;
  }

  // Consider this is a Scroll when scale factor equals 1.0.
  if (FuzzyEqualsMultiplicative(scale, 1.f)) {
    if (mState == State::eNone) {
      TransitionToState(State::ePanning);
    }
  } else {
    // Pinch gesture may begin with some scroll events.
    TransitionToState(State::ePinching);
  }

  if (mState == State::ePanning || mState == State::eInertia) {
    // Accumulate the offset (by not updating mLastX/YOffset) until we have at
    // least one pixel.
    float dx = std::abs(mLastXOffset - xoffset);
    float dy = std::abs(mLastYOffset - yoffset);
    if (dx < 1.f && dy < 1.f) {
      return S_OK;
    }
  }

  bool updateLastScale = true;
  if (mState == State::ePanning) {
    if (mShouldSendPanStart) {
      SendPan(Phase::eStart, mLastXOffset - xoffset, mLastYOffset - yoffset,
              false);
      mShouldSendPanStart = false;
    } else {
      SendPan(Phase::eMiddle, mLastXOffset - xoffset, mLastYOffset - yoffset,
              false);
    }
  } else if (mState == State::eInertia) {
    SendPan(Phase::eMiddle, mLastXOffset - xoffset, mLastYOffset - yoffset,
            true);
  } else if (mState == State::ePinching) {
    if (mShouldSendPinchStart) {
      updateLastScale = SendPinch(Phase::eStart, scale);
      // Only clear mShouldSendPinchStart if we actually sent the event
      // (updateLastScale tells us if we sent an event).
      if (updateLastScale) {
        mShouldSendPinchStart = false;
      }
    } else {
      updateLastScale = SendPinch(Phase::eMiddle, scale);
    }
  }

  if (updateLastScale) {
    mLastScale = scale;
  }
  mLastXOffset = xoffset;
  mLastYOffset = yoffset;

  return S_OK;
}

HRESULT
DManipEventHandler::OnInteraction(
    IDirectManipulationViewport2* viewport,
    DIRECTMANIPULATION_INTERACTION_TYPE interaction) {
  if (interaction == DIRECTMANIPULATION_INTERACTION_BEGIN) {
    if (!mObserver) {
      mObserver = new VObserver(this);
    }

    gfxWindowsPlatform::GetPlatform()
        ->GetGlobalVsyncDispatcher()
        ->AddMainThreadObserver(mObserver);
  }

  if (mObserver && interaction == DIRECTMANIPULATION_INTERACTION_END) {
    gfxWindowsPlatform::GetPlatform()
        ->GetGlobalVsyncDispatcher()
        ->RemoveMainThreadObserver(mObserver);
  }

  return S_OK;
}

void DManipEventHandler::Update() {
  if (mOwner) {
    mOwner->Update();
  }
}

void DirectManipulationOwner::Update() {
  if (mDmUpdateManager) {
    mDmUpdateManager->Update(nullptr);
  }
}

DirectManipulationOwner::DirectManipulationOwner(nsWindow* aWindow)
    : mWindow(aWindow) {}

DirectManipulationOwner::~DirectManipulationOwner() { Destroy(); }

bool DManipEventHandler::SendPinch(Phase aPhase, float aScale) {
  if (!mWindow) {
    return false;
  }

  if (aScale == mLastScale && aPhase != Phase::eEnd) {
    return false;
  }

  PinchGestureInput::PinchGestureType pinchGestureType =
      PinchGestureInput::PINCHGESTURE_SCALE;
  switch (aPhase) {
    case Phase::eStart:
      pinchGestureType = PinchGestureInput::PINCHGESTURE_START;
      break;
    case Phase::eMiddle:
      pinchGestureType = PinchGestureInput::PINCHGESTURE_SCALE;
      break;
    case Phase::eEnd:
      pinchGestureType = PinchGestureInput::PINCHGESTURE_END;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("handle all enum values");
  }

  TimeStamp eventTimeStamp = TimeStamp::Now();

  ModifierKeyState modifierKeyState;
  Modifiers mods = modifierKeyState.GetModifiers();

  ExternalPoint screenOffset = ViewAs<ExternalPixel>(
      mWindow->WidgetToScreenOffset(),
      PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent);

  POINT cursor_pos;
  ::GetCursorPos(&cursor_pos);
  HWND wnd = static_cast<HWND>(mWindow->GetNativeData(NS_NATIVE_WINDOW));
  ::ScreenToClient(wnd, &cursor_pos);
  ScreenPoint position = {(float)cursor_pos.x, (float)cursor_pos.y};

  PinchGestureInput event{pinchGestureType,
                          PinchGestureInput::TRACKPAD,
                          eventTimeStamp,
                          screenOffset,
                          position,
                          100.0 * ((aPhase == Phase::eEnd) ? 1.f : aScale),
                          100.0 * ((aPhase == Phase::eEnd) ? 1.f : mLastScale),
                          mods};

  if (!event.SetLineOrPageDeltaY(mWindow)) {
    return false;
  }

  mWindow->SendAnAPZEvent(event);

  return true;
}

void DManipEventHandler::SendPan(Phase aPhase, float x, float y,
                                 bool aIsInertia) {
  if (!mWindow) {
    return;
  }

  ModifierKeyState modifierKeyState;
  Modifiers mods = modifierKeyState.GetModifiers();

  POINT cursor_pos;
  ::GetCursorPos(&cursor_pos);
  HWND wnd = static_cast<HWND>(mWindow->GetNativeData(NS_NATIVE_WINDOW));
  ::ScreenToClient(wnd, &cursor_pos);
  ScreenPoint position = {(float)cursor_pos.x, (float)cursor_pos.y};

  SendPanCommon(mWindow, aPhase, position, x, y, mods, aIsInertia);
}

/* static */
void DManipEventHandler::SendPanCommon(nsWindow* aWindow, Phase aPhase,
                                       ScreenPoint aPosition, double aDeltaX,
                                       double aDeltaY, Modifiers aMods,
                                       bool aIsInertia) {
  if (!aWindow) {
    return;
  }

  PanGestureInput::PanGestureType panGestureType =
      PanGestureInput::PANGESTURE_PAN;
  if (aIsInertia) {
    switch (aPhase) {
      case Phase::eStart:
        panGestureType = PanGestureInput::PANGESTURE_MOMENTUMSTART;
        break;
      case Phase::eMiddle:
        panGestureType = PanGestureInput::PANGESTURE_MOMENTUMPAN;
        break;
      case Phase::eEnd:
        panGestureType = PanGestureInput::PANGESTURE_MOMENTUMEND;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("handle all enum values");
    }
  } else {
    switch (aPhase) {
      case Phase::eStart:
        panGestureType = PanGestureInput::PANGESTURE_START;
        break;
      case Phase::eMiddle:
        panGestureType = PanGestureInput::PANGESTURE_PAN;
        break;
      case Phase::eEnd:
        panGestureType = PanGestureInput::PANGESTURE_END;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("handle all enum values");
    }
  }

  TimeStamp eventTimeStamp = TimeStamp::Now();

  PanGestureInput event{panGestureType, eventTimeStamp, aPosition,
                        ScreenPoint(aDeltaX, aDeltaY), aMods};

  aWindow->SendAnAPZEvent(event);
}

void DirectManipulationOwner::Init(const LayoutDeviceIntRect& aBounds) {
  HRESULT hr = CoCreateInstance(
      CLSID_DirectManipulationManager, nullptr, CLSCTX_INPROC_SERVER,
      IID_IDirectManipulationManager, getter_AddRefs(mDmManager));
  if (!SUCCEEDED(hr)) {
    NS_WARNING("CoCreateInstance(CLSID_DirectManipulationManager failed");
    mDmManager = nullptr;
    return;
  }

  hr = mDmManager->GetUpdateManager(IID_IDirectManipulationUpdateManager,
                                    getter_AddRefs(mDmUpdateManager));
  if (!SUCCEEDED(hr)) {
    NS_WARNING("GetUpdateManager failed");
    mDmManager = nullptr;
    mDmUpdateManager = nullptr;
    return;
  }

  HWND wnd = static_cast<HWND>(mWindow->GetNativeData(NS_NATIVE_WINDOW));

  hr = mDmManager->CreateViewport(nullptr, wnd, IID_IDirectManipulationViewport,
                                  getter_AddRefs(mDmViewport));
  if (!SUCCEEDED(hr)) {
    NS_WARNING("CreateViewport failed");
    mDmManager = nullptr;
    mDmUpdateManager = nullptr;
    mDmViewport = nullptr;
    return;
  }

  DIRECTMANIPULATION_CONFIGURATION configuration =
      DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA |
      DIRECTMANIPULATION_CONFIGURATION_RAILS_X |
      DIRECTMANIPULATION_CONFIGURATION_RAILS_Y;
  if (StaticPrefs::apz_allow_zooming()) {
    configuration |= DIRECTMANIPULATION_CONFIGURATION_SCALING;
  }

  hr = mDmViewport->ActivateConfiguration(configuration);
  if (!SUCCEEDED(hr)) {
    NS_WARNING("ActivateConfiguration failed");
    mDmManager = nullptr;
    mDmUpdateManager = nullptr;
    mDmViewport = nullptr;
    return;
  }

  hr = mDmViewport->SetViewportOptions(
      DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE);
  if (!SUCCEEDED(hr)) {
    NS_WARNING("SetViewportOptions failed");
    mDmManager = nullptr;
    mDmUpdateManager = nullptr;
    mDmViewport = nullptr;
    return;
  }

  mDmHandler = new DManipEventHandler(mWindow, this, aBounds);

  hr = mDmViewport->AddEventHandler(wnd, mDmHandler.get(),
                                    &mDmViewportHandlerCookie);
  if (!SUCCEEDED(hr)) {
    NS_WARNING("AddEventHandler failed");
    mDmManager = nullptr;
    mDmUpdateManager = nullptr;
    mDmViewport = nullptr;
    mDmHandler = nullptr;
    return;
  }

  RECT rect = {0, 0, aBounds.Width(), aBounds.Height()};
  hr = mDmViewport->SetViewportRect(&rect);
  if (!SUCCEEDED(hr)) {
    NS_WARNING("SetViewportRect failed");
    mDmManager = nullptr;
    mDmUpdateManager = nullptr;
    mDmViewport = nullptr;
    mDmHandler = nullptr;
    return;
  }

  hr = mDmManager->Activate(wnd);
  if (!SUCCEEDED(hr)) {
    NS_WARNING("manager Activate failed");
    mDmManager = nullptr;
    mDmUpdateManager = nullptr;
    mDmViewport = nullptr;
    mDmHandler = nullptr;
    return;
  }

  hr = mDmViewport->Enable();
  if (!SUCCEEDED(hr)) {
    NS_WARNING("mDmViewport->Enable failed");
    mDmManager = nullptr;
    mDmUpdateManager = nullptr;
    mDmViewport = nullptr;
    mDmHandler = nullptr;
    return;
  }

  hr = mDmUpdateManager->Update(nullptr);
  if (!SUCCEEDED(hr)) {
    NS_WARNING("mDmUpdateManager->Update failed");
    mDmManager = nullptr;
    mDmUpdateManager = nullptr;
    mDmViewport = nullptr;
    mDmHandler = nullptr;
    return;
  }
}

void DirectManipulationOwner::ResizeViewport(
    const LayoutDeviceIntRect& aBounds) {
  if (mDmHandler) {
    mDmHandler->mBounds = aBounds;
  }

  if (mDmViewport) {
    RECT rect = {0, 0, aBounds.Width(), aBounds.Height()};
    HRESULT hr = mDmViewport->SetViewportRect(&rect);
    if (!SUCCEEDED(hr)) {
      NS_WARNING("SetViewportRect failed");
    }
  }
}

void DirectManipulationOwner::Destroy() {
  if (mDmHandler) {
    mDmHandler->mWindow = nullptr;
    mDmHandler->mOwner = nullptr;
    if (mDmHandler->mObserver) {
      gfxWindowsPlatform::GetPlatform()
          ->GetGlobalVsyncDispatcher()
          ->RemoveMainThreadObserver(mDmHandler->mObserver);
      mDmHandler->mObserver->ClearOwner();
      mDmHandler->mObserver = nullptr;
    }
  }

  HRESULT hr;
  if (mDmViewport) {
    hr = mDmViewport->Stop();
    if (!SUCCEEDED(hr)) {
      NS_WARNING("mDmViewport->Stop() failed");
    }

    hr = mDmViewport->Disable();
    if (!SUCCEEDED(hr)) {
      NS_WARNING("mDmViewport->Disable() failed");
    }

    hr = mDmViewport->RemoveEventHandler(mDmViewportHandlerCookie);
    if (!SUCCEEDED(hr)) {
      NS_WARNING("mDmViewport->RemoveEventHandler() failed");
    }

    hr = mDmViewport->Abandon();
    if (!SUCCEEDED(hr)) {
      NS_WARNING("mDmViewport->Abandon() failed");
    }
  }

  if (mWindow) {
    HWND wnd = static_cast<HWND>(mWindow->GetNativeData(NS_NATIVE_WINDOW));

    if (mDmManager) {
      hr = mDmManager->Deactivate(wnd);
      if (!SUCCEEDED(hr)) {
        NS_WARNING("mDmManager->Deactivate() failed");
      }
    }
  }

  mDmHandler = nullptr;
  mDmViewport = nullptr;
  mDmUpdateManager = nullptr;
  mDmManager = nullptr;
  mWindow = nullptr;
}

void DirectManipulationOwner::SetContact(UINT aContactId) {
  if (mDmViewport) {
    mDmViewport->SetContact(aContactId);
  }
}

/*static  */ void DirectManipulationOwner::SynthesizeNativeTouchpadPan(
    nsWindow* aWindow, nsIWidget::TouchpadGesturePhase aEventPhase,
    LayoutDeviceIntPoint aPoint, double aDeltaX, double aDeltaY,
    int32_t aModifierFlags) {
  DManipEventHandler::SynthesizeNativeTouchpadPan(
      aWindow, aEventPhase, aPoint, aDeltaX, aDeltaY, aModifierFlags);
}

/*static  */ void DManipEventHandler::SynthesizeNativeTouchpadPan(
    nsWindow* aWindow, nsIWidget::TouchpadGesturePhase aEventPhase,
    LayoutDeviceIntPoint aPoint, double aDeltaX, double aDeltaY,
    int32_t aModifierFlags) {
  ScreenPoint position = {(float)aPoint.x, (float)aPoint.y};
  Phase phase = Phase::eStart;
  if (aEventPhase == nsIWidget::PHASE_UPDATE) {
    phase = Phase::eMiddle;
  }

  if (aEventPhase == nsIWidget::PHASE_END) {
    phase = Phase::eEnd;
  }
  SendPanCommon(aWindow, phase, position, aDeltaX, aDeltaY, aModifierFlags,
                /* aIsInertia = */ false);
}

}  // namespace widget
}  // namespace mozilla
