/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "IMMHandler.h"
#include "nsWindow.h"
#include "nsWindowDefs.h"
#include "WinUtils.h"
#include "KeyboardLayout.h"
#include <algorithm>

#include "mozilla/CheckedInt.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/TextEvents.h"

#ifndef IME_PROP_ACCEPT_WIDE_VKEY
#define IME_PROP_ACCEPT_WIDE_VKEY 0x20
#endif

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

static const char*
GetBoolName(bool aBool)
{
  return aBool ? "true" : "false";
}

static void
HandleSeparator(nsACString& aDesc)
{
  if (!aDesc.IsEmpty()) {
    aDesc.AppendLiteral(" | ");
  }
}

class GetIMEGeneralPropertyName : public nsAutoCString
{
public:
  explicit GetIMEGeneralPropertyName(DWORD aFlags)
  {
    if (!aFlags) {
      AppendLiteral("no flags");
      return;
    }
    if (aFlags & IME_PROP_AT_CARET) {
      AppendLiteral("IME_PROP_AT_CARET");
    }
    if (aFlags & IME_PROP_SPECIAL_UI) {
      HandleSeparator(*this);
      AppendLiteral("IME_PROP_SPECIAL_UI");
    }
    if (aFlags & IME_PROP_CANDLIST_START_FROM_1) {
      HandleSeparator(*this);
      AppendLiteral("IME_PROP_CANDLIST_START_FROM_1");
    }
    if (aFlags & IME_PROP_UNICODE) {
      HandleSeparator(*this);
      AppendLiteral("IME_PROP_UNICODE");
    }
    if (aFlags & IME_PROP_COMPLETE_ON_UNSELECT) {
      HandleSeparator(*this);
      AppendLiteral("IME_PROP_COMPLETE_ON_UNSELECT");
    }
    if (aFlags & IME_PROP_ACCEPT_WIDE_VKEY) {
      HandleSeparator(*this);
      AppendLiteral("IME_PROP_ACCEPT_WIDE_VKEY");
    }
  }
  virtual ~GetIMEGeneralPropertyName() {}
};

class GetIMEUIPropertyName : public nsAutoCString
{
public:
  explicit GetIMEUIPropertyName(DWORD aFlags)
  {
    if (!aFlags) {
      AppendLiteral("no flags");
      return;
    }
    if (aFlags & UI_CAP_2700) {
      AppendLiteral("UI_CAP_2700");
    }
    if (aFlags & UI_CAP_ROT90) {
      HandleSeparator(*this);
      AppendLiteral("UI_CAP_ROT90");
    }
    if (aFlags & UI_CAP_ROTANY) {
      HandleSeparator(*this);
      AppendLiteral("UI_CAP_ROTANY");
    }
  }
  virtual ~GetIMEUIPropertyName() {}
};

class GetWritingModeName : public nsAutoCString
{
public:
  explicit GetWritingModeName(const WritingMode& aWritingMode)
  {
    if (!aWritingMode.IsVertical()) {
      Assign("Horizontal");
      return;
    }
    if (aWritingMode.IsVerticalLR()) {
      Assign("Vertical (LR)");
      return;
    }
    Assign("Vertical (RL)");
  }
  virtual ~GetWritingModeName() {}
};

class GetReconvertStringLog : public nsAutoCString
{
public:
  explicit GetReconvertStringLog(RECONVERTSTRING* aReconv)
  {
    AssignLiteral("{ dwSize=");
    AppendInt(static_cast<uint32_t>(aReconv->dwSize));
    AppendLiteral(", dwVersion=");
    AppendInt(static_cast<uint32_t>(aReconv->dwVersion));
    AppendLiteral(", dwStrLen=");
    AppendInt(static_cast<uint32_t>(aReconv->dwStrLen));
    AppendLiteral(", dwStrOffset=");
    AppendInt(static_cast<uint32_t>(aReconv->dwStrOffset));
    AppendLiteral(", dwCompStrLen=");
    AppendInt(static_cast<uint32_t>(aReconv->dwCompStrLen));
    AppendLiteral(", dwCompStrOffset=");
    AppendInt(static_cast<uint32_t>(aReconv->dwCompStrOffset));
    AppendLiteral(", dwTargetStrLen=");
    AppendInt(static_cast<uint32_t>(aReconv->dwTargetStrLen));
    AppendLiteral(", dwTargetStrOffset=");
    AppendInt(static_cast<uint32_t>(aReconv->dwTargetStrOffset));
    AppendLiteral(", result str=\"");
    if (aReconv->dwStrLen) {
      char16_t* strStart =
        reinterpret_cast<char16_t*>(
          reinterpret_cast<char*>(aReconv) + aReconv->dwStrOffset);
      nsDependentString str(strStart, aReconv->dwStrLen);
      Append(NS_ConvertUTF16toUTF8(str));
    }
    AppendLiteral("\" }");
  }
  virtual ~GetReconvertStringLog() {}
};

namespace mozilla {
namespace widget {

static IMMHandler* gIMMHandler = nullptr;

LazyLogModule gIMMLog("nsIMM32HandlerWidgets");

/******************************************************************************
 * IMEContext
 ******************************************************************************/

IMEContext::IMEContext(HWND aWnd)
  : mWnd(aWnd)
  , mIMC(::ImmGetContext(aWnd))
{
}

IMEContext::IMEContext(nsWindowBase* aWindowBase)
  : mWnd(aWindowBase->GetWindowHandle())
  , mIMC(::ImmGetContext(aWindowBase->GetWindowHandle()))
{
}

void
IMEContext::Init(HWND aWnd)
{
  Clear();
  mWnd = aWnd;
  mIMC = ::ImmGetContext(mWnd);
}

void
IMEContext::Init(nsWindowBase* aWindowBase)
{
  Init(aWindowBase->GetWindowHandle());
}

void
IMEContext::Clear()
{
  if (mWnd && mIMC) {
    ::ImmReleaseContext(mWnd, mIMC);
  }
  mWnd = nullptr;
  mIMC = nullptr;
}

/******************************************************************************
 * IMMHandler
 ******************************************************************************/

static UINT sWM_MSIME_MOUSE = 0; // mouse message for MSIME 98/2000

WritingMode IMMHandler::sWritingModeOfCompositionFont;
nsString IMMHandler::sIMEName;
UINT IMMHandler::sCodePage = 0;
DWORD IMMHandler::sIMEProperty = 0;
DWORD IMMHandler::sIMEUIProperty = 0;
bool IMMHandler::sAssumeVerticalWritingModeNotSupported = false;
bool IMMHandler::sHasFocus = false;
bool IMMHandler::sNativeCaretIsCreatedForPlugin = false;

// static
void
IMMHandler::EnsureHandlerInstance()
{
  if (!gIMMHandler) {
    gIMMHandler = new IMMHandler();
  }
}

// static
void
IMMHandler::Initialize()
{
  if (!sWM_MSIME_MOUSE) {
    sWM_MSIME_MOUSE = ::RegisterWindowMessage(RWM_MOUSE);
  }
  sAssumeVerticalWritingModeNotSupported =
    Preferences::GetBool(
      "intl.imm.vertical_writing.always_assume_not_supported", false);
  InitKeyboardLayout(nullptr, ::GetKeyboardLayout(0));
}

// static
void
IMMHandler::Terminate()
{
  if (!gIMMHandler)
    return;
  delete gIMMHandler;
  gIMMHandler = nullptr;
}

// static
bool
IMMHandler::IsComposingOnOurEditor()
{
  return gIMMHandler && gIMMHandler->mIsComposing;
}

// static
bool
IMMHandler::IsComposingOnPlugin()
{
  return gIMMHandler && gIMMHandler->mIsComposingOnPlugin;
}

// static
bool
IMMHandler::IsComposingWindow(nsWindow* aWindow)
{
  return gIMMHandler && gIMMHandler->mComposingWindow == aWindow;
}

// static
bool
IMMHandler::IsTopLevelWindowOfComposition(nsWindow* aWindow)
{
  if (!gIMMHandler || !gIMMHandler->mComposingWindow) {
    return false;
  }
  HWND wnd = gIMMHandler->mComposingWindow->GetWindowHandle();
  return WinUtils::GetTopLevelHWND(wnd, true) == aWindow->GetWindowHandle();
}

// static
bool
IMMHandler::IsJapanist2003Active()
{
  return sIMEName.EqualsLiteral("Japanist 2003");
}

// static
bool
IMMHandler::IsGoogleJapaneseInputActive()
{
  // NOTE: Even on Windows for en-US, the name of Google Japanese Input is
  //       written in Japanese.
  return sIMEName.Equals(L"Google \x65E5\x672C\x8A9E\x5165\x529B "
                         L"IMM32 \x30E2\x30B8\x30E5\x30FC\x30EB");
}

// static
bool
IMMHandler::ShouldDrawCompositionStringOurselves()
{
  // If current IME has special UI or its composition window should not
  // positioned to caret position, we should now draw composition string
  // ourselves.
  return !(sIMEProperty & IME_PROP_SPECIAL_UI) &&
          (sIMEProperty & IME_PROP_AT_CARET);
}

// static
bool
IMMHandler::IsVerticalWritingSupported()
{
  // Even if IME claims that they support vertical writing mode but it may not
  // support vertical writing mode for its candidate window.
  if (sAssumeVerticalWritingModeNotSupported) {
    return false;
  }
  // Google Japanese Input doesn't support vertical writing mode.  We should
  // return false if it's active IME.
  if (IsGoogleJapaneseInputActive()) {
    return false;
  }
  return !!(sIMEUIProperty & (UI_CAP_2700 | UI_CAP_ROT90 | UI_CAP_ROTANY));
}

// static
void
IMMHandler::InitKeyboardLayout(nsWindow* aWindow,
                               HKL aKeyboardLayout)
{
  UINT IMENameLength = ::ImmGetDescriptionW(aKeyboardLayout, nullptr, 0);
  if (IMENameLength) {
    // Add room for the terminating null character
    sIMEName.SetLength(++IMENameLength);
    IMENameLength =
      ::ImmGetDescriptionW(aKeyboardLayout, wwc(sIMEName.BeginWriting()),
                           IMENameLength);
    // Adjust the length to ignore the terminating null character
    sIMEName.SetLength(IMENameLength);
  } else {
    sIMEName.Truncate();
  }

  WORD langID = LOWORD(aKeyboardLayout);
  ::GetLocaleInfoW(MAKELCID(langID, SORT_DEFAULT),
                   LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                   (PWSTR)&sCodePage, sizeof(sCodePage) / sizeof(WCHAR));
  sIMEProperty = ::ImmGetProperty(aKeyboardLayout, IGP_PROPERTY);
  sIMEUIProperty = ::ImmGetProperty(aKeyboardLayout, IGP_UI);

  // If active IME is a TIP of TSF, we cannot retrieve the name with IMM32 API.
  // For hacking some bugs of some TIP, we should set an IME name from the
  // pref.
  if (sCodePage == 932 && sIMEName.IsEmpty()) {
    sIMEName =
      Preferences::GetString("intl.imm.japanese.assume_active_tip_name_as");
  }

  // Whether the IME supports vertical writing mode might be changed or
  // some IMEs may need specific font for their UI.  Therefore, we should
  // update composition font forcibly here.
  if (aWindow) {
    MaybeAdjustCompositionFont(aWindow, sWritingModeOfCompositionFont, true);
  }

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("InitKeyboardLayout, aKeyboardLayout=%08x (\"%s\"), sCodePage=%lu, "
     "sIMEProperty=%s, sIMEUIProperty=%s",
     aKeyboardLayout, NS_ConvertUTF16toUTF8(sIMEName).get(),
     sCodePage, GetIMEGeneralPropertyName(sIMEProperty).get(),
     GetIMEUIPropertyName(sIMEUIProperty).get()));
}

// static
UINT
IMMHandler::GetKeyboardCodePage()
{
  return sCodePage;
}

// static
IMENotificationRequests
IMMHandler::GetIMENotificationRequests()
{
  return IMENotificationRequests(
    IMENotificationRequests::NOTIFY_POSITION_CHANGE |
    IMENotificationRequests::NOTIFY_MOUSE_BUTTON_EVENT_ON_CHAR);
}

// used for checking the lParam of WM_IME_COMPOSITION
#define IS_COMPOSING_LPARAM(lParam) \
  ((lParam) & (GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS))
#define IS_COMMITTING_LPARAM(lParam) ((lParam) & GCS_RESULTSTR)
// Some IMEs (e.g., the standard IME for Korean) don't have caret position,
// then, we should not set caret position to compositionchange event.
#define NO_IME_CARET -1

IMMHandler::IMMHandler()
  : mComposingWindow(nullptr)
  , mCursorPosition(NO_IME_CARET)
  , mCompositionStart(0)
  , mIsComposing(false)
  , mIsComposingOnPlugin(false)
  , mNativeCaretIsCreated(false)
{
  MOZ_LOG(gIMMLog, LogLevel::Debug, ("IMMHandler is created"));
}

IMMHandler::~IMMHandler()
{
  if (mIsComposing) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("~IMMHandler, ERROR, the instance is still composing"));
  }
  MOZ_LOG(gIMMLog, LogLevel::Debug, ("IMMHandler is destroyed"));
}

