/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinIMEHandler.h"

#include "IMMHandler.h"
#include "mozilla/Preferences.h"
#include "nsWindowDefs.h"

#ifdef NS_ENABLE_TSF
#include "TSFTextStore.h"
#endif // #ifdef NS_ENABLE_TSF

#include "nsLookAndFeel.h"
#include "nsWindow.h"
#include "WinUtils.h"
#include "nsIWindowsRegKey.h"
#include "nsIWindowsUIUtils.h"

#include "shellapi.h"
#include "shlobj.h"
#include "powrprof.h"
#include "setupapi.h"
#include "cfgmgr32.h"

const char* kOskPathPrefName = "ui.osk.on_screen_keyboard_path";
const char* kOskEnabled = "ui.osk.enabled";
const char* kOskDetectPhysicalKeyboard = "ui.osk.detect_physical_keyboard";
const char* kOskRequireTabletMode = "ui.osk.require_tablet_mode";

namespace mozilla {
namespace widget {

/******************************************************************************
 * IMEHandler
 ******************************************************************************/

#ifdef NS_ENABLE_TSF
bool IMEHandler::sIsInTSFMode = false;
bool IMEHandler::sIsIMMEnabled = true;
bool IMEHandler::sPluginHasFocus = false;
bool IMEHandler::sShowingOnScreenKeyboard = false;
decltype(SetInputScopes)* IMEHandler::sSetInputScopes = nullptr;
#endif // #ifdef NS_ENABLE_TSF

// static
void
IMEHandler::Initialize()
{
#ifdef NS_ENABLE_TSF
  TSFTextStore::Initialize();
  sIsInTSFMode = TSFTextStore::IsInTSFMode();
  sIsIMMEnabled =
    !sIsInTSFMode || Preferences::GetBool("intl.tsf.support_imm", true);
  if (!sIsInTSFMode) {
    // When full TSFTextStore is not available, try to use SetInputScopes API
    // to enable at least InputScope. Use GET_MODULE_HANDLE_EX_FLAG_PIN to
    // ensure that msctf.dll will not be unloaded.
    HMODULE module = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"msctf.dll",
                           &module)) {
      sSetInputScopes = reinterpret_cast<decltype(SetInputScopes)*>(
        GetProcAddress(module, "SetInputScopes"));
    }
  }
#endif // #ifdef NS_ENABLE_TSF

  IMMHandler::Initialize();
}

// static
void
IMEHandler::Terminate()
{
#ifdef NS_ENABLE_TSF
  if (sIsInTSFMode) {
    TSFTextStore::Terminate();
    sIsInTSFMode = false;
  }
#endif // #ifdef NS_ENABLE_TSF

  IMMHandler::Terminate();
}

// static
void*
IMEHandler::GetNativeData(uint32_t aDataType)
{
#ifdef NS_ENABLE_TSF
  void* result = TSFTextStore::GetNativeData(aDataType);
  if (!result || !(*(static_cast<void**>(result)))) {
    return nullptr;
  }
  // XXX During the TSF module test, sIsInTSFMode must be true.  After that,
  //     the value should be restored but currently, there is no way for that.
  //     When the TSF test is enabled again, we need to fix this.  Perhaps,
  //     sending a message can fix this.
  sIsInTSFMode = true;
  return result;
#else // #ifdef NS_ENABLE_TSF
  return nullptr;
#endif // #ifdef NS_ENABLE_TSF #else
}

// static
bool
IMEHandler::ProcessRawKeyMessage(const MSG& aMsg)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    return TSFTextStore::ProcessRawKeyMessage(aMsg);
  }
#endif // #ifdef NS_ENABLE_TSF
  return false; // noting to do in IMM mode.
}

