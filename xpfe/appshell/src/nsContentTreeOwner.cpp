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
#include "nsContentTreeOwner.h"
#include "nsXULWindow.h"

// Helper Classes
#include "nsIGenericFactory.h"


// Interfaces needed to be included
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIWindowMediator.h"

// CIDs
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

//*****************************************************************************
//***    nsContentTreeOwner: Object Management
//*****************************************************************************

nsContentTreeOwner::nsContentTreeOwner() : mXULWindow(nsnull)
{
	NS_INIT_REFCNT();
}

nsContentTreeOwner::~nsContentTreeOwner()
{
}

//*****************************************************************************
// nsContentTreeOwner::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsContentTreeOwner)
NS_IMPL_RELEASE(nsContentTreeOwner)

NS_INTERFACE_MAP_BEGIN(nsContentTreeOwner)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocShellTreeOwner)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeOwner)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsContentTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************   

NS_IMETHODIMP nsContentTreeOwner::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem** aFoundItem)
{
   NS_ENSURE_ARG_POINTER(aFoundItem);

   *aFoundItem = nsnull;

   nsAutoString   name(aName);

   /* Special Cases */
   if(name.Length() == 0)
      return NS_OK;
   if(name.EqualsIgnoreCase("_blank"))
      return NS_OK;
   if(name.EqualsIgnoreCase("_content"))
      return mXULWindow->GetPrimaryContentShell(aFoundItem);

   nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
   NS_ENSURE_TRUE(windowMediator, NS_ERROR_FAILURE);

   nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
   NS_ENSURE_SUCCESS(windowMediator->GetXULWindowEnumerator(nsnull, 
      getter_AddRefs(windowEnumerator)), NS_ERROR_FAILURE);
   
   PRBool more;
   
   windowEnumerator->HasMoreElements(&more);
   while(more)
      {
      nsCOMPtr<nsISupports> nextWindow = nsnull;
      windowEnumerator->GetNext(getter_AddRefs(nextWindow));
      nsCOMPtr<nsIXULWindow> xulWindow(do_QueryInterface(nextWindow));
      NS_ENSURE_TRUE(xulWindow, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDocShellTreeItem> shellAsTreeItem;
      xulWindow->GetPrimaryContentShell(getter_AddRefs(shellAsTreeItem));
      if(shellAsTreeItem)
         {
         if(aRequestor == shellAsTreeItem.get())
            continue;
         // Do this so we can pass in the tree owner as the requestor so the child knows not
         // to call back up.
         nsCOMPtr<nsIDocShellTreeOwner> shellOwner;
         shellAsTreeItem->GetTreeOwner(getter_AddRefs(shellOwner));
         nsCOMPtr<nsISupports> shellOwnerSupports(do_QueryInterface(shellOwner));

         shellAsTreeItem->FindItemWithName(aName, shellOwnerSupports, aFoundItem);
         if(*aFoundItem)
            return NS_OK;   
         }
      windowEnumerator->HasMoreElements(&more);
      }
   return NS_OK;      
}

NS_IMETHODIMP nsContentTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
   mXULWindow->ContentShellAdded(aContentShell, aPrimary, aID);
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::GetNewBrowserChrome(PRInt32 aChromeFlags,
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
// nsContentTreeOwner::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsContentTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   // Ignore wigdet parents for now.  Don't think those are a vaild thing to call.
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, PR_FALSE), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::Create()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsContentTreeOwner::Destroy()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsContentTreeOwner::SetPosition(PRInt32 x, PRInt32 y)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_ARG_POINTER(x && y);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(cx && cy);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx,
   PRInt32* cy)
{
   
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::Repaint(PRBool aForce)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsContentTreeOwner::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsContentTreeOwner::GetVisibility(PRBool* aVisibility)
{
  NS_ENSURE_ARG_POINTER(aVisibility);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::SetVisibility(PRBool aVisibility)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::SetFocus()
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::SetTitle(const PRUnichar* aTitle)
{
   // Get the window title modifiers
   nsCOMPtr<nsIDOMElement> docShellElement;
   mXULWindow->GetDOMElementFromDocShell(mXULWindow->mDocShell, 
      getter_AddRefs(docShellElement));

   nsAutoString   windowTitleModifier;
   nsAutoString   titleSeparator;
   nsAutoString   titlePreface;

   if(docShellElement)
      {
      docShellElement->GetAttribute("titlemodifier", windowTitleModifier);
      docShellElement->GetAttribute("titlemenuseparator", titleSeparator);
      docShellElement->GetAttribute("titlepreface", titlePreface);
      }
   else
      {
      NS_ERROR("This condition should never happen.  If it does, "
         "we just won't get a modifier, but it still shouldn't happen.");
      }

   nsAutoString   title;
   nsAutoString   docTitle(aTitle);

   if(docTitle.Length() > 0)
      {
      if(titlePreface.Length() > 0)
         {
         // Title will be: "Preface: Doc Title - Mozilla"
         title = titlePreface + docTitle;
         }
      else 
         {
         // Title will be: "Doc Title - Mozilla"
         title = docTitle;
         }
      title += titleSeparator + windowTitleModifier;
      }
   else 
      { 
      // Title will just be plain: Mozilla
      title = windowTitleModifier;
      }

   // XXX Don't need to fully qualify this once I remove nsWebShellWindow::SetTitle
   // return mXULWindow->SetTitle(title.GetUnicode());
   return mXULWindow->nsXULWindow::SetTitle(title.GetUnicode());
}

//*****************************************************************************
// nsContentTreeOwner: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsContentTreeOwner: Accessors
//*****************************************************************************   

void nsContentTreeOwner::XULWindow(nsXULWindow* aXULWindow)
{
   mXULWindow = aXULWindow;
}

nsXULWindow* nsContentTreeOwner::XULWindow()
{
   return mXULWindow;
}