nsresult
IMMHandler::EnsureClauseArray(int32_t aCount)
{
  NS_ENSURE_ARG_MIN(aCount, 0);
  mClauseArray.SetCapacity(aCount + 32);
  return NS_OK;
}

nsresult
IMMHandler::EnsureAttributeArray(int32_t aCount)
{
  NS_ENSURE_ARG_MIN(aCount, 0);
  mAttributeArray.SetCapacity(aCount + 64);
  return NS_OK;
}

// static
void
IMMHandler::CommitComposition(nsWindow* aWindow, bool aForce)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("CommitComposition, aForce=%s, aWindow=%p, hWnd=%08x, "
     "mComposingWindow=%p%s",
     GetBoolName(aForce), aWindow, aWindow->GetWindowHandle(),
     gIMMHandler ? gIMMHandler->mComposingWindow : nullptr,
     gIMMHandler && gIMMHandler->mComposingWindow ?
       IsComposingOnOurEditor() ? " (composing on editor)" :
                                  " (composing on plug-in)" : ""));
  if (!aForce && !IsComposingWindow(aWindow)) {
    return;
  }

  IMEContext context(aWindow);
  bool associated = context.AssociateDefaultContext();
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("CommitComposition, associated=%s",
     GetBoolName(associated)));

  if (context.IsValid()) {
    ::ImmNotifyIME(context.get(), NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
    ::ImmNotifyIME(context.get(), NI_COMPOSITIONSTR, CPS_CANCEL, 0);
  }

  if (associated) {
    context.Disassociate();
  }
}

// static
void
IMMHandler::CancelComposition(nsWindow* aWindow, bool aForce)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("CancelComposition, aForce=%s, aWindow=%p, hWnd=%08x, "
     "mComposingWindow=%p%s",
     GetBoolName(aForce), aWindow, aWindow->GetWindowHandle(),
     gIMMHandler ? gIMMHandler->mComposingWindow : nullptr,
     gIMMHandler && gIMMHandler->mComposingWindow ?
       IsComposingOnOurEditor() ? " (composing on editor)" :
                                  " (composing on plug-in)" : ""));
  if (!aForce && !IsComposingWindow(aWindow)) {
    return;
  }

  IMEContext context(aWindow);
  bool associated = context.AssociateDefaultContext();
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("CancelComposition, associated=%s",
     GetBoolName(associated)));

  if (context.IsValid()) {
    ::ImmNotifyIME(context.get(), NI_COMPOSITIONSTR, CPS_CANCEL, 0);
  }

  if (associated) {
    context.Disassociate();
  }
}

// static
void
IMMHandler::OnFocusChange(bool aFocus, nsWindow* aWindow)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnFocusChange(aFocus=%s, aWindow=%p), sHasFocus=%s, "
     "IsComposingWindow(aWindow)=%s, aWindow->Destroyed()=%s, "
     "sNativeCaretIsCreatedForPlugin=%s",
     GetBoolName(aFocus), aWindow, GetBoolName(sHasFocus),
     GetBoolName(IsComposingWindow(aWindow)),
     GetBoolName(aWindow->Destroyed()),
     GetBoolName(sNativeCaretIsCreatedForPlugin)));

  if (!aFocus) {
    if (sNativeCaretIsCreatedForPlugin) {
      ::DestroyCaret();
      sNativeCaretIsCreatedForPlugin = false;
    }
    if (IsComposingWindow(aWindow) && aWindow->Destroyed()) {
      CancelComposition(aWindow);
    }
  }
  if (gIMMHandler) {
    gIMMHandler->mSelection.Clear();
  }
  sHasFocus = aFocus;
}

// static
void
IMMHandler::OnUpdateComposition(nsWindow* aWindow)
{
  if (!gIMMHandler) {
    return;
  }
 
  if (aWindow->PluginHasFocus()) {
    return;
  }

  IMEContext context(aWindow);
  gIMMHandler->SetIMERelatedWindowsPos(aWindow, context);
}

// static
void
IMMHandler::OnSelectionChange(nsWindow* aWindow,
                              const IMENotification& aIMENotification,
                              bool aIsIMMActive)
{
  if (!aIMENotification.mSelectionChangeData.mCausedByComposition &&
      aIsIMMActive) {
    MaybeAdjustCompositionFont(aWindow,
      aIMENotification.mSelectionChangeData.GetWritingMode());
  }
  // MaybeAdjustCompositionFont() may create gIMMHandler.  So, check it
  // after a call of MaybeAdjustCompositionFont().
  if (gIMMHandler) {
    gIMMHandler->mSelection.Update(aIMENotification);
  }
}

// static
void
IMMHandler::MaybeAdjustCompositionFont(nsWindow* aWindow,
                                       const WritingMode& aWritingMode,
                                       bool aForceUpdate)
{
  switch (sCodePage) {
    case 932: // Japanese Shift-JIS
    case 936: // Simlified Chinese GBK
    case 949: // Korean
    case 950: // Traditional Chinese Big5
      EnsureHandlerInstance();
      break;
    default:
      // If there is no instance of nsIMM32Hander, we shouldn't waste footprint.
      if (!gIMMHandler) {
        return;
      }
  }

  // Like Navi-Bar of ATOK, some IMEs may require proper composition font even
  // before sending WM_IME_STARTCOMPOSITION.
  IMEContext context(aWindow);
  gIMMHandler->AdjustCompositionFont(aWindow, context, aWritingMode,
                                     aForceUpdate);
}

// static
bool
IMMHandler::ProcessInputLangChangeMessage(nsWindow* aWindow,
                                          WPARAM wParam,
                                          LPARAM lParam,
                                          MSGResult& aResult)
{
  aResult.mResult = 0;
  aResult.mConsumed = false;
  // We don't need to create the instance of the handler here.
  if (gIMMHandler) {
    gIMMHandler->OnInputLangChange(aWindow, wParam, lParam, aResult);
  }
  InitKeyboardLayout(aWindow, reinterpret_cast<HKL>(lParam));
  // We can release the instance here, because the instance may be never
  // used. E.g., the new keyboard layout may not use IME, or it may use TSF.
  Terminate();
  // Don't return as "processed", the messages should be processed on nsWindow
  // too.
  return false;
}

// static
bool
IMMHandler::ProcessMessage(nsWindow* aWindow,
                           UINT msg,
                           WPARAM& wParam,
                           LPARAM& lParam,
                           MSGResult& aResult)
{
  // XXX We store the composing window in mComposingWindow.  If IME messages are
  // sent to different window, we should commit the old transaction.  And also
  // if the new window handle is not focused, probably, we should not start
  // the composition, however, such case should not be, it's just bad scenario.

  // When a plug-in has focus, we should dispatch the IME events to
  // the plug-in at first.
  if (aWindow->PluginHasFocus()) {
      bool ret = false;
      if (ProcessMessageForPlugin(aWindow, msg, wParam, lParam, ret, aResult)) {
        return ret;
      }
  }

  aResult.mResult = 0;
  switch (msg) {
    case WM_INPUTLANGCHANGE:
      return ProcessInputLangChangeMessage(aWindow, wParam, lParam, aResult);
    case WM_IME_STARTCOMPOSITION:
      EnsureHandlerInstance();
      return gIMMHandler->OnIMEStartComposition(aWindow, aResult);
    case WM_IME_COMPOSITION:
      EnsureHandlerInstance();
      return gIMMHandler->OnIMEComposition(aWindow, wParam, lParam, aResult);
    case WM_IME_ENDCOMPOSITION:
      EnsureHandlerInstance();
      return gIMMHandler->OnIMEEndComposition(aWindow, aResult);
    case WM_IME_CHAR:
      return OnIMEChar(aWindow, wParam, lParam, aResult);
    case WM_IME_NOTIFY:
      return OnIMENotify(aWindow, wParam, lParam, aResult);
    case WM_IME_REQUEST:
      EnsureHandlerInstance();
      return gIMMHandler->OnIMERequest(aWindow, wParam, lParam, aResult);
    case WM_IME_SELECT:
      return OnIMESelect(aWindow, wParam, lParam, aResult);
    case WM_IME_SETCONTEXT:
      return OnIMESetContext(aWindow, wParam, lParam, aResult);
    case WM_KEYDOWN:
      return OnKeyDownEvent(aWindow, wParam, lParam, aResult);
    case WM_CHAR:
      if (!gIMMHandler) {
        return false;
      }
      return gIMMHandler->OnChar(aWindow, wParam, lParam, aResult);
    default:
      return false;
  };
}

// static
bool
IMMHandler::ProcessMessageForPlugin(nsWindow* aWindow,
                                    UINT msg,
                                    WPARAM& wParam,
                                    LPARAM& lParam,
                                    bool& aRet,
                                    MSGResult& aResult)
{
  aResult.mResult = 0;
  aResult.mConsumed = false;
  switch (msg) {
    case WM_INPUTLANGCHANGEREQUEST:
    case WM_INPUTLANGCHANGE:
      aWindow->DispatchPluginEvent(msg, wParam, lParam, false);
      aRet = ProcessInputLangChangeMessage(aWindow, wParam, lParam, aResult);
      return true;
    case WM_IME_CHAR:
      EnsureHandlerInstance();
      aRet = gIMMHandler->OnIMECharOnPlugin(aWindow, wParam, lParam, aResult);
      return true;
    case WM_IME_SETCONTEXT:
      aRet = OnIMESetContextOnPlugin(aWindow, wParam, lParam, aResult);
      return true;
    case WM_CHAR:
      if (!gIMMHandler) {
        return true;
      }
      aRet = gIMMHandler->OnCharOnPlugin(aWindow, wParam, lParam, aResult);
      return true;
    case WM_IME_COMPOSITIONFULL:
    case WM_IME_CONTROL:
    case WM_IME_KEYDOWN:
    case WM_IME_KEYUP:
    case WM_IME_SELECT:
      aResult.mConsumed =
        aWindow->DispatchPluginEvent(msg, wParam, lParam, false);
      aRet = true;
      return true;
    case WM_IME_REQUEST:
       // Our plugin implementation is alwasy OOP.  So WM_IME_REQUEST doesn't
       // allow that parameter is pointer and shouldn't handle into Gecko.
       aRet = false;
       return true;
  }
  return false;
}

/****************************************************************************
 * message handlers
 ****************************************************************************/

void
IMMHandler::OnInputLangChange(nsWindow* aWindow,
                              WPARAM wParam,
                              LPARAM lParam,
                              MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnInputLangChange, hWnd=%08x, wParam=%08x, lParam=%08x",
     aWindow->GetWindowHandle(), wParam, lParam));

  aWindow->NotifyIME(REQUEST_TO_COMMIT_COMPOSITION);
  NS_ASSERTION(!mIsComposing, "ResetInputState failed");

  if (mIsComposing) {
    HandleEndComposition(aWindow);
  }

  aResult.mConsumed = false;
}

bool
IMMHandler::OnIMEStartComposition(nsWindow* aWindow,
                                  MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMEStartComposition, hWnd=%08x, mIsComposing=%s",
     aWindow->GetWindowHandle(), GetBoolName(mIsComposing)));
  aResult.mConsumed = ShouldDrawCompositionStringOurselves();
  if (mIsComposing) {
    NS_WARNING("Composition has been already started");
    return true;
  }

  IMEContext context(aWindow);
  HandleStartComposition(aWindow, context);
  return true;
}

bool
IMMHandler::OnIMEComposition(nsWindow* aWindow,
                             WPARAM wParam,
                             LPARAM lParam,
                             MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMEComposition, hWnd=%08x, lParam=%08x, mIsComposing=%s, "
     "GCS_RESULTSTR=%s, GCS_COMPSTR=%s, GCS_COMPATTR=%s, GCS_COMPCLAUSE=%s, "
     "GCS_CURSORPOS=%s,",
     aWindow->GetWindowHandle(), lParam, GetBoolName(mIsComposing),
     GetBoolName(lParam & GCS_RESULTSTR), GetBoolName(lParam & GCS_COMPSTR),
     GetBoolName(lParam & GCS_COMPATTR), GetBoolName(lParam & GCS_COMPCLAUSE),
     GetBoolName(lParam & GCS_CURSORPOS)));

  IMEContext context(aWindow);
  aResult.mConsumed = HandleComposition(aWindow, context, lParam);
  return true;
}

