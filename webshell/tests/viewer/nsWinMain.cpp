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
#include <windows.h>
#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsITimer.h"
#include "plevent.h"

extern "C" int  NET_PollSockets();

static HANDLE gInstance, gPrevInstance;
static nsITimer* gNetTimer;

nsNativeViewerApp::nsNativeViewerApp()
{
}

nsNativeViewerApp::~nsNativeViewerApp()
{
}

//----------------------------------------------------------------------

nsNativeBrowserWindow::nsNativeBrowserWindow()
{
}

nsNativeBrowserWindow::~nsNativeBrowserWindow()
{
}

nsresult
nsNativeBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
  HMENU menu = ::LoadMenu(gInstance, "Viewer");
  HWND hwnd = mWindow->GetNativeData(NS_NATIVE_WIDGET);
  ::SetMenu(hwnd, menu);

  return NS_OK;
}

nsEventStatus
nsNativeBrowserWindow::DispatchMenuItem(nsGUIEvent* aEvent)
{
  // Dispatch windows-only menu code goes here

  // Dispatch xp menu items
  return nsBrowserWindow::DispatchMenuItem(aEvent);
}

//----------------------------------------------------------------------

static void
PollNet(nsITimer *aTimer, void *aClosure)
{
  NET_PollSockets();
  NS_IF_RELEASE(gNetTimer);
  if (NS_OK == NS_NewTimer(&gNetTimer)) {
    gNetTimer->Init(PollNet, nsnull, 1000 / 50);
  }
}

static int
RunViewer(HANDLE instance, HANDLE prevInstance, int nCmdShow,
          nsViewerApp* app)
{
  gInstance = instance;
  gPrevInstance = prevInstance;

  app->OpenWindow();

//  SetViewer(aViewer);
//  nsIWidget *mainWindow = nsnull;
//  nsDocLoader* dl = aViewer->SetupViewer(&mainWindow, 0, 0);
 
  // Process messages
  MSG msg;
  PollNet(0, 0);
  while (::GetMessage(&msg, NULL, 0, 0)) {
    if (
#if 0
!JSConsole::sAccelTable ||
        !gConsole ||
        !gConsole->GetMainWindow() ||
        !TranslateAccelerator(gConsole->GetMainWindow(), JSConsole::sAccelTable, &msg)
#endif
        1
      ) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      NET_PollSockets();
    }
  }

//  aViewer->CleanupViewer(dl);

  return msg.wParam;
}

void main(int argc, char **argv)
{
  PL_InitializeEventsLib("");
  nsViewerApp* app = new nsNativeViewerApp();
  app->Initialize(argc, argv);
  RunViewer(GetModuleHandle(NULL), NULL, SW_SHOW, app);
  delete app;
}

int PASCAL
WinMain(HANDLE instance, HANDLE prevInstance, LPSTR cmdParam, int nCmdShow)
{
  PL_InitializeEventsLib("");
  nsViewerApp* app = new nsNativeViewerApp();
  app->Initialize(0, nsnull);
  int rv = RunViewer(instance, prevInstance, nCmdShow, app);
  delete app;
  return rv;
}