// static
bool
IMEHandler::ProcessMessage(nsWindow* aWindow, UINT aMessage,
                           WPARAM& aWParam, LPARAM& aLParam,
                           MSGResult& aResult)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    TSFTextStore::ProcessMessage(aWindow, aMessage, aWParam, aLParam, aResult);
    if (aResult.mConsumed) {
      return true;
    }
    // If we don't support IMM in TSF mode, we don't use IMMHandler.
    if (!sIsIMMEnabled) {
      return false;
    }
    // IME isn't implemented with IMM, IMMHandler shouldn't handle any
    // messages.
    if (!TSFTextStore::IsIMM_IME()) {
      return false;
    }
  }
#endif // #ifdef NS_ENABLE_TSF

  return IMMHandler::ProcessMessage(aWindow, aMessage, aWParam, aLParam,
                                    aResult);
}

#ifdef NS_ENABLE_TSF
// static
bool
IMEHandler::IsIMMActive()
{
  return TSFTextStore::IsIMM_IME();
}
#endif // #ifdef NS_ENABLE_TSF

// static
bool
IMEHandler::IsComposing()
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    return TSFTextStore::IsComposing() || IMMHandler::IsComposing();
  }
#endif // #ifdef NS_ENABLE_TSF

  return IMMHandler::IsComposing();
}

// static
bool
IMEHandler::IsComposingOn(nsWindow* aWindow)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    return TSFTextStore::IsComposingOn(aWindow) ||
           IMMHandler::IsComposingOn(aWindow);
  }
#endif // #ifdef NS_ENABLE_TSF

  return IMMHandler::IsComposingOn(aWindow);
}

// static
nsresult
IMEHandler::NotifyIME(nsWindow* aWindow,
                      const IMENotification& aIMENotification)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    switch (aIMENotification.mMessage) {
      case NOTIFY_IME_OF_SELECTION_CHANGE: {
        nsresult rv = TSFTextStore::OnSelectionChange(aIMENotification);
        // If IMM IME is active, we need to notify IMMHandler of updating
        // composition change.  It will adjust candidate window position or
        // composition window position.
        bool isIMMActive = IsIMMActive();
        if (isIMMActive) {
          IMMHandler::OnUpdateComposition(aWindow);
        }
        IMMHandler::OnSelectionChange(aWindow, aIMENotification, isIMMActive);
        return rv;
      }
      case NOTIFY_IME_OF_COMPOSITION_UPDATE:
        // If IMM IME is active, we need to notify IMMHandler of updating
        // composition change.  It will adjust candidate window position or
        // composition window position.
        if (IsIMMActive()) {
          IMMHandler::OnUpdateComposition(aWindow);
        } else {
          TSFTextStore::OnUpdateComposition();
        }
        return NS_OK;
      case NOTIFY_IME_OF_TEXT_CHANGE:
        return TSFTextStore::OnTextChange(aIMENotification);
      case NOTIFY_IME_OF_FOCUS: {
        IMMHandler::OnFocusChange(true, aWindow);
        nsresult rv =
          TSFTextStore::OnFocusChange(true, aWindow,
                                      aWindow->GetInputContext());
        IMEHandler::MaybeShowOnScreenKeyboard();
        return rv;
      }
      case NOTIFY_IME_OF_BLUR:
        IMEHandler::MaybeDismissOnScreenKeyboard();
        IMMHandler::OnFocusChange(false, aWindow);
        return TSFTextStore::OnFocusChange(false, aWindow,
                                           aWindow->GetInputContext());
      case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
        // If IMM IME is active, we should send a mouse button event via IMM.
        if (IsIMMActive()) {
          return IMMHandler::OnMouseButtonEvent(aWindow, aIMENotification);
        }
        return TSFTextStore::OnMouseButtonEvent(aIMENotification);
      case REQUEST_TO_COMMIT_COMPOSITION:
        if (TSFTextStore::IsComposingOn(aWindow)) {
          TSFTextStore::CommitComposition(false);
        } else if (IsIMMActive()) {
          IMMHandler::CommitComposition(aWindow);
        }
        return NS_OK;
      case REQUEST_TO_CANCEL_COMPOSITION:
        if (TSFTextStore::IsComposingOn(aWindow)) {
          TSFTextStore::CommitComposition(true);
        } else if (IsIMMActive()) {
          IMMHandler::CancelComposition(aWindow);
        }
        return NS_OK;
      case NOTIFY_IME_OF_POSITION_CHANGE:
        return TSFTextStore::OnLayoutChange();
      default:
        return NS_ERROR_NOT_IMPLEMENTED;
    }
  }
