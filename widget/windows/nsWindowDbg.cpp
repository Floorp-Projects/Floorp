/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsWindowDbg - Debug related utilities for nsWindow.
 */

#include "mozilla/Logging.h"
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

// Size of WPARAM, LPARAM, and LRESULT depends on 32 or 64 bit build,
// we use 64 bit types here to ensure it works in either case.
void PrintEvent(UINT msg, uint64_t wParam, uint64_t lParam, uint64_t retValue,
                bool result, bool aShowAllEvents, bool aShowMouseMoves) {
  const auto eventMsgInfo = gAllEvents.find(msg);
  const char* msgText =
      eventMsgInfo != gAllEvents.end() ? eventMsgInfo->second.mStr : nullptr;

  if (aShowAllEvents || (gLastEventMsg != msg)) {
    if (aShowMouseMoves ||
        (msg != WM_SETCURSOR && msg != WM_MOUSEMOVE && msg != WM_NCHITTEST)) {
      MOZ_LOG(
          gWindowsEventLog, LogLevel::Info,
          ("%6ld - 0x%04X (0x%08llX 0x%08llX) %s: 0x%08llX (%s)\n",
           gEventCounter++, msg, wParam, lParam, msgText ? msgText : "Unknown",
           retValue, result ? "true" : "false"));
      gLastEventMsg = msg;
    }
  }
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
