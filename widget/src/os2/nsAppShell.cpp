/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Lowe <michael.lowe@bigfoot.com>
 *   John Fairhurst <john_fairhurst@iname.com>
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

#include "nsAppShell.h"
#include "nsToolkit.h"
#include "nsThreadUtils.h"

static UINT sMsgId;

//-------------------------------------------------------------------------

MRESULT EXPENTRY EventWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  if (msg == sMsgId) {
    nsAppShell *as = reinterpret_cast<nsAppShell *>(mp2);
    as->NativeEventCallback();
    NS_RELEASE(as);
    return (MRESULT)TRUE;
  }
  return WinDefWindowProc(hwnd, msg, mp1, mp2);
}

nsAppShell::~nsAppShell()
{
  if (mEventWnd) {
    // DestroyWindow doesn't do anything when called from a non UI thread.
    // Since mEventWnd was created on the UI thread, it must be destroyed on
    // the UI thread.
    WinSendMsg(mEventWnd, WM_CLOSE, 0, 0);
  }
}

nsresult
nsAppShell::Init()
{
  // a message queue is required to create a window but
  // it is not necessarily created yet
  if (WinQueryQueueInfo(HMQ_CURRENT, NULL, 0) == FALSE) {
    // Set our app to be a PM app before attempting Win calls
    PPIB ppib;
    PTIB ptib;
    DosGetInfoBlocks(&ptib, &ppib);
    ppib->pib_ultype = 3;

    HAB hab = WinInitialize(0);
    WinCreateMsgQueue(hab, 0);
  }

  if (!sMsgId) {
    sMsgId = WinAddAtom( WinQuerySystemAtomTable(), "nsAppShell:EventID");
    WinRegisterClass((HAB)0, "nsAppShell:EventWindowClass", EventWindowProc, NULL, 0);
  }

  mEventWnd = ::WinCreateWindow(HWND_DESKTOP,
                                "nsAppShell:EventWindowClass",
                                "nsAppShell:EventWindow",
                                0,
                                0, 0,
                                10, 10,
                                HWND_DESKTOP,
                                HWND_BOTTOM,
                                0, 0, 0);
  NS_ENSURE_STATE(mEventWnd);

  return nsBaseAppShell::Init();
}

void
nsAppShell::ScheduleNativeEventCallback()
{
  // post a message to the native event queue...
  NS_ADDREF_THIS();
  WinPostMsg(mEventWnd, sMsgId, 0, reinterpret_cast<MPARAM>(this));
}

PRBool
nsAppShell::ProcessNextNativeEvent(PRBool mayWait)
{
  PRBool gotMessage = PR_FALSE;

  do {
    QMSG qmsg;
    // Give priority to system messages (in particular keyboard, mouse, timer,
    // and paint messages).
    if (WinPeekMsg((HAB)0, &qmsg, NULL, WM_CHAR, WM_VIOCHAR, PM_REMOVE) ||
        WinPeekMsg((HAB)0, &qmsg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) || 
        WinPeekMsg((HAB)0, &qmsg, NULL, 0, WM_USER-1, PM_REMOVE) || 
        WinPeekMsg((HAB)0, &qmsg, NULL, 0, 0, PM_REMOVE)) {
      gotMessage = PR_TRUE;
      ::WinDispatchMsg((HAB)0, &qmsg);
    } else if (mayWait) {
      // Block and wait for any posted application message
      ::WinWaitMsg((HAB)0, 0, 0);
    }
  } while (!gotMessage && mayWait);

  return gotMessage;
}
