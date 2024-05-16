/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define NTDDI_VERSION NTDDI_WIN10_RS1

#include "OSKInputPaneManager.h"
#include "mozilla/Maybe.h"
#include "nscore.h"
#include "nsDebug.h"
#include "nsWindowsHelpers.h"

#ifndef __MINGW32__
#  include <inputpaneinterop.h>
#  include <windows.ui.viewmanagement.h>
#  include <wrl.h>

using ABI::Windows::Foundation::ITypedEventHandler;
using namespace ABI::Windows::UI::ViewManagement;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
#endif

namespace mozilla {
namespace widget {

#ifndef __MINGW32__
static ComPtr<IInputPane> GetInputPaneInternal(HWND aHwnd) {
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

  return inputPane;
}

static ComPtr<IInputPane2> GetInputPane(HWND aHwnd) {
  ComPtr<IInputPane> inputPane = GetInputPaneInternal(aHwnd);
  if (!inputPane) {
    return nullptr;
  }

  // Bug 1645571: We need to ensure that we have a SID_InputPaneEventHandler
  // window service registered on aHwnd, or explorer.exe will mess with our
  // window through twinui!CKeyboardOcclusionMitigation::_MitigateWindow.

  Maybe<bool> hasEventHandler =
      OSKInputPaneManager::HasInputPaneEventHandlerService(aHwnd);
  if (hasEventHandler.isSome() && !hasEventHandler.value()) {
    // Run IInputPane::add_Hiding once to register the window service.
    EventRegistrationToken token{};
    HRESULT registered = inputPane->add_Hiding(
        Callback<ITypedEventHandler<InputPane*, InputPaneVisibilityEventArgs*>>(
            [](IInputPane* aInputPane, IInputPaneVisibilityEventArgs* aArgs) {
              return S_OK;
            })
            .Get(),
        &token);

    // Validate our assumption that we now have the window service registered.
    hasEventHandler =
        OSKInputPaneManager::HasInputPaneEventHandlerService(aHwnd);
    if (SUCCEEDED(registered) &&
        !(hasEventHandler.isSome() && hasEventHandler.value())) {
      // If our assumption is wrong, we undo the operation. This prevents a
      // memory leak where we would be adding a new callback every time the
      // on-screen keyboard is shown.
      inputPane->remove_Hiding(token);
    }
  }

  ComPtr<IInputPane2> inputPane2;
  inputPane.As(&inputPane2);
  return inputPane2;
}

#  ifdef DEBUG
static bool IsInputPaneVisible(ComPtr<IInputPane2>& aInputPane2) {
  ComPtr<IInputPaneControl> inputPaneControl;
  aInputPane2.As(&inputPaneControl);
  if (NS_WARN_IF(!inputPaneControl)) {
    return false;
  }
  boolean visible = false;
  inputPaneControl->get_Visible(&visible);
  return visible;
}

static bool IsForegroundApp() {
  HWND foregroundWnd = ::GetForegroundWindow();
  if (!foregroundWnd) {
    return false;
  }
  DWORD pid;
  ::GetWindowThreadProcessId(foregroundWnd, &pid);
  return pid == ::GetCurrentProcessId();
}
#  endif
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
  NS_WARNING_ASSERTION(
      result || !IsForegroundApp() || IsInputPaneVisible(inputPane2),
      "IInputPane2::TryShow is failure");
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
  NS_WARNING_ASSERTION(
      result || !IsForegroundApp() || !IsInputPaneVisible(inputPane2),
      "IInputPane2::TryHide is failure");
#endif
}

// static
Maybe<bool> OSKInputPaneManager::HasInputPaneEventHandlerService(HWND aHwnd) {
  using CoreIsWindowServiceSupportedFn =
      HRESULT(WINAPI*)(HWND aHwnd, LPCGUID aServiceSid);

  static auto sCoreIsWindowServiceSupported =
      []() -> CoreIsWindowServiceSupportedFn {
    HMODULE twinApiAppCore = LoadLibrarySystem32(L"twinapi.appcore.dll");
    if (!twinApiAppCore) {
      return nullptr;
    }
    return reinterpret_cast<CoreIsWindowServiceSupportedFn>(
        ::GetProcAddress(twinApiAppCore, reinterpret_cast<LPCSTR>(8)));
  }();

  if (!sCoreIsWindowServiceSupported) {
    return Nothing();
  }

  // {958e2b85-b035-4590-9bc5-ef01779b45dc}
  static const GUID sSID_InputPaneEventHandler{
      0x958e2b85,
      0xb035,
      0x4590,
      {0x9b, 0xc5, 0xef, 0x1, 0x77, 0x9b, 0x45, 0xdc}};

  // Since we are calling a Windows internal function here, the function
  // signature is subject to arbitrary changes from Microsoft. This could
  // result in wrong results or even crashing. Ensure that if things are so bad
  // that we are about to crash, we just return Nothing instead.
  MOZ_SEH_TRY {
    return Some(SUCCEEDED(
        sCoreIsWindowServiceSupported(aHwnd, &sSID_InputPaneEventHandler)));
  }
  MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) { return Nothing(); }
}

}  // namespace widget
}  // namespace mozilla
