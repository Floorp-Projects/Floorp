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

// Conversion between logical and physical coordinates

double
MetroUtils::LogToPhysFactor()
{
  ComPtr<IDisplayInformationStatics> dispInfoStatics;
  if (SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayInformation).Get(),
                                      dispInfoStatics.GetAddressOf()))) {
    ComPtr<IDisplayInformation> dispInfo;
    if (SUCCEEDED(dispInfoStatics->GetForCurrentView(&dispInfo))) {
      FLOAT dpi;
      if (SUCCEEDED(dispInfo->get_LogicalDpi(&dpi))) {
        return (double)dpi / 96.0f;
      }
    }
  }

  ComPtr<IDisplayPropertiesStatics> dispProps;
  if (SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayProperties).Get(),
                                      dispProps.GetAddressOf()))) {
    FLOAT dpi;
    if (SUCCEEDED(dispProps->get_LogicalDpi(&dpi))) {
      return (double)dpi / 96.0f;
    }
  }

  return 1.0;
}

double
MetroUtils::PhysToLogFactor()
{
  return 1.0 / LogToPhysFactor();
}

double
MetroUtils::ScaleFactor()
{
  // Return the resolution scale factor reported by the metro environment.
  // XXX TODO: also consider the desktop resolution setting, as IE appears to do?
  ComPtr<IDisplayInformationStatics> dispInfoStatics;
  if (SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayInformation).Get(),
                                      dispInfoStatics.GetAddressOf()))) {
    ComPtr<IDisplayInformation> dispInfo;
    if (SUCCEEDED(dispInfoStatics->GetForCurrentView(&dispInfo))) {
      ResolutionScale scale;
      if (SUCCEEDED(dispInfo->get_ResolutionScale(&scale))) {
        return (double)scale / 100.0;
      }
    }
  }

  ComPtr<IDisplayPropertiesStatics> dispProps;
  if (SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayProperties).Get(),
                                     dispProps.GetAddressOf()))) {
    ResolutionScale scale;
    if (SUCCEEDED(dispProps->get_ResolutionScale(&scale))) {
      return (double)scale / 100.0;
    }
  }

  return 1.0;
}

nsIntPoint
MetroUtils::LogToPhys(const Point& aPt)
{
  double factor = LogToPhysFactor();
  return nsIntPoint(int32_t(NS_round(aPt.X * factor)), int32_t(NS_round(aPt.Y * factor)));
}

nsIntRect
MetroUtils::LogToPhys(const Rect& aRect)
{
  double factor = LogToPhysFactor();
  return nsIntRect(int32_t(NS_round(aRect.X * factor)),
                   int32_t(NS_round(aRect.Y * factor)),
                   int32_t(NS_round(aRect.Width * factor)),
                   int32_t(NS_round(aRect.Height * factor)));
}

Point
MetroUtils::PhysToLog(const nsIntPoint& aPt)
{
  // Points contain FLOATs
  FLOAT factor = (FLOAT)PhysToLogFactor();
  Point p = { FLOAT(aPt.x) * factor, FLOAT(aPt.y) * factor };
  return p;
}

nsresult
MetroUtils::FireObserver(const char* aMessage, const char16_t* aData)
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
