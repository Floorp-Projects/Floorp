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

#include "nsBrowserWindow.h"
#include "nsViewerApp.h"

#include "nsIUnixToolkitService.h"
#include "nsIComponentManager.h"

static NS_DEFINE_CID(kCUnixToolkitServiceCID, NS_UNIX_TOOLKIT_SERVICE_CID);

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

nsNativeBrowserWindow::nsNativeBrowserWindow()
{
}

nsNativeBrowserWindow::~nsNativeBrowserWindow()
{
}

nsresult
nsNativeBrowserWindow::InitNativeWindow()
{
  return NS_OK;
}

nsresult
nsNativeBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
  return NS_OK;
}

nsEventStatus
nsNativeBrowserWindow::DispatchMenuItem(PRInt32 aID)
{
  return nsEventStatus_eIgnore;
}

int main (int argc, char **argv)
{
  nsViewerApp *app = new nsNativeViewerApp();
  NS_ADDREF(app);

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
    // Force the toolkit into "xlib" mode
    unixToolkitService->SetToolkitName("xlib");
    
    NS_RELEASE(unixToolkitService);
  }

  //////////////////////////////////////////////////////////////////////
  // End toolkit service setup
  //////////////////////////////////////////////////////////////////////

  app->Initialize(argc, argv);
  app->Run();
  NS_RELEASE(app);

  return 0;
}
