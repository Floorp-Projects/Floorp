/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   John C. Griggs <johng@corel.com>
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

#include "nsCOMPtr.h"
#include "nsAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsICmdLineService.h"

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRInt32 gAppShellCount = 0;
PRInt32 gAppShellID = 0;
#endif

//-------------------------------------------------------------------------
//
// XPCOM CIDs
//
//-------------------------------------------------------------------------
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);

PRBool nsAppShell::mRunning = PR_FALSE;

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()
{
  NS_INIT_REFCNT();
  mDispatchListener = 0;
  mEventQueue = nsnull;
  mApplication = nsnull;
#ifdef DBG_JCG
  gAppShellCount++;
  mID = gAppShellID++;
  printf("JCG: nsAppShell CTOR (%p) ID: %d, Count: %d\n",this,mID,gAppShellCount);
#endif
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
  if (mApplication) {
    mApplication->Release();
  }
  mApplication = nsnull;
#ifdef DBG_JCG
  gAppShellCount--;
  printf("JCG: nsAppShell DTOR (%p) ID: %d, Count: %d\n",this,mID,gAppShellCount);
#endif
}

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell)

//-------------------------------------------------------------------------
NS_METHOD 
nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int *bac, char **bav)
{
  int argc = bac ? *bac : 0;
  char **argv = bav;
  nsresult rv;

  nsCOMPtr<nsICmdLineService> cmdLineArgs = do_GetService(kCmdLineServiceCID);
  if (cmdLineArgs) {
    rv = cmdLineArgs->GetArgc(&argc);
    if (NS_FAILED(rv)) {
      argc = bac ? *bac : 0;
    }
    rv = cmdLineArgs->GetArgv(&argv);
    if (NS_FAILED(rv)) {
      argv = bav;
    }
  }
  mApplication = nsQApplication::Instance(argc,argv);
  if (!mApplication) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Spinup - do any preparation necessary for running a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spinup()
{
  nsresult rv = NS_OK;

  if (mApplication == nsnull)
    return NS_ERROR_NOT_INITIALIZED;

  // Get the event queue service 
  nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(kEventQueueServiceCID,&rv);
  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not obtain event queue service",PR_FALSE);
    return rv;
  }
  //Get the event queue for the thread.
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD,getter_AddRefs(mEventQueue));

  // If a queue already present use it.
  if (mEventQueue) {
    goto done;
  }
  // Create the event queue for the thread
  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not create the thread event queue",PR_FALSE);
    return rv;
  }
  //Get the event queue for the thread
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD,getter_AddRefs(mEventQueue));
  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not obtain the thread event queue",PR_FALSE);
    return rv;
  }    
done:
  mApplication->AddEventProcessorCallback(mEventQueue);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spindown()
{
  if (mApplication == nsnull)
    return NS_ERROR_NOT_INITIALIZED;

  if (mEventQueue) {
    mApplication->RemoveEventProcessorCallback(mEventQueue);
    mEventQueue = nsnull;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Run
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Run()
{
  if (mApplication == nsnull)
    return NS_ERROR_NOT_INITIALIZED;

  if (!mEventQueue) {
    Spinup();
  }
  if (!mEventQueue) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  mRunning = PR_TRUE;
  mApplication->exec();
  mRunning = PR_FALSE;

  Spindown();
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit()
{
  if (mApplication == nsnull)
    return NS_ERROR_NOT_INITIALIZED;

  if (mRunning)
    mApplication->exit(0);
  return NS_OK;
}

NS_METHOD nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *& aEvent)
{
  aRealEvent = PR_FALSE;
  aEvent = 0;
  return NS_OK;
}

NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  if (mApplication == nsnull)
    return NS_ERROR_NOT_INITIALIZED;

  if (!mRunning)
    return NS_ERROR_NOT_INITIALIZED;

  mApplication->processEvents(1);
  return NS_OK;
}

NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue,
                                             PRBool aListen)
{
  if (mApplication == nsnull)
    return NS_ERROR_NOT_INITIALIZED;

  if (aListen) {
    mApplication->AddEventProcessorCallback(aQueue);
  }
  else {
    mApplication->RemoveEventProcessorCallback(aQueue);
  }
  return NS_OK;
} 
