/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsWidgetsCID.h"
#include "nsIServiceManager.h"
#include "nsIImageManager.h"
#include "nsIComponentManager.h"
#include "nsIMenuBar.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIThread.h"

#include <stdlib.h>
#include <signal.h>

#include "plevent.h"
#include "resources.h"

#include <photon/Pg.h>
#include <Pt.h>

extern "C" char * strsignal(int);

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


void abnormal_exit_handler(int signum)
{
  /* Free any shared memory that has been allocated */
  PgShmemCleanup();

#if 1
  if (    (signum == SIGSEGV)
       || (signum == SIGILL)
	   || (signum == SIGABRT)
	 )
  {
    PR_CurrentThread();
    printf("prog = viewer\npid = %d\nsignal = %s\n", getpid(), strsignal(signum));

#if 0
    printf("stack logged to someplace\n");
    printf("need to fix xpcom/base/nsTraceRefCnt.cpp in WalkTheStack.\n");
    nsTraceRefcnt::WalkTheStack(stdout);
#endif

    printf("Sleeping for 5 minutes.\n");
    printf("Type 'gdb viewer %d' to attatch your debugger to this thread.\n", getpid());
    sleep(300);
    printf("Done sleeping...\n");
  }
#endif

  _exit(1);
} 

//----------------------------------------------------------------------
int main(int argc, char **argv)
{

  /* I need this to free shared memory in case of a crash */
  signal(SIGTERM, abnormal_exit_handler);
  signal(SIGQUIT, abnormal_exit_handler);
  signal(SIGINT,  abnormal_exit_handler);
  signal(SIGHUP,  abnormal_exit_handler);
  signal(SIGSEGV, abnormal_exit_handler);
  signal(SIGILL,  abnormal_exit_handler);
  signal(SIGABRT, abnormal_exit_handler);

  // Initialize XPCOM
  nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
  NS_ASSERTION(NS_SUCCEEDED(rv), "NS_InitXPCOM failed");
  if (NS_SUCCEEDED(rv)) {
    // The toolkit service in mozilla will look in the environment
    // to determine which toolkit to use.  Yes, it is a dumb hack to
    // force it here, but we have no choice because of toolkit specific
    // code linked into the viewer.
    putenv("MOZ_TOOLKIT=photon");
	
    gTheApp = new nsNativeViewerApp();
    gTheApp->Initialize(argc, argv);
    gTheApp->Run();
    delete gTheApp;

    NS_FreeImageManager();

    // Shutdown XPCOM
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
  }
  
  return 0;
}

