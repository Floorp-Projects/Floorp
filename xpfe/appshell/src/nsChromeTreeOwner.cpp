/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Local Includes
#include "nsChromeTreeOwner.h"
#include "nsXULWindow.h"

// Helper Classes
#include "nsString.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIServiceManager.h"
#include "nsIDocShellTreeItem.h"

// Interfaces needed to include
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIWebProgress.h"
#include "nsIWindowMediator.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULElement.h"
#include "nsIXULBrowserWindow.h"

// CIDs
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

//*****************************************************************************
// nsChromeTreeOwner string literals
//*****************************************************************************

struct nsChromeTreeOwnerLiterals
{
  const nsLiteralString kPersist;
  const nsLiteralString kScreenX;
  const nsLiteralString kScreenY;
  const nsLiteralString kWidth;
  const nsLiteralString kHeight;
  const nsLiteralString kSizemode;
  const nsLiteralString kSpace;

  nsChromeTreeOwnerLiterals()
    : NS_LITERAL_STRING_INIT(kPersist,"persist")
    , NS_LITERAL_STRING_INIT(kScreenX,"screenX")
    , NS_LITERAL_STRING_INIT(kScreenY,"screenY")
    , NS_LITERAL_STRING_INIT(kWidth,"width")
    , NS_LITERAL_STRING_INIT(kHeight,"height")
    , NS_LITERAL_STRING_INIT(kSizemode,"sizemode")
    , NS_LITERAL_STRING_INIT(kSpace," ")
  {}
};

static nsChromeTreeOwnerLiterals *gLiterals;

nsresult
nsChromeTreeOwner::InitGlobals()
{
  NS_ASSERTION(gLiterals == nullptr, "already initialized");
  gLiterals = new nsChromeTreeOwnerLiterals();
  if (!gLiterals)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

void
nsChromeTreeOwner::FreeGlobals()
{
  delete gLiterals;
  gLiterals = nullptr;
}

//*****************************************************************************
//***    nsChromeTreeOwner: Object Management
//*****************************************************************************

nsChromeTreeOwner::nsChromeTreeOwner() : mXULWindow(nullptr)
{
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
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsChromeTreeOwner::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsChromeTreeOwner::GetInterface(const nsIID& aIID, void** aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);

  if(aIID.Equals(NS_GET_IID(nsIPrompt))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if(aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if(aIID.Equals(NS_GET_IID(nsIWebBrowserChrome))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIEmbeddingSiteWindow))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIXULWindow))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->QueryInterface(aIID, aSink);
  }

  return QueryInterface(aIID, aSink);
}

//*****************************************************************************
// nsChromeTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************   

NS_IMETHODIMP nsChromeTreeOwner::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem* aOriginalRequestor,
   nsIDocShellTreeItem** aFoundItem)
{
   NS_ENSURE_ARG_POINTER(aFoundItem);

   *aFoundItem = nullptr;

   bool fIs_Content = false;

   /* Special Cases */
   if(!aName || !*aName)
      return NS_OK;

   nsDependentString name(aName);

   if(name.LowerCaseEqualsLiteral("_blank"))
      return NS_OK;
   // _main is an IE target which should be case-insensitive but isn't
   // see bug 217886 for details
   if(name.LowerCaseEqualsLiteral("_content") || name.EqualsLiteral("_main"))
      {
      NS_ENSURE_STATE(mXULWindow);
      fIs_Content = true;
      mXULWindow->GetPrimaryContentShell(aFoundItem);
      if(*aFoundItem)
         return NS_OK;
      // Otherwise fall through and ask the other windows for a content area.
      }

   nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
   NS_ENSURE_TRUE(windowMediator, NS_ERROR_FAILURE);

   nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
   NS_ENSURE_SUCCESS(windowMediator->GetXULWindowEnumerator(nullptr, 
      getter_AddRefs(windowEnumerator)), NS_ERROR_FAILURE);
   
   bool more;
   
   windowEnumerator->HasMoreElements(&more);
   while(more)
      {
      nsCOMPtr<nsISupports> nextWindow = nullptr;
      windowEnumerator->GetNext(getter_AddRefs(nextWindow));
      nsCOMPtr<nsIXULWindow> xulWindow(do_QueryInterface(nextWindow));
      NS_ENSURE_TRUE(xulWindow, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDocShellTreeItem> shellAsTreeItem;
     
      if(fIs_Content)
         {
         xulWindow->GetPrimaryContentShell(aFoundItem);
         }
      else
         {
         // Note that we don't look for targetable content shells here...
         // in fact, we aren't looking for content shells at all!
         nsCOMPtr<nsIDocShell> shell;
         xulWindow->GetDocShell(getter_AddRefs(shell));
         shellAsTreeItem = do_QueryInterface(shell);
         if (shellAsTreeItem) {
           // Get the root tree item of same type, since roots are the only
           // things that call into the treeowner to look for named items.
           nsCOMPtr<nsIDocShellTreeItem> root;
           shellAsTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(root));
           shellAsTreeItem = root;
         }
         if(shellAsTreeItem && aRequestor != shellAsTreeItem)
            {
            // Do this so we can pass in the tree owner as the requestor so the child knows not
            // to call back up.
            nsCOMPtr<nsIDocShellTreeOwner> shellOwner;
            shellAsTreeItem->GetTreeOwner(getter_AddRefs(shellOwner));
            nsCOMPtr<nsISupports> shellOwnerSupports(do_QueryInterface(shellOwner));

            shellAsTreeItem->FindItemWithName(aName, shellOwnerSupports,
                                              aOriginalRequestor, aFoundItem);
            }
         }
      if(*aFoundItem)
         return NS_OK;   
      windowEnumerator->HasMoreElements(&more);
      }
   return NS_OK;      
}

