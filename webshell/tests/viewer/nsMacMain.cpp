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
#include "nsIImageManager.h"
#include "nsIWidget.h"
#include <stdlib.h>
#include "resources.h"

#include <ToolUtils.h>			// MacOS includes
#include <Menus.h>
#include <Windows.h>
#include <Devices.h>
#include <Resources.h>
#include <Dialogs.h>

#include <PP_Messages.h>		// for PP standard menu commands
#include "nsMacMessagePump.h"	// for the windowless menu event handler

enum
{
	menu_First = 128,
	menu_Apple = menu_First,
	menu_File,
	menu_Edit,
	menu_Sample,
	menu_Last = menu_Sample,

	submenu_Print = 16,

	cmd_Sample0			= 1000,
	cmd_PrintOneColumn	= 2000,
	cmd_Find			= 3000
};


static nsNativeViewerApp* gTheApp;


static long ConvertOSMenuResultToPPMenuResult(long menuResult);
static long ConvertOSMenuResultToPPMenuResult(long menuResult)
{
	// Convert MacOS menu item to PowerPlant menu item because
	// in our sample app, we use Constructor for resource editing
	long menuID = HiWord(menuResult);
	long menuItem = LoWord(menuResult);
	Int16**	theMcmdH = (Int16**) ::GetResource('Mcmd', menuID);
	if (theMcmdH != nil)
	{
		if (::GetHandleSize((Handle)theMcmdH) > 0)
		{
			Int16 numCommands = (*theMcmdH)[0];
			if (numCommands >= menuItem)
			{
				CommandT* theCommandNums = (CommandT*)(&(*theMcmdH)[1]);
				menuItem = theCommandNums[menuItem-1];
			}
		}
		::ReleaseResource((Handle) theMcmdH);
	}
	menuResult = (menuID << 16) + menuItem;
	return (menuResult);
}


#pragma mark -
//----------------------------------------------------------------------

nsNativeViewerApp::nsNativeViewerApp()
{
	nsMacMessagePump::SetWindowlessMenuEventHandler(DispatchMenuItemWithoutWindow);
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

void nsNativeViewerApp::DispatchMenuItemWithoutWindow(PRInt32 menuResult)
{
	menuResult = ConvertOSMenuResultToPPMenuResult(menuResult);

	long menuID = HiWord(menuResult);
	long menuItem = LoWord(menuResult);
	switch (menuID)
	{
		case menu_Apple:
			switch (menuItem)
			{
				case cmd_About:
					::Alert(128, nil);
					break;
				default:
					Str255 daName;
					GetMenuItemText(GetMenuHandle(menu_Apple), menuItem, daName);
					OpenDeskAcc(daName);
					break;
			}
			break;

		case menu_File:
			
			switch (menuItem)
			{
				case cmd_New:
					gTheApp->OpenWindow();
					break;
				case cmd_Open:
					nsBrowserWindow * newWindow;
					gTheApp->OpenWindow(0, newWindow);
					newWindow->DoFileOpen();
					break;
				case cmd_Quit:
					gTheApp->Exit();
					break;
			}
			break;
		}
}

#pragma mark -
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
	for (int i = menu_First; i <= menu_Last; i++)
	{
		InsertMenu(GetMenu(i), 0);
	}
	InsertMenu(GetMenu(submenu_Print), -1);
	AppendResMenu(GetMenuHandle(menu_Apple), 'DRVR');
	DrawMenuBar();
	return NS_OK;
}

nsEventStatus
nsNativeBrowserWindow::DispatchMenuItem(PRInt32 aID)
{
	aID = ConvertOSMenuResultToPPMenuResult(aID);

	PRInt32 xpID = 0;
	long menuID = HiWord(aID);
	long menuItem = LoWord(aID);
	
	switch (menuID)
	{
		case menu_Apple:
			switch (menuItem)
			{
				case cmd_About:
					::Alert(128, nil);
					break;
				default:
					Str255 daName;
					GetMenuItemText(GetMenuHandle(menu_Apple), menuItem, daName);
					OpenDeskAcc(daName);
					break;
			}
			break;

		case menu_File:
			switch (menuItem)
			{
				case cmd_New:		xpID = VIEWER_WINDOW_OPEN;		break;
				case cmd_Open:		xpID = VIEWER_FILE_OPEN;		break;
				case cmd_Close:
					  WindowPtr whichwindow = FrontWindow();
				      nsIWidget* raptorWindow = *(nsIWidget**)::GetWRefCon(whichwindow);
				      raptorWindow->Destroy();
					break;
				case 2000:		xpID = VIEW_SOURCE;				break;
				case 2001:		xpID = VIEWER_TREEVIEW;			break;
				case cmd_Save:		/*n.a.*/						break;
				case cmd_SaveAs:	/*n.a.*/						break;
				case cmd_Revert:	/*n.a.*/						break;
				case cmd_PageSetup:	/*n.a.*/						break;
				case cmd_Print:		xpID = VIEWER_PRINT;	break;
				case cmd_Quit:		xpID = VIEWER_EXIT;				break;
			}
			break;

		case menu_Edit:
			switch (menuItem)
			{
				case cmd_Undo:		/*n.a.*/						break;
				case cmd_Cut:		xpID = VIEWER_EDIT_CUT;			break;
				case cmd_Copy:		xpID = VIEWER_EDIT_COPY;		break;
				case cmd_Paste:		xpID = VIEWER_EDIT_PASTE;		break;
				case cmd_Clear:		/*n.a.*/						break;
				case cmd_SelectAll:	xpID = VIEWER_EDIT_SELECTALL;	break;
				case cmd_Find:		xpID = VIEWER_EDIT_FINDINPAGE;	break;
			}
			break;

		case menu_Sample:
			xpID = VIEWER_DEMO0 + menuItem - cmd_Sample0;
			break;

		case submenu_Print:
			xpID = VIEWER_ONE_COLUMN + menuItem - cmd_PrintOneColumn;
			break;
	}

	// Dispatch xp menu items
	if (xpID != 0)
		return nsBrowserWindow::DispatchMenuItem(xpID);
	else
		return nsEventStatus_eIgnore;
}

#pragma mark -
//----------------------------------------------------------------------
int main(int argc, char **argv)
{

  // Hack to get il_ss set so it doesn't fail in xpcompat.c
  nsIImageManager *manager;
  NS_NewImageManager(&manager);

  gTheApp = new nsNativeViewerApp();
  NS_ADDREF(gTheApp);
  gTheApp->Initialize(argc, argv);
  gTheApp->Run();

  return 0;
}
