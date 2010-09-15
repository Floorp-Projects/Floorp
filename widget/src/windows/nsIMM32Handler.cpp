/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Original nsWindow.cpp Contributor(s):
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Ere Maijala <ere@atp.fi>
 *   Mark Hammond <markh@activestate.com>
 *   Michael Lowe <michael.lowe@bigfoot.com>
 *   Peter Bajusz <hyp-x@inf.bme.hu>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   Roy Yokoyama <yokoyama@netscape.com>
 *   Makoto Kato  <m_kato@ga2.so-net.ne.jp>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Dainis Jonitis <Dainis_Jonitis@swh-t.lv>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Ningjie Chen <chenn@email.uc.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif // MOZ_LOGGING
#include "prlog.h"

#include "nsIMM32Handler.h"
#include "nsWindow.h"

static nsIMM32Handler* gIMM32Handler = nsnull;

#ifdef PR_LOGGING
PRLogModuleInfo* gIMM32Log = nsnull;
#endif

#ifdef ENABLE_IME_MOUSE_HANDLING
static UINT sWM_MSIME_MOUSE = 0; // mouse message for MSIME 98/2000

//-------------------------------------------------------------------------
//
// from http://download.microsoft.com/download/6/0/9/60908e9e-d2c1-47db-98f6-216af76a235f/msime.h
// The document for this has been removed from MSDN...
//
//-------------------------------------------------------------------------

#define RWM_MOUSE           TEXT("MSIMEMouseOperation")

#define IMEMOUSE_NONE       0x00    // no mouse button was pushed
#define IMEMOUSE_LDOWN      0x01
#define IMEMOUSE_RDOWN      0x02
#define IMEMOUSE_MDOWN      0x04
#define IMEMOUSE_WUP        0x10    // wheel up
#define IMEMOUSE_WDOWN      0x20    // wheel down

#endif

PRPackedBool nsIMM32Handler::sIsStatusChanged = PR_FALSE;
PRPackedBool nsIMM32Handler::sIsIME = PR_TRUE;
PRPackedBool nsIMM32Handler::sIsIMEOpening = PR_FALSE;

#ifndef WINCE
UINT nsIMM32Handler::sCodePage = 0;
DWORD nsIMM32Handler::sIMEProperty = 0;
#endif

/* static */ void
nsIMM32Handler::EnsureHandlerInstance()
{
  if (!gIMM32Handler) {
    gIMM32Handler = new nsIMM32Handler();
  }
}

/* static */ void
nsIMM32Handler::Initialize()
{
#ifdef PR_LOGGING
  if (!gIMM32Log)
    gIMM32Log = PR_NewLogModule("nsIMM32HandlerWidgets");
#endif

#ifdef ENABLE_IME_MOUSE_HANDLING
  if (!sWM_MSIME_MOUSE) {
    sWM_MSIME_MOUSE = ::RegisterWindowMessage(RWM_MOUSE);
  }
#endif

  InitKeyboardLayout(::GetKeyboardLayout(0));
}

/* static */ void
nsIMM32Handler::Terminate()
{
  if (!gIMM32Handler)
    return;
  delete gIMM32Handler;
  gIMM32Handler = nsnull;
}

/* static */ PRBool
nsIMM32Handler::IsComposingOnOurEditor()
{
  return gIMM32Handler && gIMM32Handler->mIsComposing;
}

/* static */ PRBool
nsIMM32Handler::IsComposingOnPlugin()
{
  return gIMM32Handler && gIMM32Handler->mIsComposingOnPlugin;
}

/* static */ PRBool
nsIMM32Handler::IsComposingWindow(nsWindow* aWindow)
{
  return gIMM32Handler && gIMM32Handler->mComposingWindow == aWindow;
}

/* static */ PRBool
nsIMM32Handler::IsTopLevelWindowOfComposition(nsWindow* aWindow)
{
  if (!gIMM32Handler || !gIMM32Handler->mComposingWindow) {
    return PR_FALSE;
  }
  HWND wnd = gIMM32Handler->mComposingWindow->GetWindowHandle();
  return nsWindow::GetTopLevelHWND(wnd, PR_TRUE) == aWindow->GetWindowHandle();
}

/* static */ PRBool
nsIMM32Handler::IsDoingKakuteiUndo(HWND aWnd)
{
  // This message pattern is "Kakutei-Undo" on ATOK and WXG.
  // In this case, the message queue has following messages:
  // ---------------------------------------------------------------------------
  // WM_KEYDOWN              * n (wParam = VK_BACK, lParam = 0x1)
  // WM_KEYUP                * 1 (wParam = VK_BACK, lParam = 0xC0000001) # ATOK
  // WM_IME_STARTCOMPOSITION * 1 (wParam = 0x0, lParam = 0x0)
  // WM_IME_COMPOSITION      * 1 (wParam = 0x0, lParam = 0x1BF)
  // WM_CHAR                 * n (wParam = VK_BACK, lParam = 0x1)
  // WM_KEYUP                * 1 (wParam = VK_BACK, lParam = 0xC00E0001)
  // ---------------------------------------------------------------------------
  // This message pattern does not match to the above case;
  // i.e.,WM_KEYDOWN -> WM_CHAR -> WM_KEYDOWN -> WM_CHAR.
  // For more information of this problem:
  // https://bugzilla.mozilla.gr.jp/show_bug.cgi?id=2885 (written in Japanese)
  // https://bugzilla.mozilla.org/show_bug.cgi?id=194559 (written in English)
  MSG imeStartCompositionMsg, imeCompositionMsg, charMsg;
  return ::PeekMessageW(&imeStartCompositionMsg, aWnd,
                        WM_IME_STARTCOMPOSITION, WM_IME_STARTCOMPOSITION,
                        PM_NOREMOVE | PM_NOYIELD) &&
         ::PeekMessageW(&imeCompositionMsg, aWnd, WM_IME_COMPOSITION,
                        WM_IME_COMPOSITION, PM_NOREMOVE | PM_NOYIELD) &&
         ::PeekMessageW(&charMsg, aWnd, WM_CHAR, WM_CHAR,
                        PM_NOREMOVE | PM_NOYIELD) &&
         imeStartCompositionMsg.wParam == 0x0 &&
         imeStartCompositionMsg.lParam == 0x0 &&
         imeCompositionMsg.wParam == 0x0 &&
         imeCompositionMsg.lParam == 0x1BF &&
         charMsg.wParam == VK_BACK && charMsg.lParam == 0x1 &&
         imeStartCompositionMsg.time <= imeCompositionMsg.time &&
         imeCompositionMsg.time <= charMsg.time;
}

/* static */ PRBool
nsIMM32Handler::ShouldDrawCompositionStringOurselves()
{
#ifdef WINCE
  // We are not sure we should use native IME behavior...
  return PR_TRUE;
#else
  // If current IME has special UI or its composition window should not
  // positioned to caret position, we should now draw composition string
  // ourselves.
  return !(sIMEProperty & IME_PROP_SPECIAL_UI) &&
          (sIMEProperty & IME_PROP_AT_CARET);
#endif
}

/* static */ void
nsIMM32Handler::InitKeyboardLayout(HKL aKeyboardLayout)
{
#ifndef WINCE
  WORD langID = LOWORD(aKeyboardLayout);
  ::GetLocaleInfoW(MAKELCID(langID, SORT_DEFAULT),
                   LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                   (PWSTR)&sCodePage, sizeof(sCodePage) / sizeof(WCHAR));
  sIMEProperty = ::ImmGetProperty(aKeyboardLayout, IGP_PROPERTY);
  sIsIME = ::ImmIsIME(aKeyboardLayout);
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: InitKeyboardLayout, aKeyboardLayout=%08x, sCodePage=%lu, sIMEProperty=%08x sIsIME=%s\n",
     aKeyboardLayout, sCodePage, sIMEProperty, sIsIME ? "TRUE" : "FALSE"));
#endif
}

/* static */ UINT
nsIMM32Handler::GetKeyboardCodePage()
{
#ifdef WINCE
  return ::GetACP();
#else
  return sCodePage;
#endif
}

/* static */ PRBool
nsIMM32Handler::CanOptimizeKeyAndIMEMessages(MSG *aNextKeyOrIMEMessage)
{
#ifdef WINCE
  return PR_TRUE;
#else
  // If IME is opening right now, we shouldn't optimize the key and IME message
  // order because ATOK (Japanese IME of third party) has some problem with the
  // optimization.  When it finishes opening completely, it eats all key
  // messages in the message queue.  And it causes starting composition.  So,
  // we shouldn't eat the key messages before ATOK.
  return !sIsIMEOpening;
#endif
}


// used for checking the lParam of WM_IME_COMPOSITION
#define IS_COMPOSING_LPARAM(lParam) \
  ((lParam) & (GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS))
#define IS_COMMITTING_LPARAM(lParam) ((lParam) & GCS_RESULTSTR)
// Some IMEs (e.g., the standard IME for Korean) don't have caret position,
// then, we should not set caret position to text event.
#define NO_IME_CARET -1

nsIMM32Handler::nsIMM32Handler() :
  mComposingWindow(nsnull), mCursorPosition(NO_IME_CARET), mCompositionStart(0),
  mIsComposing(PR_FALSE), mIsComposingOnPlugin(PR_FALSE),
  mNativeCaretIsCreated(PR_FALSE)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS, ("IMM32: nsIMM32Handler is created\n"));
}

nsIMM32Handler::~nsIMM32Handler()
{
  if (mIsComposing) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: ~nsIMM32Handler, ERROR, the instance is still composing\n"));
  }
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS, ("IMM32: nsIMM32Handler is destroyed\n"));
}

nsresult
nsIMM32Handler::EnsureClauseArray(PRInt32 aCount)
{
  NS_ENSURE_ARG_MIN(aCount, 0);
  if (!mClauseArray.SetCapacity(aCount + 32)) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: EnsureClauseArray, aCount=%ld, FAILED to allocate\n",
       aCount));
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
nsIMM32Handler::EnsureAttributeArray(PRInt32 aCount)
{
  NS_ENSURE_ARG_MIN(aCount, 0);
  if (!mAttributeArray.SetCapacity(aCount + 64)) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: EnsureAttributeArray, aCount=%ld, FAILED to allocate\n",
       aCount));
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

