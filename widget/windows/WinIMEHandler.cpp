/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinIMEHandler.h"

#include "IMMHandler.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_intl.h"
#include "mozilla/TextEvents.h"
#include "mozilla/WindowsVersion.h"
#include "nsWindowDefs.h"
#include "WinTextEventDispatcherListener.h"

#include "TSFTextStore.h"

#include "OSKInputPaneManager.h"
#include "OSKTabTipManager.h"
#include "OSKVRManager.h"
#include "nsLookAndFeel.h"
#include "nsWindow.h"
#include "WinUtils.h"
#include "nsIWindowsRegKey.h"
#include "WindowsUIUtils.h"

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#endif  // #ifdef ACCESSIBILITY

#include "shellapi.h"
#include "shlobj.h"
#include "powrprof.h"
#include "setupapi.h"
#include "cfgmgr32.h"

#include "FxRWindowManager.h"
#include "moz_external_vr.h"

const char* kOskEnabled = "ui.osk.enabled";
const char* kOskDetectPhysicalKeyboard = "ui.osk.detect_physical_keyboard";
const char* kOskDebugReason = "ui.osk.debug.keyboardDisplayReason";

namespace mozilla {
namespace widget {

/******************************************************************************
 * IMEHandler
 ******************************************************************************/

nsWindow* IMEHandler::sFocusedWindow = nullptr;
InputContextAction::Cause IMEHandler::sLastContextActionCause =
    InputContextAction::CAUSE_UNKNOWN;
bool IMEHandler::sMaybeEditable = false;
bool IMEHandler::sForceDisableCurrentIMM_IME = false;
bool IMEHandler::sNativeCaretIsCreated = false;
bool IMEHandler::sHasNativeCaretBeenRequested = false;

bool IMEHandler::sIsInTSFMode = false;
bool IMEHandler::sIsIMMEnabled = true;
decltype(SetInputScopes)* IMEHandler::sSetInputScopes = nullptr;

static POWER_PLATFORM_ROLE sPowerPlatformRole = PlatformRoleUnspecified;
static bool sDeterminedPowerPlatformRole = false;

// static
void IMEHandler::Initialize() {
  TSFTextStore::Initialize();
  sIsInTSFMode = TSFTextStore::IsInTSFMode();
  sIsIMMEnabled =
      !sIsInTSFMode || StaticPrefs::intl_tsf_support_imm_AtStartup();
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

  IMMHandler::Initialize();

  sForceDisableCurrentIMM_IME = IMMHandler::IsActiveIMEInBlockList();
}

// static
void IMEHandler::Terminate() {
  if (sIsInTSFMode) {
    TSFTextStore::Terminate();
    sIsInTSFMode = false;
  }

  IMMHandler::Terminate();
  WinTextEventDispatcherListener::Shutdown();
}

// static
void* IMEHandler::GetNativeData(nsWindow* aWindow, uint32_t aDataType) {
  if (aDataType == NS_RAW_NATIVE_IME_CONTEXT) {
    if (IsTSFAvailable()) {
      return TSFTextStore::GetThreadManager();
    }
    IMEContext context(aWindow);
    if (context.IsValid()) {
      return context.get();
    }
    // If IMC isn't associated with the window, IME is disabled on the window
    // now.  In such case, we should return default IMC instead.
    const IMEContext& defaultIMC = aWindow->DefaultIMC();
    if (defaultIMC.IsValid()) {
      return defaultIMC.get();
    }
    // If there is no default IMC, we should return the pointer to the window
    // since if we return nullptr, IMEStateManager cannot manage composition
    // with TextComposition instance.  This is possible if no IME is installed,
    // but composition may occur with dead key sequence.
    return aWindow;
  }

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
}

// static
bool IMEHandler::ProcessRawKeyMessage(const MSG& aMsg) {
  if (IsTSFAvailable()) {
    return TSFTextStore::ProcessRawKeyMessage(aMsg);
  }
  return false;  // noting to do in IMM mode.
}

// static
bool IMEHandler::ProcessMessage(nsWindow* aWindow, UINT aMessage,
                                WPARAM& aWParam, LPARAM& aLParam,
                                MSGResult& aResult) {
  // If we're putting native caret over our caret, Windows dispatches
  // EVENT_OBJECT_LOCATIONCHANGE event on other applications which hook
  // the event with ::SetWinEventHook() and handles WM_GETOBJECT for
  // OBJID_CARET (this is request of caret from such applications) instead
  // of us.  If a11y module is active, it observes every our caret change
  // and put native caret over it automatically.  However, if other
  // applications require only caret information, activating a11y module is
  // overwork and such applications may requires carets only in editors.
  // Therefore, if it'd be possible, IMEHandler should put native caret over
  // our caret, but there is a problem.  Some versions of ATOK (Japanese TIP)
  // refer native caret and if there is, the behavior is worse than the
  // behavior without native caret.  Therefore, we shouldn't put native caret
  // as far as possible.
  if (!sHasNativeCaretBeenRequested && aMessage == WM_GETOBJECT &&
      static_cast<LONG>(aLParam) == OBJID_CARET) {
    // So, when we receive first WM_GETOBJECT for OBJID_CARET, let's start to
    // create native caret for such applications.
    sHasNativeCaretBeenRequested = true;
    // If an editable element has focus, we can put native caret now.
    // XXX Should we avoid doing this if there is composition?
    MaybeCreateNativeCaret(aWindow);
  }

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
    if (!IsIMMActive()) {
      return false;
    }
  }

