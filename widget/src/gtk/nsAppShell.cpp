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

#include "prmon.h"
#include "plhash.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsICmdLineService.h"
#include "nsGtkEventHandler.h"
#include <stdlib.h>
#include <gdk/gdkx.h>

#ifdef MOZ_GLE
#include <gle/gle.h>
#endif

#include "nsIWidget.h"
#include "nsIPref.h"

#include "glib.h"
#include "nsVoidArray.h"

static PRBool sInitialized = PR_FALSE;
static PLHashTable *sQueueHashTable = nsnull;
static PLHashTable *sCountHashTable = nsnull;
static nsVoidArray *sEventQueueList = nsnull;

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
#ifdef DEBUG_APPSHELL
  printf("our_gdk_io_destroy()\n");
#endif
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
#ifdef DEBUG_APPSHELL
  printf("our_gdk_input_add()\n");
#endif
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

// wrapper so we can call a macro
static unsigned long getNextRequest (void *aClosure) {
  return NextRequest(GDK_DISPLAY());
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
  NS_INIT_REFCNT();
#ifdef DEBUG_APPSHELL
  printf("nsAppShell::nsAppShell()\n");
#endif
  if (!sEventQueueList)
    sEventQueueList = new nsVoidArray();
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
#ifdef DEBUG_APPSHELL
  printf("nsAppShell::~nsAppShell()\n");
#endif
  // XXX we need to free this hashtable
  //  PL_HashTableDestroy(sQueueHashTable);
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
  nsCOMPtr<nsIPref> prefs = do_GetService(kPrefServiceCID, &rv);
  if (NS_FAILED(rv) || (!prefs)) 
    return;
       
  /* first check ncols */
 
  rv = prefs->GetIntPref(PREF_NCOLS, &ivalue);
  if (NS_SUCCEEDED(rv) && ivalue >= 0 && ivalue <= 255 ) {
    if ( ivalue > 6*6*6 ) // workaround for old GdkRGB's
      ivalue = 6*6*6;
    gdk_rgb_set_min_colors( ivalue );
    return;
  }

  /* next check installcmap */

  rv = prefs->GetBoolPref(PREF_INSTALLCMAP, &bvalue);
  if (NS_SUCCEEDED(rv)) {
    if ( PR_TRUE == bvalue )
      gdk_rgb_set_install( TRUE );  // force it
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
#ifdef DEBUG_APPSHELL
  printf("nsAppShell::Create()\n");
#endif
  if (sInitialized)
    return NS_OK;

  sInitialized = PR_TRUE;

  int argc = bac ? *bac : 0;
  char **argv = bav;

  nsresult rv;

  nsCOMPtr<nsICmdLineService> cmdLineArgs = do_GetService(kCmdLineServiceCID);
  if (cmdLineArgs) {
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

  HandleColormapPrefs();
  gdk_rgb_init();

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

#ifdef DEBUG_APPSHELL
  printf("nsAppShell::Spinup()\n");
#endif

  // Get the event queue service
  nsCOMPtr<nsIEventQueueService> eventQService = do_GetService(kEventQueueServiceCID, &rv);

  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not obtain event queue service", PR_FALSE);
    return rv;
  }

  //Get the event queue for the thread.
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
  
  // If we got an event queue, use it.
  if (mEventQueue)
    goto done;

  // otherwise create a new event queue for the thread
  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) {
    NS_ASSERTION("Could not create the thread event queue", PR_FALSE);
    return rv;
  }

  // Ask again nicely for the event queue now that we have created one.
  rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));

  // XXX shouldn't this be automatic?
 done:
  ListenToEventQueue(mEventQueue, PR_TRUE);

  return rv;
}

//-------------------------------------------------------------------------
//
// Spindown - do any cleanup necessary for finishing a message loop
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsAppShell::Spindown()
{
#ifdef DEBUG_APPSHELL
  printf("nsAppShell::Spindown()\n");
#endif
  if (mEventQueue) {
    ListenToEventQueue(mEventQueue, PR_FALSE);
    mEventQueue->ProcessPendingEvents();
    mEventQueue = nsnull;
  }
  return NS_OK;
}