/* static */ void
nsIMM32Handler::CommitComposition(nsWindow* aWindow, PRBool aForce)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: CommitComposition, aForce=%s, aWindow=%p, hWnd=%08x, mComposingWindow=%p%s\n",
     aForce ? "TRUE" : "FALSE",
     aWindow, aWindow->GetWindowHandle(),
     gIMM32Handler ? gIMM32Handler->mComposingWindow : nsnull,
     gIMM32Handler && gIMM32Handler->mComposingWindow ?
       IsComposingOnOurEditor() ? " (composing on editor)" :
                                  " (composing on plug-in)" : ""));
  if (!aForce && !IsComposingWindow(aWindow)) {
    return;
  }
  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  if (IMEContext.IsValid()) {
    ::ImmNotifyIME(IMEContext.get(), NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
    ::ImmNotifyIME(IMEContext.get(), NI_COMPOSITIONSTR, CPS_CANCEL, 0);
  }
}

/* static */ void
nsIMM32Handler::CancelComposition(nsWindow* aWindow, PRBool aForce)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: CancelComposition, aForce=%s, aWindow=%p, hWnd=%08x, mComposingWindow=%p%s\n",
     aForce ? "TRUE" : "FALSE",
     aWindow, aWindow->GetWindowHandle(),
     gIMM32Handler ? gIMM32Handler->mComposingWindow : nsnull,
     gIMM32Handler && gIMM32Handler->mComposingWindow ?
       IsComposingOnOurEditor() ? " (composing on editor)" :
                                  " (composing on plug-in)" : ""));
  if (!aForce && !IsComposingWindow(aWindow)) {
    return;
  }
  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  if (IMEContext.IsValid()) {
    ::ImmNotifyIME(IMEContext.get(), NI_COMPOSITIONSTR, CPS_CANCEL, 0);
  }
}

/* static */ PRBool
nsIMM32Handler::ProcessInputLangChangeMessage(nsWindow* aWindow,
                                              WPARAM wParam,
                                              LPARAM lParam,
                                              LRESULT *aRetValue,
                                              PRBool &aEatMessage)
{
  *aRetValue = 0;
  aEatMessage = PR_FALSE;
  // We don't need to create the instance of the handler here.
  if (gIMM32Handler) {
    aEatMessage = gIMM32Handler->OnInputLangChange(aWindow, wParam, lParam);
  }
  InitKeyboardLayout(reinterpret_cast<HKL>(lParam));
  // We can release the instance here, because the instance may be never
  // used. E.g., the new keyboard layout may not use IME, or it may use TSF.
  Terminate();
  // Don't return as "processed", the messages should be processed on nsWindow
  // too.
  return PR_FALSE;
}

/* static */ PRBool
nsIMM32Handler::ProcessMessage(nsWindow* aWindow, UINT msg,
                               WPARAM &wParam, LPARAM &lParam,
                               LRESULT *aRetValue, PRBool &aEatMessage)
{
  // XXX We store the composing window in mComposingWindow.  If IME messages are
  // sent to different window, we should commit the old transaction.  And also
  // if the new window handle is not focused, probably, we should not start
  // the composition, however, such case should not be, it's just bad scenario.

  if (sIsIMEOpening) {
    switch (msg) {
      case WM_INPUTLANGCHANGE:
      case WM_IME_STARTCOMPOSITION:
      case WM_IME_COMPOSITION:
      case WM_IME_ENDCOMPOSITION:
      case WM_IME_CHAR:
      case WM_IME_SELECT:
      case WM_IME_SETCONTEXT:
        // For safety, we should reset sIsIMEOpening when we receive unexpected
        // message.
        sIsIMEOpening = PR_FALSE;
    }
  }

  // When a plug-in has focus or compsition, we should dispatch the IME events
  // to the plug-in.
  if (aWindow->PluginHasFocus() || IsComposingOnPlugin()) {
      return ProcessMessageForPlugin(aWindow, msg, wParam, lParam, aRetValue,
                                   aEatMessage);
  }

  *aRetValue = 0;
  switch (msg) {
#ifdef ENABLE_IME_MOUSE_HANDLING
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      // We don't need to create the instance of the handler here.
      if (!gIMM32Handler)
        return PR_FALSE;
      if (!gIMM32Handler->OnMouseEvent(aWindow, lParam,
                            msg == WM_LBUTTONDOWN ? IMEMOUSE_LDOWN :
                            msg == WM_MBUTTONDOWN ? IMEMOUSE_MDOWN :
                                                    IMEMOUSE_RDOWN)) {
        return PR_FALSE;
      }
      aEatMessage = PR_FALSE;
      return PR_TRUE;
    }
#endif // ENABLE_IME_MOUSE_HANDLING
    case WM_INPUTLANGCHANGE:
      return ProcessInputLangChangeMessage(aWindow, wParam, lParam,
                                           aRetValue, aEatMessage);
    case WM_IME_STARTCOMPOSITION:
      EnsureHandlerInstance();
      aEatMessage = gIMM32Handler->OnIMEStartComposition(aWindow);
      return PR_TRUE;
    case WM_IME_COMPOSITION:
      EnsureHandlerInstance();
      aEatMessage = gIMM32Handler->OnIMEComposition(aWindow, wParam, lParam);
      return PR_TRUE;
    case WM_IME_ENDCOMPOSITION:
      EnsureHandlerInstance();
      aEatMessage = gIMM32Handler->OnIMEEndComposition(aWindow);
      return PR_TRUE;
    case WM_IME_CHAR:
      aEatMessage = OnIMEChar(aWindow, wParam, lParam);
      return PR_TRUE;
    case WM_IME_NOTIFY:
      aEatMessage = OnIMENotify(aWindow, wParam, lParam);
      return PR_TRUE;
    case WM_IME_REQUEST:
      EnsureHandlerInstance();
      aEatMessage =
        gIMM32Handler->OnIMERequest(aWindow, wParam, lParam, aRetValue);
      return PR_TRUE;
    case WM_IME_SELECT:
      aEatMessage = OnIMESelect(aWindow, wParam, lParam);
      return PR_TRUE;
    case WM_IME_SETCONTEXT:
      aEatMessage = OnIMESetContext(aWindow, wParam, lParam, aRetValue);
      return PR_TRUE;
    case WM_KEYDOWN:
      return OnKeyDownEvent(aWindow, wParam, lParam, aEatMessage);
    case WM_CHAR:
      if (!gIMM32Handler) {
        return PR_FALSE;
      }
      aEatMessage = gIMM32Handler->OnChar(aWindow, wParam, lParam);
      // If we eat this message, we should return "processed", otherwise,
      // the message should be handled on nsWindow, so, we should return
      // "not processed" at that time.
      return aEatMessage;
    default:
      return PR_FALSE;
  };
}

/* static */ PRBool
nsIMM32Handler::ProcessMessageForPlugin(nsWindow* aWindow, UINT msg,
                                        WPARAM &wParam, LPARAM &lParam,
                                        LRESULT *aRetValue,
                                        PRBool &aEatMessage)
{
  *aRetValue = 0;
  aEatMessage = PR_FALSE;
  switch (msg) {
    case WM_INPUTLANGCHANGEREQUEST:
    case WM_INPUTLANGCHANGE:
      aWindow->DispatchPluginEvent(msg, wParam, lParam, PR_FALSE);
      return ProcessInputLangChangeMessage(aWindow, wParam, lParam,
                                           aRetValue, aEatMessage);
    case WM_IME_COMPOSITION:
      EnsureHandlerInstance();
      aEatMessage =
        gIMM32Handler->OnIMECompositionOnPlugin(aWindow, wParam, lParam);
      return PR_TRUE;
    case WM_IME_STARTCOMPOSITION:
      EnsureHandlerInstance();
      aEatMessage =
        gIMM32Handler->OnIMEStartCompositionOnPlugin(aWindow, wParam, lParam);
      return PR_TRUE;
    case WM_IME_ENDCOMPOSITION:
      EnsureHandlerInstance();
      aEatMessage =
        gIMM32Handler->OnIMEEndCompositionOnPlugin(aWindow, wParam, lParam);
      return PR_TRUE;
    case WM_IME_CHAR:
      EnsureHandlerInstance();
      aEatMessage =
        gIMM32Handler->OnIMECharOnPlugin(aWindow, wParam, lParam);
      return PR_TRUE;
    case WM_IME_SETCONTEXT:
      aEatMessage = OnIMESetContextOnPlugin(aWindow, wParam, lParam, aRetValue);
      return PR_TRUE;
    case WM_IME_NOTIFY:
      if (wParam == IMN_SETOPENSTATUS) {
        // finished being opening
        sIsIMEOpening = PR_FALSE;
      }
      return PR_FALSE;
    case WM_KEYDOWN:
      if (wParam == VK_PROCESSKEY) {
        // If we receive when IME isn't open, it means IME is opening right now.
        nsIMEContext IMEContext(aWindow->GetWindowHandle());
        sIsIMEOpening = IMEContext.IsValid() &&
                        ::ImmGetOpenStatus(IMEContext.get());
      }
      return PR_FALSE;
    case WM_CHAR:
      if (!gIMM32Handler) {
        return PR_FALSE;
      }
      aEatMessage =
        gIMM32Handler->OnCharOnPlugin(aWindow, wParam, lParam);
      return PR_FALSE;  // is going to be handled by nsWindow.
    case WM_IME_COMPOSITIONFULL:
    case WM_IME_CONTROL:
    case WM_IME_KEYDOWN:
    case WM_IME_KEYUP:
    case WM_IME_REQUEST:
    case WM_IME_SELECT:
      aEatMessage = aWindow->DispatchPluginEvent(msg, wParam, lParam, PR_FALSE);
      return PR_TRUE;
  }
  return PR_FALSE;
}

/****************************************************************************
 * message handlers
 ****************************************************************************/

