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

#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsICmdLineService.h"

#include "nsIMotifAppContextService.h"

#include <stdlib.h>

#ifdef LINUX
#define DO_THE_EDITRES_THING
#endif

#ifdef DO_THE_EDITRES_THING
#include <X11/Xmu/Editres.h>
#endif

#include "xlibrgb.h"

#include "nsIPref.h"

//-------------------------------------------------------------------------
//
// XPCOM CIDs
//
//-------------------------------------------------------------------------
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell)

//-------------------------------------------------------------------------
NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

XtAppContext nsAppShell::sAppContext = nsnull;

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int* bac, char ** bav)
{
  int argc = bac ? *bac : 0;
  char **argv = bav;

  nsresult rv;

  nsCOMPtr<nsICmdLineService> cmdLineArgs = 
           do_GetService(kCmdLineServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    rv = cmdLineArgs->GetArgc(&argc);
    if(NS_FAILED(rv))
      argc = bac ? *bac : 0;

    rv = cmdLineArgs->GetArgv(&argv);
    if(NS_FAILED(rv))
      argv = bav;
  }

  XtSetLanguageProc(NULL, NULL, NULL);
							
  mTopLevel = XtAppInitialize(&sAppContext,   // app_context_return
                              "nsAppShell",   // application_class
                              NULL,           // options
                              0,              // num_options
                              &argc,          // argc_in_out
                              argv,           // argv_in_out
                              NULL,           // fallback_resources
                              NULL,           // args
                              0);             // num_args

  printf("nsAppShell::Create() app_context = %p\n",sAppContext);

  xlib_rgb_init(XtDisplay(mTopLevel), XtScreen(mTopLevel));

  printf("xlib_rgb_init(display=%p,screen=%p)\n",
         XtDisplay(mTopLevel),
         XtScreen(mTopLevel));

  SetAppContext(sAppContext);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

static void event_processor_callback(XtPointer       aClosure,
                                     int *           aFd,
                                     XtIntervalId *  aId)
{
  nsIEventQueue *eventQueue = (nsIEventQueue*)aClosure;
  eventQueue->ProcessPendingEvents();
}

NS_METHOD nsAppShell::Run()
{
  NS_ADDREF_THIS();
  nsresult   rv = NS_OK;
  nsIEventQueue * EQueue = nsnull;

  // Get the event queue service 
  nsCOMPtr<nsIEventQueueService> eventQService = 
           do_GetService(kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return rv;
  }

#ifdef DEBUG
  printf("Got the event queue from the service\n");
#endif /* DEBUG */

  //Get the event queue for the thread.
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &EQueue);

  // If a queue already present use it.
  if (EQueue)
    goto done;

  // Create the event queue for the thread
  rv = eventQService->CreateThreadEventQueue();
  if (NS_OK != rv) {
    NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
    return rv;
  }
  //Get the event queue for the thread
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &EQueue);
  if (NS_OK != rv) {
    NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
    return rv;
  }    


done:

  printf("Calling XtAppAddInput() with event queue\n");

  XtAppAddInput(nsAppShell::GetAppContext(),
                EQueue->GetEventQueueSelectFD(),
                (XtPointer) XtInputReadMask, 
                event_processor_callback, 
                EQueue);

  XtRealizeWidget(mTopLevel);

#ifdef DO_THE_EDITRES_THING
	XtAddEventHandler(mTopLevel,
                    (EventMask) 0,
                    True,
                    (XtEventHandler) _XEditResCheckMessages,
                    (XtPointer)NULL);
#endif

  XEvent event;

  for (;;) 
  {
    XtAppNextEvent(sAppContext, &event);

    XtDispatchEvent(&event);

    if (mDispatchListener)
      mDispatchListener->AfterDispatch();
  } 

  NS_IF_RELEASE(EQueue);
  Release();
  return NS_OK;
}

NS_METHOD nsAppShell::Spinup()
{
  return NS_OK;
}

NS_METHOD nsAppShell::Spindown()
{
  return NS_OK;
}

NS_METHOD nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  //XXX:Implement this.
  return NS_OK;
}

NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void * aEvent)
{
  //XXX:Implement this.
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit()
{
  exit(0);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()
{ 
  NS_INIT_REFCNT();
  mDispatchListener = 0;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  if (aDataType == NS_NATIVE_SHELL) {
    return mTopLevel;
  }
  return nsnull;
}


static NS_DEFINE_CID(kCMotifAppContextServiceCID, NS_MOTIF_APP_CONTEXT_SERVICE_CID);

//-------------------------------------------------------------------------
//
// SetAppContext
//
//-------------------------------------------------------------------------
/* static */ void 
nsAppShell::SetAppContext(XtAppContext aAppContext)
{
  NS_ASSERTION(aAppContext != nsnull,"App context cant be null.");

  static PRBool once = PR_TRUE;

  if (once)
  {
    once = PR_FALSE;

    nsresult   rv;
    nsIMotifAppContextService * ac_service = nsnull;
    
    rv = nsComponentManager::CreateInstance(kCMotifAppContextServiceCID,
                                            nsnull,
                                            NS_GET_IID(nsIMotifAppContextService),
                                            (void **)& ac_service);
    
    NS_ASSERTION(rv == NS_OK,"Cannot obtain app context service.");

    if (ac_service)
    {
      printf("nsAppShell::SetAppContext() ac_service = %p\n",ac_service);

      nsresult rv2 = ac_service->SetAppContext(aAppContext);

      NS_ASSERTION(rv2 == NS_OK,"Cannot set the app context.");

      printf("nsAppShell::SetAppContext() All is ok.\n");

      NS_RELEASE(ac_service);
    }
  }
}
