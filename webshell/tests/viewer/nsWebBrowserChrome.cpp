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

// Local Includes
#include "nsWebBrowserChrome.h"
#include "nsBrowserWindow.h"
#include "nsWebCrawler.h"
#include "nsViewerApp.h"

// Helper Classes
#include "nsIGenericFactory.h"
#include "nsString.h"


// Interfaces needed to be included
#include "nsIDocShellTreeItem.h"

// CIDs

//*****************************************************************************
//***    nsWebBrowserChrome: Object Management
//*****************************************************************************

nsWebBrowserChrome::nsWebBrowserChrome() : mBrowserWindow(nsnull)
{
	NS_INIT_REFCNT();
}

nsWebBrowserChrome::~nsWebBrowserChrome()
{
}

//*****************************************************************************
// nsWebBrowserChrome::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsWebBrowserChrome)
NS_IMPL_RELEASE(nsWebBrowserChrome)

NS_INTERFACE_MAP_BEGIN(nsWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsWebBrowserChrome::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowserChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   return QueryInterface(aIID, aInstancePtr);
}

//*****************************************************************************
// nsWebBrowserChrome::nsIWebBrowserChrome
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowserChrome::SetJSStatus(const PRUnichar* aStatus)
{
   NS_ENSURE_STATE(mBrowserWindow->mStatus);

   PRUint32 size;
   mBrowserWindow->mStatus->SetText(aStatus, size);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::SetJSDefaultStatus(const PRUnichar* aStatus)
{
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::SetOverLink(const PRUnichar* aLink)
{
   NS_ENSURE_STATE(mBrowserWindow->mStatus);

   PRUint32 size;
   mBrowserWindow->mStatus->SetText(aLink, size);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::SetChromeMask(PRUint32 aChromeMask)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::GetChromeMask(PRUint32* aChromeMask)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::GetNewBrowser(PRUint32 aChromeMask, 
   nsIWebBrowser** aWebBrowser)
{
   if(mBrowserWindow->mWebCrawler && (mBrowserWindow->mWebCrawler->Crawling() || 
      mBrowserWindow->mWebCrawler->LoadingURLList()))
      {
      // Do not fly javascript popups when we are crawling
      *aWebBrowser = nsnull;
      return NS_ERROR_NOT_IMPLEMENTED;
      }

   nsBrowserWindow* browser = nsnull;
   mBrowserWindow->mApp->OpenWindow(aChromeMask, browser);

   NS_ENSURE_TRUE(browser, NS_ERROR_FAILURE);

   *aWebBrowser = browser->mWebBrowser;
   NS_IF_ADDREF(*aWebBrowser);
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::FindNamedBrowser(const PRUnichar* aName,
   nsIWebBrowser** aWebBrowser)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::ShowAsModal()
{
   return ShowModal();
}

//*****************************************************************************
// nsWebBrowserChrome::nsIDocShellTreeOwner
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowserChrome::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::SizeShellTo(nsIDocShellTreeItem* aShell,
   PRInt32 aCX, PRInt32 aCY)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::ShowModal()
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::GetNewWindow(PRInt32 aChromeFlags, 
   nsIDocShellTreeItem** aDocShellTreeItem)
{
   if(mBrowserWindow->mWebCrawler && (mBrowserWindow->mWebCrawler->Crawling() || 
      mBrowserWindow->mWebCrawler->LoadingURLList()))
      {
      // Do not fly javascript popups when we are crawling
      *aDocShellTreeItem = nsnull;
      return NS_ERROR_NOT_IMPLEMENTED;
      }

   nsBrowserWindow* browser = nsnull;
   mBrowserWindow->mApp->OpenWindow(nsIWebBrowserChrome::allChrome, browser);

   NS_ENSURE_TRUE(browser, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDocShellTreeItem> newDocShellAsItem(do_QueryInterface(browser->mDocShell));
   *aDocShellTreeItem = newDocShellAsItem;
   NS_IF_ADDREF(*aDocShellTreeItem);
   return NS_OK;
}

//*****************************************************************************
// nsWebBrowserChrome::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowserChrome::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   // Ignore wigdet parents for now.  Don't think those are a vaild thing to call.
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, PR_FALSE), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::Create()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsWebBrowserChrome::Destroy()
{
   return mBrowserWindow->Destroy();
}

NS_IMETHODIMP nsWebBrowserChrome::SetPosition(PRInt32 aX, PRInt32 aY)
{
   PRInt32 cx=0;
   PRInt32 cy=0;

   NS_ENSURE_SUCCESS(GetSize(&cx, &cy), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(SetPositionAndSize(aX, aY, cx, cy, PR_FALSE), 
      NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::GetPosition(PRInt32* aX, PRInt32* aY)
{
   return GetPositionAndSize(aX, aY, nsnull, nsnull);
}

NS_IMETHODIMP nsWebBrowserChrome::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   PRInt32 x=0;
   PRInt32 y=0;

   NS_ENSURE_SUCCESS(GetPosition(&x, &y), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, aCX, aCY, aRepaint), 
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::GetSize(PRInt32* aCX, PRInt32* aCY)
{
   return GetPositionAndSize(nsnull, nsnull, aCX, aCY);
}

NS_IMETHODIMP nsWebBrowserChrome::SetPositionAndSize(PRInt32 aX, PRInt32 aY, 
   PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   return mBrowserWindow->SetPositionAndSize(aX, aY, aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsWebBrowserChrome::GetPositionAndSize(PRInt32* aX, PRInt32* aY, 
   PRInt32* aCX, PRInt32* aCY)
{
   return mBrowserWindow->GetPositionAndSize(aX, aY, aCX, aCY);
}

NS_IMETHODIMP nsWebBrowserChrome::Repaint(PRBool aForce)
{
   return mBrowserWindow->Repaint(aForce);
}

NS_IMETHODIMP nsWebBrowserChrome::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWebBrowserChrome::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWebBrowserChrome::GetVisibility(PRBool* aVisibility)
{
   return mBrowserWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsWebBrowserChrome::SetVisibility(PRBool aVisibility)
{
   return mBrowserWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsWebBrowserChrome::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::SetFocus()
{  
   return mBrowserWindow->SetFocus();
}

NS_IMETHODIMP nsWebBrowserChrome::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   return mBrowserWindow->FocusAvailable(aCurrentFocus, aTookFocus);
}

NS_IMETHODIMP nsWebBrowserChrome::GetTitle(PRUnichar** aTitle)
{
   return mBrowserWindow->GetTitle(aTitle);
}

NS_IMETHODIMP nsWebBrowserChrome::SetTitle(const PRUnichar* aTitle)
{
   mBrowserWindow->mTitle = aTitle;

   nsAutoString newTitle(aTitle);

   newTitle.Append(" - Raptor");
   
   mBrowserWindow->SetTitle(newTitle.GetUnicode());
   return NS_OK;
}

//*****************************************************************************
// nsWebBrowserChrome: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsWebBrowserChrome: Accessors
//*****************************************************************************   

void nsWebBrowserChrome::BrowserWindow(nsBrowserWindow* aBrowserWindow)
{
   mBrowserWindow = aBrowserWindow;
}

nsBrowserWindow* nsWebBrowserChrome::BrowserWindow()
{
   return mBrowserWindow;
}

