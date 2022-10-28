/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsWindowDbg - Debug related utilities for nsWindow.
 */

#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "nsWindowDbg.h"
#include "WinUtils.h"

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

namespace mozilla::widget {
nsAutoCString DefaultParamInfoFn(uint64_t wParam, uint64_t lParam,
                                 bool /* isPreCall */) {
  nsAutoCString result;
  result.AppendPrintf("(0x%08llX 0x%08llX)", wParam, lParam);
  return result;
}

nsAutoCString EmptyParamInfoFn(uint64_t wParam, uint64_t lParam,
                               bool /* isPreCall */) {
  nsAutoCString result;
  return result;
}

void AppendEnumValueInfo(
    nsAutoCString& str, uint64_t value,
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

bool AppendFlagsInfo(nsAutoCString& str, uint64_t flags,
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
}  // namespace mozilla::widget

PrintEvent::PrintEvent(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT retValue)
    : mMsg(msg),
      mWParam(wParam),
      mLParam(lParam),
      mRetValue(retValue),
      mResult(mozilla::Nothing()),
      mShouldLogPostCall(false) {
  if (PrintEventInternal()) {
    // this event was logged, so reserve this counter number for the post-call
    mEventCounter = mozilla::Some(gEventCounter);
    ++gEventCounter;
  }
}

PrintEvent::~PrintEvent() {
  // If mResult is Nothing, perhaps an exception was thrown or something
  // before SetResult() was supposed to be called.
  if (mResult.isSome()) {
    if (PrintEventInternal() && mEventCounter.isNothing()) {
      // We didn't reserve a counter in the pre-call, so reserve it here.
      ++gEventCounter;
    }
  }
}

void EventMsgInfo::LogParameters(nsAutoCString& str, WPARAM wParam,
                                 LPARAM lParam, bool isPreCall) {
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

// if mResult is not set, this is used to log the parameters passed in the
// message, otherwise we are logging the parameters after we have handled the
// message. This is useful for events where we might change the parameters while
// handling the message (for example WM_GETTEXT and WM_NCCALCSIZE)
// Returns whether this message was logged, so we need to reserve a
// counter number for it.
bool PrintEvent::PrintEventInternal() {
  mozilla::LogLevel logLevel = (*gWindowsEventLog).Level();
  bool isPreCall = mResult.isNothing();
  if (isPreCall || mShouldLogPostCall) {
    if (isPreCall) {
      bool shouldLogAtAll =
          logLevel >= mozilla::LogLevel::Info &&
          (logLevel >= mozilla::LogLevel::Debug || (gLastEventMsg != mMsg)) &&
          (logLevel >= mozilla::LogLevel::Verbose ||
           (mMsg != WM_SETCURSOR && mMsg != WM_MOUSEMOVE &&
            mMsg != WM_NCHITTEST));
      // Since calling mParamInfoFn() allocates a string, only go down this code
      // path if we're going to log this message to reduce allocations.
      if (!shouldLogAtAll) {
        return false;
      }
      mShouldLogPostCall = true;
      bool shouldLogPreCall = gEventsToLogOriginalParams.has(mMsg);
      if (!shouldLogPreCall) {
        // Pre-call and we don't want to log both, so skip this one.
        return false;
      }
    }
    const auto& eventMsgInfo = gAllEvents.find(mMsg);
    const char* msgText =
        eventMsgInfo != gAllEvents.end() ? eventMsgInfo->second.mStr : nullptr;
    nsAutoCString paramInfo;
    if (eventMsgInfo != gAllEvents.end()) {
      eventMsgInfo->second.LogParameters(paramInfo, mWParam, mLParam,
                                         isPreCall);
    } else {
      paramInfo = DefaultParamInfoFn(mWParam, mLParam, isPreCall);
    }
    const char* resultMsg = mResult.isSome()
                                ? (mResult.value() ? "true" : "false")
                                : "initial call";
    MOZ_LOG(
        gWindowsEventLog, LogLevel::Info,
        ("%6ld - 0x%04X %s %s: 0x%08llX (%s)\n",
         mEventCounter.valueOr(gEventCounter), mMsg, paramInfo.get(),
         msgText ? msgText : "Unknown",
         mResult.isSome() ? static_cast<uint64_t>(mRetValue) : 0, resultMsg));
    gLastEventMsg = mMsg;
    return true;
  }
  return false;
}

#ifdef DEBUG
void DDError(const char* msg, HRESULT hr) {
  /*XXX make nicer */
  MOZ_LOG(gWindowsLog, LogLevel::Error,
          ("direct draw error %s: 0x%08lx\n", msg, hr));
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
