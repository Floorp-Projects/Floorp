/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=79:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Local Includes
#include "nsContentTreeOwner.h"
#include "nsXULWindow.h"

// Helper Classes
#include "nsIServiceManager.h"
#include "nsAutoPtr.h"
#include "nsCxPusher.h"

// Interfaces needed to be included
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDOMXULElement.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIWindowMediator.h"
#include "nsIXULBrowserWindow.h"
#include "nsIPrincipal.h"
#include "nsIURIFixup.h"
#include "nsCDefaultURIFixup.h"
#include "nsIWebNavigation.h"
#include "nsDocShellCID.h"
#include "nsIExternalURLHandlerService.h"
#include "nsIMIMEInfo.h"
#include "mozilla/BrowserElementParent.h"

#include "nsIDOMDocument.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsIDocument.h"
#if defined(XP_MACOSX)
#include "nsThreadUtils.h"
#endif

#include "mozilla/Preferences.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;

// CIDs
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

//*****************************************************************************
//*** nsSiteWindow declaration
//*****************************************************************************

class nsSiteWindow : public nsIEmbeddingSiteWindow
{
public:
  nsSiteWindow(nsContentTreeOwner *aAggregator);
  virtual ~nsSiteWindow();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIEMBEDDINGSITEWINDOW

private:
  nsContentTreeOwner *mAggregator;
};

//*****************************************************************************
//***    nsContentTreeOwner: Object Management
//*****************************************************************************

nsContentTreeOwner::nsContentTreeOwner(bool fPrimary) : mXULWindow(nullptr), 
   mPrimary(fPrimary), mContentTitleSetting(false)
{
  // note if this fails, QI on nsIEmbeddingSiteWindow(2) will simply fail
  mSiteWindow = new nsSiteWindow(this);
}

nsContentTreeOwner::~nsContentTreeOwner()
{
  delete mSiteWindow;
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
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
   NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome3)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIWindowProvider)
   // NOTE: This is using aggregation because there are some properties and
   // method on nsIBaseWindow (which we implement) and on
   // nsIEmbeddingSiteWindow (which we also implement) that have the same name.
   // And it just so happens that we want different behavior for these methods
   // and properties depending on the interface through which they're called
   // (SetFocus() is a good example here).  If it were not for that, we could
   // ditch the aggregation and just deal with not being able to use NS_DECL_*
   // macros for this stuff....
   NS_INTERFACE_MAP_ENTRY_AGGREGATED(nsIEmbeddingSiteWindow, mSiteWindow)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsContentTreeOwner::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsContentTreeOwner::GetInterface(const nsIID& aIID, void** aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);
  *aSink = 0;

  if(aIID.Equals(NS_GET_IID(nsIPrompt))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if(aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIDocShellTreeItem))) {
    NS_ENSURE_STATE(mXULWindow);
    nsCOMPtr<nsIDocShell> shell;
    mXULWindow->GetDocShell(getter_AddRefs(shell));
    if (shell)
      return shell->QueryInterface(aIID, aSink);
    return NS_ERROR_FAILURE;
  }

  if (aIID.Equals(NS_GET_IID(nsIDOMWindow))) {
    NS_ENSURE_STATE(mXULWindow);
    nsCOMPtr<nsIDocShellTreeItem> shell;
    mXULWindow->GetPrimaryContentShell(getter_AddRefs(shell));
    if (shell) {
      nsCOMPtr<nsIInterfaceRequestor> thing(do_QueryInterface(shell));
      if (thing)
        return thing->GetInterface(aIID, aSink);
    }
    return NS_ERROR_FAILURE;
  }

  if (aIID.Equals(NS_GET_IID(nsIXULWindow))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->QueryInterface(aIID, aSink);
  }

  return QueryInterface(aIID, aSink);
}

//*****************************************************************************
// nsContentTreeOwner::nsIDocShellTreeOwner
//*****************************************************************************   

