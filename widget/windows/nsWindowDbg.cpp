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

using namespace mozilla::widget;

extern mozilla::LazyLogModule gWindowsLog;

#if defined(POPUP_ROLLUP_DEBUG_OUTPUT)
MSGFEventMsgInfo gMSGFEvents[] = {
  "MSGF_DIALOGBOX",      0,
  "MSGF_MESSAGEBOX",     1,
  "MSGF_MENU",           2,
  "MSGF_SCROLLBAR",      5,
  "MSGF_NEXTWINDOW",     6,
  "MSGF_MAX",            8,
  "MSGF_USER",           4096,
  nullptr, 0};
#endif

static long gEventCounter = 0;
static long gLastEventMsg = 0;

void PrintEvent(UINT msg, bool aShowAllEvents, bool aShowMouseMoves)
{
  int inx = 0;
  while (gAllEvents[inx].mId != msg && gAllEvents[inx].mStr != nullptr) {
    inx++;
  }
  if (aShowAllEvents || (!aShowAllEvents && gLastEventMsg != (long)msg)) {
    if (aShowMouseMoves || (!aShowMouseMoves && msg != 0x0020 && msg != 0x0200 && msg != 0x0084)) {
      MOZ_LOG(gWindowsLog, LogLevel::Info, 
             ("%6d - 0x%04X %s\n", gEventCounter++, msg, 
              gAllEvents[inx].mStr ? gAllEvents[inx].mStr : "Unknown"));
      gLastEventMsg = msg;
    }
  }
}

#ifdef DEBUG
void DDError(const char *msg, HRESULT hr)
{
  /*XXX make nicer */
  MOZ_LOG(gWindowsLog, LogLevel::Error,
         ("direct draw error %s: 0x%08lx\n", msg, hr));
}
#endif

#ifdef DEBUG_VK
bool is_vk_down(int vk)
{
   SHORT st = GetKeyState(vk);
#ifdef DEBUG
   MOZ_LOG(gWindowsLog, LogLevel::Info, ("is_vk_down vk=%x st=%x\n",vk, st));
#endif
   return (st < 0);
}
#endif