  bool keepGoing =
      IMMHandler::ProcessMessage(aWindow, aMessage, aWParam, aLParam, aResult);

  // If user changes active IME to an IME which is listed in our block list,
  // we should disassociate IMC from the window for preventing the IME to work
  // and crash.
  if (aMessage == WM_INPUTLANGCHANGE) {
    bool disableIME = IMMHandler::IsActiveIMEInBlockList();
    if (disableIME != sForceDisableCurrentIMM_IME) {
      bool enable =
          !disableIME && WinUtils::IsIMEEnabled(aWindow->InputContextRef());
      AssociateIMEContext(aWindow, enable);
      sForceDisableCurrentIMM_IME = disableIME;
    }
  }

  return keepGoing;
}

// static
bool IMEHandler::IsA11yHandlingNativeCaret() {
#ifndef ACCESSIBILITY
  return false;
#else   // #ifndef ACCESSIBILITY
  // Let's assume that when there is the service, it handles native caret.
  return GetAccService() != nullptr;
#endif  // #ifndef ACCESSIBILITY #else
}

// static
bool IMEHandler::IsIMMActive() { return TSFTextStore::IsIMM_IMEActive(); }

// static
bool IMEHandler::IsComposing() {
  if (IsTSFAvailable()) {
    return TSFTextStore::IsComposing() || IMMHandler::IsComposing();
  }

  return IMMHandler::IsComposing();
}

// static
bool IMEHandler::IsComposingOn(nsWindow* aWindow) {
  if (IsTSFAvailable()) {
    return TSFTextStore::IsComposingOn(aWindow) ||
           IMMHandler::IsComposingOn(aWindow);
  }

  return IMMHandler::IsComposingOn(aWindow);
}

// static
nsresult IMEHandler::NotifyIME(nsWindow* aWindow,
                               const IMENotification& aIMENotification) {
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
      case NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED:
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
        sFocusedWindow = aWindow;
        IMMHandler::OnFocusChange(true, aWindow);
        nsresult rv = TSFTextStore::OnFocusChange(true, aWindow,
                                                  aWindow->GetInputContext());
        MaybeCreateNativeCaret(aWindow);
        IMEHandler::MaybeShowOnScreenKeyboard(aWindow,
                                              aWindow->GetInputContext());
        return rv;
      }
      case NOTIFY_IME_OF_BLUR:
        sFocusedWindow = nullptr;
        IMEHandler::MaybeDismissOnScreenKeyboard(aWindow);
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