NS_IMETHODIMP nsContentTreeOwner::FindItemWithName(const PRUnichar* aName,
   nsIDocShellTreeItem* aRequestor, nsIDocShellTreeItem* aOriginalRequestor,
   nsIDocShellTreeItem** aFoundItem)
{
   NS_ENSURE_ARG_POINTER(aFoundItem);

   *aFoundItem = nullptr;

   bool fIs_Content = false;

   /* Special Cases */
   if (!aName || !*aName)
      return NS_OK;

   nsDependentString name(aName);

   if (name.LowerCaseEqualsLiteral("_blank"))
      return NS_OK;
   // _main is an IE target which should be case-insensitive but isn't
   // see bug 217886 for details
   if (name.LowerCaseEqualsLiteral("_content") ||
       name.EqualsLiteral("_main")) {
     // If we're being called with an aRequestor and it's targetable, just
     // return it -- _main and _content from inside targetable content shells
     // should just be that content shell.  Note that we don't have to worry
     // about the case when it's not targetable because it's primary -- that
     // will Just Work when we call GetPrimaryContentShell.
     NS_ENSURE_STATE(mXULWindow);
     if (aRequestor) {
       // This better be the root item!
#ifdef DEBUG
       nsCOMPtr<nsIDocShellTreeItem> debugRoot;
       aRequestor->GetSameTypeRootTreeItem(getter_AddRefs(debugRoot));
       NS_ASSERTION(SameCOMIdentity(debugRoot, aRequestor),
                    "Bogus aRequestor");
#endif

       int32_t count = mXULWindow->mTargetableShells.Count();
       for (int32_t i = 0; i < count; ++i) {
         nsCOMPtr<nsIDocShellTreeItem> item =
           do_QueryReferent(mXULWindow->mTargetableShells[i]);
         if (SameCOMIdentity(item, aRequestor)) {
           NS_ADDREF(*aFoundItem = aRequestor);
           return NS_OK;
         }
       }
     }
     mXULWindow->GetPrimaryContentShell(aFoundItem);
     if(*aFoundItem)
       return NS_OK;
     // Fall through and keep looking...
     fIs_Content = true;
   }

   nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
   NS_ENSURE_TRUE(windowMediator, NS_ERROR_FAILURE);

   nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
   NS_ENSURE_SUCCESS(windowMediator->GetXULWindowEnumerator(nullptr, 
      getter_AddRefs(windowEnumerator)), NS_ERROR_FAILURE);
   
   bool more;
   
   windowEnumerator->HasMoreElements(&more);
   while(more) {
     nsCOMPtr<nsISupports> nextWindow = nullptr;
     windowEnumerator->GetNext(getter_AddRefs(nextWindow));
     nsCOMPtr<nsIXULWindow> xulWindow(do_QueryInterface(nextWindow));
     NS_ENSURE_TRUE(xulWindow, NS_ERROR_FAILURE);

     if (fIs_Content) {
       xulWindow->GetPrimaryContentShell(aFoundItem);
     } else {
       // Get all the targetable windows from xulWindow and search them
       nsRefPtr<nsXULWindow> win;
       xulWindow->QueryInterface(NS_GET_IID(nsXULWindow), getter_AddRefs(win));
       if (win) {
         int32_t count = win->mTargetableShells.Count();
         int32_t i;
         for (i = 0; i < count && !*aFoundItem; ++i) {
           nsCOMPtr<nsIDocShellTreeItem> shellAsTreeItem =
             do_QueryReferent(win->mTargetableShells[i]);
           if (shellAsTreeItem) {
             // Get the root tree item of same type, since roots are the only
             // things that call into the treeowner to look for named items.
             // XXXbz ideally we could guarantee that mTargetableShells only
             // contains roots, but the current treeowner apis don't allow
             // that... yet.
             nsCOMPtr<nsIDocShellTreeItem> root;
             shellAsTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(root));
             NS_ASSERTION(root, "Must have root tree item of same type");
             shellAsTreeItem.swap(root);
             if (aRequestor != shellAsTreeItem) {
               // Do this so we can pass in the tree owner as the
               // requestor so the child knows not to call back up.
               nsCOMPtr<nsIDocShellTreeOwner> shellOwner;
               shellAsTreeItem->GetTreeOwner(getter_AddRefs(shellOwner));
               nsCOMPtr<nsISupports> shellOwnerSupports =
                 do_QueryInterface(shellOwner);

               shellAsTreeItem->FindItemWithName(aName, shellOwnerSupports,
                                                 aOriginalRequestor,
                                                 aFoundItem);
             }
           }
         }
       }
     }
     
     if (*aFoundItem)
       return NS_OK;

     windowEnumerator->HasMoreElements(&more);
   }
   return NS_OK;      
}

NS_IMETHODIMP
nsContentTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
                                      bool aPrimary, bool aTargetable,
                                      const nsAString& aID)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->ContentShellAdded(aContentShell, aPrimary, aTargetable,
                                       aID);
}

NS_IMETHODIMP
nsContentTreeOwner::ContentShellRemoved(nsIDocShellTreeItem* aContentShell)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->ContentShellRemoved(aContentShell);
}

