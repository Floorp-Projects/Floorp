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
#include "nsWidgetsCID.h"
#include "nsIImageManager.h"
#include "nsIComponentManager.h"
#include "nsIMenuBar.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include <stdlib.h>
#include "plevent.h"
#include "resources.h"

#include <photon/Pg.h>
#include <Pt.h>

#define PRINTF printf

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
  PRINTF( ">>> nsViewerApp::Run() <<<\n" );
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

static NS_DEFINE_IID( kMenuBarCID, 	NS_MENUBAR_CID );
static NS_DEFINE_IID( kIMenuBarIID, NS_IMENUBAR_IID );
static NS_DEFINE_IID( kMenuCID, 	NS_MENU_CID );
static NS_DEFINE_IID( kIMenuIID, 	NS_IMENU_IID );
static NS_DEFINE_IID( kMenuItemCID, NS_MENUITEM_CID );
static NS_DEFINE_IID( kIMenuItemIID,NS_IMENUITEM_IID );

extern void  CreateViewerMenus(PtWidget_t*, void *);

nsresult
nsNativeBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
  PtWidget_t *mMenuBar= nsnull;
  void        *voidData;

  voidData = mWindow->GetNativeData(NS_NATIVE_WINDOW);
  mMenuBar = PtCreateWidget( PtMenuBar, (PtWidget_t *) voidData , 0, NULL);

  if (mMenuBar)
  {
      mWindow->ShowMenuBar( PR_TRUE );
      ::CreateViewerMenus(mMenuBar,this);
  }
  return NS_OK;
}

nsresult
nsNativeBrowserWindow::GetMenuBarHeight(PRInt32 * aHeightOut)
{
  NS_ASSERTION(nsnull != aHeightOut,"null out param.");

  *aHeightOut = 31;

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

