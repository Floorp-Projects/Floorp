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
#ifdef XP_PC
#include <windows.h>
#include "JSConsole.h"
#endif
#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsITimer.h"
#include "plevent.h"

#ifdef XP_PC
JSConsole *gConsole;
HINSTANCE gInstance, gPrevInstance;
#endif

static nsITimer* gNetTimer;

/*nsNativeViewerApp::nsNativeViewerApp() 
{
}

nsNativeViewerApp::~nsNativeViewerApp()
{
}

int
nsNativeViewerApp::Run()
{
  OpenWindow();
 
  // Process messages
  MSG msg;
  while (::GetMessage(&msg, NULL, 0, 0)) {
    if (!JSConsole::sAccelTable ||
        !gConsole ||
        !gConsole->GetMainWindow() ||
        !TranslateAccelerator(gConsole->GetMainWindow(),
                              JSConsole::sAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  return msg.wParam;
}
*/
//----------------------------------------------------------------------

/*nsNativeBrowserWindow::nsNativeBrowserWindow()
{
}

nsNativeBrowserWindow::~nsNativeBrowserWindow()
{
}

nsresult
nsBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
  HMENU menu = ::LoadMenu(gInstance, "Viewer");
  HWND hwnd = (HWND)mWindow->GetNativeData(NS_NATIVE_WIDGET);
  ::SetMenu(hwnd, menu);
  


  return NS_OK;
}

nsEventStatus
nsNativeBrowserWindow::DispatchMenuItem(PRInt32 aID)
{
  // Dispatch windows-only menu code goes here

  // Dispatch xp menu items
  return nsBrowserWindow::DispatchMenuItem(aID);
}*/

//----------------------------------------------------------------------

int main(int argc, char **argv)
{
#ifdef XP_PC
  PL_InitializeEventsLib("");
#endif

  nsViewerApp* app = new nsViewerApp();
  NS_ADDREF(app);
  /* we should, um, check for failure */
  if (app->Initialize(argc, argv) != NS_OK)
    return 0;
  app->OpenWindow();
  app->Run();
  NS_RELEASE(app);

  return 0;
}

#ifdef XP_PC
int PASCAL
WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdParam, 
        int nCmdShow)
{
  gInstance = instance;
  gPrevInstance = prevInstance;
  PL_InitializeEventsLib("");
  nsViewerApp* app = new nsViewerApp();
  app->Initialize(0, nsnull);
  app->OpenWindow();

  int result = app->Run();
  delete app;
  return result;
}
#endif
