/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
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
#include "nsIContentViewer.h"
#include "nsIComponentManager.h"
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

void handle_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer p);

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
                                          NS_GET_IID(nsIUnixToolkitService),
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
  
  rv = eventQueueService->GetThreadEventQueue(NS_CURRENT_THREAD, &eventQueue);
  
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

  // XXX - fix me!
  //sgWebShell->Init((nsNativeWidget *)GDK_WINDOW_XWINDOW(main_window->window),
  //		  0, 0,
  //500, 500);
  NS_ASSERTION(PR_FALSE, "Please fix this code.");

  gsWindowService->GetEventDispatcher(&gsEventDispatcher);

  rv = nsServiceManager::GetService(kPrefCID, kIPrefIID,
                                    (nsISupports**)&sgPrefs);

  if (NS_OK != rv) {
    printf("failed to get prefs instance\n");
    return rv;
  }
  
  sgPrefs->StartUp();
  sgPrefs->ReadUserPrefs();

  nsIContentViewer *content_viewer=nsnull;
  // XXX fix me!
  //rv = sgWebShell->GetContentViewer(&content_viewer);
  NS_ASSERTION(PR_FALSE, "Please fix this code.");
  if (NS_SUCCEEDED(rv) && content_viewer) {
    content_viewer->Show();
    NS_RELEASE(content_viewer);
  }
    
  //  sgWebShell->Show();

  // attach the size_allocate signal to the main window
  gtk_signal_connect_after(GTK_OBJECT(main_window),
                           "size_allocate",
                           GTK_SIGNAL_FUNC(handle_size_allocate),
                           sgWebShell);
  
  char *url = "http://www.mozilla.org/unix/xlib.html";

  nsString URL(url);
  PRUnichar *u_url = URL.ToNewUnicode();
  // XXX fix me - what's the new API?
  //sgWebShell->LoadURL(u_url);
  
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

// this will pick up size changes in the main window and resize
// the web shell window accordingly

void handle_size_allocate(GtkWidget *w, GtkAllocation *alloc, gpointer p)
{
  printf("handling size allocate\n");
  nsIWebShell *moz_widget = (nsIWebShell *)p;
  nsIContentViewer *content_viewer=nsnull;
  nsresult rv=NS_OK;
 
  // XXX - fix me!
  //  rv = moz_widget->GetContentViewer(&content_viewer);
  NS_ASSERTION(PR_FALSE, "Please fix this code.");
  if (NS_SUCCEEDED(rv) && content_viewer) {
    nsRect bounds(0,0, alloc->width, alloc->height);
    
    content_viewer->SetBounds(bounds);
    NS_RELEASE(content_viewer);
  }
}