bool
IMMHandler::OnIMEEndComposition(nsWindow* aWindow,
                                MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMEEndComposition, hWnd=%08x, mIsComposing=%s",
     aWindow->GetWindowHandle(), GetBoolName(mIsComposing)));

  aResult.mConsumed = ShouldDrawCompositionStringOurselves();
  if (!mIsComposing) {
    return true;
  }

  // Korean IME posts WM_IME_ENDCOMPOSITION first when we hit space during
  // composition. Then, we should ignore the message and commit the composition
  // string at following WM_IME_COMPOSITION.
  MSG compositionMsg;
  if (WinUtils::PeekMessage(&compositionMsg, aWindow->GetWindowHandle(),
                            WM_IME_STARTCOMPOSITION, WM_IME_COMPOSITION,
                            PM_NOREMOVE) &&
      compositionMsg.message == WM_IME_COMPOSITION &&
      IS_COMMITTING_LPARAM(compositionMsg.lParam)) {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("OnIMEEndComposition, WM_IME_ENDCOMPOSITION is followed by "
       "WM_IME_COMPOSITION, ignoring the message..."));
    return true;
  }

  // Otherwise, e.g., ChangJie doesn't post WM_IME_COMPOSITION before
  // WM_IME_ENDCOMPOSITION when composition string becomes empty.
  // Then, we should dispatch a compositionupdate event, a compositionchange
  // event and a compositionend event.
  // XXX Shouldn't we dispatch the compositionchange event with actual or
  //     latest composition string?
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMEEndComposition, mCompositionString=\"%s\"%s",
     NS_ConvertUTF16toUTF8(mCompositionString).get(),
     mCompositionString.IsEmpty() ? "" : ", but canceling it..."));

  HandleEndComposition(aWindow, &EmptyString());

  return true;
}

// static
bool
IMMHandler::OnIMEChar(nsWindow* aWindow,
                      WPARAM wParam,
                      LPARAM lParam,
                      MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMEChar, hWnd=%08x, char=%08x",
     aWindow->GetWindowHandle(), wParam));

  // We don't need to fire any compositionchange events from here. This method
  // will be called when the composition string of the current IME is not drawn
  // by us and some characters are committed. In that case, the committed
  // string was processed in nsWindow::OnIMEComposition already.

  // We need to consume the message so that Windows don't send two WM_CHAR msgs
  aResult.mConsumed = true;
  return true;
}

// static
bool
IMMHandler::OnIMECompositionFull(nsWindow* aWindow,
                                 MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMECompositionFull, hWnd=%08x",
     aWindow->GetWindowHandle()));

  // not implement yet
  aResult.mConsumed = false;
  return true;
}

// static
bool
IMMHandler::OnIMENotify(nsWindow* aWindow,
                        WPARAM wParam,
                        LPARAM lParam,
                        MSGResult& aResult)
{
  switch (wParam) {
    case IMN_CHANGECANDIDATE:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_CHANGECANDIDATE, lParam=%08x",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_CLOSECANDIDATE:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_CLOSECANDIDATE, lParam=%08x",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_CLOSESTATUSWINDOW:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_CLOSESTATUSWINDOW",
         aWindow->GetWindowHandle()));
      break;
    case IMN_GUIDELINE:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_GUIDELINE",
         aWindow->GetWindowHandle()));
      break;
    case IMN_OPENCANDIDATE:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_OPENCANDIDATE, lParam=%08x",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_OPENSTATUSWINDOW:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_OPENSTATUSWINDOW",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETCANDIDATEPOS:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_SETCANDIDATEPOS, lParam=%08x",
         aWindow->GetWindowHandle(), lParam));
      break;
    case IMN_SETCOMPOSITIONFONT:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_SETCOMPOSITIONFONT",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETCOMPOSITIONWINDOW:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_SETCOMPOSITIONWINDOW",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETCONVERSIONMODE:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_SETCONVERSIONMODE",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETOPENSTATUS:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_SETOPENSTATUS",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETSENTENCEMODE:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_SETSENTENCEMODE",
         aWindow->GetWindowHandle()));
      break;
    case IMN_SETSTATUSWINDOWPOS:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_SETSTATUSWINDOWPOS",
         aWindow->GetWindowHandle()));
      break;
    case IMN_PRIVATE:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMENotify, hWnd=%08x, IMN_PRIVATE",
         aWindow->GetWindowHandle()));
      break;
  }

  // not implement yet
  aResult.mConsumed = false;
  return true;
}

bool
IMMHandler::OnIMERequest(nsWindow* aWindow,
                         WPARAM wParam,
                         LPARAM lParam,
                         MSGResult& aResult)
{
  switch (wParam) {
    case IMR_RECONVERTSTRING:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMERequest, hWnd=%08x, IMR_RECONVERTSTRING",
         aWindow->GetWindowHandle()));
      aResult.mConsumed = HandleReconvert(aWindow, lParam, &aResult.mResult);
      return true;
    case IMR_QUERYCHARPOSITION:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMERequest, hWnd=%08x, IMR_QUERYCHARPOSITION",
         aWindow->GetWindowHandle()));
      aResult.mConsumed =
        HandleQueryCharPosition(aWindow, lParam, &aResult.mResult);
      return true;
    case IMR_DOCUMENTFEED:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMERequest, hWnd=%08x, IMR_DOCUMENTFEED",
         aWindow->GetWindowHandle()));
      aResult.mConsumed = HandleDocumentFeed(aWindow, lParam, &aResult.mResult);
      return true;
    default:
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("OnIMERequest, hWnd=%08x, wParam=%08x",
         aWindow->GetWindowHandle(), wParam));
      aResult.mConsumed = false;
      return true;
  }
}

// static
bool
IMMHandler::OnIMESelect(nsWindow* aWindow,
                        WPARAM wParam,
                        LPARAM lParam,
                        MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMESelect, hWnd=%08x, wParam=%08x, lParam=%08x",
     aWindow->GetWindowHandle(), wParam, lParam));

  // not implement yet
  aResult.mConsumed = false;
  return true;
}

// static
bool
IMMHandler::OnIMESetContext(nsWindow* aWindow,
                            WPARAM wParam,
                            LPARAM lParam,
                            MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMESetContext, hWnd=%08x, %s, lParam=%08x",
     aWindow->GetWindowHandle(), wParam ? "Active" : "Deactive", lParam));

  aResult.mConsumed = false;

  // NOTE: If the aWindow is top level window of the composing window because
  // when a window on deactive window gets focus, WM_IME_SETCONTEXT (wParam is
  // TRUE) is sent to the top level window first.  After that,
  // WM_IME_SETCONTEXT (wParam is FALSE) is sent to the top level window.
  // Finally, WM_IME_SETCONTEXT (wParam is TRUE) is sent to the focused window.
  // The top level window never becomes composing window, so, we can ignore
  // the WM_IME_SETCONTEXT on the top level window.
  if (IsTopLevelWindowOfComposition(aWindow)) {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("OnIMESetContext, hWnd=%08x is top level window"));
    return true;
  }

  // When IME context is activating on another window,
  // we should commit the old composition on the old window.
  bool cancelComposition = false;
  if (wParam && gIMMHandler) {
    cancelComposition =
      gIMMHandler->CommitCompositionOnPreviousWindow(aWindow);
  }

  if (wParam && (lParam & ISC_SHOWUICOMPOSITIONWINDOW) &&
      ShouldDrawCompositionStringOurselves()) {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("OnIMESetContext, ISC_SHOWUICOMPOSITIONWINDOW is removed"));
    lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
  }

  // We should sent WM_IME_SETCONTEXT to the DefWndProc here because the
  // ancestor windows shouldn't receive this message.  If they receive the
  // message, we cannot know whether which window is the target of the message.
  aResult.mResult = ::DefWindowProc(aWindow->GetWindowHandle(),
                                    WM_IME_SETCONTEXT, wParam, lParam);

  // Cancel composition on the new window if we committed our composition on
  // another window.
  if (cancelComposition) {
    CancelComposition(aWindow, true);
  }

  aResult.mConsumed = true;
  return true;
}

bool
IMMHandler::OnChar(nsWindow* aWindow,
                   WPARAM wParam,
                   LPARAM lParam,
                   MSGResult& aResult)
{
  // The return value must be same as aResult.mConsumed because only when we
  // consume the message, the caller shouldn't do anything anymore but
  // otherwise, the caller should handle the message.
  aResult.mConsumed = false;
  if (IsIMECharRecordsEmpty()) {
    return aResult.mConsumed;
  }
  WPARAM recWParam;
  LPARAM recLParam;
  DequeueIMECharRecords(recWParam, recLParam);
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnChar, aWindow=%p, wParam=%08x, lParam=%08x, "
     "recorded: wParam=%08x, lParam=%08x",
     aWindow->GetWindowHandle(), wParam, lParam, recWParam, recLParam));
  // If an unexpected char message comes, we should reset the records,
  // of course, this shouldn't happen.
  if (recWParam != wParam || recLParam != lParam) {
    ResetIMECharRecords();
    return aResult.mConsumed;
  }
  // Eat the char message which is caused by WM_IME_CHAR because we should
  // have processed the IME messages, so, this message could be come from
  // a windowless plug-in.
  aResult.mConsumed = true;
  return aResult.mConsumed;
}

/****************************************************************************
 * message handlers for plug-in
 ****************************************************************************/

void
IMMHandler::OnIMEStartCompositionOnPlugin(nsWindow* aWindow,
                                          WPARAM wParam,
                                          LPARAM lParam)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMEStartCompositionOnPlugin, hWnd=%08x, mIsComposingOnPlugin=%s",
     aWindow->GetWindowHandle(), GetBoolName(mIsComposingOnPlugin)));
  mIsComposingOnPlugin = true;
  mDispatcher = GetTextEventDispatcherFor(aWindow);
  mComposingWindow = aWindow;
  IMEContext context(aWindow);
  SetIMERelatedWindowsPosOnPlugin(aWindow, context);
  // On widnowless plugin, we should assume that the focused editor is always
  // in horizontal writing mode.
  AdjustCompositionFont(aWindow, context, WritingMode());
}

void
IMMHandler::OnIMECompositionOnPlugin(nsWindow* aWindow,
                                     WPARAM wParam,
                                     LPARAM lParam)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMECompositionOnPlugin, hWnd=%08x, lParam=%08x, "
     "mIsComposingOnPlugin=%s, GCS_RESULTSTR=%s, GCS_COMPSTR=%s, "
     "GCS_COMPATTR=%s, GCS_COMPCLAUSE=%s, GCS_CURSORPOS=%s",
     aWindow->GetWindowHandle(), lParam, GetBoolName(mIsComposingOnPlugin),
     GetBoolName(lParam & GCS_RESULTSTR), GetBoolName(lParam & GCS_COMPSTR),
     GetBoolName(lParam & GCS_COMPATTR), GetBoolName(lParam & GCS_COMPCLAUSE),
     GetBoolName(lParam & GCS_CURSORPOS)));
  // We should end composition if there is a committed string.
  if (IS_COMMITTING_LPARAM(lParam)) {
    mIsComposingOnPlugin = false;
    mComposingWindow = nullptr;
    mDispatcher = nullptr;
    return;
  }
  // Continue composition if there is still a string being composed.
  if (IS_COMPOSING_LPARAM(lParam)) {
    mIsComposingOnPlugin = true;
    mDispatcher = GetTextEventDispatcherFor(aWindow);
    mComposingWindow = aWindow;
    IMEContext context(aWindow);
    SetIMERelatedWindowsPosOnPlugin(aWindow, context);
  }
}

void
IMMHandler::OnIMEEndCompositionOnPlugin(nsWindow* aWindow,
                                        WPARAM wParam,
                                        LPARAM lParam)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMEEndCompositionOnPlugin, hWnd=%08x, mIsComposingOnPlugin=%s",
     aWindow->GetWindowHandle(), GetBoolName(mIsComposingOnPlugin)));

  mIsComposingOnPlugin = false;
  mComposingWindow = nullptr;
  mDispatcher = nullptr;

  if (mNativeCaretIsCreated) {
    ::DestroyCaret();
    mNativeCaretIsCreated = false;
  }
}

