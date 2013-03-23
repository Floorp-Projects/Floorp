/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinIMEHandler.h"
#include "nsIMM32Handler.h"

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
bool IMEHandler::sPluginHasFocus = false;
#endif // #ifdef NS_ENABLE_TSF

// static
void
IMEHandler::Initialize()
{
#ifdef NS_ENABLE_TSF
  nsTextStore::Initialize();
  sIsInTSFMode = nsTextStore::IsInTSFMode();
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
IMEHandler::IsIMEEnabled(const InputContext& aInputContext)
{
  return IsIMEEnabled(aInputContext.mIMEState.mEnabled);
}

// static
bool
IMEHandler::IsIMEEnabled(IMEState::Enabled aIMEState)
{
  return (aIMEState == mozilla::widget::IMEState::ENABLED ||
          aIMEState == mozilla::widget::IMEState::PLUGIN);
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
                           LRESULT* aRetValue, bool& aEatMessage)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    if (aMessage == WM_USER_TSF_TEXTCHANGE) {
      nsTextStore::OnTextChangeMsg();
      aEatMessage = true;
      return true;
    }
    return false;
  }
#endif // #ifdef NS_ENABLE_TSF

  return nsIMM32Handler::ProcessMessage(aWindow, aMessage, aWParam, aLParam,
                                        aRetValue, aEatMessage);
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
                      NotificationToIME aNotification)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    switch (aNotification) {
      case NOTIFY_IME_OF_SELECTION_CHANGE:
        return nsTextStore::OnSelectionChange();
      case NOTIFY_IME_OF_FOCUS:
        return nsTextStore::OnFocusChange(true, aWindow,
                 aWindow->GetInputContext().mIMEState.mEnabled);
      case NOTIFY_IME_OF_BLUR:
        return nsTextStore::OnFocusChange(false, aWindow,
                 aWindow->GetInputContext().mIMEState.mEnabled);
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
      default:
        return NS_ERROR_NOT_IMPLEMENTED;
    }
  }
