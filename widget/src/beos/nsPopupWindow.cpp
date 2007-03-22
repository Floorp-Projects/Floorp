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
 * The Original Code is the Mozilla Browser.
 *
 * The Initial Developer of the Original Code is
 * Fredrik Holmqvist <thesuckiestemail@yahoo.se>.
 * Portions created by the Initial Developer are Copyright (C) 2006
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#include "nsPopupWindow.h"

nsPopupWindow::nsPopupWindow() : nsWindow()
{
}


//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------
nsresult nsPopupWindow::StandardWindowCreate(nsIWidget *aParent,
                                        const nsRect &aRect,
                                        EVENT_CALLBACK aHandleEventFunction,
                                        nsIDeviceContext *aContext,
                                        nsIAppShell *aAppShell,
                                        nsIToolkit *aToolkit,
                                        nsWidgetInitData *aInitData,
                                        nsNativeWidget aNativeParent)
{
	NS_ASSERTION(aInitData->mWindowType == eWindowType_popup,
		"The windowtype is not handled by this class.");

	NS_ASSERTION(!aParent, "Popups should not be hooked into nsIWidget hierarchy");
	
	mIsTopWidgetWindow = PR_FALSE;
	
	BaseCreate(aParent, aRect, aHandleEventFunction, aContext, aAppShell,
		aToolkit, aInitData);

	mListenForResizes = aNativeParent ? PR_TRUE : aInitData->mListenForResizes;
		
	// Switch to the "main gui thread" if necessary... This method must
	// be executed on the "gui thread"...
	//
	nsToolkit* toolkit = (nsToolkit *)mToolkit;
	if (toolkit && !toolkit->IsGuiThread())
	{
		uint32 args[7];
		args[0] = (uint32)aParent;
		args[1] = (uint32)&aRect;
		args[2] = (uint32)aHandleEventFunction;
		args[3] = (uint32)aContext;
		args[4] = (uint32)aAppShell;
		args[5] = (uint32)aToolkit;
		args[6] = (uint32)aInitData;

		if (nsnull != aParent)
		{
			// nsIWidget parent dispatch
			MethodInfo info(this, this, nsSwitchToUIThread::CREATE, 7, args);
			toolkit->CallMethod(&info);
		}
		else
		{
			// Native parent dispatch
			MethodInfo info(this, this, nsSwitchToUIThread::CREATE_NATIVE, 7, args);
			toolkit->CallMethod(&info);
		}
		return NS_OK;
	}

	mParent = aParent;
	// Useful shortcut, wondering if we can use it also in GetParent() instead 
	// nsIWidget* type mParent.
	mWindowParent = (nsWindow *)aParent;
	SetBounds(aRect);

	// Default mode for window, everything switched off.
	// B_AVOID_FOCUS | B_NO_WORKSPACE_ACTIVATION : 
	// popups always avoid focus and don't force the user to another workspace.
	uint32 flags = B_NOT_RESIZABLE | B_NOT_MINIMIZABLE | B_NOT_ZOOMABLE
		| B_NOT_CLOSABLE | B_ASYNCHRONOUS_CONTROLS | B_AVOID_FOCUS
		| B_NO_WORKSPACE_ACTIVATION;
	window_look look = B_NO_BORDER_WINDOW_LOOK;

	//eBorderStyle_default is to ask the OS to handle it as it sees best.
	//eBorderStyle_all is same as top_level window default.
	if ( !(eBorderStyle_default == mBorderStyle || eBorderStyle_all & mBorderStyle))
	{
		if (eBorderStyle_border & mBorderStyle)
			look = B_MODAL_WINDOW_LOOK;

		if (eBorderStyle_resizeh & mBorderStyle)
		{
			//Resize demands at least border
			look = B_MODAL_WINDOW_LOOK;
			flags &= !B_NOT_RESIZABLE;
		}

		//We don't have titlebar menus, so treat like title as it demands titlebar.
		if (eBorderStyle_title & mBorderStyle || eBorderStyle_menu & mBorderStyle)
			look = B_TITLED_WINDOW_LOOK;

		if (eBorderStyle_minimize & mBorderStyle)
			flags &= !B_NOT_MINIMIZABLE;

		if (eBorderStyle_maximize & mBorderStyle)
			flags &= !B_NOT_ZOOMABLE;

		if (eBorderStyle_close & mBorderStyle)
			flags &= !B_NOT_CLOSABLE;
	}
	
	if (aNativeParent)
	{
		// Due poor BeOS capability to control windows hierarchy/z-order we use this
		// workaround to show eWindowType_popup (e.g. drop-downs) over floating
		// (subset) parent window.
		if (((BView *)aNativeParent)->Window() &&
			((BView *)aNativeParent)->Window()->IsFloating())
		{
			mBWindowFeel = B_FLOATING_ALL_WINDOW_FEEL;
		}
	}


	nsWindowBeOS * w = new nsWindowBeOS(this, 
		BRect(aRect.x, aRect.y, aRect.x + aRect.width - 1, aRect.y + aRect.height - 1),
		"", look, mBWindowFeel, flags);
	if (!w)
		return NS_ERROR_OUT_OF_MEMORY;

	mView = new nsViewBeOS(this, w->Bounds(), "popup view",
		B_FOLLOW_ALL, B_WILL_DRAW);

	if (!mView)
		return NS_ERROR_OUT_OF_MEMORY;

	w->AddChild(mView);
	// Run Looper. No proper destroy without it.
	w->Run();	
	DispatchStandardEvent(NS_CREATE);
	return NS_OK;
}

NS_METHOD nsPopupWindow::Show(PRBool bState)
{
	//Bring popup to correct workspace.
	if (bState == PR_TRUE && mView && mView->Window() != NULL )
		mView->Window()->SetWorkspaces(B_CURRENT_WORKSPACE);
		
	return nsWindow::Show(bState);
}


