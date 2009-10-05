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

#include "nsChildView.h"

nsChildView::nsChildView() : nsWindow()
{
}


nsresult nsChildView::Create(nsIWidget *aParent,
                             nsNativeWidget aNativeParent,
                             const nsRect &aRect,
                             EVENT_CALLBACK aHandleEventFunction,
                             nsIDeviceContext *aContext,
                             nsIAppShell *aAppShell,
                             nsIToolkit *aToolkit,
                             nsWidgetInitData *aInitData)
{

	NS_ASSERTION(aInitData->mWindowType == eWindowType_child
		|| aInitData->mWindowType == eWindowType_plugin, 
		"The windowtype is not handled by this class." );

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
	mWindowParent = (nsWindow *) aParent;
	SetBounds(aRect);

 	//NS_NATIVE_GRAPHIC maybe?
 	//Parent may be a BView if we embed.
	BView *parent= (BView *)
		(aParent ? aParent->GetNativeData(NS_NATIVE_WIDGET) :  aNativeParent);
	//There seems to be three of these on startup,
	//but I believe that these are because of bugs in 
	//other code as they existed before rewriting this
	//function.
	NS_PRECONDITION(parent, "Childviews without parents don't get added to anything.");
	// A childview that is never added to a parent is very strange.
	if (!parent)
		return NS_ERROR_FAILURE;
				
	mView = new nsViewBeOS(this, 
		BRect(aRect.x, aRect.y, aRect.x + aRect.width - 1, aRect.y + aRect.height - 1)
		, "Child view", 0, B_WILL_DRAW);
#if defined(BeIME)
	mView->SetFlags(mView->Flags() | B_INPUT_METHOD_AWARE);
#endif	
	bool mustUnlock = parent->Parent() && parent->LockLooper();
 	parent->AddChild(mView);
	if (mustUnlock)
		parent->UnlockLooper();
	DispatchStandardEvent(NS_CREATE);
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_METHOD nsChildView::Show(PRBool bState)
{
	if (!mEnabled)
		return NS_OK;

	if (!mView || !mView->LockLooper())
		return NS_OK;

	//We need to do the IsHidden() checks
	//because BeOS counts no of Hide()
	//and Show() checks. BeBook:
	// For a hidden view to become visible again, the number of Hide() calls 
	// must be matched by an equal number of Show() calls.
	if (PR_FALSE == bState)
	{
		if (!mView->IsHidden())
			mView->Hide();
	}
	else
	{
		if (mView->IsHidden())
			mView->Show();              
	}
	
	mView->UnlockLooper();
	mIsVisible = bState;	
	return NS_OK;
}


NS_METHOD nsChildView::SetTitle(const nsAString& aTitle)
{
	if (mView)
		mView->SetName(NS_ConvertUTF16toUTF8(aTitle).get());
	return NS_OK;
}

