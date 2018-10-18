/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <winsdkver.h>
#include <wrl.h>

#include "nsServiceManagerUtils.h"

#include "WindowsUIUtils.h"

#include "nsIObserverService.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIAppShellService.h"
#include "nsAppShellCID.h"
#include "nsIXULWindow.h"
#include "mozilla/Services.h"
#include "mozilla/WindowsVersion.h"
#include "nsString.h"
#include "nsIWidget.h"

/* mingw currently doesn't support windows.ui.viewmanagement.h, so we disable it until it's fixed. */
#ifndef __MINGW32__

#include <windows.ui.viewmanagement.h>

#pragma comment(lib, "runtimeobject.lib")

using namespace mozilla;
using namespace ABI::Windows::UI;
using namespace ABI::Windows::UI::ViewManagement;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::ApplicationModel::DataTransfer;

/* All of this is win10 stuff and we're compiling against win81 headers
 * for now, so we may need to do some legwork: */
#if WINVER_MAXVER < 0x0A00
namespace ABI {
  namespace Windows {
    namespace UI {
      namespace ViewManagement {
        enum UserInteractionMode {
          UserInteractionMode_Mouse = 0,
          UserInteractionMode_Touch = 1
        };
      }
    }
  }
}

#endif

#ifndef RuntimeClass_Windows_UI_ViewManagement_UIViewSettings
#define RuntimeClass_Windows_UI_ViewManagement_UIViewSettings L"Windows.UI.ViewManagement.UIViewSettings"
#endif

#if WINVER_MAXVER < 0x0A00
namespace ABI {
  namespace Windows {
    namespace UI {
      namespace ViewManagement {
        interface IUIViewSettings;
        MIDL_INTERFACE("C63657F6-8850-470D-88F8-455E16EA2C26")
          IUIViewSettings : public IInspectable
          {
            public:
              virtual HRESULT STDMETHODCALLTYPE get_UserInteractionMode(UserInteractionMode *value) = 0;
          };

        extern const __declspec(selectany) IID & IID_IUIViewSettings = __uuidof(IUIViewSettings);
      }
    }
  }
}
#endif

#ifndef IUIViewSettingsInterop

typedef interface IUIViewSettingsInterop IUIViewSettingsInterop;

MIDL_INTERFACE("3694dbf9-8f68-44be-8ff5-195c98ede8a6")
IUIViewSettingsInterop : public IInspectable
{
public:
  virtual HRESULT STDMETHODCALLTYPE GetForWindow(HWND hwnd, REFIID riid, void **ppv) = 0;
};
#endif

#ifndef __IDataTransferManagerInterop_INTERFACE_DEFINED__
#define __IDataTransferManagerInterop_INTERFACE_DEFINED__

typedef interface IDataTransferManagerInterop IDataTransferManagerInterop;

MIDL_INTERFACE("3A3DCD6C-3EAB-43DC-BCDE-45671CE800C8")
IDataTransferManagerInterop : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetForWindow(HWND appWindow, REFIID riid, void **dataTransferManager) = 0;
    virtual HRESULT STDMETHODCALLTYPE ShowShareUIForWindow(HWND appWindow) = 0;
};

#endif

#endif

WindowsUIUtils::WindowsUIUtils() :
  mInTabletMode(eTabletModeUnknown)
{
}

WindowsUIUtils::~WindowsUIUtils()
{
}

/*
 * Implement the nsISupports methods...
 */
NS_IMPL_ISUPPORTS(WindowsUIUtils,
                  nsIWindowsUIUtils)

NS_IMETHODIMP
WindowsUIUtils::GetInTabletMode(bool* aResult)
{
  if (mInTabletMode == eTabletModeUnknown) {
    UpdateTabletModeState();
  }
  *aResult = mInTabletMode == eTabletModeOn;
  return NS_OK;
}

