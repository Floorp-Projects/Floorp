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

// Local includes
#include "nsXULWindow.h"

// Helper classes
#include "nsString.h"
#include "prprf.h"

//Interfaces needed to be included
#include "nsIServiceManager.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIWindowMediator.h"

// XXX Get rid of this
#pragma message("WARNING: XXX bad include, remove it.")
#include "nsIWebShellWindow.h"

// CIDs
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

//*****************************************************************************
//***    nsXULWindow: Object Management
//*****************************************************************************

nsXULWindow::nsXULWindow() : mContentTreeOwner(nsnull), 
   mChromeTreeOwner(nsnull)
{
	NS_INIT_REFCNT();
}

nsXULWindow::~nsXULWindow()
{
   Destroy();
}

//*****************************************************************************
// nsXULWindow::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsXULWindow)
NS_IMPL_RELEASE(nsXULWindow)

NS_INTERFACE_MAP_BEGIN(nsXULWindow)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULWindow)
   NS_INTERFACE_MAP_ENTRY(nsIXULWindow)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsXULWindow::nsIXULWindow
//*****************************************************************************   

NS_IMETHODIMP nsXULWindow::GetDocShell(nsIDocShell** aDocShell)
{
   NS_ENSURE_ARG_POINTER(aDocShell);

   *aDocShell = mDocShell;
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetPrimaryContentShell(nsIDocShellTreeItem** 
   aDocShellTreeItem)
{
   NS_ENSURE_ARG_POINTER(aDocShellTreeItem);

   //XXX First Check
   NS_ERROR("Not Yet Implemented");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULWindow::GetContentShellById(const PRUnichar* aID, 
   nsIDocShellTreeItem** aDocShellTreeItem)
{
   //XXX First Check
   NS_ERROR("Not Yet Implemented");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULWindow::SetPersistence(PRBool aPersistX, PRBool aPersistY,
   PRBool aPersistCX, PRBool aPersistCY)

{
   nsCOMPtr<nsIDOMElement> docShellElement;
   GetDOMElementFromDocShell(mDocShell, getter_AddRefs(docShellElement));
   if(!docShellElement)
      return NS_ERROR_FAILURE;

   nsAutoString persistString;
   docShellElement->GetAttribute("persist", persistString);

   PRBool saveString = PR_FALSE;
   PRInt32 index;

   // Set X
   index = persistString.Find("screenX");
   if(!aPersistX && (index >= 0))
      {
      persistString.Cut(index, 7);
      saveString = PR_TRUE;
      }
   else if(aPersistX && (index < 0 ))
      {
      persistString.Append(" screenX");
      saveString = PR_TRUE;
      }
   // Set Y
   index = persistString.Find("screenY");
   if(!aPersistY && (index >= 0))
      {
      persistString.Cut(index, 7);
      saveString = PR_TRUE;
      }
   else if(aPersistY && (index < 0 ))
      {
      persistString.Append(" screenY");
      saveString = PR_TRUE;
      }
   // Set CX
   index = persistString.Find("width");
   if(!aPersistCX && (index >= 0))
      {
      persistString.Cut(index, 5);
      saveString = PR_TRUE;
      }
   else if(aPersistCX && (index < 0 ))
      {
      persistString.Append(" width");
      saveString = PR_TRUE;
      }
   // Set CY
   index = persistString.Find("height");
   if(!aPersistCY && (index >= 0))
      {
      persistString.Cut(index, 6);
      saveString = PR_TRUE;
      }
   else if(aPersistCY && (index < 0 ))
      {
      persistString.Append(" height");
      saveString = PR_TRUE;
      }

   if(saveString) 
      docShellElement->SetAttribute("persist", persistString);

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetPersistence(PRBool* aPersistX, PRBool* aPersistY,
   PRBool* aPersistCX, PRBool* aPersistCY)
{
   nsCOMPtr<nsIDOMElement> docShellElement;
   GetDOMElementFromDocShell(mDocShell, getter_AddRefs(docShellElement));
   if(!docShellElement) 
      return NS_ERROR_FAILURE;

   nsAutoString persistString;
   docShellElement->GetAttribute("persist", persistString);

   if(aPersistX)
      *aPersistX = persistString.Find("screenX") >= 0 ? PR_TRUE : PR_FALSE;
   if(aPersistY)
      *aPersistY = persistString.Find("screenY") >= 0 ? PR_TRUE : PR_FALSE;
   if(aPersistCX)
      *aPersistCX = persistString.Find("width") >= 0 ? PR_TRUE : PR_FALSE;
   if(aPersistCY)
      *aPersistCY = persistString.Find("height") >= 0 ? PR_TRUE : PR_FALSE;

   return NS_OK;
}

//*****************************************************************************
// nsXULWindow::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsXULWindow::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::Create()
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::Destroy()
{
   if(mDocShell)
      {
      nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(mDocShell));
      shellAsWin->Destroy();
      mDocShell = nsnull;
      }
   if(mContentTreeOwner)   
      {
      mContentTreeOwner->XULWindow(nsnull);
      NS_RELEASE(mContentTreeOwner);
      }
   if(mChromeTreeOwner)
      {
      mChromeTreeOwner->XULWindow(nsnull);
      NS_RELEASE(mChromeTreeOwner);
      }
   mWindow = nsnull;

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetPosition(PRInt32 aX, PRInt32 aY)
{
   mWindow->Move(aX, aY);
   PersistPositionAndSize(PR_TRUE, PR_FALSE);
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_ARG_POINTER(x && y);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetSize(PRInt32 cx, PRInt32 cy, PRBool fRepaint)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_ARG_POINTER(cx && cy);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx,
   PRInt32* cy)
{
   nsRect      rect;

   mWindow->GetScreenBounds(rect);

   if(x)
      *x = rect.x;
   if(y)
      *y = rect.y;
   if(cx)
      *cx = rect.width;
   if(cy)
      *cy = rect.height;

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::Repaint(PRBool aForce)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetParentWidget(nsIWidget* aParentWidget)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetVisibility(PRBool* aVisibility)
{
  NS_ENSURE_ARG_POINTER(aVisibility);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetVisibility(PRBool aVisibility)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetFocus()
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetTitle(const PRUnichar* aTitle)
{
   NS_ENSURE_STATE(mWindow);

   nsAutoString title(aTitle);

   NS_ENSURE_SUCCESS(mWindow->SetTitle(title), NS_ERROR_FAILURE);

   // Tell the window mediator that a title has changed
   nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
   if(!windowMediator)
      return NS_OK;

   // XXX Update windowMediator to take nsIXULWindow
   nsCOMPtr<nsIWebShellWindow> 
      webShellWindow(do_QueryInterface(NS_STATIC_CAST(nsIXULWindow*, this)));
   NS_ENSURE_TRUE(webShellWindow, NS_ERROR_FAILURE);
   windowMediator->UpdateWindowTitle(webShellWindow, aTitle);

   return NS_OK;
}

//*****************************************************************************
// nsXULWindow: Helpers
//*****************************************************************************   

NS_IMETHODIMP nsXULWindow::EnsureChromeTreeOwner()
{
   if(mChromeTreeOwner)
      return NS_OK;

   mChromeTreeOwner = new nsChromeTreeOwner();
   NS_ENSURE_TRUE(mChromeTreeOwner, NS_ERROR_OUT_OF_MEMORY);

   NS_ADDREF(mChromeTreeOwner);
   mChromeTreeOwner->XULWindow(this);

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::EnsureContentTreeOwner()
{
   if(mContentTreeOwner)
      return NS_OK;

   mContentTreeOwner = new nsContentTreeOwner();
   NS_ENSURE_TRUE(mContentTreeOwner, NS_ERROR_FAILURE);

   NS_ADDREF(mContentTreeOwner);
   mContentTreeOwner->XULWindow(this);
   
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetDOMElementFromDocShell(nsIDocShell* aDocShell,
   nsIDOMElement** aDOMElement)
{
   NS_ENSURE_ARG(aDocShell);
   NS_ENSURE_ARG_POINTER(aDOMElement);

   *aDOMElement = nsnull;

   nsCOMPtr<nsIContentViewer> cv;
   
   aDocShell->GetContentViewer(getter_AddRefs(cv));
   if(!cv)
      return NS_ERROR_FAILURE;

   nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
   if(!docv)   
      return NS_ERROR_FAILURE;

   nsCOMPtr<nsIDocument> doc;
   docv->GetDocument(*getter_AddRefs(doc));
   nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(doc));
   if(!domdoc) 
      return NS_ERROR_FAILURE;

   domdoc->GetDocumentElement(aDOMElement);
   if(!*aDOMElement)
      return NS_ERROR_FAILURE;

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::PersistPositionAndSize(PRBool aPosition, PRBool aSize)
{
   nsCOMPtr<nsIDOMElement> docShellElement;
   GetDOMElementFromDocShell(mDocShell, getter_AddRefs(docShellElement));
   if(!docShellElement)
      return NS_ERROR_FAILURE;

   PRInt32 x, y, cx, cy;
   NS_ENSURE_SUCCESS(GetPositionAndSize(&x, &y, &cx, &cy), NS_ERROR_FAILURE);

   // (But only for size elements which are persisted.)
   /* Note we use the same cheesy way to determine that as in
     nsXULDocument.cpp. Some day that'll be fixed and there will
     be an obscure bug here. */
   /* Note that storing sizes which are not persisted makes it
     difficult to distinguish between windows intrinsically sized
     and not. */
   nsAutoString   persistString;
   docShellElement->GetAttribute("persist", persistString);

   char           sizeBuf[10];
   nsAutoString   sizeString;
   if(aPosition)
      {
      if(persistString.Find("screenX") >= 0)
         {
         PR_snprintf(sizeBuf, sizeof(sizeBuf), "%ld", (long)x);
         sizeString = sizeBuf;
         docShellElement->SetAttribute("screenX", sizeString);
         }
      if(persistString.Find("screenY") >= 0)
         {
         PR_snprintf(sizeBuf, sizeof(sizeBuf), "%ld", (long)y);
         sizeString = sizeBuf;
         docShellElement->SetAttribute("screenY", sizeString);
         }
      }

   if(aSize)
      {
      if(persistString.Find("width") >= 0)
         {
         PR_snprintf(sizeBuf, sizeof(sizeBuf), "%ld", (long)cx);
         sizeString = sizeBuf;
         docShellElement->SetAttribute("width", sizeString);
         }
      if(persistString.Find("height") >= 0)
         {
         PR_snprintf(sizeBuf, sizeof(sizeBuf), "%ld", (long)cy);
         sizeString = sizeBuf;
         docShellElement->SetAttribute("height", sizeString);
         }
      }

   return NS_OK;
}

//*****************************************************************************
// nsXULWindow: Accessors
//*****************************************************************************   