NS_IMETHODIMP
nsContentTreeOwner::GetPrimaryContentShell(nsIDocShellTreeItem** aShell)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetPrimaryContentShell(aShell);
}

NS_IMETHODIMP
nsContentTreeOwner::GetContentWindow(JSContext* aCx, JS::Value* aVal)
{
  NS_ENSURE_STATE(mXULWindow);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsContentTreeOwner::SizeShellTo(nsIDocShellTreeItem* aShellItem,
   int32_t aCX, int32_t aCY)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SizeShellTo(aShellItem, aCX, aCY);
}

NS_IMETHODIMP
nsContentTreeOwner::SetPersistence(bool aPersistPosition,
                                   bool aPersistSize,
                                   bool aPersistSizeMode)
{
  NS_ENSURE_STATE(mXULWindow);
  nsCOMPtr<dom::Element> docShellElement = mXULWindow->GetWindowDOMElement();
  if (!docShellElement)
    return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(NS_LITERAL_STRING("persist"), persistString);

  bool saveString = false;
  int32_t index;

  // Set X
  index = persistString.Find("screenX");
  if (!aPersistPosition && index >= 0) {
    persistString.Cut(index, 7);
    saveString = true;
  } else if (aPersistPosition && index < 0) {
    persistString.AppendLiteral(" screenX");
    saveString = true;
  }
  // Set Y
  index = persistString.Find("screenY");
  if (!aPersistPosition && index >= 0) {
    persistString.Cut(index, 7);
    saveString = true;
  } else if (aPersistPosition && index < 0) {
    persistString.AppendLiteral(" screenY");
    saveString = true;
  }
  // Set CX
  index = persistString.Find("width");
  if (!aPersistSize && index >= 0) {
    persistString.Cut(index, 5);
    saveString = true;
  } else if (aPersistSize && index < 0) {
    persistString.AppendLiteral(" width");
    saveString = true;
  }
  // Set CY
  index = persistString.Find("height");
  if (!aPersistSize && index >= 0) {
    persistString.Cut(index, 6);
    saveString = true;
  } else if (aPersistSize && index < 0) {
    persistString.AppendLiteral(" height");
    saveString = true;
  }
  // Set SizeMode
  index = persistString.Find("sizemode");
  if (!aPersistSizeMode && (index >= 0)) {
    persistString.Cut(index, 8);
    saveString = true;
  } else if (aPersistSizeMode && (index < 0)) {
    persistString.AppendLiteral(" sizemode");
    saveString = true;
  }

  ErrorResult rv;
  if(saveString) {
    docShellElement->SetAttribute(NS_LITERAL_STRING("persist"), persistString, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentTreeOwner::GetPersistence(bool* aPersistPosition,
                                   bool* aPersistSize,
                                   bool* aPersistSizeMode)
{
  NS_ENSURE_STATE(mXULWindow);
  nsCOMPtr<dom::Element> docShellElement = mXULWindow->GetWindowDOMElement();
  if (!docShellElement)
    return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(NS_LITERAL_STRING("persist"), persistString);

  // data structure doesn't quite match the question, but it's close enough
  // for what we want (since this method is never actually called...)
  if (aPersistPosition)
    *aPersistPosition = persistString.Find("screenX") >= 0 || persistString.Find("screenY") >= 0 ? true : false;
  if (aPersistSize)
    *aPersistSize = persistString.Find("width") >= 0 || persistString.Find("height") >= 0 ? true : false;
  if (aPersistSizeMode)
    *aPersistSizeMode = persistString.Find("sizemode") >= 0 ? true : false;

  return NS_OK;
}

NS_IMETHODIMP
nsContentTreeOwner::GetTargetableShellCount(uint32_t* aResult)
{
  NS_ENSURE_STATE(mXULWindow);
  *aResult = mXULWindow->mTargetableShells.Count();
  return NS_OK;
}

//*****************************************************************************
// nsContentTreeOwner::nsIWebBrowserChrome3
//*****************************************************************************   

NS_IMETHODIMP nsContentTreeOwner::OnBeforeLinkTraversal(const nsAString &originalTarget,
                                                        nsIURI *linkURI,
                                                        nsIDOMNode *linkNode,
                                                        bool isAppTab,
                                                        nsAString &_retval)
{
  NS_ENSURE_STATE(mXULWindow);

  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow;
  mXULWindow->GetXULBrowserWindow(getter_AddRefs(xulBrowserWindow));

  if (xulBrowserWindow)
    return xulBrowserWindow->OnBeforeLinkTraversal(originalTarget, linkURI,
                                                   linkNode, isAppTab, _retval);
  
  _retval = originalTarget;
  return NS_OK;
}

//*****************************************************************************
// nsContentTreeOwner::nsIWebBrowserChrome2
//*****************************************************************************   

NS_IMETHODIMP nsContentTreeOwner::SetStatusWithContext(uint32_t aStatusType,
                                                       const nsAString &aStatusText,
                                                       nsISupports *aStatusContext)
{
  // We only allow the status to be set from the primary content shell
  if (!mPrimary && aStatusType != STATUS_LINK)
    return NS_OK;

  NS_ENSURE_STATE(mXULWindow);
  
  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow;
  mXULWindow->GetXULBrowserWindow(getter_AddRefs(xulBrowserWindow));

  if (xulBrowserWindow)
  {
    switch(aStatusType)
    {
    case STATUS_SCRIPT:
      xulBrowserWindow->SetJSStatus(aStatusText);
      break;
    case STATUS_LINK:
      {
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aStatusContext);
        xulBrowserWindow->SetOverLink(aStatusText, element);
        break;
      }
    }
  }

  return NS_OK;
}

//*****************************************************************************
// nsContentTreeOwner::nsIWebBrowserChrome
//*****************************************************************************   

NS_IMETHODIMP nsContentTreeOwner::SetStatus(uint32_t aStatusType,
                                            const PRUnichar* aStatus)
{
  return SetStatusWithContext(aStatusType,
      aStatus ? static_cast<const nsString &>(nsDependentString(aStatus))
              : EmptyString(),
      nullptr);
}

NS_IMETHODIMP nsContentTreeOwner::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsContentTreeOwner::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
  // Unimplemented, and probably will remain so; xpfe windows have docshells,
  // not webbrowsers.
  NS_ENSURE_ARG_POINTER(aWebBrowser);
  *aWebBrowser = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsContentTreeOwner::SetChromeFlags(uint32_t aChromeFlags)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetChromeFlags(aChromeFlags);
}

NS_IMETHODIMP nsContentTreeOwner::GetChromeFlags(uint32_t* aChromeFlags)
{
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetChromeFlags(aChromeFlags);
}

NS_IMETHODIMP nsContentTreeOwner::DestroyBrowserWindow()
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsContentTreeOwner::SizeBrowserTo(int32_t aCX, int32_t aCY)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsContentTreeOwner::ShowAsModal()
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->ShowModal();
}