  switch (aIMENotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      IMMHandler::CommitComposition(aWindow);
      return NS_OK;
    case REQUEST_TO_CANCEL_COMPOSITION:
      IMMHandler::CancelComposition(aWindow);
      return NS_OK;
    case NOTIFY_IME_OF_POSITION_CHANGE:
    case NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED:
      IMMHandler::OnUpdateComposition(aWindow);
      return NS_OK;
    case NOTIFY_IME_OF_SELECTION_CHANGE:
      IMMHandler::OnSelectionChange(aWindow, aIMENotification, true);
      // IMMHandler::OnSelectionChange() cannot work without its singleton
      // instance.  Therefore, IMEHandler needs to create native caret instead
      // if it's necessary.
      MaybeCreateNativeCaret(aWindow);
      return NS_OK;
    case NOTIFY_IME_OF_MOUSE_BUTTON_EVENT:
      return IMMHandler::OnMouseButtonEvent(aWindow, aIMENotification);
    case NOTIFY_IME_OF_FOCUS:
      sFocusedWindow = aWindow;
      IMMHandler::OnFocusChange(true, aWindow);
      IMEHandler::MaybeShowOnScreenKeyboard(aWindow,
                                            aWindow->GetInputContext());
      MaybeCreateNativeCaret(aWindow);
      return NS_OK;
    case NOTIFY_IME_OF_BLUR:
      sFocusedWindow = nullptr;
      IMEHandler::MaybeDismissOnScreenKeyboard(aWindow);
      IMMHandler::OnFocusChange(false, aWindow);
      // If a plugin gets focus while TSF has focus, we need to notify TSF of
      // the blur.
      if (TSFTextStore::ThinksHavingFocus()) {
        return TSFTextStore::OnFocusChange(false, aWindow,
                                           aWindow->GetInputContext());
      }
      return NS_OK;
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

// static
IMENotificationRequests IMEHandler::GetIMENotificationRequests() {
  if (IsTSFAvailable()) {
    if (!sIsIMMEnabled) {
      return TSFTextStore::GetIMENotificationRequests();
    }
    // Even if TSF is available, the active IME may be an IMM-IME.
    // Unfortunately, changing the result of GetIMENotificationRequests() while
    // an editor has focus isn't supported by IMEContentObserver nor
    // ContentCacheInParent.  Therefore, we need to request whole notifications
    // which are necessary either IMMHandler or TSFTextStore.
    return IMMHandler::GetIMENotificationRequests() |
           TSFTextStore::GetIMENotificationRequests();
  }

  return IMMHandler::GetIMENotificationRequests();
}

// static
TextEventDispatcherListener*
IMEHandler::GetNativeTextEventDispatcherListener() {
  return WinTextEventDispatcherListener::GetInstance();
}

// static
bool IMEHandler::GetOpenState(nsWindow* aWindow) {
  if (IsTSFAvailable() && !IsIMMActive()) {
    return TSFTextStore::GetIMEOpenState();
  }

  IMEContext context(aWindow);
  return context.GetOpenState();
}

// static
void IMEHandler::OnDestroyWindow(nsWindow* aWindow) {
  // When focus is in remote process, but the window is being destroyed, we
  // need to clean up TSFTextStore here since NOTIFY_IME_OF_BLUR won't reach
  // here because BrowserParent already lost the reference to the nsWindow when
  // it receives from the remote process.
  if (sFocusedWindow == aWindow) {
    MOZ_ASSERT(aWindow->GetInputContext().IsOriginContentProcess(),
               "input context of focused widget should've been set by a remote "
               "process "
               "if IME focus isn't cleared before destroying the widget");
    NotifyIME(aWindow, IMENotification(NOTIFY_IME_OF_BLUR));
  }

  // We need to do nothing here for TSF. Just restore the default context
  // if it's been disassociated.
  if (!sIsInTSFMode) {
    // MSDN says we need to set IS_DEFAULT to avoid memory leak when we use
    // SetInputScopes API. Use an empty string to do this.
    SetInputScopeForIMM32(aWindow, u""_ns, u""_ns, false);
  }
  AssociateIMEContext(aWindow, true);
}

// static
bool IMEHandler::NeedsToAssociateIMC() { return !sForceDisableCurrentIMM_IME; }

// static
void IMEHandler::SetInputContext(nsWindow* aWindow, InputContext& aInputContext,
                                 const InputContextAction& aAction) {
  sLastContextActionCause = aAction.mCause;
  // FYI: If there is no composition, this call will do nothing.
  NotifyIME(aWindow, IMENotification(REQUEST_TO_COMMIT_COMPOSITION));

  if (aInputContext.mHTMLInputMode.EqualsLiteral("none")) {
    IMEHandler::MaybeDismissOnScreenKeyboard(aWindow, Sync::Yes);
  } else if (aAction.UserMightRequestOpenVKB()) {
    IMEHandler::MaybeShowOnScreenKeyboard(aWindow, aInputContext);
  }

  bool enable = WinUtils::IsIMEEnabled(aInputContext);
  bool adjustOpenState = (enable && aInputContext.mIMEState.mOpen !=
                                        IMEState::DONT_CHANGE_OPEN_STATE);
  bool open =
      (adjustOpenState && aInputContext.mIMEState.mOpen == IMEState::OPEN);

  // Note that even while a plugin has focus, we need to notify TSF of that.
  if (sIsInTSFMode) {
    TSFTextStore::SetInputContext(aWindow, aInputContext, aAction);
    if (IsTSFAvailable()) {
      if (sIsIMMEnabled) {
        // Associate IMC with aWindow only when it's necessary.
        AssociateIMEContext(aWindow, enable && NeedsToAssociateIMC());
      }
      if (adjustOpenState) {
        TSFTextStore::SetIMEOpenState(open);
      }
      return;
    }
  } else {
    // Set at least InputScope even when TextStore is not available.
    SetInputScopeForIMM32(aWindow, aInputContext.mHTMLInputType,
                          aInputContext.mHTMLInputMode,
                          aInputContext.mInPrivateBrowsing);
  }

  AssociateIMEContext(aWindow, enable);

  IMEContext context(aWindow);
  if (adjustOpenState) {
    context.SetOpenState(open);
  }
}

// static
void IMEHandler::AssociateIMEContext(nsWindow* aWindowBase, bool aEnable) {
  IMEContext context(aWindowBase);
  if (aEnable) {
    context.AssociateDefaultContext();
    return;
  }
  // Don't disassociate the context after the window is destroyed.
  if (aWindowBase->Destroyed()) {
    return;
  }
  context.Disassociate();
}

// static
void IMEHandler::InitInputContext(nsWindow* aWindow,
                                  InputContext& aInputContext) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->GetWindowHandle(),
             "IMEHandler::SetInputContext() requires non-nullptr HWND");

