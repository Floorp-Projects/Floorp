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
#include "nsAppShellCIDs.h"
#include "nsString.h"
#include "nsWidgetsCID.h"
#include "prprf.h"

//Interfaces needed to be included
#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"
#include "nsIDocumentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMXULDocument.h"
#include "nsIInterfaceRequestor.h"
#include "nsIIOService.h"
#include "nsIJSContextStack.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIObserverService.h"
#include "nsIWindowMediator.h"
#include "nsIScreenManager.h"
#include "nsIScreen.h"
#include "nsIScrollable.h"
#include "nsIPref.h"

#include "nsStyleConsts.h"

// XXX Get rid of this
#pragma message("WARNING: XXX bad include, remove it.")
#include "nsIWebShellWindow.h"
#include "nsWebShellWindow.h" // get rid of this one, too...

// CIDs
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

//*****************************************************************************
//***    nsXULWindow: Object Management
//*****************************************************************************

nsXULWindow::nsXULWindow() : mChromeTreeOwner(nsnull), 
   mContentTreeOwner(nsnull), mPrimaryContentTreeOwner(nsnull),
   mContinueModalLoop(PR_FALSE), mModalStatus(NS_OK), mChromeLoaded(PR_FALSE), 
   mShowAfterLoad(PR_FALSE), mIntrinsicallySized(PR_FALSE),
   mCenterAfterLoad(PR_FALSE), mZlevel(nsIXULWindow::normalZ)
   
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
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsXULWindow::nsIIntefaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsXULWindow::GetInterface(const nsIID& aIID, void** aSink)
{
  nsresult rv;
   NS_ENSURE_ARG_POINTER(aSink);

   if (aIID.Equals(NS_GET_IID(nsIPrompt))) {
     // XXX until nsIWebShellWindow goes away:
     nsCOMPtr<nsIWebShellWindow> webShellWin = 
       do_QueryInterface(NS_STATIC_CAST(nsIXULWindow*, this), &rv);
     if (NS_FAILED(rv)) return rv;
     return webShellWin->GetPrompter((nsIPrompt**)aSink);
   }
   if(aIID.Equals(NS_GET_IID(nsIWebBrowserChrome)) && 
      NS_SUCCEEDED(EnsureContentTreeOwner()) &&
      NS_SUCCEEDED(mContentTreeOwner->QueryInterface(aIID, aSink)))
      return NS_OK;
   else
      return QueryInterface(aIID, aSink);

   NS_IF_ADDREF(((nsISupports*)*aSink));
   return NS_OK;   
}

//*****************************************************************************
// nsXULWindow::nsIXULWindow
//*****************************************************************************   

