/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define NTDDI_VERSION NTDDI_WIN10_RS1

#include "OSKInputPaneManager.h"
#include "nsDebug.h"

#ifndef __MINGW32__
#  include <inputpaneinterop.h>
#  include <windows.ui.viewmanagement.h>
#  include <wrl.h>

using namespace ABI::Windows::UI::ViewManagement;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
#endif

namespace mozilla {
namespace widget {

#ifndef __MINGW32__
static ComPtr<IInputPane2> GetInputPane(HWND aHwnd) {
  ComPtr<IInputPaneInterop> inputPaneInterop;
  HRESULT hr = RoGetActivationFactory(
      HStringReference(RuntimeClass_Windows_UI_ViewManagement_InputPane).Get(),
      IID_PPV_ARGS(&inputPaneInterop));
  if (NS_WARN_IF(FAILED(hr))) {
    return nullptr;
  }

  ComPtr<IInputPane> inputPane;
  hr = inputPaneInterop->GetForWindow(aHwnd, IID_PPV_ARGS(&inputPane));
  if (NS_WARN_IF(FAILED(hr))) {
    return nullptr;
  }

  ComPtr<IInputPane2> inputPane2;
  inputPane.As(&inputPane2);
  return inputPane2;
}
#endif

// static
void OSKInputPaneManager::ShowOnScreenKeyboard(HWND aWnd) {
#ifndef __MINGW32__
  ComPtr<IInputPane2> inputPane2 = GetInputPane(aWnd);
  if (!inputPane2) {
    return;
  }
  boolean result;
  inputPane2->TryShow(&result);
  NS_WARNING_ASSERTION(result, "IInputPane2::TryShow is faiure");
#endif
}

// static
void OSKInputPaneManager::DismissOnScreenKeyboard(HWND aWnd) {
#ifndef __MINGW32__
  ComPtr<IInputPane2> inputPane2 = GetInputPane(aWnd);
  if (!inputPane2) {
    return;
  }
  boolean result;
  inputPane2->TryHide(&result);
  NS_WARNING_ASSERTION(result, "IInputPane2::TryHide is failure");
#endif
}

}  // namespace widget
}  // namespace mozilla
