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

#include <stdlib.h>

#include "nsViewerApp.h"
#include "nsBrowserWindow.h"
#include "nsIWidget.h"
#include "nsIServiceManager.h"
#include "resources.h"

#include <ToolUtils.h>			// MacOS includes
#include <Menus.h>
#include <Windows.h>
#include <Devices.h>
#include <Resources.h>
#include <Dialogs.h>

#include "nsMacMessagePump.h"	// for the windowless menu event handler
#include "nsILeakDetector.h"
#include "macstdlibextras.h"

typedef		SInt32			MessageT;
typedef   PRUint32    Uint32;
const MessageT	cmd_Undo			= 11;	// nil
const MessageT	cmd_Cut				= 12;	// nil
const MessageT	cmd_Copy			= 13;	// nil
const MessageT	cmd_Paste			= 14;	// nil
const MessageT	cmd_Clear			= 15;	// nil
const MessageT	cmd_SelectAll		= 16;	// nil

const MessageT	cmd_About			= 1;	// nil

					// File Menu
const MessageT	cmd_New				= 2;	// nil
const MessageT	cmd_Open			= 3;	// nil
const MessageT	cmd_Close			= 4;	// nil
const MessageT	cmd_Save			= 5;	// nil
const MessageT	cmd_SaveAs			= 6;	// nil
const MessageT	cmd_Revert			= 7;	// nil
const MessageT	cmd_PageSetup		= 8;	// nil
const MessageT	cmd_Print			= 9;	// nil
const MessageT	cmd_PrintOne		= 17;	// nil
const MessageT	cmd_Quit			= 10;	// nil
const MessageT	cmd_Preferences		= 27;	// nil


enum
{
};


enum
{
	menu_First = 128,
	menu_Apple = menu_First,
	menu_File,
	menu_Edit,
	menu_Sample,
	menu_Debug,
	menu_Tools,
	menu_URLS,
	menu_Last = menu_URLS,

	submenu_Print = 16,
	submenu_CompatibilityMode = 32,
	
	cmd_Sample0					= 1000,
	cmd_FirstXPToolkitSample	= 1100,
	cmd_PrintOneColumn	= 2000,
	cmd_Find						= 3000,

	cmd_ViewSource			= 2200,
	cmd_PrintSetup,

	cmd_DebugMode				= 4000,
	cmd_ReflowTest,
	cmd_DumpContents,
	cmd_DumpFrames,
	cmd_DumpViews,
	cmd_DumpStyleSheets,
	cmd_DumpStyleContexts,
	cmd_ShowContentSize,
	cmd_ShowFrameSize,
	cmd_ShowStyleSize,
	cmd_DebugSave,
	cmd_DebugOutputText,
	cmd_DebugOutputHTML,	
	cmd_DebugToggleSelection,
	cmd_DebugRobot,
	cmd_ShowContentQuality,
	cmd_GFXWidgetMode,
	cmd_NativeWidgetMode,
	cmd_GFXScrollBars,
	cmd_NativeScrollBars,
	cmd_DumpLeaks,

	item_GFXWidgetMode = 24,
	item_NativeWidgetMode,

	cmd_Compatibility_UseDTD	= 4200,
	cmd_Compatibility_NavQuirks,
	cmd_Compatibility_Standard,
	
	cmd_JSConsole				= 5000,
	cmd_EditorMode,
	cmd_Top100,
	cmd_TableInspector,
	cmd_ImageInspector,
	
	cmd_SaveURL1 = 6000,
	cmd_SaveURL2,
	cmd_LoadURL1,
	cmd_LoadURL2

};

static nsNativeViewerApp* gTheApp;


//----------------------------------------------------------------------

