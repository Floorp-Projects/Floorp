/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include <qwidget.h>

#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsQtMenu.h"
#include <stdlib.h>
#include "plevent.h"
#include "nsIServiceManager.h"
#include "nsGfxCIID.h"

static nsNativeViewerApp* gTheApp;

nsNativeViewerApp::nsNativeViewerApp()
{
}

nsNativeViewerApp::~nsNativeViewerApp()
{
}

int nsNativeViewerApp::Run()
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

nsresult nsNativeBrowserWindow::InitNativeWindow()
{
    // override to do something special with platform native windows
    return NS_OK;
}

static void MenuProc(PRUint32 aId) 
{
    // XXX our menus are horked: we can't support multiple windows!
    nsBrowserWindow* bw = (nsBrowserWindow*)
        nsBrowserWindow::gBrowsers->ElementAt(0);
    bw->DispatchMenuItem(aId);
}

PRInt32 gMenuBarHeight = 0;

nsresult
nsNativeBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
    CreateViewerMenus((QWidget*)mWindow->GetNativeData(NS_NATIVE_WIDGET),
                      this,
                      &gMenuBarHeight);

    return NS_OK;
}

nsresult
nsNativeBrowserWindow::GetMenuBarHeight(PRInt32 * aHeightOut)
{
  NS_ASSERTION(nsnull != aHeightOut,"null out param.");

  *aHeightOut = gMenuBarHeight;

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
  gTheApp = new nsNativeViewerApp();
  
  putenv("MOZ_TOOLKIT=qt");
  
  gTheApp->Initialize(argc, argv);
  gTheApp->Run();
  
  return 0;
}


