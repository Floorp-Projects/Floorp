/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 04/05/2000  IBM Corp.       Fixed undefined identifier.
 *                              
 *
 */

#define INCL_PM
#define INCL_DOS
#include <os2.h>

#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsIAppShell.h"
#include "resources.h"

   // OS2TODO
#define IDM_VIEWERBAR  40990

nsNativeBrowserWindow::nsNativeBrowserWindow() {}
nsNativeBrowserWindow::~nsNativeBrowserWindow() {}

nsresult
nsNativeBrowserWindow::InitNativeWindow()
{
	// override to do something special with platform native windows
  return NS_OK;
}

nsresult nsNativeBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
   HWND hwnd = (HWND)mWindow->GetNativeData(NS_NATIVE_WIDGET);
   HWND hwndFrame = WinQueryWindow( hwnd, QW_PARENT);
   HWND hwndMenu = WinLoadMenu( hwndFrame, 0, IDM_VIEWERBAR);
   WinSendMsg( hwndFrame, WM_UPDATEFRAME, MPFROMLONG(FCF_MENU), 0);
   return NS_OK; 
}

nsresult
nsNativeBrowserWindow::GetMenuBarHeight(PRInt32 * aHeightOut)
{
  NS_ASSERTION(nsnull != aHeightOut,"null out param.");

  *aHeightOut = 0;

  return NS_OK;
}

nsEventStatus nsNativeBrowserWindow::DispatchMenuItem(PRInt32 aID)
{
   return nsBrowserWindow::DispatchMenuItem(aID);
}

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

int main(int argc, char **argv)
{
   nsViewerApp* app = new nsNativeViewerApp;
   NS_ADDREF(app);
   app->Initialize(argc, argv);
   app->Run();
   NS_RELEASE(app);

   return 0;
}