PRBool
nsIMM32Handler::OnInputLangChange(nsWindow* aWindow,
                                  WPARAM wParam,
                                  LPARAM lParam)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnInputLangChange, hWnd=%08x, wParam=%08x, lParam=%08x\n",
     aWindow->GetWindowHandle(), wParam, lParam));

  aWindow->ResetInputState();
  NS_ASSERTION(!mIsComposing, "ResetInputState failed");

  if (mIsComposing) {
    HandleEndComposition(aWindow);
  }

  return PR_FALSE;
}

PRBool
nsIMM32Handler::OnIMEStartComposition(nsWindow* aWindow)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMEStartComposition, hWnd=%08x, mIsComposing=%s\n",
     aWindow->GetWindowHandle(), mIsComposing ? "TRUE" : "FALSE"));
  if (mIsComposing) {
    NS_WARNING("Composition has been already started");
    return ShouldDrawCompositionStringOurselves();
  }

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  HandleStartComposition(aWindow, IMEContext);
  return ShouldDrawCompositionStringOurselves();
}

PRBool
nsIMM32Handler::OnIMEComposition(nsWindow* aWindow,
                                 WPARAM wParam,
                                 LPARAM lParam)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMEComposition, hWnd=%08x, lParam=%08x, mIsComposing=%s\n",
     aWindow->GetWindowHandle(), lParam, mIsComposing ? "TRUE" : "FALSE"));
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMEComposition, GCS_RESULTSTR=%s, GCS_COMPSTR=%s, GCS_COMPATTR=%s, GCS_COMPCLAUSE=%s, GCS_CURSORPOS=%s\n",
     lParam & GCS_RESULTSTR  ? "YES" : "no",
     lParam & GCS_COMPSTR    ? "YES" : "no",
     lParam & GCS_COMPATTR   ? "YES" : "no",
     lParam & GCS_COMPCLAUSE ? "YES" : "no",
     lParam & GCS_CURSORPOS  ? "YES" : "no"));

  NS_PRECONDITION(!aWindow->PluginHasFocus(),
    "OnIMEComposition should not be called when a plug-in has focus");

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  return HandleComposition(aWindow, IMEContext, lParam);
}

PRBool
nsIMM32Handler::OnIMEEndComposition(nsWindow* aWindow)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMEEndComposition, hWnd=%08x, mIsComposing=%s\n",
     aWindow->GetWindowHandle(), mIsComposing ? "TRUE" : "FALSE"));

  if (!mIsComposing) {
    return ShouldDrawCompositionStringOurselves();
  }

  // IME on Korean NT somehow send WM_IME_ENDCOMPOSITION
  // first when we hit space in composition mode
  // we need to clear out the current composition string
  // in that case.
  mCompositionString.Truncate();

  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  DispatchTextEvent(aWindow, IMEContext, PR_FALSE);

  HandleEndComposition(aWindow);

  return ShouldDrawCompositionStringOurselves();
}

/* static */ PRBool
nsIMM32Handler::OnIMEChar(nsWindow* aWindow,
                          WPARAM wParam,
                          LPARAM lParam)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMEChar, hWnd=%08x, char=%08x\n",
     aWindow->GetWindowHandle(), wParam));

  // We don't need to fire any text events from here. This method will be
  // called when the composition string of the current IME is not drawn by us
  // and some characters are committed. In that case, the committed string was
  // processed in nsWindow::OnIMEComposition already.

  // We need to return TRUE here so that Windows don't send two WM_CHAR msgs
  return PR_TRUE;
}

/* static */ PRBool
nsIMM32Handler::OnIMECompositionFull(nsWindow* aWindow)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMECompositionFull, hWnd=%08x\n",
     aWindow->GetWindowHandle()));

  // not implement yet
  return PR_FALSE;
}

/* static */ PRBool
nsIMM32Handler::OnIMENotify(nsWindow* aWindow,
                            WPARAM wParam,
                            LPARAM lParam)
{
#ifdef PR_LOGGING
  switch (wParam) {
    case IMN_CHANGECANDIDATE:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_CHANGECANDIDATE, lParam=%08x\n",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_CLOSECANDIDATE:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_CLOSECANDIDATE, lParam=%08x\n",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_CLOSESTATUSWINDOW:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_CLOSESTATUSWINDOW\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_GUIDELINE:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_GUIDELINE\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_OPENCANDIDATE:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_OPENCANDIDATE, lParam=%08x\n",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_OPENSTATUSWINDOW:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_OPENSTATUSWINDOW\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETCANDIDATEPOS:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETCANDIDATEPOS, lParam=%08x\n",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_SETCOMPOSITIONFONT:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETCOMPOSITIONFONT\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETCOMPOSITIONWINDOW:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETCOMPOSITIONWINDOW\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETCONVERSIONMODE:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETCONVERSIONMODE\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETOPENSTATUS:
      sIsIMEOpening = PR_FALSE;
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETOPENSTATUS\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETSENTENCEMODE:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETSENTENCEMODE\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETSTATUSWINDOWPOS:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_SETSTATUSWINDOWPOS\n",
         aWindow->GetWindowHandle()));
      break;
    case IMN_PRIVATE:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMENotify, hWnd=%08x, IMN_PRIVATE\n",
         aWindow->GetWindowHandle()));
      break;
  }
#endif // PR_LOGGING

  if (::GetKeyState(NS_VK_ALT) >= 0) {
    return PR_FALSE;
  }

  // XXXmnakano Following code was added by bug 28852 (Key combo to trun ON/OFF
  // Japanese IME incorrectly activates "File" menu).  If one or more keypress
  // events come between Alt keydown event and Alt keyup event, XUL menubar
  // isn't activated by the Alt keyup event.  Therefore, this code sends dummy
  // keypress event to Gecko.  But this is ugly, and this fires incorrect DOM
  // keypress event.  So, we should find another way for the bug.

  // add hacky code here
  nsModifierKeyState modKeyState(PR_FALSE, PR_FALSE, PR_TRUE);
  aWindow->DispatchKeyEvent(NS_KEY_PRESS, 0, nsnull, 192, nsnull, modKeyState);
  sIsStatusChanged = sIsStatusChanged || (wParam == IMN_SETOPENSTATUS);
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMENotify, sIsStatusChanged=%s\n",
     sIsStatusChanged ? "TRUE" : "FALSE"));

  // not implement yet
  return PR_FALSE;
}

PRBool
nsIMM32Handler::OnIMERequest(nsWindow* aWindow,
                             WPARAM wParam,
                             LPARAM lParam,
                             LRESULT *oResult)
{
  switch (wParam) {
    case IMR_RECONVERTSTRING:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMERequest, hWnd=%08x, IMR_RECONVERTSTRING\n",
         aWindow->GetWindowHandle()));
      return HandleReconvert(aWindow, lParam, oResult);
    case IMR_QUERYCHARPOSITION:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMERequest, hWnd=%08x, IMR_QUERYCHARPOSITION\n",
         aWindow->GetWindowHandle()));
      return HandleQueryCharPosition(aWindow, lParam, oResult);
    case IMR_DOCUMENTFEED:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMERequest, hWnd=%08x, IMR_DOCUMENTFEED\n",
         aWindow->GetWindowHandle()));
      return HandleDocumentFeed(aWindow, lParam, oResult);
    default:
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: OnIMERequest, hWnd=%08x, wParam=%08x\n",
         aWindow->GetWindowHandle(), wParam));
      return PR_FALSE;
  }
}

/* static */ PRBool
nsIMM32Handler::OnIMESelect(nsWindow* aWindow,
                            WPARAM wParam,
                            LPARAM lParam)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMESelect, hWnd=%08x, wParam=%08x, lParam=%08x\n",
     aWindow->GetWindowHandle(), wParam, lParam));

  // not implement yet
  return PR_FALSE;
}

/* static */ PRBool
nsIMM32Handler::OnIMESetContext(nsWindow* aWindow,
                                WPARAM wParam,
                                LPARAM lParam,
                                LRESULT *aResult)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMESetContext, hWnd=%08x, %s, lParam=%08x\n",
     aWindow->GetWindowHandle(), wParam ? "Active" : "Deactive", lParam));

  // NOTE: If the aWindow is top level window of the composing window because
  // when a window on deactive window gets focus, WM_IME_SETCONTEXT (wParam is
  // TRUE) is sent to the top level window first.  After that,
  // WM_IME_SETCONTEXT (wParam is FALSE) is sent to the top level window.
  // Finally, WM_IME_SETCONTEXT (wParam is TRUE) is sent to the focused window.
  // The top level window never becomes composing window, so, we can ignore
  // the WM_IME_SETCONTEXT on the top level window.
  if (IsTopLevelWindowOfComposition(aWindow)) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: OnIMESetContext, hWnd=%08x is top level window\n"));
    return PR_FALSE;
  }

  // When IME context is activating on another window,
  // we should commit the old composition on the old window.
  PRBool cancelComposition = PR_FALSE;
  if (wParam && gIMM32Handler) {
    cancelComposition =
      gIMM32Handler->CommitCompositionOnPreviousWindow(aWindow);
  }

  if (wParam && (lParam & ISC_SHOWUICOMPOSITIONWINDOW) &&
      ShouldDrawCompositionStringOurselves()) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: OnIMESetContext, ISC_SHOWUICOMPOSITIONWINDOW is removed\n"));
    lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
  }

  // We should sent WM_IME_SETCONTEXT to the DefWndProc here because the
  // ancestor windows shouldn't receive this message.  If they receive the
  // message, we cannot know whether which window is the target of the message.
  *aResult = ::DefWindowProc(aWindow->GetWindowHandle(),
                             WM_IME_SETCONTEXT, wParam, lParam);

  // Cancel composition on the new window if we committed our composition on
  // another window.
  if (cancelComposition) {
    CancelComposition(aWindow, PR_TRUE);
  }

  return PR_TRUE;
}