NS_IMETHODIMP
WindowsUIUtils::UpdateTabletModeState()
{
#ifndef __MINGW32__
  if (!IsWin10OrLater()) {
    return NS_OK;
  }

  nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  nsCOMPtr<nsIXULWindow> hiddenWindow;

  nsresult rv = appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDocShell> docShell;
  rv = hiddenWindow->GetDocShell(getter_AddRefs(docShell));
  if (NS_FAILED(rv) || !docShell) {
    return rv;
  }

  nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(docShell));

  if (!baseWindow)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWidget> widget;
  baseWindow->GetMainWidget(getter_AddRefs(widget));

  if (!widget)
    return NS_ERROR_FAILURE;

  HWND winPtr = (HWND)widget->GetNativeData(NS_NATIVE_WINDOW);
  ComPtr<IUIViewSettingsInterop> uiViewSettingsInterop;

  HRESULT hr = GetActivationFactory(
      HStringReference(RuntimeClass_Windows_UI_ViewManagement_UIViewSettings).Get(),
      &uiViewSettingsInterop);
  if (SUCCEEDED(hr)) {
    ComPtr<IUIViewSettings> uiViewSettings;
    hr = uiViewSettingsInterop->GetForWindow(winPtr, IID_PPV_ARGS(&uiViewSettings));
    if (SUCCEEDED(hr)) {
      UserInteractionMode mode;
      hr = uiViewSettings->get_UserInteractionMode(&mode);
      if (SUCCEEDED(hr)) {
        TabletModeState oldTabletModeState = mInTabletMode;
        mInTabletMode = (mode == UserInteractionMode_Touch) ? eTabletModeOn : eTabletModeOff;
        if (mInTabletMode != oldTabletModeState) {
          nsCOMPtr<nsIObserverService> observerService =
            mozilla::services::GetObserverService();
          observerService->NotifyObservers(nullptr, "tablet-mode-change",
            ((mInTabletMode == eTabletModeOn) ? u"tablet-mode" : u"normal-mode"));
        }
      }
    }
  }
#endif

  return NS_OK;
}

struct HStringDeleter
{
  typedef HSTRING pointer;
  void operator()(pointer aString)
  {
      WindowsDeleteString(aString);
  }
};

typedef mozilla::UniquePtr<HSTRING, HStringDeleter> HStringUniquePtr;

NS_IMETHODIMP
WindowsUIUtils::ShareUrl(const nsAString& aUrlToShare,
                         const nsAString& aShareTitle)
{
#ifndef __MINGW32__
  if (!IsWin10OrLater()) {
    return NS_OK;
  }

  HSTRING rawTitle;
  HRESULT hr = WindowsCreateString(PromiseFlatString(aShareTitle).get(), aShareTitle.Length(), &rawTitle);
  if (FAILED(hr)) {
    return NS_OK;
  }
  HStringUniquePtr title(rawTitle);

  HSTRING rawUrl;
  hr = WindowsCreateString(PromiseFlatString(aUrlToShare).get(), aUrlToShare.Length(), &rawUrl);
  if (FAILED(hr)) {
    return NS_OK;
  }
  HStringUniquePtr url(rawUrl);

  ComPtr<IUriRuntimeClassFactory> uriFactory;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(), &uriFactory);
  if (FAILED(hr)) {
    return NS_OK;
  }

  ComPtr<IUriRuntimeClass> uri;
  hr = uriFactory->CreateUri(url.get(), &uri);
  if (FAILED(hr)) {
    return NS_OK;
  }

  HWND hwnd = GetForegroundWindow();
  if (!hwnd) {
    return NS_OK;
  }

  ComPtr<IDataTransferManagerInterop> dtmInterop;
  hr = RoGetActivationFactory(HStringReference(
    RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager)
                         .Get(), IID_PPV_ARGS(&dtmInterop));
  if (FAILED(hr)) {
    return NS_OK;
  }

  ComPtr<IDataTransferManager> dtm;
  hr = dtmInterop->GetForWindow(hwnd, IID_PPV_ARGS(&dtm));
  if (FAILED(hr)) {
    return NS_OK;
  }

  auto callback = Callback < ITypedEventHandler<DataTransferManager*, DataRequestedEventArgs* >> (
          [uri = std::move(uri), title = std::move(title)](IDataTransferManager*, IDataRequestedEventArgs* pArgs) -> HRESULT
    {
      ComPtr<IDataRequest> spDataRequest;
      HRESULT hr = pArgs->get_Request(&spDataRequest);
      if (FAILED(hr)) {
        return hr;
      }

      ComPtr<IDataPackage> spDataPackage;
      hr = spDataRequest->get_Data(&spDataPackage);
      if (FAILED(hr)) {
        return hr;
      }

      ComPtr<IDataPackage2> spDataPackage2;
      hr = spDataPackage->QueryInterface(IID_PPV_ARGS(&spDataPackage2));
      if (FAILED(hr)) {
        return hr;
      }

      ComPtr<IDataPackagePropertySet> spDataPackageProperties;
      hr = spDataPackage->get_Properties(&spDataPackageProperties);
      if (FAILED(hr)) {
        return hr;
      }

      hr = spDataPackageProperties->put_Title(title.get());
      if (FAILED(hr)) {
        return hr;
      }

      hr = spDataPackage2->SetWebLink(uri.Get());
      if (FAILED(hr)) {
        return hr;
      }

      return S_OK;
    });

  EventRegistrationToken dataRequestedToken;
  hr = dtm->add_DataRequested(callback.Get(), &dataRequestedToken);
  if (FAILED(hr)) {
    return NS_OK;
  }

  hr = dtmInterop->ShowShareUIForWindow(hwnd);
  if (FAILED(hr)) {
    return NS_OK;
  }

#endif

  return NS_OK;
}
