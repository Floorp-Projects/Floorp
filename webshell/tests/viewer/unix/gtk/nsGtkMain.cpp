/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include <gtk/gtk.h>

#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsGtkMenu.h"
#include "nsIImageManager.h"
#include "plevent.h"

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
  mAppShell->Run();
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
  
  char stack[4096];
  
  nsTraceRefcnt::WalkTheStack(stack, sizeof(stack));
  
  // Convert all spaces between symbols to newlines for readability
  char * needle  = "+0x";
  char * haystack  = stack;
  char * sp = NULL;
  
  while((sp = strstr(haystack,needle)))
  {
    char * ws = strchr(sp,' ');

    if (ws)
    {
      *ws = '\n';

      haystack = ws + 1;
    }
  }

  printf("stack = %s\n\n",stack);

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

  // Hack to get il_ss set so it doesn't fail in xpcompat.c
  nsIImageManager *manager;
  NS_NewImageManager(&manager);

  gTheApp = new nsNativeViewerApp();

  // Damn, there's no PR_PutEnv() ???
  //PR_PutEnv("MOZ_TOOLKIT=gtk");

  // The toolkit service in mozilla will look in the environment
  // to determine which toolkit to use.  Yes, it is a dumb hack to
  // force it here, but we have no choice because of toolkit specific
  // code linked into the viewer.
  putenv("MOZ_TOOLKIT=gtk");

  gTheApp->Initialize(argc, argv);
  gTheApp->Run();

  return 0;
}