PRBool
nsIMM32Handler::OnChar(nsWindow* aWindow,
                       WPARAM wParam,
                       LPARAM lParam)
{
  if (IsIMECharRecordsEmpty()) {
    return PR_FALSE;
  }
  WPARAM recWParam;
  LPARAM recLParam;
  DequeueIMECharRecords(recWParam, recLParam);
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnChar, aWindow=%p, wParam=%08x, lParam=%08x,\n",
     aWindow->GetWindowHandle(), wParam, lParam));
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("               recorded: wParam=%08x, lParam=%08x\n",
     recWParam, recLParam));
  // If an unexpected char message comes, we should reset the records,
  // of course, this shouldn't happen.
  if (recWParam != wParam || recLParam != lParam) {
    ResetIMECharRecords();
    return PR_FALSE;
  }
  // Eat the char message which is caused by WM_IME_CHAR because we should
  // have processed the IME messages, so, this message could be come from
  // a windowless plug-in.
  return PR_TRUE;
}

/****************************************************************************
 * message handlers for plug-in
 ****************************************************************************/

PRBool
nsIMM32Handler::OnIMEStartCompositionOnPlugin(nsWindow* aWindow,
                                              WPARAM wParam,
                                              LPARAM lParam)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMEStartCompositionOnPlugin, hWnd=%08x, mIsComposingOnPlugin=%s\n",
     aWindow->GetWindowHandle(), mIsComposingOnPlugin ? "TRUE" : "FALSE"));
  mIsComposingOnPlugin = PR_TRUE;
  mComposingWindow = aWindow;
  PRBool handled =
    aWindow->DispatchPluginEvent(WM_IME_STARTCOMPOSITION, wParam, lParam,
                                 PR_FALSE);
  return handled;
}

PRBool
nsIMM32Handler::OnIMECompositionOnPlugin(nsWindow* aWindow,
                                         WPARAM wParam,
                                         LPARAM lParam)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMECompositionOnPlugin, hWnd=%08x, lParam=%08x, mIsComposingOnPlugin=%s\n",
     aWindow->GetWindowHandle(), lParam,
     mIsComposingOnPlugin ? "TRUE" : "FALSE"));
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMECompositionOnPlugin, GCS_RESULTSTR=%s, GCS_COMPSTR=%s, GCS_COMPATTR=%s, GCS_COMPCLAUSE=%s, GCS_CURSORPOS=%s\n",
     lParam & GCS_RESULTSTR  ? "YES" : "no",
     lParam & GCS_COMPSTR    ? "YES" : "no",
     lParam & GCS_COMPATTR   ? "YES" : "no",
     lParam & GCS_COMPCLAUSE ? "YES" : "no",
     lParam & GCS_CURSORPOS  ? "YES" : "no"));
  // We should end composition if there is a committed string.
  if (IS_COMMITTING_LPARAM(lParam)) {
    mIsComposingOnPlugin = PR_FALSE;
    mComposingWindow = nsnull;
  }
  // Continue composition if there is still a string being composed.
  if (IS_COMPOSING_LPARAM(lParam)) {
    mIsComposingOnPlugin = PR_TRUE;
    mComposingWindow = aWindow;
  }
  PRBool handled =
    aWindow->DispatchPluginEvent(WM_IME_COMPOSITION, wParam, lParam, PR_TRUE);
  return handled;
}

PRBool
nsIMM32Handler::OnIMEEndCompositionOnPlugin(nsWindow* aWindow,
                                            WPARAM wParam,
                                            LPARAM lParam)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMEEndCompositionOnPlugin, hWnd=%08x, mIsComposingOnPlugin=%s\n",
     aWindow->GetWindowHandle(), mIsComposingOnPlugin ? "TRUE" : "FALSE"));

  mIsComposingOnPlugin = PR_FALSE;
  mComposingWindow = nsnull;
  PRBool handled =
    aWindow->DispatchPluginEvent(WM_IME_ENDCOMPOSITION, wParam, lParam,
                                 PR_FALSE);
  return handled;
}

PRBool
nsIMM32Handler::OnIMECharOnPlugin(nsWindow* aWindow,
                                  WPARAM wParam,
                                  LPARAM lParam)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMECharOnPlugin, hWnd=%08x, char=%08x, scancode=%08x\n",
     aWindow->GetWindowHandle(), wParam, lParam));

  PRBool handled =
    aWindow->DispatchPluginEvent(WM_IME_CHAR, wParam, lParam, PR_TRUE);

  if (!handled) {
    // Record the WM_CHAR messages which are going to be coming.
    EnsureHandlerInstance();
    EnqueueIMECharRecords(wParam, lParam);
  }
  return handled;
}

/* static */ PRBool
nsIMM32Handler::OnIMESetContextOnPlugin(nsWindow* aWindow,
                                        WPARAM wParam,
                                        LPARAM lParam,
                                        LRESULT *aResult)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnIMESetContextOnPlugin, hWnd=%08x, %s, lParam=%08x\n",
     aWindow->GetWindowHandle(), wParam ? "Active" : "Deactive", lParam));

  // If the IME context becomes active on a plug-in, we should commit
  // our composition.  And also we should cancel the composition on new
  // window.  Note that if IsTopLevelWindowOfComposition(aWindow) returns
  // true, we should ignore the message here, see the comment in
  // OnIMESetContext() for the detail.
  if (wParam && gIMM32Handler && !IsTopLevelWindowOfComposition(aWindow)) {
    if (gIMM32Handler->CommitCompositionOnPreviousWindow(aWindow)) {
      CancelComposition(aWindow);
    }
  }

  // Dispatch message to the plug-in.
  // XXX When a windowless plug-in gets focus, we should send
  //     WM_IME_SETCONTEXT
  aWindow->DispatchPluginEvent(WM_IME_SETCONTEXT, wParam, lParam, PR_FALSE);

  // We should send WM_IME_SETCONTEXT to the DefWndProc here.  It shouldn't
  // be received on ancestor windows, see OnIMESetContext() for the detail.
  *aResult = ::DefWindowProc(aWindow->GetWindowHandle(),
                             WM_IME_SETCONTEXT, wParam, lParam);

  // Don't synchronously dispatch the pending events when we receive
  // WM_IME_SETCONTEXT because we get it during plugin destruction.
  // (bug 491848)
  return PR_TRUE;
}

PRBool
nsIMM32Handler::OnCharOnPlugin(nsWindow* aWindow,
                               WPARAM wParam,
                               LPARAM lParam)
{
  if (IsIMECharRecordsEmpty()) {
    return PR_FALSE;
  }

  WPARAM recWParam;
  LPARAM recLParam;
  DequeueIMECharRecords(recWParam, recLParam);
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnCharOnPlugin, aWindow=%p, wParam=%08x, lParam=%08x,\n",
     aWindow->GetWindowHandle(), wParam, lParam));
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("                       recorded: wParam=%08x, lParam=%08x\n",
     recWParam, recLParam));
  // If an unexpected char message comes, we should reset the records,
  // of course, this shouldn't happen.
  if (recWParam != wParam || recLParam != lParam) {
    ResetIMECharRecords();
  }
  // WM_CHAR on plug-in is always handled by nsWindow.
  return PR_FALSE;
}

/****************************************************************************
 * others
 ****************************************************************************/

void
nsIMM32Handler::HandleStartComposition(nsWindow* aWindow,
                                       const nsIMEContext &aIMEContext)
{
  NS_PRECONDITION(!mIsComposing,
    "HandleStartComposition is called but mIsComposing is TRUE");
  NS_PRECONDITION(!aWindow->PluginHasFocus(),
    "HandleStartComposition should not be called when a plug-in has focus");

  nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT, aWindow);
  nsIntPoint point(0, 0);
  aWindow->InitEvent(selection, &point);
  aWindow->DispatchWindowEvent(&selection);
  if (!selection.mSucceeded) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleStartComposition, FAILED (NS_QUERY_SELECTED_TEXT)\n"));
    return;
  }

  mCompositionStart = selection.mReply.mOffset;

  nsCompositionEvent event(PR_TRUE, NS_COMPOSITION_START, aWindow);
  aWindow->InitEvent(event, &point);
  aWindow->DispatchWindowEvent(&event);

  SetIMERelatedWindowsPos(aWindow, aIMEContext);

  mIsComposing = PR_TRUE;
  mComposingWindow = aWindow;

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: HandleStartComposition, START composition, mCompositionStart=%ld\n",
     mCompositionStart));
}

