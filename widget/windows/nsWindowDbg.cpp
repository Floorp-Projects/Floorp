/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsWindowDbg - Debug related utilities for nsWindow.
 */

#include "nsWindowDbg.h"
#include "nsToolkit.h"
#include "WinPointerEvents.h"
#include "nsWindowLoggedMessages.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "nsWindow.h"
#include "GeckoProfiler.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"

#include <winuser.h>
#include <dbt.h>
#include <imm.h>
#include <tpcshrd.h>

#include <unordered_set>

using namespace mozilla;
using namespace mozilla::widget;
extern mozilla::LazyLogModule gWindowsLog;
static mozilla::LazyLogModule gWindowsEventLog("WindowsEvent");

#if defined(POPUP_ROLLUP_DEBUG_OUTPUT)
MSGFEventMsgInfo gMSGFEvents[] = {
    "MSGF_DIALOGBOX", 0,    "MSGF_MESSAGEBOX", 1, "MSGF_MENU", 2,
    "MSGF_SCROLLBAR", 5,    "MSGF_NEXTWINDOW", 6, "MSGF_MAX",  8,
    "MSGF_USER",      4096, nullptr,           0};
#endif

static long gEventCounter = 0;
static UINT gLastEventMsg = 0;

namespace geckoprofiler::markers {

struct WindowProcMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("WindowProc");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const ProfilerString8View& aMsgLoopName,
                                   UINT aMsg, WPARAM aWParam, LPARAM aLParam) {
    aWriter.StringProperty("messageLoop", aMsgLoopName);
    aWriter.IntProperty("uMsg", aMsg);
    const char* name;
    if (aMsg < WM_USER) {
      const auto eventMsgInfo = mozilla::widget::gAllEvents.find(aMsg);
      if (eventMsgInfo != mozilla::widget::gAllEvents.end()) {
        name = eventMsgInfo->second.mStr;
      } else {
        name = "ui message";
      }
    } else if (aMsg >= WM_USER && aMsg < WM_APP) {
      name = "WM_USER message";
    } else if (aMsg >= WM_APP && aMsg < 0xC000) {
      name = "WM_APP message";
    } else if (aMsg >= 0xC000 && aMsg < 0x10000) {
      name = "registered windows message";
    } else {
      name = "system message";
    }
    aWriter.StringProperty("name", MakeStringSpan(name));

    if (aWParam) {
      aWriter.IntProperty("wParam", aWParam);
    }
    if (aLParam) {
      aWriter.IntProperty("lParam", aLParam);
    }
  }

  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.AddKeyFormat("uMsg", MS::Format::Integer);
    schema.SetChartLabel(
        "{marker.data.messageLoop} | {marker.data.name} ({marker.data.uMsg})");
    schema.SetTableLabel(
        "{marker.name} - {marker.data.messageLoop} - {marker.data.name} "
        "({marker.data.uMsg})");
    schema.SetTooltipLabel(
        "{marker.data.messageLoop} - {marker.name} - {marker.data.name}");
    schema.AddKeyFormat("wParam", MS::Format::Integer);
    schema.AddKeyFormat("lParam", MS::Format::Integer);
    return schema;
  }
};

}  // namespace geckoprofiler::markers

