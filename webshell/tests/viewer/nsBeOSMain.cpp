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
 */
#include <signal.h>
#include <be/app/Application.h>

#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
//#include "JSConsole.h"
#include "nsBrowserWindow.h"
#include "nsIImageManager.h"
#include <stdlib.h>
#include "plevent.h"

//#include "nsTimer.h"
//#include "nsITimer.h"

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

	if(mAppShell)
		mAppShell->Run();

	return NS_OK;
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
	printf("nsNativeBrowserWindow:: CreateMenuBar not implemented\n");
#if 0
  HMENU menu = ::LoadMenu(gInstance, "Viewer");
  HWND hwnd = (HWND)mWindow->GetNativeData(NS_NATIVE_WIDGET);
  ::SetMenu(hwnd, menu);
#endif

  return NS_OK;
}

nsresult
nsNativeBrowserWindow::GetMenuBarHeight(PRInt32 * aHeightOut)
{
  NS_ASSERTION(nsnull != aHeightOut,"null out param.");

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

class nsViewerBeOSApp : public BApplication
{
public:
  nsViewerBeOSApp(int argc, char **argv) 
    : BApplication("application/x-vnd.mozilla.viewer") {

    if (!viewer_app) {
      viewer_app = new nsNativeViewerApp();
    }

    if (viewer_app) {
      NS_ADDREF(viewer_app);
      viewer_app->Initialize(argc, argv);
      viewer_app->Run();
      NS_RELEASE(viewer_app);
    }
  }

private:
  nsViewerApp *viewer_app;

};

//----------------------------------------------------------------------

static nsViewerBeOSApp *beos_app = 0;

static void beos_signal_handler(int signum) {
  fprintf(stderr, "beos_signal_handler: %d\n", signum);
  if (beos_app) {
    beos_app->Quit();
  }
}

int main(int argc, char **argv)
{
  signal(SIGTERM, beos_signal_handler);

  beos_app = new nsViewerBeOSApp(argc, argv);

  if (beos_app)
    beos_app->Run();
  else
    return B_NO_MEMORY;

  return 0;
}
