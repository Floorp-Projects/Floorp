/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   John Fairhurst <john_fairhurst@iname.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsToolkit.h"
#include "nsSwitchToUIThread.h"

NS_IMPL_ISUPPORTS1(nsToolkit, nsIToolkit)

//
// Static thread local storage index of the Toolkit 
// object associated with a given thread...
//
static PRUintn gToolkitTLSIndex = 0;

//
// main for the message pump thread
//
PRBool gThreadState = PR_FALSE;

struct ThreadInitInfo {
    PRMonitor *monitor;
    nsToolkit *toolkit;
};

void PR_CALLBACK RunPump(void* arg)
{
    ThreadInitInfo *info = (ThreadInitInfo*)arg;
    ::PR_EnterMonitor(info->monitor);

    // do registration and creation in this thread
    info->toolkit->CreateInternalWindow(PR_GetCurrentThread());

    gThreadState = PR_TRUE;

    ::PR_Notify(info->monitor);
    ::PR_ExitMonitor(info->monitor);

    delete info;

    // Process messages
    QMSG qmsg;
    while (WinGetMsg((HAB)0, &qmsg, 0, 0, 0)) {
        WinDispatchMsg((HAB)0, &qmsg);
    }
}

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()  
{
    NS_INIT_REFCNT();
    mGuiThread  = NULL;
    mDispatchWnd = 0;
    mMonitor = PR_NewMonitor();
}


//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
    NS_PRECONDITION(::WinIsWindow((HAB)0, mDispatchWnd), "Invalid window handle");

    // Destroy the Dispatch Window
    ::WinDestroyWindow(mDispatchWnd);
    mDispatchWnd = NULL;

    // Remove the TLS reference to the toolkit...
    PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);
    PR_DestroyMonitor( mMonitor);
}


//-------------------------------------------------------------------------
//
// Register the window class for the internal window and create the window
//
//-------------------------------------------------------------------------
void nsToolkit::CreateInternalWindow(PRThread *aThread)
{
    
    NS_PRECONDITION(aThread, "null thread");
    mGuiThread  = aThread;

    //
    // create the internal window
    //
    WinRegisterClass((HAB)0, "nsToolkitClass", nsToolkitWindowProc, NULL, 0);
    mDispatchWnd = ::WinCreateWindow(HWND_DESKTOP,
                                     "nsToolkitClass",
                                     "NetscapeDispatchWnd",
                                     WS_DISABLED,
                                     -50, -50,
                                     10, 10,
                                     HWND_DESKTOP,
                                     HWND_BOTTOM,
                                     0, 0, 0);
    VERIFY(mDispatchWnd);
}


//-------------------------------------------------------------------------
//
// Create a new thread and run the message pump in there
//
//-------------------------------------------------------------------------
void nsToolkit::CreateUIThread()
{
    PRMonitor *monitor = ::PR_NewMonitor();

    ::PR_EnterMonitor(monitor);

    ThreadInitInfo *ti = new ThreadInitInfo();
    ti->monitor = monitor;
    ti->toolkit = this;

    // create a gui thread
    mGuiThread = ::PR_CreateThread(PR_SYSTEM_THREAD,
                                    RunPump,
                                    (void*)ti,
                                    PR_PRIORITY_NORMAL,
                                    PR_LOCAL_THREAD,
                                    PR_UNJOINABLE_THREAD,
                                    0);

    // wait for the gui thread to start
    while(gThreadState == PR_FALSE) {
        ::PR_Wait(monitor, PR_INTERVAL_NO_TIMEOUT);
    }

    // at this point the thread is running
    ::PR_ExitMonitor(monitor);
    ::PR_DestroyMonitor(monitor);
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsToolkit::Init(PRThread *aThread)
{
    // Store the thread ID of the thread containing the message pump.  
    // If no thread is provided create one
    if (NULL != aThread) {
        CreateInternalWindow(aThread);
    } else {
        // create a thread where the message pump will run
        CreateUIThread();
    }
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsToolkit WindowProc. Used to call methods on the "main GUI thread"...
//
//-------------------------------------------------------------------------
MRESULT EXPENTRY nsToolkitWindowProc(HWND hWnd, ULONG msg, MPARAM mp1,
                                     MPARAM mp2)
{
    switch (msg) {
        case WM_CALLMETHOD:
        {
            MethodInfo *info = (MethodInfo *)mp2;
            PRMonitor *monitor = (PRMonitor *)mp1;
            info->Invoke();
            PR_EnterMonitor(monitor);
            PR_Notify(monitor);
            PR_ExitMonitor(monitor);
        }
        case WM_SENDMSG:
        {
            SendMsgStruct *pData = (SendMsgStruct*) mp1;
            // send the message
            pData->rc = WinSendMsg( pData->hwnd, pData->msg, pData->mp1, pData->mp2);
            // signal the monitor to let the caller continue
            PR_EnterMonitor( pData->pMonitor);
            PR_Notify( pData->pMonitor);
            PR_ExitMonitor( pData->pMonitor);
        }
        default:
            return ::WinDefWindowProc(hWnd, msg, mp1, mp2);
    }
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

void nsToolkit::CallMethod(MethodInfo *info)
{
   PR_EnterMonitor(mMonitor);

   ::WinPostMsg(mDispatchWnd, WM_CALLMETHOD, MPFROMP(mMonitor), MPFROMP(info));

   PR_Wait(mMonitor, PR_INTERVAL_NO_TIMEOUT);
   PR_ExitMonitor( mMonitor);
}

MRESULT nsToolkit::SendMsg( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT rc = 0;

   if( hwnd && IsGuiThread())
      rc = WinSendMsg( hwnd, msg, mp1, mp2);
   else if( hwnd)
   {
      PR_EnterMonitor( mMonitor);

      SendMsgStruct data( hwnd, msg, mp1, mp2, mMonitor);

      // post a message to the window
      WinPostMsg( mDispatchWnd, WM_SENDMSG, MPFROMP(&data), 0);

      // wait for it to complete...
      PR_Wait( mMonitor, PR_INTERVAL_NO_TIMEOUT);
      PR_ExitMonitor( mMonitor);

      rc = data.rc;
   }

   return rc;
}
