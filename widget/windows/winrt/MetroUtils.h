/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "nsPoint.h"
#include "WinUtils.h"
#include "nsRect.h"

#include "mozwrlbase.h"

#include <stdio.h>
#include <windows.foundation.h>
#include <windows.ui.viewmanagement.h>

// HRESULT checkers, these warn on failure in debug builds
#ifdef DEBUG
#define DebugLogHR(hr) LogHRESULT(hr)
#else
#define DebugLogHR(hr)
#endif
#define AssertHRESULT(hr)           \
  if (FAILED(hr)) {                 \
    DebugLogHR(hr);                 \
    return;                         \
  }
#define AssertRetHRESULT(hr, res)   \
  if (FAILED(hr)) {                 \
    DebugLogHR(hr);                 \
    return res;                     \
  }

// MS Point helpers
#define POINT_CEIL_X(position) (uint32_t)ceil(position.X)
#define POINT_CEIL_Y(position) (uint32_t)ceil(position.Y)

class nsIBrowserDOMWindow;
class nsIDOMWindow;

namespace mozilla {
namespace widget {
namespace winrt {

template<unsigned int size, typename T>
HRESULT ActivateGenericInstance(wchar_t const (&RuntimeClassName)[size], Microsoft::WRL::ComPtr<T>& aOutObject) {
  Microsoft::WRL::ComPtr<IActivationFactory> factory;
  HRESULT hr = ABI::Windows::Foundation::GetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(RuntimeClassName).Get(),
                                                              factory.GetAddressOf());
  if (FAILED(hr))
    return hr;
  Microsoft::WRL::ComPtr<IInspectable> inspect;
  hr = factory->ActivateInstance(inspect.GetAddressOf());
  if (FAILED(hr))
    return hr;
  return inspect.As(&aOutObject);
}

} } }

class MetroUtils
{
  typedef ABI::Windows::Foundation::IUriRuntimeClass IUriRuntimeClass;
  typedef Microsoft::WRL::Wrappers::HString HString;
  typedef ABI::Windows::UI::ViewManagement::ApplicationViewState ApplicationViewState;
  typedef ABI::Windows::Foundation::Point Point;
  typedef ABI::Windows::Foundation::Rect Rect;

public:
  // Functions to convert between logical pixels as used by most Windows APIs
  // and physical (device) pixels.
  static double LogToPhysFactor();
  static double PhysToLogFactor();
  static nsIntPoint LogToPhys(const Point& aPt);
  static nsIntRect LogToPhys(const Rect& aRect);
  static Point PhysToLog(const nsIntPoint& aPt);

  // Resolution scale factor
  static double ScaleFactor();

  static nsresult FireObserver(const char* aMessage, const char16_t* aData = nullptr);

  static HRESULT CreateUri(HSTRING aUriStr, Microsoft::WRL::ComPtr<IUriRuntimeClass>& aUriOut);
  static HRESULT CreateUri(HString& aHString, Microsoft::WRL::ComPtr<IUriRuntimeClass>& aUriOut);
  static HRESULT GetViewState(ApplicationViewState& aState);
  static HRESULT TryUnsnap(bool* aResult = nullptr);
  static HRESULT ShowSettingsFlyout();

private:
  static nsresult GetBrowserDOMWindow(nsCOMPtr<nsIBrowserDOMWindow> &aBWin);
  static nsresult GetMostRecentWindow(const char16_t* aType, nsIDOMWindow** aWindow);
};
