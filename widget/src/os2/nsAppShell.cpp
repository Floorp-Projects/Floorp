/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *  Michael Lowe <michael.lowe@bigfoot.com>
 *  John Fairhurst <john_fairhurst@iname.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAppShell.h"
#include "nsToolkit.h"
#include "nsIWidget.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"

#include "nsWidgetsCID.h"

NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell) 

static int gKeepGoing = 1;
//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
{ 
}



//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int* argc, char ** argv)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

#include "nsITimerManager.h"

NS_METHOD nsAppShell::Run(void)
{
  NS_ADDREF_THIS();
  QMSG  qmsg;
  int  keepGoing = 1;

  nsresult rv;
  nsCOMPtr<nsITimerManager> timerManager(do_GetService("@mozilla.org/timer/manager;1", &rv));
  if (NS_FAILED(rv)) return rv;

  // Using idle timers breaks drag drop, so turn it off for now
  timerManager->SetUseIdleTimers(PR_FALSE);

  gKeepGoing = 1;
  // Process messages
  do {
    // Give priority to system messages (in particular keyboard, mouse,
    // timer, and paint messages).
    if (WinPeekMsg((HAB)0, &qmsg, NULL, WM_CHAR, WM_VIOCHAR, PM_REMOVE) ||
        WinPeekMsg((HAB)0, &qmsg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) ||
        WinPeekMsg((HAB)0, &qmsg, NULL, 0, WM_USER-1, PM_REMOVE) ||
        WinPeekMsg((HAB)0, &qmsg, NULL, 0, 0, PM_REMOVE)) {
      keepGoing = (qmsg.msg != WM_QUIT);

      if (keepGoing != 0) {
        WinDispatchMsg((HAB)0, &qmsg);
      } else {
        if ((qmsg.hwnd) && (qmsg.mp1 || qmsg.mp2)) {
          // If close is selected from the task list, only close
          // that window, not the entire application
          WinSendMsg(qmsg.hwnd, WM_SYSCOMMAND, MPFROMSHORT(SC_CLOSE), 0);
          keepGoing = 1;
        }
      }
    } else {

      PRBool hasTimers;
      timerManager->HasIdleTimers(&hasTimers);
      if (hasTimers) {
        do {
          timerManager->FireNextIdleTimer();
          timerManager->HasIdleTimers(&hasTimers);
        } while (hasTimers && WinPeekMsg((HAB)0, &qmsg, NULL, 0, 0, PM_NOREMOVE));
      } else {

        if (!gKeepGoing) {
          // In this situation, PostQuitMessage() was called, but the WM_QUIT
          // message was removed from the event queue by someone else -
          // (see bug #54725).  So, just exit the loop as if WM_QUIT had been
          // reeceived...
          keepGoing = 0;
          printf("here\n");
        } else {
          // Block and wait for any posted application message
          ::WinWaitMsg((HAB)0, 0, 0);
        }
      }
    }

  } while (keepGoing != 0);
  Release();
  return NS_OK;
}

inline NS_METHOD nsAppShell::Spinup(void)
{ return NS_OK; }

inline NS_METHOD nsAppShell::Spindown(void)
{ return NS_OK; }

inline NS_METHOD nsAppShell::ListenToEventQueue(nsIEventQueue * aQueue, PRBool aListen)
{ return NS_OK; }

NS_METHOD
nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  static QMSG qmsg;

  PRBool gotMessage = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsITimerManager> timerManager(do_GetService("@mozilla.org/timer/manager;1", &rv));
  if (NS_FAILED(rv)) return rv;

  do {
    // Give priority to system messages (in particular keyboard, mouse,
    // timer, and paint messages).
    if (WinPeekMsg((HAB)0, &qmsg, NULL, WM_CHAR, WM_VIOCHAR, PM_REMOVE) ||
        WinPeekMsg((HAB)0, &qmsg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) || 
        WinPeekMsg((HAB)0, &qmsg, NULL, 0, WM_USER-1, PM_REMOVE) || 
        WinPeekMsg((HAB)0, &qmsg, NULL, 0, 0, PM_REMOVE)) {
      gotMessage = PR_TRUE;
    } else {
      PRBool hasTimers;
      timerManager->HasIdleTimers(&hasTimers);
      if (hasTimers) {
        do {
          timerManager->FireNextIdleTimer();
          timerManager->HasIdleTimers(&hasTimers);
        } while (hasTimers && WinPeekMsg((HAB)0, &qmsg, NULL, 0, 0, PM_NOREMOVE));
      } else {
        // Block and wait for any posted application message
        ::WinWaitMsg((HAB)0, 0, 0);
      }
    }

  } while (gotMessage == PR_FALSE);

#ifdef DEBUG_danm
  if (qmsg.msg != WM_TIMER)
    printf("-> %d", qmsg.msg);
#endif

  aEvent = &qmsg;
  aRealEvent = PR_TRUE;
  return NS_OK;
}

nsresult nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  WinDispatchMsg((HAB)0, (QMSG *)aEvent);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit(void)
{
  WinPostQueueMsg(HMQ_CURRENT, WM_QUIT, 0, 0);
  //
  // Also, set a global flag, just in case someone eats the WM_QUIT message.
  // see bug #54725.
  //
  gKeepGoing = 0;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
}

