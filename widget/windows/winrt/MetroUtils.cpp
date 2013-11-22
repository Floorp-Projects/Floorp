/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsRect.h"

#include <wrl/wrappers/corewrappers.h>
#include <windows.ui.applicationsettings.h>
#include <windows.graphics.display.h>
#include "DisplayInfo_sdk81.h"

using namespace ABI::Windows::UI::ApplicationSettings;

using namespace mozilla;

using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI::ViewManagement;
using namespace ABI::Windows::Graphics::Display;

// File-scoped statics (unnamed namespace)
namespace {
  FLOAT LogToPhysFactor() {
    ComPtr<IDisplayInformationStatics> dispInfoStatics;
    if (SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayInformation).Get(),
                                       dispInfoStatics.GetAddressOf()))) {
      ComPtr<IDisplayInformation> dispInfo;
      if (SUCCEEDED(dispInfoStatics->GetForCurrentView(&dispInfo))) {
        FLOAT dpi;
        if (SUCCEEDED(dispInfo->get_LogicalDpi(&dpi))) {
          return dpi / 96.0f;
        }
      }
    }

    ComPtr<IDisplayPropertiesStatics> dispProps;
    if (SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayProperties).Get(),
                                       dispProps.GetAddressOf()))) {
      FLOAT dpi;
      if (SUCCEEDED(dispProps->get_LogicalDpi(&dpi))) {
        return dpi / 96.0f;
      }
    }

    return 1.0f;
  }

  FLOAT PhysToLogFactor() {
    return 1.0f / LogToPhysFactor();
  }
};

// Conversion between logical and physical coordinates
int32_t
MetroUtils::LogToPhys(FLOAT aValue)
{
  return int32_t(NS_round(aValue * LogToPhysFactor()));
}

nsIntPoint
MetroUtils::LogToPhys(const Point& aPt)
{
  FLOAT factor = LogToPhysFactor();
  return nsIntPoint(int32_t(NS_round(aPt.X * factor)), int32_t(NS_round(aPt.Y * factor)));
}

nsIntRect
MetroUtils::LogToPhys(const Rect& aRect)
{
  FLOAT factor = LogToPhysFactor();
  return nsIntRect(int32_t(NS_round(aRect.X * factor)),
                   int32_t(NS_round(aRect.Y * factor)),
                   int32_t(NS_round(aRect.Width * factor)),
                   int32_t(NS_round(aRect.Height * factor)));
}

FLOAT
MetroUtils::PhysToLog(int32_t aValue)
{
  return FLOAT(aValue) * PhysToLogFactor();
}

Point
MetroUtils::PhysToLog(const nsIntPoint& aPt)
{
  FLOAT factor = PhysToLogFactor();
  Point p = { FLOAT(aPt.x) * factor, FLOAT(aPt.y) * factor };
  return p;
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