NS_IMETHODIMP
nsChromeTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
                                     bool aPrimary, bool aTargetable,
                                     const nsAString& aID)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->ContentShellAdded(aContentShell, aPrimary, aTargetable,
                                       aID);
}

NS_IMETHODIMP
nsChromeTreeOwner::ContentShellRemoved(nsIDocShellTreeItem* aContentShell)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->ContentShellRemoved(aContentShell);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetPrimaryContentShell(aShell);
}

NS_IMETHODIMP nsChromeTreeOwner::SizeShellTo(nsIDocShellTreeItem* aShellItem,
   PRInt32 aCX, PRInt32 aCY)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SizeShellTo(aShellItem, aCX, aCY);
}

NS_IMETHODIMP
nsChromeTreeOwner::SetPersistence(bool aPersistPosition,
                                  bool aPersistSize,
                                  bool aPersistSizeMode)
{
  NS_ENSURE_STATE(mXULWindow);
  nsCOMPtr<nsIDOMElement> docShellElement;
  mXULWindow->GetWindowDOMElement(getter_AddRefs(docShellElement));
  if (!docShellElement)
    return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(gLiterals->kPersist, persistString);

  bool saveString = false;
  PRInt32 index;

#define FIND_PERSIST_STRING(aString, aCond)            \
  index = persistString.Find(aString);                 \
  if (!aCond && index > kNotFound) {                   \
    persistString.Cut(index, aString.Length());        \
    saveString = true;                              \
  } else if (aCond && index == kNotFound) {            \
    persistString.Append(gLiterals->kSpace + aString); \
    saveString = true;                              \
  }
  FIND_PERSIST_STRING(gLiterals->kScreenX,  aPersistPosition);
  FIND_PERSIST_STRING(gLiterals->kScreenY,  aPersistPosition);
  FIND_PERSIST_STRING(gLiterals->kWidth,    aPersistSize);
  FIND_PERSIST_STRING(gLiterals->kHeight,   aPersistSize);
  FIND_PERSIST_STRING(gLiterals->kSizemode, aPersistSizeMode);

  if (saveString) 
    docShellElement->SetAttribute(gLiterals->kPersist, persistString);

  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::GetPersistence(bool* aPersistPosition,
                                  bool* aPersistSize,
                                  bool* aPersistSizeMode)
{
  NS_ENSURE_STATE(mXULWindow);
  nsCOMPtr<nsIDOMElement> docShellElement;
  mXULWindow->GetWindowDOMElement(getter_AddRefs(docShellElement));
  if (!docShellElement) 
    return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(gLiterals->kPersist, persistString);

  // data structure doesn't quite match the question, but it's close enough
  // for what we want (since this method is never actually called...)
  if (aPersistPosition)
    *aPersistPosition = persistString.Find(gLiterals->kScreenX) > kNotFound ||
                        persistString.Find(gLiterals->kScreenY) > kNotFound;
  if (aPersistSize)
    *aPersistSize = persistString.Find(gLiterals->kWidth) > kNotFound ||
                    persistString.Find(gLiterals->kHeight) > kNotFound;
  if (aPersistSizeMode)
    *aPersistSizeMode = persistString.Find(gLiterals->kSizemode) > kNotFound;

  return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::GetTargetableShellCount(PRUint32* aResult)
{
  *aResult = 0;
  return NS_OK;
}

//*****************************************************************************
// nsChromeTreeOwner::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsChromeTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   // Ignore widget parents for now.  Don't think those are a vaild thing to call.
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, false), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::Create()
{
   NS_ASSERTION(false, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsChromeTreeOwner::Destroy()
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->Destroy();
}

NS_IMETHODIMP nsChromeTreeOwner::SetPosition(PRInt32 x, PRInt32 y)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetPosition(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPosition(PRInt32* x, PRInt32* y)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetPosition(x, y);
}

NS_IMETHODIMP nsChromeTreeOwner::SetSize(PRInt32 cx, PRInt32 cy, bool fRepaint)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetSize(cx, cy, fRepaint);
}

NS_IMETHODIMP nsChromeTreeOwner::GetSize(PRInt32* cx, PRInt32* cy)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetSize(cx, cy);
}

