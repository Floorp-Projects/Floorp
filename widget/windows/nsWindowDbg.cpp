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

// if result is not set, this is used to log the parameters passed in the
// message, otherwise we are logging the parameters after we have handled the
// message. This is useful for events where we might change the parameters while
// handling the message (for example WM_GETTEXT and WM_NCCALCSIZE)
// Returns whether this message was logged, so we need to reserve a
// counter number for it.
bool PrintEvent::PrintEventInternal() {
  const auto eventMsgInfo = gAllEvents.find(mMsg);
  const char* msgText =
      eventMsgInfo != gAllEvents.end() ? eventMsgInfo->second.mStr : nullptr;

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
    const char* resultMsg = mResult.isSome()
                                ? (mResult.value() ? "true" : "false")
                                : "initial call";
    MOZ_LOG(
        gWindowsEventLog, LogLevel::Info,
        ("%6ld - 0x%04X (0x%08llX 0x%08llX) %s: 0x%08llX (%s)\n",
         mEventCounter.valueOr(gEventCounter), mMsg,
         static_cast<uint64_t>(mWParam), static_cast<uint64_t>(mLParam),
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
