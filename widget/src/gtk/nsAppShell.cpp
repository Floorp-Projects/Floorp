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
#include "nsXPComCIID.h"
#include <stdlib.h>


static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()
{
  mRefCnt = 0;
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

NS_METHOD nsAppShell::Create(int* argc, char ** argv)
{
  gchar *path;
  
  gtk_set_locale ();

  gtk_init (argc, &argv);

  gdk_rgb_init();
  gdk_rgb_set_verbose(PR_TRUE);

  path = g_strdup_printf(g_get_home_dir(),"/.gtkrc");
  gtk_rc_parse(path);
  g_free(path);

//  gtk_rc_init();

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