bool
IMMHandler::OnIMECharOnPlugin(nsWindow* aWindow,
                              WPARAM wParam,
                              LPARAM lParam,
                              MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMECharOnPlugin, hWnd=%08x, char=%08x, scancode=%08x",
     aWindow->GetWindowHandle(), wParam, lParam));

  aResult.mConsumed =
    aWindow->DispatchPluginEvent(WM_IME_CHAR, wParam, lParam, true);

  if (!aResult.mConsumed) {
    // Record the WM_CHAR messages which are going to be coming.
    EnsureHandlerInstance();
    EnqueueIMECharRecords(wParam, lParam);
  }
  return true;
}

// static
bool
IMMHandler::OnIMESetContextOnPlugin(nsWindow* aWindow,
                                    WPARAM wParam,
                                    LPARAM lParam,
                                    MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnIMESetContextOnPlugin, hWnd=%08x, %s, lParam=%08x",
     aWindow->GetWindowHandle(), wParam ? "Active" : "Deactive", lParam));

  // If the IME context becomes active on a plug-in, we should commit
  // our composition.  And also we should cancel the composition on new
  // window.  Note that if IsTopLevelWindowOfComposition(aWindow) returns
  // true, we should ignore the message here, see the comment in
  // OnIMESetContext() for the detail.
  if (wParam && gIMMHandler && !IsTopLevelWindowOfComposition(aWindow)) {
    if (gIMMHandler->CommitCompositionOnPreviousWindow(aWindow)) {
      CancelComposition(aWindow);
    }
  }

  // Dispatch message to the plug-in.
  // XXX When a windowless plug-in gets focus, we should send
  //     WM_IME_SETCONTEXT
  aWindow->DispatchPluginEvent(WM_IME_SETCONTEXT, wParam, lParam, false);

  // We should send WM_IME_SETCONTEXT to the DefWndProc here.  It shouldn't
  // be received on ancestor windows, see OnIMESetContext() for the detail.
  aResult.mResult = ::DefWindowProc(aWindow->GetWindowHandle(),
                                    WM_IME_SETCONTEXT, wParam, lParam);

  // Don't synchronously dispatch the pending events when we receive
  // WM_IME_SETCONTEXT because we get it during plugin destruction.
  // (bug 491848)
  aResult.mConsumed = true;
  return true;
}

bool
IMMHandler::OnCharOnPlugin(nsWindow* aWindow,
                           WPARAM wParam,
                           LPARAM lParam,
                           MSGResult& aResult)
{
  NS_WARNING("OnCharOnPlugin");
  if (mIsComposing) {
    aWindow->NotifyIME(REQUEST_TO_COMMIT_COMPOSITION);
    return true;
  }

  // We should never consume char message on windowless plugin.
  aResult.mConsumed = false;
  if (IsIMECharRecordsEmpty()) {
    return false;
  }

  WPARAM recWParam;
  LPARAM recLParam;
  DequeueIMECharRecords(recWParam, recLParam);
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnCharOnPlugin, aWindow=%p, wParam=%08x, lParam=%08x, "
     "recorded: wParam=%08x, lParam=%08x",
     aWindow->GetWindowHandle(), wParam, lParam, recWParam, recLParam));
  // If an unexpected char message comes, we should reset the records,
  // of course, this shouldn't happen.
  if (recWParam != wParam || recLParam != lParam) {
    ResetIMECharRecords();
  }
  // WM_CHAR on plug-in is always handled by nsWindow.
  return false;
}

/****************************************************************************
 * others
 ****************************************************************************/

TextEventDispatcher*
IMMHandler::GetTextEventDispatcherFor(nsWindow* aWindow)
{
  return aWindow == mComposingWindow && mDispatcher ?
    mDispatcher.get() : aWindow->GetTextEventDispatcher();
}

void
IMMHandler::HandleStartComposition(nsWindow* aWindow,
                                   const IMEContext& aContext)
{
  NS_PRECONDITION(!mIsComposing,
    "HandleStartComposition is called but mIsComposing is TRUE");

  Selection& selection = GetSelection();
  if (!selection.EnsureValidSelection(aWindow)) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleStartComposition, FAILED, due to "
       "Selection::EnsureValidSelection() failure"));
    return;
  }

  AdjustCompositionFont(aWindow, aContext, selection.mWritingMode);

  mCompositionStart = selection.mOffset;
  mCursorPosition = NO_IME_CARET;

  RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcherFor(aWindow);
  nsresult rv = dispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleStartComposition, FAILED due to "
       "TextEventDispatcher::BeginNativeInputTransaction() failure"));
    return;
  }
  WidgetEventTime eventTime = aWindow->CurrentMessageWidgetEventTime();
  nsEventStatus status;
  rv = dispatcher->StartComposition(status, &eventTime);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleStartComposition, FAILED, due to "
       "TextEventDispatcher::StartComposition() failure"));
    return;
  }

  mIsComposing = true;
  mComposingWindow = aWindow;
  mDispatcher = dispatcher;

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("HandleStartComposition, START composition, mCompositionStart=%ld",
     mCompositionStart));
}

bool
IMMHandler::HandleComposition(nsWindow* aWindow,
                              const IMEContext& aContext,
                              LPARAM lParam)
{
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
    if (WinUtils::PeekMessage(&msg1, wnd, WM_IME_STARTCOMPOSITION,
                              WM_IME_COMPOSITION, PM_NOREMOVE) &&
        msg1.message == WM_IME_STARTCOMPOSITION &&
        WinUtils::PeekMessage(&msg2, wnd, WM_IME_ENDCOMPOSITION,
                              WM_IME_COMPOSITION, PM_NOREMOVE) &&
        msg2.message == WM_IME_COMPOSITION) {
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("HandleComposition, Ignores due to find a "
         "WM_IME_STARTCOMPOSITION"));
      return ShouldDrawCompositionStringOurselves();
    }
  }

  bool startCompositionMessageHasBeenSent = mIsComposing;

  //
  // This catches a fixed result
  //
  if (IS_COMMITTING_LPARAM(lParam)) {
    if (!mIsComposing) {
      HandleStartComposition(aWindow, aContext);
    }

    GetCompositionString(aContext, GCS_RESULTSTR, mCompositionString);

    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("HandleComposition, GCS_RESULTSTR"));

    HandleEndComposition(aWindow, &mCompositionString);

    if (!IS_COMPOSING_LPARAM(lParam)) {
      return ShouldDrawCompositionStringOurselves();
    }
  }


  //
  // This provides us with a composition string
  //
  if (!mIsComposing) {
    HandleStartComposition(aWindow, aContext);
  }

  //--------------------------------------------------------
  // 1. Get GCS_COMPSTR
  //--------------------------------------------------------
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("HandleComposition, GCS_COMPSTR"));

  nsAutoString previousCompositionString(mCompositionString);
  GetCompositionString(aContext, GCS_COMPSTR, mCompositionString);

  if (!IS_COMPOSING_LPARAM(lParam)) {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("HandleComposition, lParam doesn't indicate composing, "
       "mCompositionString=\"%s\", previousCompositionString=\"%s\"",
       NS_ConvertUTF16toUTF8(mCompositionString).get(),
       NS_ConvertUTF16toUTF8(previousCompositionString).get()));

    // If composition string isn't changed, we can trust the lParam.
    // So, we need to do nothing.
    if (previousCompositionString == mCompositionString) {
      return ShouldDrawCompositionStringOurselves();
    }

    // IME may send WM_IME_COMPOSITION without composing lParam values
    // when composition string becomes empty (e.g., using Backspace key).
    // If composition string is empty, we should dispatch a compositionchange
    // event with empty string and clear the clause information.
    if (mCompositionString.IsEmpty()) {
      mClauseArray.Clear();
      mAttributeArray.Clear();
      mCursorPosition = 0;
      DispatchCompositionChangeEvent(aWindow, aContext);
      return ShouldDrawCompositionStringOurselves();
    }

    // Otherwise, we cannot trust the lParam value.  We might need to
    // dispatch compositionchange event with the latest composition string
    // information.
  }

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
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("HandleComposition, Aborting GCS_COMPSTR"));
    HandleEndComposition(aWindow);
    return IS_COMMITTING_LPARAM(lParam);
  }

  //--------------------------------------------------------
  // 2. Get GCS_COMPCLAUSE
  //--------------------------------------------------------
  long clauseArrayLength =
    ::ImmGetCompositionStringW(aContext.get(), GCS_COMPCLAUSE, nullptr, 0);
  clauseArrayLength /= sizeof(uint32_t);

  if (clauseArrayLength > 0) {
    nsresult rv = EnsureClauseArray(clauseArrayLength);
    NS_ENSURE_SUCCESS(rv, false);

    // Intelligent ABC IME (Simplified Chinese IME, the code page is 936)
    // will crash in ImmGetCompositionStringW for GCS_COMPCLAUSE (bug 424663).
    // See comment 35 of the bug for the detail. Therefore, we should use A
    // API for it, however, we should not kill Unicode support on all IMEs.
    bool useA_API = !(sIMEProperty & IME_PROP_UNICODE);

    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("HandleComposition, GCS_COMPCLAUSE, useA_API=%s",
       useA_API ? "TRUE" : "FALSE"));

    long clauseArrayLength2 = 
      useA_API ?
        ::ImmGetCompositionStringA(aContext.get(), GCS_COMPCLAUSE,
                                   mClauseArray.Elements(),
                                   mClauseArray.Capacity() * sizeof(uint32_t)) :
        ::ImmGetCompositionStringW(aContext.get(), GCS_COMPCLAUSE,
                                   mClauseArray.Elements(),
                                   mClauseArray.Capacity() * sizeof(uint32_t));
    clauseArrayLength2 /= sizeof(uint32_t);

    if (clauseArrayLength != clauseArrayLength2) {
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("HandleComposition, GCS_COMPCLAUSE, clauseArrayLength=%ld but "
         "clauseArrayLength2=%ld",
         clauseArrayLength, clauseArrayLength2));
      if (clauseArrayLength > clauseArrayLength2)
        clauseArrayLength = clauseArrayLength2;
    }

    if (useA_API && clauseArrayLength > 0) {
      // Convert each values of sIMECompClauseArray. The values mean offset of
      // the clauses in ANSI string. But we need the values in Unicode string.
      nsAutoCString compANSIStr;
      if (ConvertToANSIString(mCompositionString, GetKeyboardCodePage(),
                              compANSIStr)) {
        uint32_t maxlen = compANSIStr.Length();
        mClauseArray.SetLength(clauseArrayLength);
        mClauseArray[0] = 0; // first value must be 0
        for (int32_t i = 1; i < clauseArrayLength; i++) {
          uint32_t len = std::min(mClauseArray[i], maxlen);
          mClauseArray[i] = ::MultiByteToWideChar(GetKeyboardCodePage(), 
                                                  MB_PRECOMPOSED,
                                                  (LPCSTR)compANSIStr.get(),
                                                  len, nullptr, 0);
        }
      }
    }
  }
  // compClauseArrayLength may be negative. I.e., ImmGetCompositionStringW
  // may return an error code.
  mClauseArray.SetLength(std::max<long>(0, clauseArrayLength));

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("HandleComposition, GCS_COMPCLAUSE, mClauseLength=%ld",
     mClauseArray.Length()));

  //--------------------------------------------------------
  // 3. Get GCS_COMPATTR
  //--------------------------------------------------------
  // This provides us with the attribute string necessary 
  // for doing hiliting
  long attrArrayLength =
    ::ImmGetCompositionStringW(aContext.get(), GCS_COMPATTR, nullptr, 0);
  attrArrayLength /= sizeof(uint8_t);

  if (attrArrayLength > 0) {
    nsresult rv = EnsureAttributeArray(attrArrayLength);
    NS_ENSURE_SUCCESS(rv, false);
    attrArrayLength =
      ::ImmGetCompositionStringW(aContext.get(), GCS_COMPATTR,
                                 mAttributeArray.Elements(),
                                 mAttributeArray.Capacity() * sizeof(uint8_t));
  }

  // attrStrLen may be negative. I.e., ImmGetCompositionStringW may return an
  // error code.
  mAttributeArray.SetLength(std::max<long>(0, attrArrayLength));

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("HandleComposition, GCS_COMPATTR, mAttributeLength=%ld",
     mAttributeArray.Length()));

  //--------------------------------------------------------
  // 4. Get GCS_CURSOPOS
  //--------------------------------------------------------
  // Some IMEs (e.g., the standard IME for Korean) don't have caret position.
  if (lParam & GCS_CURSORPOS) {
    mCursorPosition =
      ::ImmGetCompositionStringW(aContext.get(), GCS_CURSORPOS, nullptr, 0);
    if (mCursorPosition < 0) {
      mCursorPosition = NO_IME_CARET; // The result is error
    }
  } else {
    mCursorPosition = NO_IME_CARET;
  }

  NS_ASSERTION(mCursorPosition <= (long)mCompositionString.Length(),
               "illegal pos");

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("HandleComposition, GCS_CURSORPOS, mCursorPosition=%d",
     mCursorPosition));

  //--------------------------------------------------------
  // 5. Send the compositionchange event
  //--------------------------------------------------------
  DispatchCompositionChangeEvent(aWindow, aContext);

  return ShouldDrawCompositionStringOurselves();
}

