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

#include "nsIUnixToolkitService.h"
#include "nsIComponentManager.h"

static NS_DEFINE_CID(kCUnixToolkitServiceCID, NS_UNIX_TOOLKIT_SERVICE_CID);

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

static void MenuProc(PRUint32 aID) 
{
  //  return nsBrowserWindow::DispatchMenuItem(aID);
  return;
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
  //  return nsBrowserWindow::DispatchMenuItem(aID);
  return nsEventStatus_eIgnore;
}


//----------------------------------------------------------------------

int main(int argc, char **argv)
{
  // Hack to get il_ss set so it doesn't fail in xpcompat.c
  nsIImageManager *manager;
  NS_NewImageManager(&manager);

  gTheApp = new nsNativeViewerApp();

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
    
  nsresult rv = 
    nsComponentManager::CreateInstance(kCUnixToolkitServiceCID,
                                       nsnull,
                                       nsIUnixToolkitService::GetIID(),
                                       (void **) &unixToolkitService);
  
  NS_ASSERTION(NS_SUCCEEDED(rv),"Cannot obtain unix toolkit service.");

  if (NS_SUCCEEDED(rv) && (nsnull != unixToolkitService))
  {
    // Force the toolkit into "motif" mode regardless of MOZ_TOOLKIT
    unixToolkitService->SetToolkitName("motif");
    
    NS_RELEASE(unixToolkitService);
  }

  //////////////////////////////////////////////////////////////////////
  // End toolkit service setup
  //////////////////////////////////////////////////////////////////////

  gTheApp->Initialize(argc, argv);
  gTheApp->Run();

  return 0;
}