NS_IMETHODIMP nsContentTreeOwner::IsWindowModal(bool *_retval)
{
  NS_ENSURE_STATE(mXULWindow);
  *_retval = mXULWindow->mContinueModalLoop;
  return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::ExitModalEventLoop(nsresult aStatus)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->ExitModalLoop(aStatus);   
}

//*****************************************************************************
// nsContentTreeOwner::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsContentTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, int32_t x, int32_t y, int32_t cx, int32_t cy)   
{
   // Ignore wigdet parents for now.  Don't think those are a vaild thing to call.
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, false), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::Create()
{
   NS_ASSERTION(false, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsContentTreeOwner::Destroy()
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->Destroy();
}

NS_IMETHODIMP nsContentTreeOwner::GetUnscaledDevicePixelsPerCSSPixel(double* aScale)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetUnscaledDevicePixelsPerCSSPixel(aScale);
}

NS_IMETHODIMP nsContentTreeOwner::SetPosition(int32_t aX, int32_t aY)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetPosition(aX, aY);
}

NS_IMETHODIMP nsContentTreeOwner::GetPosition(int32_t* aX, int32_t* aY)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetPosition(aX, aY);
}

NS_IMETHODIMP nsContentTreeOwner::SetSize(int32_t aCX, int32_t aCY, bool aRepaint)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetSize(aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsContentTreeOwner::GetSize(int32_t* aCX, int32_t* aCY)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetSize(aCX, aCY);
}

NS_IMETHODIMP nsContentTreeOwner::SetPositionAndSize(int32_t aX, int32_t aY,
   int32_t aCX, int32_t aCY, bool aRepaint)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetPositionAndSize(aX, aY, aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsContentTreeOwner::GetPositionAndSize(int32_t* aX, int32_t* aY,
   int32_t* aCX, int32_t* aCY)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetPositionAndSize(aX, aY, aCX, aCY); 
}

