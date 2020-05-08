/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectManipulationOwner.h"
#include "nsWindow.h"

// Direct Manipulation is only defined for Win8 and newer.
#if defined(_WIN32_WINNT)
#  undef _WIN32_WINNT
#  define _WIN32_WINNT _WIN32_WINNT_WIN8
#endif  // defined(_WIN32_WINNT)
#if defined(NTDDI_VERSION)
#  undef NTDDI_VERSION
#  define NTDDI_VERSION NTDDI_WIN8
#endif  // defined(NTDDI_VERSION)

#include "directmanipulation.h"

namespace mozilla {
namespace widget {

class DManipEventHandler : public IDirectManipulationViewportEventHandler,
                           public IDirectManipulationInteractionEventHandler {
 public:
  friend class DirectManipulationOwner;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DManipEventHandler)

  STDMETHODIMP QueryInterface(REFIID, void**) override;

  friend class DirectManipulationOwner;

  explicit DManipEventHandler(nsWindow* aWindow,
                              DirectManipulationOwner* aOwner)
      : mWindow(aWindow), mOwner(aOwner) {}

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
   public:
    bool NotifyVsync(const mozilla::VsyncEvent& aVsync) override {
      if (mOwner) {
        mOwner->Update();
      }
      return true;
    }
    explicit VObserver(DManipEventHandler* aOwner) : mOwner(aOwner) {}

    void ClearOwner() { mOwner = nullptr; }

   private:
    virtual ~VObserver() {}
    DManipEventHandler* mOwner;
  };

 private:
  virtual ~DManipEventHandler() = default;

  nsWindow* mWindow;
  DirectManipulationOwner* mOwner;
  RefPtr<VObserver> mObserver;
};

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
  return S_OK;
}

HRESULT
DManipEventHandler::OnViewportUpdated(IDirectManipulationViewport* viewport) {
  return S_OK;
}

HRESULT
DManipEventHandler::OnContentUpdated(IDirectManipulationViewport* viewport,
                                     IDirectManipulationContent* content) {
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

    gfxWindowsPlatform::GetPlatform()->GetHardwareVsync()->AddGenericObserver(
        mObserver);
  }

  if (mObserver && interaction == DIRECTMANIPULATION_INTERACTION_END) {
    gfxWindowsPlatform::GetPlatform()
        ->GetHardwareVsync()
        ->RemoveGenericObserver(mObserver);
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

  mDmHandler = new DManipEventHandler(mWindow, this);

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
          ->GetHardwareVsync()
          ->RemoveGenericObserver(mDmHandler->mObserver);
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

}  // namespace widget
}  // namespace mozilla
