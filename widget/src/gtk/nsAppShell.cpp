/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "plevent.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsSelectionMgr.h"
#include "nsXPComCIID.h"
#include "nsICmdLineService.h"
#include <stdlib.h>


static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

static NS_DEFINE_IID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_IID(kICmdLineServiceIID, NS_ICOMMANDLINE_SERVICE_IID);

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()
{
  mRefCnt = 0;
  mDispatchListener = 0;
  mSelectionMgr = 0;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
  NS_IF_RELEASE(mSelectionMgr);
}

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
NS_IMPL_ISUPPORTS(nsAppShell,kIAppShellIID);

//-------------------------------------------------------------------------
NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

static void event_processor_callback(gpointer data,
                                     gint source,
                                     GdkInputCondition condition)
{
  PLEventQueue *event = (PLEventQueue*)data;
  PR_ProcessPendingEvents(event);
}

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

#ifdef CMDLINEARGS
NS_METHOD nsAppShell::Create(int *bac, char **bav)
#else
NS_METHOD nsAppShell::Create(int *argc, char **argv)
#endif

{
  gchar *path;
#ifdef CMDLINEARGS
  int *argc;
  char **argv;
  nsICmdLineService *cmdLineArgs=nsnull;
  nsresult   rv = NS_OK;

  rv = nsServiceManager::GetService(kCmdLineServiceCID,
                                    kICmdLineServiceIID,
                                    (nsISupports **)&cmdLineArgs);
 // Get the value of -width option
  rv = cmdLineArgs->GetArgc(argc);
  rv = cmdLineArgs->GetArgv(&argv);
#endif
  gtk_set_locale ();

  gtk_init (argc, &argv);

  // delete the cmdLineArgs thing?

  gdk_rgb_init();

  path = g_strdup_printf("%s%s", g_get_home_dir(),"/.gtkrc");
  gtk_rc_parse(path);
  g_free(path);

//  gtk_rc_init();

  // Create the selection manager
  if (!mSelectionMgr)
      NS_NewSelectionMgr(&mSelectionMgr);

  return NS_OK;
}

NS_METHOD nsAppShell::Run()
{

  nsresult   rv = NS_OK;
  PLEventQueue * EQueue = nsnull;

  // Get the event queue service 
  rv = nsServiceManager::GetService(kEventQueueServiceCID, 
                                    kIEventQueueServiceIID,
                                    (nsISupports **) &mEventQService);

  if (NS_OK != rv) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return rv;
  }

  printf("Got thew event queue from the service\n");
  //Get the event queue for the thread.
  rv = mEventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);

  // If a queue already present use it.
  if (nsnull != EQueue)
     goto done;

  // Create the event queue for the thread
  rv = mEventQService->CreateThreadEventQueue();
  if (NS_OK != rv) {
    NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
    return rv;
  }
  //Get the event queue for the thread
  rv = mEventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);
  if (NS_OK != rv) {
      NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
      return rv;
  }    


done:
  printf("Calling gdk_input with event queue\n");
  gdk_input_add(PR_GetEventQueueSelectFD(EQueue),
                GDK_INPUT_READ,
                event_processor_callback,
                EQueue);

  gtk_main();

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit()
{
  gtk_main_quit ();
  gtk_exit(0);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  if (aDataType == NS_NATIVE_SHELL) {
  // this isn't accually used, but if it was, we need to gtk_widget_ref() it.


//    return mTopLevel;
  }
  return nsnull;
}

// XXX temporary code for Dialog investigation
nsresult nsAppShell::GetNativeEvent(void *& aEvent, nsIWidget* aWidget, PRBool &aIsInWindow, PRBool &aIsMouseEvent)
{
  aIsInWindow   = PR_FALSE;
  aIsMouseEvent = PR_FALSE;

  return NS_ERROR_FAILURE;
}

nsresult nsAppShell::DispatchNativeEvent(void * aEvent)
{
  return NS_ERROR_FAILURE;
}

NS_METHOD
nsAppShell::GetSelectionMgr(nsISelectionMgr** aSelectionMgr)
{
  *aSelectionMgr = mSelectionMgr;
  NS_IF_ADDREF(mSelectionMgr);
  if (!mSelectionMgr)
    return NS_ERROR_NOT_INITIALIZED;
  return NS_OK;
}