NS_IMETHODIMP nsContentTreeOwner::Repaint(bool aForce)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->Repaint(aForce);
}

NS_IMETHODIMP nsContentTreeOwner::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetParentWidget(aParentWidget);
}

NS_IMETHODIMP nsContentTreeOwner::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(false, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsContentTreeOwner::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetParentNativeWindow(aParentNativeWindow);
}

NS_IMETHODIMP nsContentTreeOwner::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(false, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsContentTreeOwner::GetNativeHandle(nsAString& aNativeHandle)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetNativeHandle(aNativeHandle);
}

NS_IMETHODIMP nsContentTreeOwner::GetVisibility(bool* aVisibility)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsContentTreeOwner::SetVisibility(bool aVisibility)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsContentTreeOwner::GetEnabled(bool *aEnabled)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->GetEnabled(aEnabled);
}

NS_IMETHODIMP nsContentTreeOwner::SetEnabled(bool aEnable)
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetEnabled(aEnable);
}

NS_IMETHODIMP nsContentTreeOwner::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);
   NS_ENSURE_STATE(mXULWindow);

   *aMainWidget = mXULWindow->mWindow;
   NS_IF_ADDREF(*aMainWidget);

   return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::SetFocus()
{
   NS_ENSURE_STATE(mXULWindow);
   return mXULWindow->SetFocus();
}

NS_IMETHODIMP nsContentTreeOwner::GetTitle(PRUnichar** aTitle)
{
   NS_ENSURE_ARG_POINTER(aTitle);
   NS_ENSURE_STATE(mXULWindow);

   return mXULWindow->GetTitle(aTitle);
}

NS_IMETHODIMP nsContentTreeOwner::SetTitle(const PRUnichar* aTitle)
{
   // We only allow the title to be set from the primary content shell
  if(!mPrimary || !mContentTitleSetting)
    return NS_OK;

  NS_ENSURE_STATE(mXULWindow);
  
  nsAutoString   title;
  nsAutoString   docTitle(aTitle);

  if (docTitle.IsEmpty())
    docTitle.Assign(mTitleDefault);

  if (!docTitle.IsEmpty()) {
    if (!mTitlePreface.IsEmpty()) {
      // Title will be: "Preface: Doc Title - Mozilla"
      title.Assign(mTitlePreface);
      title.Append(docTitle);
    }
    else {
      // Title will be: "Doc Title - Mozilla"
      title = docTitle;
    }

    if (!mWindowTitleModifier.IsEmpty())
      title += mTitleSeparator + mWindowTitleModifier;
  }
  else
    title.Assign(mWindowTitleModifier); // Title will just be plain "Mozilla"

  //
  // if there is no location bar we modify the title to display at least
  // the scheme and host (if any) as an anti-spoofing measure.
  //
  nsCOMPtr<dom::Element> docShellElement = mXULWindow->GetWindowDOMElement();

  if (docShellElement) {
    nsAutoString chromeString;
    docShellElement->GetAttribute(NS_LITERAL_STRING("chromehidden"), chromeString);
    if (chromeString.Find(NS_LITERAL_STRING("location")) != kNotFound) {
      //
      // location bar is turned off, find the browser location
      //
      // use the document's nsPrincipal to find the true owner
      // in case of javascript: or data: documents
      //
      nsCOMPtr<nsIDocShellTreeItem> dsitem;
      GetPrimaryContentShell(getter_AddRefs(dsitem));
      nsCOMPtr<nsIDOMDocument> domdoc(do_GetInterface(dsitem));
      nsCOMPtr<nsIScriptObjectPrincipal> doc(do_QueryInterface(domdoc));
      if (doc) {
        nsCOMPtr<nsIURI> uri;
        nsIPrincipal* principal = doc->GetPrincipal();
        if (principal) {
          principal->GetURI(getter_AddRefs(uri));
          if (uri) {
            //
            // remove any user:pass information
            //
            nsCOMPtr<nsIURIFixup> fixup(do_GetService(NS_URIFIXUP_CONTRACTID));
            if (fixup) {
              nsCOMPtr<nsIURI> tmpuri;
              nsresult rv = fixup->CreateExposableURI(uri,getter_AddRefs(tmpuri));
              if (NS_SUCCEEDED(rv) && tmpuri) {
                // (don't bother if there's no host)
                nsAutoCString host;
                nsAutoCString prepath;
                tmpuri->GetHost(host);
                tmpuri->GetPrePath(prepath);
                if (!host.IsEmpty()) {
                  //
                  // We have a scheme/host, update the title
                  //
                  title.Insert(NS_ConvertUTF8toUTF16(prepath) +
                               mTitleSeparator, 0);
                }
              }
            }
          }
        }
      }
    }
    nsIDocument* document = docShellElement->OwnerDoc();
    ErrorResult rv;
    document->SetTitle(title, rv);
    return rv.ErrorCode();
  }

  return mXULWindow->SetTitle(title.get());
}