  static bool sInitialized = false;
  if (!sInitialized) {
    sInitialized = true;
    // Some TIPs like QQ Input (Simplified Chinese) may need normal window
    // (i.e., windows except message window) when initializing themselves.
    // Therefore, we need to initialize TSF/IMM modules after first normal
    // window is created.  InitInputContext() should be called immediately
    // after creating each normal window, so, here is a good place to
    // initialize these modules.
    Initialize();
  }

  // For a11y, the default enabled state should be 'enabled'.
  aInputContext.mIMEState.mEnabled = IMEEnabled::Enabled;

  if (sIsInTSFMode) {
    TSFTextStore::SetInputContext(
        aWindow, aInputContext,
        InputContextAction(InputContextAction::CAUSE_UNKNOWN,
                           InputContextAction::WIDGET_CREATED));
    // IME context isn't necessary in pure TSF mode.
    if (!sIsIMMEnabled) {
      AssociateIMEContext(aWindow, false);
    }
    return;
  }

#ifdef DEBUG
  // NOTE: IMC may be null if IMM module isn't installed.
  IMEContext context(aWindow);
  MOZ_ASSERT(context.IsValid() || !CurrentKeyboardLayoutHasIME());
#endif  // #ifdef DEBUG
}

#ifdef DEBUG
// static
bool IMEHandler::CurrentKeyboardLayoutHasIME() {
  if (sIsInTSFMode) {
    return TSFTextStore::CurrentKeyboardLayoutHasIME();
  }

  return IMMHandler::IsIMEAvailable();
}
#endif  // #ifdef DEBUG

// static
void IMEHandler::OnKeyboardLayoutChanged() {
  // Be aware, this method won't be called until TSFStaticSink starts to
  // observe active TIP change.  If you need to be notified of this, you
  // need to create TSFStaticSink::Observe() or something and call it
  // TSFStaticSink::EnsureInitActiveTIPKeyboard() forcibly.

  if (!sIsIMMEnabled || !IsTSFAvailable()) {
    return;
  }
}

// static
void IMEHandler::SetInputScopeForIMM32(nsWindow* aWindow,
                                       const nsAString& aHTMLInputType,
                                       const nsAString& aHTMLInputMode,
                                       bool aInPrivateBrowsing) {
  if (sIsInTSFMode || !sSetInputScopes || aWindow->Destroyed()) {
    return;
  }
  AutoTArray<InputScope, 3> scopes;

  // IME may refer only first input scope, but we will append inputmode's
  // input scopes since IME may refer it like Chrome.
  AppendInputScopeFromType(aHTMLInputType, scopes);
  AppendInputScopeFromInputMode(aHTMLInputMode, scopes);

  if (aInPrivateBrowsing) {
    scopes.AppendElement(IS_PRIVATE);
  }

  if (scopes.IsEmpty()) {
    // At least, 1 item is necessary.
    scopes.AppendElement(IS_DEFAULT);
  }

  sSetInputScopes(aWindow->GetWindowHandle(), scopes.Elements(),
                  scopes.Length(), nullptr, 0, nullptr, nullptr);
}

// static
void IMEHandler::AppendInputScopeFromInputMode(const nsAString& aHTMLInputMode,
                                               nsTArray<InputScope>& aScopes) {
  if (aHTMLInputMode.EqualsLiteral("mozAwesomebar")) {
    // Even if Awesomebar has focus, user may not input URL directly.
    // However, on-screen keyboard for URL should be shown because it has
    // some useful additional keys like ".com" and they are not hindrances
    // even when inputting non-URL text, e.g., words to search something in
    // the web.  On the other hand, a lot of Microsoft's IMEs and Google
    // Japanese Input make their open state "closed" automatically if we
    // notify them of URL as the input scope.  However, this is very annoying
    // for the users when they try to input some words to search the web or
    // bookmark/history items.  Therefore, if they are active, we need to
    // notify them of the default input scope for avoiding this issue.
    // FYI: We cannot check active TIP without TSF.  Therefore, if it's
    //      not in TSF mode, this will check only if active IMM-IME is Google
    //      Japanese Input.  Google Japanese Input is a TIP of TSF basically.
    //      However, if the OS is Win7 or it's installed on Win7 but has not
    //      been updated yet even after the OS is upgraded to Win8 or later,
    //      it's installed as IMM-IME.
    if (TSFTextStore::ShouldSetInputScopeOfURLBarToDefault()) {
      return;
    }
    // Don't append IS_SEARCH here for showing on-screen keyboard for URL.
    if (!aScopes.Contains(IS_URL)) {
      aScopes.AppendElement(IS_URL);
    }
    return;
  }

  // https://html.spec.whatwg.org/dev/interaction.html#attr-inputmode
  if (aHTMLInputMode.EqualsLiteral("url")) {
    if (!aScopes.Contains(IS_SEARCH)) {
      aScopes.AppendElement(IS_URL);
    }
    return;
  }
  if (aHTMLInputMode.EqualsLiteral("email")) {
    if (!aScopes.Contains(IS_EMAIL_SMTPEMAILADDRESS)) {
      aScopes.AppendElement(IS_EMAIL_SMTPEMAILADDRESS);
    }
    return;
  }
  if (aHTMLInputMode.EqualsLiteral("tel")) {
    if (!aScopes.Contains(IS_TELEPHONE_FULLTELEPHONENUMBER)) {
      aScopes.AppendElement(IS_TELEPHONE_FULLTELEPHONENUMBER);
    }
    if (!aScopes.Contains(IS_TELEPHONE_LOCALNUMBER)) {
      aScopes.AppendElement(IS_TELEPHONE_LOCALNUMBER);
    }
    return;
  }
  if (aHTMLInputMode.EqualsLiteral("numeric")) {
    if (!aScopes.Contains(IS_DIGITS)) {
      aScopes.AppendElement(IS_DIGITS);
    }
    return;
  }
  if (aHTMLInputMode.EqualsLiteral("decimal")) {
    if (!aScopes.Contains(IS_NUMBER)) {
      aScopes.AppendElement(IS_NUMBER);
    }
    return;
  }
  if (aHTMLInputMode.EqualsLiteral("search")) {
    if (NeedsSearchInputScope() && !aScopes.Contains(IS_SEARCH)) {
      aScopes.AppendElement(IS_SEARCH);
    }
    return;
  }
}

