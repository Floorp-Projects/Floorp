/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Michael Lowe <michael.lowe@bigfoot.com>
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
#include <windows.h>

// unknwn.h is needed to build with WIN32_LEAN_AND_MEAN
#include <unknwn.h>

#include "nsWidgetsCID.h"
#ifdef MOZ_AIMM
#include "aimm.h"
#endif

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell) 

static int gKeepGoing = 1;
//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
{ 
  mDispatchListener = 0;
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
NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener) 
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

#include "nsITimerManager.h"

BOOL PeekKeyAndIMEMessage(LPMSG msg, HWND hwnd)
{
  MSG msg1, msg2, *lpMsg;
  BOOL b1, b2;
#ifdef MOZ_UNICODE
  b1 = nsToolkit::mPeekMessage(&msg1, NULL, WM_KEYFIRST, WM_IME_KEYLAST, PM_NOREMOVE);
  b2 = nsToolkit::mPeekMessage(&msg2, NULL, WM_IME_SETCONTEXT, WM_IME_KEYUP, PM_NOREMOVE);
#else
  b1 = ::PeekMessage(&msg1, NULL, WM_KEYFIRST, WM_IME_KEYLAST, PM_NOREMOVE);
  b2 = ::PeekMessage(&msg2, NULL, WM_IME_SETCONTEXT, WM_IME_KEYUP, PM_NOREMOVE);
#endif /* MOZ_UNICODE */
  if (b1 || b2) {
    if (b1 && b2) {
      if (msg1.time < msg2.time)
        lpMsg = &msg1;
      else
        lpMsg = &msg2;
    } else if (b1)
      lpMsg = &msg1;
    else
      lpMsg = &msg2;
#ifdef MOZ_UNICODE
    return nsToolkit::mPeekMessage(msg, hwnd, lpMsg->message, lpMsg->message, PM_REMOVE);
#else
    return ::PeekMessage(msg, hwnd, lpMsg->message, lpMsg->message, PM_REMOVE);
#endif /* MOZ_UNICODE */
  }

  return false;
}


NS_METHOD nsAppShell::Run(void)
{
  NS_ADDREF_THIS();
  MSG  msg;
  int  keepGoing = 1;

  nsresult rv;
  nsCOMPtr<nsITimerManager> timerManager(do_GetService("@mozilla.org/timer/manager;1", &rv));
  if (NS_FAILED(rv)) return rv;

  timerManager->SetUseIdleTimers(PR_TRUE);

  gKeepGoing = 1;
  // Process messages
  do {
    // Give priority to system messages (in particular keyboard, mouse,
    // timer, and paint messages).
     if (PeekKeyAndIMEMessage(&msg, NULL) ||
#ifdef MOZ_UNICODE
         nsToolkit::mPeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) || 
         nsToolkit::mPeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
#else
         ::PeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) || 
         ::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
#endif /* MOZ_UNICODE */
      keepGoing = (msg.message != WM_QUIT);

      if (keepGoing != 0) {
//#ifdef MOZ_AIMM // not need?
//      if (!nsToolkit::gAIMMMsgPumpOwner || (nsToolkit::gAIMMMsgPumpOwner->OnTranslateMessage(&msg) != S_OK))
//#endif
        TranslateMessage(&msg);
#ifdef MOZ_UNICODE
        nsToolkit::mDispatchMessage(&msg);
#else
        ::DispatchMessage(&msg);
#endif /* MOZ_UNICODE */
        if (mDispatchListener)
          mDispatchListener->AfterDispatch();
      }
    } else {

      PRBool hasTimers;
      timerManager->HasIdleTimers(&hasTimers);
      if (hasTimers) {
        do {
          timerManager->FireNextIdleTimer();
          timerManager->HasIdleTimers(&hasTimers);
#ifdef MOZ_UNICODE
        } while (hasTimers && !nsToolkit::mPeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE));
#else
        } while (hasTimers && !::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE));
#endif
      } else {

        if (!gKeepGoing) {
          // In this situation, PostQuitMessage() was called, but the WM_QUIT
          // message was removed from the event queue by someone else -
          // (see bug #54725).  So, just exit the loop as if WM_QUIT had been
          // reeceived...
          keepGoing = 0;
        } else {
          // Block and wait for any posted application message
          ::WaitMessage();
        }
      }
    }

  } while (keepGoing != 0);
  Release();
  return msg.wParam;
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
  static MSG msg;

  BOOL gotMessage = false;

  nsresult rv;
  nsCOMPtr<nsITimerManager> timerManager(do_GetService("@mozilla.org/timer/manager;1", &rv));
  if (NS_FAILED(rv)) return rv;

  do {
    // Give priority to system messages (in particular keyboard, mouse,
    // timer, and paint messages).
     if (PeekKeyAndIMEMessage(&msg, NULL) ||
#ifdef MOZ_UNICODE
        nsToolkit::mPeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) || 
        nsToolkit::mPeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
#else
        ::PeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) || 
        ::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
#endif /* MOZ_UNICODE */
      gotMessage = true;
    } else {
      PRBool hasTimers;
      timerManager->HasIdleTimers(&hasTimers);
      if (hasTimers) {
        do {
          timerManager->FireNextIdleTimer();
          timerManager->HasIdleTimers(&hasTimers);
#ifdef MOZ_UNICODE
        } while (hasTimers && !nsToolkit::mPeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE));
#else
        } while (hasTimers && !::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE));
#endif /* MOZ_UNICODE */
      } else {
        // Block and wait for any posted application message
        ::WaitMessage();
      }
    }

  } while (!gotMessage);

#ifdef DEBUG_danm
  if (msg.message != WM_TIMER)
    printf("-> %d", msg.message);
#endif

//#ifdef MOZ_AIMM // not need?
//  if (!nsToolkit::gAIMMMsgPumpOwner || (nsToolkit::gAIMMMsgPumpOwner->OnTranslateMessage(&msg) != S_OK))
//#endif
  TranslateMessage(&msg);
  aEvent = &msg;
  aRealEvent = PR_TRUE;
  return NS_OK;
}

nsresult nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  DispatchMessage((MSG *)aEvent);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit(void)
{
  PostQuitMessage(0);
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

