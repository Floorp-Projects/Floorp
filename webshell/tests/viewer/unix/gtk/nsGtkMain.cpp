/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include <gtk/gtk.h>

#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsGtkMenu.h"
#include "nsIServiceManager.h"
#include "nsIImageManager.h"
#include "nsIThread.h"
#include "plevent.h"
#include "prinit.h"
#include "prlog.h"

static nsNativeViewerApp* gTheApp;

nsNativeViewerApp::nsNativeViewerApp()
{
}

nsNativeViewerApp::~nsNativeViewerApp()
{
}

int
nsNativeViewerApp::Run()
{
  OpenWindow();
  // XXX mAppShell can be set to NULL while mAppShell->Run() is running
  // because nsViewerApp::Exit() is called.  I don't know what should
  // happen, but this is a workaround that neither crashes nor leaks.
  // See <URL: http://bugzilla.mozilla.org/show_bug.cgi?id=28557 >
  nsIAppShell* theAppShell = mAppShell;
  NS_ADDREF(theAppShell);
  theAppShell->Run();
  NS_RELEASE(theAppShell);
  return 0;
}

//----------------------------------------------------------------------

nsNativeBrowserWindow::nsNativeBrowserWindow()
{
}

nsNativeBrowserWindow::~nsNativeBrowserWindow()
{
}

nsresult
nsNativeBrowserWindow::InitNativeWindow()
{
	// override to do something special with platform native windows
  return NS_OK;
}

static GtkWidget * sgHackMenuBar = nsnull;

nsresult
nsNativeBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
  CreateViewerMenus(mWindow,this,&sgHackMenuBar);
  return NS_OK;
}

nsresult
nsNativeBrowserWindow::GetMenuBarHeight(PRInt32 * aHeightOut)
{
  NS_ASSERTION(nsnull != aHeightOut,"null out param.");

  *aHeightOut = 0;

  if (nsnull != sgHackMenuBar)
  {
    *aHeightOut = sgHackMenuBar->allocation.height;
  }

  return NS_OK;
}

nsEventStatus
nsNativeBrowserWindow::DispatchMenuItem(PRInt32 aID)
{
  // Dispatch motif-only menu code goes here

  // Dispatch xp menu items
  return nsBrowserWindow::DispatchMenuItem(aID);
}

//----------------------------------------------------------------------


#ifdef DEBUG_ramiro
#define CRAWL_STACK_ON_SIGSEGV
#endif // DEBUG_ramiro


#ifdef CRAWL_STACK_ON_SIGSEGV

#include <signal.h>
#include <unistd.h>
#include "nsTraceRefcnt.h"

extern "C" char * strsignal(int);

static char _progname[1024] = "huh?";

void
ah_crap_handler(int signum)
{
  PR_CurrentThread();

  printf("prog = %s\npid = %d\nsignal = %s\n",
         _progname,
         getpid(),
         strsignal(signum));
  
  printf("stack logged to someplace\n");
  nsTraceRefcnt::WalkTheStack(stdout);

  printf("Sleeping for 5 minutes.\n");
  printf("Type 'gdb %s %d' to attatch your debugger to this thread.\n",
         _progname,
         getpid());

  sleep(300);

  printf("Done sleeping...\n");
} 
#endif // CRAWL_STACK_ON_SIGSEGV

int main(int argc, char **argv)
{
#ifdef CRAWL_STACK_ON_SIGSEGV
  strcpy(_progname,argv[0]);
  signal(SIGSEGV, ah_crap_handler);
  signal(SIGILL, ah_crap_handler);
  signal(SIGABRT, ah_crap_handler);
#endif // CRAWL_STACK_ON_SIGSEGV

  // Initialize XPCOM
  nsresult rv = NS_InitXPCOM(nsnull, nsnull);
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_InitXPCOM failed");
  if (NS_SUCCEEDED(rv)) {
    // The toolkit service in mozilla will look in the environment
    // to determine which toolkit to use.  Yes, it is a dumb hack to
    // force it here, but we have no choice because of toolkit specific
    // code linked into the viewer.
    putenv("MOZ_TOOLKIT=gtk");

    gTheApp = new nsNativeViewerApp();
    gTheApp->Initialize(argc, argv);
    gTheApp->Run();
    delete gTheApp;

     // Shutdown XPCOM
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

    // XXX This is disabled because sometimes it hangs waiting for a
    // netlib thread to exit, which isn't exiting :-(. Need more
    // shutdown logic in necko first.
#if 0
    // Shutdown NSPR
    PR_LogFlush();
    PR_Cleanup();
#endif
  }

  return 0;
}