// static
void IMEHandler::AppendInputScopeFromType(const nsAString& aHTMLInputType,
                                          nsTArray<InputScope>& aScopes) {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-input-element.html
  if (aHTMLInputType.EqualsLiteral("url")) {
    aScopes.AppendElement(IS_URL);
    return;
  }
  if (aHTMLInputType.EqualsLiteral("search")) {
    if (NeedsSearchInputScope()) {
      aScopes.AppendElement(IS_SEARCH);
    }
    return;
  }
  if (aHTMLInputType.EqualsLiteral("email")) {
    aScopes.AppendElement(IS_EMAIL_SMTPEMAILADDRESS);
    return;
  }
  if (aHTMLInputType.EqualsLiteral("password")) {
    aScopes.AppendElement(IS_PASSWORD);
    return;
  }
  if (aHTMLInputType.EqualsLiteral("datetime") ||
      aHTMLInputType.EqualsLiteral("datetime-local")) {
    aScopes.AppendElement(IS_DATE_FULLDATE);
    aScopes.AppendElement(IS_TIME_FULLTIME);
    return;
  }
  if (aHTMLInputType.EqualsLiteral("date") ||
      aHTMLInputType.EqualsLiteral("month") ||
      aHTMLInputType.EqualsLiteral("week")) {
    aScopes.AppendElement(IS_DATE_FULLDATE);
    return;
  }
  if (aHTMLInputType.EqualsLiteral("time")) {
    aScopes.AppendElement(IS_TIME_FULLTIME);
    return;
  }
  if (aHTMLInputType.EqualsLiteral("tel")) {
    aScopes.AppendElement(IS_TELEPHONE_FULLTELEPHONENUMBER);
    aScopes.AppendElement(IS_TELEPHONE_LOCALNUMBER);
    return;
  }
  if (aHTMLInputType.EqualsLiteral("number")) {
    aScopes.AppendElement(IS_NUMBER);
    return;
  }
}

// static
bool IMEHandler::NeedsSearchInputScope() {
  return !StaticPrefs::intl_tsf_hack_atok_search_input_scope_disabled() ||
         !TSFTextStore::IsATOKActive();
}

// static
bool IMEHandler::IsOnScreenKeyboardSupported() {
#ifdef NIGHTLY_BUILD
  if (FxRWindowManager::GetInstance()->IsFxRWindow(sFocusedWindow)) {
    return true;
  }
#endif  // NIGHTLY_BUILD
  if (!Preferences::GetBool(kOskEnabled, true) ||
      !IMEHandler::NeedOnScreenKeyboard()) {
    return false;
  }

  // On Windows 11, we ignore tablet mode (see bug 1722208)
  if (!IsWin11OrLater()) {
    // On Windows 10 we require tablet mode, unless the user has set the
    // relevant setting to enable the on-screen keyboard in desktop mode.
    if (!IsInTabletMode() && !AutoInvokeOnScreenKeyboardInDesktopMode()) {
      return false;
    }
  }

  return true;
}

// static
void IMEHandler::MaybeShowOnScreenKeyboard(nsWindow* aWindow,
                                           const InputContext& aInputContext) {
  if (aInputContext.mHTMLInputMode.EqualsLiteral("none")) {
    return;
  }

  if (!IsOnScreenKeyboardSupported()) {
    return;
  }

  IMEHandler::ShowOnScreenKeyboard(aWindow);
}

