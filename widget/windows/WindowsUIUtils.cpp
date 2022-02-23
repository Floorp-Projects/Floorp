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
#include "nsIAppShellService.h"
#include "nsAppShellCID.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/media/MediaUtils.h"
#include "nsString.h"
#include "nsIWidget.h"
#include "nsIWindowMediator.h"
#include "nsPIDOMWindow.h"
#include "nsWindowGfx.h"
#include "Units.h"

/* mingw currently doesn't support windows.ui.viewmanagement.h, so we disable it
 * until it's fixed. */
#ifndef __MINGW32__

#  include <windows.ui.viewmanagement.h>

#  pragma comment(lib, "runtimeobject.lib")

using namespace ABI::Windows::UI;
using namespace ABI::Windows::UI::ViewManagement;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::ApplicationModel::DataTransfer;

/* All of this is win10 stuff and we're compiling against win81 headers
 * for now, so we may need to do some legwork: */
#  if WINVER_MAXVER < 0x0A00
namespace ABI {
namespace Windows {
namespace UI {
namespace ViewManagement {
enum UserInteractionMode {
  UserInteractionMode_Mouse = 0,
  UserInteractionMode_Touch = 1
};
}
}  // namespace UI
}  // namespace Windows
}  // namespace ABI

#  endif

#  ifndef RuntimeClass_Windows_UI_ViewManagement_UIViewSettings
#    define RuntimeClass_Windows_UI_ViewManagement_UIViewSettings \
      L"Windows.UI.ViewManagement.UIViewSettings"
#  endif

#  if WINVER_MAXVER < 0x0A00
namespace ABI {
namespace Windows {
namespace UI {
namespace ViewManagement {
interface IUIViewSettings;
MIDL_INTERFACE("C63657F6-8850-470D-88F8-455E16EA2C26")
IUIViewSettings : public IInspectable {
 public:
  virtual HRESULT STDMETHODCALLTYPE get_UserInteractionMode(
      UserInteractionMode * value) = 0;
};

extern const __declspec(selectany) IID& IID_IUIViewSettings =
    __uuidof(IUIViewSettings);
}  // namespace ViewManagement
}  // namespace UI
}  // namespace Windows
}  // namespace ABI
#  endif

#  ifndef IUIViewSettingsInterop

using IUIViewSettingsInterop = interface IUIViewSettingsInterop;

MIDL_INTERFACE("3694dbf9-8f68-44be-8ff5-195c98ede8a6")
IUIViewSettingsInterop : public IInspectable {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetForWindow(HWND hwnd, REFIID riid,
                                                 void** ppv) = 0;
};
#  endif

#  ifndef __IDataTransferManagerInterop_INTERFACE_DEFINED__
#    define __IDataTransferManagerInterop_INTERFACE_DEFINED__

using IDataTransferManagerInterop = interface IDataTransferManagerInterop;

MIDL_INTERFACE("3A3DCD6C-3EAB-43DC-BCDE-45671CE800C8")
IDataTransferManagerInterop : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetForWindow(
      HWND appWindow, REFIID riid, void** dataTransferManager) = 0;
  virtual HRESULT STDMETHODCALLTYPE ShowShareUIForWindow(HWND appWindow) = 0;
};

#  endif

#  if !defined( \
      ____x_ABI_CWindows_CApplicationModel_CDataTransfer_CIDataPackage4_INTERFACE_DEFINED__)
#    define ____x_ABI_CWindows_CApplicationModel_CDataTransfer_CIDataPackage4_INTERFACE_DEFINED__

MIDL_INTERFACE("13a24ec8-9382-536f-852a-3045e1b29a3b")
IDataPackage4 : public IInspectable {
 public:
  virtual HRESULT STDMETHODCALLTYPE add_ShareCanceled(
      __FITypedEventHandler_2_Windows__CApplicationModel__CDataTransfer__CDataPackage_IInspectable *
          handler,
      EventRegistrationToken * token) = 0;
  virtual HRESULT STDMETHODCALLTYPE remove_ShareCanceled(
      EventRegistrationToken token) = 0;
};

#  endif

#endif

using namespace mozilla;

WindowsUIUtils::WindowsUIUtils() : mInTabletMode(eTabletModeUnknown) {}

WindowsUIUtils::~WindowsUIUtils() = default;

/*
 * Implement the nsISupports methods...
 */
NS_IMPL_ISUPPORTS(WindowsUIUtils, nsIWindowsUIUtils)

