/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *	Chris Seawood <cls@seawood.org>
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
#include <signal.h>
#include <be/app/Application.h>

#include "nsIAppShell.h"
#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsBrowserWindow.h"
#include <stdlib.h>
#include "plevent.h"

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

	if(mAppShell) {
		nsIAppShell *theAppShell = mAppShell;
		NS_ADDREF(theAppShell);
		mAppShell->Run();
		NS_RELEASE(theAppShell);
	}

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

nsresult
nsNativeBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
	printf("nsNativeBrowserWindow:: CreateMenuBar not implemented\n");

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

class nsBeOSApp : public BApplication
{
public:
  nsBeOSApp(sem_id sem)
    : BApplication("application/x-vnd.mozilla.viewer"), init(sem) { }

	void ReadyToRun(void) {
		release_sem(init);
	}

	static int32 Main(void *args) {
		nsBeOSApp *app = new nsBeOSApp((sem_id)args);
		if (!app)
			return B_ERROR;
		return app->Run();
	}

private:
	sem_id init;
};

//----------------------------------------------------------------------
static nsNativeViewerApp *NVApp = 0;

static void beos_signal_handler(int signum) {
#ifdef DEBUG
	fprintf(stderr, "beos_signal_handler: %d\n", signum);
#endif

	// Exit native appshell loop so that viewer will shutdown normally
	if (NVApp)
		NVApp->Exit();
}

static nsresult InitializeBeOSApp(void)
{
	nsresult rv = NS_OK;

	sem_id initsem = create_sem(0, "beapp init");
	if (initsem < B_OK)
		return NS_ERROR_FAILURE;

	thread_id tid = spawn_thread(nsBeOSApp::Main, "BApplication", 
					B_NORMAL_PRIORITY, (void *)initsem);
	if (tid < B_OK || B_OK != resume_thread(tid))
		rv = NS_ERROR_FAILURE;

	if (B_OK != acquire_sem(initsem))
		rv = NS_ERROR_FAILURE;
	if (B_OK != delete_sem(initsem))
		rv = NS_ERROR_FAILURE;

	return rv;
}


int main(int argc, char **argv)
{
	signal(SIGTERM, beos_signal_handler);

	if (NS_OK != InitializeBeOSApp())
		return 1;

	// Init XPCOM
	nsresult rv = NS_InitXPCOM(nsnull, nsnull);
	NS_ASSERTION(NS_SUCCEEDED(rv), "NS_InitXPCOM failed");
	if (NS_SUCCEEDED(rv)) {

		// Run viewer app
		NVApp = new nsNativeViewerApp();
		if (!NVApp) {
#ifdef DEBUG
			fprintf(stderr, "Could not allocate mem for nsNativeViewerApp\n");
#endif
			// Shutdown XPCOM
			rv = NS_ShutdownXPCOM(nsnull);
			return 1;
		}
		NVApp->Initialize(argc,argv);
		NVApp->Run();
		delete NVApp;
		NVApp = 0;

		// Shutdown XPCOM
		rv = NS_ShutdownXPCOM(nsnull);
		NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");
	}

	// Delete BApplication
	if (be_app->Lock())
		be_app->Quit();
	delete be_app;

	return 0;
}