void
IMMHandler::HandleEndComposition(nsWindow* aWindow,
                                 const nsAString* aCommitString)
{
  MOZ_ASSERT(mIsComposing,
    "HandleEndComposition is called but mIsComposing is FALSE");

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("HandleEndComposition(aWindow=0x%p, aCommitString=0x%p (\"%s\"))",
     aWindow, aCommitString,
     aCommitString ? NS_ConvertUTF16toUTF8(*aCommitString).get() : ""));

  if (mNativeCaretIsCreated) {
    ::DestroyCaret();
    mNativeCaretIsCreated = false;
  }

  RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcherFor(aWindow);
  nsresult rv = dispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleEndComposition, FAILED due to "
       "TextEventDispatcher::BeginNativeInputTransaction() failure"));
    return;
  }
  WidgetEventTime eventTime = aWindow->CurrentMessageWidgetEventTime();
  nsEventStatus status;
  rv = dispatcher->CommitComposition(status, aCommitString, &eventTime);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleStartComposition, FAILED, due to "
       "TextEventDispatcher::CommitComposition() failure"));
    return;
  }
  mIsComposing = false;
  // XXX aWindow and mComposingWindow are always same??
  mComposingWindow = nullptr;
  mDispatcher = nullptr;
}

bool
IMMHandler::HandleReconvert(nsWindow* aWindow,
                            LPARAM lParam,
                            LRESULT* oResult)
{
  *oResult = 0;
  RECONVERTSTRING* pReconv = reinterpret_cast<RECONVERTSTRING*>(lParam);

  Selection& selection = GetSelection();
  if (!selection.EnsureValidSelection(aWindow)) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleReconvert, FAILED, due to "
       "Selection::EnsureValidSelection() failure"));
    return false;
  }

  uint32_t len = selection.Length();
  uint32_t needSize = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!pReconv) {
    // Return need size to reconvert.
    if (len == 0) {
      MOZ_LOG(gIMMLog, LogLevel::Error,
        ("HandleReconvert, There are not selected text"));
      return false;
    }
    *oResult = needSize;
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("HandleReconvert, succeeded, result=%ld",
       *oResult));
    return true;
  }

  if (pReconv->dwSize < needSize) {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("HandleReconvert, FAILED, pReconv->dwSize=%ld, needSize=%ld",
       pReconv->dwSize, needSize));
    return false;
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
               selection.mString.get(), len * sizeof(WCHAR));

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("HandleReconvert, SUCCEEDED, pReconv=%s, result=%ld",
     GetReconvertStringLog(pReconv).get(), *oResult));

  return true;
}

bool
IMMHandler::HandleQueryCharPosition(nsWindow* aWindow,
                                    LPARAM lParam,
                                    LRESULT* oResult)
{
  uint32_t len = mIsComposing ? mCompositionString.Length() : 0;
  *oResult = false;
  IMECHARPOSITION* pCharPosition = reinterpret_cast<IMECHARPOSITION*>(lParam);
  if (!pCharPosition) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleQueryCharPosition, FAILED, due to pCharPosition is null"));
    return false;
  }
  if (pCharPosition->dwSize < sizeof(IMECHARPOSITION)) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleReconvert, FAILED, pCharPosition->dwSize=%ld, "
       "sizeof(IMECHARPOSITION)=%ld",
       pCharPosition->dwSize, sizeof(IMECHARPOSITION)));
    return false;
  }
  if (::GetFocus() != aWindow->GetWindowHandle()) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleReconvert, FAILED, ::GetFocus()=%08x, OurWindowHandle=%08x",
       ::GetFocus(), aWindow->GetWindowHandle()));
    return false;
  }
  if (pCharPosition->dwCharPos > len) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleQueryCharPosition, FAILED, pCharPosition->dwCharPos=%ld, "
       "len=%ld",
       pCharPosition->dwCharPos, len));
    return false;
  }

  LayoutDeviceIntRect r;
  bool ret =
    GetCharacterRectOfSelectedTextAt(aWindow, pCharPosition->dwCharPos, r);
  NS_ENSURE_TRUE(ret, false);

  LayoutDeviceIntRect screenRect;
  // We always need top level window that is owner window of the popup window
  // even if the content of the popup window has focus.
  ResolveIMECaretPos(aWindow->GetTopLevelWindow(false),
                     r, nullptr, screenRect);

  // XXX This might need to check writing mode.  However, MSDN doesn't explain
  //     how to set the values in vertical writing mode. Additionally, IME
  //     doesn't work well with top-left of the character (this is explicitly
  //     documented) and its horizontal width.  So, it might be better to set
  //     top-right corner of the character and horizontal width, but we're not
  //     sure if it doesn't cause any problems with a lot of IMEs...
  pCharPosition->pt.x = screenRect.x;
  pCharPosition->pt.y = screenRect.y;

  pCharPosition->cLineHeight = r.height;

  WidgetQueryContentEvent editorRect(true, eQueryEditorRect, aWindow);
  aWindow->InitEvent(editorRect);
  DispatchEvent(aWindow, editorRect);
  if (NS_WARN_IF(!editorRect.mSucceeded)) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleQueryCharPosition, eQueryEditorRect failed"));
    ::GetWindowRect(aWindow->GetWindowHandle(), &pCharPosition->rcDocument);
  } else {
    LayoutDeviceIntRect editorRectInWindow = editorRect.mReply.mRect;
    nsWindow* window = editorRect.mReply.mFocusedWidget ?
      static_cast<nsWindow*>(editorRect.mReply.mFocusedWidget) : aWindow;
    LayoutDeviceIntRect editorRectInScreen;
    ResolveIMECaretPos(window, editorRectInWindow, nullptr, editorRectInScreen);
    ::SetRect(&pCharPosition->rcDocument,
              editorRectInScreen.x, editorRectInScreen.y,
              editorRectInScreen.XMost(), editorRectInScreen.YMost());
  }

  *oResult = TRUE;

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("HandleQueryCharPosition, SUCCEEDED, pCharPosition={ pt={ x=%d, "
     "y=%d }, cLineHeight=%d, rcDocument={ left=%d, top=%d, right=%d, "
     "bottom=%d } }",
     pCharPosition->pt.x, pCharPosition->pt.y, pCharPosition->cLineHeight,
     pCharPosition->rcDocument.left, pCharPosition->rcDocument.top,
     pCharPosition->rcDocument.right, pCharPosition->rcDocument.bottom));
  return true;
}

bool
IMMHandler::HandleDocumentFeed(nsWindow* aWindow,
                               LPARAM lParam,
                               LRESULT* oResult)
{
  *oResult = 0;
  RECONVERTSTRING* pReconv = reinterpret_cast<RECONVERTSTRING*>(lParam);

  LayoutDeviceIntPoint point(0, 0);

  bool hasCompositionString =
    mIsComposing && ShouldDrawCompositionStringOurselves();

  int32_t targetOffset, targetLength;
  if (!hasCompositionString) {
    Selection& selection = GetSelection();
    if (!selection.EnsureValidSelection(aWindow)) {
      MOZ_LOG(gIMMLog, LogLevel::Error,
        ("HandleDocumentFeed, FAILED, due to "
         "Selection::EnsureValidSelection() failure"));
      return false;
    }
    targetOffset = int32_t(selection.mOffset);
    targetLength = int32_t(selection.Length());
  } else {
    targetOffset = int32_t(mCompositionStart);
    targetLength = int32_t(mCompositionString.Length());
  }

  // XXX nsString::Find and nsString::RFind take int32_t for offset, so,
  //     we cannot support this message when the current offset is larger than
  //     INT32_MAX.
  if (targetOffset < 0 || targetLength < 0 ||
      targetOffset + targetLength < 0) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleDocumentFeed, FAILED, due to the selection is out of "
       "range"));
    return false;
  }

  // Get all contents of the focused editor.
  WidgetQueryContentEvent textContent(true, eQueryTextContent, aWindow);
  textContent.InitForQueryTextContent(0, UINT32_MAX);
  aWindow->InitEvent(textContent, &point);
  DispatchEvent(aWindow, textContent);
  if (!textContent.mSucceeded) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleDocumentFeed, FAILED, due to eQueryTextContent failure"));
    return false;
  }

  nsAutoString str(textContent.mReply.mString);
  if (targetOffset > int32_t(str.Length())) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleDocumentFeed, FAILED, due to the caret offset is invalid"));
    return false;
  }

  // Get the focused paragraph, we decide that it starts from the previous CRLF
  // (or start of the editor) to the next one (or the end of the editor).
  int32_t paragraphStart = str.RFind("\n", false, targetOffset, -1) + 1;
  int32_t paragraphEnd =
    str.Find("\r", false, targetOffset + targetLength, -1);
  if (paragraphEnd < 0) {
    paragraphEnd = str.Length();
  }
  nsDependentSubstring paragraph(str, paragraphStart,
                                 paragraphEnd - paragraphStart);

  uint32_t len = paragraph.Length();
  uint32_t needSize = sizeof(RECONVERTSTRING) + len * sizeof(WCHAR);

  if (!pReconv) {
    *oResult = needSize;
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("HandleDocumentFeed, succeeded, result=%ld",
       *oResult));
    return true;
  }

  if (pReconv->dwSize < needSize) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("HandleDocumentFeed, FAILED, pReconv->dwSize=%ld, needSize=%ld",
       pReconv->dwSize, needSize));
    return false;
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
    uint32_t offset, length;
    if (!GetTargetClauseRange(&offset, &length)) {
      MOZ_LOG(gIMMLog, LogLevel::Error,
        ("HandleDocumentFeed, FAILED, due to GetTargetClauseRange() "
         "failure"));
      return false;
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

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("HandleDocumentFeed, SUCCEEDED, pReconv=%s, result=%ld",
     GetReconvertStringLog(pReconv).get(), *oResult));

  return true;
}

bool
IMMHandler::CommitCompositionOnPreviousWindow(nsWindow* aWindow)
{
  if (!mComposingWindow || mComposingWindow == aWindow) {
    return false;
  }

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("CommitCompositionOnPreviousWindow, mIsComposing=%s",
     GetBoolName(mIsComposing)));

  // If we have composition, we should dispatch composition events internally.
  if (mIsComposing) {
    IMEContext context(mComposingWindow);
    NS_ASSERTION(context.IsValid(), "IME context must be valid");

    HandleEndComposition(mComposingWindow);
    return true;
  }

  return false;
}

static TextRangeType
PlatformToNSAttr(uint8_t aAttr)
{
  switch (aAttr)
  {
    case ATTR_INPUT_ERROR:
    // case ATTR_FIXEDCONVERTED:
    case ATTR_INPUT:
      return TextRangeType::eRawClause;
    case ATTR_CONVERTED:
      return TextRangeType::eConvertedClause;
    case ATTR_TARGET_NOTCONVERTED:
      return TextRangeType::eSelectedRawClause;
    case ATTR_TARGET_CONVERTED:
      return TextRangeType::eSelectedClause;
    default:
      NS_ASSERTION(false, "unknown attribute");
      return TextRangeType::eCaret;
  }
}

// static
void
IMMHandler::DispatchEvent(nsWindow* aWindow, WidgetGUIEvent& aEvent)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("DispatchEvent(aWindow=0x%p, aEvent={ mMessage=%s }, "
     "aWindow->Destroyed()=%s",
     aWindow, ToChar(aEvent.mMessage), GetBoolName(aWindow->Destroyed())));

  if (aWindow->Destroyed()) {
    return;
  }

  aWindow->DispatchWindowEvent(&aEvent);
}

