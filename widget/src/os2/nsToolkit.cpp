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
 */

#include "nsToolkit.h"

#include "nsIAppShell.h"
#include "nsGUIEvent.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

static PRUintn gToolkitTLSIndex = 0;

// Bits to deal with the case where a new toolkit is initted with a null ----
// thread.  In this case it has to create a new thread to be the PM thread.
// Hopefully this will never happen!

// struct passed to new thread
struct ThreadInitInfo
{
   PRMonitor *pMonitor;
   nsToolkit *toolkit;

   ThreadInitInfo( nsToolkit *tk, PRMonitor *pMon)
      : pMonitor( pMon), toolkit( tk)
   {}
};

// main for the message pump thread
extern "C" void RunPump( void *arg)
{
   ThreadInitInfo *info = (ThreadInitInfo *) arg;
   nsIAppShell    *pShell = nsnull;
   nsresult        res;

   static NS_DEFINE_IID(kAppShellCID, NS_APPSHELL_CID);

   res = nsComponentManager::CreateInstance( kAppShellCID, nsnull,
                                             NS_GET_IID(nsIAppShell),
                                             (void **) &pShell);
   NS_ASSERTION( res == NS_OK, "Couldn't create new shell");

   pShell->Create( 0, 0);

   // do registration and creation in this thread
   info->toolkit->CreateInternalWindow( PR_GetCurrentThread());

   // let the main thread continue
   PR_EnterMonitor( info->pMonitor);
   PR_Notify( info->pMonitor);
   PR_ExitMonitor( info->pMonitor);

   pShell->Run();
   NS_RELEASE( pShell);
}

// toolkit method to create a new thread and process the msgq there
void nsToolkit::CreatePMThread()
{
   PR_EnterMonitor( mMonitor);

   ThreadInitInfo ti( this, mMonitor);

   // create a pm thread
   mPMThread = ::PR_CreateThread( PR_SYSTEM_THREAD,
                                  RunPump,
                                  &ti,
                                  PR_PRIORITY_NORMAL,
                                  PR_LOCAL_THREAD,
                                  PR_UNJOINABLE_THREAD,
                                  0);
   // wait for it to start
   PR_Wait( mMonitor, PR_INTERVAL_NO_TIMEOUT);
   PR_ExitMonitor( mMonitor);
}

// 'Normal use' toolkit methods ---------------------------------------------
nsToolkit::nsToolkit()  : mDispatchWnd( 0), mPMThread( 0)
{
   NS_INIT_REFCNT();
   mMonitor = PR_NewMonitor();
}

nsToolkit::~nsToolkit()
{
   // Destroy the Dispatch Window
   WinDestroyWindow( mDispatchWnd);

   // Destroy monitor
   PR_DestroyMonitor( mMonitor);
}

// nsISupports implementation macro
NS_IMPL_ISUPPORTS(nsToolkit, NS_GET_IID(nsIToolkit))

static MRESULT EXPENTRY fnwpDispatch( HWND, ULONG, MPARAM, MPARAM);
#define UWC_DISPATCH "DispatchWndClass"

// Create the internal window - also sets the pm thread 'formally'
void nsToolkit::CreateInternalWindow( PRThread *aThread)
{
   NS_PRECONDITION(aThread, "null thread");
   mPMThread  = aThread;

   BOOL rc = WinRegisterClass( 0/*hab*/, UWC_DISPATCH, fnwpDispatch, 0, 0);
   NS_ASSERTION( rc, "Couldn't register class");

   // create the internal window - just use a static
   mDispatchWnd = WinCreateWindow( HWND_DESKTOP,
                                   UWC_DISPATCH,
                                   0, 0,
                                   0, 0, 0, 0,
                                   HWND_DESKTOP,
                                   HWND_BOTTOM,
                                   0, 0, 0);

   NS_ASSERTION( mDispatchWnd, "Couldn't create toolkit internal window");

#if DEBUG_sobotka
   printf("\n+++++++++++nsToolkit created dispatch window 0x%lx\n", mDispatchWnd);
#endif
}

// Set up the toolkit - create window, check for thread.
nsresult nsToolkit::Init( PRThread *aThread)
{
   // Store the thread ID of the thread with the message queue.
   // If no thread is provided create one (!!)
   if( aThread)
      CreateInternalWindow( aThread);
   else
      // create a thread where the message pump will run
      CreatePMThread();
   return NS_OK;
}

// Bits to reflect events into the pm thread --------------------------------