nsNativeViewerApp::nsNativeViewerApp()
{
	//nsMacMessagePump::SetWindowlessMenuEventHandler(DispatchMenuItemWithoutWindow);
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
					#if !TARGET_CARBON
					OpenDeskAcc(daName);
					#endif
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
					gTheApp->OpenWindow((PRUint32)0, newWindow);
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
nsNativeBrowserWindow::InitNativeWindow()
{
	// this is where we get a chance to set up the window refCon
	NS_PRECONDITION(nsnull != mWindow, "Null window in InitNativeWindow");
	
	WindowPtr		wind = (WindowPtr)mWindow->GetNativeData(NS_NATIVE_DISPLAY);
	if (!wind) return NS_ERROR_NULL_POINTER;

	::SetWRefCon(wind, (long)this);
	return NS_OK;
}

static void CloseFrontWindow()
{
	WindowPtr		wind = ::FrontWindow();
	if (!wind) return;
	
	nsBrowserWindow		*browserWindow = (nsBrowserWindow *)GetWRefCon(wind);
	if (!browserWindow) return;
	
	browserWindow->Destroy();
}


nsresult
nsNativeBrowserWindow::CreateMenuBar(PRInt32 aWidth)
{
	for (int i = menu_First; i <= menu_Last; i++)
	{
		InsertMenu(GetMenu(i), 0);
	}
	InsertMenu(GetMenu(submenu_Print), -1);
	InsertMenu(GetMenu(submenu_CompatibilityMode), -1);
	AppendResMenu(GetMenuHandle(menu_Apple), 'DRVR');
	DrawMenuBar();
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
	nsEventStatus status = nsEventStatus_eIgnore;
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
					#if !TARGET_CARBON
					OpenDeskAcc(daName);
					#endif
					break;
			}
			break;

		case menu_File:
			switch (menuItem)
			{
				case cmd_New:			xpID = VIEWER_WINDOW_OPEN;		break;
				case cmd_Open:		xpID = VIEWER_FILE_OPEN;			break;
				case cmd_Close:
					CloseFrontWindow();
					status = nsEventStatus_eConsumeNoDefault;
					break;

				case cmd_ViewSource:			xpID = VIEW_SOURCE;				break;
				case cmd_Save:			/*n.a.*/						break;
				case cmd_SaveAs:		/*n.a.*/						break;
				case cmd_Revert:		/*n.a.*/						break;
				case cmd_PageSetup:	/*n.a.*/						break;
				case cmd_Print:					xpID = VIEWER_PRINT;				break;
				case cmd_PrintSetup:		xpID = VIEWER_PRINT_SETUP;	break;
				case cmd_Quit:					xpID = VIEWER_EXIT;					break;
			}
			break;

		case menu_Edit:
			switch (menuItem)
			{
				case cmd_Undo:		/*n.a.*/														break;
				case cmd_Cut:					xpID = VIEWER_EDIT_CUT;					break;
				case cmd_Copy:				xpID = VIEWER_EDIT_COPY;				break;
				case cmd_Paste:				xpID = VIEWER_EDIT_PASTE;				break;
				case cmd_Clear:		/*n.a.*/														break;
				case cmd_SelectAll:		xpID = VIEWER_EDIT_SELECTALL;		break;
				case cmd_Find:				xpID = VIEWER_EDIT_FINDINPAGE;	break;
				case cmd_Preferences:	xpID = VIEWER_PREFS;						break;
			}
			break;

		case menu_Sample:
			if ( menuItem < cmd_FirstXPToolkitSample )
				xpID = VIEWER_DEMO0 + menuItem - cmd_Sample0;
			else
				xpID = VIEWER_XPTOOLKITDEMOBASE + (menuItem - cmd_FirstXPToolkitSample);
			break;

		case menu_Debug:
			switch (menuItem)
			{
				case cmd_DebugMode:					xpID = VIEWER_VISUAL_DEBUGGING;			break;
				case cmd_ReflowTest:				xpID = VIEWER_REFLOW_TEST;					break;

				case cmd_DumpContents:			xpID = VIEWER_DUMP_CONTENT;					break;
				case cmd_DumpFrames:				xpID = VIEWER_DUMP_FRAMES;					break;
				case cmd_DumpViews:					xpID = VIEWER_DUMP_VIEWS;						break;

				case cmd_DumpStyleSheets:		xpID = VIEWER_DUMP_STYLE_SHEETS;		break;
				case cmd_DumpStyleContexts:	xpID = VIEWER_DUMP_STYLE_CONTEXTS;	break;

				case cmd_ShowContentSize:		xpID = VIEWER_SHOW_CONTENT_SIZE;		break;
				case cmd_ShowFrameSize:			xpID = VIEWER_SHOW_FRAME_SIZE;			break;
				case cmd_ShowStyleSize:			xpID = VIEWER_SHOW_STYLE_SIZE;			break;
				
				case cmd_DebugSave:							xpID = VIEWER_DEBUGSAVE;					break;
				case cmd_DebugOutputText:				xpID = VIEWER_DISPLAYTEXT;				break;
				case cmd_DebugOutputHTML:				xpID = VIEWER_DISPLAYHTML;				break;
				case cmd_DebugToggleSelection:	xpID = VIEWER_TOGGLE_SELECTION;			break;
				case cmd_DebugRobot:						xpID = VIEWER_DEBUGROBOT;						break;
				case cmd_ShowContentQuality:		xpID =VIEWER_SHOW_CONTENT_QUALITY;	break;
				case cmd_DumpLeaks:
					{
						nsresult rv;
						nsCOMPtr<nsILeakDetector> leakDetector = 
						         do_GetService("@mozilla.org/xpcom/leakdetector;1", &rv);
						if (NS_SUCCEEDED(rv))
							leakDetector->DumpLeaks();
					}
					break;
				case cmd_GFXScrollBars:		xpID =VIEWER_GFX_SCROLLBARS_ON;	break;
				case cmd_NativeScrollBars: xpID =VIEWER_GFX_SCROLLBARS_OFF; break;
			}
			break;
			
		case menu_Tools:
			switch (menuItem)
			{
				case cmd_JSConsole:					xpID = JS_CONSOLE;								break;
				case cmd_EditorMode:				xpID = EDITOR_MODE;								break;
				case cmd_Top100:						xpID = VIEWER_TOP100;							break;
				case cmd_TableInspector:		xpID = VIEWER_TABLE_INSPECTOR;		break;
				case cmd_ImageInspector:		xpID = VIEWER_IMAGE_INSPECTOR;		break;
			}
			break;
			
    case menu_URLS:
			switch (menuItem)
			{
				case cmd_SaveURL1:					xpID = VIEWER_SAVE_TEST_URL1;								break;
				case cmd_SaveURL2:					xpID = VIEWER_SAVE_TEST_URL2;								break;
				case cmd_LoadURL1:					xpID = VIEWER_GOTO_TEST_URL1;								break;
				case cmd_LoadURL2:					xpID = VIEWER_GOTO_TEST_URL2;								break;  
			}  	
      break;	
			
		case submenu_Print:
			xpID = VIEWER_ONE_COLUMN + menuItem - cmd_PrintOneColumn;
			break;
			
		case submenu_CompatibilityMode:
			switch (menuItem)
			{
				case cmd_Compatibility_UseDTD:		  xpID = VIEWER_USE_DTD_MODE;	break;
				case cmd_Compatibility_NavQuirks:		xpID = VIEWER_NAV_QUIRKS_MODE;	break;
				case cmd_Compatibility_Standard:		xpID = VIEWER_STANDARD_MODE;		break;
			}
			break;
	}

	// Dispatch xp menu items
	if (xpID != 0) {
		// beard: nsBrowserWindow::DispatchMenuItem almost always returns nsEventStatus_eIgnore.
		// this causes double menu item dispatching for most items except for VIEWER_EXIT!
		nsBrowserWindow::DispatchMenuItem(xpID);
		status = nsEventStatus_eConsumeNoDefault;
	}
	return status;
}