//*****************************************************************************
// nsContentTreeOwner: nsIWindowProvider
//*****************************************************************************   
NS_IMETHODIMP
nsContentTreeOwner::ProvideWindow(nsIDOMWindow* aParent,
                                  uint32_t aChromeFlags,
                                  bool aCalledFromJS,
                                  bool aPositionSpecified,
                                  bool aSizeSpecified,
                                  nsIURI* aURI,
                                  const nsAString& aName,
                                  const nsACString& aFeatures,
                                  bool* aWindowIsNew,
                                  nsIDOMWindow** aReturn)
{
  NS_ENSURE_ARG_POINTER(aParent);
  
  *aReturn = nullptr;

  if (!mXULWindow) {
    // Nothing to do here
    return NS_OK;
  }

#ifdef DEBUG
  nsCOMPtr<nsIWebNavigation> parentNav = do_GetInterface(aParent);
  nsCOMPtr<nsIDocShellTreeOwner> parentOwner = do_GetInterface(parentNav);
  NS_ASSERTION(SameCOMIdentity(parentOwner,
                               static_cast<nsIDocShellTreeOwner*>(this)),
               "Parent from wrong docshell tree?");
#endif

  // If aParent is inside an <iframe mozbrowser> and this isn't a request to
  // open a modal-type window, we're going to create a new <iframe mozbrowser>
  // and return its window here.
  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(aParent);
  if (docshell && docshell->GetIsInBrowserOrApp() &&
      !(aChromeFlags & (nsIWebBrowserChrome::CHROME_MODAL |
                        nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                        nsIWebBrowserChrome::CHROME_OPENAS_CHROME))) {

    BrowserElementParent::OpenWindowResult opened =
      BrowserElementParent::OpenWindowInProcess(aParent, aURI, aName,
                                                aFeatures, aReturn);

    // If OpenWindowInProcess handled the open (by opening it or blocking the
    // popup), tell our caller not to proceed trying to create a new window
    // through other means.
    if (opened != BrowserElementParent::OPEN_WINDOW_IGNORED) {
      *aWindowIsNew = opened == BrowserElementParent::OPEN_WINDOW_ADDED;
      return *aWindowIsNew ? NS_OK : NS_ERROR_ABORT;
    }

    // If we're in an app and the target is _blank, send the url to the OS
    if (aName.LowerCaseEqualsLiteral("_blank")) {
      nsCOMPtr<nsIExternalURLHandlerService> exUrlServ(
                        do_GetService(NS_EXTERNALURLHANDLERSERVICE_CONTRACTID));
      if (exUrlServ) {

        nsCOMPtr<nsIHandlerInfo> info;
        bool found;
        exUrlServ->GetURLHandlerInfoFromOS(aURI, &found, getter_AddRefs(info));
  
        if (info && found) {
          info->LaunchWithURI(aURI, nullptr);
          return NS_ERROR_ABORT;
        }

      }
    }
  }

  // the parent window is fullscreen mode or not.
  bool isFullScreen = false;
  if (aParent) {
    aParent->GetFullScreen(&isFullScreen);
  }

  // Where should we open this?
  int32_t containerPref;
  if (NS_FAILED(Preferences::GetInt("browser.link.open_newwindow",
                                    &containerPref))) {
    return NS_OK;
  }

  bool isDisabledOpenNewWindow =
    isFullScreen &&
    Preferences::GetBool("browser.link.open_newwindow.disabled_in_fullscreen");

  if (isDisabledOpenNewWindow && (containerPref == nsIBrowserDOMWindow::OPEN_NEWWINDOW)) {
    containerPref = nsIBrowserDOMWindow::OPEN_NEWTAB;
  }

  if (containerPref != nsIBrowserDOMWindow::OPEN_NEWTAB &&
      containerPref != nsIBrowserDOMWindow::OPEN_CURRENTWINDOW) {
    // Just open a window normally
    return NS_OK;
  }

  if (aCalledFromJS) {
    /* Now check our restriction pref.  The restriction pref is a power-user's
       fine-tuning pref. values:     
       0: no restrictions - divert everything
       1: don't divert window.open at all
       2: don't divert window.open with features
    */
    int32_t restrictionPref =
      Preferences::GetInt("browser.link.open_newwindow.restriction", 2);
    if (restrictionPref < 0 || restrictionPref > 2) {
      restrictionPref = 2; // Sane default behavior
    }

    if (isDisabledOpenNewWindow) {
      // In browser fullscreen, the window should be opened
      // in the current window with no features (see bug 803675)
      restrictionPref = 0;
    }

    if (restrictionPref == 1) {
      return NS_OK;
    }

    if (restrictionPref == 2 &&
        // Only continue if there are no size/position features and no special
        // chrome flags.
        (aChromeFlags != nsIWebBrowserChrome::CHROME_ALL ||
         aPositionSpecified || aSizeSpecified)) {
      return NS_OK;
    }
  }

  nsCOMPtr<nsIDOMWindow> domWin;
  mXULWindow->GetWindowDOMWindow(getter_AddRefs(domWin));
  nsCOMPtr<nsIDOMChromeWindow> chromeWin = do_QueryInterface(domWin);
  if (!chromeWin) {
    // Really odd... but whatever
    NS_WARNING("nsXULWindow's DOMWindow is not a chrome window");
    return NS_OK;
  }
  
  nsCOMPtr<nsIBrowserDOMWindow> browserDOMWin;
  chromeWin->GetBrowserDOMWindow(getter_AddRefs(browserDOMWin));
  if (!browserDOMWin) {
    return NS_OK;
  }

  *aWindowIsNew = (containerPref != nsIBrowserDOMWindow::OPEN_CURRENTWINDOW);

  {
    nsCxPusher pusher;
    pusher.PushNull();

    // Get a new rendering area from the browserDOMWin.  We don't want
    // to be starting any loads here, so get it with a null URI.
    return browserDOMWin->OpenURI(nullptr, aParent, containerPref,
                                  nsIBrowserDOMWindow::OPEN_NEW, aReturn);
  }
}