void
IMMHandler::DispatchCompositionChangeEvent(nsWindow* aWindow,
                                           const IMEContext& aContext)
{
  NS_ASSERTION(mIsComposing, "conflict state");
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("DispatchCompositionChangeEvent"));

  // If we don't need to draw composition string ourselves, we don't need to
  // fire compositionchange event during composing.
  if (!ShouldDrawCompositionStringOurselves()) {
    // But we need to adjust composition window pos and native caret pos, here.
    SetIMERelatedWindowsPos(aWindow, aContext);
    return;
  }

  RefPtr<nsWindow> kungFuDeathGrip(aWindow);
  RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcherFor(aWindow);
  nsresult rv = dispatcher->BeginNativeInputTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("DispatchCompositionChangeEvent, FAILED due to "
       "TextEventDispatcher::BeginNativeInputTransaction() failure"));
    return;
  }

  // NOTE: Calling SetIMERelatedWindowsPos() from this method will be failure
  //       in e10s mode.  compositionchange event will notify this of
  //       NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED, then
  //       SetIMERelatedWindowsPos() will be called.

  // XXX Sogou (Simplified Chinese IME) returns contradictory values:
  //     The cursor position is actual cursor position. However, other values
  //     (composition string and attributes) are empty.

  if (mCompositionString.IsEmpty()) {
    // Don't append clause information if composition string is empty.
  } else if (mClauseArray.IsEmpty()) {
    // Some IMEs don't return clause array information, then, we assume that
    // all characters in the composition string are in one clause.
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("DispatchCompositionChangeEvent, mClauseArray.Length()=0"));
    rv =dispatcher->SetPendingComposition(mCompositionString, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(gIMMLog, LogLevel::Error,
        ("DispatchCompositionChangeEvent, FAILED due to"
         "TextEventDispatcher::SetPendingComposition() failure"));
      return;
    }
  } else {
    // iterate over the attributes
    rv = dispatcher->SetPendingCompositionString(mCompositionString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      MOZ_LOG(gIMMLog, LogLevel::Error,
        ("DispatchCompositionChangeEvent, FAILED due to"
         "TextEventDispatcher::SetPendingCompositionString() failure"));
      return;
    }
    uint32_t lastOffset = 0;
    for (uint32_t i = 0; i < mClauseArray.Length() - 1; i++) {
      uint32_t current = mClauseArray[i + 1];
      if (current > mCompositionString.Length()) {
        MOZ_LOG(gIMMLog, LogLevel::Info,
          ("DispatchCompositionChangeEvent, mClauseArray[%ld]=%lu. "
           "This is larger than mCompositionString.Length()=%lu",
           i + 1, current, mCompositionString.Length()));
        current = int32_t(mCompositionString.Length());
      }

      uint32_t length = current - lastOffset;
      if (NS_WARN_IF(lastOffset >= mAttributeArray.Length())) {
        MOZ_LOG(gIMMLog, LogLevel::Error,
          ("DispatchCompositionChangeEvent, FAILED due to invalid data of "
            "mClauseArray or mAttributeArray"));
        return;
      }
      TextRangeType textRangeType =
        PlatformToNSAttr(mAttributeArray[lastOffset]);
      rv = dispatcher->AppendClauseToPendingComposition(length, textRangeType);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        MOZ_LOG(gIMMLog, LogLevel::Error,
          ("DispatchCompositionChangeEvent, FAILED due to"
           "TextEventDispatcher::AppendClauseToPendingComposition() failure"));
        return;
      }

      lastOffset = current;

      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("DispatchCompositionChangeEvent, index=%ld, rangeType=%s, "
         "range length=%lu",
         i, ToChar(textRangeType), length));
    }
  }

  if (mCursorPosition == NO_IME_CARET) {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("DispatchCompositionChangeEvent, no caret"));
  } else {
    uint32_t cursor = static_cast<uint32_t>(mCursorPosition);
    if (cursor > mCompositionString.Length()) {
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("CreateTextRangeArray, mCursorPosition=%ld. "
         "This is larger than mCompositionString.Length()=%lu",
         mCursorPosition, mCompositionString.Length()));
      cursor = mCompositionString.Length();
    }

    // If caret is in the target clause, the target clause will be painted as
    // normal selection range.  Since caret shouldn't be in selection range on
    // Windows, we shouldn't append caret range in such case.
    const TextRangeArray* clauses = dispatcher->GetPendingCompositionClauses();
    const TextRange* targetClause =
      clauses ? clauses->GetTargetClause() : nullptr;
    if (targetClause &&
        cursor >= targetClause->mStartOffset &&
        cursor <= targetClause->mEndOffset) {
      // Forget the caret position specified by IME since Gecko's caret position
      // will be at the end of composition string.
      mCursorPosition = NO_IME_CARET;
      MOZ_LOG(gIMMLog, LogLevel::Info,
        ("CreateTextRangeArray, no caret due to it's in the target "
         "clause, now, mCursorPosition is NO_IME_CARET"));
    }

    if (mCursorPosition != NO_IME_CARET) {
      rv = dispatcher->SetCaretInPendingComposition(cursor, 0);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        MOZ_LOG(gIMMLog, LogLevel::Error,
          ("DispatchCompositionChangeEvent, FAILED due to"
           "TextEventDispatcher::SetCaretInPendingComposition() failure"));
        return;
      }
    }
  }

  WidgetEventTime eventTime = aWindow->CurrentMessageWidgetEventTime();
  nsEventStatus status;
  rv = dispatcher->FlushPendingComposition(status, &eventTime);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("DispatchCompositionChangeEvent, FAILED due to"
       "TextEventDispatcher::FlushPendingComposition() failure"));
    return;
  }
}

void
IMMHandler::GetCompositionString(const IMEContext& aContext,
                                 DWORD aIndex,
                                 nsAString& aCompositionString) const
{
  aCompositionString.Truncate();

  // Retrieve the size of the required output buffer.
  long lRtn = ::ImmGetCompositionStringW(aContext.get(), aIndex, nullptr, 0);
  if (lRtn < 0 ||
      !aCompositionString.SetLength((lRtn / sizeof(WCHAR)) + 1,
                                    mozilla::fallible)) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("GetCompositionString, FAILED, due to OOM"));
    return; // Error or out of memory.
  }

  // Actually retrieve the composition string information.
  lRtn = ::ImmGetCompositionStringW(aContext.get(), aIndex,
                                    (LPVOID)aCompositionString.BeginWriting(),
                                    lRtn + sizeof(WCHAR));
  aCompositionString.SetLength(lRtn / sizeof(WCHAR));

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("GetCompositionString, succeeded, aCompositionString=\"%s\"",
     NS_ConvertUTF16toUTF8(aCompositionString).get()));
}

bool
IMMHandler::GetTargetClauseRange(uint32_t* aOffset,
                                 uint32_t* aLength)
{
  NS_ENSURE_TRUE(aOffset, false);
  NS_ENSURE_TRUE(mIsComposing, false);
  NS_ENSURE_TRUE(ShouldDrawCompositionStringOurselves(), false);

  bool found = false;
  *aOffset = mCompositionStart;
  for (uint32_t i = 0; i < mAttributeArray.Length(); i++) {
    if (mAttributeArray[i] == ATTR_TARGET_NOTCONVERTED ||
        mAttributeArray[i] == ATTR_TARGET_CONVERTED) {
      *aOffset = mCompositionStart + i;
      found = true;
      break;
    }
  }

  if (!aLength) {
    return true;
  }

  if (!found) {
    // The all composition string is targetted when there is no ATTR_TARGET_*
    // clause. E.g., there is only ATTR_INPUT
    *aLength = mCompositionString.Length();
    return true;
  }

  uint32_t offsetInComposition = *aOffset - mCompositionStart;
  *aLength = mCompositionString.Length() - offsetInComposition;
  for (uint32_t i = offsetInComposition; i < mAttributeArray.Length(); i++) {
    if (mAttributeArray[i] != ATTR_TARGET_NOTCONVERTED &&
        mAttributeArray[i] != ATTR_TARGET_CONVERTED) {
      *aLength = i - offsetInComposition;
      break;
    }
  }
  return true;
}

bool
IMMHandler::ConvertToANSIString(const nsAFlatString& aStr,
                                UINT aCodePage,
                                nsACString& aANSIStr)
{
  int len = ::WideCharToMultiByte(aCodePage, 0,
                                  (LPCWSTR)aStr.get(), aStr.Length(),
                                  nullptr, 0, nullptr, nullptr);
  NS_ENSURE_TRUE(len >= 0, false);

  if (!aANSIStr.SetLength(len, mozilla::fallible)) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("ConvertToANSIString, FAILED, due to OOM"));
    return false;
  }
  ::WideCharToMultiByte(aCodePage, 0, (LPCWSTR)aStr.get(), aStr.Length(),
                        (LPSTR)aANSIStr.BeginWriting(), len, nullptr, nullptr);
  return true;
}

bool
IMMHandler::GetCharacterRectOfSelectedTextAt(nsWindow* aWindow,
                                             uint32_t aOffset,
                                             LayoutDeviceIntRect& aCharRect,
                                             WritingMode* aWritingMode)
{
  LayoutDeviceIntPoint point(0, 0);

  Selection& selection = GetSelection();
  if (!selection.EnsureValidSelection(aWindow)) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("GetCharacterRectOfSelectedTextAt, FAILED, due to "
       "Selection::EnsureValidSelection() failure"));
    return false;
  }

  // If the offset is larger than the end of composition string or selected
  // string, we should return false since such case must be a bug of the caller
  // or the active IME.  If it's an IME's bug, we need to set targetLength to
  // aOffset.
  uint32_t targetLength =
    mIsComposing ? mCompositionString.Length() : selection.Length();
  if (NS_WARN_IF(aOffset > targetLength)) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("GetCharacterRectOfSelectedTextAt, FAILED, due to "
       "aOffset is too large (aOffset=%u, targetLength=%u, mIsComposing=%s)",
       aOffset, targetLength, GetBoolName(mIsComposing)));
    return false;
  }

  // If there is caret, we might be able to use caret rect.
  uint32_t caretOffset = UINT32_MAX;
  // There is a caret only when the normal selection is collapsed.
  if (selection.Collapsed()) {
    if (mIsComposing) {
      // If it's composing, mCursorPosition is the offset to caret in
      // the composition string.
      if (mCursorPosition != NO_IME_CARET) {
        MOZ_ASSERT(mCursorPosition >= 0);
        caretOffset = mCursorPosition;
      } else if (!ShouldDrawCompositionStringOurselves() ||
                 mCompositionString.IsEmpty()) {
        // Otherwise, if there is no composition string, we should assume that
        // there is a caret at the start of composition string.
        caretOffset = 0;
      }
    } else {
      // If there is no composition, the selection offset is the caret offset.
      caretOffset = 0;
    }
  }

  // If there is a caret and retrieving offset is same as the caret offset,
  // we should use the caret rect.
  if (aOffset != caretOffset) {
    WidgetQueryContentEvent charRect(true, eQueryTextRect, aWindow);
    WidgetQueryContentEvent::Options options;
    options.mRelativeToInsertionPoint = true;
    charRect.InitForQueryTextRect(aOffset, 1, options);
    aWindow->InitEvent(charRect, &point);
    DispatchEvent(aWindow, charRect);
    if (charRect.mSucceeded) {
      aCharRect = charRect.mReply.mRect;
      if (aWritingMode) {
        *aWritingMode = charRect.GetWritingMode();
      }
      MOZ_LOG(gIMMLog, LogLevel::Debug,
        ("GetCharacterRectOfSelectedTextAt, Succeeded, aOffset=%u, "
         "aCharRect={ x: %ld, y: %ld, width: %ld, height: %ld }, "
         "charRect.GetWritingMode()=%s",
         aOffset, aCharRect.x, aCharRect.y, aCharRect.width, aCharRect.height,
         GetWritingModeName(charRect.GetWritingMode()).get()));
      return true;
    }
  }

  return GetCaretRect(aWindow, aCharRect, aWritingMode);
}

bool
IMMHandler::GetCaretRect(nsWindow* aWindow,
                         LayoutDeviceIntRect& aCaretRect,
                         WritingMode* aWritingMode)
{
  LayoutDeviceIntPoint point(0, 0);

  WidgetQueryContentEvent caretRect(true, eQueryCaretRect, aWindow);
  WidgetQueryContentEvent::Options options;
  options.mRelativeToInsertionPoint = true;
  caretRect.InitForQueryCaretRect(0, options);
  aWindow->InitEvent(caretRect, &point);
  DispatchEvent(aWindow, caretRect);
  if (!caretRect.mSucceeded) {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("GetCaretRect, FAILED, due to eQueryCaretRect failure"));
    return false;
  }
  aCaretRect = caretRect.mReply.mRect;
  if (aWritingMode) {
    *aWritingMode = caretRect.GetWritingMode();
  }
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("GetCaretRect, SUCCEEDED, "
     "aCaretRect={ x: %ld, y: %ld, width: %ld, height: %ld }, "
     "caretRect.GetWritingMode()=%s",
     aCaretRect.x, aCaretRect.y, aCaretRect.width, aCaretRect.height,
     GetWritingModeName(caretRect.GetWritingMode()).get()));
  return true;
}