#endif //NS_ENABLE_TSF

  switch (aIMENotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      IMMHandler::CommitComposition(aWindow);
      return NS_OK;
    case REQUEST_TO_CANCEL_COMPOSITION:
      IMMHandler::CancelComposition(aWindow);
      return NS_OK;
    case NOTIFY_IME_OF_POSITION_CHANGE:
    case NOTIFY_IME_OF_COMPOSITION_UPDATE:
      IMMHandler::OnUpdateComposition(aWindow);
      return NS_OK;
    case NOTIFY_IME_OF_SELECTION_CHANGE:
      IMMHandler::OnSelectionChange(aWindow, aIMENotification, true);
      return NS_OK;
    case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
      return IMMHandler::OnMouseButtonEvent(aWindow, aIMENotification);
    case NOTIFY_IME_OF_FOCUS:
      IMMHandler::OnFocusChange(true, aWindow);
      IMEHandler::MaybeShowOnScreenKeyboard();
      return NS_OK;
    case NOTIFY_IME_OF_BLUR:
      IMEHandler::MaybeDismissOnScreenKeyboard();
      IMMHandler::OnFocusChange(false, aWindow);
#ifdef NS_ENABLE_TSF
      // If a plugin gets focus while TSF has focus, we need to notify TSF of
      // the blur.
      if (TSFTextStore::ThinksHavingFocus()) {
        return TSFTextStore::OnFocusChange(false, aWindow,
                                           aWindow->GetInputContext());
      }
#endif //NS_ENABLE_TSF
      return NS_OK;
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

// static
nsIMEUpdatePreference
IMEHandler::GetUpdatePreference()
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    return TSFTextStore::GetIMEUpdatePreference();
  }
#endif //NS_ENABLE_TSF

  return IMMHandler::GetIMEUpdatePreference();
}

// static
bool
IMEHandler::GetOpenState(nsWindow* aWindow)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable() && !IsIMMActive()) {
    return TSFTextStore::GetIMEOpenState();
  }
#endif //NS_ENABLE_TSF

  IMEContext context(aWindow);
  return context.GetOpenState();
}

// static
void
IMEHandler::OnDestroyWindow(nsWindow* aWindow)
{
#ifdef NS_ENABLE_TSF
  // We need to do nothing here for TSF. Just restore the default context
  // if it's been disassociated.
  if (!sIsInTSFMode) {
    // MSDN says we need to set IS_DEFAULT to avoid memory leak when we use
    // SetInputScopes API. Use an empty string to do this.
    SetInputScopeForIMM32(aWindow, EmptyString());
  }
#endif // #ifdef NS_ENABLE_TSF
  AssociateIMEContext(aWindow, true);
}