#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"

static gint
tm_flush_logfiles(gpointer data)
{
  NS_TraceMallocFlushLogfiles();
  return 1;
}
#endif

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

#ifdef NS_TRACE_MALLOC
  gtk_idle_add(tm_flush_logfiles, nsnull);
#endif

  // kick up gtk_main.  this won't return until gtk_main_quit is called
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

  return NS_OK;
}

#define NUMBER_HASH_KEY(_num) ((PLHashNumber) _num)

static PLHashNumber
IntHashKey(PRInt32 key)
{
  return NUMBER_HASH_KEY(key);
}

NS_IMETHODIMP nsAppShell::ListenToEventQueue(nsIEventQueue *aQueue,
                                             PRBool aListen)
{
#ifdef DEBUG_APPSHELL
  printf("ListenToEventQueue(%p, %d) this=%p\n", aQueue, aListen, this);
#endif
  if (!sQueueHashTable) {
    sQueueHashTable = PL_NewHashTable(3, (PLHashFunction)IntHashKey,
                                      PL_CompareValues, PL_CompareValues, 0, 0);
  }
  if (!sCountHashTable) {
    sCountHashTable = PL_NewHashTable(3, (PLHashFunction)IntHashKey,
                                      PL_CompareValues, PL_CompareValues, 0, 0);
  }    

  if (aListen) {
    /* add listener */
    PRInt32 key = aQueue->GetEventQueueSelectFD();

    /* only add if we arn't already in the table */
    if (!PL_HashTableLookup(sQueueHashTable, GINT_TO_POINTER(key))) {
      gint tag;
      tag = our_gdk_input_add(aQueue->GetEventQueueSelectFD(),
                              event_processor_callback,
                              aQueue,
                              G_PRIORITY_HIGH_IDLE);
      if (tag >= 0) {
        PL_HashTableAdd(sQueueHashTable, GINT_TO_POINTER(key), GINT_TO_POINTER(tag));
      }
      PLEventQueue *plqueue;
      aQueue->GetPLEventQueue(&plqueue);
      PL_RegisterEventIDFunc(plqueue, getNextRequest, 0);
      sEventQueueList->AppendElement(plqueue);
    }
    /* bump up the count */
    gint count = GPOINTER_TO_INT(PL_HashTableLookup(sCountHashTable, GINT_TO_POINTER(key)));
    PL_HashTableAdd(sCountHashTable, GINT_TO_POINTER(key), GINT_TO_POINTER(count+1));
  } else {
    /* remove listener */
    PRInt32 key = aQueue->GetEventQueueSelectFD();
    
    PLEventQueue *plqueue;
    aQueue->GetPLEventQueue(&plqueue);
    PL_UnregisterEventIDFunc(plqueue);
    sEventQueueList->RemoveElement(plqueue);

    gint count = GPOINTER_TO_INT(PL_HashTableLookup(sCountHashTable, GINT_TO_POINTER(key)));
    if (count - 1 == 0) {
      gint tag = GPOINTER_TO_INT(PL_HashTableLookup(sQueueHashTable, GINT_TO_POINTER(key)));
      if (tag > 0) {
        g_source_remove(tag);
        PL_HashTableRemove(sQueueHashTable, GINT_TO_POINTER(key));
      }
    }
    PL_HashTableAdd(sCountHashTable, GINT_TO_POINTER(key), GINT_TO_POINTER(count-1));

  }

  return NS_OK;
}

PRBool processQueue(void *aElement, void *aData)
{
  PLEventQueue *queue = (PLEventQueue *) aElement;
  unsigned int  id = NS_PTR_TO_INT32(aData);
  PL_ProcessEventsBeforeID(queue, id);
  return PR_TRUE;
}

void
nsAppShell::ProcessBeforeID(unsigned long aID)
{
  if (sEventQueueList)
    sEventQueueList->EnumerateForwards(processQueue, (void *)aID);
}