bool
IMMHandler::SetIMERelatedWindowsPos(nsWindow* aWindow,
                                    const IMEContext& aContext)
{
  LayoutDeviceIntRect r;
  // Get first character rect of current a normal selected text or a composing
  // string.
  WritingMode writingMode;
  bool ret = GetCharacterRectOfSelectedTextAt(aWindow, 0, r, &writingMode);
  NS_ENSURE_TRUE(ret, false);
  nsWindow* toplevelWindow = aWindow->GetTopLevelWindow(false);
  LayoutDeviceIntRect firstSelectedCharRect;
  ResolveIMECaretPos(toplevelWindow, r, aWindow, firstSelectedCharRect);

  // Set native caret size/position to our caret. Some IMEs honor it. E.g.,
  // "Intelligent ABC" (Simplified Chinese) and "MS PinYin 3.0" (Simplified
  // Chinese) on XP.
  LayoutDeviceIntRect caretRect(firstSelectedCharRect);
  if (GetCaretRect(aWindow, r)) {
    ResolveIMECaretPos(toplevelWindow, r, aWindow, caretRect);
  } else {
    NS_WARNING("failed to get caret rect");
    caretRect.width = 1;
  }
  if (!mNativeCaretIsCreated) {
    mNativeCaretIsCreated = ::CreateCaret(aWindow->GetWindowHandle(), nullptr,
                                          caretRect.width, caretRect.height);
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("SetIMERelatedWindowsPos, mNativeCaretIsCreated=%s, "
       "width=%ld, height=%ld",
       GetBoolName(mNativeCaretIsCreated), caretRect.width, caretRect.height));
  }
  ::SetCaretPos(caretRect.x, caretRect.y);

  if (ShouldDrawCompositionStringOurselves()) {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("SetIMERelatedWindowsPos, Set candidate window"));

    // Get a rect of first character in current target in composition string.
    LayoutDeviceIntRect firstTargetCharRect, lastTargetCharRect;
    if (mIsComposing && !mCompositionString.IsEmpty()) {
      // If there are no targetted selection, we should use it's first character
      // rect instead.
      uint32_t offset, length;
      if (!GetTargetClauseRange(&offset, &length)) {
        MOZ_LOG(gIMMLog, LogLevel::Error,
          ("SetIMERelatedWindowsPos, FAILED, due to "
           "GetTargetClauseRange() failure"));
        return false;
      }
      ret = GetCharacterRectOfSelectedTextAt(aWindow,
                                             offset - mCompositionStart,
                                             firstTargetCharRect, &writingMode);
      NS_ENSURE_TRUE(ret, false);
      if (length) {
        ret = GetCharacterRectOfSelectedTextAt(aWindow,
                offset + length - 1 - mCompositionStart, lastTargetCharRect);
        NS_ENSURE_TRUE(ret, false);
      } else {
        lastTargetCharRect = firstTargetCharRect;
      }
    } else {
      // If there are no composition string, we should use a first character
      // rect.
      ret = GetCharacterRectOfSelectedTextAt(aWindow, 0,
                                             firstTargetCharRect, &writingMode);
      NS_ENSURE_TRUE(ret, false);
      lastTargetCharRect = firstTargetCharRect;
    }
    ResolveIMECaretPos(toplevelWindow, firstTargetCharRect,
                       aWindow, firstTargetCharRect);
    ResolveIMECaretPos(toplevelWindow, lastTargetCharRect,
                       aWindow, lastTargetCharRect);
    LayoutDeviceIntRect targetClauseRect;
    targetClauseRect.UnionRect(firstTargetCharRect, lastTargetCharRect);

    // Move the candidate window to proper position from the target clause as
    // far as possible.
    CANDIDATEFORM candForm;
    candForm.dwIndex = 0;
    if (!writingMode.IsVertical() || IsVerticalWritingSupported()) {
      candForm.dwStyle = CFS_EXCLUDE;
      // Candidate window shouldn't overlap the target clause in any writing
      // mode.
      candForm.rcArea.left = targetClauseRect.x;
      candForm.rcArea.right = targetClauseRect.XMost();
      candForm.rcArea.top = targetClauseRect.y;
      candForm.rcArea.bottom = targetClauseRect.YMost();
      if (!writingMode.IsVertical()) {
        // In horizontal layout, current point of interest should be top-left
        // of the first character.
        candForm.ptCurrentPos.x = firstTargetCharRect.x;
        candForm.ptCurrentPos.y = firstTargetCharRect.y;
      } else if (writingMode.IsVerticalRL()) {
        // In vertical layout (RL), candidate window should be positioned right
        // side of target clause.  However, we don't set vertical writing font
        // to the IME.  Therefore, the candidate window may be positioned
        // bottom-left of target clause rect with these information.
        candForm.ptCurrentPos.x = targetClauseRect.x;
        candForm.ptCurrentPos.y = targetClauseRect.y;
      } else {
        MOZ_ASSERT(writingMode.IsVerticalLR(), "Did we miss some causes?");
        // In vertical layout (LR), candidate window should be poisitioned left
        // side of target clause.  Although, we don't set vertical writing font
        // to the IME, the candidate window may be positioned bottom-right of
        // the target clause rect with these information.
        candForm.ptCurrentPos.x = targetClauseRect.XMost();
        candForm.ptCurrentPos.y = targetClauseRect.y;
      }
    } else {
      // If vertical writing is not supported by IME, let's set candidate
      // window position to the bottom-left of the target clause because
      // the position must be the safest position to prevent the candidate
      // window to overlap with the target clause.
      candForm.dwStyle = CFS_CANDIDATEPOS;
      candForm.ptCurrentPos.x = targetClauseRect.x;
      candForm.ptCurrentPos.y = targetClauseRect.YMost();
    }
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("SetIMERelatedWindowsPos, Calling ImmSetCandidateWindow()... "
       "ptCurrentPos={ x=%d, y=%d }, "
       "rcArea={ left=%d, top=%d, right=%d, bottom=%d }, "
       "writingMode=%s",
       candForm.ptCurrentPos.x, candForm.ptCurrentPos.y,
       candForm.rcArea.left, candForm.rcArea.top,
       candForm.rcArea.right, candForm.rcArea.bottom,
       GetWritingModeName(writingMode).get()));
    ::ImmSetCandidateWindow(aContext.get(), &candForm);
  } else {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("SetIMERelatedWindowsPos, Set composition window"));

    // Move the composition window to caret position (if selected some
    // characters, we should use first character rect of them).
    // And in this mode, IME adjusts the candidate window position
    // automatically. So, we don't need to set it.
    COMPOSITIONFORM compForm;
    compForm.dwStyle = CFS_POINT;
    compForm.ptCurrentPos.x =
      !writingMode.IsVerticalLR() ? firstSelectedCharRect.x :
                                    firstSelectedCharRect.XMost();
    compForm.ptCurrentPos.y = firstSelectedCharRect.y;
    ::ImmSetCompositionWindow(aContext.get(), &compForm);
  }

  return true;
}

void
IMMHandler::SetIMERelatedWindowsPosOnPlugin(nsWindow* aWindow,
                                            const IMEContext& aContext)
{
  WidgetQueryContentEvent editorRectEvent(true, eQueryEditorRect, aWindow);
  aWindow->InitEvent(editorRectEvent);
  DispatchEvent(aWindow, editorRectEvent);
  if (!editorRectEvent.mSucceeded) {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("SetIMERelatedWindowsPosOnPlugin, "
       "FAILED, due to eQueryEditorRect failure"));
    return;
  }

  // Clip the plugin rect by the client rect of the window because composition
  // window needs to be specified the position in the client area.
  nsWindow* toplevelWindow = aWindow->GetTopLevelWindow(false);
  LayoutDeviceIntRect pluginRectInScreen =
    editorRectEvent.mReply.mRect + toplevelWindow->WidgetToScreenOffset();
  LayoutDeviceIntRect winRectInScreen = aWindow->GetClientBounds();
  // composition window cannot be positioned on the edge of client area.
  winRectInScreen.width--;
  winRectInScreen.height--;
  LayoutDeviceIntRect clippedPluginRect;
  clippedPluginRect.x =
    std::min(std::max(pluginRectInScreen.x, winRectInScreen.x),
             winRectInScreen.XMost());
  clippedPluginRect.y =
    std::min(std::max(pluginRectInScreen.y, winRectInScreen.y),
             winRectInScreen.YMost());
  int32_t xMost = std::min(pluginRectInScreen.XMost(), winRectInScreen.XMost());
  int32_t yMost = std::min(pluginRectInScreen.YMost(), winRectInScreen.YMost());
  clippedPluginRect.width = std::max(0, xMost - clippedPluginRect.x);
  clippedPluginRect.height = std::max(0, yMost - clippedPluginRect.y);
  clippedPluginRect -= aWindow->WidgetToScreenOffset();

  // Cover the plugin with native caret.  This prevents IME's window and plugin
  // overlap.
  if (mNativeCaretIsCreated) {
    ::DestroyCaret();
  }
  mNativeCaretIsCreated =
    ::CreateCaret(aWindow->GetWindowHandle(), nullptr,
                  clippedPluginRect.width, clippedPluginRect.height);
  ::SetCaretPos(clippedPluginRect.x, clippedPluginRect.y);

  // Set the composition window to bottom-left of the clipped plugin.
  // As far as we know, there is no IME for RTL language.  Therefore, this code
  // must not need to take care of RTL environment.
  COMPOSITIONFORM compForm;
  compForm.dwStyle = CFS_POINT;
  compForm.ptCurrentPos.x = clippedPluginRect.BottomLeft().x;
  compForm.ptCurrentPos.y = clippedPluginRect.BottomLeft().y;
  if (!::ImmSetCompositionWindow(aContext.get(), &compForm)) {
    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("SetIMERelatedWindowsPosOnPlugin, "
       "FAILED, due to ::ImmSetCompositionWindow() failure"));
    return;
  }
}

void
IMMHandler::ResolveIMECaretPos(nsIWidget* aReferenceWidget,
                               LayoutDeviceIntRect& aCursorRect,
                               nsIWidget* aNewOriginWidget,
                               LayoutDeviceIntRect& aOutRect)
{
  aOutRect = aCursorRect;

  if (aReferenceWidget == aNewOriginWidget)
    return;

  if (aReferenceWidget)
    aOutRect.MoveBy(aReferenceWidget->WidgetToScreenOffset());

  if (aNewOriginWidget)
    aOutRect.MoveBy(-aNewOriginWidget->WidgetToScreenOffset());
}

static void
SetHorizontalFontToLogFont(const nsAString& aFontFace,
                           LOGFONTW& aLogFont)
{
  aLogFont.lfEscapement = aLogFont.lfOrientation = 0;
  if (NS_WARN_IF(aFontFace.Length() > LF_FACESIZE - 1)) {
    memcpy(aLogFont.lfFaceName, L"System", sizeof(L"System"));
    return;
  }
  memcpy(aLogFont.lfFaceName, aFontFace.BeginReading(),
         aFontFace.Length() * sizeof(wchar_t));
  aLogFont.lfFaceName[aFontFace.Length()] = 0;
}

static void
SetVerticalFontToLogFont(const nsAString& aFontFace,
                         LOGFONTW& aLogFont)
{
  aLogFont.lfEscapement = aLogFont.lfOrientation = 2700;
  if (NS_WARN_IF(aFontFace.Length() > LF_FACESIZE - 2)) {
    memcpy(aLogFont.lfFaceName, L"@System", sizeof(L"@System"));
    return;
  }
  aLogFont.lfFaceName[0] = '@';
  memcpy(&aLogFont.lfFaceName[1], aFontFace.BeginReading(),
         aFontFace.Length() * sizeof(wchar_t));
  aLogFont.lfFaceName[aFontFace.Length() + 1] = 0;
}