//*****************************************************************************
// nsContentTreeOwner: Accessors
//*****************************************************************************

#if defined(XP_MACOSX)
class nsContentTitleSettingEvent : public nsRunnable
{
public:
  nsContentTitleSettingEvent(dom::Element* dse, const nsAString& wtm)
    : mElement(dse),
      mTitleDefault(wtm) {}

  NS_IMETHOD Run()
  {
    ErrorResult rv;
    mElement->SetAttribute(NS_LITERAL_STRING("titledefault"), mTitleDefault, rv);
    mElement->RemoveAttribute(NS_LITERAL_STRING("titlemodifier"), rv);
    return NS_OK;
  }

private:
  nsCOMPtr<dom::Element> mElement;
  nsString mTitleDefault;
};
#endif

void nsContentTreeOwner::XULWindow(nsXULWindow* aXULWindow)
{
   mXULWindow = aXULWindow;
   if (mXULWindow && mPrimary) {
      // Get the window title modifiers
      nsCOMPtr<dom::Element> docShellElement = mXULWindow->GetWindowDOMElement();

      nsAutoString   contentTitleSetting;

      if(docShellElement)
         {
         docShellElement->GetAttribute(NS_LITERAL_STRING("contenttitlesetting"), contentTitleSetting);
         if(contentTitleSetting.EqualsLiteral("true"))
            {
            mContentTitleSetting = true;
            docShellElement->GetAttribute(NS_LITERAL_STRING("titledefault"), mTitleDefault);
            docShellElement->GetAttribute(NS_LITERAL_STRING("titlemodifier"), mWindowTitleModifier);
            docShellElement->GetAttribute(NS_LITERAL_STRING("titlepreface"), mTitlePreface);

#if defined(XP_MACOSX)
            // On OS X, treat the titlemodifier like it's the titledefault, and don't ever append
            // the separator + appname.
            if (mTitleDefault.IsEmpty()) {
                NS_DispatchToCurrentThread(
                    new nsContentTitleSettingEvent(docShellElement,
                                                   mWindowTitleModifier));
                mTitleDefault = mWindowTitleModifier;
                mWindowTitleModifier.Truncate();
            }
#endif
            docShellElement->GetAttribute(NS_LITERAL_STRING("titlemenuseparator"), mTitleSeparator);
            }
         }
      else
         {
         NS_ERROR("This condition should never happen.  If it does, "
            "we just won't get a modifier, but it still shouldn't happen.");
         }
      }
}