NS_IMETHODIMP
WindowsUIUtils::GetSystemSmallIconSize(int32_t* aSize) {
  NS_ENSURE_ARG(aSize);

  mozilla::LayoutDeviceIntSize size =
      nsWindowGfx::GetIconMetrics(nsWindowGfx::kSmallIcon);
  *aSize = std::max(size.width, size.height);
  return NS_OK;
}

NS_IMETHODIMP
WindowsUIUtils::GetSystemLargeIconSize(int32_t* aSize) {
  NS_ENSURE_ARG(aSize);

  mozilla::LayoutDeviceIntSize size =
      nsWindowGfx::GetIconMetrics(nsWindowGfx::kRegularIcon);
  *aSize = std::max(size.width, size.height);
  return NS_OK;
}

NS_IMETHODIMP
WindowsUIUtils::SetWindowIcon(mozIDOMWindowProxy* aWindow,
                              imgIContainer* aSmallIcon,
                              imgIContainer* aBigIcon) {
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIWidget> widget =
      nsGlobalWindowOuter::Cast(aWindow)->GetMainWidget();
  nsWindow* window = static_cast<nsWindow*>(widget.get());

  nsresult rv;

  if (aSmallIcon) {
    HICON hIcon = nullptr;
    rv = nsWindowGfx::CreateIcon(
        aSmallIcon, false, mozilla::LayoutDeviceIntPoint(),
        nsWindowGfx::GetIconMetrics(nsWindowGfx::kSmallIcon), &hIcon);
    NS_ENSURE_SUCCESS(rv, rv);

    window->SetSmallIcon(hIcon);
  }

  if (aBigIcon) {
    HICON hIcon = nullptr;
    rv = nsWindowGfx::CreateIcon(
        aBigIcon, false, mozilla::LayoutDeviceIntPoint(),
        nsWindowGfx::GetIconMetrics(nsWindowGfx::kRegularIcon), &hIcon);
    NS_ENSURE_SUCCESS(rv, rv);

    window->SetBigIcon(hIcon);
  }

  return NS_OK;
}

NS_IMETHODIMP
WindowsUIUtils::SetWindowIconNoData(mozIDOMWindowProxy* aWindow) {
  NS_ENSURE_ARG(aWindow);

  nsCOMPtr<nsIWidget> widget =
      nsGlobalWindowOuter::Cast(aWindow)->GetMainWidget();
  nsWindow* window = static_cast<nsWindow*>(widget.get());

  window->SetSmallIconNoData();
  window->SetBigIconNoData();

  return NS_OK;
}

NS_IMETHODIMP
WindowsUIUtils::GetInTabletMode(bool* aResult) {
  if (mInTabletMode == eTabletModeUnknown) {
    UpdateTabletModeState();
  }
  *aResult = mInTabletMode == eTabletModeOn;
  return NS_OK;
}

NS_IMETHODIMP
WindowsUIUtils::UpdateTabletModeState() {
#ifndef __MINGW32__
  if (!IsWin10OrLater()) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIWindowMediator> winMediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIWidget> widget;
  nsCOMPtr<mozIDOMWindowProxy> navWin;

  rv = winMediator->GetMostRecentWindow(u"navigator:browser",
                                        getter_AddRefs(navWin));
  if (NS_FAILED(rv) || !navWin) {
    // Fall back to the hidden window
    nsCOMPtr<nsIAppShellService> appShell(
        do_GetService(NS_APPSHELLSERVICE_CONTRACTID));

    rv = appShell->GetHiddenDOMWindow(getter_AddRefs(navWin));
    if (NS_FAILED(rv) || !navWin) {
      return rv;
    }
  }

  nsPIDOMWindowOuter* win = nsPIDOMWindowOuter::From(navWin);
  widget = widget::WidgetUtils::DOMWindowToWidget(win);

  if (!widget) {
    return NS_ERROR_FAILURE;
  }

  HWND winPtr = (HWND)widget->GetNativeData(NS_NATIVE_WINDOW);
  ComPtr<IUIViewSettingsInterop> uiViewSettingsInterop;

  HRESULT hr = GetActivationFactory(
      HStringReference(RuntimeClass_Windows_UI_ViewManagement_UIViewSettings)
          .Get(),
      &uiViewSettingsInterop);
  if (SUCCEEDED(hr)) {
    ComPtr<IUIViewSettings> uiViewSettings;
    hr = uiViewSettingsInterop->GetForWindow(winPtr,
                                             IID_PPV_ARGS(&uiViewSettings));
    if (SUCCEEDED(hr)) {
      UserInteractionMode mode;
      hr = uiViewSettings->get_UserInteractionMode(&mode);
      if (SUCCEEDED(hr)) {
        TabletModeState oldTabletModeState = mInTabletMode;
        mInTabletMode = (mode == UserInteractionMode_Touch) ? eTabletModeOn
                                                            : eTabletModeOff;
        if (mInTabletMode != oldTabletModeState) {
          nsCOMPtr<nsIObserverService> observerService =
              mozilla::services::GetObserverService();
          observerService->NotifyObservers(
              nullptr, "tablet-mode-change",
              ((mInTabletMode == eTabletModeOn) ? u"tablet-mode"
                                                : u"normal-mode"));
        }
      }
    }
  }
#endif

  return NS_OK;
}