// static
void
IMEHandler::SetInputContext(nsWindow* aWindow,
                            InputContext& aInputContext,
                            const InputContextAction& aAction)
{
  // FYI: If there is no composition, this call will do nothing.
  NotifyIME(aWindow, IMENotification(REQUEST_TO_COMMIT_COMPOSITION));

  const InputContext& oldInputContext = aWindow->GetInputContext();

  // Assume that SetInputContext() is called only when aWindow has focus.
  sPluginHasFocus = (aInputContext.mIMEState.mEnabled == IMEState::PLUGIN);

  bool enable = WinUtils::IsIMEEnabled(aInputContext);
  bool adjustOpenState = (enable &&
    aInputContext.mIMEState.mOpen != IMEState::DONT_CHANGE_OPEN_STATE);
  bool open = (adjustOpenState &&
    aInputContext.mIMEState.mOpen == IMEState::OPEN);

  aInputContext.mNativeIMEContext = nullptr;

#ifdef NS_ENABLE_TSF
  // Note that even while a plugin has focus, we need to notify TSF of that.
  if (sIsInTSFMode) {
    TSFTextStore::SetInputContext(aWindow, aInputContext, aAction);
    if (IsTSFAvailable()) {
      aInputContext.mNativeIMEContext = TSFTextStore::GetThreadManager();
      if (sIsIMMEnabled) {
        // Associate IME context for IMM-IMEs.
        AssociateIMEContext(aWindow, enable);
      } else if (oldInputContext.mIMEState.mEnabled == IMEState::PLUGIN) {
        // Disassociate the IME context from the window when plugin loses focus
        // in pure TSF mode.
        AssociateIMEContext(aWindow, false);
      }
      if (adjustOpenState) {
        TSFTextStore::SetIMEOpenState(open);
      }
      return;
    }
  } else {
    // Set at least InputScope even when TextStore is not available.
    SetInputScopeForIMM32(aWindow, aInputContext.mHTMLInputType);
  }
#endif // #ifdef NS_ENABLE_TSF

  AssociateIMEContext(aWindow, enable);

  IMEContext context(aWindow);
  if (adjustOpenState) {
    context.SetOpenState(open);
  }

  if (aInputContext.mNativeIMEContext) {
    return;
  }

  // The old InputContext must store the default IMC or old TextStore.
  // When IME context is disassociated from the window, use it.
  aInputContext.mNativeIMEContext = enable ?
    static_cast<void*>(context.get()) : oldInputContext.mNativeIMEContext;
}

// static
void
IMEHandler::AssociateIMEContext(nsWindow* aWindow, bool aEnable)
{
  IMEContext context(aWindow);
  if (aEnable) {
    context.AssociateDefaultContext();
    return;
  }
  // Don't disassociate the context after the window is destroyed.
  if (aWindow->Destroyed()) {
    return;
  }
  context.Disassociate();
}

// static
void
IMEHandler::InitInputContext(nsWindow* aWindow, InputContext& aInputContext)
{
  // For a11y, the default enabled state should be 'enabled'.
  aInputContext.mIMEState.mEnabled = IMEState::ENABLED;

#ifdef NS_ENABLE_TSF
  if (sIsInTSFMode) {
    TSFTextStore::SetInputContext(aWindow, aInputContext,
      InputContextAction(InputContextAction::CAUSE_UNKNOWN,
                         InputContextAction::GOT_FOCUS));
    aInputContext.mNativeIMEContext = TSFTextStore::GetThreadManager();
    MOZ_ASSERT(aInputContext.mNativeIMEContext);
    // IME context isn't necessary in pure TSF mode.
    if (!sIsIMMEnabled) {
      AssociateIMEContext(aWindow, false);
    }
    return;
  }
#endif // #ifdef NS_ENABLE_TSF

  // NOTE: mNativeIMEContext may be null if IMM module isn't installed.
  IMEContext context(aWindow);
  aInputContext.mNativeIMEContext = static_cast<void*>(context.get());
  MOZ_ASSERT(aInputContext.mNativeIMEContext || !CurrentKeyboardLayoutHasIME());
  // If no IME context is available, we should set the widget's pointer since
  // nullptr indicates there is only one context per process on the platform.
  if (!aInputContext.mNativeIMEContext) {
    aInputContext.mNativeIMEContext = static_cast<void*>(aWindow);
  }
}

#ifdef DEBUG
// static
bool
IMEHandler::CurrentKeyboardLayoutHasIME()
{
#ifdef NS_ENABLE_TSF
  if (sIsInTSFMode) {
    return TSFTextStore::CurrentKeyboardLayoutHasIME();
  }
#endif // #ifdef NS_ENABLE_TSF

  return IMMHandler::IsIMEAvailable();
}
#endif // #ifdef DEBUG