PRBool
nsIMM32Handler::HandleComposition(nsWindow* aWindow,
                                  const nsIMEContext &aIMEContext,
                                  LPARAM lParam)
{
  NS_PRECONDITION(!aWindow->PluginHasFocus(),
    "HandleComposition should not be called when a plug-in has focus");

  // for bug #60050
  // MS-IME 95/97/98/2000 may send WM_IME_COMPOSITION with non-conversion
  // mode before it send WM_IME_STARTCOMPOSITION.
  // However, ATOK sends a WM_IME_COMPOSITION before WM_IME_STARTCOMPOSITION,
  // and if we access ATOK via some APIs, ATOK will sometimes fail to
  // initialize its state.  If WM_IME_STARTCOMPOSITION is already in the
  // message queue, we should ignore the strange WM_IME_COMPOSITION message and
  // skip to the next.  So, we should look for next composition message
  // (WM_IME_STARTCOMPOSITION or WM_IME_ENDCOMPOSITION or WM_IME_COMPOSITION),
  // and if it's WM_IME_STARTCOMPOSITION, and one more next composition message
  // is WM_IME_COMPOSITION, current IME is ATOK, probably.  Otherwise, we
  // should start composition forcibly.
  if (!mIsComposing) {
    MSG msg1, msg2;
    HWND wnd = aWindow->GetWindowHandle();
    if (::PeekMessageW(&msg1, wnd, WM_IME_STARTCOMPOSITION, WM_IME_COMPOSITION,
                       PM_NOREMOVE) &&
        msg1.message == WM_IME_STARTCOMPOSITION &&
        ::PeekMessageW(&msg2, wnd, WM_IME_ENDCOMPOSITION, WM_IME_COMPOSITION,
                       PM_NOREMOVE) &&
        msg2.message == WM_IME_COMPOSITION) {
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: HandleComposition, Ignores due to find a WM_IME_STARTCOMPOSITION\n"));
      return ShouldDrawCompositionStringOurselves();
    }
  }

  if (!IS_COMMITTING_LPARAM(lParam) && !IS_COMPOSING_LPARAM(lParam)) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleComposition, Handle 0 length TextEvent\n"));

    // XXX This block should be wrong. If composition string is not change,
    // we should do nothing.

    if (!mIsComposing) {
      HandleStartComposition(aWindow, aIMEContext);
    }

    mCompositionString.Truncate();
    DispatchTextEvent(aWindow, aIMEContext, PR_FALSE);

    return ShouldDrawCompositionStringOurselves();
  }


  PRBool startCompositionMessageHasBeenSent = mIsComposing;

  //
  // This catches a fixed result
  //
  if (IS_COMMITTING_LPARAM(lParam)) {
    if (!mIsComposing) {
      HandleStartComposition(aWindow, aIMEContext);
    }

    GetCompositionString(aIMEContext, GCS_RESULTSTR);

    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleComposition, GCS_RESULTSTR\n"));

    DispatchTextEvent(aWindow, aIMEContext, PR_FALSE);
    HandleEndComposition(aWindow);

    if (!IS_COMPOSING_LPARAM(lParam)) {
      return ShouldDrawCompositionStringOurselves();
    }
  }


  //
  // This provides us with a composition string
  //
  if (!mIsComposing) {
    HandleStartComposition(aWindow, aIMEContext);
  }

  //--------------------------------------------------------
  // 1. Get GCS_COMPSTR
  //--------------------------------------------------------
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: HandleComposition, GCS_COMPSTR\n"));

  GetCompositionString(aIMEContext, GCS_COMPSTR);

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=296339
  if (mCompositionString.IsEmpty() && !startCompositionMessageHasBeenSent) {
    // In this case, maybe, the sender is MSPinYin. That sends *only*
    // WM_IME_COMPOSITION with GCS_COMP* and GCS_RESULT* when
    // user inputted the Chinese full stop. So, that doesn't send
    // WM_IME_STARTCOMPOSITION and WM_IME_ENDCOMPOSITION.
    // If WM_IME_STARTCOMPOSITION was not sent and the composition
    // string is null (it indicates the composition transaction ended),
    // WM_IME_ENDCOMPOSITION may not be sent. If so, we cannot run
    // HandleEndComposition() in other place.
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleComposition, Aborting GCS_COMPSTR\n"));
    HandleEndComposition(aWindow);
    return IS_COMMITTING_LPARAM(lParam);
  }

  //--------------------------------------------------------
  // 2. Get GCS_COMPCLAUSE
  //--------------------------------------------------------
  long clauseArrayLength =
    ::ImmGetCompositionStringW(aIMEContext.get(), GCS_COMPCLAUSE, NULL, 0);
  clauseArrayLength /= sizeof(PRUint32);

  if (clauseArrayLength > 0) {
    nsresult rv = EnsureClauseArray(clauseArrayLength);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

#ifndef WINCE
    // Intelligent ABC IME (Simplified Chinese IME, the code page is 936)
    // will crash in ImmGetCompositionStringW for GCS_COMPCLAUSE (bug 424663).
    // See comment 35 of the bug for the detail. Therefore, we should use A
    // API for it, however, we should not kill Unicode support on all IMEs.
    PRBool useA_API = !(sIMEProperty & IME_PROP_UNICODE);

    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleComposition, GCS_COMPCLAUSE, useA_API=%s\n",
       useA_API ? "TRUE" : "FALSE"));
#endif

    long clauseArrayLength2 = 
#ifndef WINCE
      useA_API ?
        ::ImmGetCompositionStringA(aIMEContext.get(), GCS_COMPCLAUSE,
                                   mClauseArray.Elements(),
                                   mClauseArray.Capacity() * sizeof(PRUint32)) :
#endif
        ::ImmGetCompositionStringW(aIMEContext.get(), GCS_COMPCLAUSE,
                                   mClauseArray.Elements(),
                                   mClauseArray.Capacity() * sizeof(PRUint32));
    clauseArrayLength2 /= sizeof(PRUint32);

    if (clauseArrayLength != clauseArrayLength2) {
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: HandleComposition, GCS_COMPCLAUSE, clauseArrayLength=%ld but clauseArrayLength2=%ld\n",
         clauseArrayLength, clauseArrayLength2));
      if (clauseArrayLength > clauseArrayLength2)
        clauseArrayLength = clauseArrayLength2;
    }

#ifndef WINCE
    if (useA_API) {
      // Convert each values of sIMECompClauseArray. The values mean offset of
      // the clauses in ANSI string. But we need the values in Unicode string.
      nsCAutoString compANSIStr;
      if (ConvertToANSIString(mCompositionString, GetKeyboardCodePage(),
                              compANSIStr)) {
        PRUint32 maxlen = compANSIStr.Length();
        mClauseArray[0] = 0; // first value must be 0
        for (PRInt32 i = 1; i < clauseArrayLength; i++) {
          PRUint32 len = PR_MIN(mClauseArray[i], maxlen);
          mClauseArray[i] = ::MultiByteToWideChar(GetKeyboardCodePage(), 
                                                  MB_PRECOMPOSED,
                                                  (LPCSTR)compANSIStr.get(),
                                                  len, NULL, 0);
        }
      }
    }
#endif
  }
  // compClauseArrayLength may be negative. I.e., ImmGetCompositionStringW
  // may return an error code.
  mClauseArray.SetLength(PR_MAX(0, clauseArrayLength));

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: HandleComposition, GCS_COMPCLAUSE, mClauseLength=%ld\n",
     mClauseArray.Length()));

  //--------------------------------------------------------
  // 3. Get GCS_COMPATTR
  //--------------------------------------------------------
  // This provides us with the attribute string necessary 
  // for doing hiliting
  long attrArrayLength =
    ::ImmGetCompositionStringW(aIMEContext.get(), GCS_COMPATTR, NULL, 0);
  attrArrayLength /= sizeof(PRUint8);

  if (attrArrayLength > 0) {
    nsresult rv = EnsureAttributeArray(attrArrayLength);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    attrArrayLength =
      ::ImmGetCompositionStringW(aIMEContext.get(), GCS_COMPATTR,
                                 mAttributeArray.Elements(),
                                 mAttributeArray.Capacity() * sizeof(PRUint8));
  }

  // attrStrLen may be negative. I.e., ImmGetCompositionStringW may return an
  // error code.
  mAttributeArray.SetLength(PR_MAX(0, attrArrayLength));

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: HandleComposition, GCS_COMPATTR, mAttributeLength=%ld\n",
     mAttributeArray.Length()));

  //--------------------------------------------------------
  // 4. Get GCS_CURSOPOS
  //--------------------------------------------------------
  // Some IMEs (e.g., the standard IME for Korean) don't have caret position.
  if (lParam & GCS_CURSORPOS) {
    mCursorPosition =
      ::ImmGetCompositionStringW(aIMEContext.get(), GCS_CURSORPOS, NULL, 0);
    if (mCursorPosition < 0) {
      mCursorPosition = NO_IME_CARET; // The result is error
    }
  } else {
    mCursorPosition = NO_IME_CARET;
  }

  NS_ASSERTION(mCursorPosition <= (long)mCompositionString.Length(),
               "illegal pos");

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: HandleComposition, GCS_CURSORPOS, mCursorPosition=%d\n",
     mCursorPosition));

  //--------------------------------------------------------
  // 5. Send the text event
  //--------------------------------------------------------
  DispatchTextEvent(aWindow, aIMEContext);

  return ShouldDrawCompositionStringOurselves();
}

void
nsIMM32Handler::HandleEndComposition(nsWindow* aWindow)
{
  NS_PRECONDITION(mIsComposing,
    "HandleEndComposition is called but mIsComposing is FALSE");
  NS_PRECONDITION(!aWindow->PluginHasFocus(),
    "HandleComposition should not be called when a plug-in has focus");

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: HandleEndComposition\n"));

  nsCompositionEvent event(PR_TRUE, NS_COMPOSITION_END, aWindow);
  nsIntPoint point(0, 0);

  if (mNativeCaretIsCreated) {
    ::DestroyCaret();
    mNativeCaretIsCreated = PR_FALSE;
  }

  aWindow->InitEvent(event, &point);
  aWindow->DispatchWindowEvent(&event);
  mIsComposing = PR_FALSE;
  mComposingWindow = nsnull;
}

static void
DumpReconvertString(RECONVERTSTRING* aReconv)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("  dwSize=%ld, dwVersion=%ld, dwStrLen=%ld, dwStrOffset=%ld\n",
     aReconv->dwSize, aReconv->dwVersion,
     aReconv->dwStrLen, aReconv->dwStrOffset));
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("  dwCompStrLen=%ld, dwCompStrOffset=%ld, dwTargetStrLen=%ld, dwTargetStrOffset=%ld\n",
     aReconv->dwCompStrLen, aReconv->dwCompStrOffset,
     aReconv->dwTargetStrLen, aReconv->dwTargetStrOffset));
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("  result str=\"%s\"\n",
     NS_ConvertUTF16toUTF8(
       nsAutoString((PRUnichar*)((char*)(aReconv) + aReconv->dwStrOffset),
                    aReconv->dwStrLen)).get()));
}

