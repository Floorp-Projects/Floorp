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
#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsMotifMenu.h"
#include "nsIImageManager.h"
#include "nsIEventQueueService.h"

#include <stdlib.h>
#include "plevent.h"

extern "C" char *fe_GetConfigDir(void) {
  printf("XXX: return /tmp for fe_GetConfigDir\n");
  return strdup("/tmp");
}

XtAppContext gAppContext; // XXX This should be changed
static nsNativeViewerApp* gTheApp;
PLEventQueue*  gUnixMainEventQueue = nsnull;

extern void nsWebShell_SetUnixEventQueue(PLEventQueue* aEventQueue);

static void nsUnixEventProcessorCallback(XtPointer aClosure, int* aFd, XtIntervalId *aId) 
{
  NS_ASSERTION(*aFd==PR_GetEventQueueSelectFD(gUnixMainEventQueue), "Error in nsUnixMain.cpp:nsUnixEventProcessCallback");
  PR_ProcessPendingEvents(gUnixMainEventQueue);
}

nsNativeViewerApp::nsNativeViewerApp()
{
}

nsNativeViewerApp::~nsNativeViewerApp()
{
}

int
nsNativeViewerApp::Run()
{
   // Setup event queue for dispatching 
   // asynchronous posts of form data + clicking on links.
   // Lifted from cmd/xfe/mozilla.c
  nsresult rv = nsnull; 
#ifdef OLD_EVENT_QUEUE
  gUnixMainEventQueue = PR_CreateEventQueue("viewer-event-queue", PR_GetCurrentThread());
  if (nsnull == gUnixMainEventQueue) {
     // Force an assertion
    NS_ASSERTION("Can not create an event loop", PR_FALSE); 
  }

   // XXX Setup webshell's event queue. This should be changed
  nsWebShell_SetUnixEventQueue(gUnixMainEventQueue);
#else
  if (nsnull == mEventQService) {
     NS_ASSERTION("No event queue service available", PR_FALSE);
     return -1;
  }
 rv = mEventQService->GetThreadEventQueue(PR_GetCurrentThread(),  &gUnixMainEventQueue);
  if (NS_OK !=  rv) {
    NS_ASSERTION("Could not obtain thread event queue", PR_FALSE);
  }


#endif  /* OLD_EVENT_QUEUE */
  XtAppAddInput(gAppContext, PR_GetEventQueueSelectFD(gUnixMainEventQueue), 
   (XtPointer)(XtInputReadMask), nsUnixEventProcessorCallback, 0);

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

static void MenuProc(PRUint32 aId) 
{
  // XXX our menus are horked: we can't support multiple windows!
  nsBrowserWindow* bw = (nsBrowserWindow*)
    nsBrowserWindow::gBrowsers.ElementAt(0);
  bw->DispatchMenuItem(aId);
}

nsresult
nsNativeBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
  CreateViewerMenus(XtParent((Widget)mWindow->GetNativeData(NS_NATIVE_WIDGET)), MenuProc);
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

int main(int argc, char **argv)
{
  // Hack to get il_ss set so it doesn't fail in xpcompat.c
  nsIImageManager *manager;
  NS_NewImageManager(&manager);

  gTheApp = new nsNativeViewerApp();
  gTheApp->Initialize(argc, argv);
  gTheApp->Run();

  return 0;
}


