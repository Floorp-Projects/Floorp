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
 *
 */

#include "nsAppShell.h"
#include "nsHashtable.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"

NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

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

NS_IMPL_ISUPPORTS(nsAppShell, nsIAppShell::GetIID())

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

   // Add a reference to deal with async. shutdown
   NS_ADDREF_THIS();

   // XXX if we're HMQ_CURRENT, peek the first message & get the right one ?

   // Process messages
   for( ;;)
   {
      if( TRUE == WinGetMsg( mHab, &mQmsg, 0, 0, 0))
      {
         WinDispatchMsg( mHab, &mQmsg);
      }
      else
      {  // WM_QUIT
         if( mQmsg.hwnd)
            // send WM_SYSCOMMAND, SC_CLOSE to window (tasklist close)
            WinSendMsg( mQmsg.hwnd, WM_SYSCOMMAND, MPFROMSHORT(SC_CLOSE), 0);
         else               
         {
            if( mQuitNow) // Don't want to close the app when a window is
               break;     // closed, just when our `Exit' is called.
         }
      }

      if( mDispatchListener)
         mDispatchListener->AfterDispatch();
   }

   // reset mQuitNow flag for re-entrant appshells
   mQuitNow = FALSE;

   // Release the reference we added earlier
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

//-------------------------------------------------------------------------
//
// PushThreadEventQueue - begin processing events from a new queue
//   note this is the Windows implementation and may suffice, but
//   this is untested on os/2.
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::PushThreadEventQueue()
{
  nsresult rv;

  // push a nested event queue for event processing from netlib
  // onto our UI thread queue stack.
  NS_WITH_SERVICE(nsIEventQueueService, eQueueService, kEventQueueServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = eQueueService->PushThreadEventQueue();
  else
    NS_ERROR("Appshell unable to obtain eventqueue service.");
  return rv;
}

//-------------------------------------------------------------------------
//
// PopThreadEventQueue - stop processing on a previously pushed event queue
//   note this is the Windows implementation and may suffice, but
//   this is untested on os/2.
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::PopThreadEventQueue()
{
  nsresult rv;

  NS_WITH_SERVICE(nsIEventQueueService, eQueueService, kEventQueueServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = eQueueService->PopThreadEventQueue();
  else
    NS_ERROR("Appshell unable to obtain eventqueue service.");
  return rv;
}

// These are for modal dialogs and also tres weird.
//
// XXX must handle WM_QUIT sensibly (close window)
//     -- dependency on xptoolkit dialogs being close by other means.
//
nsresult nsAppShell::GetNativeEvent( PRBool &aRealEvent, void *&aEvent)
{
   BOOL     isNotQuit = WinGetMsg( mHab, &mQmsg, 0, 0, 0);
   nsresult rc = NS_ERROR_FAILURE;

   if( isNotQuit)
   {
      aRealEvent = PR_TRUE;
      rc = NS_OK;
      aEvent = &mQmsg;
   }
   else
      aRealEvent = PR_FALSE;

   return rc;
}

nsresult nsAppShell::DispatchNativeEvent( PRBool /*aRealEvent*/, void */*aEvent*/)
{
   WinDispatchMsg( mHab, &mQmsg);
   return NS_OK;
}

nsresult nsAppShell::EventIsForModalWindow( PRBool    aRealEvent,
                                            void      */*aEvent*/,
                                            nsIWidget *aWidget,
                                            PRBool    *aForWindow)
{
   nsresult rc = NS_ERROR_FAILURE;

   if( PR_FALSE == aRealEvent)
   {
      *aForWindow = PR_FALSE;
      rc = NS_OK;
   }
   else if( nsnull != aWidget)
   {
      // Set aForWindow if either:
      //   * the message is for a descendent of the given window
      //   * the message is for another window, but is a message which
      //     should be allowed for a disabled window.

      PRBool isMouseEvent = PR_FALSE;
      PRBool isInWindow = PR_FALSE;

      // Examine the target window & find the frame
      // XXX should GetNativeData() use GetMainWindow() ?
      HWND hwnd = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
      hwnd = WinQueryWindow(hwnd, QW_PARENT);

      if( hwnd == mQmsg.hwnd || WinIsChild( mQmsg.hwnd, hwnd))
         isInWindow = PR_TRUE;

      // XXX really ought to do something about focus here

      if( !isInWindow)
      {
         // Block mouse messages for non-modal windows
         if( mQmsg.msg >= WM_MOUSEFIRST && mQmsg.msg <= WM_MOUSELAST)
            isMouseEvent = PR_TRUE;
         else if( mQmsg.msg >= WM_MOUSETRANSLATEFIRST &&
                  mQmsg.msg <= WM_MOUSETRANSLATELAST)
            isMouseEvent = PR_TRUE;
         else if( mQmsg.msg == WMU_MOUSEENTER || mQmsg.msg == WMU_MOUSELEAVE)
            isMouseEvent = PR_TRUE;
      }

      // set dispatch indicator
      *aForWindow = isInWindow || !isMouseEvent;

      rc = NS_OK;
   }

   return rc;
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