PRBool
nsIMM32Handler::HandleReconvert(nsWindow* aWindow,
                                LPARAM lParam,
                                LRESULT *oResult)
{
  *oResult = 0;
  RECONVERTSTRING* pReconv = reinterpret_cast<RECONVERTSTRING*>(lParam);

  nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT, aWindow);
  nsIntPoint point(0, 0);
  aWindow->InitEvent(selection, &point);
  aWindow->DispatchWindowEvent(&selection);
  if (!selection.mSucceeded) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleReconvert, FAILED (NS_QUERY_SELECTED_TEXT)\n"));
    return PR_FALSE;
  }

  PRUint32 len = selection.mReply.mString.Length();
  PRUint32 needSize = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!pReconv) {
    // Return need size to reconvert.
    if (len == 0) {
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: HandleReconvert, There are not selected text\n"));
      return PR_FALSE;
    }
    *oResult = needSize;
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleReconvert, SUCCEEDED result=%ld\n",
       *oResult));
    return PR_TRUE;
  }

  if (pReconv->dwSize < needSize) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleReconvert, FAILED pReconv->dwSize=%ld, needSize=%ld\n",
       pReconv->dwSize, needSize));
    return PR_FALSE;
  }

  *oResult = needSize;

  // Fill reconvert struct
  pReconv->dwVersion         = 0;
  pReconv->dwStrLen          = len;
  pReconv->dwStrOffset       = sizeof(RECONVERTSTRING);
  pReconv->dwCompStrLen      = len;
  pReconv->dwCompStrOffset   = 0;
  pReconv->dwTargetStrLen    = len;
  pReconv->dwTargetStrOffset = 0;

  ::CopyMemory(reinterpret_cast<LPVOID>(lParam + sizeof(RECONVERTSTRING)),
               selection.mReply.mString.get(), len * sizeof(WCHAR));

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: HandleReconvert, SUCCEEDED result=%ld\n",
     *oResult));
  DumpReconvertString(pReconv);

  return PR_TRUE;
}

PRBool
nsIMM32Handler::HandleQueryCharPosition(nsWindow* aWindow,
                                        LPARAM lParam,
                                        LRESULT *oResult)
{
  PRUint32 len = mIsComposing ? mCompositionString.Length() : 0;
  *oResult = PR_FALSE;
  IMECHARPOSITION* pCharPosition = reinterpret_cast<IMECHARPOSITION*>(lParam);
  if (!pCharPosition) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleQueryCharPosition, FAILED (pCharPosition is null)\n"));
    return PR_FALSE;
  }
  if (pCharPosition->dwSize < sizeof(IMECHARPOSITION)) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleReconvert, FAILED, pCharPosition->dwSize=%ld, sizeof(IMECHARPOSITION)=%ld\n",
       pCharPosition->dwSize, sizeof(IMECHARPOSITION)));
    return PR_FALSE;
  }
  if (::GetFocus() != aWindow->GetWindowHandle()) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleReconvert, FAILED, ::GetFocus()=%08x, OurWindowHandle=%08x\n",
       ::GetFocus(), aWindow->GetWindowHandle()));
    return PR_FALSE;
  }
  if (pCharPosition->dwCharPos > len) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleQueryCharPosition, FAILED, pCharPosition->dwCharPos=%ld, len=%ld\n",
      pCharPosition->dwCharPos, len));
    return PR_FALSE;
  }

  nsIntRect r;
  PRBool ret =
    GetCharacterRectOfSelectedTextAt(aWindow, pCharPosition->dwCharPos, r);
  NS_ENSURE_TRUE(ret, PR_FALSE);

  nsIntRect screenRect;
  // We always need top level window that is owner window of the popup window
  // even if the content of the popup window has focus.
  ResolveIMECaretPos(aWindow->GetTopLevelWindow(PR_FALSE),
                     r, nsnull, screenRect);
  pCharPosition->pt.x = screenRect.x;
  pCharPosition->pt.y = screenRect.y;

  pCharPosition->cLineHeight = r.height;

  // XXX we should use NS_QUERY_EDITOR_RECT event here.
  ::GetWindowRect(aWindow->GetWindowHandle(), &pCharPosition->rcDocument);

  *oResult = TRUE;

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: HandleQueryCharPosition, SUCCEEDED\n"));
  return PR_TRUE;
}

PRBool
nsIMM32Handler::HandleDocumentFeed(nsWindow* aWindow,
                                   LPARAM lParam,
                                   LRESULT *oResult)
{
  *oResult = 0;
  RECONVERTSTRING* pReconv = reinterpret_cast<RECONVERTSTRING*>(lParam);

  nsIntPoint point(0, 0);

  PRBool hasCompositionString =
    mIsComposing && ShouldDrawCompositionStringOurselves();

  PRInt32 targetOffset, targetLength;
  if (!hasCompositionString) {
    nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT, aWindow);
    aWindow->InitEvent(selection, &point);
    aWindow->DispatchWindowEvent(&selection);
    if (!selection.mSucceeded) {
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: HandleDocumentFeed, FAILED (NS_QUERY_SELECTED_TEXT)\n"));
      return PR_FALSE;
    }
    targetOffset = PRInt32(selection.mReply.mOffset);
    targetLength = PRInt32(selection.mReply.mString.Length());
  } else {
    targetOffset = PRInt32(mCompositionStart);
    targetLength = PRInt32(mCompositionString.Length());
  }

  // XXX nsString::Find and nsString::RFind take PRInt32 for offset, so,
  //     we cannot support this message when the current offset is larger than
  //     PR_INT32_MAX.
  if (targetOffset < 0 || targetLength < 0 ||
      targetOffset + targetLength < 0) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleDocumentFeed, FAILED (The selection is out of range)\n"));
    return PR_FALSE;
  }

  // Get all contents of the focused editor.
  nsQueryContentEvent textContent(PR_TRUE, NS_QUERY_TEXT_CONTENT, aWindow);
  textContent.InitForQueryTextContent(0, PR_UINT32_MAX);
  aWindow->InitEvent(textContent, &point);
  aWindow->DispatchWindowEvent(&textContent);
  if (!textContent.mSucceeded) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleDocumentFeed, FAILED (NS_QUERY_TEXT_CONTENT)\n"));
    return PR_FALSE;
  }

  nsAutoString str(textContent.mReply.mString);
  if (targetOffset > PRInt32(str.Length())) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleDocumentFeed, FAILED (The caret offset is invalid)\n"));
    return PR_FALSE;
  }

  // Get the focused paragraph, we decide that it starts from the previous CRLF
  // (or start of the editor) to the next one (or the end of the editor).
  PRInt32 paragraphStart = str.RFind("\n", PR_FALSE, targetOffset, -1) + 1;
  PRInt32 paragraphEnd =
    str.Find("\r", PR_FALSE, targetOffset + targetLength, -1);
  if (paragraphEnd < 0) {
    paragraphEnd = str.Length();
  }
  nsDependentSubstring paragraph(str, paragraphStart,
                                 paragraphEnd - paragraphStart);

  PRUint32 len = paragraph.Length();
  PRUint32 needSize = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!pReconv) {
    *oResult = needSize;
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleDocumentFeed, SUCCEEDED result=%ld\n",
       *oResult));
    return PR_TRUE;
  }

  if (pReconv->dwSize < needSize) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: HandleDocumentFeed, FAILED pReconv->dwSize=%ld, needSize=%ld\n",
       pReconv->dwSize, needSize));
    return PR_FALSE;
  }

  // Fill reconvert struct
  pReconv->dwVersion         = 0;
  pReconv->dwStrLen          = len;
  pReconv->dwStrOffset       = sizeof(RECONVERTSTRING);
  if (hasCompositionString) {
    pReconv->dwCompStrLen      = targetLength;
    pReconv->dwCompStrOffset   =
      (targetOffset - paragraphStart) * sizeof(WCHAR);
    // Set composition target clause information
    PRUint32 offset, length;
    if (!GetTargetClauseRange(&offset, &length)) {
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: HandleDocumentFeed, FAILED, by GetTargetClauseRange\n"));
      return PR_FALSE;
    }
    pReconv->dwTargetStrLen    = length;
    pReconv->dwTargetStrOffset = (offset - paragraphStart) * sizeof(WCHAR);
  } else {
    pReconv->dwTargetStrLen    = targetLength;
    pReconv->dwTargetStrOffset =
      (targetOffset - paragraphStart) * sizeof(WCHAR);
    // There is no composition string, so, the length is zero but we should
    // set the cursor offset to the composition str offset.
    pReconv->dwCompStrLen      = 0;
    pReconv->dwCompStrOffset   = pReconv->dwTargetStrOffset;
  }

  *oResult = needSize;
  ::CopyMemory(reinterpret_cast<LPVOID>(lParam + sizeof(RECONVERTSTRING)),
               paragraph.BeginReading(), len * sizeof(WCHAR));

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: HandleDocumentFeed, SUCCEEDED result=%ld\n",
     *oResult));
  DumpReconvertString(pReconv);

  return PR_TRUE;
}

PRBool
nsIMM32Handler::CommitCompositionOnPreviousWindow(nsWindow* aWindow)
{
  if (!mComposingWindow || mComposingWindow == aWindow) {
    return PR_FALSE;
  }

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: CommitCompositionOnPreviousWindow, mIsComposing=%s, mIsComposingOnPlugin=%s\n",
     mIsComposing ? "TRUE" : "FALSE", mIsComposingOnPlugin ? "TRUE" : "FALSE"));

  // If we have composition, we should dispatch composition events internally.
  if (mIsComposing) {
    nsIMEContext IMEContext(mComposingWindow->GetWindowHandle());
    NS_ASSERTION(IMEContext.IsValid(), "IME context must be valid");

    DispatchTextEvent(mComposingWindow, IMEContext, PR_FALSE);
    HandleEndComposition(mComposingWindow);
    return PR_TRUE;
  }

  // XXX When plug-in has composition, we should commit composition on the
  // plug-in.  However, we need some more work for that.
  return mIsComposingOnPlugin;
}

static PRUint32
PlatformToNSAttr(PRUint8 aAttr)
{
  switch (aAttr)
  {
    case ATTR_INPUT_ERROR:
    // case ATTR_FIXEDCONVERTED:
    case ATTR_INPUT:
      return NS_TEXTRANGE_RAWINPUT;
    case ATTR_CONVERTED:
      return NS_TEXTRANGE_CONVERTEDTEXT;
    case ATTR_TARGET_NOTCONVERTED:
      return NS_TEXTRANGE_SELECTEDRAWTEXT;
    case ATTR_TARGET_CONVERTED:
      return NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
    default:
      NS_ASSERTION(PR_FALSE, "unknown attribute");
      return NS_TEXTRANGE_CARETPOSITION;
  }
}

