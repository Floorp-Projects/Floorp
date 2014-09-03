/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinIMEHandler.h"

#include "mozilla/Preferences.h"
#include "nsIMM32Handler.h"
#include "nsWindowDefs.h"

#ifdef NS_ENABLE_TSF
#include "nsTextStore.h"
#endif // #ifdef NS_ENABLE_TSF

#include "nsWindow.h"
#include "WinUtils.h"

namespace mozilla {
namespace widget {

/******************************************************************************
 * IMEHandler
 ******************************************************************************/

#ifdef NS_ENABLE_TSF
bool IMEHandler::sIsInTSFMode = false;
bool IMEHandler::sIsIMMEnabled = true;
bool IMEHandler::sPluginHasFocus = false;
decltype(SetInputScopes)* IMEHandler::sSetInputScopes = nullptr;
#endif // #ifdef NS_ENABLE_TSF

// static
void
IMEHandler::Initialize()
{
#ifdef NS_ENABLE_TSF
  nsTextStore::Initialize();
  sIsInTSFMode = nsTextStore::IsInTSFMode();
  sIsIMMEnabled =
    !sIsInTSFMode || Preferences::GetBool("intl.tsf.support_imm", true);
  if (!sIsInTSFMode) {
    // When full nsTextStore is not available, try to use SetInputScopes API
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

  nsIMM32Handler::Initialize();
}

// static
void
IMEHandler::Terminate()
{
#ifdef NS_ENABLE_TSF
  if (sIsInTSFMode) {
    nsTextStore::Terminate();
    sIsInTSFMode = false;
  }
#endif // #ifdef NS_ENABLE_TSF

  nsIMM32Handler::Terminate();
}

// static
void*
IMEHandler::GetNativeData(uint32_t aDataType)
{
#ifdef NS_ENABLE_TSF
  void* result = nsTextStore::GetNativeData(aDataType);
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
    return nsTextStore::ProcessRawKeyMessage(aMsg);
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
    nsTextStore::ProcessMessage(aWindow, aMessage, aWParam, aLParam, aResult);
    if (aResult.mConsumed) {
      return true;
    }
    // If we don't support IMM in TSF mode, we don't use nsIMM32Handler.
    if (!sIsIMMEnabled) {
      return false;
    }
    // IME isn't implemented with IMM, nsIMM32Handler shouldn't handle any
    // messages.
    if (!nsTextStore::IsIMM_IME()) {
      return false;
    }
  }
#endif // #ifdef NS_ENABLE_TSF

  return nsIMM32Handler::ProcessMessage(aWindow, aMessage, aWParam, aLParam,
                                        aResult);
}

// static
bool
IMEHandler::IsComposing()
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    return nsTextStore::IsComposing();
  }
#endif // #ifdef NS_ENABLE_TSF

  return nsIMM32Handler::IsComposing();
}

// static
bool
IMEHandler::IsComposingOn(nsWindow* aWindow)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    return nsTextStore::IsComposingOn(aWindow);
  }
#endif // #ifdef NS_ENABLE_TSF

  return nsIMM32Handler::IsComposingOn(aWindow);
}

// static
nsresult
IMEHandler::NotifyIME(nsWindow* aWindow,
                      const IMENotification& aIMENotification)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    switch (aIMENotification.mMessage) {
      case NOTIFY_IME_OF_SELECTION_CHANGE:
        return nsTextStore::OnSelectionChange();
      case NOTIFY_IME_OF_TEXT_CHANGE:
        return nsTextStore::OnTextChange(aIMENotification);
      case NOTIFY_IME_OF_FOCUS:
        return nsTextStore::OnFocusChange(true, aWindow,
                                          aWindow->GetInputContext());
      case NOTIFY_IME_OF_BLUR:
        return nsTextStore::OnFocusChange(false, aWindow,
                                          aWindow->GetInputContext());
      case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
        return nsTextStore::OnMouseButtonEvent(aIMENotification);
      case REQUEST_TO_COMMIT_COMPOSITION:
        if (nsTextStore::IsComposingOn(aWindow)) {
          nsTextStore::CommitComposition(false);
        }
        return NS_OK;
      case REQUEST_TO_CANCEL_COMPOSITION:
        if (nsTextStore::IsComposingOn(aWindow)) {
          nsTextStore::CommitComposition(true);
        }
        return NS_OK;
      case NOTIFY_IME_OF_POSITION_CHANGE:
        return nsTextStore::OnLayoutChange();
      default:
        return NS_ERROR_NOT_IMPLEMENTED;
    }
  }