// static
void IMEHandler::MaybeDismissOnScreenKeyboard(nsWindow* aWindow, Sync aSync) {
#ifdef NIGHTLY_BUILD
  if (FxRWindowManager::GetInstance()->IsFxRWindow(aWindow)) {
    OSKVRManager::DismissOnScreenKeyboard();
  }
#endif  // NIGHTLY_BUILD
  if (aSync == Sync::Yes) {
    DismissOnScreenKeyboard(aWindow);
    return;
  }

  RefPtr<nsWindow> window(aWindow);
  NS_DispatchToCurrentThreadQueue(
      NS_NewRunnableFunction("IMEHandler::MaybeDismissOnScreenKeyboard",
                             [window]() {
                               if (window->Destroyed()) {
                                 return;
                               }
                               if (!sFocusedWindow) {
                                 DismissOnScreenKeyboard(window);
                               }
                             }),
      EventQueuePriority::Idle);
}

// static
bool IMEHandler::WStringStartsWithCaseInsensitive(const std::wstring& aHaystack,
                                                  const std::wstring& aNeedle) {
  std::wstring lowerCaseHaystack(aHaystack);
  std::wstring lowerCaseNeedle(aNeedle);
  std::transform(lowerCaseHaystack.begin(), lowerCaseHaystack.end(),
                 lowerCaseHaystack.begin(), ::tolower);
  std::transform(lowerCaseNeedle.begin(), lowerCaseNeedle.end(),
                 lowerCaseNeedle.begin(), ::tolower);
  return wcsstr(lowerCaseHaystack.c_str(), lowerCaseNeedle.c_str()) ==
         lowerCaseHaystack.c_str();
}

// Returns false if a physical keyboard is detected on Windows 8 and up,
// or there is some other reason why an onscreen keyboard is not necessary.
// Returns true if no keyboard is found and this device looks like it needs
// an on-screen keyboard for text input.
// static
bool IMEHandler::NeedOnScreenKeyboard() {
  if (!Preferences::GetBool(kOskDetectPhysicalKeyboard, true)) {
    Preferences::SetString(kOskDebugReason, L"IKPOS: Detection disabled.");
    return true;
  }

  // If the last focus cause was not user-initiated (ie a result of code
  // setting focus to an element) then don't auto-show a keyboard. This
  // avoids cases where the keyboard would pop up "just" because e.g. a
  // web page chooses to focus a search field on the page, even when that
  // really isn't what the user is trying to do at that moment.
  if (!InputContextAction::IsHandlingUserInput(sLastContextActionCause)) {
    return false;
  }

  // This function should be only invoked for machines with touch screens.
  if ((::GetSystemMetrics(SM_DIGITIZER) & NID_INTEGRATED_TOUCH) !=
      NID_INTEGRATED_TOUCH) {
    Preferences::SetString(kOskDebugReason, L"IKPOS: Touch screen not found.");
    return false;
  }

  // If the device is docked, the user is treating the device as a PC.
  if (::GetSystemMetrics(SM_SYSTEMDOCKED) != 0) {
    Preferences::SetString(kOskDebugReason, L"IKPOS: System docked.");
    return false;
  }

  // To determine whether a keyboard is present on the device, we do the
  // following:-
  // 1. If the platform role is that of a mobile or slate device, check the
  //    system metric SM_CONVERTIBLESLATEMODE to see if it is being used
  //    in slate mode. If it is, also check that the last input was a touch.
  //    If all of this is true, then we should show the on-screen keyboard.

  // 2. If step 1 didn't determine we should show the keyboard, we check if
  //    this device has keyboards attached to it.

  // Check if the device is being used as a laptop or a tablet. This can be
  // checked by first checking the role of the device and then the
  // corresponding system metric (SM_CONVERTIBLESLATEMODE). If it is being
  // used as a tablet then we want the OSK to show up.
  if (!sDeterminedPowerPlatformRole) {
    sDeterminedPowerPlatformRole = true;
    sPowerPlatformRole = WinUtils::GetPowerPlatformRole();
  }

  // If this a mobile or slate (tablet) device, check if it is in slate mode.
  // If the last input was touch, ignore whether or not a keyboard is present.
  if ((sPowerPlatformRole == PlatformRoleMobile ||
       sPowerPlatformRole == PlatformRoleSlate) &&
      ::GetSystemMetrics(SM_CONVERTIBLESLATEMODE) == 0 &&
      sLastContextActionCause == InputContextAction::CAUSE_TOUCH) {
    Preferences::SetString(
        kOskDebugReason,
        L"IKPOS: Mobile/Slate Platform role, in slate mode with touch event.");
    return true;
  }

  return !IMEHandler::IsKeyboardPresentOnSlate();
}