nsXULWindow* nsContentTreeOwner::XULWindow()
{
   return mXULWindow;
}

//*****************************************************************************
//*** nsSiteWindow implementation
//*****************************************************************************

nsSiteWindow::nsSiteWindow(nsContentTreeOwner *aAggregator)
{
  mAggregator = aAggregator;
}

nsSiteWindow::~nsSiteWindow()
{
}

NS_IMPL_ADDREF_USING_AGGREGATOR(nsSiteWindow, mAggregator)
NS_IMPL_RELEASE_USING_AGGREGATOR(nsSiteWindow, mAggregator)

NS_INTERFACE_MAP_BEGIN(nsSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
NS_INTERFACE_MAP_END_AGGREGATED(mAggregator)

NS_IMETHODIMP
nsSiteWindow::SetDimensions(uint32_t aFlags,
                    int32_t aX, int32_t aY, int32_t aCX, int32_t aCY)
{
  // XXX we're ignoring aFlags
  return mAggregator->SetPositionAndSize(aX, aY, aCX, aCY, true);
}

NS_IMETHODIMP
nsSiteWindow::GetDimensions(uint32_t aFlags,
                    int32_t *aX, int32_t *aY, int32_t *aCX, int32_t *aCY)
{
  // XXX we're ignoring aFlags
  return mAggregator->GetPositionAndSize(aX, aY, aCX, aCY);
}

NS_IMETHODIMP
nsSiteWindow::SetFocus(void)
{
#if 0
  /* This implementation focuses the main document and could make sense.
     However this method is actually being used from within
     nsGlobalWindow::Focus (providing a hook for MDI embedding apps)
     and it's better for our purposes to not pick a document and
     focus it, but allow nsGlobalWindow to carry on unhindered.
  */
  nsXULWindow *window = mAggregator->XULWindow();
  if (window) {
    nsCOMPtr<nsIDocShell> docshell;
    window->GetDocShell(getter_AddRefs(docshell));
    nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(docshell));
    if (domWindow)
      domWindow->Focus();
  }
#endif
  return NS_OK;
}

/* this implementation focuses another window. if there isn't another
   window to focus, we do nothing. */
NS_IMETHODIMP
nsSiteWindow::Blur(void)
{
  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  nsCOMPtr<nsIXULWindow>        xulWindow;
  bool                          more, foundUs;
  nsXULWindow                  *ourWindow = mAggregator->XULWindow();

  {
    nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(kWindowMediatorCID));
    if (windowMediator)
      windowMediator->GetZOrderXULWindowEnumerator(0, true,
                        getter_AddRefs(windowEnumerator));
  }

  if (!windowEnumerator)
    return NS_ERROR_FAILURE;

  // step through the top-level windows
  foundUs = false;
  windowEnumerator->HasMoreElements(&more);
  while (more) {

    nsCOMPtr<nsISupports>  nextWindow;
    nsCOMPtr<nsIXULWindow> nextXULWindow;

    windowEnumerator->GetNext(getter_AddRefs(nextWindow));
    nextXULWindow = do_QueryInterface(nextWindow);

    // got it!(?)
    if (foundUs) {
      xulWindow = nextXULWindow;
      break;
    }

    // remember the very first one, in case we have to wrap
    if (!xulWindow)
      xulWindow = nextXULWindow;

    // look for us
    if (nextXULWindow == ourWindow)
      foundUs = true;

    windowEnumerator->HasMoreElements(&more);
  }

  // change focus to the window we just found
  if (xulWindow) {
    nsCOMPtr<nsIDocShell> docshell;
    xulWindow->GetDocShell(getter_AddRefs(docshell));
    nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(docshell));
    if (domWindow)
      domWindow->Focus();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSiteWindow::GetVisibility(bool *aVisibility)
{
  return mAggregator->GetVisibility(aVisibility);
}

NS_IMETHODIMP
nsSiteWindow::SetVisibility(bool aVisibility)
{
  return mAggregator->SetVisibility(aVisibility);
}

NS_IMETHODIMP
nsSiteWindow::GetTitle(PRUnichar * *aTitle)
{
  return mAggregator->GetTitle(aTitle);
}

NS_IMETHODIMP
nsSiteWindow::SetTitle(const PRUnichar * aTitle)
{
  return mAggregator->SetTitle(aTitle);
}

NS_IMETHODIMP
nsSiteWindow::GetSiteWindow(void **aSiteWindow)
{
  return mAggregator->GetParentNativeWindow(aSiteWindow);
}