NS_IMETHODIMP nsChromeTreeOwner::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, bool fRepaint)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetPositionAndSize(x, y, cx, cy, fRepaint);
}

NS_IMETHODIMP nsChromeTreeOwner::GetPositionAndSize(PRInt32* x, PRInt32* y, PRInt32* cx,
   PRInt32* cy)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetPositionAndSize(x, y, cx, cy);
}

NS_IMETHODIMP nsChromeTreeOwner::Repaint(bool aForce)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->Repaint(aForce);
}

NS_IMETHODIMP nsChromeTreeOwner::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetParentWidget(aParentWidget);
}

NS_IMETHODIMP nsChromeTreeOwner::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(false, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsChromeTreeOwner::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetParentNativeWindow(aParentNativeWindow);
}

NS_IMETHODIMP nsChromeTreeOwner::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(false, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsChromeTreeOwner::GetNativeHandle(nsAString& aNativeHandle)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetNativeHandle(aNativeHandle);
}

NS_IMETHODIMP nsChromeTreeOwner::GetVisibility(bool* aVisibility)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsChromeTreeOwner::SetVisibility(bool aVisibility)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsChromeTreeOwner::GetEnabled(bool *aEnabled)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetEnabled(aEnabled);
}

NS_IMETHODIMP nsChromeTreeOwner::SetEnabled(bool aEnable)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetEnabled(aEnable);
}

NS_IMETHODIMP nsChromeTreeOwner::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);
   NS_ENSURE_STATE(mXULWindow);

   *aMainWidget = mXULWindow->mWindow;
   NS_IF_ADDREF(*aMainWidget);

   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::SetFocus()
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetFocus();
}

NS_IMETHODIMP nsChromeTreeOwner::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetTitle(aTitle);
}

NS_IMETHODIMP nsChromeTreeOwner::SetTitle(const PRUnichar* aTitle)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetTitle(aTitle);
}

//*****************************************************************************
// nsChromeTreeOwner::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP
nsChromeTreeOwner::OnProgressChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    PRInt32 aCurSelfProgress,
                                    PRInt32 aMaxSelfProgress, 
                                    PRInt32 aCurTotalProgress,
                                    PRInt32 aMaxTotalProgress)
{
   return NS_OK;
}

NS_IMETHODIMP
nsChromeTreeOwner::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 PRUint32 aProgressStateFlags,
                                 nsresult aStatus)
{
   return NS_OK;
}

NS_IMETHODIMP nsChromeTreeOwner::OnLocationChange(nsIWebProgress* aWebProgress,
                                                  nsIRequest* aRequest,
                                                  nsIURI* aLocation,
                                                  PRUint32 aFlags)
{
  bool itsForYou = true;

  if (aWebProgress) {
    NS_ENSURE_STATE(mXULWindow);
    nsCOMPtr<nsIDOMWindow> progressWin;
    aWebProgress->GetDOMWindow(getter_AddRefs(progressWin));

    nsCOMPtr<nsIDocShell> docshell;
    mXULWindow->GetDocShell(getter_AddRefs(docshell));
    nsCOMPtr<nsIDOMWindow> ourWin(do_QueryInterface(docshell));

    if (ourWin != progressWin)
      itsForYou = false;
  }

   // If loading a new root .xul document, then redo chrome.
  if (itsForYou) {
    NS_ENSURE_STATE(mXULWindow);
    mXULWindow->mChromeLoaded = false;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsChromeTreeOwner::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const PRUnichar* aMessage)
{
    return NS_OK;
}



NS_IMETHODIMP 
nsChromeTreeOwner::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRUint32 state)
{
    return NS_OK;
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
