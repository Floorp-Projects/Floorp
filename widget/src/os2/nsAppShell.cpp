/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 */

#include "nsAppShell.h"
#include "nsToolkit.h"
#include "nsIWidget.h"
#include "nsHashtable.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsITimer.h"
#include "nsITimerQueue.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kTimerManagerCID, NS_TIMERMANAGER_CID);

// Appshell manager.  Same threads must get the same appshell object,
// or else the message queue will be taken over by the second (nested)
// appshell and subsequently killed when it does.  This is bad because
// the main (first) appshell should still be running.

TID QueryCurrentTID();

class nsAppshellManager
{
   HMTX         mLock;
   nsHashtable *mTable;

   struct ThreadKey : public nsVoidKey
   {
      ThreadKey() : nsVoidKey( (void*) QueryCurrentTID()) {}
   };

 public:
   nsAppshellManager()
   {
      mTable = new nsHashtable;
      DosCreateMutexSem( 0, &mLock, 0, FALSE /*unowned*/);
   }

  ~nsAppshellManager()
   {
      DosCloseMutexSem( mLock);
      delete mTable;
   }

   nsIAppShell *GetAppshell()
   {
      ThreadKey key;

      DosRequestMutexSem( mLock, SEM_INDEFINITE_WAIT);

      nsIAppShell *pShell = (nsIAppShell*) mTable->Get( &key);

      if( nsnull == pShell)
      {
         pShell = new nsAppShell;
         mTable->Put( &key, (void*) pShell);
      }

      DosReleaseMutexSem( mLock);

      return pShell;
   }

   void RemoveAppshell( nsIAppShell *aShell)
   {
      ThreadKey key;

      DosRequestMutexSem( mLock, SEM_INDEFINITE_WAIT);

      nsIAppShell *pShell = (nsIAppShell*) mTable->Get( &key);

      if( pShell != aShell)
         printf( "Appshell object dying in a foreign thread\n");
      else
         mTable->Remove( &key);

      DosReleaseMutexSem( mLock);
   }
};

static nsAppshellManager *pManager = nsnull;

NS_IMPL_ISUPPORTS(nsAppShell, NS_GET_IID(nsIAppShell))

// nsAppShell constructor
nsAppShell::nsAppShell()
{ 
   NS_INIT_REFCNT();
   mDispatchListener = 0;
   mHab = 0; mHmq = 0;
   mQuitNow = FALSE;
   memset( &mQmsg, 0, sizeof mQmsg);
}

nsresult nsAppShell::SetDispatchListener( nsDispatchListener *aDispatchListener) 
{
   mDispatchListener = aDispatchListener;
   return NS_OK;
}

// Create the application shell
nsresult nsAppShell::Create( int */*argc*/, char **/*argv*/)
{
   // It is possible that there already is a HAB/HMQ for this thread.
   // This condition arises when an event queue (underlying, PLEvent) is
   // created before the appshell.
   // When this happens, we must take over ownership of these existing
   // objects (and so destroy them when we go, too).

   if( FALSE == WinQueryQueueInfo( HMQ_CURRENT, 0, 0))
   {
      mHab = WinInitialize( 0);
      mHmq = WinCreateMsgQueue( mHab, 0);
   }
   else
   {
      mHab = 0; // HABs don't actually seem to be checked, ever.
                // But 0 == (current pid,current tid)
      mHmq = HMQ_CURRENT;
   }

   return NS_OK;
}

// Enter a message handler loop
nsresult nsAppShell::Run()
{
   if( !mHmq) return NS_ERROR_FAILURE; // (mHab = 0 is okay)

   NS_ADDREF_THIS();
   int  keepGoing = 1;
  
#if 0
   nsresult rv;
   NS_WITH_SERVICE(nsITimerQueue, queue, kTimerManagerCID, &rv);
   if (NS_FAILED(rv)) return rv;
#endif

   // Process messages
   do {
      // Give priority to system messages (in particular keyboard, mouse,
      // timer, and paint messages).
      if (WinPeekMsg((HAB)0, &mQmsg, NULL, WM_CHAR, WM_CHAR, PM_REMOVE) ||
          WinPeekMsg((HAB)0, &mQmsg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) ||
          WinPeekMsg((HAB)0, &mQmsg, NULL, 0, WM_USER-1, PM_REMOVE) ||
          WinPeekMsg((HAB)0, &mQmsg, NULL, 0, 0, PM_REMOVE)) {

         if (mQmsg.msg != WM_QUIT) {
            WinDispatchMsg((HAB)0, &mQmsg);
            if (mDispatchListener)
               mDispatchListener->AfterDispatch();
         } else {
            if( mQmsg.hwnd) {
               // send WM_SYSCOMMAND, SC_CLOSE to window (tasklist close)
               WinSendMsg( mQmsg.hwnd, WM_SYSCOMMAND, MPFROMSHORT(SC_CLOSE), 0);
            } else {
               if( mQuitNow)     // Don't want to close the app when a window 
                  keepGoing = 0; // is closed, just when our `Exit' is called.
            }
         }

#if 0
      // process timer queue.
      } else if (queue->HasReadyTimers(NS_PRIORITY_LOWEST)) {

         do {
            queue->FireNextReadyTimer(NS_PRIORITY_LOWEST);
         } while (queue->HasReadyTimers(NS_PRIORITY_LOWEST) && 
                  !WinPeekMsg((HAB)0, &mQmsg, NULL, 0, 0, PM_NOREMOVE));
#endif      
      } else {
         // Block and wait for any posted application message
         WinWaitMsg((HAB)0, 0, 0);
      }

   } while (keepGoing != 0);

   // reset mQuitNow flag for re-entrant appshells
   mQuitNow = FALSE;

   Release();
   return NS_OK;
}