// static
void
IMEHandler::SetInputScopeForIMM32(nsWindow* aWindow,
                                  const nsAString& aHTMLInputType)
{
  if (sIsInTSFMode || !sSetInputScopes || aWindow->Destroyed()) {
    return;
  }
  UINT arraySize = 0;
  const InputScope* scopes = nullptr;
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-input-element.html
  if (aHTMLInputType.IsEmpty() || aHTMLInputType.EqualsLiteral("text")) {
    static const InputScope inputScopes[] = { IS_DEFAULT };
    scopes = &inputScopes[0];
    arraySize = ArrayLength(inputScopes);
  } else if (aHTMLInputType.EqualsLiteral("url")) {
    static const InputScope inputScopes[] = { IS_URL };
    scopes = &inputScopes[0];
    arraySize = ArrayLength(inputScopes);
  } else if (aHTMLInputType.EqualsLiteral("search")) {
    static const InputScope inputScopes[] = { IS_SEARCH };
    scopes = &inputScopes[0];
    arraySize = ArrayLength(inputScopes);
  } else if (aHTMLInputType.EqualsLiteral("email")) {
    static const InputScope inputScopes[] = { IS_EMAIL_SMTPEMAILADDRESS };
    scopes = &inputScopes[0];
    arraySize = ArrayLength(inputScopes);
  } else if (aHTMLInputType.EqualsLiteral("password")) {
    static const InputScope inputScopes[] = { IS_PASSWORD };
    scopes = &inputScopes[0];
    arraySize = ArrayLength(inputScopes);
  } else if (aHTMLInputType.EqualsLiteral("datetime") ||
             aHTMLInputType.EqualsLiteral("datetime-local")) {
    static const InputScope inputScopes[] = {
      IS_DATE_FULLDATE, IS_TIME_FULLTIME };
    scopes = &inputScopes[0];
    arraySize = ArrayLength(inputScopes);
  } else if (aHTMLInputType.EqualsLiteral("date") ||
             aHTMLInputType.EqualsLiteral("month") ||
             aHTMLInputType.EqualsLiteral("week")) {
    static const InputScope inputScopes[] = { IS_DATE_FULLDATE };
    scopes = &inputScopes[0];
    arraySize = ArrayLength(inputScopes);
  } else if (aHTMLInputType.EqualsLiteral("time")) {
    static const InputScope inputScopes[] = { IS_TIME_FULLTIME };
    scopes = &inputScopes[0];
    arraySize = ArrayLength(inputScopes);
  } else if (aHTMLInputType.EqualsLiteral("tel")) {
    static const InputScope inputScopes[] = {
      IS_TELEPHONE_FULLTELEPHONENUMBER, IS_TELEPHONE_LOCALNUMBER };
    scopes = &inputScopes[0];
    arraySize = ArrayLength(inputScopes);
  } else if (aHTMLInputType.EqualsLiteral("number")) {
    static const InputScope inputScopes[] = { IS_NUMBER };
    scopes = &inputScopes[0];
    arraySize = ArrayLength(inputScopes);
  }
  if (scopes && arraySize > 0) {
    sSetInputScopes(aWindow->GetWindowHandle(), scopes, arraySize, nullptr, 0,
                    nullptr, nullptr);
  }
}

// static
void
IMEHandler::MaybeShowOnScreenKeyboard()
{
  if (sPluginHasFocus ||
      !IsWin10OrLater() ||
      !Preferences::GetBool(kOskEnabled, true) ||
      sShowingOnScreenKeyboard ||
      IMEHandler::IsKeyboardPresentOnSlate()) {
    return;
  }

  // Tablet Mode is only supported on Windows 10 and higher.
  // When touch-event detection within IME is better supported
  // this check may be removed, and ShowOnScreenKeyboard can
  // run on Windows 8 and higher (adjusting the IsWin10OrLater
  // guard above and within MaybeDismissOnScreenKeyboard).
  if (!IsInTabletMode() &&
      Preferences::GetBool(kOskRequireTabletMode, true) &&
      !AutoInvokeOnScreenKeyboardInDesktopMode()) {
    return;
  }

 IMEHandler::ShowOnScreenKeyboard();
}