#ifdef PR_LOGGING
static const char*
GetRangeTypeName(PRUint32 aRangeType)
{
  switch (aRangeType) {
    case NS_TEXTRANGE_RAWINPUT:
      return "NS_TEXTRANGE_RAWINPUT";
    case NS_TEXTRANGE_CONVERTEDTEXT:
      return "NS_TEXTRANGE_CONVERTEDTEXT";
    case NS_TEXTRANGE_SELECTEDRAWTEXT:
      return "NS_TEXTRANGE_SELECTEDRAWTEXT";
    case NS_TEXTRANGE_SELECTEDCONVERTEDTEXT:
      return "NS_TEXTRANGE_SELECTEDCONVERTEDTEXT";
    case NS_TEXTRANGE_CARETPOSITION:
      return "NS_TEXTRANGE_CARETPOSITION";
    default:
      return "UNKNOWN SELECTION TYPE!!";
  }
}
#endif

void
nsIMM32Handler::DispatchTextEvent(nsWindow* aWindow,
                                  const nsIMEContext &aIMEContext,
                                  PRBool aCheckAttr)
{
  NS_ASSERTION(mIsComposing, "conflict state");
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: DispatchTextEvent, aCheckAttr=%s\n",
     aCheckAttr ? "TRUE": "FALSE"));

  // If we don't need to draw composition string ourselves and this is not
  // commit event (i.e., under composing), we don't need to fire text event
  // during composing.
  if (aCheckAttr && !ShouldDrawCompositionStringOurselves()) {
    // But we need to adjust composition window pos and native caret pos, here.
    SetIMERelatedWindowsPos(aWindow, aIMEContext);
    return;
  }

  nsTextEvent event(PR_TRUE, NS_TEXT_TEXT, aWindow);
  nsIntPoint point(0, 0);

  aWindow->InitEvent(event, &point);

  nsAutoTArray<nsTextRange, 4> textRanges;

  if (aCheckAttr) {
    SetTextRangeList(textRanges);
  }

  event.rangeCount = textRanges.Length();
  event.rangeArray = textRanges.Elements();

  event.theText = mCompositionString.get();
  nsModifierKeyState modKeyState;
  event.isShift = modKeyState.mIsShiftDown;
  event.isControl = modKeyState.mIsControlDown;
  event.isMeta = PR_FALSE;
  event.isAlt = modKeyState.mIsAltDown;

  aWindow->DispatchWindowEvent(&event);

  SetIMERelatedWindowsPos(aWindow, aIMEContext);
}

void
nsIMM32Handler::SetTextRangeList(nsTArray<nsTextRange> &aTextRangeList)
{
  // Sogou (Simplified Chinese IME) returns contradictory values: The cursor
  // position is actual cursor position. However, other values (composition
  // string and attributes) are empty. So, if you want to remove following
  // assertion, be careful.
  NS_ASSERTION(ShouldDrawCompositionStringOurselves(),
    "SetTextRangeList is called when we don't need to fire text event");

  nsTextRange range;
  if (mClauseArray.Length() == 0) {
    // Some IMEs don't return clause array information, then, we assume that
    // all characters in the composition string are in one clause.
    range.mStartOffset = 0;
    range.mEndOffset = mCompositionString.Length();
    range.mRangeType = NS_TEXTRANGE_RAWINPUT;
    aTextRangeList.AppendElement(range);

    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: SetTextRangeList, mClauseLength=0\n"));
  } else {
    // iterate over the attributes
    PRUint32 lastOffset = 0;
    for (PRUint32 i = 0; i < mClauseArray.Length() - 1; i++) {
      PRUint32 current = mClauseArray[i + 1];
      if (current > mCompositionString.Length()) {
        PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
          ("IMM32: SetTextRangeList, mClauseArray[%ld]=%lu. This is larger than mCompositionString.Length()=%lu\n",
           i + 1, current, mCompositionString.Length()));
        current = PRInt32(mCompositionString.Length());
      }

      range.mRangeType = PlatformToNSAttr(mAttributeArray[lastOffset]);
      range.mStartOffset = lastOffset;
      range.mEndOffset = current;
      aTextRangeList.AppendElement(range);

      lastOffset = current;

      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: SetTextRangeList, index=%ld, rangeType=%s, range=[%lu-%lu]\n",
         i, GetRangeTypeName(range.mRangeType), range.mStartOffset,
         range.mEndOffset));
    }
  }

  if (mCursorPosition == NO_IME_CARET) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: GetTextRangeList, no caret\n"));
    return;
  }

  PRInt32 cursor = mCursorPosition;
  if (PRUint32(cursor) > mCompositionString.Length()) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: SetTextRangeList, mCursorPosition=%ld. This is larger than mCompositionString.Length()=%lu\n",
       mCursorPosition, mCompositionString.Length()));
    cursor = mCompositionString.Length();
  }

  range.mStartOffset = range.mEndOffset = cursor;
  range.mRangeType = NS_TEXTRANGE_CARETPOSITION;
  aTextRangeList.AppendElement(range);

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: SetTextRangeList, caret position=%ld\n",
     range.mStartOffset));
}

void
nsIMM32Handler::GetCompositionString(const nsIMEContext &aIMEContext,
                                     DWORD aIndex)
{
  // Retrieve the size of the required output buffer.
  long lRtn = ::ImmGetCompositionStringW(aIMEContext.get(), aIndex, NULL, 0);
  if (lRtn < 0 ||
      !EnsureStringLength(mCompositionString, (lRtn / sizeof(WCHAR)) + 1)) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: GetCompositionString, FAILED by OOM\n"));
    return; // Error or out of memory.
  }

  // Actually retrieve the composition string information.
  lRtn = ::ImmGetCompositionStringW(aIMEContext.get(), aIndex,
                                    (LPVOID)mCompositionString.BeginWriting(),
                                    lRtn + sizeof(WCHAR));
  mCompositionString.SetLength(lRtn / sizeof(WCHAR));

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: GetCompositionString, SUCCEEDED mCompositionString=\"%s\"\n",
     NS_ConvertUTF16toUTF8(mCompositionString).get()));
}

PRBool
nsIMM32Handler::GetTargetClauseRange(PRUint32 *aOffset, PRUint32 *aLength)
{
  NS_ENSURE_TRUE(aOffset, PR_FALSE);
  NS_ENSURE_TRUE(mIsComposing, PR_FALSE);
  NS_ENSURE_TRUE(ShouldDrawCompositionStringOurselves(), PR_FALSE);

  PRBool found = PR_FALSE;
  *aOffset = mCompositionStart;
  for (PRUint32 i = 0; i < mAttributeArray.Length(); i++) {
    if (mAttributeArray[i] == ATTR_TARGET_NOTCONVERTED ||
        mAttributeArray[i] == ATTR_TARGET_CONVERTED) {
      *aOffset = mCompositionStart + i;
      found = PR_TRUE;
      break;
    }
  }

  if (!aLength) {
    return PR_TRUE;
  }

  if (!found) {
    // The all composition string is targetted when there is no ATTR_TARGET_*
    // clause. E.g., there is only ATTR_INPUT
    *aLength = mCompositionString.Length();
    return PR_TRUE;
  }

  PRUint32 offsetInComposition = *aOffset - mCompositionStart;
  *aLength = mCompositionString.Length() - offsetInComposition;
  for (PRUint32 i = offsetInComposition; i < mAttributeArray.Length(); i++) {
    if (mAttributeArray[i] != ATTR_TARGET_NOTCONVERTED &&
        mAttributeArray[i] != ATTR_TARGET_CONVERTED) {
      *aLength = i - offsetInComposition;
      break;
    }
  }
  return PR_TRUE;
}

PRBool
nsIMM32Handler::ConvertToANSIString(const nsAFlatString& aStr, UINT aCodePage,
                                   nsACString& aANSIStr)
{
  int len = ::WideCharToMultiByte(aCodePage, 0,
                                  (LPCWSTR)aStr.get(), aStr.Length(),
                                  NULL, 0, NULL, NULL);
  NS_ENSURE_TRUE(len >= 0, PR_FALSE);

  if (!EnsureStringLength(aANSIStr, len)) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: ConvertToANSIString, FAILED by OOM\n"));
    return PR_FALSE;
  }
  ::WideCharToMultiByte(aCodePage, 0, (LPCWSTR)aStr.get(), aStr.Length(),
                        (LPSTR)aANSIStr.BeginWriting(), len, NULL, NULL);
  return PR_TRUE;
}

PRBool
nsIMM32Handler::GetCharacterRectOfSelectedTextAt(nsWindow* aWindow,
                                                 PRUint32 aOffset,
                                                 nsIntRect &aCharRect)
{
  nsIntPoint point(0, 0);

  nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT, aWindow);
  aWindow->InitEvent(selection, &point);
  aWindow->DispatchWindowEvent(&selection);
  if (!selection.mSucceeded) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: GetCharacterRectOfSelectedTextAt, aOffset=%lu, FAILED (NS_QUERY_SELECTED_TEXT)\n",
       aOffset));
    return PR_FALSE;
  }

  PRUint32 offset = selection.mReply.mOffset + aOffset;
  PRBool useCaretRect = selection.mReply.mString.IsEmpty();
  if (useCaretRect && ShouldDrawCompositionStringOurselves() &&
      mIsComposing && !mCompositionString.IsEmpty()) {
    // There is not a normal selection, but we have composition string.
    // XXX mnakano - Should we implement NS_QUERY_IME_SELECTED_TEXT?
    useCaretRect = PR_FALSE;
    if (mCursorPosition != NO_IME_CARET) {
      PRUint32 cursorPosition =
        PR_MIN(PRUint32(mCursorPosition), mCompositionString.Length());
      offset -= cursorPosition;
      NS_ASSERTION(offset >= 0, "offset is negative!");
    }
  }

  nsIntRect r;
  if (!useCaretRect) {
    nsQueryContentEvent charRect(PR_TRUE, NS_QUERY_TEXT_RECT, aWindow);
    charRect.InitForQueryTextRect(offset, 1);
    aWindow->InitEvent(charRect, &point);
    aWindow->DispatchWindowEvent(&charRect);
    if (charRect.mSucceeded) {
      aCharRect = charRect.mReply.mRect;
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: GetCharacterRectOfSelectedTextAt, aOffset=%lu, SUCCEEDED\n",
         aOffset));
      PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
        ("IMM32: GetCharacterRectOfSelectedTextAt, aCharRect={ x: %ld, y: %ld, width: %ld, height: %ld }\n",
         aCharRect.x, aCharRect.y, aCharRect.width, aCharRect.height));
      return PR_TRUE;
    }
  }

  return GetCaretRect(aWindow, aCharRect);
}

