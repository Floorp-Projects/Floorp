/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include <gtk/gtk.h>

/* oops.  gdkx.h doesn't have extern "C" around it. */
extern "C" {
#include <gdk/gdkx.h>
}

#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIXlibWindowService.h"
#include "nsIUnixToolkitService.h"
#include "nsIWebShell.h"
#include "nsRepository.h"
#include "nsIPref.h"
#include "xlibrgb.h"

static NS_DEFINE_IID(kIEventQueueServiceIID,
                     NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kEventQueueServiceCID,
                     NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kWindowServiceCID,
		     NS_XLIB_WINDOW_SERVICE_CID);
static NS_DEFINE_IID(kWindowServiceIID,
		     NS_XLIB_WINDOW_SERVICE_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCUnixToolkitServiceCID, NS_UNIX_TOOLKIT_SERVICE_CID);

extern "C" void NS_SetupRegistry();

static GdkFilterReturn test_filter (GdkXEvent *gdk_xevent,
				    GdkEvent  *event,
				    gpointer   data);

static void event_processor_callback(gpointer data,
                                     gint source,
                                     GdkInputCondition condition)
{
  printf("event_processor_callback()\n");
  nsIEventQueue *eventQueue = (nsIEventQueue*)data;
  eventQueue->ProcessPendingEvents();
}

static void WindowCreateCallback(PRUint32 aID)
{
  GdkWindow *window;

  // import that into gdk
  window = gdk_window_foreign_new((guint32)aID);

  // attach it to a filter
  gdk_window_add_filter(window, test_filter, NULL);

  printf("window created\n");
}

static void WindowDestroyCallback(PRUint32 aID)
{
  printf("window destroyed\n");
}

static nsXlibEventDispatcher gsEventDispatcher = nsnull;

static nsIXlibWindowService *  gsWindowService = nsnull;
static nsIWebShell *           sgWebShell = nsnull;
static nsIPref *               sgPrefs = nsnull;


int main(int argc, char **argv) 
{
  GtkWidget * main_window = NULL;

  gtk_init(&argc, &argv);

  nsresult   rv;

  // init xlibrgb
  // XXX this is a hack, will replace with a service RSN
  xlib_rgb_init(GDK_DISPLAY(), DefaultScreenOfDisplay(GDK_DISPLAY()));

  main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_widget_set_usize(main_window, 500, 500);

  gtk_widget_realize(main_window);


  //////////////////////////////////////////////////////////////////////
  //
  // Toolkit Service setup
  // 
  // Note: This must happend before NS_SetupRegistry() is called so
  //       that the toolkit specific xpcom components can be registered
  //       as needed.
  //
  //////////////////////////////////////////////////////////////////////
  nsIUnixToolkitService * unixToolkitService = nsnull;
    
  rv = nsComponentManager::CreateInstance(kCUnixToolkitServiceCID,
                                          nsnull,
                                          nsIUnixToolkitService::GetIID(),
                                          (void **) &unixToolkitService);
  
  NS_ASSERTION(NS_SUCCEEDED(rv),"Cannot obtain unix toolkit service.");

  if (!NS_SUCCEEDED(rv))
	return 1;

  // Force the toolkit into "xlib" mode regardless of MOZ_TOOLKIT
  unixToolkitService->SetToolkitName("xlib");

  NS_RELEASE(unixToolkitService);

  //////////////////////////////////////////////////////////////////////
  // End toolkit service setup
  //////////////////////////////////////////////////////////////////////


  //////////////////////////////////////////////////////////////////////
  //
  // Setup the registry
  //
  //////////////////////////////////////////////////////////////////////
  NS_SetupRegistry();

  printf("Creating event queue.\n");
    
  nsIEventQueueService * eventQueueService = nsnull;
  nsIEventQueue * eventQueue = nsnull;
  
  // Create the Event Queue for the UI thread...
  
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
									kIEventQueueServiceIID,
									(nsISupports **)&eventQueueService);
  
  NS_ASSERTION(NS_SUCCEEDED(rv),"Could not obtain the event queue service.");

  if (!NS_SUCCEEDED(rv))
	return 1;

  rv = eventQueueService->CreateThreadEventQueue();
  
  NS_ASSERTION(NS_SUCCEEDED(rv),"Could not create the event queue for the the thread.");
  
  if (!NS_SUCCEEDED(rv))
	return 1;
  
  rv = eventQueueService->GetThreadEventQueue(PR_GetCurrentThread(), &eventQueue);
  
  NS_ASSERTION(NS_SUCCEEDED(rv),"Could not get the newly created thread event queue.\n");
  
  if (!NS_SUCCEEDED(rv))
	return 1;
  
  NS_RELEASE(eventQueueService);
  
  rv = nsServiceManager::GetService(kWindowServiceCID,
				    kWindowServiceIID,
				    (nsISupports **)&gsWindowService);

  NS_ASSERTION(NS_SUCCEEDED(rv),"Couldn't obtain window service\n");

  if (!NS_SUCCEEDED(rv))
	return 1;

  gsWindowService->SetWindowCreateCallback(WindowCreateCallback);
  gsWindowService->SetWindowDestroyCallback(WindowDestroyCallback);

  gdk_input_add(eventQueue->GetEventQueueSelectFD(),
				GDK_INPUT_READ,
				event_processor_callback,
				eventQueue);

  gtk_widget_show(main_window);

  rv = nsRepository::CreateInstance(kWebShellCID, nsnull,
				    kIWebShellIID,
				    (void**)&sgWebShell);

  NS_ASSERTION(NS_SUCCEEDED(rv),"Cannot create WebShell.\n");

  if (!NS_SUCCEEDED(rv))
	return 1;

  sgWebShell->Init((nsNativeWidget *)GDK_WINDOW_XWINDOW(main_window->window),
		  0, 0,
		  500, 500);

  gsWindowService->GetEventDispatcher(&gsEventDispatcher);

  rv = nsComponentManager::CreateInstance(kPrefCID, NULL, kIPrefIID,
					  (void **) &sgPrefs);

  if (NS_OK != rv) {
    printf("failed to get prefs instance\n");
    return rv;
  }
  
  sgPrefs->StartUp();
  sgPrefs->ReadUserPrefs();

  sgWebShell->SetPrefs(sgPrefs);
  sgWebShell->Show();
  
  char *url = "http://www.slashdot.org/";

  nsString URL(url);
  PRUnichar *u_url = URL.ToNewUnicode();
  sgWebShell->LoadURL(u_url);
  
  gtk_main();

  return 0;
}


static GdkFilterReturn test_filter (GdkXEvent *gdk_xevent,
				    GdkEvent  *event,
				    gpointer   data)
{
  printf("test_filter called\n");
  XEvent *xevent;
  xevent = (XEvent *)gdk_xevent;

  (*gsEventDispatcher)((nsXlibNativeEvent) xevent);

//   gsWindowService->DispatchNativeXlibEvent((void *)xevent);

  return GDK_FILTER_REMOVE;
}