#ifndef __MINGW32__
struct HStringDeleter {
  using pointer = HSTRING;
  void operator()(pointer aString) { WindowsDeleteString(aString); }
};

using HStringUniquePtr = UniquePtr<HSTRING, HStringDeleter>;

Result<HStringUniquePtr, HRESULT> ConvertToWindowsString(
    const nsAString& aStr) {
  HSTRING rawStr;
  HRESULT hr = WindowsCreateString(PromiseFlatString(aStr).get(), aStr.Length(),
                                   &rawStr);
  if (FAILED(hr)) {
    return Err(hr);
  }
  return HStringUniquePtr(rawStr);
}

static Result<Ok, nsresult> RequestShare(
    const std::function<HRESULT(IDataRequestedEventArgs* pArgs)>& aCallback) {
  if (!IsWin10OrLater()) {
    return Err(NS_ERROR_FAILURE);
  }

  HWND hwnd = GetForegroundWindow();
  if (!hwnd) {
    return Err(NS_ERROR_FAILURE);
  }

  ComPtr<IDataTransferManagerInterop> dtmInterop;
  ComPtr<IDataTransferManager> dtm;

  HRESULT hr = RoGetActivationFactory(
      HStringReference(
          RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager)
          .Get(),
      IID_PPV_ARGS(&dtmInterop));
  if (FAILED(hr) ||
      FAILED(dtmInterop->GetForWindow(hwnd, IID_PPV_ARGS(&dtm)))) {
    return Err(NS_ERROR_FAILURE);
  }

  auto callback = Callback<
      ITypedEventHandler<DataTransferManager*, DataRequestedEventArgs*>>(
      [aCallback](IDataTransferManager*,
                  IDataRequestedEventArgs* pArgs) -> HRESULT {
        return aCallback(pArgs);
      });

  EventRegistrationToken dataRequestedToken;
  if (FAILED(dtm->add_DataRequested(callback.Get(), &dataRequestedToken)) ||
      FAILED(dtmInterop->ShowShareUIForWindow(hwnd))) {
    return Err(NS_ERROR_FAILURE);
  }

  return Ok();
}

static Result<Ok, nsresult> AddShareEventListeners(
    const RefPtr<mozilla::media::Refcountable<MozPromiseHolder<SharePromise>>>&
        aPromiseHolder,
    const ComPtr<IDataPackage>& aDataPackage) {
  ComPtr<IDataPackage3> spDataPackage3;

  if (FAILED(aDataPackage.As(&spDataPackage3))) {
    return Err(NS_ERROR_FAILURE);
  }

  auto completedCallback =
      Callback<ITypedEventHandler<DataPackage*, ShareCompletedEventArgs*>>(
          [aPromiseHolder](IDataPackage*,
                           IShareCompletedEventArgs*) -> HRESULT {
            aPromiseHolder->Resolve(true, __func__);
            return S_OK;
          });

  EventRegistrationToken dataRequestedToken;
  if (FAILED(spDataPackage3->add_ShareCompleted(completedCallback.Get(),
                                                &dataRequestedToken))) {
    return Err(NS_ERROR_FAILURE);
  }

  ComPtr<IDataPackage4> spDataPackage4;
  if (SUCCEEDED(aDataPackage.As(&spDataPackage4))) {
    // Use SharedCanceled API only on supported versions of Windows
    // So that the older ones can still use ShareUrl()

    auto canceledCallback =
        Callback<ITypedEventHandler<DataPackage*, IInspectable*>>(
            [aPromiseHolder](IDataPackage*, IInspectable*) -> HRESULT {
              aPromiseHolder->Reject(NS_ERROR_FAILURE, __func__);
              return S_OK;
            });

    if (FAILED(spDataPackage4->add_ShareCanceled(canceledCallback.Get(),
                                                 &dataRequestedToken))) {
      return Err(NS_ERROR_FAILURE);
    }
  }

  return Ok();
}
#endif