PRBool
nsIMM32Handler::GetCaretRect(nsWindow* aWindow, nsIntRect &aCaretRect)
{
  nsIntPoint point(0, 0);

  nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT, aWindow);
  aWindow->InitEvent(selection, &point);
  aWindow->DispatchWindowEvent(&selection);
  if (!selection.mSucceeded) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: GetCaretRect,  FAILED (NS_QUERY_SELECTED_TEXT)\n"));
    return PR_FALSE;
  }

  PRUint32 offset = selection.mReply.mOffset;

  nsQueryContentEvent caretRect(PR_TRUE, NS_QUERY_CARET_RECT, aWindow);
  caretRect.InitForQueryCaretRect(offset);
  aWindow->InitEvent(caretRect, &point);
  aWindow->DispatchWindowEvent(&caretRect);
  if (!caretRect.mSucceeded) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: GetCaretRect,  FAILED (NS_QUERY_CARET_RECT)\n"));
    return PR_FALSE;
  }
  aCaretRect = caretRect.mReply.mRect;
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: GetCaretRect, SUCCEEDED, aCaretRect={ x: %ld, y: %ld, width: %ld, height: %ld }\n",
     aCaretRect.x, aCaretRect.y, aCaretRect.width, aCaretRect.height));
  return PR_TRUE;
}

PRBool
nsIMM32Handler::SetIMERelatedWindowsPos(nsWindow* aWindow,
                                        const nsIMEContext &aIMEContext)
{
  nsIntRect r;
  // Get first character rect of current a normal selected text or a composing
  // string.
  PRBool ret = GetCharacterRectOfSelectedTextAt(aWindow, 0, r);
  NS_ENSURE_TRUE(ret, PR_FALSE);
  nsWindow* toplevelWindow = aWindow->GetTopLevelWindow(PR_FALSE);
  nsIntRect firstSelectedCharRect;
  ResolveIMECaretPos(toplevelWindow, r, aWindow, firstSelectedCharRect);

  // Set native caret size/position to our caret. Some IMEs honor it. E.g.,
  // "Intelligent ABC" (Simplified Chinese) and "MS PinYin 3.0" (Simplified
  // Chinese) on XP.
  nsIntRect caretRect(firstSelectedCharRect);
  if (GetCaretRect(aWindow, r)) {
    ResolveIMECaretPos(toplevelWindow, r, aWindow, caretRect);
  } else {
    NS_WARNING("failed to get caret rect");
    caretRect.width = 1;
  }
  if (!mNativeCaretIsCreated) {
    mNativeCaretIsCreated = ::CreateCaret(aWindow->GetWindowHandle(), nsnull,
                                          caretRect.width, caretRect.height);
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: SetIMERelatedWindowsPos, mNativeCaretIsCreated=%s, width=%ld height=%ld\n",
       mNativeCaretIsCreated ? "TRUE" : "FALSE",
       caretRect.width, caretRect.height));
  }
  ::SetCaretPos(caretRect.x, caretRect.y);

  if (ShouldDrawCompositionStringOurselves()) {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: SetIMERelatedWindowsPos, Set candidate window\n"));

    // Get a rect of first character in current target in composition string.
    if (mIsComposing && !mCompositionString.IsEmpty()) {
      // If there are no targetted selection, we should use it's first character
      // rect instead.
      PRUint32 offset;
      if (!GetTargetClauseRange(&offset)) {
        PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
          ("IMM32: SetIMERelatedWindowsPos, FAILED, by GetTargetClauseRange\n"));
        return PR_FALSE;
      }
      ret = GetCharacterRectOfSelectedTextAt(aWindow,
                                             offset - mCompositionStart, r);
      NS_ENSURE_TRUE(ret, PR_FALSE);
    } else {
      // If there are no composition string, we should use a first character
      // rect.
      ret = GetCharacterRectOfSelectedTextAt(aWindow, 0, r);
      NS_ENSURE_TRUE(ret, PR_FALSE);
    }
    nsIntRect firstTargetCharRect;
    ResolveIMECaretPos(toplevelWindow, r, aWindow, firstTargetCharRect);

    // Move the candidate window to first character position of the target.
    CANDIDATEFORM candForm;
    candForm.dwIndex = 0;
    candForm.dwStyle = CFS_EXCLUDE;
    candForm.ptCurrentPos.x = firstTargetCharRect.x;
    candForm.ptCurrentPos.y = firstTargetCharRect.y;
    candForm.rcArea.right = candForm.rcArea.left = candForm.ptCurrentPos.x;
    candForm.rcArea.top = candForm.ptCurrentPos.y;
    candForm.rcArea.bottom = candForm.ptCurrentPos.y +
                               firstTargetCharRect.height;
    ::ImmSetCandidateWindow(aIMEContext.get(), &candForm);
  } else {
    PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
      ("IMM32: SetIMERelatedWindowsPos, Set composition window\n"));

    // Move the composition window to caret position (if selected some
    // characters, we should use first character rect of them).
    // And in this mode, IME adjusts the candidate window position
    // automatically. So, we don't need to set it.
    COMPOSITIONFORM compForm;
    compForm.dwStyle = CFS_POINT;
    compForm.ptCurrentPos.x = firstSelectedCharRect.x;
    compForm.ptCurrentPos.y = firstSelectedCharRect.y;
    ::ImmSetCompositionWindow(aIMEContext.get(), &compForm);
  }

  return PR_TRUE;
}

void
nsIMM32Handler::ResolveIMECaretPos(nsIWidget* aReferenceWidget,
                                   nsIntRect& aCursorRect,
                                   nsIWidget* aNewOriginWidget,
                                   nsIntRect& aOutRect)
{
  aOutRect = aCursorRect;

  if (aReferenceWidget == aNewOriginWidget)
    return;

  if (aReferenceWidget)
    aOutRect.MoveBy(aReferenceWidget->WidgetToScreenOffset());

  if (aNewOriginWidget)
    aOutRect.MoveBy(-aNewOriginWidget->WidgetToScreenOffset());
}


#ifdef ENABLE_IME_MOUSE_HANDLING

PRBool
nsIMM32Handler::OnMouseEvent(nsWindow* aWindow, LPARAM lParam, int aAction)
{
  if (!sWM_MSIME_MOUSE || !mIsComposing) {
    return PR_FALSE;
  }

  nsIntPoint cursor(LOWORD(lParam), HIWORD(lParam));
  nsQueryContentEvent charAtPt(PR_TRUE, NS_QUERY_CHARACTER_AT_POINT, aWindow);
  aWindow->InitEvent(charAtPt, &cursor);
  aWindow->DispatchWindowEvent(&charAtPt);
  if (!charAtPt.mSucceeded ||
      charAtPt.mReply.mOffset == nsQueryContentEvent::NOT_FOUND ||
      charAtPt.mReply.mOffset < mCompositionStart ||
      charAtPt.mReply.mOffset >
        mCompositionStart + mCompositionString.Length()) {
    return PR_FALSE;
  }

  // calcurate positioning and offset
  // char :            JCH1|JCH2|JCH3
  // offset:           0011 1122 2233
  // positioning:      2301 2301 2301
  nsIntRect cursorInTopLevel, cursorRect(cursor, nsIntSize(0, 0));
  ResolveIMECaretPos(aWindow, cursorRect,
                     aWindow->GetTopLevelWindow(PR_FALSE), cursorInTopLevel);
  PRInt32 cursorXInChar = cursorInTopLevel.x - charAtPt.mReply.mRect.x;
  int positioning = cursorXInChar * 4 / charAtPt.mReply.mRect.width;
  positioning = (positioning + 2) % 4;

  int offset = charAtPt.mReply.mOffset - mCompositionStart;
  if (positioning < 2) {
    offset++;
  }

  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnMouseEvent, x,y=%ld,%ld, offset=%ld, positioning=%ld\n",
     cursor.x, cursor.y, offset, positioning));

  // send MS_MSIME_MOUSE message to default IME window.
  HWND imeWnd = ::ImmGetDefaultIMEWnd(aWindow->GetWindowHandle());
  nsIMEContext IMEContext(aWindow->GetWindowHandle());
  return ::SendMessageW(imeWnd, sWM_MSIME_MOUSE,
                        MAKELONG(MAKEWORD(aAction, positioning), offset),
                        (LPARAM) IMEContext.get()) == 1;
}

#endif // ENABLE_IME_MOUSE_HANDLING

/* static */ PRBool
nsIMM32Handler::OnKeyDownEvent(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                               PRBool &aEatMessage)
{
  PR_LOG(gIMM32Log, PR_LOG_ALWAYS,
    ("IMM32: OnKeyDownEvent, hWnd=%08x, wParam=%08x, lParam=%08x\n",
     aWindow->GetWindowHandle(), wParam, lParam));
  aEatMessage = PR_FALSE;
  switch (wParam) {
    case VK_PROCESSKEY:
      // If we receive when IME isn't open, it means IME is opening right now.
      if (sIsIME) {
        nsIMEContext IMEContext(aWindow->GetWindowHandle());
        sIsIMEOpening =
          IMEContext.IsValid() && !::ImmGetOpenStatus(IMEContext.get());
      }
      return PR_FALSE;
    case VK_TAB:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_END:
    case VK_HOME:
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
      // If IME didn't process the key message (the virtual key code wasn't
      // converted to VK_PROCESSKEY), and the virtual key code event causes
      // to move caret, we should cancel the composition here.  Then, this
      // event will be dispatched.
      // XXX I think that we should dispatch all key events during composition,
      //     and nsEditor should cancel/commit the composition if it *thinks*
      //     it's needed.
      if (IsComposingOnOurEditor()) {
        // NOTE: We don't need to cancel the composition on another window.
        CancelComposition(aWindow, PR_FALSE);
      }
      return PR_FALSE;
    default:
      return PR_FALSE;
  }
}
