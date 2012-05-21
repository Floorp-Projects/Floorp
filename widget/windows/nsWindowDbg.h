/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WindowDbg_h__
#define WindowDbg_h__

/*
 * nsWindowDbg - Debug related utilities for nsWindow.
 */

#include "nsWindowDefs.h"

// Enabled main event loop debug event output
//#define EVENT_DEBUG_OUTPUT

// Enables debug output for popup rollup hooks
//#define POPUP_ROLLUP_DEBUG_OUTPUT

// Enable window size and state debug output
//#define WINSTATE_DEBUG_OUTPUT

// nsIWidget defines a set of debug output statements
// that are called in various places within the code.
//#define WIDGET_DEBUG_OUTPUT

// Enable IS_VK_DOWN debug output
//#define DEBUG_VK

// Main event loop debug output flags
#if defined(EVENT_DEBUG_OUTPUT)
#define SHOW_REPEAT_EVENTS      true
#define SHOW_MOUSEMOVE_EVENTS   false
#endif // defined(EVENT_DEBUG_OUTPUT)

#if defined(POPUP_ROLLUP_DEBUG_OUTPUT) || defined(EVENT_DEBUG_OUTPUT) || 1
void PrintEvent(UINT msg, bool aShowAllEvents, bool aShowMouseMoves);
#endif // defined(POPUP_ROLLUP_DEBUG_OUTPUT) || defined(EVENT_DEBUG_OUTPUT)

#if defined(POPUP_ROLLUP_DEBUG_OUTPUT)
typedef struct {
  char * mStr;
  int    mId;
} MSGFEventMsgInfo;

#define DISPLAY_NMM_PRT(_arg) PR_LOG(gWindowsLog, PR_LOG_ALWAYS, ((_arg)));
#else
#define DISPLAY_NMM_PRT(_arg)
#endif // defined(POPUP_ROLLUP_DEBUG_OUTPUT)

#if defined(DEBUG)
void DDError(const char *msg, HRESULT hr);
#endif // defined(DEBUG)

#if defined(DEBUG_VK)
bool is_vk_down(int vk);
#define IS_VK_DOWN is_vk_down
#else
#define IS_VK_DOWN(a) (GetKeyState(a) < 0)
#endif // defined(DEBUG_VK)

#endif /* WindowDbg_h__ */