NS_IMETHODIMP nsXULWindow::GetDocShell(nsIDocShell** aDocShell)
{
   NS_ENSURE_ARG_POINTER(aDocShell);

   *aDocShell = mDocShell;
   NS_IF_ADDREF(*aDocShell);
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetZlevel(PRUint32 *outLevel)
{
   *outLevel = mZlevel;
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetIntrinsicallySized(PRBool aIntrinsicallySized)
{
   mIntrinsicallySized = aIntrinsicallySized;
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetIntrinsicallySized(PRBool* aIntrinsicallySized)
{
   NS_ENSURE_ARG_POINTER(aIntrinsicallySized);

   *aIntrinsicallySized = mIntrinsicallySized;
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetPrimaryContentShell(nsIDocShellTreeItem** 
   aDocShellTreeItem)
{
   NS_ENSURE_ARG_POINTER(aDocShellTreeItem);
   *aDocShellTreeItem = nsnull;

   PRInt32 count = mContentShells.Count();
   for(PRInt32 i = 0; i < count; i++)
      {
      nsContentShellInfo* shellInfo = (nsContentShellInfo*)mContentShells.ElementAt(i);
      if(shellInfo->primary)
         {
         *aDocShellTreeItem = shellInfo->child;
         NS_ADDREF(*aDocShellTreeItem);
         return NS_OK;
         }
      }

   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULWindow::GetContentShellById(const PRUnichar* aID, 
   nsIDocShellTreeItem** aDocShellTreeItem)
{
   NS_ENSURE_ARG_POINTER(aDocShellTreeItem);
   *aDocShellTreeItem = nsnull;

   PRInt32 count = mContentShells.Count();
   for(PRInt32 i = 0; i < count; i++)
      {
      nsContentShellInfo* shellInfo = (nsContentShellInfo*)mContentShells.ElementAt(i);
      if(shellInfo->id == aID)
         {
         *aDocShellTreeItem = shellInfo->child;
         NS_ADDREF(*aDocShellTreeItem);
         return NS_OK;
         }
      }

   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULWindow::ShowModal()
{
   nsCOMPtr<nsIAppShell> appShell(do_CreateInstance(kAppShellCID));
   NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);

   appShell->Create(0, nsnull);
   appShell->Spinup();
   // Store locally so it doesn't die on us
   nsCOMPtr<nsIWidget> window = mWindow;
   nsCOMPtr<nsIXULWindow> tempRef = this;  
                                          

   window->SetModal(PR_TRUE);
   mContinueModalLoop = PR_TRUE;
   EnableParent(PR_FALSE);

   nsCOMPtr<nsIJSContextStack> stack(do_GetService("nsThreadJSContextStack"));
   nsresult rv = NS_OK;
   if(stack  && NS_SUCCEEDED(stack->Push(nsnull)))
      {
      while(NS_SUCCEEDED(rv) && mContinueModalLoop)
         {
         void* data;
         PRBool isRealEvent;
         PRBool processEvent;

         rv = appShell->GetNativeEvent(isRealEvent, data);
         if(NS_SUCCEEDED(rv))
            {
            window->ModalEventFilter(isRealEvent, data, &processEvent);
            if(processEvent)
               appShell->DispatchNativeEvent(isRealEvent, data);
            }
         }
      JSContext* cx;
      stack->Pop(&cx);
      NS_ASSERTION(cx == nsnull, "JSContextStack mismatch");
      }
   else
      rv = NS_ERROR_FAILURE;

   mContinueModalLoop = PR_FALSE;

   window->SetModal(PR_FALSE);
   appShell->Spindown();

   return mModalStatus;
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
   if(!mWindow)
      return NS_OK;

#ifdef XP_PC
   ActivateParent();
#endif

#ifdef XP_MAC // Anyone still using native menus should add themselves here.
  {
  // unregister as document listener
  // this is needed for menus
   nsCOMPtr<nsIContentViewer> cv;
   if(mDocShell)
 	   mDocShell->GetContentViewer(getter_AddRefs(cv));
   nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
   if(docv)
      {
      nsCOMPtr<nsIDocument> doc;
      docv->GetDocument(*getter_AddRefs(doc));
/*      if(doc)
         doc->RemoveObserver(NS_STATIC_CAST(nsIDocumentObserver*, this));  */
      }
   }
#endif

   nsCOMPtr<nsIAppShellService> appShell(do_GetService(kAppShellServiceCID));
   if(appShell)
      appShell->UnregisterTopLevelWindow(NS_STATIC_CAST(nsIXULWindow*, this));
   
   // let's make sure the window doesn't get deleted out from under us
   // while we are trying to close....this can happen if the docshell
   // we close ends up being the last owning reference to this xulwindow

   // XXXTAB This shouldn't be an issue anymore because the ownership model
   // only goes in one direction.  When webshell container is fully removed
   // try removing this...

   nsCOMPtr<nsIXULWindow> placeHolder = this;

   // Remove modality (if any) and hide while destroying. More than
   // a convenience, the hide prevents user interaction with the partially
   // destroyed window. This is especially necessary when the eldest window
   // in a stack of modal windows is destroyed first. It happens.
   ExitModalLoop(NS_OK);
   mWindow->Show(PR_FALSE);

   mDOMWindow = nsnull;
   if(mDocShell) {
      nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(mDocShell));
      shellAsWin->Destroy();
      mDocShell = nsnull; // this can cause reentrancy of this function
   }

   // Remove our ref on the content shells
   PRInt32 count;
   count = mContentShells.Count();
   for(PRInt32 i = 0; i < count; i++) {
      nsContentShellInfo* shellInfo = (nsContentShellInfo*)(mContentShells.ElementAt(i));
      delete shellInfo;
   }
   mContentShells.Clear();

   if(mContentTreeOwner) {
      mContentTreeOwner->XULWindow(nsnull);
      NS_RELEASE(mContentTreeOwner);
   }
   if(mPrimaryContentTreeOwner) {
      mPrimaryContentTreeOwner->XULWindow(nsnull);
      NS_RELEASE(mPrimaryContentTreeOwner);
   }
   if(mChromeTreeOwner) {
      mChromeTreeOwner->XULWindow(nsnull);
      NS_RELEASE(mChromeTreeOwner);
   }
   if(mWindow) {
      mWindow->SetClientData(0); // nsWebShellWindow hackery
      mWindow = nsnull;
   }

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetPosition(PRInt32 aX, PRInt32 aY)
{
   NS_ENSURE_SUCCESS(mWindow->Move(aX, aY), NS_ERROR_FAILURE);
   PersistPositionAndSize(PR_TRUE, PR_FALSE, PR_FALSE);
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetPosition(PRInt32* aX, PRInt32* aY)
{
   return GetPositionAndSize(aX, aY, nsnull, nsnull);
}

NS_IMETHODIMP nsXULWindow::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   mIntrinsicallySized = PR_FALSE;
   NS_ENSURE_SUCCESS(mWindow->Resize(aCX, aCY, aRepaint), NS_ERROR_FAILURE);
   PersistPositionAndSize(PR_FALSE, PR_TRUE, PR_FALSE);
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetSize(PRInt32* aCX, PRInt32* aCY)
{
   return GetPositionAndSize(nsnull, nsnull, aCX, aCY);
}

NS_IMETHODIMP nsXULWindow::SetPositionAndSize(PRInt32 aX, PRInt32 aY, 
   PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   mIntrinsicallySized = PR_FALSE;
   NS_ENSURE_SUCCESS(mWindow->Resize(aX, aY, aCX, aCY, aRepaint), NS_ERROR_FAILURE);
   PersistPositionAndSize(PR_TRUE, PR_TRUE, PR_FALSE);
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

// only does screen-centering right now
NS_IMETHODIMP nsXULWindow::Center(nsIXULWindow *aRelative, PRBool aScreen, PRBool aAlert) {

  PRInt32  left, top, width, height,
           ourWidth, ourHeight;
  nsresult result;

  if (!mChromeLoaded) {
    // note we lose the parameters. at time of writing, this isn't a problem.
    mCenterAfterLoad = PR_TRUE;
    return NS_OK;
  }

  if (!aScreen && !aRelative)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService("component://netscape/gfx/screenmanager", &result);
  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIScreen> screen;

  if (aRelative) {
    nsCOMPtr<nsIBaseWindow> base(do_QueryInterface(aRelative, &result));
    if (base) { // assume result will be an error indication
      base->GetPositionAndSize(&left, &top, &width, &height);
      if (aScreen)
        screenmgr->ScreenForRect(left, top, width, height, getter_AddRefs(screen));
    }
  } else
    screenmgr->GetPrimaryScreen(getter_AddRefs(screen));
  if (NS_FAILED(result))
    return result;

  if (aScreen)
    screen->GetAvailRect(&left, &top, &width, &height);

  GetSize(&ourWidth, &ourHeight);
  SetPosition(left+(width-ourWidth)/2, top+(height-ourHeight)/(aAlert?3:2));

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
   NS_ENSURE_STATE(mWindow);

   *aParentWidget = mWindow->GetParent();
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

   nsCOMPtr<nsIWidget> parentWidget;
   NS_ENSURE_SUCCESS(GetParentWidget(getter_AddRefs(parentWidget)), NS_ERROR_FAILURE);

   *aParentNativeWindow = parentWidget->GetNativeData(NS_NATIVE_WIDGET);
   
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
   if(!mChromeLoaded)
      {
      mShowAfterLoad = aVisibility;
      return NS_OK;
      }

   if(mDebuting)
      return NS_OK;
   mDebuting = PR_TRUE;  // (Show / Focus is recursive)

   //XXXTAB Do we really need to show docshell and the window?  Isn't 
   // the window good enough?
   nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(mDocShell));
   shellAsWin->SetVisibility(aVisibility);
   mWindow->Show(aVisibility);

   nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
   if(windowMediator)
      windowMediator->UpdateWindowTimeStamp(NS_STATIC_CAST(nsIXULWindow*, this));

   // Hide splash screen (if there is one).
   static PRBool splashScreenGone = PR_FALSE;
   if(!splashScreenGone)
      {
      nsCOMPtr<nsIAppShellService> appShellService(do_GetService(kAppShellServiceCID));
      if(appShellService)
         appShellService->HideSplashScreen();
      splashScreenGone = PR_TRUE;
      }
   mDebuting = PR_FALSE;
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);
   
   *aMainWidget = mWindow;
   NS_IF_ADDREF(*aMainWidget);
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

   windowMediator->UpdateWindowTitle(NS_STATIC_CAST(nsIXULWindow*, this), aTitle);

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

   mContentTreeOwner = new nsContentTreeOwner(PR_FALSE);
   NS_ENSURE_TRUE(mContentTreeOwner, NS_ERROR_FAILURE);

   NS_ADDREF(mContentTreeOwner);
   mContentTreeOwner->XULWindow(this);
   
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::EnsurePrimaryContentTreeOwner()
{
   if(mPrimaryContentTreeOwner)
      return NS_OK;

   mPrimaryContentTreeOwner = new nsContentTreeOwner(PR_TRUE);
   NS_ENSURE_TRUE(mPrimaryContentTreeOwner, NS_ERROR_FAILURE);

   NS_ADDREF(mPrimaryContentTreeOwner);
   mPrimaryContentTreeOwner->XULWindow(this);

   return NS_OK;
}

void nsXULWindow::OnChromeLoaded()
{
   mChromeLoaded = PR_TRUE;
  
   if(mContentTreeOwner)
      mContentTreeOwner->ApplyChromeFlags();

   LoadTitleFromXUL();
   LoadPositionAndSizeFromXUL(PR_TRUE, PR_TRUE);

   if(mIntrinsicallySized)
      {
      nsCOMPtr<nsIContentViewer> cv;
      mDocShell->GetContentViewer(getter_AddRefs(cv));
      nsCOMPtr<nsIMarkupDocumentViewer> markupViewer(do_QueryInterface(cv));
      if(markupViewer)
         markupViewer->SizeToContent();
      }

   //LoadContentAreas();

   if (mCenterAfterLoad)
      Center(nsnull, PR_TRUE, PR_FALSE);

   if(mShowAfterLoad)
      SetVisibility(PR_TRUE);
}

NS_IMETHODIMP nsXULWindow::LoadPositionAndSizeFromXUL(PRBool aPosition, 
   PRBool aSize)
{
/* NB: we'll want to pay attention to the "sizemode" attribute (maximizing
   the window if it asks) someday after the widget implementations know how
   to do that.
*/
   nsCOMPtr<nsIDOMElement> docShellElement;
   GetWindowDOMElement(getter_AddRefs(docShellElement));
   NS_ENSURE_TRUE(docShellElement, NS_ERROR_FAILURE);

   PRInt32 curX = 0;
   PRInt32 curY = 0;
   PRInt32 curCX = 0;
   PRInt32 curCY = 0;

   GetPositionAndSize(&curX, &curY, &curCX, &curCY);

   PRInt32 errorCode;
   PRInt32 temp;

   if(aPosition)
      {
      PRInt32 specX = curX;
      PRInt32 specY = curY;
      nsAutoString sizeString;

      if(NS_SUCCEEDED(docShellElement->GetAttribute(NS_ConvertASCIItoUCS2("screenX"), sizeString)))
         {
         temp = sizeString.ToInteger(&errorCode);
         if(NS_SUCCEEDED(errorCode) && temp > 0)
            specX = temp;
         }
      if(NS_SUCCEEDED(docShellElement->GetAttribute(NS_ConvertASCIItoUCS2("screenY"), sizeString)))
         {
         temp = sizeString.ToInteger(&errorCode);
         if(NS_SUCCEEDED(errorCode) && temp > 0)
            specY = temp;
         }

      mWindow->ConstrainPosition(&specX, &specY);
      if((specX != curX) || (specY != curY))
         SetPosition(specX, specY);
      }

   if(aSize)
      {
      PRInt32 specCX = curCX;
      PRInt32 specCY = curCY;
      nsAutoString sizeString;

      if(NS_SUCCEEDED(docShellElement->GetAttribute(NS_ConvertASCIItoUCS2("width"), sizeString)))
         {
         temp = sizeString.ToInteger(&errorCode);
         if(NS_SUCCEEDED(errorCode) && temp > 0)
            {
            specCX = temp;
            mIntrinsicallySized = PR_FALSE;
            }
         }
      if(NS_SUCCEEDED(docShellElement->GetAttribute(NS_ConvertASCIItoUCS2("height"), sizeString)))
         {
         temp = sizeString.ToInteger(&errorCode);
         if(NS_SUCCEEDED(errorCode) && temp > 0)
            {
            specCY = temp;
            mIntrinsicallySized = PR_FALSE;
            }
         }

      if((specCX != curCX) || (specCY != curCY))
         SetSize(specCX, specCY, PR_FALSE);
      }

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::LoadTitleFromXUL()
{
   nsCOMPtr<nsIDOMElement> docShellElement;
   GetWindowDOMElement(getter_AddRefs(docShellElement));
   NS_ENSURE_TRUE(docShellElement, NS_ERROR_FAILURE);

   nsAutoString windowTitle;
   docShellElement->GetAttribute(NS_ConvertASCIItoUCS2("title"), windowTitle);
   if(windowTitle.IsEmpty())
      return NS_OK;

   NS_ENSURE_SUCCESS(EnsureChromeTreeOwner(), NS_ERROR_FAILURE);
   mChromeTreeOwner->SetTitle(windowTitle.GetUnicode());

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::PersistPositionAndSize(PRBool aPosition, PRBool aSize, PRBool aSizeMode)
{
   // can happen when the persistence timer fires at an inopportune time
   // during window shutdown
   if (!mDocShell)
     return NS_ERROR_FAILURE;

   nsCOMPtr<nsIDOMElement> docShellElement;
   GetWindowDOMElement(getter_AddRefs(docShellElement));
   if(!docShellElement)
      return NS_ERROR_FAILURE;

   PRInt32 x, y, cx, cy;
   PRInt32 sizeMode;

   NS_ENSURE_SUCCESS(GetPositionAndSize(&x, &y, &cx, &cy), NS_ERROR_FAILURE);
   mWindow->GetSizeMode(&sizeMode);

   // (But only for size elements which are persisted.)
   /* Note we use the same cheesy way to determine that as in
     nsXULDocument.cpp. Some day that'll be fixed and there will
     be an obscure bug here. */
   /* Note that storing sizes which are not persisted makes it
     difficult to distinguish between windows intrinsically sized
     and not. */
   nsAutoString   persistString;
   docShellElement->GetAttribute(NS_ConvertASCIItoUCS2("persist"), persistString);

   char           sizeBuf[10];
   nsAutoString   sizeString;
   if(aPosition && sizeMode == nsSizeMode_Normal)
      {
      if(persistString.Find("screenX") >= 0)
         {
         PR_snprintf(sizeBuf, sizeof(sizeBuf), "%ld", (long)x);
         sizeString.AssignWithConversion(sizeBuf);
         docShellElement->SetAttribute(NS_ConvertASCIItoUCS2("screenX"), sizeString);
         }
      if(persistString.Find("screenY") >= 0)
         {
         PR_snprintf(sizeBuf, sizeof(sizeBuf), "%ld", (long)y);
         sizeString.AssignWithConversion(sizeBuf);
         docShellElement->SetAttribute(NS_ConvertASCIItoUCS2("screenY"), sizeString);
         }
      }

   if(aSize && sizeMode == nsSizeMode_Normal)
      {
      if(persistString.Find("width") >= 0)
         {
         PR_snprintf(sizeBuf, sizeof(sizeBuf), "%ld", (long)cx);
         sizeString.AssignWithConversion(sizeBuf);
         docShellElement->SetAttribute(NS_ConvertASCIItoUCS2("width"), sizeString);
         }
      if(persistString.Find("height") >= 0)
         {
         PR_snprintf(sizeBuf, sizeof(sizeBuf), "%ld", (long)cy);
         sizeString.AssignWithConversion(sizeBuf);
         docShellElement->SetAttribute(NS_ConvertASCIItoUCS2("height"), sizeString);
         }
      }

   if (aSizeMode && persistString.Find("sizemode") >= 0) {
      PRInt32 sizemode;
      if (NS_FAILED(mWindow->GetSizeMode(&sizemode)))
        sizemode = nsSizeMode_Normal;
      sizeString.AssignWithConversion("n");
      if (sizemode == nsSizeMode_Minimized)
        sizeString.AssignWithConversion("m");
      else if (sizemode == nsSizeMode_Maximized)
        sizeString.AssignWithConversion("M");
      docShellElement->SetAttribute(NS_ConvertASCIItoUCS2("sizemode"), sizeString);
   }

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetWindowDOMWindow(nsIDOMWindow** aDOMWindow)
{
   NS_ENSURE_STATE(mDocShell);

   if(!mDOMWindow)
      mDOMWindow = do_GetInterface(mDocShell);
   NS_ENSURE_TRUE(mDOMWindow, NS_ERROR_FAILURE);

   *aDOMWindow = mDOMWindow;
   NS_ADDREF(*aDOMWindow);
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetWindowDOMElement(nsIDOMElement** aDOMElement)
{
   NS_ENSURE_STATE(mDocShell);
   NS_ENSURE_ARG_POINTER(aDOMElement);

   *aDOMElement = nsnull;

   nsCOMPtr<nsIContentViewer> cv;
   
   mDocShell->GetContentViewer(getter_AddRefs(cv));
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

NS_IMETHODIMP nsXULWindow::GetDOMElementById(char* aID, nsIDOMElement** aDOMElement)
{
   NS_ENSURE_STATE(mDocShell);
   NS_ENSURE_ARG_POINTER(aDOMElement);

   *aDOMElement = nsnull;

   nsCOMPtr<nsIContentViewer> cv;
   
   mDocShell->GetContentViewer(getter_AddRefs(cv));
   if(!cv)
      return NS_ERROR_FAILURE;

   nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
   if(!docv)   
      return NS_ERROR_FAILURE;

   nsCOMPtr<nsIDocument> doc;
   docv->GetDocument(*getter_AddRefs(doc));
   nsCOMPtr<nsIDOMXULDocument> domdoc(do_QueryInterface(doc));
   if(!domdoc) 
      return NS_ERROR_FAILURE;
   
   NS_ENSURE_SUCCESS(domdoc->GetElementById(NS_ConvertASCIItoUCS2(aID), aDOMElement), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   PRBool aPrimary, const PRUnichar* aID)
{
  nsContentShellInfo* shellInfo = nsnull;
  nsAutoString newID(aID);

  PRInt32 count = mContentShells.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsContentShellInfo* info = (nsContentShellInfo*)mContentShells.ElementAt(i);
    nsAutoString srcID(info->id);
       
    if (info->primary == aPrimary && srcID.Equals(newID)) {
      // We already exist. Do a replace.
      info->child = aContentShell;
      shellInfo = info;
      break;
    }
  }

  if (!shellInfo) {
    shellInfo = new nsContentShellInfo(nsAutoString(aID), aPrimary, aContentShell);
    mContentShells.AppendElement((void*)shellInfo);
  }
    
  // Set the default content tree owner if one does not exist.
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  aContentShell->GetTreeOwner(getter_AddRefs(treeOwner));
  if (!treeOwner) {
    if (aPrimary) {
      NS_ENSURE_SUCCESS(EnsurePrimaryContentTreeOwner(), NS_ERROR_FAILURE);
      aContentShell->SetTreeOwner(mPrimaryContentTreeOwner);
    }
    else {
      NS_ENSURE_SUCCESS(EnsureContentTreeOwner(), NS_ERROR_FAILURE);
      aContentShell->SetTreeOwner(mContentTreeOwner);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SizeShellTo(nsIDocShellTreeItem* aShellItem,
   PRInt32 aCX, PRInt32 aCY)
{
   // XXXTAB This is wrong, we should actually reflow based on the passed in'
   // shell.  For now we are hacking and doing delta sizing.  This is bad
   // because it assumes all size we add will go to the shell which probably
   // won't happen.

   nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(aShellItem));
   NS_ENSURE_TRUE(shellAsWin, NS_ERROR_FAILURE);

   PRInt32 width = 0;
   PRInt32 height = 0;
   shellAsWin->GetSize(&width, &height);

   PRInt32 widthDelta = aCX - width;
   PRInt32 heightDelta = aCY - height;

   if(widthDelta || heightDelta)
      {
      PRInt32 winCX = 0;
      PRInt32 winCY = 0;

      GetSize(&winCX, &winCY);
      SetSize(winCX + widthDelta, winCY + heightDelta, PR_TRUE);
      PersistPositionAndSize(PR_FALSE, PR_TRUE, PR_FALSE);
      }

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::ExitModalLoop(nsresult aStatus)
{
   if (mContinueModalLoop) // was a modal window
     EnableParent(PR_TRUE);
   mContinueModalLoop = PR_FALSE;
   mModalStatus = aStatus;
   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetNewWindow(PRInt32 aChromeFlags,
   nsIDocShellTreeItem** aDocShellTreeItem)
{
   NS_ENSURE_ARG_POINTER(aDocShellTreeItem);

   if(aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)
      return CreateNewChromeWindow(aChromeFlags, aDocShellTreeItem);
   else
      return CreateNewContentWindow(aChromeFlags, aDocShellTreeItem);

   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULWindow::CreateNewChromeWindow(PRInt32 aChromeFlags,
   nsIDocShellTreeItem** aDocShellTreeItem)
{
   nsCOMPtr<nsIAppShellService> appShell(do_GetService(kAppShellServiceCID));
   NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);
   
   // Just do a normal create of a window and return.
   //XXXTAB remove this when appshell talks in terms of nsIXULWindow
   nsCOMPtr<nsIXULWindow> parent;
   if(aChromeFlags & nsIWebBrowserChrome::CHROME_DEPENDENT)
      parent = this;

   nsCOMPtr<nsIXULWindow> newWindow;
   appShell->CreateTopLevelWindow(parent, nsnull, PR_FALSE, PR_FALSE,
      aChromeFlags, NS_SIZETOCONTENT, NS_SIZETOCONTENT,
      getter_AddRefs(newWindow));

   NS_ENSURE_TRUE(newWindow, NS_ERROR_FAILURE);

   // XXX Ick, this should be able to go away.....
   nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(newWindow));
   if(browserChrome)
      browserChrome->SetChromeFlags(aChromeFlags);

   nsCOMPtr<nsIDocShell> docShell;
   newWindow->GetDocShell(getter_AddRefs(docShell));
   CallQueryInterface(docShell, aDocShellTreeItem);

   return NS_OK;
}

NS_IMETHODIMP nsXULWindow::CreateNewContentWindow(PRInt32 aChromeFlags,
   nsIDocShellTreeItem** aDocShellTreeItem)
{
   nsCOMPtr<nsIAppShellService> appShell(do_GetService(kAppShellServiceCID));
   NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);

   // We need to create a new top level window and then enter a nested
   // loop. Eventually the new window will be told that it has loaded,
   // at which time we know it is safe to spin out of the nested loop
   // and allow the opening code to proceed.

   // First push a nested event queue for event processing from netlib
   // onto our UI thread queue stack.
   nsEventQueueStack queuePusher;
   NS_ENSURE_SUCCESS(queuePusher.Success(), NS_ERROR_FAILURE);

   nsCOMPtr<nsIURI> uri;

   nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
   if (prefs) {
     char *urlStr;
     PRBool strAllocated = PR_TRUE;
     nsresult prefres;
     prefres = prefs->CopyCharPref("browser.chromeURL", &urlStr);
     if (NS_SUCCEEDED(prefres) && urlStr[0] == '\0') {
       PL_strfree(urlStr);
       prefres = NS_ERROR_FAILURE;
     }
     if (NS_FAILED(prefres)) {
       urlStr = "chrome://navigator/content/navigator.xul";
       strAllocated = PR_FALSE;
     }

     nsCOMPtr<nsIIOService> service(do_GetService(kIOServiceCID));
     if (service)
       service->NewURI(urlStr, nsnull, getter_AddRefs(uri));
     if (strAllocated)
       PL_strfree(urlStr);
   }
   NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

   nsCOMPtr<nsIXULWindow> newWindow;
   appShell->CreateTopLevelWindow(nsnull, uri, PR_FALSE, PR_FALSE,
                                 aChromeFlags, 615, 480,
                                 getter_AddRefs(newWindow));

   NS_ENSURE_TRUE(newWindow, NS_ERROR_FAILURE);

   nsCOMPtr<nsIWebShellWindow> webShellWindow(do_QueryInterface(newWindow));

   nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(newWindow));
   if(browserChrome)
      browserChrome->SetChromeFlags(aChromeFlags);

   nsCOMPtr<nsIAppShell> subShell(do_CreateInstance(kAppShellCID));
   NS_ENSURE_TRUE(subShell, NS_ERROR_FAILURE);

   subShell->Create(0, nsnull);
   subShell->Spinup();

   // Specify that we want the window to remain locked until the chrome has loaded.
   webShellWindow->LockUntilChromeLoad();

   PRBool locked = PR_FALSE;
   webShellWindow->GetLockedState(locked);

   // Push nsnull onto the JSContext stack before we dispatch a native event.
   nsCOMPtr<nsIJSContextStack> stack(do_GetService("nsThreadJSContextStack"));
   if(stack && NS_SUCCEEDED(stack->Push(nsnull)))
      {
      nsresult looprv = NS_OK;
      while(NS_SUCCEEDED(looprv) && locked)
         {
         void      *data;
         PRBool    isRealEvent;
    
         looprv = subShell->GetNativeEvent(isRealEvent, data);
         subShell->DispatchNativeEvent(isRealEvent, data);

         webShellWindow->GetLockedState(locked);
         }

      JSContext *cx;
      stack->Pop(&cx);
      NS_ASSERTION(cx == nsnull, "JSContextStack mismatch");
      }

   subShell->Spindown();

   // We're out of the nested loop.
   // During the layout of the new window, all content shells were located and placed
   // into the new window's content shell array.  Locate the "content area" content
   // shell.
   newWindow->GetPrimaryContentShell(aDocShellTreeItem);
   return NS_OK;
}

// XXX can this switch now?
/// This should rightfully be somebody's PROGID?
// Will switch when the "app shell browser component" arrives.
static const char *prefix = "component://netscape/appshell/component/browser/window";

NS_IMETHODIMP nsXULWindow::NotifyObservers(const PRUnichar* aTopic, 
   const PRUnichar* aData)
{
   nsCOMPtr<nsIObserverService> service(do_GetService(NS_OBSERVERSERVICE_PROGID));

   if(!service)
      return NS_ERROR_FAILURE;

   nsCOMPtr<nsIWebShellWindow> 
      removeme(do_QueryInterface(NS_STATIC_CAST(nsIXULWindow*, this)));

   nsAutoString topic; topic.AssignWithConversion(prefix);
   topic.AppendWithConversion(";");
   topic += aTopic;

   NS_ENSURE_SUCCESS(service->Notify(removeme, topic.GetUnicode(), aData),
      NS_ERROR_FAILURE);
   return NS_OK;
}

void nsXULWindow::EnableParent(PRBool aEnable)
{
  nsCOMPtr<nsIBaseWindow> parentWindow;
  nsCOMPtr<nsIWidget>     parentWidget;

  parentWindow = do_QueryReferent(mParentWindow);
  if (parentWindow)
    parentWindow->GetMainWidget(getter_AddRefs(parentWidget));
  if (parentWidget)
    parentWidget->Enable(aEnable);
}

/* Bring our parent window to the top, if we are the topmost window.
   This stupid-sounding method is used to hack around a bug in the Windows OS
   which causes it to seemingly activate a random window instead of the
   expected one after closing at least two owned windows. (Mozilla bug
   22658). This method is expected to be called during window teardown.
*/
void nsXULWindow::ActivateParent() {

  // do we have an owner/parent window?
  nsCOMPtr<nsIBaseWindow> parent(do_QueryReferent(mParentWindow));
  if (!parent)
    return;

  // are we the topmost window?
  nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
  if (!windowMediator)
    return;

  nsCOMPtr<nsIDOMWindow> topDOMWindow;
  windowMediator->GetMostRecentWindow(nsnull, getter_AddRefs(topDOMWindow));
  nsCOMPtr<nsIDOMWindow> ourDOMWindow(do_GetInterface(mDocShell));

  if (ourDOMWindow != topDOMWindow)
    return;

  // yes, we're topmost. bring our parent to the top.
  nsCOMPtr<nsIWidget> parentWidget;
  parent->GetMainWidget(getter_AddRefs(parentWidget));
  if (parentWidget)
    parentWidget->PlaceBehind(0, PR_TRUE);
}

// Constrain the window to its proper z-level
PRBool nsXULWindow::ConstrainToZLevel(
                      PRBool      aImmediate,
                      nsWindowZ  *aPlacement,
                      nsIWidget  *aReqBelow,
                      nsIWidget **aActualBelow) {

#if 0
  /* Do we have a parent window? This means our z-order is already constrained,
     since we're a dependent window. Our window list isn't hierarchical,
     so we can't properly calculate placement for such a window.
     Should we just abort? */
  nsCOMPtr<nsIBaseWindow> parentWindow = do_QueryReferent(mParentWindow);
  if (parentWindow)
    return PR_FALSE;
#endif

  nsCOMPtr<nsIWindowMediator> mediator(do_GetService(kWindowMediatorCID));
  if(!mediator)
    return PR_FALSE;

  PRBool         altered;
  PRUint32       position,
                 newPosition;
  nsIXULWindow  *us = NS_STATIC_CAST(nsIXULWindow *, this);

  altered = PR_FALSE;

  // translate from nsIWindowMediator to nsGUIEvent constants
  position = nsIWindowMediator::zLevelTop;
  if (*aPlacement == nsWindowZBottom)
    position = nsIWindowMediator::zLevelBottom;
  else if (*aPlacement == nsWindowZRelative)
    position = nsIWindowMediator::zLevelBelow;

  if (NS_SUCCEEDED(mediator->CalculateZPosition(us, position, aReqBelow,
                               &newPosition, aActualBelow, &altered))) {
    if (altered || aImmediate) {
      if (newPosition == nsIWindowMediator::zLevelTop)
        *aPlacement = nsWindowZTop;
      else if (newPosition == nsIWindowMediator::zLevelBottom)
        *aPlacement = nsWindowZBottom;
      else
        *aPlacement = nsWindowZRelative;

      if (aImmediate) {
        nsCOMPtr<nsIBaseWindow> ourBase = do_QueryInterface(NS_STATIC_CAST(nsIXULWindow *,this));
        if (ourBase) {
          nsCOMPtr<nsIWidget> ourWidget;
          ourBase->GetMainWidget(getter_AddRefs(ourWidget));
          ourWidget->PlaceBehind(*aActualBelow, PR_FALSE);
        }
      }
    }

    /* (CalculateZPosition can tell us to be below nothing, because it tries
       not to change something it doesn't recognize. A request to verify
       being below an unrecognized window, then, is treated as a request
       to come to the top (below null) */
    nsCOMPtr<nsIXULWindow> windowAbove;
    if (newPosition == nsIWindowMediator::zLevelBelow && *aActualBelow) {
      void *data;
      (*aActualBelow)->GetClientData(data);
      if (data) {
        nsWebShellWindow *win;
        win = NS_REINTERPRET_CAST(nsWebShellWindow *, data);
        windowAbove = do_QueryInterface(NS_STATIC_CAST(nsIWebShellWindow *,win));
      }
    }

    mediator->SetZPosition(us, newPosition, windowAbove);
  }

  return altered;
}

/* Disable scrollbars in the primary content shell (to open a browser window
   without scrollbars, for example)
*/
void nsXULWindow::KillContentScrollbars() {

  nsCOMPtr<nsIDocShellTreeItem> content;
  if (NS_SUCCEEDED(GetPrimaryContentShell(getter_AddRefs(content))) && content) {
    nsCOMPtr<nsIScrollable> shell(do_QueryInterface(content));
    if (shell)
      shell->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y, NS_STYLE_OVERFLOW_HIDDEN);
      shell->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X, NS_STYLE_OVERFLOW_HIDDEN);
  }
}

//*****************************************************************************
// nsXULWindow: Accessors
//*****************************************************************************

//*****************************************************************************
//*** nsContentShellInfo: Object Management
//*****************************************************************************   

nsContentShellInfo::nsContentShellInfo(const nsString& aID, PRBool aPrimary, 
   nsIDocShellTreeItem* aContentShell) : id(aID), primary(aPrimary), child(aContentShell)
{
}

nsContentShellInfo::~nsContentShellInfo()
{
   //XXX Set Tree Owner to null if the tree owner is nsXULWindow->mContentTreeOwner
} 

//*****************************************************************************
//*** nsEventQueueStack: Object Implementation
//*****************************************************************************   

nsEventQueueStack::nsEventQueueStack() : mQueue(nsnull)
{
   mService = do_GetService(kEventQueueServiceCID);

   if(mService)
      mService->PushThreadEventQueue(getter_AddRefs(mQueue));
}
nsEventQueueStack::~nsEventQueueStack()
{
   if(mQueue)
      mService->PopThreadEventQueue(mQueue);
   mService = nsnull;
}

nsresult nsEventQueueStack::Success()
{
   return mQueue ? NS_OK : NS_ERROR_FAILURE; 
}



