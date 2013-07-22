/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WinIMEHandler_h_
#define WinIMEHandler_h_

#include "nscore.h"
#include "nsEvent.h"
#include "nsIWidget.h"
#include <windows.h>
#include <inputscope.h>

#define NS_WM_IMEFIRST WM_IME_SETCONTEXT
#define NS_WM_IMELAST  WM_IME_KEYUP

class nsWindow;

namespace mozilla {
namespace widget {

struct MSGResult;

/**
 * IMEHandler class is a mediator class.  On Windows, there are two IME API
 * sets: One is IMM which is legacy API set. The other is TSF which is modern
 * API set. By using this class, non-IME handler classes don't need to worry
 * that we're in which mode.
 */
class IMEHandler MOZ_FINAL
{
public:
  static void Initialize();
  static void Terminate();

  /**
   * Returns TSF related native data.
   */
  static void* GetNativeData(uint32_t aDataType);

  /**
   * ProcessRawKeyMessage() message is called before calling TranslateMessage()
   * and DispatchMessage().  If this returns true, the message is consumed.
   * Then, caller must not perform TranslateMessage() nor DispatchMessage().
   */
  static bool ProcessRawKeyMessage(const MSG& aMsg);

  /**
   * When the message is not needed to handle anymore by the caller, this
   * returns true.  Otherwise, false.
   */
  static bool ProcessMessage(nsWindow* aWindow, UINT aMessage,
                             WPARAM& aWParam, LPARAM& aLParam,
                             MSGResult& aResult);

  /**
   * When there is a composition, returns true.  Otherwise, false.
   */
  static bool IsComposing();

  /**
   * When there is a composition and it's in the window, returns true.
   * Otherwise, false.
   */
  static bool IsComposingOn(nsWindow* aWindow);

  /**
   * Notifies IME of the notification (a request or an event).
   */
  static nsresult NotifyIME(nsWindow* aWindow,
                            NotificationToIME aNotification);

  /**
   * Notifies IME of text change in the focused editable content.
   */
  static nsresult NotifyIMEOfTextChange(uint32_t aStart,
                                        uint32_t aOldEnd,
                                        uint32_t aNewEnd);

  /**
   * Returns update preferences.
   */
  static nsIMEUpdatePreference GetUpdatePreference();

  /**
   * Returns IME open state on the window.
   */
  static bool GetOpenState(nsWindow* aWindow);

  /**
   * Called when the window is destroying.
   */
  static void OnDestroyWindow(nsWindow* aWindow);

  /**
   * Called when nsIWidget::SetInputContext() is called before the window's
   * InputContext is modified actually.
   */
  static void SetInputContext(nsWindow* aWindow,
                              InputContext& aInputContext,
                              const InputContextAction& aAction);

  /**
   * Called when the window is created.
   */
  static void InitInputContext(nsWindow* aWindow, InputContext& aInputContext);

#ifdef DEBUG
  /**
   * Returns true when current keyboard layout has IME.  Otherwise, false.
   */
  static bool CurrentKeyboardLayoutHasIME();
#endif // #ifdef DEBUG

private:
#ifdef NS_ENABLE_TSF
  typedef HRESULT (WINAPI *SetInputScopesFunc)(HWND windowHandle,
                                               const InputScope *inputScopes,
                                               UINT numInputScopes,
                                               wchar_t **phrase_list,
                                               UINT numPhraseList,
                                               wchar_t *regExp,
                                               wchar_t *srgs);
  static SetInputScopesFunc sSetInputScopes;
  static void SetInputScopeForIMM32(nsWindow* aWindow,
                                    const nsAString& aHTMLInputType);
  static bool sIsInTSFMode;
  static bool sPluginHasFocus;

  static bool IsTSFAvailable() { return (sIsInTSFMode && !sPluginHasFocus); }
#endif // #ifdef NS_ENABLE_TSF
};

} // namespace widget
} // namespace mozilla

#endif // #ifndef WinIMEHandler_h_
