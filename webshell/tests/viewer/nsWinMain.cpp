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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Michael Lowe <michael.lowe@bigfoot.com>
 */
#include <windows.h>
#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsITimer.h"
#include "JSConsole.h"
#include "plevent.h"
#include "nsIServiceManager.h"
#include "nsIThread.h"
#include "nsWidgetsCID.h"
#include "nsTimerManager.h"

JSConsole *gConsole;
HINSTANCE gInstance, gPrevInstance;
static nsITimer* gNetTimer;
static NS_DEFINE_CID(kTimerManagerCID, NS_TIMERMANAGER_CID);

nsNativeViewerApp::nsNativeViewerApp()
{
}

nsNativeViewerApp::~nsNativeViewerApp()
{
}


int
nsNativeViewerApp::Run() 
{
  if (mJustShutdown)
    return 0;

  OpenWindow();
 
  // Process messages
  MSG  msg;
  int  keepGoing = 1;

  nsresult rv;
  NS_WITH_SERVICE(nsITimerQueue, queue, kTimerManagerCID, &rv);
  if (NS_FAILED(rv)) return 0;

  // Pump all messages
  do {
    // Give priority to system messages (in particular keyboard, mouse,
    // timer, and paint messages).
    // Note: on Win98 and NT 5.0 we can also use PM_QS_INPUT and PM_QS_PAINT flags.
    if (::PeekMessage(&msg, NULL, 0, WM_USER-1, PM_REMOVE) || 
      ::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

      keepGoing = (msg.message != WM_QUIT);

      // If we successfully retrieved a message then dispatch it
      if (keepGoing >= 0) {
        if (!JSConsole::sAccelTable || !gConsole || !gConsole->GetMainWindow() ||
          !TranslateAccelerator(gConsole->GetMainWindow(), JSConsole::sAccelTable, &msg)) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
      }

    // process timer queue.
    } else if (queue->HasReadyTimers(NS_PRIORITY_LOWEST)) {

      do {
        //printf("fire\n");
        queue->FireNextReadyTimer(NS_PRIORITY_LOWEST);
      } while (queue->HasReadyTimers(NS_PRIORITY_LOWEST) && 
              !::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE));

    } else {
       // Block and wait for any posted application message
      ::WaitMessage();
    }
  } while (keepGoing != 0);

  return msg.wParam;
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

nsresult
nsNativeBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
  HMENU menu = ::LoadMenu(gInstance, "Viewer");
  HWND hwnd = (HWND)mWindow->GetNativeData(NS_NATIVE_WIDGET);
  ::SetMenu(hwnd, menu);

  return NS_OK;
}

nsresult
nsNativeBrowserWindow::GetMenuBarHeight(PRInt32 * aHeightOut)
{
  NS_ENSURE_ARG_POINTER(aHeightOut);

  *aHeightOut = 0;

  return NS_OK;
}

nsEventStatus
nsNativeBrowserWindow::DispatchMenuItem(PRInt32 aID)
{
  // Dispatch windows-only menu code goes here

  // Dispatch xp menu items
  return nsBrowserWindow::DispatchMenuItem(aID);
}

//----------------------------------------------------------------------

int main(int argc, char **argv)
{
  nsresult rv;
  rv = NS_InitXPCOM(nsnull, nsnull);
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_InitXPCOM failed");
  nsViewerApp* app = new nsNativeViewerApp();
  NS_ADDREF(app);
  app->Initialize(argc, argv);
  int result = app->Run();
  app->Exit();  // this exit is needed for the -x case where the close box is never clicked
  NS_RELEASE(app);
  rv = NS_ShutdownXPCOM(nsnull);
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
  return result;
}

int PASCAL
WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParam, 
        int nCmdShow)
{
  gInstance = instance;
  gPrevInstance = prevInstance;
  return main(0, nsnull);
}
