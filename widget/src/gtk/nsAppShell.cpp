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
#include "nsICmdLineService.h"
#include "nsXPComCIID.h"
#include <stdlib.h>

#include "nsIWidget.h"

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
  nsIEventQueue *eventQueue = (nsIEventQueue*)data;
  eventQueue->ProcessPendingEvents();
}

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int *bac, char **bav)
{
  gchar *home=nsnull;
  gchar *path=nsnull;

  int argc = bac ? *bac : 0;
  char **argv = bav;
#if 1
  nsresult rv;

  NS_WITH_SERVICE(nsICmdLineService, cmdLineArgs, kCmdLineServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    rv = cmdLineArgs->GetArgc(&argc);
    if(NS_FAILED(rv))
      argc = bac ? *bac : 0;

    rv = cmdLineArgs->GetArgv(&argv);
    if(NS_FAILED(rv))
      argv = bav;
  }
#endif

  gtk_set_locale ();

  gtk_init (&argc, &argv);

  // delete the cmdLineArgs thing?

  gdk_rgb_init();

  home = g_get_home_dir();
  if ((char*)nsnull != home) {
    path = g_strdup_printf("%s%c%s", home, G_DIR_SEPARATOR, ".gtkrc");
    if ((char *)nsnull != path) {
      gtk_rc_parse(path);
      g_free(path);
    }
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
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Spindown()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Run
//
//-------------------------------------------------------------------------
NS_METHOD nsAppShell::Run()
{
  NS_ADDREF_THIS();
  nsresult   rv = NS_OK;
  nsIEventQueue * EQueue = nsnull;

  // Get the event queue service 
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return rv;
  }

#ifdef DEBUG
  printf("Got the event queue from the service\n");
#endif /* DEBUG */

  //Get the event queue for the thread.
  rv = eventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);

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
  rv = eventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);
  if (NS_OK != rv) {
    NS_ASSERTION("Could not obtain the thread event queue", PR_FALSE);
    return rv;
  }    


done:
  printf("Calling gdk_input_add with event queue\n");
  gdk_input_add(EQueue->GetEventQueueSelectFD(),
                GDK_INPUT_READ,
                event_processor_callback,
                EQueue);
  gtk_main();

  NS_IF_RELEASE(EQueue);
  Release();
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

    //  return mTopLevel;
  }
  return nsnull;
}

NS_METHOD nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *& aEvent)
{
  GdkEvent *event;

  aEvent = 0;
  aRealEvent = PR_FALSE;
  event = gdk_event_peek();

  if ((GdkEvent *) nsnull != event ) { 
    aRealEvent = PR_TRUE;
    aEvent = event;
  } else 
    g_main_iteration (PR_TRUE);

  return NS_OK;
}

NS_METHOD nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
	if ( aRealEvent == PR_TRUE )
    g_main_iteration (PR_TRUE);
	return NS_OK;
}

NS_METHOD nsAppShell::EventIsForModalWindow(PRBool aRealEvent,
                                            void *aEvent,
                                            nsIWidget *aWidget,
                                            PRBool *aForWindow)
{
  PRBool isInWindow, isMouseEvent;
  GdkEventAny *msg = (GdkEventAny *) aEvent;

  if (aRealEvent == PR_FALSE) {
    *aForWindow = PR_FALSE;
    return NS_OK;
  }

  isInWindow = PR_FALSE;
  if (aWidget != nsnull) {
    // Get Native Window for dialog window
    GdkWindow *win;
    win = (GdkWindow *)aWidget->GetNativeData(NS_NATIVE_WINDOW);

    // Find top most window of event window
    GdkWindow *eWin = msg->window;
    if (nsnull != eWin) {
      if (win == eWin) {
        isInWindow = PR_TRUE;
      }
    }
  }

  isMouseEvent = PR_FALSE;
  switch (msg->type)
  {
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      isMouseEvent = PR_TRUE;
    default:
      isMouseEvent = PR_FALSE;
  }

  *aForWindow = isInWindow == PR_TRUE || 
    isMouseEvent == PR_FALSE ? PR_TRUE : PR_FALSE;

  gdk_event_free( (GdkEvent *) aEvent );
  return NS_OK;
}