// static
void
IMEHandler::MaybeDismissOnScreenKeyboard()
{
  if (sPluginHasFocus ||
      !IsWin10OrLater() ||
      !sShowingOnScreenKeyboard) {
    return;
  }

  IMEHandler::DismissOnScreenKeyboard();
}

// static
bool
IMEHandler::WStringStartsWithCaseInsensitive(const std::wstring& aHaystack,
                                             const std::wstring& aNeedle)
{
  std::wstring lowerCaseHaystack(aHaystack);
  std::wstring lowerCaseNeedle(aNeedle);
  std::transform(lowerCaseHaystack.begin(), lowerCaseHaystack.end(),
                 lowerCaseHaystack.begin(), ::tolower);
  std::transform(lowerCaseNeedle.begin(), lowerCaseNeedle.end(),
                 lowerCaseNeedle.begin(), ::tolower);
  return wcsstr(lowerCaseHaystack.c_str(),
                lowerCaseNeedle.c_str()) == lowerCaseHaystack.c_str();
}

// Returns true if a physical keyboard is detected on Windows 8 and up.
// Uses the Setup APIs to enumerate the attached keyboards and returns true
// if the keyboard count is 1 or more. While this will work in most cases
// it won't work if there are devices which expose keyboard interfaces which
// are attached to the machine.
// Based on IsKeyboardPresentOnSlate() in Chromium's base/win/win_util.cc.
// static
bool
IMEHandler::IsKeyboardPresentOnSlate()
{
  // This function is only supported for Windows 8 and up.
  if (!IsWin8OrLater()) {
    return true;
  }

  if (!Preferences::GetBool(kOskDetectPhysicalKeyboard, true)) {
    // Detection for physical keyboard has been disabled for testing.
    return false;
  }

  // This function should be only invoked for machines with touch screens.
  if ((::GetSystemMetrics(SM_DIGITIZER) & NID_INTEGRATED_TOUCH)
        != NID_INTEGRATED_TOUCH) {
    return true;
  }

  // If the device is docked, the user is treating the device as a PC.
  if (::GetSystemMetrics(SM_SYSTEMDOCKED) != 0) {
    return true;
  }

  // To determine whether a keyboard is present on the device, we do the
  // following:-
  // 1. Check whether the device supports auto rotation. If it does then
  //    it possibly supports flipping from laptop to slate mode. If it
  //    does not support auto rotation, then we assume it is a desktop
  //    or a normal laptop and assume that there is a keyboard.

  // 2. If the device supports auto rotation, then we get its platform role
  //    and check the system metric SM_CONVERTIBLESLATEMODE to see if it is
  //    being used in slate mode. If yes then we return false here to ensure
  //    that the OSK is displayed.

  // 3. If step 1 and 2 fail then we check attached keyboards and return true
  //    if we find ACPI\* or HID\VID* keyboards.

  typedef BOOL (WINAPI* GetAutoRotationState)(PAR_STATE state);
  GetAutoRotationState get_rotation_state =
    reinterpret_cast<GetAutoRotationState>(::GetProcAddress(
      ::GetModuleHandleW(L"user32.dll"), "GetAutoRotationState"));

  if (get_rotation_state) {
    AR_STATE auto_rotation_state = AR_ENABLED;
    get_rotation_state(&auto_rotation_state);
    if ((auto_rotation_state & AR_NOSENSOR) ||
        (auto_rotation_state & AR_NOT_SUPPORTED)) {
      // If there is no auto rotation sensor or rotation is not supported in
      // the current configuration, then we can assume that this is a desktop
      // or a traditional laptop.
      return true;
    }
  }

  // Check if the device is being used as a laptop or a tablet. This can be
  // checked by first checking the role of the device and then the
  // corresponding system metric (SM_CONVERTIBLESLATEMODE). If it is being
  // used as a tablet then we want the OSK to show up.
  typedef POWER_PLATFORM_ROLE (WINAPI* PowerDeterminePlatformRole)();
  PowerDeterminePlatformRole power_determine_platform_role =
    reinterpret_cast<PowerDeterminePlatformRole>(::GetProcAddress(
      ::LoadLibraryW(L"PowrProf.dll"), "PowerDeterminePlatformRole"));
  if (power_determine_platform_role) {
    POWER_PLATFORM_ROLE role = power_determine_platform_role();
    if (((role == PlatformRoleMobile) || (role == PlatformRoleSlate)) &&
         (::GetSystemMetrics(SM_CONVERTIBLESLATEMODE) == 0)) {
      return false;
    }
  }

  const GUID KEYBOARD_CLASS_GUID =
    { 0x4D36E96B, 0xE325,  0x11CE,
      { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 } };

  // Query for all the keyboard devices.
  HDEVINFO device_info =
    ::SetupDiGetClassDevs(&KEYBOARD_CLASS_GUID, nullptr,
                          nullptr, DIGCF_PRESENT);
  if (device_info == INVALID_HANDLE_VALUE) {
    return false;
  }

  // Enumerate all keyboards and look for ACPI\PNP and HID\VID devices. If
  // the count is more than 1 we assume that a keyboard is present. This is
  // under the assumption that there will always be one keyboard device.
  for (DWORD i = 0;; ++i) {
    SP_DEVINFO_DATA device_info_data = { 0 };
    device_info_data.cbSize = sizeof(device_info_data);
    if (!::SetupDiEnumDeviceInfo(device_info, i, &device_info_data)) {
      break;
    }

    // Get the device ID.
    wchar_t device_id[MAX_DEVICE_ID_LEN];
    CONFIGRET status = ::CM_Get_Device_ID(device_info_data.DevInst,
                                          device_id,
                                          MAX_DEVICE_ID_LEN,
                                          0);
    if (status == CR_SUCCESS) {
      // To reduce the scope of the hack we only look for ACPI and HID\\VID
      // prefixes in the keyboard device ids.
      if (IMEHandler::WStringStartsWithCaseInsensitive(device_id,
                                                       L"ACPI") ||
          IMEHandler::WStringStartsWithCaseInsensitive(device_id,
                                                       L"HID\\VID")) {
        // The heuristic we are using is to check the count of keyboards and
        // return true if the API's report one or more keyboards. Please note
        // that this will break for non keyboard devices which expose a
        // keyboard PDO.
        return true;
      }
    }
  }
  return false;
}