/**
 * Quit AppleEvent handler.
 */
static pascal OSErr handleQuitApplication(const AppleEvent*, AppleEvent*, UInt32)
{
	if (gTheApp != nsnull) {
		gTheApp->Exit();
	} else {
		ExitToShell();
	}
	return noErr;
}

#pragma mark -
//----------------------------------------------------------------------
int main(int argc, char **argv)
{
	// Set up the toolbox and (if DEBUG) the console
	InitializeMacToolbox();

	// Install an a Quit AppleEvent handler.
	OSErr err = AEInstallEventHandler(kCoreEventClass, kAEQuitApplication,
									NewAEEventHandlerProc(handleQuitApplication), 0, false);
	NS_ASSERTION((err==noErr), "AEInstallEventHandler failed");

	// Start up XPCOM?
 	nsresult rv = NS_InitXPCOM(nsnull, nsnull);
	NS_ASSERTION(NS_SUCCEEDED(rv), "NS_InitXPCOM failed");

	gTheApp = new nsNativeViewerApp();
	if (gTheApp != nsnull) {
		NS_ADDREF(gTheApp);
		if (gTheApp->Initialize(argc, argv) == NS_OK)
			gTheApp->Run();
		NS_RELEASE(gTheApp);
	}

	// Shutdown XPCOM?
	rv = NS_ShutdownXPCOM(nsnull);
	NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

	return 0;
}