// additional structure to provide synchronization & returncode
// Unlike Windows, we cannot use WinSendMsg()
struct RealMethodInfo
{
   MethodInfo *pInfo;
   PRMonitor  *pMonitor;
   nsresult    rc;

   RealMethodInfo( MethodInfo *info, PRMonitor *pMon)
        : pInfo( info), pMonitor( pMon), rc( NS_ERROR_FAILURE)
   {}
};

nsresult nsToolkit::CallMethod( MethodInfo *pInfo)
{
   PR_EnterMonitor( mMonitor);

   RealMethodInfo rminfo( pInfo, mMonitor);

   // post the message to the window
   WinPostMsg( mDispatchWnd, WMU_CALLMETHOD, MPFROMP(&rminfo), 0);

   // wait for it to complete...
   PR_Wait( mMonitor, PR_INTERVAL_NO_TIMEOUT);
   PR_ExitMonitor( mMonitor);

   // cleanup & return
   return rminfo.rc;
}

struct SendMsgStruct
{
   HWND    hwnd;
   ULONG   msg;
   MPARAM  mp1, mp2;
   MRESULT rc;

   PRMonitor *pMonitor;

   SendMsgStruct( HWND h, ULONG m, MPARAM p1, MPARAM p2, PRMonitor *pMon)
        : hwnd( h), msg( m), mp1( p1), mp2( p2), rc( 0), pMonitor( pMon)
   {}
};

MRESULT nsToolkit::SendMsg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT rc = 0;

   if( hwnd && IsPMThread())
      rc = WinSendMsg( hwnd, msg, mp1, mp2);
   else if( hwnd)
   {
      PR_EnterMonitor( mMonitor);

      SendMsgStruct data( hwnd, msg, mp1, mp2, mMonitor);

      // post a message to the window
      WinPostMsg( mDispatchWnd, WMU_SENDMSG, MPFROMP(&data), 0);

      // wait for it to complete...
      PR_Wait( mMonitor, PR_INTERVAL_NO_TIMEOUT);
      PR_ExitMonitor( mMonitor);

      rc = data.rc;
   }

   return rc;
}

MRESULT EXPENTRY fnwpDispatch( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mRC = 0;

   if( msg == WMU_CALLMETHOD)
   {
      RealMethodInfo *pInfo = (RealMethodInfo*) mp1;
      // call the method (indirection-fest :-)
      pInfo->rc = pInfo->pInfo->target->CallMethod( pInfo->pInfo);
      // signal the monitor to let the caller continue
      PR_EnterMonitor( pInfo->pMonitor);
      PR_Notify( pInfo->pMonitor);
      PR_ExitMonitor( pInfo->pMonitor);
   }
   else if( msg == WMU_SENDMSG)
   {
      SendMsgStruct *pData = (SendMsgStruct*) mp1;
      // send the message
      pData->rc = WinSendMsg( pData->hwnd, pData->msg, pData->mp1, pData->mp2);
      // signal the monitor to let the caller continue
      PR_EnterMonitor( pData->pMonitor);
      PR_Notify( pData->pMonitor);
      PR_ExitMonitor( pData->pMonitor);
   }
   else
      mRC = WinDefWindowProc( hwnd, msg, mp1, mp2);

   return mRC;
}

//-------------------------------------------------------------------------
//
// Return the nsIToolkit for the current thread.  If a toolkit does not
// yet exist, then one will be created...
//
//-------------------------------------------------------------------------
NS_METHOD NS_GetCurrentToolkit(nsIToolkit* *aResult)
{
  nsIToolkit* toolkit = nsnull;
  nsresult rv = NS_OK;
  PRStatus status;

  // Create the TLS index the first time through...
  if (0 == gToolkitTLSIndex) {
    status = PR_NewThreadPrivateIndex(&gToolkitTLSIndex, NULL);
    if (PR_FAILURE == status) {
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    toolkit = (nsIToolkit*)PR_GetThreadPrivate(gToolkitTLSIndex);

    //
    // Create a new toolkit for this thread...
    //
    if (!toolkit) {
      toolkit = new nsToolkit();

      if (!toolkit) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      } else {
        NS_ADDREF(toolkit);
        toolkit->Init(PR_GetCurrentThread());
        //
        // The reference stored in the TLS is weak.  It is removed in the
        // nsToolkit destructor...
        //
        PR_SetThreadPrivate(gToolkitTLSIndex, (void*)toolkit);
      }
    } else {
      NS_ADDREF(toolkit);
    }
    *aResult = toolkit;
  }

  return rv;
}
