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

#include "prmon.h"
#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsICmdLineService.h"
#include <stdlib.h>

#include "nsIWidget.h"
#include "nsIPref.h"

//-------------------------------------------------------------------------
//
// XPCOM CIDs
//
//-------------------------------------------------------------------------
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

// a linked, ordered list of event queues and their tokens
class EventQueueToken {
public:
  EventQueueToken(const nsIEventQueue *aQueue, const gint aToken);

  const nsIEventQueue *mQueue;
  gint mToken;
  EventQueueToken *next;
};
EventQueueToken::EventQueueToken(const nsIEventQueue *aQueue, const gint aToken) {
  mQueue = aQueue;
  mToken = aToken;
  next = 0;
}
class EventQueueTokenQueue {
public:
  EventQueueTokenQueue();
  virtual ~EventQueueTokenQueue();
  void PushToken(nsIEventQueue *aQueue, gint aToken);
  PRBool PopToken(nsIEventQueue *aQueue, gint *aToken);

private:
  EventQueueToken *mHead;
};
EventQueueTokenQueue::EventQueueTokenQueue() {
  mHead = 0;
}
EventQueueTokenQueue::~EventQueueTokenQueue() {
  NS_ASSERTION(!mHead, "event queue token deleted when not empty");
  // and leak. it's an error, anyway
}
void EventQueueTokenQueue::PushToken(nsIEventQueue *aQueue, gint aToken) {
  EventQueueToken *newToken = new EventQueueToken(aQueue, aToken);
  NS_ASSERTION(newToken, "couldn't allocate token queue element");
  if (newToken) {
    newToken->next = mHead;
    mHead = newToken;
  }
}
PRBool EventQueueTokenQueue::PopToken(nsIEventQueue *aQueue, gint *aToken) {
  EventQueueToken *token, *lastToken;
  PRBool          found = PR_FALSE;
  NS_ASSERTION(mHead, "attempt to retrieve event queue token from empty queue");
  if (mHead)
    NS_ASSERTION(mHead->mQueue == aQueue, "retrieving event queue from past head of queue queue");

  token = mHead;
  lastToken = 0;
  while (token && token->mQueue != aQueue) {
    lastToken = token;
    token = token->next;
  }
  if (token) {
    if (lastToken)
      lastToken->next = token->next;
    else
      mHead = token->next;
    found = PR_TRUE;
    *aToken = token->mToken;
    delete token;
  }
  return found;
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
  mLock = PR_NewLock();
  mEventQueueTokens = new EventQueueTokenQueue();
  // throw on error would really be civilized here
  NS_ASSERTION(mLock, "couldn't obtain lock in appshell");
  NS_ASSERTION(mEventQueueTokens, "couldn't allocate event queue token queue");
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
  PR_DestroyLock(mLock);
  delete mEventQueueTokens;
}

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell)

//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
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

#define PREF_NCOLS "browser.ncols"
#define PREF_INSTALLCMAP "browser.installcmap"

static void
HandleColormapPrefs( void )
{
	PRInt32 ivalue = 0;
        PRBool bvalue;
	nsresult rv;

	/* The default is to do nothing. INSTALLCMAP has precedence over
	   NCOLS. Ignore the fact we can't do this if it fails, as it is
	   not critical */
 
	NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
	if (NS_FAILED(rv) || (!prefs)) 
		return;
       
	/* first check ncols */
 
	rv = prefs->GetIntPref(PREF_NCOLS, &ivalue);
	if (NS_SUCCEEDED(rv) && ivalue >= 0 && ivalue <= 255 ) {
		gdk_rgb_set_min_colors( ivalue );
		return;
	}

	/* next check installcmap */

	rv = prefs->GetBoolPref(PREF_INSTALLCMAP, &bvalue);
	if (NS_SUCCEEDED(rv)) {
		if ( PR_TRUE == bvalue )
			gdk_rgb_set_min_colors( 255 );	// force it
		else
			gdk_rgb_set_min_colors( 0 );
	}
}
	
//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsAppShell::Create(int *bac, char **bav)
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

  HandleColormapPrefs();
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
NS_IMETHODIMP nsAppShell::Spinup()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Spindown()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// PushThreadEventQueue
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsAppShell::PushThreadEventQueue()
{
  nsresult      rv;
  gint          inputToken;
  nsIEventQueue *eQueue;

  // push a nested event queue for event processing from netlib
  // onto our UI thread queue stack.
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    PR_Lock(mLock);
    rv = eventQService->PushThreadEventQueue();
    if (NS_SUCCEEDED(rv)) {
      eventQService->GetThreadEventQueue(PR_GetCurrentThread(), &eQueue);
      inputToken = gdk_input_add(eQueue->GetEventQueueSelectFD(),
                                 GDK_INPUT_READ,
                                 event_processor_callback,
                                 eQueue);
      mEventQueueTokens->PushToken(eQueue, inputToken);
    }
    PR_Unlock(mLock);
  } else
    NS_ERROR("Appshell unable to obtain eventqueue service.");
  return rv;
}

//-------------------------------------------------------------------------
//
// PopThreadEventQueue
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsAppShell::PopThreadEventQueue()
{
  nsresult      rv;
  nsIEventQueue *eQueue;

  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    gint queueToken;
    PR_Lock(mLock);
    eventQService->GetThreadEventQueue(PR_GetCurrentThread(), &eQueue);
    eventQService->PopThreadEventQueue();
    if (mEventQueueTokens->PopToken(eQueue, &queueToken))
      gdk_input_remove(queueToken);
    PR_Unlock(mLock);
    NS_IF_RELEASE(eQueue);
  } else
    NS_ERROR("Appshell unable to obtain eventqueue service.");
  return rv;
}

//-------------------------------------------------------------------------
//
// Run
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Run()
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

#ifdef DEBUG
  printf("Calling gdk_input_add with event queue\n");
#endif /* DEBUG */

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

NS_IMETHODIMP nsAppShell::Exit()
{
  gtk_main_quit ();

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

NS_IMETHODIMP nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *& aEvent)
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

NS_IMETHODIMP nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
	if ( aRealEvent == PR_TRUE )
    g_main_iteration (PR_TRUE);
	return NS_OK;
}

NS_IMETHODIMP nsAppShell::EventIsForModalWindow(PRBool aRealEvent,
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