#endif //NS_ENABLE_TSF

  switch (aIMENotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      nsIMM32Handler::CommitComposition(aWindow);
      return NS_OK;
    case REQUEST_TO_CANCEL_COMPOSITION:
      nsIMM32Handler::CancelComposition(aWindow);
      return NS_OK;
    case NOTIFY_IME_OF_POSITION_CHANGE:
    case NOTIFY_IME_OF_COMPOSITION_UPDATE:
      nsIMM32Handler::OnUpdateComposition(aWindow);
      return NS_OK;
#ifdef NS_ENABLE_TSF
    case NOTIFY_IME_OF_BLUR:
      // If a plugin gets focus while TSF has focus, we need to notify TSF of
      // the blur.
      if (nsTextStore::ThinksHavingFocus()) {
        return nsTextStore::OnFocusChange(false, aWindow,
                                          aWindow->GetInputContext());
      }
      return NS_ERROR_NOT_IMPLEMENTED;
#endif //NS_ENABLE_TSF
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
    return nsTextStore::GetIMEUpdatePreference();
  }
#endif //NS_ENABLE_TSF

  return nsIMM32Handler::GetIMEUpdatePreference();
}

// static
bool
IMEHandler::GetOpenState(nsWindow* aWindow)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    return nsTextStore::GetIMEOpenState();
  }
#endif //NS_ENABLE_TSF

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  return IMEContext.GetOpenState();
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
    nsTextStore::SetInputContext(aWindow, aInputContext, aAction);
    if (IsTSFAvailable()) {
      aInputContext.mNativeIMEContext = nsTextStore::GetThreadManager();
      if (sIsIMMEnabled) {
        // Associate IME context for IMM-IMEs.
        AssociateIMEContext(aWindow, enable);
      } else if (oldInputContext.mIMEState.mEnabled == IMEState::PLUGIN) {
        // Disassociate the IME context from the window when plugin loses focus
        // in pure TSF mode.
        AssociateIMEContext(aWindow, false);
      }
      if (adjustOpenState) {
        nsTextStore::SetIMEOpenState(open);
      }
      return;
    }
  } else {
    // Set at least InputScope even when TextStore is not available.
    SetInputScopeForIMM32(aWindow, aInputContext.mHTMLInputType);
  }
#endif // #ifdef NS_ENABLE_TSF

  AssociateIMEContext(aWindow, enable);

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  if (adjustOpenState) {
    IMEContext.SetOpenState(open);
  }

  if (aInputContext.mNativeIMEContext) {
    return;
  }

  // The old InputContext must store the default IMC or old TextStore.
  // When IME context is disassociated from the window, use it.
  aInputContext.mNativeIMEContext = enable ?
    static_cast<void*>(IMEContext.get()) : oldInputContext.mNativeIMEContext;
}

// static
void
IMEHandler::AssociateIMEContext(nsWindow* aWindow, bool aEnable)
{
  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  if (aEnable) {
    IMEContext.AssociateDefaultContext();
    return;
  }
  // Don't disassociate the context after the window is destroyed.
  if (aWindow->Destroyed()) {
    return;
  }
  IMEContext.Disassociate();
}

// static
void
IMEHandler::InitInputContext(nsWindow* aWindow, InputContext& aInputContext)
{
  // For a11y, the default enabled state should be 'enabled'.
  aInputContext.mIMEState.mEnabled = IMEState::ENABLED;

#ifdef NS_ENABLE_TSF
  if (sIsInTSFMode) {
    nsTextStore::SetInputContext(aWindow, aInputContext,
      InputContextAction(InputContextAction::CAUSE_UNKNOWN,
                         InputContextAction::GOT_FOCUS));
    aInputContext.mNativeIMEContext = nsTextStore::GetThreadManager();
    MOZ_ASSERT(aInputContext.mNativeIMEContext);
    // IME context isn't necessary in pure TSF mode.
    if (!sIsIMMEnabled) {
      AssociateIMEContext(aWindow, false);
    }
    return;
  }
#endif // #ifdef NS_ENABLE_TSF

  // NOTE: mNativeIMEContext may be null if IMM module isn't installed.
  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  aInputContext.mNativeIMEContext = static_cast<void*>(IMEContext.get());
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
    return nsTextStore::CurrentKeyboardLayoutHasIME();
  }
#endif // #ifdef NS_ENABLE_TSF

  return nsIMM32Handler::IsIMEAvailable();
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

} // namespace widget
} // namespace mozilla
