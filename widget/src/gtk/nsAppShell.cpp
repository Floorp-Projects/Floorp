/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "prmon.h"
#include "nsCOMPtr.h"
#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsICmdLineService.h"
#include "nsGtkEventHandler.h"
#include <stdlib.h>

#ifdef MOZ_GLE
#include <gle/gle.h>
#endif

#include "nsIWidget.h"
#include "nsIPref.h"


#include "glib.h"

static PRBool sInitialized = PR_FALSE;

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


//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()
{
  mEventQueue  = nsnull;
  NS_INIT_REFCNT();
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

NS_IMPL_ISUPPORTS1(nsAppShell, nsIAppShell)

//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
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
		if ( ivalue > 6*6*6 )	// workaround for old GdkRGB's
			ivalue = 6*6*6;
		gdk_rgb_set_min_colors( ivalue );
		return;
	}

	/* next check installcmap */

	rv = prefs->GetBoolPref(PREF_INSTALLCMAP, &bvalue);
	if (NS_SUCCEEDED(rv)) {
		if ( PR_TRUE == bvalue )
			gdk_rgb_set_install( TRUE );	// force it
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
  if (sInitialized)
    return NS_OK;

  sInitialized = PR_TRUE;

  gchar *home=nsnull;
  gchar *path=nsnull;

  int argc = bac ? *bac : 0;
  char **argv = bav;

  nsresult rv;

  nsCOMPtr<nsICmdLineService> cmdLineArgs;
  cmdLineArgs = do_GetService(kCmdLineServiceCID);
  if (cmdLineArgs)
  {
    rv = cmdLineArgs->GetArgc(&argc);
    if(NS_FAILED(rv))
      argc = bac ? *bac : 0;

    rv = cmdLineArgs->GetArgv(&argv);
    if(NS_FAILED(rv))
      argv = bav;
  }

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
  nsresult   rv = NS_OK;
  
  // Get the event queue service 
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return rv;
  }

  //Get the event queue for the thread.
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &mEventQueue);
  
  // If a queue already present use it.
  if (mEventQueue)
    return rv;

  // Create the event queue for the thread
  rv = eventQService->CreateThreadEventQueue();
  if (NS_OK != rv) {
    NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
    return rv;
  }

  //Get the event queue for the thread
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, &mEventQueue);
  
  return rv;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Spindown()
{
  if (mEventQueue)
  { 
    mEventQueue->ProcessPendingEvents();
    NS_RELEASE(mEventQueue);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Run
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Run()
{
  if (!mEventQueue)
    Spinup();
  
  if (!mEventQueue)
    return NS_ERROR_NOT_INITIALIZED;

    
  our_gdk_input_add(mEventQueue->GetEventQueueSelectFD(),
                    event_processor_callback,
                    mEventQueue, 
                    G_PRIORITY_DEFAULT);
  gtk_main();

  Spindown();

  return NS_OK; 
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsAppShell::Exit()
{
  gtk_main_quit();
  return NS_OK;
}

// does nothing. used by xp code with non-gtk expectations.
// this method will be removed once xp eventloops are working.
NS_IMETHODIMP nsAppShell::GetNativeEvent(PRBool &aRealEvent, void *& aEvent)
{
  aRealEvent = PR_FALSE;
  aEvent = 0;
  return NS_OK;
}

// simply executes one iteration of the event loop. used by xp code with
// non-gtk expectations.
// this method will be removed once xp eventloops are working.
NS_IMETHODIMP nsAppShell::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  if (!mEventQueue)
    return NS_ERROR_NOT_INITIALIZED;
  
  g_main_iteration(PR_TRUE);

  return mEventQueue->ProcessPendingEvents();
}

NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue,
                                             PRBool aListen)
{
  return NS_OK;
}