void
IMMHandler::AdjustCompositionFont(nsWindow* aWindow,
                                  const IMEContext& aContext,
                                  const WritingMode& aWritingMode,
                                  bool aForceUpdate)
{
  // An instance of IMMHandler is destroyed when active IME is changed.
  // Therefore, we need to store the information which are set to the IM
  // context to static variables since IM context is never recreated.
  static bool sCompositionFontsInitialized = false;
  static nsString sCompositionFont =
    Preferences::GetString("intl.imm.composition_font");

  // If composition font is customized by pref, we need to modify the
  // composition font of the IME context at first time even if the writing mode
  // is horizontal.
  bool setCompositionFontForcibly = aForceUpdate ||
    (!sCompositionFontsInitialized && !sCompositionFont.IsEmpty());

  static WritingMode sCurrentWritingMode;
  static nsString sCurrentIMEName;
  if (!setCompositionFontForcibly &&
      sWritingModeOfCompositionFont == aWritingMode &&
      sCurrentIMEName == sIMEName) {
    // Nothing to do if writing mode isn't being changed.
    return;
  }

  // Decide composition fonts for both horizontal writing mode and vertical
  // writing mode.  If the font isn't specified by the pref, use default
  // font which is already set to the IM context.  And also in vertical writing
  // mode, insert '@' to the start of the font.
  if (!sCompositionFontsInitialized) {
    sCompositionFontsInitialized = true;
    // sCompositionFontH must not start with '@' and its length is less than
    // LF_FACESIZE since it needs to end with null terminating character.
    if (sCompositionFont.IsEmpty() ||
        sCompositionFont.Length() > LF_FACESIZE - 1 ||
        sCompositionFont[0] == '@') {
      LOGFONTW defaultLogFont;
      if (NS_WARN_IF(!::ImmGetCompositionFont(aContext.get(),
                                              &defaultLogFont))) {
        MOZ_LOG(gIMMLog, LogLevel::Error,
          ("AdjustCompositionFont, ::ImmGetCompositionFont() failed"));
        sCompositionFont.AssignLiteral("System");
      } else {
        // The font face is typically, "System".
        sCompositionFont.Assign(defaultLogFont.lfFaceName);
      }
    }

    MOZ_LOG(gIMMLog, LogLevel::Info,
      ("AdjustCompositionFont, sCompositionFont=\"%s\" is initialized",
       NS_ConvertUTF16toUTF8(sCompositionFont).get()));
  }

  static nsString sCompositionFontForJapanist2003;
  if (IsJapanist2003Active() && sCompositionFontForJapanist2003.IsEmpty()) {
    const char* kCompositionFontForJapanist2003 =
      "intl.imm.composition_font.japanist_2003";
    sCompositionFontForJapanist2003 =
      Preferences::GetString(kCompositionFontForJapanist2003);
    // If the font name is not specified properly, let's use
    // "MS PGothic" instead.
    if (sCompositionFontForJapanist2003.IsEmpty() ||
        sCompositionFontForJapanist2003.Length() > LF_FACESIZE - 2 ||
        sCompositionFontForJapanist2003[0] == '@') {
      sCompositionFontForJapanist2003.AssignLiteral("MS PGothic");
    }
  }

  sWritingModeOfCompositionFont = aWritingMode;
  sCurrentIMEName = sIMEName;

  LOGFONTW logFont;
  memset(&logFont, 0, sizeof(logFont));
  if (!::ImmGetCompositionFont(aContext.get(), &logFont)) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("AdjustCompositionFont, ::ImmGetCompositionFont() failed"));
    logFont.lfFaceName[0] = 0;
  }
  // Need to reset some information which should be recomputed with new font.
  logFont.lfWidth = 0;
  logFont.lfWeight = FW_DONTCARE;
  logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
  logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  logFont.lfPitchAndFamily = DEFAULT_PITCH;

  if (!aWindow->PluginHasFocus() &&
    aWritingMode.IsVertical() && IsVerticalWritingSupported()) {
    SetVerticalFontToLogFont(
      IsJapanist2003Active() ? sCompositionFontForJapanist2003 :
                               sCompositionFont, logFont);
  } else {
    SetHorizontalFontToLogFont(
      IsJapanist2003Active() ? sCompositionFontForJapanist2003 :
                               sCompositionFont, logFont);
  }
  MOZ_LOG(gIMMLog, LogLevel::Warning,
    ("AdjustCompositionFont, calling ::ImmSetCompositionFont(\"%s\")",
     NS_ConvertUTF16toUTF8(nsDependentString(logFont.lfFaceName)).get()));
  ::ImmSetCompositionFontW(aContext.get(), &logFont);
}

// static
nsresult
IMMHandler::OnMouseButtonEvent(nsWindow* aWindow,
                               const IMENotification& aIMENotification)
{
  // We don't need to create the instance of the handler here.
  if (!gIMMHandler) {
    return NS_OK;
  }

  if (!sWM_MSIME_MOUSE || !IsComposingOnOurEditor() ||
      !ShouldDrawCompositionStringOurselves()) {
    return NS_OK;
  }

  // We need to handle only mousedown event.
  if (aIMENotification.mMouseButtonEventData.mEventMessage != eMouseDown) {
    return NS_OK;
  }

  // If the character under the cursor is not in the composition string,
  // we don't need to notify IME of it.
  uint32_t compositionStart = gIMMHandler->mCompositionStart;
  uint32_t compositionEnd =
    compositionStart + gIMMHandler->mCompositionString.Length();
  if (aIMENotification.mMouseButtonEventData.mOffset < compositionStart ||
      aIMENotification.mMouseButtonEventData.mOffset >= compositionEnd) {
    return NS_OK;
  }

  BYTE button;
  switch (aIMENotification.mMouseButtonEventData.mButton) {
    case WidgetMouseEventBase::eLeftButton:
      button = IMEMOUSE_LDOWN;
      break;
    case WidgetMouseEventBase::eMiddleButton:
      button = IMEMOUSE_MDOWN;
      break;
    case WidgetMouseEventBase::eRightButton:
      button = IMEMOUSE_RDOWN;
      break;
    default:
      return NS_OK;
  }

  // calcurate positioning and offset
  // char :            JCH1|JCH2|JCH3
  // offset:           0011 1122 2233
  // positioning:      2301 2301 2301
  nsIntPoint cursorPos =
    aIMENotification.mMouseButtonEventData.mCursorPos.AsIntPoint();
  nsIntRect charRect =
    aIMENotification.mMouseButtonEventData.mCharRect.AsIntRect();
  int32_t cursorXInChar = cursorPos.x - charRect.x;
  // The event might hit to zero-width character, see bug 694913.
  // The reason might be:
  // * There are some zero-width characters are actually.
  // * font-size is specified zero.
  // But nobody reproduced this bug actually...
  // We should assume that user clicked on right most of the zero-width
  // character in such case.
  int positioning = 1;
  if (charRect.width > 0) {
    positioning = cursorXInChar * 4 / charRect.width;
    positioning = (positioning + 2) % 4;
  }

  int offset =
    aIMENotification.mMouseButtonEventData.mOffset - compositionStart;
  if (positioning < 2) {
    offset++;
  }

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnMouseButtonEvent, x,y=%ld,%ld, offset=%ld, positioning=%ld",
     cursorPos.x, cursorPos.y, offset, positioning));

  // send MS_MSIME_MOUSE message to default IME window.
  HWND imeWnd = ::ImmGetDefaultIMEWnd(aWindow->GetWindowHandle());
  IMEContext context(aWindow);
  if (::SendMessageW(imeWnd, sWM_MSIME_MOUSE,
                     MAKELONG(MAKEWORD(button, positioning), offset),
                     (LPARAM) context.get()) == 1) {
    return NS_SUCCESS_EVENT_CONSUMED;
  }
  return NS_OK;
}

// static
bool
IMMHandler::OnKeyDownEvent(nsWindow* aWindow,
                           WPARAM wParam,
                           LPARAM lParam,
                           MSGResult& aResult)
{
  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("OnKeyDownEvent, hWnd=%08x, wParam=%08x, lParam=%08x",
     aWindow->GetWindowHandle(), wParam, lParam));
  aResult.mConsumed = false;
  switch (wParam) {
    case VK_TAB:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_END:
    case VK_HOME:
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_RETURN:
      // If IME didn't process the key message (the virtual key code wasn't
      // converted to VK_PROCESSKEY), and the virtual key code event causes
      // moving caret or editing text with keeping composing state, we should
      // cancel the composition here because we cannot support moving
      // composition string with DOM events (IE also cancels the composition
      // in same cases).  Then, this event will be dispatched.
      if (IsComposingOnOurEditor()) {
        // NOTE: We don't need to cancel the composition on another window.
        CancelComposition(aWindow, false);
      }
      return false;
    default:
      return false;
  }
}

// static
void
IMMHandler::SetCandidateWindow(nsWindow* aWindow, CANDIDATEFORM* aForm)
{
  // Hack for ATOK 2011 - 2016 (Japanese IME).  They refer native caret
  // position at deciding candidate window position.  Note that we cannot
  // check active IME since TIPs are wrapped and hidden by CUAS.
  if (aWindow->PluginHasFocus()) {
    // We cannot retrieve proper character height from plugin.  Therefore,
    // we should assume that the caret height is always 20px since if less than
    // this height, candidate window may overlap with composition string when
    // there is no enough space under composition string to show candidate
    // window.
    static const int32_t kCaretHeight = 20;
    if (sNativeCaretIsCreatedForPlugin) {
      ::DestroyCaret();
    }
    sNativeCaretIsCreatedForPlugin =
      ::CreateCaret(aWindow->GetWindowHandle(), nullptr, 0, kCaretHeight);
    if (sNativeCaretIsCreatedForPlugin) {
      LayoutDeviceIntPoint caretPosition(aForm->ptCurrentPos.x,
                                         aForm->ptCurrentPos.y - kCaretHeight);
      nsWindow* toplevelWindow = aWindow->GetTopLevelWindow(false);
      if (toplevelWindow && toplevelWindow != aWindow) {
        caretPosition += toplevelWindow->WidgetToScreenOffset();
        caretPosition -= aWindow->WidgetToScreenOffset();
      }
      ::SetCaretPos(caretPosition.x, caretPosition.y);
    }
  }
  IMEContext context(aWindow);
  ::ImmSetCandidateWindow(context.get(), aForm);
}

// staitc
void
IMMHandler::DefaultProcOfPluginEvent(nsWindow* aWindow, const NPEvent* aEvent)
{
  switch (aEvent->event) {
    case WM_IME_STARTCOMPOSITION:
      EnsureHandlerInstance();
      gIMMHandler->OnIMEStartCompositionOnPlugin(aWindow, aEvent->wParam,
                                                 aEvent->lParam);
      break;

    case WM_IME_COMPOSITION:
      if (gIMMHandler) {
        gIMMHandler->OnIMECompositionOnPlugin(aWindow, aEvent->wParam,
                                              aEvent->lParam);
      }
      break;

    case WM_IME_ENDCOMPOSITION:
      if (gIMMHandler) {
        gIMMHandler->OnIMEEndCompositionOnPlugin(aWindow, aEvent->wParam,
                                                 aEvent->lParam);
      }
      break;
  }
}

/******************************************************************************
 * IMMHandler::Selection
 ******************************************************************************/

bool
IMMHandler::Selection::IsValid() const
{
  if (!mIsValid || NS_WARN_IF(mOffset == UINT32_MAX)) {
    return false;
  }
  CheckedInt<uint32_t> endOffset =
    CheckedInt<uint32_t>(mOffset) + Length();
  return endOffset.isValid();
}

bool
IMMHandler::Selection::Update(const IMENotification& aIMENotification)
{
  mOffset = aIMENotification.mSelectionChangeData.mOffset;
  mString = aIMENotification.mSelectionChangeData.String();
  mWritingMode = aIMENotification.mSelectionChangeData.GetWritingMode();
  mIsValid = true;

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("Selection::Update, aIMENotification={ mSelectionChangeData={ "
     "mOffset=%u, mLength=%u, GetWritingMode()=%s } }",
     mOffset, mString.Length(), GetWritingModeName(mWritingMode).get()));

  if (!IsValid()) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("Selection::Update, FAILED, due to invalid range"));
    Clear();
    return false;
  }
  return true;
}

bool
IMMHandler::Selection::Init(nsWindow* aWindow)
{
  Clear();

  WidgetQueryContentEvent selection(true, eQuerySelectedText, aWindow);
  LayoutDeviceIntPoint point(0, 0);
  aWindow->InitEvent(selection, &point);
  DispatchEvent(aWindow, selection);
  if (NS_WARN_IF(!selection.mSucceeded)) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("Selection::Init, FAILED, due to eQuerySelectedText failure"));
    return false;
  }
  // If the window is destroyed during querying selected text, we shouldn't
  // do anymore.
  if (aWindow->Destroyed()) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("Selection::Init, FAILED, due to the widget destroyed"));
    return false;
  }

  mOffset = selection.mReply.mOffset;
  mString = selection.mReply.mString;
  mWritingMode = selection.GetWritingMode();
  mIsValid = true;

  MOZ_LOG(gIMMLog, LogLevel::Info,
    ("Selection::Init, selection={ mReply={ mOffset=%u, "
     "mString.Length()=%u, mWritingMode=%s } }",
     mOffset, mString.Length(), GetWritingModeName(mWritingMode).get()));

  if (!IsValid()) {
    MOZ_LOG(gIMMLog, LogLevel::Error,
      ("Selection::Init, FAILED, due to invalid range"));
    Clear();
    return false;
  }
  return true;
}

bool
IMMHandler::Selection::EnsureValidSelection(nsWindow* aWindow)
{
  if (IsValid()) {
    return true;
  }
  return Init(aWindow);
}

} // namespace widget
} // namespace mozilla