// GetNativeData - return the HMQ for NS_NATIVE_SHELL
void *nsAppShell::GetNativeData( PRUint32 aDataType)
{
   void *rc = 0;

   switch( aDataType)
   {
      case NS_NATIVE_SHELL: rc = (void*) mHmq; break;
   }

   return rc;
}

// Exit a message handler loop
nsresult nsAppShell::Exit()
{
   NS_ASSERTION( mQuitNow == FALSE, "Double AppShell::Exit");
   mQuitNow = TRUE;
   WinPostQueueMsg( mHmq, WM_QUIT, 0, 0);
   return NS_OK;
}

// nsAppShell destructor
nsAppShell::~nsAppShell()
{
   WinDestroyMsgQueue( mHmq);
   WinTerminate( mHab);
}

// These are for modal dialogs and also tres weird.
//
// XXX must handle WM_QUIT sensibly (close window)
//     -- dependency on xptoolkit dialogs being close by other means.
//
nsresult nsAppShell::GetNativeEvent( PRBool &aRealEvent, void *&aEvent)
{
   PRBool gotMessage = PR_FALSE;

#if 0
   nsresult rv;
   NS_WITH_SERVICE(nsITimerQueue, queue, kTimerManagerCID, &rv);
   if (NS_FAILED(rv)) return rv;
#endif

   do {
      // Give priority to system messages (in particular keyboard, mouse,
      // timer, and paint messages).
      if (WinPeekMsg((HAB)0, &mQmsg, NULL, WM_CHAR, WM_CHAR, PM_REMOVE) ||
          WinPeekMsg((HAB)0, &mQmsg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) || 
          WinPeekMsg((HAB)0, &mQmsg, NULL, 0, WM_USER-1, PM_REMOVE) || 
          WinPeekMsg((HAB)0, &mQmsg, NULL, 0, 0, PM_REMOVE)) {

        gotMessage = PR_TRUE;

#if 0
      // process timer queue.
      } else if (queue->HasReadyTimers(NS_PRIORITY_LOWEST)) {

         do {
            queue->FireNextReadyTimer(NS_PRIORITY_LOWEST);
         } while (queue->HasReadyTimers(NS_PRIORITY_LOWEST) && 
                  !WinPeekMsg((HAB)0, &mQmsg, NULL, 0, 0, PM_NOREMOVE));
#endif
      } else {
         // Block and wait for any posted application message
         WinWaitMsg((HAB)0, 0, 0);
      }

   } while (gotMessage == PR_FALSE);

   aEvent = &mQmsg;
   aRealEvent = PR_TRUE;
   return NS_OK;
}

nsresult nsAppShell::DispatchNativeEvent( PRBool /*aRealEvent*/, void */*aEvent*/)
{
   WinDispatchMsg( mHab, &mQmsg);
   return NS_OK;
}

extern "C" nsresult NS_CreateAppshell( nsIAppShell **aAppShell)
{
   if( !aAppShell)
      return NS_ERROR_NULL_POINTER;

   BOOL bFirstTime = FALSE;

   if( !pManager)
   {
      bFirstTime = TRUE;
      pManager = new nsAppshellManager;
   }

   *aAppShell = pManager->GetAppshell();

   if( bFirstTime)
      gModuleData.Init( *aAppShell);

   return NS_OK;
}

// Get the current TID [vacpp doesn't have gettid()]
TID QueryCurrentTID()
{
   PTIB pTib = 0;
   PPIB pPib = 0;
   DosGetInfoBlocks( &pTib, &pPib);
   return pTib->tib_ptib2->tib2_ultid;
}
