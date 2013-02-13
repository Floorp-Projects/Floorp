/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
// so we can get logging even in release builds
#define FORCE_PR_LOG 1
#endif

#include "MetroUtils.h"
#include <windows.h>
#include "nsICommandLineRunner.h"
#include "nsNetUtil.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIWindowMediator.h"
#include "nsIURI.h"
#include "prlog.h"
#include "nsIObserverService.h"

#include <wrl/wrappers/corewrappers.h>
#include <windows.ui.applicationsettings.h>

using namespace ABI::Windows::UI::ApplicationSettings;

using namespace mozilla;

using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI::ViewManagement;

// File-scoped statics (unnamed namespace)
namespace {
#ifdef PR_LOGGING
  PRLogModuleInfo* metroWidgetLog = PR_NewLogModule("MetroWidget");
#endif
};

void Log(const wchar_t *fmt, ...)
{
  va_list a = NULL;
  wchar_t szDebugString[1024];
  if(!lstrlenW(fmt))
    return;
  va_start(a,fmt);
  vswprintf(szDebugString, 1024, fmt, a);
  va_end(a);
  if(!lstrlenW(szDebugString))
    return;

  // MSVC, including remote debug sessions
  OutputDebugStringW(szDebugString);
  OutputDebugStringW(L"\n");

  // desktop console
  wprintf(L"%s\n", szDebugString);

#ifdef PR_LOGGING
  NS_ASSERTION(metroWidgetLog, "Called MetroUtils Log() but MetroWidget "
                               "log module doesn't exist!");
  int len = wcslen(szDebugString);
  if (len) {
    char* utf8 = new char[len+1];
    memset(utf8, 0, sizeof(utf8));
    if (WideCharToMultiByte(CP_ACP, 0, szDebugString,
                            -1, utf8, len+1, NULL,
                            NULL) > 0) {
      PR_LOG(metroWidgetLog, PR_LOG_ALWAYS, (utf8));
    }
    delete[] utf8;
  }
#endif
}

nsresult
MetroUtils::FireObserver(const char* aMessage, const PRUnichar* aData)
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    return observerService->NotifyObservers(nullptr, aMessage, aData);
  }
  return NS_ERROR_FAILURE;
}

HRESULT MetroUtils::CreateUri(HSTRING aUriStr, ComPtr<IUriRuntimeClass>& aUriOut)
{
  HRESULT hr;
  ComPtr<IUriRuntimeClassFactory> uriFactory;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(), &uriFactory);
  AssertRetHRESULT(hr, hr);
  ComPtr<IUriRuntimeClass> uri;
  return uriFactory->CreateUri(aUriStr, &aUriOut);
}

HRESULT MetroUtils::CreateUri(HString& aHString, ComPtr<IUriRuntimeClass>& aUriOut)
{
  return MetroUtils::CreateUri(aHString.Get(), aUriOut);
}

HRESULT
MetroUtils::GetViewState(ApplicationViewState& aState)
{
  HRESULT hr;
  ComPtr<IApplicationViewStatics> appViewStatics;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_ViewManagement_ApplicationView).Get(),
    appViewStatics.GetAddressOf());
  AssertRetHRESULT(hr, hr);
  hr = appViewStatics->get_Value(&aState);
  return hr;
}

HRESULT
MetroUtils::TryUnsnap(bool* aResult)
{
  HRESULT hr;
  ComPtr<IApplicationViewStatics> appViewStatics;
  hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_ViewManagement_ApplicationView).Get(),
    appViewStatics.GetAddressOf());
  AssertRetHRESULT(hr, hr);
  boolean success = false;
  hr = appViewStatics->TryUnsnap(&success);
  if (aResult)
    *aResult = success;
  return hr;
}

HRESULT
MetroUtils::ShowSettingsFlyout()
{
  ComPtr<ISettingsPaneStatics> settingsPaneStatics;
  HRESULT hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_ApplicationSettings_SettingsPane).Get(),
                                    settingsPaneStatics.GetAddressOf());
  if (SUCCEEDED(hr)) {
    settingsPaneStatics->Show();
  }

  return hr;
}
