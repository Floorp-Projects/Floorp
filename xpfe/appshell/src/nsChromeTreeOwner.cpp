/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#include "nsChromeTreeOwner.h"
#include "nsXULWindow.h"

//*****************************************************************************
//***    nsChromeTreeOwner: Object Management
//*****************************************************************************

nsChromeTreeOwner::nsChromeTreeOwner() : mXULWindow(nsnull)
{
	NS_INIT_REFCNT();
}

nsChromeTreeOwner::~nsChromeTreeOwner()
{
}

//*****************************************************************************
// nsChromeTreeOwner::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsChromeTreeOwner)
NS_IMPL_RELEASE(nsChromeTreeOwner)

NS_INTERFACE_MAP_BEGIN(nsChromeTreeOwner)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShellTreeOwner)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsChromeTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************   

NS_IMETHODIMP nsChromeTreeOwner::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{

	/*
	Return the child DocShellTreeItem with the specified name.
	name - This is the name of the item that is trying to be found.
	aRequestor - This is the docshellTreeItem that is requesting the find.  This
		parameter is used to identify when the child is asking it's parent to find
		a child with the specific name.  The parent uses this parameter to ensure
		a resursive state does not occur by not again asking the requestor for find
		a shell by the specified name.  Inversely the child uses it to ensure it
		does not ask it's parent to do the search if it's parent is the one that
		asked it to search.
	*/

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
	/*
	Called when a content shell is added to the the docShell Tree.
	aContentShell - the docShell that has been added.
	aPrimary - true if this is the primary content shell
	aID - the ID of the docShell that has been added.
	*/

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::GetNewBrowserChrome(PRInt32 aChromeFlags,
   nsIWebBrowserChrome** aWebBrowserChrome)
{
	/*
		Tells the implementer of this interface to create a new webBrowserChrome
		object for it.  Typically this means the implemetor will create a new 
		top level window that is represented by nsIWebBrowserChrome.  This
		most often will be called when for instance there is a need for a new
		JS window, etc.  Soon after this new object is returned, the webBrowser
		attribute will checked, if one does not exist, one will be created and
		setWebBrowser will be called with the new widget to instantiate in this 
		new window.	
	*/

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

//*****************************************************************************
// nsChromeTreeOwner::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsChromeTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::Create()
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::Destroy()
{
   // We don't support the dynamic destroy and recreate on the object.  Just
   // create a new object!
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsChromeTreeOwner::SetPosition(PRInt32 x, PRInt32 y)
{
   return mXULWindow->SetPosition(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPosition(PRInt32* x, PRInt32* y)
{
   return mXULWindow->GetPosition(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   return mXULWindow->SetSize(cx, cy, fRepaint);
}

NS_IMETHODIMP nsChromeTreeOwner::GetSize(PRInt32* cx, PRInt32* cy)
{
   return mXULWindow->GetSize(cx, cy);
}

NS_IMETHODIMP nsChromeTreeOwner::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   return mXULWindow->SetPositionAndSize(x, y, cx, cy, fRepaint);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx,
   PRInt32* cy)
{
   return mXULWindow->GetPositionAndSize(x, y, cx, cy);
}

NS_IMETHODIMP nsChromeTreeOwner::Repaint(PRBool aForce)
{
   return mXULWindow->Repaint(aForce);
}

NS_IMETHODIMP nsChromeTreeOwner::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::SetParentWidget(nsIWidget* aParentWidget)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsChromeTreeOwner::GetVisibility(PRBool* aVisibility)
{
   return mXULWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsChromeTreeOwner::SetVisibility(PRBool aVisibility)
{
   return mXULWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsChromeTreeOwner::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::SetFocus()
{
   return mXULWindow->SetFocus();
}

NS_IMETHODIMP nsChromeTreeOwner::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   return mXULWindow->FocusAvailable(aCurrentFocus, aTookFocus);
}

NS_IMETHODIMP nsChromeTreeOwner::GetTitle(PRUnichar** aTitle)
{
   return mXULWindow->GetTitle(aTitle);
}

NS_IMETHODIMP nsChromeTreeOwner::SetTitle(const PRUnichar* aTitle)
{
   return mXULWindow->SetTitle(aTitle);
}

//*****************************************************************************
// nsChromeTreeOwner: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsChromeTreeOwner: Accessors
//*****************************************************************************   

void nsChromeTreeOwner::XULWindow(nsXULWindow* aXULWindow)
{
   mXULWindow = aXULWindow;
}

nsXULWindow* nsChromeTreeOwner::XULWindow()
{
   return mXULWindow;
}