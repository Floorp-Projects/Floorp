/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
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

// Enable heap debug dump message handling
//#define HEAP_DUMP_EVENT

// Enable window size and state debug output
//#define WINSTATE_DEBUG_OUTPUT

// nsIWidget defines a set of debug output statements
// that are called in various places within the code.
//#define WIDGET_DEBUG_OUTPUT

// Enable IS_VK_DOWN debug output
//#define DEBUG_VK

// Main event loop debug output flags
#if defined(EVENT_DEBUG_OUTPUT)
#define SHOW_REPEAT_EVENTS      PR_TRUE
#define SHOW_MOUSEMOVE_EVENTS   PR_FALSE
#endif // defined(EVENT_DEBUG_OUTPUT)

#if defined(POPUP_ROLLUP_DEBUG_OUTPUT) || defined(EVENT_DEBUG_OUTPUT)
void PrintEvent(UINT msg, PRBool aShowAllEvents, PRBool aShowMouseMoves);
#endif // defined(POPUP_ROLLUP_DEBUG_OUTPUT) || defined(EVENT_DEBUG_OUTPUT)

#if defined(POPUP_ROLLUP_DEBUG_OUTPUT)
typedef struct {
  char * mStr;
  int    mId;
} MSGFEventMsgInfo;

#define DISPLAY_NMM_PRT(_arg) printf((_arg));
#else
#define DISPLAY_NMM_PRT(_arg)
#endif // defined(POPUP_ROLLUP_DEBUG_OUTPUT)

#if defined(HEAP_DUMP_EVENT)
void InitHeapDump();
nsresult HeapDump(UINT msg, WPARAM wParam, LPARAM lParam);
UINT GetHeapMsg();
#endif // defined(HEAP_DUMP_EVENT)

#if defined(DEBUG)
void DDError(const char *msg, HRESULT hr);
#endif // defined(DEBUG)

#if defined(DEBUG_VK)
PRBool is_vk_down(int vk);
#define IS_VK_DOWN is_vk_down
#else
#define IS_VK_DOWN(a) (GetKeyState(a) < 0)
#endif // defined(DEBUG_VK)

#endif /* WindowDbg_h__ */