namespace mozilla::widget {

AutoProfilerMessageMarker::AutoProfilerMessageMarker(
    Span<const char> aMsgLoopName, HWND hWnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
    : mMsgLoopName(aMsgLoopName), mMsg(msg), mWParam(wParam), mLParam(lParam) {
  if (profiler_thread_is_being_profiled_for_markers()) {
    mOptions.emplace(MarkerOptions(MarkerTiming::IntervalStart()));
    nsWindow* win = WinUtils::GetNSWindowPtr(hWnd);
    if (win) {
      nsIWidgetListener* wl = win->GetWidgetListener();
      if (wl) {
        PresShell* presShell = wl->GetPresShell();
        if (presShell) {
          dom::Document* doc = presShell->GetDocument();
          if (doc) {
            mOptions->Set(MarkerInnerWindowId(doc->InnerWindowID()));
          }
        }
      }
    }
  }
}

AutoProfilerMessageMarker::~AutoProfilerMessageMarker() {
  if (!profiler_thread_is_being_profiled_for_markers()) {
    return;
  }

  if (mOptions) {
    mOptions->TimingRef().SetIntervalEnd();
  } else {
    mOptions.emplace(MarkerOptions(MarkerTiming::IntervalEnd()));
  }
  profiler_add_marker(
      "WindowProc", ::mozilla::baseprofiler::category::OTHER,
      std::move(*mOptions), geckoprofiler::markers::WindowProcMarker{},
      ProfilerString8View::WrapNullTerminatedString(mMsgLoopName.data()), mMsg,
      mWParam, mLParam);
}

// Using an unordered_set so we can initialize this with nice syntax instead of
// having to add them one at a time to a mozilla::HashSet.
std::unordered_set<UINT> gEventsToLogOriginalParams = {
    WM_WINDOWPOSCHANGING,  // (dummy comments for clang-format)
    WM_SIZING,             //
    WM_STYLECHANGING,
    WM_GETTEXT,
    WM_GETMINMAXINFO,
    WM_MEASUREITEM,
    WM_NCCALCSIZE,
};

// If you add an event here, you must add cases for these to
// MakeMessageSpecificData() and AppendFriendlyMessageSpecificData()
// in nsWindowLoggedMessages.cpp.
std::unordered_set<UINT> gEventsToRecordInAboutPage = {
    WM_WINDOWPOSCHANGING,  // (dummy comments for clang-format)
    WM_WINDOWPOSCHANGED,   //
    WM_SIZING,
    WM_SIZE,
    WM_DPICHANGED,
    WM_SETTINGCHANGE,
    WM_NCCALCSIZE,
    WM_MOVE,
    WM_MOVING,
    WM_GETMINMAXINFO,
};

NativeEventLogger::NativeEventLogger(Span<const char> aMsgLoopName, HWND hwnd,
                                     UINT msg, WPARAM wParam, LPARAM lParam)
    : mProfilerMarker(aMsgLoopName, hwnd, msg, wParam, lParam),
      mMsgLoopName(aMsgLoopName.data()),
      mHwnd(hwnd),
      mMsg(msg),
      mWParam(wParam),
      mLParam(lParam),
      mResult(mozilla::Nothing()),
      mShouldLogPostCall(false) {
  if (NativeEventLoggerInternal()) {
    // this event was logged, so reserve this counter number for the post-call
    mEventCounter = mozilla::Some(gEventCounter);
    ++gEventCounter;
  }
}

NativeEventLogger::~NativeEventLogger() {
  // If mResult is Nothing, perhaps an exception was thrown or something
  // before SetResult() was supposed to be called.
  if (mResult.isSome()) {
    if (NativeEventLoggerInternal() && mEventCounter.isNothing()) {
      // We didn't reserve a counter in the pre-call, so reserve it here.
      ++gEventCounter;
    }
  }
  if (mMsg == WM_DESTROY) {
    // Remove any logged messages for this window.
    WindowClosed(mHwnd);
  }
}

void EventMsgInfo::LogParameters(nsCString& str, WPARAM wParam, LPARAM lParam,
                                 bool isPreCall) {
  if (mParamInfoFn) {
    str = mParamInfoFn(wParam, lParam, isPreCall);
  } else {
    if (mWParamInfoFn) {
      mWParamInfoFn(str, wParam, mWParamName, isPreCall);
    }
    if (mLParamInfoFn) {
      if (mWParamInfoFn) {
        str.AppendASCII(" ");
      }
      mLParamInfoFn(str, lParam, mLParamName, isPreCall);
    }
  }
}

nsAutoCString DefaultParamInfo(uint64_t wParam, uint64_t lParam,
                               bool /* isPreCall */) {
  nsAutoCString result;
  result.AppendPrintf("wParam=0x%08llX lParam=0x%08llX", wParam, lParam);
  return result;
}

void AppendEnumValueInfo(
    nsCString& str, uint64_t value,
    const std::unordered_map<uint64_t, const char*>& valuesAndNames,
    const char* name) {
  if (name != nullptr) {
    str.AppendPrintf("%s=", name);
  }
  auto entry = valuesAndNames.find(value);
  if (entry == valuesAndNames.end()) {
    str.AppendPrintf("Unknown (0x%08llX)", value);
  } else {
    str.AppendASCII(entry->second);
  }
}

bool AppendFlagsInfo(nsCString& str, uint64_t flags,
                     const nsTArray<EnumValueAndName>& flagsAndNames,
                     const char* name) {
  if (name != nullptr) {
    str.AppendPrintf("%s=", name);
  }
  bool firstAppend = true;
  for (const EnumValueAndName& flagAndName : flagsAndNames) {
    if (MOZ_UNLIKELY(flagAndName.mFlag == 0)) {
      // Special case - only want to write this if nothing else was set.
      // For this to make sense, 0 values should come at the end of
      // flagsAndNames.
      if (flags == 0 && firstAppend) {
        firstAppend = false;
        str.AppendASCII(flagAndName.mName);
      }
    } else if ((flags & flagAndName.mFlag) == flagAndName.mFlag) {
      if (MOZ_LIKELY(!firstAppend)) {
        str.Append('|');
      }
      firstAppend = false;
      str.AppendASCII(flagAndName.mName);
      flags = flags & ~flagAndName.mFlag;
    }
  }
  if (flags != 0) {
    if (MOZ_LIKELY(!firstAppend)) {
      str.Append('|');
    }
    firstAppend = false;
    str.AppendPrintf("Unknown (0x%08llX)", flags);
  }
  return !firstAppend;
}

// if mResult is not set, this is used to log the parameters passed in the
// message, otherwise we are logging the parameters after we have handled the
// message. This is useful for events where we might change the parameters while
// handling the message (for example WM_GETTEXT and WM_NCCALCSIZE)
// Returns whether this message was logged, so we need to reserve a
// counter number for it.
bool NativeEventLogger::NativeEventLoggerInternal() {
  mozilla::LogLevel const targetLogLevel = [&] {
    // These messages often take up more than 90% of logs if not filtered out.
    if (mMsg == WM_SETCURSOR || mMsg == WM_MOUSEMOVE || mMsg == WM_NCHITTEST) {
      return LogLevel::Verbose;
    }
    if (gLastEventMsg == mMsg) {
      return LogLevel::Debug;
    }
    return LogLevel::Info;
  }();

  bool isPreCall = mResult.isNothing();
  if (isPreCall || mShouldLogPostCall) {
    bool recordInAboutPage = gEventsToRecordInAboutPage.find(mMsg) !=
                             gEventsToRecordInAboutPage.end();
    bool writeToWindowsLog;
    if (isPreCall) {
      writeToWindowsLog = MOZ_LOG_TEST(gWindowsEventLog, targetLogLevel);
      bool shouldLogAtAll = recordInAboutPage || writeToWindowsLog;
      // Since calling mParamInfoFn() allocates a string, only go down this code
      // path if we're going to log this message to reduce allocations.
      if (!shouldLogAtAll) {
        return false;
      }
      mShouldLogPostCall = true;
      bool shouldLogPreCall = gEventsToLogOriginalParams.find(mMsg) !=
                              gEventsToLogOriginalParams.end();
      if (!shouldLogPreCall) {
        // Pre-call and we don't want to log both, so skip this one.
        return false;
      }
    } else {
      writeToWindowsLog = true;
    }
    if (recordInAboutPage) {
      LogWindowMessage(mHwnd, mMsg, isPreCall,
                       mEventCounter.valueOr(gEventCounter), mWParam, mLParam,
                       mResult, mRetValue);
    }
    gLastEventMsg = mMsg;
    if (writeToWindowsLog) {
      const auto& eventMsgInfo = gAllEvents.find(mMsg);
      const char* msgText = eventMsgInfo != gAllEvents.end()
                                ? eventMsgInfo->second.mStr
                                : nullptr;
      nsAutoCString paramInfo;
      if (eventMsgInfo != gAllEvents.end()) {
        eventMsgInfo->second.LogParameters(paramInfo, mWParam, mLParam,
                                           isPreCall);
      } else {
        paramInfo = DefaultParamInfo(mWParam, mLParam, isPreCall);
      }
      const char* resultMsg = mResult.isSome()
                                  ? (mResult.value() ? "true" : "false")
                                  : "initial call";
      nsAutoCString logMessage;
      logMessage.AppendPrintf(
          "%s | %6ld %08" PRIX64 " - 0x%04X %s%s%s: 0x%08" PRIX64 " (%s)\n",
          mMsgLoopName, mEventCounter.valueOr(gEventCounter),
          reinterpret_cast<uint64_t>(mHwnd), mMsg,
          msgText ? msgText : "Unknown", paramInfo.IsEmpty() ? "" : " ",
          paramInfo.get(),
          mResult.isSome() ? static_cast<uint64_t>(mRetValue) : 0, resultMsg);
      const char* logMessageData = logMessage.Data();
      MOZ_LOG(gWindowsEventLog, targetLogLevel, ("%s", logMessageData));
    }
    return true;
  }
  return false;
}

void TrueFalseParamInfo(nsCString& result, uint64_t value, const char* name,
                        bool /* isPreCall */) {
  result.AppendPrintf("%s=%s", name, value == TRUE ? "TRUE" : "FALSE");
}

void TrueFalseLowOrderWordParamInfo(nsCString& result, uint64_t value,
                                    const char* name, bool /* isPreCall */) {
  result.AppendPrintf("%s=%s", name, LOWORD(value) == TRUE ? "TRUE" : "FALSE");
}

void HexParamInfo(nsCString& result, uint64_t value, const char* name,
                  bool /* isPreCall */) {
  result.AppendPrintf("%s=0x%08llX", name, value);
}

void IntParamInfo(nsCString& result, uint64_t value, const char* name,
                  bool /* isPreCall */) {
  result.AppendPrintf("%s=%lld", name, value);
}

void RectParamInfo(nsCString& str, uint64_t value, const char* name,
                   bool /* isPreCall */) {
  LPRECT rect = reinterpret_cast<LPRECT>(value);
  if (rect == nullptr) {
    str.AppendPrintf("NULL rect?");
    return;
  }
  if (name != nullptr) {
    str.AppendPrintf("%s ", name);
  }
  str.AppendPrintf("left=%ld top=%ld right=%ld bottom=%ld", rect->left,
                   rect->top, rect->right, rect->bottom);
}

#define VALANDNAME_ENTRY(_msg) \
  { _msg, #_msg }

void CreateStructParamInfo(nsCString& str, uint64_t value, const char* name,
                           bool /* isPreCall */) {
  CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(value);
  if (createStruct == nullptr) {
    str.AppendASCII("NULL createStruct?");
    return;
  }
  str.AppendPrintf(
      "%s: hInstance=%p hMenu=%p hwndParent=%p lpszName=%S lpszClass=%S x=%d "
      "y=%d cx=%d cy=%d",
      name, createStruct->hInstance, createStruct->hMenu,
      createStruct->hwndParent, createStruct->lpszName, createStruct->lpszClass,
      createStruct->x, createStruct->y, createStruct->cx, createStruct->cy);
  str.AppendASCII(" ");
  const static nsTArray<EnumValueAndName> windowStyles = {
      // these combinations of other flags need to come first
      VALANDNAME_ENTRY(WS_OVERLAPPEDWINDOW), VALANDNAME_ENTRY(WS_POPUPWINDOW),
      VALANDNAME_ENTRY(WS_CAPTION),
      // regular flags
      VALANDNAME_ENTRY(WS_POPUP), VALANDNAME_ENTRY(WS_CHILD),
      VALANDNAME_ENTRY(WS_MINIMIZE), VALANDNAME_ENTRY(WS_VISIBLE),
      VALANDNAME_ENTRY(WS_DISABLED), VALANDNAME_ENTRY(WS_CLIPSIBLINGS),
      VALANDNAME_ENTRY(WS_CLIPCHILDREN), VALANDNAME_ENTRY(WS_MAXIMIZE),
      VALANDNAME_ENTRY(WS_BORDER), VALANDNAME_ENTRY(WS_DLGFRAME),
      VALANDNAME_ENTRY(WS_VSCROLL), VALANDNAME_ENTRY(WS_HSCROLL),
      VALANDNAME_ENTRY(WS_SYSMENU), VALANDNAME_ENTRY(WS_THICKFRAME),
      VALANDNAME_ENTRY(WS_GROUP), VALANDNAME_ENTRY(WS_TABSTOP),
      // zero value needs to come last
      VALANDNAME_ENTRY(WS_OVERLAPPED)};
  AppendFlagsInfo(str, createStruct->style, windowStyles, "style");
  str.AppendASCII(" ");
  const nsTArray<EnumValueAndName> extendedWindowStyles = {
      // these combinations of other flags need to come first
      VALANDNAME_ENTRY(WS_EX_OVERLAPPEDWINDOW),
      VALANDNAME_ENTRY(WS_EX_PALETTEWINDOW),
      // regular flags
      VALANDNAME_ENTRY(WS_EX_DLGMODALFRAME),
      VALANDNAME_ENTRY(WS_EX_NOPARENTNOTIFY),
      VALANDNAME_ENTRY(WS_EX_TOPMOST),
      VALANDNAME_ENTRY(WS_EX_ACCEPTFILES),
      VALANDNAME_ENTRY(WS_EX_TRANSPARENT),
      VALANDNAME_ENTRY(WS_EX_MDICHILD),
      VALANDNAME_ENTRY(WS_EX_TOOLWINDOW),
      VALANDNAME_ENTRY(WS_EX_WINDOWEDGE),
      VALANDNAME_ENTRY(WS_EX_CLIENTEDGE),
      VALANDNAME_ENTRY(WS_EX_CONTEXTHELP),
      VALANDNAME_ENTRY(WS_EX_RIGHT),
      VALANDNAME_ENTRY(WS_EX_LEFT),
      VALANDNAME_ENTRY(WS_EX_RTLREADING),
      VALANDNAME_ENTRY(WS_EX_LTRREADING),
      VALANDNAME_ENTRY(WS_EX_LEFTSCROLLBAR),
      VALANDNAME_ENTRY(WS_EX_RIGHTSCROLLBAR),
      VALANDNAME_ENTRY(WS_EX_CONTROLPARENT),
      VALANDNAME_ENTRY(WS_EX_STATICEDGE),
      VALANDNAME_ENTRY(WS_EX_APPWINDOW),
      VALANDNAME_ENTRY(WS_EX_LAYERED),
      VALANDNAME_ENTRY(WS_EX_NOINHERITLAYOUT),
      VALANDNAME_ENTRY(WS_EX_LAYOUTRTL),
      VALANDNAME_ENTRY(WS_EX_NOACTIVATE),
      VALANDNAME_ENTRY(WS_EX_COMPOSITED),
      VALANDNAME_ENTRY(WS_EX_NOREDIRECTIONBITMAP),
  };
  AppendFlagsInfo(str, createStruct->dwExStyle, extendedWindowStyles,
                  "dwExStyle");
}

void XLowWordYHighWordParamInfo(nsCString& str, uint64_t value,
                                const char* name, bool /* isPreCall */) {
  str.AppendPrintf("%s: x=%d y=%d", name, static_cast<int>(LOWORD(value)),
                   static_cast<int>(HIWORD(value)));
}

void PointParamInfo(nsCString& str, uint64_t value, const char* name,
                    bool /* isPreCall */) {
  str.AppendPrintf("%s: x=%d y=%d", name, static_cast<int>(GET_X_LPARAM(value)),
                   static_cast<int>(GET_Y_LPARAM(value)));
}

void PointExplicitParamInfo(nsCString& str, POINT point, const char* name) {
  str.AppendPrintf("%s: x=%ld y=%ld", name, point.x, point.y);
}

void PointsParamInfo(nsCString& str, uint64_t value, const char* name,
                     bool /* isPreCall */) {
  PPOINTS points = reinterpret_cast<PPOINTS>(&value);
  str.AppendPrintf("%s: x=%d y=%d", name, points->x, points->y);
}

void VirtualKeyParamInfo(nsCString& result, uint64_t param, const char* name,
                         bool /* isPreCall */) {
  const static std::unordered_map<uint64_t, const char*> virtualKeys{
      VALANDNAME_ENTRY(VK_LBUTTON),
      VALANDNAME_ENTRY(VK_RBUTTON),
      VALANDNAME_ENTRY(VK_CANCEL),
      VALANDNAME_ENTRY(VK_MBUTTON),
      VALANDNAME_ENTRY(VK_XBUTTON1),
      VALANDNAME_ENTRY(VK_XBUTTON2),
      VALANDNAME_ENTRY(VK_BACK),
      VALANDNAME_ENTRY(VK_TAB),
      VALANDNAME_ENTRY(VK_CLEAR),
      VALANDNAME_ENTRY(VK_RETURN),
      VALANDNAME_ENTRY(VK_SHIFT),
      VALANDNAME_ENTRY(VK_CONTROL),
      VALANDNAME_ENTRY(VK_MENU),
      VALANDNAME_ENTRY(VK_PAUSE),
      VALANDNAME_ENTRY(VK_CAPITAL),
      VALANDNAME_ENTRY(VK_KANA),
      VALANDNAME_ENTRY(VK_HANGUL),
#ifdef VK_IME_ON
      VALANDNAME_ENTRY(VK_IME_ON),
#endif
      VALANDNAME_ENTRY(VK_JUNJA),
      VALANDNAME_ENTRY(VK_FINAL),
      VALANDNAME_ENTRY(VK_HANJA),
      VALANDNAME_ENTRY(VK_KANJI),
#ifdef VK_IME_OFF
      VALANDNAME_ENTRY(VK_IME_OFF),
#endif
      VALANDNAME_ENTRY(VK_ESCAPE),
      VALANDNAME_ENTRY(VK_CONVERT),
      VALANDNAME_ENTRY(VK_NONCONVERT),
      VALANDNAME_ENTRY(VK_ACCEPT),
      VALANDNAME_ENTRY(VK_MODECHANGE),
      VALANDNAME_ENTRY(VK_SPACE),
      VALANDNAME_ENTRY(VK_PRIOR),
      VALANDNAME_ENTRY(VK_NEXT),
      VALANDNAME_ENTRY(VK_END),
      VALANDNAME_ENTRY(VK_HOME),
      VALANDNAME_ENTRY(VK_LEFT),
      VALANDNAME_ENTRY(VK_UP),
      VALANDNAME_ENTRY(VK_RIGHT),
      VALANDNAME_ENTRY(VK_DOWN),
      VALANDNAME_ENTRY(VK_SELECT),
      VALANDNAME_ENTRY(VK_PRINT),
      VALANDNAME_ENTRY(VK_EXECUTE),
      VALANDNAME_ENTRY(VK_SNAPSHOT),
      VALANDNAME_ENTRY(VK_INSERT),
      VALANDNAME_ENTRY(VK_DELETE),
      VALANDNAME_ENTRY(VK_HELP),
      VALANDNAME_ENTRY(VK_LWIN),
      VALANDNAME_ENTRY(VK_RWIN),
      VALANDNAME_ENTRY(VK_APPS),
      VALANDNAME_ENTRY(VK_SLEEP),
      VALANDNAME_ENTRY(VK_NUMPAD0),
      VALANDNAME_ENTRY(VK_NUMPAD1),
      VALANDNAME_ENTRY(VK_NUMPAD2),
      VALANDNAME_ENTRY(VK_NUMPAD3),
      VALANDNAME_ENTRY(VK_NUMPAD4),
      VALANDNAME_ENTRY(VK_NUMPAD5),
      VALANDNAME_ENTRY(VK_NUMPAD6),
      VALANDNAME_ENTRY(VK_NUMPAD7),
      VALANDNAME_ENTRY(VK_NUMPAD8),
      VALANDNAME_ENTRY(VK_NUMPAD9),
      VALANDNAME_ENTRY(VK_MULTIPLY),
      VALANDNAME_ENTRY(VK_ADD),
      VALANDNAME_ENTRY(VK_SEPARATOR),
      VALANDNAME_ENTRY(VK_SUBTRACT),
      VALANDNAME_ENTRY(VK_DECIMAL),
      VALANDNAME_ENTRY(VK_DIVIDE),
      VALANDNAME_ENTRY(VK_F1),
      VALANDNAME_ENTRY(VK_F2),
      VALANDNAME_ENTRY(VK_F3),
      VALANDNAME_ENTRY(VK_F4),
      VALANDNAME_ENTRY(VK_F5),
      VALANDNAME_ENTRY(VK_F6),
      VALANDNAME_ENTRY(VK_F7),
      VALANDNAME_ENTRY(VK_F8),
      VALANDNAME_ENTRY(VK_F9),
      VALANDNAME_ENTRY(VK_F10),
      VALANDNAME_ENTRY(VK_F11),
      VALANDNAME_ENTRY(VK_F12),
      VALANDNAME_ENTRY(VK_F13),
      VALANDNAME_ENTRY(VK_F14),
      VALANDNAME_ENTRY(VK_F15),
      VALANDNAME_ENTRY(VK_F16),
      VALANDNAME_ENTRY(VK_F17),
      VALANDNAME_ENTRY(VK_F18),
      VALANDNAME_ENTRY(VK_F19),
      VALANDNAME_ENTRY(VK_F20),
      VALANDNAME_ENTRY(VK_F21),
      VALANDNAME_ENTRY(VK_F22),
      VALANDNAME_ENTRY(VK_F23),
      VALANDNAME_ENTRY(VK_F24),
      VALANDNAME_ENTRY(VK_NUMLOCK),
      VALANDNAME_ENTRY(VK_SCROLL),
      VALANDNAME_ENTRY(VK_LSHIFT),
      VALANDNAME_ENTRY(VK_RSHIFT),
      VALANDNAME_ENTRY(VK_LCONTROL),
      VALANDNAME_ENTRY(VK_RCONTROL),
      VALANDNAME_ENTRY(VK_LMENU),
      VALANDNAME_ENTRY(VK_RMENU),
      VALANDNAME_ENTRY(VK_BROWSER_BACK),
      VALANDNAME_ENTRY(VK_BROWSER_FORWARD),
      VALANDNAME_ENTRY(VK_BROWSER_REFRESH),
      VALANDNAME_ENTRY(VK_BROWSER_STOP),
      VALANDNAME_ENTRY(VK_BROWSER_SEARCH),
      VALANDNAME_ENTRY(VK_BROWSER_FAVORITES),
      VALANDNAME_ENTRY(VK_BROWSER_HOME),
      VALANDNAME_ENTRY(VK_VOLUME_MUTE),
      VALANDNAME_ENTRY(VK_VOLUME_DOWN),
      VALANDNAME_ENTRY(VK_VOLUME_UP),
      VALANDNAME_ENTRY(VK_MEDIA_NEXT_TRACK),
      VALANDNAME_ENTRY(VK_MEDIA_PREV_TRACK),
      VALANDNAME_ENTRY(VK_MEDIA_STOP),
      VALANDNAME_ENTRY(VK_MEDIA_PLAY_PAUSE),
      VALANDNAME_ENTRY(VK_LAUNCH_MAIL),
      VALANDNAME_ENTRY(VK_LAUNCH_MEDIA_SELECT),
      VALANDNAME_ENTRY(VK_LAUNCH_APP1),
      VALANDNAME_ENTRY(VK_LAUNCH_APP2),
      VALANDNAME_ENTRY(VK_OEM_1),
      VALANDNAME_ENTRY(VK_OEM_PLUS),
      VALANDNAME_ENTRY(VK_OEM_COMMA),
      VALANDNAME_ENTRY(VK_OEM_MINUS),
      VALANDNAME_ENTRY(VK_OEM_PERIOD),
      VALANDNAME_ENTRY(VK_OEM_2),
      VALANDNAME_ENTRY(VK_OEM_3),
      VALANDNAME_ENTRY(VK_OEM_4),
      VALANDNAME_ENTRY(VK_OEM_5),
      VALANDNAME_ENTRY(VK_OEM_6),
      VALANDNAME_ENTRY(VK_OEM_7),
      VALANDNAME_ENTRY(VK_OEM_8),
      VALANDNAME_ENTRY(VK_OEM_102),
      VALANDNAME_ENTRY(VK_PROCESSKEY),
      VALANDNAME_ENTRY(VK_PACKET),
      VALANDNAME_ENTRY(VK_ATTN),
      VALANDNAME_ENTRY(VK_CRSEL),
      VALANDNAME_ENTRY(VK_EXSEL),
      VALANDNAME_ENTRY(VK_EREOF),
      VALANDNAME_ENTRY(VK_PLAY),
      VALANDNAME_ENTRY(VK_ZOOM),
      VALANDNAME_ENTRY(VK_NONAME),
      VALANDNAME_ENTRY(VK_PA1),
      VALANDNAME_ENTRY(VK_OEM_CLEAR),
      {0x30, "0"},
      {0x31, "1"},
      {0x32, "2"},
      {0x33, "3"},
      {0x34, "4"},
      {0x35, "5"},
      {0x36, "6"},
      {0x37, "7"},
      {0x38, "8"},
      {0x39, "9"},
      {0x41, "A"},
      {0x42, "B"},
      {0x43, "C"},
      {0x44, "D"},
      {0x45, "E"},
      {0x46, "F"},
      {0x47, "G"},
      {0x48, "H"},
      {0x49, "I"},
      {0x4A, "J"},
      {0x4B, "K"},
      {0x4C, "L"},
      {0x4D, "M"},
      {0x4E, "N"},
      {0x4F, "O"},
      {0x50, "P"},
      {0x51, "Q"},
      {0x52, "S"},
      {0x53, "T"},
      {0x54, "U"},
      {0x55, "V"},
      {0x56, "W"},
      {0x57, "X"},
      {0x58, "Y"},
      {0x59, "Z"},
  };
  AppendEnumValueInfo(result, param, virtualKeys, name);
}

void VirtualModifierKeysParamInfo(nsCString& result, uint64_t param,
                                  const char* name, bool /* isPreCall */) {
  const static nsTArray<EnumValueAndName> virtualKeys{
      VALANDNAME_ENTRY(MK_CONTROL),  VALANDNAME_ENTRY(MK_LBUTTON),
      VALANDNAME_ENTRY(MK_MBUTTON),  VALANDNAME_ENTRY(MK_RBUTTON),
      VALANDNAME_ENTRY(MK_SHIFT),    VALANDNAME_ENTRY(MK_XBUTTON1),
      VALANDNAME_ENTRY(MK_XBUTTON2), {0, "(none)"}};
  AppendFlagsInfo(result, param, virtualKeys, name);
}

void ParentNotifyEventParamInfo(nsCString& str, uint64_t param,
                                const char* /* name */, bool /* isPreCall */) {
  const static std::unordered_map<uint64_t, const char*> eventValues{
      VALANDNAME_ENTRY(WM_CREATE),      VALANDNAME_ENTRY(WM_DESTROY),
      VALANDNAME_ENTRY(WM_LBUTTONDOWN), VALANDNAME_ENTRY(WM_MBUTTONDOWN),
      VALANDNAME_ENTRY(WM_RBUTTONDOWN), VALANDNAME_ENTRY(WM_XBUTTONDOWN),
      VALANDNAME_ENTRY(WM_POINTERDOWN)};
  AppendEnumValueInfo(str, LOWORD(param), eventValues, "event");
  str.AppendASCII(" ");
  HexParamInfo(str, HIWORD(param), "hiWord", false);
}

void KeystrokeFlagsParamInfo(nsCString& str, uint64_t param,
                             const char* /* name */, bool /* isPreCall */) {
  WORD repeatCount = LOWORD(param);
  WORD keyFlags = HIWORD(param);
  WORD scanCode = LOBYTE(keyFlags);
  bool isExtendedKey = (keyFlags & KF_EXTENDED) == KF_EXTENDED;
  if (isExtendedKey) {
    scanCode = MAKEWORD(scanCode, 0xE0);
  }
  bool contextCode = (keyFlags & KF_ALTDOWN) == KF_ALTDOWN;
  bool wasKeyDown = (keyFlags & KF_REPEAT) == KF_REPEAT;
  bool transitionState = (keyFlags & KF_UP) == KF_UP;

  str.AppendPrintf(
      "repeatCount: %d scanCode: %d isExtended: %d, contextCode: %d "
      "previousKeyState: %d transitionState: %d",
      repeatCount, scanCode, isExtendedKey ? 1 : 0, contextCode ? 1 : 0,
      wasKeyDown ? 1 : 0, transitionState ? 1 : 0);
};

void VirtualKeysLowWordDistanceHighWordParamInfo(nsCString& str, uint64_t value,
                                                 const char* /* name */,
                                                 bool isPreCall) {
  VirtualModifierKeysParamInfo(str, LOWORD(value), "virtualKeys", isPreCall);
  str.AppendASCII(" ");
  IntParamInfo(str, HIWORD(value), "distance", isPreCall);
}

void ShowWindowReasonParamInfo(nsCString& str, uint64_t value, const char* name,
                               bool /* isPreCall */) {
  const static std::unordered_map<uint64_t, const char*> showWindowReasonValues{
      VALANDNAME_ENTRY(SW_OTHERUNZOOM),
      VALANDNAME_ENTRY(SW_OTHERZOOM),
      VALANDNAME_ENTRY(SW_PARENTCLOSING),
      VALANDNAME_ENTRY(SW_PARENTOPENING),
      {0, "Call to ShowWindow()"}};
  AppendEnumValueInfo(str, value, showWindowReasonValues, name);
}

void WindowEdgeParamInfo(nsCString& str, uint64_t value, const char* name,
                         bool /* isPreCall */) {
  const static std::unordered_map<uint64_t, const char*> windowEdgeValues{
      VALANDNAME_ENTRY(WMSZ_BOTTOM),      VALANDNAME_ENTRY(WMSZ_BOTTOMLEFT),
      VALANDNAME_ENTRY(WMSZ_BOTTOMRIGHT), VALANDNAME_ENTRY(WMSZ_LEFT),
      VALANDNAME_ENTRY(WMSZ_RIGHT),       VALANDNAME_ENTRY(WMSZ_TOP),
      VALANDNAME_ENTRY(WMSZ_TOPLEFT),     VALANDNAME_ENTRY(WMSZ_TOPRIGHT)};
  AppendEnumValueInfo(str, value, windowEdgeValues, name);
}

void UiActionParamInfo(nsCString& str, uint64_t value, const char* name,
                       bool /* isPreCall */) {
  const static std::unordered_map<uint64_t, const char*> uiActionValues{
      VALANDNAME_ENTRY(SPI_GETACCESSTIMEOUT),
      VALANDNAME_ENTRY(SPI_GETAUDIODESCRIPTION),
      VALANDNAME_ENTRY(SPI_GETCLIENTAREAANIMATION),
      VALANDNAME_ENTRY(SPI_GETDISABLEOVERLAPPEDCONTENT),
      VALANDNAME_ENTRY(SPI_GETFILTERKEYS),
      VALANDNAME_ENTRY(SPI_GETFOCUSBORDERHEIGHT),
      VALANDNAME_ENTRY(SPI_GETFOCUSBORDERWIDTH),
      VALANDNAME_ENTRY(SPI_GETHIGHCONTRAST),
      VALANDNAME_ENTRY(SPI_GETLOGICALDPIOVERRIDE),
      VALANDNAME_ENTRY(SPI_SETLOGICALDPIOVERRIDE),
      VALANDNAME_ENTRY(SPI_GETMESSAGEDURATION),
      VALANDNAME_ENTRY(SPI_GETMOUSECLICKLOCK),
      VALANDNAME_ENTRY(SPI_GETMOUSECLICKLOCKTIME),
      VALANDNAME_ENTRY(SPI_GETMOUSEKEYS),
      VALANDNAME_ENTRY(SPI_GETMOUSESONAR),
      VALANDNAME_ENTRY(SPI_GETMOUSEVANISH),
      VALANDNAME_ENTRY(SPI_GETSCREENREADER),
      VALANDNAME_ENTRY(SPI_GETSERIALKEYS),
      VALANDNAME_ENTRY(SPI_GETSHOWSOUNDS),
      VALANDNAME_ENTRY(SPI_GETSOUNDSENTRY),
      VALANDNAME_ENTRY(SPI_GETSTICKYKEYS),
      VALANDNAME_ENTRY(SPI_GETTOGGLEKEYS),
      VALANDNAME_ENTRY(SPI_SETACCESSTIMEOUT),
      VALANDNAME_ENTRY(SPI_SETAUDIODESCRIPTION),
      VALANDNAME_ENTRY(SPI_SETCLIENTAREAANIMATION),
      VALANDNAME_ENTRY(SPI_SETDISABLEOVERLAPPEDCONTENT),
      VALANDNAME_ENTRY(SPI_SETFILTERKEYS),
      VALANDNAME_ENTRY(SPI_SETFOCUSBORDERHEIGHT),
      VALANDNAME_ENTRY(SPI_SETFOCUSBORDERWIDTH),
      VALANDNAME_ENTRY(SPI_SETHIGHCONTRAST),
      VALANDNAME_ENTRY(SPI_SETMESSAGEDURATION),
      VALANDNAME_ENTRY(SPI_SETMOUSECLICKLOCK),
      VALANDNAME_ENTRY(SPI_SETMOUSECLICKLOCKTIME),
      VALANDNAME_ENTRY(SPI_SETMOUSEKEYS),
      VALANDNAME_ENTRY(SPI_SETMOUSESONAR),
      VALANDNAME_ENTRY(SPI_SETMOUSEVANISH),
      VALANDNAME_ENTRY(SPI_SETSCREENREADER),
      VALANDNAME_ENTRY(SPI_SETSERIALKEYS),
      VALANDNAME_ENTRY(SPI_SETSHOWSOUNDS),
      VALANDNAME_ENTRY(SPI_SETSOUNDSENTRY),
      VALANDNAME_ENTRY(SPI_SETSTICKYKEYS),
      VALANDNAME_ENTRY(SPI_SETTOGGLEKEYS),
      VALANDNAME_ENTRY(SPI_GETCLEARTYPE),
      VALANDNAME_ENTRY(SPI_GETDESKWALLPAPER),
      VALANDNAME_ENTRY(SPI_GETDROPSHADOW),
      VALANDNAME_ENTRY(SPI_GETFLATMENU),
      VALANDNAME_ENTRY(SPI_GETFONTSMOOTHING),
      VALANDNAME_ENTRY(SPI_GETFONTSMOOTHINGCONTRAST),
      VALANDNAME_ENTRY(SPI_GETFONTSMOOTHINGORIENTATION),
      VALANDNAME_ENTRY(SPI_GETFONTSMOOTHINGTYPE),
      VALANDNAME_ENTRY(SPI_GETWORKAREA),
      VALANDNAME_ENTRY(SPI_SETCLEARTYPE),
      VALANDNAME_ENTRY(SPI_SETCURSORS),
      VALANDNAME_ENTRY(SPI_SETDESKPATTERN),
      VALANDNAME_ENTRY(SPI_SETDESKWALLPAPER),
      VALANDNAME_ENTRY(SPI_SETDROPSHADOW),
      VALANDNAME_ENTRY(SPI_SETFLATMENU),
      VALANDNAME_ENTRY(SPI_SETFONTSMOOTHING),
      VALANDNAME_ENTRY(SPI_SETFONTSMOOTHINGCONTRAST),
      VALANDNAME_ENTRY(SPI_SETFONTSMOOTHINGORIENTATION),
      VALANDNAME_ENTRY(SPI_SETFONTSMOOTHINGTYPE),
      VALANDNAME_ENTRY(SPI_SETWORKAREA),
      VALANDNAME_ENTRY(SPI_GETICONMETRICS),
      VALANDNAME_ENTRY(SPI_GETICONTITLELOGFONT),
      VALANDNAME_ENTRY(SPI_GETICONTITLEWRAP),
      VALANDNAME_ENTRY(SPI_ICONHORIZONTALSPACING),
      VALANDNAME_ENTRY(SPI_ICONVERTICALSPACING),
      VALANDNAME_ENTRY(SPI_SETICONMETRICS),
      VALANDNAME_ENTRY(SPI_SETICONS),
      VALANDNAME_ENTRY(SPI_SETICONTITLELOGFONT),
      VALANDNAME_ENTRY(SPI_SETICONTITLEWRAP),
      VALANDNAME_ENTRY(SPI_GETBEEP),
      VALANDNAME_ENTRY(SPI_GETBLOCKSENDINPUTRESETS),
      VALANDNAME_ENTRY(SPI_GETCONTACTVISUALIZATION),
      VALANDNAME_ENTRY(SPI_SETCONTACTVISUALIZATION),
      VALANDNAME_ENTRY(SPI_GETDEFAULTINPUTLANG),
      VALANDNAME_ENTRY(SPI_GETGESTUREVISUALIZATION),
      VALANDNAME_ENTRY(SPI_SETGESTUREVISUALIZATION),
      VALANDNAME_ENTRY(SPI_GETKEYBOARDCUES),
      VALANDNAME_ENTRY(SPI_GETKEYBOARDDELAY),
      VALANDNAME_ENTRY(SPI_GETKEYBOARDPREF),
      VALANDNAME_ENTRY(SPI_GETKEYBOARDSPEED),
      VALANDNAME_ENTRY(SPI_GETMOUSE),
      VALANDNAME_ENTRY(SPI_GETMOUSEHOVERHEIGHT),
      VALANDNAME_ENTRY(SPI_GETMOUSEHOVERTIME),
      VALANDNAME_ENTRY(SPI_GETMOUSEHOVERWIDTH),
      VALANDNAME_ENTRY(SPI_GETMOUSESPEED),
      VALANDNAME_ENTRY(SPI_GETMOUSETRAILS),
      VALANDNAME_ENTRY(SPI_GETMOUSEWHEELROUTING),
      VALANDNAME_ENTRY(SPI_SETMOUSEWHEELROUTING),
      VALANDNAME_ENTRY(SPI_GETPENVISUALIZATION),
      VALANDNAME_ENTRY(SPI_SETPENVISUALIZATION),
      VALANDNAME_ENTRY(SPI_GETSNAPTODEFBUTTON),
      VALANDNAME_ENTRY(SPI_GETSYSTEMLANGUAGEBAR),
      VALANDNAME_ENTRY(SPI_SETSYSTEMLANGUAGEBAR),
      VALANDNAME_ENTRY(SPI_GETTHREADLOCALINPUTSETTINGS),
      VALANDNAME_ENTRY(SPI_SETTHREADLOCALINPUTSETTINGS),
      VALANDNAME_ENTRY(SPI_GETWHEELSCROLLCHARS),
      VALANDNAME_ENTRY(SPI_GETWHEELSCROLLLINES),
      VALANDNAME_ENTRY(SPI_SETBEEP),
      VALANDNAME_ENTRY(SPI_SETBLOCKSENDINPUTRESETS),
      VALANDNAME_ENTRY(SPI_SETDEFAULTINPUTLANG),
      VALANDNAME_ENTRY(SPI_SETDOUBLECLICKTIME),
      VALANDNAME_ENTRY(SPI_SETDOUBLECLKHEIGHT),
      VALANDNAME_ENTRY(SPI_SETDOUBLECLKWIDTH),
      VALANDNAME_ENTRY(SPI_SETKEYBOARDCUES),
      VALANDNAME_ENTRY(SPI_SETKEYBOARDDELAY),
      VALANDNAME_ENTRY(SPI_SETKEYBOARDPREF),
      VALANDNAME_ENTRY(SPI_SETKEYBOARDSPEED),
      VALANDNAME_ENTRY(SPI_SETLANGTOGGLE),
      VALANDNAME_ENTRY(SPI_SETMOUSE),
      VALANDNAME_ENTRY(SPI_SETMOUSEBUTTONSWAP),
      VALANDNAME_ENTRY(SPI_SETMOUSEHOVERHEIGHT),
      VALANDNAME_ENTRY(SPI_SETMOUSEHOVERTIME),
      VALANDNAME_ENTRY(SPI_SETMOUSEHOVERWIDTH),
      VALANDNAME_ENTRY(SPI_SETMOUSESPEED),
      VALANDNAME_ENTRY(SPI_SETMOUSETRAILS),
      VALANDNAME_ENTRY(SPI_SETSNAPTODEFBUTTON),
      VALANDNAME_ENTRY(SPI_SETWHEELSCROLLCHARS),
      VALANDNAME_ENTRY(SPI_SETWHEELSCROLLLINES),
      VALANDNAME_ENTRY(SPI_GETMENUDROPALIGNMENT),
      VALANDNAME_ENTRY(SPI_GETMENUFADE),
      VALANDNAME_ENTRY(SPI_GETMENUSHOWDELAY),
      VALANDNAME_ENTRY(SPI_SETMENUDROPALIGNMENT),
      VALANDNAME_ENTRY(SPI_SETMENUFADE),
      VALANDNAME_ENTRY(SPI_SETMENUSHOWDELAY),
      VALANDNAME_ENTRY(SPI_GETLOWPOWERACTIVE),
      VALANDNAME_ENTRY(SPI_GETLOWPOWERTIMEOUT),
      VALANDNAME_ENTRY(SPI_GETPOWEROFFACTIVE),
      VALANDNAME_ENTRY(SPI_GETPOWEROFFTIMEOUT),
      VALANDNAME_ENTRY(SPI_SETLOWPOWERACTIVE),
      VALANDNAME_ENTRY(SPI_SETLOWPOWERTIMEOUT),
      VALANDNAME_ENTRY(SPI_SETPOWEROFFACTIVE),
      VALANDNAME_ENTRY(SPI_SETPOWEROFFTIMEOUT),
      VALANDNAME_ENTRY(SPI_GETSCREENSAVEACTIVE),
      VALANDNAME_ENTRY(SPI_GETSCREENSAVERRUNNING),
      VALANDNAME_ENTRY(SPI_GETSCREENSAVESECURE),
      VALANDNAME_ENTRY(SPI_GETSCREENSAVETIMEOUT),
      VALANDNAME_ENTRY(SPI_SETSCREENSAVEACTIVE),
      VALANDNAME_ENTRY(SPI_SETSCREENSAVERRUNNING),
      VALANDNAME_ENTRY(SPI_SETSCREENSAVESECURE),
      VALANDNAME_ENTRY(SPI_SETSCREENSAVETIMEOUT),
      VALANDNAME_ENTRY(SPI_GETHUNGAPPTIMEOUT),
      VALANDNAME_ENTRY(SPI_GETWAITTOKILLTIMEOUT),
      VALANDNAME_ENTRY(SPI_GETWAITTOKILLSERVICETIMEOUT),
      VALANDNAME_ENTRY(SPI_SETHUNGAPPTIMEOUT),
      VALANDNAME_ENTRY(SPI_SETWAITTOKILLTIMEOUT),
      VALANDNAME_ENTRY(SPI_SETWAITTOKILLSERVICETIMEOUT),
      VALANDNAME_ENTRY(SPI_GETCOMBOBOXANIMATION),
      VALANDNAME_ENTRY(SPI_GETCURSORSHADOW),
      VALANDNAME_ENTRY(SPI_GETGRADIENTCAPTIONS),
      VALANDNAME_ENTRY(SPI_GETHOTTRACKING),
      VALANDNAME_ENTRY(SPI_GETLISTBOXSMOOTHSCROLLING),
      VALANDNAME_ENTRY(SPI_GETMENUANIMATION),
      VALANDNAME_ENTRY(SPI_GETMENUUNDERLINES),
      VALANDNAME_ENTRY(SPI_GETSELECTIONFADE),
      VALANDNAME_ENTRY(SPI_GETTOOLTIPANIMATION),
      VALANDNAME_ENTRY(SPI_GETTOOLTIPFADE),
      VALANDNAME_ENTRY(SPI_GETUIEFFECTS),
      VALANDNAME_ENTRY(SPI_SETCOMBOBOXANIMATION),
      VALANDNAME_ENTRY(SPI_SETCURSORSHADOW),
      VALANDNAME_ENTRY(SPI_SETGRADIENTCAPTIONS),
      VALANDNAME_ENTRY(SPI_SETHOTTRACKING),
      VALANDNAME_ENTRY(SPI_SETLISTBOXSMOOTHSCROLLING),
      VALANDNAME_ENTRY(SPI_SETMENUANIMATION),
      VALANDNAME_ENTRY(SPI_SETMENUUNDERLINES),
      VALANDNAME_ENTRY(SPI_SETSELECTIONFADE),
      VALANDNAME_ENTRY(SPI_SETTOOLTIPANIMATION),
      VALANDNAME_ENTRY(SPI_SETTOOLTIPFADE),
      VALANDNAME_ENTRY(SPI_SETUIEFFECTS),
      VALANDNAME_ENTRY(SPI_GETACTIVEWINDOWTRACKING),
      VALANDNAME_ENTRY(SPI_GETACTIVEWNDTRKZORDER),
      VALANDNAME_ENTRY(SPI_GETACTIVEWNDTRKTIMEOUT),
      VALANDNAME_ENTRY(SPI_GETANIMATION),
      VALANDNAME_ENTRY(SPI_GETBORDER),
      VALANDNAME_ENTRY(SPI_GETCARETWIDTH),
      VALANDNAME_ENTRY(SPI_GETDOCKMOVING),
      VALANDNAME_ENTRY(SPI_GETDRAGFROMMAXIMIZE),
      VALANDNAME_ENTRY(SPI_GETDRAGFULLWINDOWS),
      VALANDNAME_ENTRY(SPI_GETFOREGROUNDFLASHCOUNT),
      VALANDNAME_ENTRY(SPI_GETFOREGROUNDLOCKTIMEOUT),
      VALANDNAME_ENTRY(SPI_GETMINIMIZEDMETRICS),
      VALANDNAME_ENTRY(SPI_GETMOUSEDOCKTHRESHOLD),
      VALANDNAME_ENTRY(SPI_GETMOUSEDRAGOUTTHRESHOLD),
      VALANDNAME_ENTRY(SPI_GETMOUSESIDEMOVETHRESHOLD),
      VALANDNAME_ENTRY(SPI_GETNONCLIENTMETRICS),
      VALANDNAME_ENTRY(SPI_GETPENDOCKTHRESHOLD),
      VALANDNAME_ENTRY(SPI_GETPENDRAGOUTTHRESHOLD),
      VALANDNAME_ENTRY(SPI_GETPENSIDEMOVETHRESHOLD),
      VALANDNAME_ENTRY(SPI_GETSHOWIMEUI),
      VALANDNAME_ENTRY(SPI_GETSNAPSIZING),
      VALANDNAME_ENTRY(SPI_GETWINARRANGING),
      VALANDNAME_ENTRY(SPI_SETACTIVEWINDOWTRACKING),
      VALANDNAME_ENTRY(SPI_SETACTIVEWNDTRKZORDER),
      VALANDNAME_ENTRY(SPI_SETACTIVEWNDTRKTIMEOUT),
      VALANDNAME_ENTRY(SPI_SETANIMATION),
      VALANDNAME_ENTRY(SPI_SETBORDER),
      VALANDNAME_ENTRY(SPI_SETCARETWIDTH),
      VALANDNAME_ENTRY(SPI_SETDOCKMOVING),
      VALANDNAME_ENTRY(SPI_SETDRAGFROMMAXIMIZE),
      VALANDNAME_ENTRY(SPI_SETDRAGFULLWINDOWS),
      VALANDNAME_ENTRY(SPI_SETFOREGROUNDFLASHCOUNT),
      VALANDNAME_ENTRY(SPI_SETFOREGROUNDLOCKTIMEOUT),
      VALANDNAME_ENTRY(SPI_SETMINIMIZEDMETRICS),
      VALANDNAME_ENTRY(SPI_SETMOUSEDOCKTHRESHOLD),
      VALANDNAME_ENTRY(SPI_SETMOUSEDRAGOUTTHRESHOLD),
      VALANDNAME_ENTRY(SPI_SETMOUSESIDEMOVETHRESHOLD),
      VALANDNAME_ENTRY(SPI_SETNONCLIENTMETRICS),
      VALANDNAME_ENTRY(SPI_SETPENDOCKTHRESHOLD),
      VALANDNAME_ENTRY(SPI_SETPENDRAGOUTTHRESHOLD),
      VALANDNAME_ENTRY(SPI_SETPENSIDEMOVETHRESHOLD),
      VALANDNAME_ENTRY(SPI_SETSHOWIMEUI),
      VALANDNAME_ENTRY(SPI_SETSNAPSIZING),
      VALANDNAME_ENTRY(SPI_SETWINARRANGING),
  };
  AppendEnumValueInfo(str, value, uiActionValues, name);
}

nsAutoCString WmSizeParamInfo(uint64_t wParam, uint64_t lParam,
                              bool /* isPreCall */) {
  nsAutoCString result;
  const static std::unordered_map<uint64_t, const char*> sizeValues{
      VALANDNAME_ENTRY(SIZE_RESTORED), VALANDNAME_ENTRY(SIZE_MINIMIZED),
      VALANDNAME_ENTRY(SIZE_MAXIMIZED), VALANDNAME_ENTRY(SIZE_MAXSHOW),
      VALANDNAME_ENTRY(SIZE_MAXHIDE)};
  AppendEnumValueInfo(result, wParam, sizeValues, "size");
  result.AppendPrintf(" width=%d height=%d", static_cast<int>(LOWORD(lParam)),
                      static_cast<int>(HIWORD(lParam)));
  return result;
}

const nsTArray<EnumValueAndName> windowPositionFlags = {
    VALANDNAME_ENTRY(SWP_DRAWFRAME),  VALANDNAME_ENTRY(SWP_HIDEWINDOW),
    VALANDNAME_ENTRY(SWP_NOACTIVATE), VALANDNAME_ENTRY(SWP_NOCOPYBITS),
    VALANDNAME_ENTRY(SWP_NOMOVE),     VALANDNAME_ENTRY(SWP_NOOWNERZORDER),
    VALANDNAME_ENTRY(SWP_NOREDRAW),   VALANDNAME_ENTRY(SWP_NOSENDCHANGING),
    VALANDNAME_ENTRY(SWP_NOSIZE),     VALANDNAME_ENTRY(SWP_NOZORDER),
    VALANDNAME_ENTRY(SWP_SHOWWINDOW),
};

void WindowPosParamInfo(nsCString& str, uint64_t value, const char* name,
                        bool /* isPreCall */) {
  LPWINDOWPOS windowPos = reinterpret_cast<LPWINDOWPOS>(value);
  if (windowPos == nullptr) {
    str.AppendASCII("null windowPos?");
    return;
  }
  HexParamInfo(str, reinterpret_cast<uint64_t>(windowPos->hwnd), "hwnd", false);
  str.AppendASCII(" ");
  HexParamInfo(str, reinterpret_cast<uint64_t>(windowPos->hwndInsertAfter),
               "hwndInsertAfter", false);
  str.AppendASCII(" ");
  IntParamInfo(str, windowPos->x, "x", false);
  str.AppendASCII(" ");
  IntParamInfo(str, windowPos->y, "y", false);
  str.AppendASCII(" ");
  IntParamInfo(str, windowPos->cx, "cx", false);
  str.AppendASCII(" ");
  IntParamInfo(str, windowPos->cy, "cy", false);
  str.AppendASCII(" ");
  AppendFlagsInfo(str, windowPos->flags, windowPositionFlags, "flags");
}

void StyleOrExtendedParamInfo(nsCString& str, uint64_t value, const char* name,
                              bool /* isPreCall */) {
  const static std::unordered_map<uint64_t, const char*> styleOrExtended{
      VALANDNAME_ENTRY(GWL_EXSTYLE), VALANDNAME_ENTRY(GWL_STYLE)};
  AppendEnumValueInfo(str, value, styleOrExtended, name);
}

void StyleStructParamInfo(nsCString& str, uint64_t value, const char* name,
                          bool /* isPreCall */) {
  LPSTYLESTRUCT styleStruct = reinterpret_cast<LPSTYLESTRUCT>(value);
  if (styleStruct == nullptr) {
    str.AppendASCII("null STYLESTRUCT?");
    return;
  }
  HexParamInfo(str, styleStruct->styleOld, "styleOld", false);
  str.AppendASCII(" ");
  HexParamInfo(str, styleStruct->styleNew, "styleNew", false);
}

void NcCalcSizeParamsParamInfo(nsCString& str, uint64_t value, const char* name,
                               bool /* isPreCall */) {
  LPNCCALCSIZE_PARAMS params = reinterpret_cast<LPNCCALCSIZE_PARAMS>(value);
  if (params == nullptr) {
    str.AppendASCII("null NCCALCSIZE_PARAMS?");
    return;
  }
  str.AppendPrintf("%s[0]: ", name);
  RectParamInfo(str, reinterpret_cast<uintptr_t>(&params->rgrc[0]), nullptr,
                false);
  str.AppendPrintf(" %s[1]: ", name);
  RectParamInfo(str, reinterpret_cast<uintptr_t>(&params->rgrc[1]), nullptr,
                false);
  str.AppendPrintf(" %s[2]: ", name);
  RectParamInfo(str, reinterpret_cast<uintptr_t>(&params->rgrc[2]), nullptr,
                false);
  str.AppendASCII(" ");
  WindowPosParamInfo(str, reinterpret_cast<uintptr_t>(params->lppos), nullptr,
                     false);
}

nsAutoCString WmNcCalcSizeParamInfo(uint64_t wParam, uint64_t lParam,
                                    bool /* isPreCall */) {
  nsAutoCString result;
  TrueFalseParamInfo(result, wParam, "shouldIndicateValidArea", false);
  result.AppendASCII(" ");
  if (wParam == TRUE) {
    NcCalcSizeParamsParamInfo(result, lParam, "ncCalcSizeParams", false);
  } else {
    RectParamInfo(result, lParam, "rect", false);
  }
  return result;
}

void ActivateWParamInfo(nsCString& result, uint64_t wParam, const char* name,
                        bool /* isPreCall */) {
  const static std::unordered_map<uint64_t, const char*> activateValues{
      VALANDNAME_ENTRY(WA_ACTIVE), VALANDNAME_ENTRY(WA_CLICKACTIVE),
      VALANDNAME_ENTRY(WA_INACTIVE)};
  AppendEnumValueInfo(result, wParam, activateValues, name);
}

void HitTestParamInfo(nsCString& result, uint64_t param, const char* name,
                      bool /* isPreCall */) {
  const static std::unordered_map<uint64_t, const char*> hitTestResults{
      VALANDNAME_ENTRY(HTBORDER),     VALANDNAME_ENTRY(HTBOTTOM),
      VALANDNAME_ENTRY(HTBOTTOMLEFT), VALANDNAME_ENTRY(HTBOTTOMRIGHT),
      VALANDNAME_ENTRY(HTCAPTION),    VALANDNAME_ENTRY(HTCLIENT),
      VALANDNAME_ENTRY(HTCLOSE),      VALANDNAME_ENTRY(HTERROR),
      VALANDNAME_ENTRY(HTGROWBOX),    VALANDNAME_ENTRY(HTHELP),
      VALANDNAME_ENTRY(HTHSCROLL),    VALANDNAME_ENTRY(HTLEFT),
      VALANDNAME_ENTRY(HTMENU),       VALANDNAME_ENTRY(HTMAXBUTTON),
      VALANDNAME_ENTRY(HTMINBUTTON),  VALANDNAME_ENTRY(HTNOWHERE),
      VALANDNAME_ENTRY(HTREDUCE),     VALANDNAME_ENTRY(HTRIGHT),
      VALANDNAME_ENTRY(HTSIZE),       VALANDNAME_ENTRY(HTSYSMENU),
      VALANDNAME_ENTRY(HTTOP),        VALANDNAME_ENTRY(HTTOPLEFT),
      VALANDNAME_ENTRY(HTTOPRIGHT),   VALANDNAME_ENTRY(HTTRANSPARENT),
      VALANDNAME_ENTRY(HTVSCROLL),    VALANDNAME_ENTRY(HTZOOM),
  };
  AppendEnumValueInfo(result, param, hitTestResults, name);
}

void SetCursorLParamInfo(nsCString& result, uint64_t lParam,
                         const char* /* name */, bool /* isPreCall */) {
  HitTestParamInfo(result, LOWORD(lParam), "hitTestResult", false);
  result.AppendASCII(" ");
  HexParamInfo(result, HIWORD(lParam), "message", false);
}

void MinMaxInfoParamInfo(nsCString& result, uint64_t value,
                         const char* /* name */, bool /* isPreCall */) {
  PMINMAXINFO minMaxInfo = reinterpret_cast<PMINMAXINFO>(value);
  if (minMaxInfo == nullptr) {
    result.AppendPrintf("NULL minMaxInfo?");
    return;
  }
  PointExplicitParamInfo(result, minMaxInfo->ptMaxSize, "maxSize");
  result.AppendASCII(" ");
  PointExplicitParamInfo(result, minMaxInfo->ptMaxPosition, "maxPosition");
  result.AppendASCII(" ");
  PointExplicitParamInfo(result, minMaxInfo->ptMinTrackSize, "minTrackSize");
  result.AppendASCII(" ");
  PointExplicitParamInfo(result, minMaxInfo->ptMaxTrackSize, "maxTrackSize");
}

void WideStringParamInfo(nsCString& result, uint64_t value, const char* name,
                         bool /* isPreCall */) {
  result.AppendPrintf("%s=%S", name, reinterpret_cast<LPCWSTR>(value));
}

void DeviceEventParamInfo(nsCString& result, uint64_t value, const char* name,
                          bool /* isPreCall */) {
  const static std::unordered_map<uint64_t, const char*> deviceEventValues{
      VALANDNAME_ENTRY(DBT_DEVNODES_CHANGED),
      VALANDNAME_ENTRY(DBT_QUERYCHANGECONFIG),
      VALANDNAME_ENTRY(DBT_CONFIGCHANGED),
      VALANDNAME_ENTRY(DBT_CONFIGCHANGECANCELED),
      VALANDNAME_ENTRY(DBT_DEVICEARRIVAL),
      VALANDNAME_ENTRY(DBT_DEVICEQUERYREMOVE),
      VALANDNAME_ENTRY(DBT_DEVICEQUERYREMOVEFAILED),
      VALANDNAME_ENTRY(DBT_DEVICEREMOVEPENDING),
      VALANDNAME_ENTRY(DBT_DEVICEREMOVECOMPLETE),
      VALANDNAME_ENTRY(DBT_DEVICETYPESPECIFIC),
      VALANDNAME_ENTRY(DBT_CUSTOMEVENT),
      VALANDNAME_ENTRY(DBT_USERDEFINED)};
  AppendEnumValueInfo(result, value, deviceEventValues, name);
}

void ResolutionParamInfo(nsCString& result, uint64_t value, const char* name,
                         bool /* isPreCall */) {
  result.AppendPrintf("horizontalRes=%d verticalRes=%d", LOWORD(value),
                      HIWORD(value));
}

// Window message with default wParam/lParam logging
#define ENTRY(_msg)                         \
  {                                         \
    _msg, { #_msg, _msg, DefaultParamInfo } \
  }
// Window message with no parameters
#define ENTRY_WITH_NO_PARAM_INFO(_msg) \
  {                                    \
    _msg, { #_msg, _msg, nullptr }     \
  }
// Window message with custom parameter logging functions
#define ENTRY_WITH_CUSTOM_PARAM_INFO(_msg, paramInfoFn) \
  {                                                     \
    _msg, { #_msg, _msg, paramInfoFn }                  \
  }
// Window message with separate custom wParam and lParam logging functions
#define ENTRY_WITH_SPLIT_PARAM_INFOS(_msg, wParamInfoFn, wParamName,           \
                                     lParamInfoFn, lParamName)                 \
  {                                                                            \
    _msg, {                                                                    \
      #_msg, _msg, nullptr, wParamInfoFn, wParamName, lParamInfoFn, lParamName \
    }                                                                          \
  }
std::unordered_map<UINT, EventMsgInfo> gAllEvents = {
    ENTRY_WITH_NO_PARAM_INFO(WM_NULL),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_CREATE, nullptr, nullptr,
                                 CreateStructParamInfo, "createStruct"),
    ENTRY_WITH_NO_PARAM_INFO(WM_DESTROY),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_MOVE, nullptr, nullptr,
                                 XLowWordYHighWordParamInfo, "upperLeft"),
    ENTRY_WITH_CUSTOM_PARAM_INFO(WM_SIZE, WmSizeParamInfo),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_ACTIVATE, ActivateWParamInfo, "wParam",
                                 HexParamInfo, "handle"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_SETFOCUS, HexParamInfo, "handle", nullptr,
                                 nullptr),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_KILLFOCUS, HexParamInfo, "handle", nullptr,
                                 nullptr),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_ENABLE, TrueFalseParamInfo, "enabled",
                                 nullptr, nullptr),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_SETREDRAW, TrueFalseParamInfo,
                                 "redrawState", nullptr, nullptr),
    ENTRY(WM_SETTEXT),
    ENTRY(WM_GETTEXT),
    ENTRY(WM_GETTEXTLENGTH),
    ENTRY_WITH_NO_PARAM_INFO(WM_PAINT),
    ENTRY_WITH_NO_PARAM_INFO(WM_CLOSE),
    ENTRY(WM_QUERYENDSESSION),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_QUIT, HexParamInfo, "exitCode", nullptr,
                                 nullptr),
    ENTRY(WM_QUERYOPEN),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_ERASEBKGND, HexParamInfo, "deviceContext",
                                 nullptr, nullptr),
    ENTRY(WM_SYSCOLORCHANGE),
    ENTRY(WM_ENDSESSION),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_SHOWWINDOW, TrueFalseParamInfo,
                                 "windowBeingShown", ShowWindowReasonParamInfo,
                                 "status"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_SETTINGCHANGE, UiActionParamInfo,
                                 "uiAction", WideStringParamInfo,
                                 "paramChanged"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_DEVMODECHANGE, nullptr, nullptr,
                                 WideStringParamInfo, "deviceName"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_ACTIVATEAPP, TrueFalseParamInfo,
                                 "activated", HexParamInfo, "threadId"),
    ENTRY(WM_FONTCHANGE),
    ENTRY(WM_TIMECHANGE),
    ENTRY(WM_CANCELMODE),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_SETCURSOR, HexParamInfo, "windowHandle",
                                 SetCursorLParamInfo, ""),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_MOUSEACTIVATE, HexParamInfo, "windowHandle",
                                 SetCursorLParamInfo, ""),
    ENTRY(WM_CHILDACTIVATE),
    ENTRY(WM_QUEUESYNC),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_GETMINMAXINFO, nullptr, nullptr,
                                 MinMaxInfoParamInfo, ""),
    ENTRY(WM_PAINTICON),
    ENTRY(WM_ICONERASEBKGND),
    ENTRY(WM_NEXTDLGCTL),
    ENTRY(WM_SPOOLERSTATUS),
    ENTRY(WM_DRAWITEM),
    ENTRY(WM_MEASUREITEM),
    ENTRY(WM_DELETEITEM),
    ENTRY(WM_VKEYTOITEM),
    ENTRY(WM_CHARTOITEM),
    ENTRY(WM_SETFONT),
    ENTRY(WM_GETFONT),
    ENTRY(WM_SETHOTKEY),
    ENTRY(WM_GETHOTKEY),
    ENTRY(WM_QUERYDRAGICON),
    ENTRY(WM_COMPAREITEM),
    ENTRY(WM_GETOBJECT),
    ENTRY(WM_COMPACTING),
    ENTRY(WM_COMMNOTIFY),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_WINDOWPOSCHANGING, nullptr, nullptr,
                                 WindowPosParamInfo, "newSizeAndPos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_WINDOWPOSCHANGED, nullptr, nullptr,
                                 WindowPosParamInfo, "newSizeAndPos"),
    ENTRY(WM_POWER),
    ENTRY(WM_COPYDATA),
    ENTRY(WM_CANCELJOURNAL),
    ENTRY(WM_NOTIFY),
    ENTRY(WM_INPUTLANGCHANGEREQUEST),
    ENTRY(WM_INPUTLANGCHANGE),
    ENTRY(WM_TCARD),
    ENTRY(WM_HELP),
    ENTRY(WM_USERCHANGED),
    ENTRY(WM_NOTIFYFORMAT),
    ENTRY(WM_CONTEXTMENU),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_STYLECHANGING, StyleOrExtendedParamInfo,
                                 "styleOrExtended", StyleStructParamInfo,
                                 "newStyles"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_STYLECHANGED, StyleOrExtendedParamInfo,
                                 "styleOrExtended", StyleStructParamInfo,
                                 "newStyles"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_DISPLAYCHANGE, IntParamInfo, "bitsPerPixel",
                                 ResolutionParamInfo, ""),
    ENTRY(WM_GETICON),
    ENTRY(WM_SETICON),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCCREATE, nullptr, nullptr,
                                 CreateStructParamInfo, "createStruct"),
    ENTRY_WITH_NO_PARAM_INFO(WM_NCDESTROY),
    ENTRY_WITH_CUSTOM_PARAM_INFO(WM_NCCALCSIZE, WmNcCalcSizeParamInfo),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCHITTEST, nullptr, nullptr,
                                 XLowWordYHighWordParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCPAINT, HexParamInfo, "updateRegionHandle",
                                 nullptr, nullptr),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCACTIVATE, TrueFalseParamInfo,
                                 "isTitleBarOrIconActive", HexParamInfo,
                                 "updateRegion"),
    ENTRY(WM_GETDLGCODE),
    ENTRY(WM_SYNCPAINT),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCMOUSEMOVE, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCLBUTTONDOWN, HitTestParamInfo,
                                 "hitTestValue", PointsParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCLBUTTONUP, HitTestParamInfo,
                                 "hitTestValue", PointsParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCLBUTTONDBLCLK, HitTestParamInfo,
                                 "hitTestValue", PointsParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCRBUTTONDOWN, HitTestParamInfo,
                                 "hitTestValue", PointsParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCRBUTTONUP, HitTestParamInfo,
                                 "hitTestValue", PointsParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCRBUTTONDBLCLK, HitTestParamInfo,
                                 "hitTestValue", PointsParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCMBUTTONDOWN, HitTestParamInfo,
                                 "hitTestValue", PointsParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCMBUTTONUP, HitTestParamInfo,
                                 "hitTestValue", PointsParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCMBUTTONDBLCLK, HitTestParamInfo,
                                 "hitTestValue", PointsParamInfo, "mousePos"),
    ENTRY(EM_GETSEL),
    ENTRY(EM_SETSEL),
    ENTRY(EM_GETRECT),
    ENTRY(EM_SETRECT),
    ENTRY(EM_SETRECTNP),
    ENTRY(EM_SCROLL),
    ENTRY(EM_LINESCROLL),
    ENTRY(EM_SCROLLCARET),
    ENTRY(EM_GETMODIFY),
    ENTRY(EM_SETMODIFY),
    ENTRY(EM_GETLINECOUNT),
    ENTRY(EM_LINEINDEX),
    ENTRY(EM_SETHANDLE),
    ENTRY(EM_GETHANDLE),
    ENTRY(EM_GETTHUMB),
    ENTRY(EM_LINELENGTH),
    ENTRY(EM_REPLACESEL),
    ENTRY(EM_GETLINE),
    ENTRY(EM_LIMITTEXT),
    ENTRY(EM_CANUNDO),
    ENTRY(EM_UNDO),
    ENTRY(EM_FMTLINES),
    ENTRY(EM_LINEFROMCHAR),
    ENTRY(EM_SETTABSTOPS),
    ENTRY(EM_SETPASSWORDCHAR),
    ENTRY(EM_EMPTYUNDOBUFFER),
    ENTRY(EM_GETFIRSTVISIBLELINE),
    ENTRY(EM_SETREADONLY),
    ENTRY(EM_SETWORDBREAKPROC),
    ENTRY(EM_GETWORDBREAKPROC),
    ENTRY(EM_GETPASSWORDCHAR),
    ENTRY(EM_SETMARGINS),
    ENTRY(EM_GETMARGINS),
    ENTRY(EM_GETLIMITTEXT),
    ENTRY(EM_POSFROMCHAR),
    ENTRY(EM_CHARFROMPOS),
    ENTRY(EM_SETIMESTATUS),
    ENTRY(EM_GETIMESTATUS),
    ENTRY(SBM_SETPOS),
    ENTRY(SBM_GETPOS),
    ENTRY(SBM_SETRANGE),
    ENTRY(SBM_SETRANGEREDRAW),
    ENTRY(SBM_GETRANGE),
    ENTRY(SBM_ENABLE_ARROWS),
    ENTRY(SBM_SETSCROLLINFO),
    ENTRY(SBM_GETSCROLLINFO),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_KEYDOWN, VirtualKeyParamInfo, "vKey",
                                 KeystrokeFlagsParamInfo, ""),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_KEYUP, VirtualKeyParamInfo, "vKey",
                                 KeystrokeFlagsParamInfo, ""),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_CHAR, IntParamInfo, "charCode",
                                 KeystrokeFlagsParamInfo, ""),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_DEADCHAR, IntParamInfo, "charCode",
                                 KeystrokeFlagsParamInfo, ""),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_SYSKEYDOWN, VirtualKeyParamInfo, "vKey",
                                 KeystrokeFlagsParamInfo, ""),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_SYSKEYUP, VirtualKeyParamInfo, "vKey",
                                 KeystrokeFlagsParamInfo, ""),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_SYSCHAR, IntParamInfo, "charCode",
                                 KeystrokeFlagsParamInfo, ""),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_SYSDEADCHAR, IntParamInfo, "charCode",
                                 KeystrokeFlagsParamInfo, ""),
    ENTRY(WM_KEYLAST),
    ENTRY(WM_IME_STARTCOMPOSITION),
    ENTRY(WM_IME_ENDCOMPOSITION),
    ENTRY(WM_IME_COMPOSITION),
    ENTRY(WM_INITDIALOG),
    ENTRY(WM_COMMAND),
    ENTRY(WM_SYSCOMMAND),
    ENTRY(WM_TIMER),
    ENTRY(WM_HSCROLL),
    ENTRY(WM_VSCROLL),
    ENTRY(WM_INITMENU),
    ENTRY(WM_INITMENUPOPUP),
    ENTRY(WM_MENUSELECT),
    ENTRY(WM_MENUCHAR),
    ENTRY(WM_ENTERIDLE),
    ENTRY(WM_MENURBUTTONUP),
    ENTRY(WM_MENUDRAG),
    ENTRY(WM_MENUGETOBJECT),
    ENTRY(WM_UNINITMENUPOPUP),
    ENTRY(WM_MENUCOMMAND),
    ENTRY(WM_CHANGEUISTATE),
    ENTRY(WM_QUERYUISTATE),
    ENTRY(WM_UPDATEUISTATE),
    ENTRY(WM_CTLCOLORMSGBOX),
    ENTRY(WM_CTLCOLOREDIT),
    ENTRY(WM_CTLCOLORLISTBOX),
    ENTRY(WM_CTLCOLORBTN),
    ENTRY(WM_CTLCOLORDLG),
    ENTRY(WM_CTLCOLORSCROLLBAR),
    ENTRY(WM_CTLCOLORSTATIC),
    ENTRY(CB_GETEDITSEL),
    ENTRY(CB_LIMITTEXT),
    ENTRY(CB_SETEDITSEL),
    ENTRY(CB_ADDSTRING),
    ENTRY(CB_DELETESTRING),
    ENTRY(CB_DIR),
    ENTRY(CB_GETCOUNT),
    ENTRY(CB_GETCURSEL),
    ENTRY(CB_GETLBTEXT),
    ENTRY(CB_GETLBTEXTLEN),
    ENTRY(CB_INSERTSTRING),
    ENTRY(CB_RESETCONTENT),
    ENTRY(CB_FINDSTRING),
    ENTRY(CB_SELECTSTRING),
    ENTRY(CB_SETCURSEL),
    ENTRY(CB_SHOWDROPDOWN),
    ENTRY(CB_GETITEMDATA),
    ENTRY(CB_SETITEMDATA),
    ENTRY(CB_GETDROPPEDCONTROLRECT),
    ENTRY(CB_SETITEMHEIGHT),
    ENTRY(CB_GETITEMHEIGHT),
    ENTRY(CB_SETEXTENDEDUI),
    ENTRY(CB_GETEXTENDEDUI),
    ENTRY(CB_GETDROPPEDSTATE),
    ENTRY(CB_FINDSTRINGEXACT),
    ENTRY(CB_SETLOCALE),
    ENTRY(CB_GETLOCALE),
    ENTRY(CB_GETTOPINDEX),
    ENTRY(CB_SETTOPINDEX),
    ENTRY(CB_GETHORIZONTALEXTENT),
    ENTRY(CB_SETHORIZONTALEXTENT),
    ENTRY(CB_GETDROPPEDWIDTH),
    ENTRY(CB_SETDROPPEDWIDTH),
    ENTRY(CB_INITSTORAGE),
    ENTRY(CB_MSGMAX),
    ENTRY(LB_ADDSTRING),
    ENTRY(LB_INSERTSTRING),
    ENTRY(LB_DELETESTRING),
    ENTRY(LB_SELITEMRANGEEX),
    ENTRY(LB_RESETCONTENT),
    ENTRY(LB_SETSEL),
    ENTRY(LB_SETCURSEL),
    ENTRY(LB_GETSEL),
    ENTRY(LB_GETCURSEL),
    ENTRY(LB_GETTEXT),
    ENTRY(LB_GETTEXTLEN),
    ENTRY(LB_GETCOUNT),
    ENTRY(LB_SELECTSTRING),
    ENTRY(LB_DIR),
    ENTRY(LB_GETTOPINDEX),
    ENTRY(LB_FINDSTRING),
    ENTRY(LB_GETSELCOUNT),
    ENTRY(LB_GETSELITEMS),
    ENTRY(LB_SETTABSTOPS),
    ENTRY(LB_GETHORIZONTALEXTENT),
    ENTRY(LB_SETHORIZONTALEXTENT),
    ENTRY(LB_SETCOLUMNWIDTH),
    ENTRY(LB_ADDFILE),
    ENTRY(LB_SETTOPINDEX),
    ENTRY(LB_GETITEMRECT),
    ENTRY(LB_GETITEMDATA),
    ENTRY(LB_SETITEMDATA),
    ENTRY(LB_SELITEMRANGE),
    ENTRY(LB_SETANCHORINDEX),
    ENTRY(LB_GETANCHORINDEX),
    ENTRY(LB_SETCARETINDEX),
    ENTRY(LB_GETCARETINDEX),
    ENTRY(LB_SETITEMHEIGHT),
    ENTRY(LB_GETITEMHEIGHT),
    ENTRY(LB_FINDSTRINGEXACT),
    ENTRY(LB_SETLOCALE),
    ENTRY(LB_GETLOCALE),
    ENTRY(LB_SETCOUNT),
    ENTRY(LB_INITSTORAGE),
    ENTRY(LB_ITEMFROMPOINT),
    ENTRY(LB_MSGMAX),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_MOUSEMOVE, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_LBUTTONDOWN, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_LBUTTONUP, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_LBUTTONDBLCLK, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_RBUTTONDOWN, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_RBUTTONUP, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_RBUTTONDBLCLK, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_MBUTTONDOWN, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_MBUTTONUP, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_MBUTTONDBLCLK, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_MOUSEWHEEL,
                                 VirtualKeysLowWordDistanceHighWordParamInfo,
                                 "", XLowWordYHighWordParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_MOUSEHWHEEL,
                                 VirtualKeysLowWordDistanceHighWordParamInfo,
                                 "", XLowWordYHighWordParamInfo, "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_PARENTNOTIFY, ParentNotifyEventParamInfo,
                                 "", PointParamInfo, "pointerLocation"),
    ENTRY(WM_ENTERMENULOOP),
    ENTRY(WM_EXITMENULOOP),
    ENTRY(WM_NEXTMENU),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_SIZING, WindowEdgeParamInfo, "edge",
                                 RectParamInfo, "rect"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_CAPTURECHANGED, nullptr, nullptr,
                                 HexParamInfo, "windowHandle"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_MOVING, nullptr, nullptr, RectParamInfo,
                                 "rect"),
    ENTRY(WM_POWERBROADCAST),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_DEVICECHANGE, DeviceEventParamInfo, "event",
                                 HexParamInfo, "data"),
    ENTRY(WM_MDICREATE),
    ENTRY(WM_MDIDESTROY),
    ENTRY(WM_MDIACTIVATE),
    ENTRY(WM_MDIRESTORE),
    ENTRY(WM_MDINEXT),
    ENTRY(WM_MDIMAXIMIZE),
    ENTRY(WM_MDITILE),
    ENTRY(WM_MDICASCADE),
    ENTRY(WM_MDIICONARRANGE),
    ENTRY(WM_MDIGETACTIVE),
    ENTRY(WM_MDISETMENU),
    ENTRY(WM_ENTERSIZEMOVE),
    ENTRY(WM_EXITSIZEMOVE),
    ENTRY(WM_DROPFILES),
    ENTRY(WM_MDIREFRESHMENU),
    ENTRY(WM_IME_SETCONTEXT),
    ENTRY(WM_IME_NOTIFY),
    ENTRY(WM_IME_CONTROL),
    ENTRY(WM_IME_COMPOSITIONFULL),
    ENTRY(WM_IME_SELECT),
    ENTRY(WM_IME_CHAR),
    ENTRY(WM_IME_REQUEST),
    ENTRY(WM_IME_KEYDOWN),
    ENTRY(WM_IME_KEYUP),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_NCMOUSEHOVER, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_MOUSEHOVER, VirtualModifierKeysParamInfo,
                                 "virtualKeys", XLowWordYHighWordParamInfo,
                                 "mousePos"),
    ENTRY_WITH_NO_PARAM_INFO(WM_NCMOUSELEAVE),
    ENTRY_WITH_NO_PARAM_INFO(WM_MOUSELEAVE),
    ENTRY(WM_CUT),
    ENTRY(WM_COPY),
    ENTRY(WM_PASTE),
    ENTRY(WM_CLEAR),
    ENTRY(WM_UNDO),
    ENTRY(WM_RENDERFORMAT),
    ENTRY(WM_RENDERALLFORMATS),
    ENTRY(WM_DESTROYCLIPBOARD),
    ENTRY(WM_DRAWCLIPBOARD),
    ENTRY(WM_PAINTCLIPBOARD),
    ENTRY(WM_VSCROLLCLIPBOARD),
    ENTRY(WM_SIZECLIPBOARD),
    ENTRY(WM_ASKCBFORMATNAME),
    ENTRY(WM_CHANGECBCHAIN),
    ENTRY(WM_HSCROLLCLIPBOARD),
    ENTRY(WM_QUERYNEWPALETTE),
    ENTRY(WM_PALETTEISCHANGING),
    ENTRY(WM_PALETTECHANGED),
    ENTRY(WM_HOTKEY),
    ENTRY(WM_PRINT),
    ENTRY(WM_PRINTCLIENT),
    ENTRY(WM_THEMECHANGED),
    ENTRY(WM_HANDHELDFIRST),
    ENTRY(WM_HANDHELDLAST),
    ENTRY(WM_AFXFIRST),
    ENTRY(WM_AFXLAST),
    ENTRY(WM_PENWINFIRST),
    ENTRY(WM_PENWINLAST),
    ENTRY(WM_APP),
    ENTRY_WITH_NO_PARAM_INFO(WM_DWMCOMPOSITIONCHANGED),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_DWMNCRENDERINGCHANGED, TrueFalseParamInfo,
                                 "DwmNcRendering", nullptr, nullptr),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_DWMCOLORIZATIONCOLORCHANGED, HexParamInfo,
                                 "color:AARRGGBB", TrueFalseParamInfo,
                                 "isOpaque"),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_DWMWINDOWMAXIMIZEDCHANGE,
                                 TrueFalseParamInfo, "maximized", nullptr,
                                 nullptr),
    ENTRY(WM_DWMSENDICONICTHUMBNAIL),  // lParam: HIWORD is x, LOWORD is y
    ENTRY_WITH_NO_PARAM_INFO(WM_DWMSENDICONICLIVEPREVIEWBITMAP),
    ENTRY(WM_TABLET_QUERYSYSTEMGESTURESTATUS),
    ENTRY(WM_GESTURE),
    ENTRY(WM_GESTURENOTIFY),
    ENTRY(WM_GETTITLEBARINFOEX),
    ENTRY_WITH_SPLIT_PARAM_INFOS(WM_DPICHANGED, XLowWordYHighWordParamInfo,
                                 "newDPI", RectParamInfo,
                                 "suggestedSizeAndPos"),
};
#undef ENTRY
#undef ENTRY_WITH_NO_PARAM_INFO
#undef ENTRY_WITH_CUSTOM_PARAM_INFO
#undef ENTRY_WITH_SPLIT_PARAM_INFO

}  // namespace mozilla::widget

#ifdef DEBUG
void DDError(const char* msg, HRESULT hr) {
  /*XXX make nicer */
  MOZ_LOG(gWindowsLog, LogLevel::Error,
          ("DirectDraw error %s: 0x%08lx\n", msg, hr));
}
#endif

#ifdef DEBUG_VK
bool is_vk_down(int vk) {
  SHORT st = GetKeyState(vk);
#  ifdef DEBUG
  MOZ_LOG(gWindowsLog, LogLevel::Info, ("is_vk_down vk=%x st=%x\n", vk, st));
#  endif
  return (st < 0);
}
#endif