// Uses the Setup APIs to enumerate the attached keyboards and returns true
// if the keyboard count is 1 or more. While this will work in most cases
// it won't work if there are devices which expose keyboard interfaces which
// are attached to the machine.
// Based on IsKeyboardPresentOnSlate() in Chromium's base/win/win_util.cc.
// static
bool IMEHandler::IsKeyboardPresentOnSlate() {
  const GUID KEYBOARD_CLASS_GUID = {
      0x4D36E96B,
      0xE325,
      0x11CE,
      {0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18}};

  // Query for all the keyboard devices.
  HDEVINFO device_info = ::SetupDiGetClassDevs(&KEYBOARD_CLASS_GUID, nullptr,
                                               nullptr, DIGCF_PRESENT);
  if (device_info == INVALID_HANDLE_VALUE) {
    Preferences::SetString(kOskDebugReason, L"IKPOS: No keyboard info.");
    return false;
  }

  // Enumerate all keyboards and look for ACPI\PNP and HID\VID devices. If
  // the count is more than 1 we assume that a keyboard is present. This is
  // under the assumption that there will always be one keyboard device.
  for (DWORD i = 0;; ++i) {
    SP_DEVINFO_DATA device_info_data = {0};
    device_info_data.cbSize = sizeof(device_info_data);
    if (!::SetupDiEnumDeviceInfo(device_info, i, &device_info_data)) {
      break;
    }

    // Get the device ID.
    wchar_t device_id[MAX_DEVICE_ID_LEN];
    CONFIGRET status = ::CM_Get_Device_ID(device_info_data.DevInst, device_id,
                                          MAX_DEVICE_ID_LEN, 0);
    if (status == CR_SUCCESS) {
      static const std::wstring BT_HID_DEVICE = L"HID\\{00001124";
      static const std::wstring BT_HOGP_DEVICE = L"HID\\{00001812";
      // To reduce the scope of the hack we only look for ACPI and HID\\VID
      // prefixes in the keyboard device ids.
      if (IMEHandler::WStringStartsWithCaseInsensitive(device_id, L"ACPI") ||
          IMEHandler::WStringStartsWithCaseInsensitive(device_id,
                                                       L"HID\\VID") ||
          IMEHandler::WStringStartsWithCaseInsensitive(device_id,
                                                       BT_HID_DEVICE) ||
          IMEHandler::WStringStartsWithCaseInsensitive(device_id,
                                                       BT_HOGP_DEVICE)) {
        // The heuristic we are using is to check the count of keyboards and
        // return true if the API's report one or more keyboards. Please note
        // that this will break for non keyboard devices which expose a
        // keyboard PDO.
        Preferences::SetString(kOskDebugReason,
                               L"IKPOS: Keyboard presence confirmed.");
        return true;
      }
    }
  }
  Preferences::SetString(kOskDebugReason,
                         L"IKPOS: Lack of keyboard confirmed.");
  return false;
}

// static
bool IMEHandler::IsInTabletMode() {
  bool isInTabletMode = WindowsUIUtils::GetInTabletMode();
  if (isInTabletMode) {
    Preferences::SetString(kOskDebugReason, L"IITM: GetInTabletMode=true.");
  } else {
    Preferences::SetString(kOskDebugReason, L"IITM: GetInTabletMode=false.");
  }
  return isInTabletMode;
}

static bool ReadEnableDesktopModeAutoInvoke(uint32_t aRoot,
                                            nsIWindowsRegKey* aRegKey,
                                            uint32_t& aValue) {
  nsresult rv;
  rv = aRegKey->Open(aRoot, u"SOFTWARE\\Microsoft\\TabletTip\\1.7"_ns,
                     nsIWindowsRegKey::ACCESS_QUERY_VALUE);
  if (NS_FAILED(rv)) {
    Preferences::SetString(kOskDebugReason,
                           L"AIOSKIDM: failed opening regkey.");
    return false;
  }
  // EnableDesktopModeAutoInvoke is an opt-in option from the Windows
  // Settings to "Automatically show the touch keyboard in windowed apps
  // when there's no keyboard attached to your device." If the user has
  // opted-in to this behavior, the tablet-mode requirement is skipped.
  rv = aRegKey->ReadIntValue(u"EnableDesktopModeAutoInvoke"_ns, &aValue);
  if (NS_FAILED(rv)) {
    Preferences::SetString(kOskDebugReason,
                           L"AIOSKIDM: failed reading value of regkey.");
    return false;
  }
  return true;
}