// static
bool
IMEHandler::IsInTabletMode()
{
  nsCOMPtr<nsIWindowsUIUtils>
    uiUtils(do_GetService("@mozilla.org/windows-ui-utils;1"));
  if (NS_WARN_IF(!uiUtils)) {
    return false;
  }
  bool isInTabletMode = false;
  uiUtils->GetInTabletMode(&isInTabletMode);
  return isInTabletMode;
}

// static
bool
IMEHandler::AutoInvokeOnScreenKeyboardInDesktopMode()
{
  nsresult rv;
  nsCOMPtr<nsIWindowsRegKey> regKey
    (do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  rv = regKey->Open(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                    NS_LITERAL_STRING("SOFTWARE\\Microsoft\\TabletTip\\1.7"),
                    nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) {
    return false;
  }
  // EnableDesktopModeAutoInvoke is an opt-in option from the Windows
  // Settings to "Automatically show the touch keyboard in windowed apps
  // when there's no keyboard attached to your device." If the user has
  // opted-in to this behavior, the tablet-mode requirement is skipped.
  uint32_t value;
  rv = regKey->ReadIntValue(NS_LITERAL_STRING("EnableDesktopModeAutoInvoke"),
                            &value);
  if (NS_FAILED(rv)) {
    return false;
  }
  return !!value;
}

// Based on DisplayVirtualKeyboard() in Chromium's base/win/win_util.cc.
// static
void
IMEHandler::ShowOnScreenKeyboard()
{
  nsAutoString cachedPath;
  nsresult result = Preferences::GetString(kOskPathPrefName, &cachedPath);
  if (NS_FAILED(result) || cachedPath.IsEmpty()) {
    wchar_t path[MAX_PATH];
    // The path to TabTip.exe is defined at the following registry key.
    // This is pulled out of the 64-bit registry hive directly.
    const wchar_t kRegKeyName[] =
      L"Software\\Classes\\CLSID\\"
      L"{054AAE20-4BEA-4347-8A35-64A533254A9D}\\LocalServer32";
    if (!WinUtils::GetRegistryKey(HKEY_LOCAL_MACHINE,
                                  kRegKeyName,
                                  nullptr,
                                  path,
                                  sizeof path)) {
      return;
    }

    std::wstring wstrpath(path);
    // The path provided by the registry will often contain
    // %CommonProgramFiles%, which will need to be replaced if it is present.
    size_t commonProgramFilesOffset = wstrpath.find(L"%CommonProgramFiles%");
    if (commonProgramFilesOffset != std::wstring::npos) {
      // The path read from the registry contains the %CommonProgramFiles%
      // environment variable prefix. On 64 bit Windows the
      // SHGetKnownFolderPath function returns the common program files path
      // with the X86 suffix for the FOLDERID_ProgramFilesCommon value.
      // To get the correct path to TabTip.exe we first read the environment
      // variable CommonProgramW6432 which points to the desired common
      // files path. Failing that we fallback to the SHGetKnownFolderPath API.

      // We then replace the %CommonProgramFiles% value with the actual common
      // files path found in the process.
      std::wstring commonProgramFilesPath;
      std::vector<wchar_t> commonProgramFilesPathW6432;
      DWORD bufferSize = ::GetEnvironmentVariableW(L"CommonProgramW6432",
                                                   nullptr, 0);
      if (bufferSize) {
        commonProgramFilesPathW6432.resize(bufferSize);
        ::GetEnvironmentVariableW(L"CommonProgramW6432",
                                  commonProgramFilesPathW6432.data(),
                                  bufferSize);
        commonProgramFilesPath =
          std::wstring(commonProgramFilesPathW6432.data());
      } else {
        PWSTR path = nullptr;
        HRESULT hres =
          WinUtils::SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0,
                                         nullptr, &path);
        if (FAILED(hres) || !path) {
          return;
        }
        commonProgramFilesPath =
          static_cast<const wchar_t*>(nsDependentString(path).get());
        ::CoTaskMemFree(path);
      }
      wstrpath.replace(commonProgramFilesOffset,
                       wcslen(L"%CommonProgramFiles%"),
                       commonProgramFilesPath);
    }

    cachedPath.Assign(wstrpath.data());
    Preferences::SetString(kOskPathPrefName, cachedPath);
  }

  const char16_t *cachedPathPtr;
  cachedPath.GetData(&cachedPathPtr);
  ShellExecuteW(nullptr,
                L"",
                char16ptr_t(cachedPathPtr),
                nullptr,
                nullptr,
                SW_SHOW);
  sShowingOnScreenKeyboard = true;
}

// Based on DismissVirtualKeyboard() in Chromium's base/win/win_util.cc.
// static
void
IMEHandler::DismissOnScreenKeyboard()
{
  sShowingOnScreenKeyboard = false;

  // Dismiss the virtual keyboard by generating the ESC keystroke
  // programmatically.
  const wchar_t kOSKClassName[] = L"IPTip_Main_Window";
  HWND osk = ::FindWindowW(kOSKClassName, nullptr);
  if (::IsWindow(osk) && ::IsWindowEnabled(osk)) {
    ::PostMessage(osk, WM_SYSCOMMAND, SC_CLOSE, 0);
  }
}

} // namespace widget
} // namespace mozilla
