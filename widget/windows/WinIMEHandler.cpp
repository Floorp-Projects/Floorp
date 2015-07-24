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
      case NOTIFY_IME_OF_FOCUS:
        IMMHandler::OnFocusChange(true, aWindow);
        return TSFTextStore::OnFocusChange(true, aWindow,
                                           aWindow->GetInputContext());
      case NOTIFY_IME_OF_BLUR:
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
      return NS_OK;
    case NOTIFY_IME_OF_BLUR:
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

} // namespace widget
} // namespace mozilla