RefPtr<SharePromise> WindowsUIUtils::Share(nsAutoString aTitle,
                                           nsAutoString aText,
                                           nsAutoString aUrl) {
  auto promiseHolder = MakeRefPtr<
      mozilla::media::Refcountable<MozPromiseHolder<SharePromise>>>();
  RefPtr<SharePromise> promise = promiseHolder->Ensure(__func__);

#ifndef __MINGW32__
  auto result = RequestShare([promiseHolder, title = std::move(aTitle),
                              text = std::move(aText), url = std::move(aUrl)](
                                 IDataRequestedEventArgs* pArgs) {
    ComPtr<IDataRequest> spDataRequest;
    ComPtr<IDataPackage> spDataPackage;
    ComPtr<IDataPackage2> spDataPackage2;
    ComPtr<IDataPackagePropertySet> spDataPackageProperties;

    if (FAILED(pArgs->get_Request(&spDataRequest)) ||
        FAILED(spDataRequest->get_Data(&spDataPackage)) ||
        FAILED(spDataPackage.As(&spDataPackage2)) ||
        FAILED(spDataPackage->get_Properties(&spDataPackageProperties))) {
      promiseHolder->Reject(NS_ERROR_FAILURE, __func__);
      return E_FAIL;
    }

    /*
     * Windows always requires a title, and an empty string does not work.
     * Thus we trick the API by passing a whitespace when we have no title.
     * https://docs.microsoft.com/en-us/windows/uwp/app-to-app/share-data
     */
    auto wTitle = ConvertToWindowsString((title.IsVoid() || title.Length() == 0)
                                             ? nsAutoString(u" "_ns)
                                             : title);
    if (wTitle.isErr() ||
        FAILED(spDataPackageProperties->put_Title(wTitle.unwrap().get()))) {
      promiseHolder->Reject(NS_ERROR_FAILURE, __func__);
      return E_FAIL;
    }

    // Assign even if empty, as Windows requires some data to share
    auto wText = ConvertToWindowsString(text);
    if (wText.isErr() || FAILED(spDataPackage->SetText(wText.unwrap().get()))) {
      promiseHolder->Reject(NS_ERROR_FAILURE, __func__);
      return E_FAIL;
    }

    if (!url.IsVoid()) {
      auto wUrl = ConvertToWindowsString(url);
      if (wUrl.isErr()) {
        promiseHolder->Reject(NS_ERROR_FAILURE, __func__);
        return wUrl.unwrapErr();
      }

      ComPtr<IUriRuntimeClassFactory> uriFactory;
      ComPtr<IUriRuntimeClass> uri;

      auto hr = GetActivationFactory(
          HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
          &uriFactory);

      if (FAILED(hr) ||
          FAILED(uriFactory->CreateUri(wUrl.unwrap().get(), &uri)) ||
          FAILED(spDataPackage2->SetWebLink(uri.Get()))) {
        promiseHolder->Reject(NS_ERROR_FAILURE, __func__);
        return E_FAIL;
      }
    }

    if (!StaticPrefs::widget_windows_share_wait_action_enabled()) {
      promiseHolder->Resolve(true, __func__);
    } else if (AddShareEventListeners(promiseHolder, spDataPackage).isErr()) {
      promiseHolder->Reject(NS_ERROR_FAILURE, __func__);
      return E_FAIL;
    }

    return S_OK;
  });
  if (result.isErr()) {
    promiseHolder->Reject(result.unwrapErr(), __func__);
  }
#else
  promiseHolder->Reject(NS_ERROR_FAILURE, __func__);
#endif

  return promise;
}

NS_IMETHODIMP
WindowsUIUtils::ShareUrl(const nsAString& aUrlToShare,
                         const nsAString& aShareTitle) {
  nsAutoString text;
  text.SetIsVoid(true);
  WindowsUIUtils::Share(nsAutoString(aShareTitle), text,
                        nsAutoString(aUrlToShare));
  return NS_OK;
}