#endif //NS_ENABLE_TSF

  switch (aNotification) {
    case REQUEST_TO_COMMIT_COMPOSITION:
      nsIMM32Handler::CommitComposition(aWindow);
      return NS_OK;
    case REQUEST_TO_CANCEL_COMPOSITION:
      nsIMM32Handler::CancelComposition(aWindow);
      return NS_OK;
#ifdef NS_ENABLE_TSF
    case NOTIFY_IME_OF_BLUR:
      // If a plugin gets focus while TSF has focus, we need to notify TSF of
      // the blur.
      if (nsTextStore::ThinksHavingFocus()) {
        return nsTextStore::OnFocusChange(false, aWindow,
                 aWindow->GetInputContext().mIMEState.mEnabled);
      }
      return NS_ERROR_NOT_IMPLEMENTED;
#endif //NS_ENABLE_TSF
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

// static
nsresult
IMEHandler::NotifyIMEOfTextChange(uint32_t aStart,
                                  uint32_t aOldEnd,
                                  uint32_t aNewEnd)
{
#ifdef NS_ENABLE_TSF
  if (IsTSFAvailable()) {
    return nsTextStore::OnTextChange(aStart, aOldEnd, aNewEnd);
  }
#endif //NS_ENABLE_TSF

  return NS_ERROR_NOT_IMPLEMENTED;
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

  return nsIMEUpdatePreference(false, false);
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
  // We need to do nothing here for TSF. Just restore the default context
  // if it's been disassociated.
  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  IMEContext.AssociateDefaultContext();
}

// static
void
IMEHandler::SetInputContext(nsWindow* aWindow, InputContext& aInputContext)
{
  // FYI: If there is no composition, this call will do nothing.
  NotifyIME(aWindow, REQUEST_TO_COMMIT_COMPOSITION);

  // Assume that SetInputContext() is called only when aWindow has focus.
  sPluginHasFocus = (aInputContext.mIMEState.mEnabled == IMEState::PLUGIN);

  bool enable = IsIMEEnabled(aInputContext);
  bool adjustOpenState = (enable &&
    aInputContext.mIMEState.mOpen != IMEState::DONT_CHANGE_OPEN_STATE);
  bool open = (adjustOpenState &&
    aInputContext.mIMEState.mOpen == IMEState::OPEN);

  aInputContext.mNativeIMEContext = nullptr;

#ifdef NS_ENABLE_TSF
  // Note that even while a plugin has focus, we need to notify TSF of that.
  if (sIsInTSFMode) {
    nsTextStore::SetInputContext(aInputContext);
    if (IsTSFAvailable()) {
      aInputContext.mNativeIMEContext = nsTextStore::GetTextStore();
    }
    // Currently, nsTextStore doesn't set focus to keyboard disabled document.
    // Therefore, we still need to perform the following legacy code.
  }
#endif // #ifdef NS_ENABLE_TSF

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  if (enable) {
    IMEContext.AssociateDefaultContext();
    if (!aInputContext.mNativeIMEContext) {
      aInputContext.mNativeIMEContext = static_cast<void*>(IMEContext.get());
    }
  } else if (!aWindow->Destroyed()) {
    // Don't disassociate the context after the window is destroyed.
    IMEContext.Disassociate();
    if (!aInputContext.mNativeIMEContext) {
      // The old InputContext must store the default IMC.
      aInputContext.mNativeIMEContext =
        aWindow->GetInputContext().mNativeIMEContext;
    }
  }

  if (adjustOpenState) {
#ifdef NS_ENABLE_TSF
    if (IsTSFAvailable()) {
      nsTextStore::SetIMEOpenState(open);
      return;
    }
#endif // #ifdef NS_ENABLE_TSF
    IMEContext.SetOpenState(open);
  }
}

// static
void
IMEHandler::InitInputContext(nsWindow* aWindow, InputContext& aInputContext)
{
  // For a11y, the default enabled state should be 'enabled'.
  aInputContext.mIMEState.mEnabled = IMEState::ENABLED;

#ifdef NS_ENABLE_TSF
  if (sIsInTSFMode) {
    nsTextStore::SetInputContext(aInputContext);
    aInputContext.mNativeIMEContext = nsTextStore::GetTextStore();
    MOZ_ASSERT(aInputContext.mNativeIMEContext);
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
bool
IMEHandler::IsDoingKakuteiUndo(HWND aWnd)
{
  // Following message pattern is caused by "Kakutei-Undo" of ATOK or WXG:
  // ---------------------------------------------------------------------------
  // WM_KEYDOWN              * n (wParam = VK_BACK, lParam = 0x1)
  // WM_KEYUP                * 1 (wParam = VK_BACK, lParam = 0xC0000001) # ATOK
  // WM_IME_STARTCOMPOSITION * 1 (wParam = 0x0, lParam = 0x0)
  // WM_IME_COMPOSITION      * 1 (wParam = 0x0, lParam = 0x1BF)
  // WM_CHAR                 * n (wParam = VK_BACK, lParam = 0x1)
  // WM_KEYUP                * 1 (wParam = VK_BACK, lParam = 0xC00E0001)
  // ---------------------------------------------------------------------------
  // This doesn't match usual key message pattern such as:
  //   WM_KEYDOWN -> WM_CHAR -> WM_KEYDOWN -> WM_CHAR -> ... -> WM_KEYUP
  // See following bugs for the detail.
  // https://bugzilla.mozilla.gr.jp/show_bug.cgi?id=2885 (written in Japanese)
  // https://bugzilla.mozilla.org/show_bug.cgi?id=194559 (written in English)
  MSG startCompositionMsg, compositionMsg, charMsg;
  return WinUtils::PeekMessage(&startCompositionMsg, aWnd,
                               WM_IME_STARTCOMPOSITION, WM_IME_STARTCOMPOSITION,
                               PM_NOREMOVE | PM_NOYIELD) &&
         WinUtils::PeekMessage(&compositionMsg, aWnd, WM_IME_COMPOSITION,
                               WM_IME_COMPOSITION, PM_NOREMOVE | PM_NOYIELD) &&
         WinUtils::PeekMessage(&charMsg, aWnd, WM_CHAR, WM_CHAR,
                               PM_NOREMOVE | PM_NOYIELD) &&
         startCompositionMsg.wParam == 0x0 &&
         startCompositionMsg.lParam == 0x0 &&
         compositionMsg.wParam == 0x0 &&
         compositionMsg.lParam == 0x1BF &&
         charMsg.wParam == VK_BACK && charMsg.lParam == 0x1 &&
         startCompositionMsg.time <= compositionMsg.time &&
         compositionMsg.time <= charMsg.time;
}

} // namespace widget
} // namespace mozilla
