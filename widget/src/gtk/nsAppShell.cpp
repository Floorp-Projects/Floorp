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
#include "nsCOMPtr.h"
#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsICmdLineService.h"
#include <stdlib.h>

#ifdef MOZ_GLE
#include <gle/gle.h>
#endif

#include "nsIWidget.h"
#include "nsIPref.h"


#include "glib.h"

struct OurGdkIOClosure {
  GdkInputFunction  function;
  gpointer          data;
};

static gboolean
our_gdk_io_invoke(GIOChannel* source, GIOCondition condition, gpointer data)
{
  OurGdkIOClosure* ioc = (OurGdkIOClosure*) data;
  if (ioc) {
    (*ioc->function)(ioc->data, g_io_channel_unix_get_fd(source),
                     GDK_INPUT_READ);
  }
  return TRUE;
}

static void
our_gdk_io_destroy(gpointer data)
{
  OurGdkIOClosure* ioc = (OurGdkIOClosure*) data;
  if (ioc) {
    g_free(ioc);
  }
}

static gint
our_gdk_input_add (gint              source,
                   GdkInputFunction  function,
                   gpointer          data,
                   gint              priority)
{
  guint result;
  OurGdkIOClosure *closure = g_new (OurGdkIOClosure, 1);
  GIOChannel *channel;

  closure->function = function;
  closure->data = data;

  channel = g_io_channel_unix_new (source);
  result = g_io_add_watch_full (channel, priority, G_IO_IN,
                                our_gdk_io_invoke,
                                closure, our_gdk_io_destroy);
  g_io_channel_unref (channel);

  return result;
}


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
  EventQueueToken(nsIEventQueue *aQueue, const gint aToken);
  virtual ~EventQueueToken();  
  nsIEventQueue   *mQueue;
  gint mToken;
  EventQueueToken *mNext;
};

EventQueueToken::EventQueueToken(nsIEventQueue *aQueue, const gint aToken) {
  mQueue = aQueue;
  NS_IF_ADDREF(mQueue);
  mToken = aToken;
  mNext = 0;
}

EventQueueToken::~EventQueueToken(){
 NS_IF_RELEASE(mQueue);
}

class EventQueueTokenQueue {
public:
  EventQueueTokenQueue();
  virtual ~EventQueueTokenQueue();
  nsresult PushToken(nsIEventQueue *aQueue, gint aToken);
  PRBool PopToken(nsIEventQueue *aQueue, gint *aToken);

private:
  EventQueueToken *mHead;
};

EventQueueTokenQueue::EventQueueTokenQueue() {
  mHead = 0;
}

EventQueueTokenQueue::~EventQueueTokenQueue() {

  // if we reach this point with an empty token queue, well, fab. however,
  // we expect the first event queue to still be active. so we take
  // special care to unhook that queue (not that failing to do so seems
  // to hurt anything). more queues than that would be an error.
//NS_ASSERTION(!mHead || !mHead->mNext, "event queue token list deleted when not empty");
  // (and skip the assertion for now. we're leaking event queues because they
  // are referenced by things that leak, so this assertion goes off a lot.)
  if (mHead) {
    gdk_input_remove(mHead->mToken);
    delete mHead;
    // and leak the rest. it's an error, anyway
  }
}

nsresult EventQueueTokenQueue::PushToken(nsIEventQueue *aQueue, gint aToken) {
  EventQueueToken *newToken = new EventQueueToken(aQueue, aToken);
  NS_ASSERTION(newToken, "couldn't allocate token queue element");
  if (!newToken)
    return NS_ERROR_OUT_OF_MEMORY;

  newToken->mNext = mHead;
  mHead = newToken;
  return NS_OK;
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
    token = token->mNext;
  }
  if (token) {
    if (lastToken)
      lastToken->mNext = token->mNext;
    else
      mHead = token->mNext;
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
  mEventQueueTokens = new EventQueueTokenQueue();
  // throw on error would really be civilized here
  NS_ASSERTION(mEventQueueTokens, "couldn't allocate event queue token queue");
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
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
  if (eventQueue)
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

#ifdef MOZ_GLE
  gle_init (&argc, &argv);
#endif

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

  // (has to be called explicitly for this, the primordial appshell, because
  // of startup ordering problems.)
  ListenToEventQueue(EQueue, PR_TRUE);

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

NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue,
                                             PRBool aListen)
{
  // tell gdk to listen to the event queue or not

  gint queueToken;
  if (aListen) {
    queueToken = our_gdk_input_add(aQueue->GetEventQueueSelectFD(),
                                   event_processor_callback,
                                   aQueue, G_PRIORITY_DEFAULT_IDLE);
    mEventQueueTokens->PushToken(aQueue, queueToken);
  } else {
    if (mEventQueueTokens->PopToken(aQueue, &queueToken))
      gdk_input_remove(queueToken);
  }
  return NS_OK;
}