// static
bool IMEHandler::AutoInvokeOnScreenKeyboardInDesktopMode() {
  nsresult rv;
  nsCOMPtr<nsIWindowsRegKey> regKey(
      do_CreateInstance("@mozilla.org/windows-registry-key;1", &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Preferences::SetString(kOskDebugReason,
                           L"AIOSKIDM: "
                           L"nsIWindowsRegKey not available");
    return false;
  }

  uint32_t value;
  if (!ReadEnableDesktopModeAutoInvoke(nsIWindowsRegKey::ROOT_KEY_CURRENT_USER,
                                       regKey, value) &&
      !ReadEnableDesktopModeAutoInvoke(nsIWindowsRegKey::ROOT_KEY_LOCAL_MACHINE,
                                       regKey, value)) {
    return false;
  }
  if (!!value) {
    Preferences::SetString(kOskDebugReason, L"AIOSKIDM: regkey value=true.");
  } else {
    Preferences::SetString(kOskDebugReason, L"AIOSKIDM: regkey value=false.");
  }
  return !!value;
}

// Based on DisplayVirtualKeyboard() in Chromium's base/win/win_util.cc.
// static
void IMEHandler::ShowOnScreenKeyboard(nsWindow* aWindow) {
#ifdef NIGHTLY_BUILD
  if (FxRWindowManager::GetInstance()->IsFxRWindow(sFocusedWindow)) {
    OSKVRManager::ShowOnScreenKeyboard();
    return;
  }
#endif  // NIGHTLY_BUILD

  if (IsWin10AnniversaryUpdateOrLater()) {
    OSKInputPaneManager::ShowOnScreenKeyboard(aWindow->GetWindowHandle());
    return;
  }

  OSKTabTipManager::ShowOnScreenKeyboard();
}

// Based on DismissVirtualKeyboard() in Chromium's base/win/win_util.cc.
// static
void IMEHandler::DismissOnScreenKeyboard(nsWindow* aWindow) {
  // Dismiss the virtual keyboard if it's open
  if (IsWin10AnniversaryUpdateOrLater()) {
    OSKInputPaneManager::DismissOnScreenKeyboard(aWindow->GetWindowHandle());
    return;
  }

  OSKTabTipManager::DismissOnScreenKeyboard();
}

bool IMEHandler::MaybeCreateNativeCaret(nsWindow* aWindow) {
  MOZ_ASSERT(aWindow);

  if (IsA11yHandlingNativeCaret()) {
    return false;
  }

  if (!sHasNativeCaretBeenRequested) {
    // If we have not received WM_GETOBJECT for OBJID_CARET, there may be new
    // application which requires our caret information.  For kicking its
    // window event proc, we should fire a window event here.
    // (If there is such application, sHasNativeCaretBeenRequested will be set
    // to true later.)
    // FYI: If we create native caret and move its position, native caret
    //      causes EVENT_OBJECT_LOCATIONCHANGE event with OBJID_CARET and
    //      OBJID_CLIENT.
    ::NotifyWinEvent(EVENT_OBJECT_LOCATIONCHANGE, aWindow->GetWindowHandle(),
                     OBJID_CARET, OBJID_CLIENT);
    return false;
  }

  MaybeDestroyNativeCaret();

  // If focused content is not text editable, we don't support caret
  // caret information without a11y module.
  if (!aWindow->GetInputContext().mIMEState.IsEditable()) {
    return false;
  }

  WidgetQueryContentEvent queryCaretRectEvent(true, eQueryCaretRect, aWindow);
  aWindow->InitEvent(queryCaretRectEvent);

  WidgetQueryContentEvent::Options options;
  options.mRelativeToInsertionPoint = true;
  queryCaretRectEvent.InitForQueryCaretRect(0, options);

  aWindow->DispatchWindowEvent(queryCaretRectEvent);
  if (NS_WARN_IF(queryCaretRectEvent.Failed())) {
    return false;
  }

  return CreateNativeCaret(aWindow, queryCaretRectEvent.mReply->mRect);
}

bool IMEHandler::CreateNativeCaret(nsWindow* aWindow,
                                   const LayoutDeviceIntRect& aCaretRect) {
  MOZ_ASSERT(aWindow);

  MOZ_ASSERT(!IsA11yHandlingNativeCaret());

  sNativeCaretIsCreated =
      ::CreateCaret(aWindow->GetWindowHandle(), nullptr, aCaretRect.Width(),
                    aCaretRect.Height());
  if (!sNativeCaretIsCreated) {
    return false;
  }
  nsWindow* toplevelWindow = aWindow->GetTopLevelWindow(false);
  if (NS_WARN_IF(!toplevelWindow)) {
    MaybeDestroyNativeCaret();
    return false;
  }

  LayoutDeviceIntPoint caretPosition(aCaretRect.TopLeft());
  if (toplevelWindow != aWindow) {
    caretPosition += toplevelWindow->WidgetToScreenOffset();
    caretPosition -= aWindow->WidgetToScreenOffset();
  }

  ::SetCaretPos(caretPosition.x, caretPosition.y);
  return true;
}

void IMEHandler::MaybeDestroyNativeCaret() {
  if (!sNativeCaretIsCreated) {
    return;
  }
  ::DestroyCaret();
  sNativeCaretIsCreated = false;
}

}  // namespace widget
}  // namespace mozilla
