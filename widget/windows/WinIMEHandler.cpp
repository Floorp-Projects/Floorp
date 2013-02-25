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

namespace mozilla {
namespace widget {

/******************************************************************************
 * IMEHandler
 ******************************************************************************/

#ifdef NS_ENABLE_TSF
bool IMEHandler::sIsInTSFMode = false;
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
bool
IMEHandler::ProcessMessage(nsWindow* aWindow, UINT aMessage,
                           WPARAM& aWParam, LPARAM& aLParam,
                           LRESULT* aRetValue, bool& aEatMessage)
{
#ifdef NS_ENABLE_TSF
  if (sIsInTSFMode) {
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
  if (sIsInTSFMode) {
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
  if (sIsInTSFMode) {
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
  if (sIsInTSFMode) {
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
  if (sIsInTSFMode) {
    return nsTextStore::OnTextChange(aStart, aOldEnd, aNewEnd);
  }
#endif //NS_ENABLE_TSF

  return NS_ERROR_NOT_IMPLEMENTED;
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
  return ::PeekMessageW(&startCompositionMsg, aWnd,
                        WM_IME_STARTCOMPOSITION, WM_IME_STARTCOMPOSITION,
                        PM_NOREMOVE | PM_NOYIELD) &&
         ::PeekMessageW(&compositionMsg, aWnd, WM_IME_COMPOSITION,
                        WM_IME_COMPOSITION, PM_NOREMOVE | PM_NOYIELD) &&
         ::PeekMessageW(&charMsg, aWnd, WM_CHAR, WM_CHAR,
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
