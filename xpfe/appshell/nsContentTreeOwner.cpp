/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=80:
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

// Interfaces needed to be included
#include "nsIDOMWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsIWindowMediator.h"
#include "nsIXULBrowserWindow.h"
#include "nsIPrincipal.h"
#include "nsIURIFixup.h"
#include "nsIWebNavigation.h"
#include "nsDocShellCID.h"
#include "nsIExternalURLHandlerService.h"
#include "nsIMIMEInfo.h"
#include "nsIWidget.h"
#include "nsWindowWatcher.h"
#include "mozilla/BrowserElementParent.h"
#include "mozilla/Components.h"
#include "mozilla/NullPrincipal.h"
#include "nsDocShell.h"
#include "nsDocShellLoadState.h"

#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "mozilla/dom/Document.h"
#if defined(XP_MACOSX)
#  include "nsThreadUtils.h"
#endif

#include "mozilla/Preferences.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h"

using namespace mozilla;

//*****************************************************************************
//*** nsSiteWindow declaration
//*****************************************************************************

class nsSiteWindow : public nsIEmbeddingSiteWindow {
  // nsSiteWindow shares a lifetime with nsContentTreeOwner, and proxies it's
  // AddRef and Release calls to said object.
  // When nsContentTreeOwner is destroyed, nsSiteWindow will be destroyed as
  // well. nsContentTreeOwner is a friend class of nsSiteWindow such that it can
  // call nsSiteWindow's destructor, which is private, as public destructors on
  // reference counted classes are generally unsafe.
  friend class nsContentTreeOwner;

 public:
  explicit nsSiteWindow(nsContentTreeOwner* aAggregator);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIEMBEDDINGSITEWINDOW

 private:
  virtual ~nsSiteWindow();
  nsContentTreeOwner* mAggregator;
};

//*****************************************************************************
//***    nsContentTreeOwner: Object Management
//*****************************************************************************

nsContentTreeOwner::nsContentTreeOwner(bool fPrimary)
    : mXULWindow(nullptr), mPrimary(fPrimary), mContentTitleSetting(false) {
  // note if this fails, QI on nsIEmbeddingSiteWindow(2) will simply fail
  mSiteWindow = new nsSiteWindow(this);
}

nsContentTreeOwner::~nsContentTreeOwner() { delete mSiteWindow; }

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

NS_IMETHODIMP nsContentTreeOwner::GetInterface(const nsIID& aIID,
                                               void** aSink) {
  NS_ENSURE_ARG_POINTER(aSink);
  *aSink = 0;

  if (aIID.Equals(NS_GET_IID(nsIPrompt))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    NS_ENSURE_STATE(mXULWindow);
    return mXULWindow->GetInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIDocShellTreeItem))) {
    NS_ENSURE_STATE(mXULWindow);
    nsCOMPtr<nsIDocShell> shell;
    mXULWindow->GetDocShell(getter_AddRefs(shell));
    if (shell) return shell->QueryInterface(aIID, aSink);
    return NS_ERROR_FAILURE;
  }

  if (aIID.Equals(NS_GET_IID(nsIDOMWindow)) ||
      aIID.Equals(NS_GET_IID(nsPIDOMWindowOuter))) {
    NS_ENSURE_STATE(mXULWindow);
    nsCOMPtr<nsIDocShellTreeItem> shell;
    mXULWindow->GetPrimaryContentShell(getter_AddRefs(shell));
    if (shell) {
      nsCOMPtr<nsIInterfaceRequestor> thing(do_QueryInterface(shell));
      if (thing) return thing->GetInterface(aIID, aSink);
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

NS_IMETHODIMP
nsContentTreeOwner::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
                                      bool aPrimary) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->ContentShellAdded(aContentShell, aPrimary);
}

NS_IMETHODIMP
nsContentTreeOwner::ContentShellRemoved(nsIDocShellTreeItem* aContentShell) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->ContentShellRemoved(aContentShell);
}

NS_IMETHODIMP
nsContentTreeOwner::GetPrimaryContentShell(nsIDocShellTreeItem** aShell) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetPrimaryContentShell(aShell);
}

NS_IMETHODIMP
nsContentTreeOwner::RemoteTabAdded(nsIRemoteTab* aTab, bool aPrimary) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->RemoteTabAdded(aTab, aPrimary);
}

NS_IMETHODIMP
nsContentTreeOwner::RemoteTabRemoved(nsIRemoteTab* aTab) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->RemoteTabRemoved(aTab);
}

NS_IMETHODIMP
nsContentTreeOwner::GetPrimaryRemoteTab(nsIRemoteTab** aTab) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetPrimaryRemoteTab(aTab);
}

NS_IMETHODIMP
nsContentTreeOwner::GetPrimaryContentSize(int32_t* aWidth, int32_t* aHeight) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetPrimaryContentSize(aWidth, aHeight);
}

NS_IMETHODIMP
nsContentTreeOwner::SetPrimaryContentSize(int32_t aWidth, int32_t aHeight) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetPrimaryContentSize(aWidth, aHeight);
}

NS_IMETHODIMP
nsContentTreeOwner::GetRootShellSize(int32_t* aWidth, int32_t* aHeight) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetRootShellSize(aWidth, aHeight);
}

NS_IMETHODIMP
nsContentTreeOwner::SetRootShellSize(int32_t aWidth, int32_t aHeight) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetRootShellSize(aWidth, aHeight);
}

NS_IMETHODIMP nsContentTreeOwner::SizeShellTo(nsIDocShellTreeItem* aShellItem,
                                              int32_t aCX, int32_t aCY) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SizeShellTo(aShellItem, aCX, aCY);
}

NS_IMETHODIMP
nsContentTreeOwner::SetPersistence(bool aPersistPosition, bool aPersistSize,
                                   bool aPersistSizeMode) {
  NS_ENSURE_STATE(mXULWindow);
  nsCOMPtr<dom::Element> docShellElement = mXULWindow->GetWindowDOMElement();
  if (!docShellElement) return NS_ERROR_FAILURE;

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
  if (saveString) {
    docShellElement->SetAttribute(NS_LITERAL_STRING("persist"), persistString,
                                  rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentTreeOwner::GetPersistence(bool* aPersistPosition, bool* aPersistSize,
                                   bool* aPersistSizeMode) {
  NS_ENSURE_STATE(mXULWindow);
  nsCOMPtr<dom::Element> docShellElement = mXULWindow->GetWindowDOMElement();
  if (!docShellElement) return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(NS_LITERAL_STRING("persist"), persistString);

  // data structure doesn't quite match the question, but it's close enough
  // for what we want (since this method is never actually called...)
  if (aPersistPosition)
    *aPersistPosition =
        persistString.Find("screenX") >= 0 || persistString.Find("screenY") >= 0
            ? true
            : false;
  if (aPersistSize)
    *aPersistSize =
        persistString.Find("width") >= 0 || persistString.Find("height") >= 0
            ? true
            : false;
  if (aPersistSizeMode)
    *aPersistSizeMode = persistString.Find("sizemode") >= 0 ? true : false;

  return NS_OK;
}

NS_IMETHODIMP
nsContentTreeOwner::GetTabCount(uint32_t* aResult) {
  if (mXULWindow) {
    return mXULWindow->GetTabCount(aResult);
  }

  *aResult = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsContentTreeOwner::GetHasPrimaryContent(bool* aResult) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetHasPrimaryContent(aResult);
}

//*****************************************************************************
// nsContentTreeOwner::nsIWebBrowserChrome3
//*****************************************************************************

NS_IMETHODIMP nsContentTreeOwner::OnBeforeLinkTraversal(
    const nsAString& originalTarget, nsIURI* linkURI, nsINode* linkNode,
    bool isAppTab, nsAString& _retval) {
  NS_ENSURE_STATE(mXULWindow);

  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow;
  mXULWindow->GetXULBrowserWindow(getter_AddRefs(xulBrowserWindow));

  if (xulBrowserWindow)
    return xulBrowserWindow->OnBeforeLinkTraversal(originalTarget, linkURI,
                                                   linkNode, isAppTab, _retval);

  _retval = originalTarget;
  return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::ShouldLoadURI(
    nsIDocShell* aDocShell, nsIURI* aURI, nsIURI* aReferrer, bool aHasPostData,
    nsIPrincipal* aTriggeringPrincipal, nsIContentSecurityPolicy* aCsp,
    bool* _retval) {
  NS_ENSURE_STATE(mXULWindow);

  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow;
  mXULWindow->GetXULBrowserWindow(getter_AddRefs(xulBrowserWindow));

  if (xulBrowserWindow)
    return xulBrowserWindow->ShouldLoadURI(aDocShell, aURI, aReferrer,
                                           aHasPostData, aTriggeringPrincipal,
                                           aCsp, _retval);

  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::ShouldLoadURIInThisProcess(nsIURI* aURI,
                                                             bool* aRetVal) {
  MOZ_ASSERT_UNREACHABLE("Should only be called in child process.");
  *aRetVal = true;
  return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::ReloadInFreshProcess(
    nsIDocShell* aDocShell, nsIURI* aURI, nsIURI* aReferrer,
    nsIPrincipal* aTriggeringPrincipal, uint32_t aLoadFlags,
    nsIContentSecurityPolicy* aCsp, bool* aRetVal) {
  NS_WARNING("Cannot reload in fresh process from a nsContentTreeOwner!");
  *aRetVal = false;
  return NS_OK;
}

//*****************************************************************************
// nsContentTreeOwner::nsIWebBrowserChrome2
//*****************************************************************************

NS_IMETHODIMP nsContentTreeOwner::SetStatusWithContext(
    uint32_t aStatusType, const nsAString& aStatusText,
    nsISupports* aStatusContext) {
  // We only allow the status to be set from the primary content shell
  if (!mPrimary && aStatusType != STATUS_LINK) return NS_OK;

  NS_ENSURE_STATE(mXULWindow);

  nsCOMPtr<nsIXULBrowserWindow> xulBrowserWindow;
  mXULWindow->GetXULBrowserWindow(getter_AddRefs(xulBrowserWindow));

  if (xulBrowserWindow) {
    switch (aStatusType) {
      case STATUS_LINK: {
        nsCOMPtr<dom::Element> element = do_QueryInterface(aStatusContext);
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
                                            const char16_t* aStatus) {
  return SetStatusWithContext(
      aStatusType,
      aStatus ? static_cast<const nsString&>(nsDependentString(aStatus))
              : EmptyString(),
      nullptr);
}

NS_IMETHODIMP nsContentTreeOwner::SetChromeFlags(uint32_t aChromeFlags) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetChromeFlags(aChromeFlags);
}

NS_IMETHODIMP nsContentTreeOwner::GetChromeFlags(uint32_t* aChromeFlags) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetChromeFlags(aChromeFlags);
}

NS_IMETHODIMP nsContentTreeOwner::ShowAsModal() {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->ShowModal();
}

NS_IMETHODIMP nsContentTreeOwner::IsWindowModal(bool* _retval) {
  NS_ENSURE_STATE(mXULWindow);
  *_retval = mXULWindow->mContinueModalLoop;
  return NS_OK;
}

//*****************************************************************************
// nsContentTreeOwner::nsIBaseWindow
//*****************************************************************************

NS_IMETHODIMP nsContentTreeOwner::InitWindow(nativeWindow aParentNativeWindow,
                                             nsIWidget* parentWidget, int32_t x,
                                             int32_t y, int32_t cx,
                                             int32_t cy) {
  // Ignore wigdet parents for now.  Don't think those are a vaild thing to
  // call.
  NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, 0), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::Create() {
  NS_ASSERTION(false, "You can't call this");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsContentTreeOwner::Destroy() {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->Destroy();
}

NS_IMETHODIMP nsContentTreeOwner::GetUnscaledDevicePixelsPerCSSPixel(
    double* aScale) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetUnscaledDevicePixelsPerCSSPixel(aScale);
}

NS_IMETHODIMP nsContentTreeOwner::GetDevicePixelsPerDesktopPixel(
    double* aScale) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetDevicePixelsPerDesktopPixel(aScale);
}

NS_IMETHODIMP nsContentTreeOwner::SetPositionDesktopPix(int32_t aX,
                                                        int32_t aY) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetPositionDesktopPix(aX, aY);
}

NS_IMETHODIMP nsContentTreeOwner::SetPosition(int32_t aX, int32_t aY) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetPosition(aX, aY);
}

NS_IMETHODIMP nsContentTreeOwner::GetPosition(int32_t* aX, int32_t* aY) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetPosition(aX, aY);
}

NS_IMETHODIMP nsContentTreeOwner::SetSize(int32_t aCX, int32_t aCY,
                                          bool aRepaint) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetSize(aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsContentTreeOwner::GetSize(int32_t* aCX, int32_t* aCY) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetSize(aCX, aCY);
}

NS_IMETHODIMP nsContentTreeOwner::SetPositionAndSize(int32_t aX, int32_t aY,
                                                     int32_t aCX, int32_t aCY,
                                                     uint32_t aFlags) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetPositionAndSize(aX, aY, aCX, aCY, aFlags);
}

NS_IMETHODIMP nsContentTreeOwner::GetPositionAndSize(int32_t* aX, int32_t* aY,
                                                     int32_t* aCX,
                                                     int32_t* aCY) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetPositionAndSize(aX, aY, aCX, aCY);
}

NS_IMETHODIMP nsContentTreeOwner::Repaint(bool aForce) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->Repaint(aForce);
}

NS_IMETHODIMP nsContentTreeOwner::GetParentWidget(nsIWidget** aParentWidget) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetParentWidget(aParentWidget);
}

NS_IMETHODIMP nsContentTreeOwner::SetParentWidget(nsIWidget* aParentWidget) {
  NS_ASSERTION(false, "You can't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsContentTreeOwner::GetParentNativeWindow(
    nativeWindow* aParentNativeWindow) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetParentNativeWindow(aParentNativeWindow);
}

NS_IMETHODIMP nsContentTreeOwner::SetParentNativeWindow(
    nativeWindow aParentNativeWindow) {
  NS_ASSERTION(false, "You can't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsContentTreeOwner::GetNativeHandle(nsAString& aNativeHandle) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetNativeHandle(aNativeHandle);
}

NS_IMETHODIMP nsContentTreeOwner::GetVisibility(bool* aVisibility) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsContentTreeOwner::SetVisibility(bool aVisibility) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsContentTreeOwner::GetEnabled(bool* aEnabled) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->GetEnabled(aEnabled);
}

NS_IMETHODIMP nsContentTreeOwner::SetEnabled(bool aEnable) {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetEnabled(aEnable);
}

NS_IMETHODIMP nsContentTreeOwner::GetMainWidget(nsIWidget** aMainWidget) {
  NS_ENSURE_ARG_POINTER(aMainWidget);
  NS_ENSURE_STATE(mXULWindow);

  *aMainWidget = mXULWindow->mWindow;
  NS_IF_ADDREF(*aMainWidget);

  return NS_OK;
}

NS_IMETHODIMP nsContentTreeOwner::SetFocus() {
  NS_ENSURE_STATE(mXULWindow);
  return mXULWindow->SetFocus();
}

NS_IMETHODIMP nsContentTreeOwner::GetTitle(nsAString& aTitle) {
  NS_ENSURE_STATE(mXULWindow);

  return mXULWindow->GetTitle(aTitle);
}

NS_IMETHODIMP nsContentTreeOwner::SetTitle(const nsAString& aTitle) {
  // We only allow the title to be set from the primary content shell
  if (!mPrimary || !mContentTitleSetting) return NS_OK;

  NS_ENSURE_STATE(mXULWindow);

  nsAutoString title;
  nsAutoString docTitle(aTitle);

  if (docTitle.IsEmpty()) docTitle.Assign(mTitleDefault);

  if (!docTitle.IsEmpty()) {
    if (!mTitlePreface.IsEmpty()) {
      // Title will be: "Preface: Doc Title - Mozilla"
      title.Assign(mTitlePreface);
      title.Append(docTitle);
    } else {
      // Title will be: "Doc Title - Mozilla"
      title = docTitle;
    }

    if (!mWindowTitleModifier.IsEmpty())
      title += mTitleSeparator + mWindowTitleModifier;
  } else
    title.Assign(mWindowTitleModifier);  // Title will just be plain "Mozilla"

  //
  // if there is no location bar we modify the title to display at least
  // the scheme and host (if any) as an anti-spoofing measure.
  //
  nsCOMPtr<dom::Element> docShellElement = mXULWindow->GetWindowDOMElement();

  if (docShellElement) {
    nsAutoString chromeString;
    docShellElement->GetAttribute(NS_LITERAL_STRING("chromehidden"),
                                  chromeString);
    if (chromeString.Find(NS_LITERAL_STRING("location")) != kNotFound) {
      //
      // location bar is turned off, find the browser location
      //
      // use the document's ContentPrincipal to find the true owner
      // in case of javascript: or data: documents
      //
      nsCOMPtr<nsIDocShellTreeItem> dsitem;
      GetPrimaryContentShell(getter_AddRefs(dsitem));
      RefPtr<dom::Document> doc = dsitem ? dsitem->GetDocument() : nullptr;
      if (doc) {
        nsCOMPtr<nsIURI> uri;
        nsIPrincipal* principal = doc->GetPrincipal();
        if (principal) {
          principal->GetURI(getter_AddRefs(uri));
          if (uri) {
            //
            // remove any user:pass information
            //
            nsCOMPtr<nsIURIFixup> fixup(components::URIFixup::Service());
            if (fixup) {
              nsCOMPtr<nsIURI> tmpuri;
              nsresult rv =
                  fixup->CreateExposableURI(uri, getter_AddRefs(tmpuri));
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
                  title.Insert(NS_ConvertUTF8toUTF16(prepath) + mTitleSeparator,
                               0);
                }
              }
            }
          }
        }
      }
    }
    dom::Document* document = docShellElement->OwnerDoc();
    ErrorResult rv;
    document->SetTitle(title, rv);
    return rv.StealNSResult();
  }

  return mXULWindow->SetTitle(title);
}

//*****************************************************************************
// nsContentTreeOwner: nsIWindowProvider
//*****************************************************************************
NS_IMETHODIMP
nsContentTreeOwner::ProvideWindow(
    mozIDOMWindowProxy* aParent, uint32_t aChromeFlags, bool aCalledFromJS,
    bool aPositionSpecified, bool aSizeSpecified, nsIURI* aURI,
    const nsAString& aName, const nsACString& aFeatures, bool aForceNoOpener,
    bool aForceNoReferrer, nsDocShellLoadState* aLoadState, bool* aWindowIsNew,
    mozIDOMWindowProxy** aReturn) {
  NS_ENSURE_ARG_POINTER(aParent);

  auto* parentWin = nsPIDOMWindowOuter::From(aParent);
  dom::BrowsingContext* parent =
      parentWin ? parentWin->GetBrowsingContext() : nullptr;

  *aReturn = nullptr;

  if (!mXULWindow) {
    // Nothing to do here
    return NS_OK;
  }

#ifdef DEBUG
  nsCOMPtr<nsIWebNavigation> parentNav = do_GetInterface(aParent);
  nsCOMPtr<nsIDocShellTreeOwner> parentOwner = do_GetInterface(parentNav);
  NS_ASSERTION(
      SameCOMIdentity(parentOwner, static_cast<nsIDocShellTreeOwner*>(this)),
      "Parent from wrong docshell tree?");
#endif

  // If aParent is inside an <iframe mozbrowser> and this isn't a request to
  // open a modal-type window, we're going to create a new <iframe mozbrowser>
  // and return its window here.
  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(aParent);
  if (docshell && docshell->GetIsInMozBrowser() &&
      !(aChromeFlags & (nsIWebBrowserChrome::CHROME_MODAL |
                        nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                        nsIWebBrowserChrome::CHROME_OPENAS_CHROME))) {
    BrowserElementParent::OpenWindowResult opened =
        BrowserElementParent::OpenWindowInProcess(
            parent, aURI, aName, aFeatures, aForceNoOpener, aReturn);

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

  int32_t openLocation = nsWindowWatcher::GetWindowOpenLocation(
      parentWin, aChromeFlags, aCalledFromJS, aPositionSpecified,
      aSizeSpecified);

  if (openLocation != nsIBrowserDOMWindow::OPEN_NEWTAB &&
      openLocation != nsIBrowserDOMWindow::OPEN_CURRENTWINDOW) {
    // Just open a window normally
    return NS_OK;
  }

  nsCOMPtr<mozIDOMWindowProxy> domWin;
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

  *aWindowIsNew = (openLocation != nsIBrowserDOMWindow::OPEN_CURRENTWINDOW);

  {
    dom::AutoNoJSAPI nojsapi;

    uint32_t flags = nsIBrowserDOMWindow::OPEN_NEW;
    if (aForceNoOpener) {
      flags |= nsIBrowserDOMWindow::OPEN_NO_OPENER;
    }
    if (aForceNoReferrer) {
      flags |= nsIBrowserDOMWindow::OPEN_NO_REFERRER;
    }

    // Get a new rendering area from the browserDOMWin.
    // Since we are not loading any URI, we follow the principle of least
    // privilege and use a nullPrincipal as the triggeringPrincipal.
    //
    // This method handles setting the opener for us, so we don't need to set it
    // ourselves.
    RefPtr<NullPrincipal> nullPrincipal =
        NullPrincipal::CreateWithoutOriginAttributes();
    return browserDOMWin->CreateContentWindow(
        aURI, aParent, openLocation, flags, nullPrincipal, nullptr, aReturn);
  }
}

//*****************************************************************************
// nsContentTreeOwner: Accessors
//*****************************************************************************

#if defined(XP_MACOSX)
class nsContentTitleSettingEvent : public Runnable {
 public:
  nsContentTitleSettingEvent(dom::Element* dse, const nsAString& wtm)
      : Runnable("nsContentTitleSettingEvent"),
        mElement(dse),
        mTitleDefault(wtm) {}

  NS_IMETHOD Run() override {
    ErrorResult rv;
    mElement->SetAttribute(NS_LITERAL_STRING("titledefault"), mTitleDefault,
                           rv);
    mElement->RemoveAttribute(NS_LITERAL_STRING("titlemodifier"), rv);
    return NS_OK;
  }

 private:
  nsCOMPtr<dom::Element> mElement;
  nsString mTitleDefault;
};
#endif

void nsContentTreeOwner::XULWindow(nsXULWindow* aXULWindow) {
  mXULWindow = aXULWindow;
  if (mXULWindow && mPrimary) {
    // Get the window title modifiers
    nsCOMPtr<dom::Element> docShellElement = mXULWindow->GetWindowDOMElement();

    nsAutoString contentTitleSetting;

    if (docShellElement) {
      docShellElement->GetAttribute(NS_LITERAL_STRING("contenttitlesetting"),
                                    contentTitleSetting);
      if (contentTitleSetting.EqualsLiteral("true")) {
        mContentTitleSetting = true;
        docShellElement->GetAttribute(NS_LITERAL_STRING("titledefault"),
                                      mTitleDefault);
        docShellElement->GetAttribute(NS_LITERAL_STRING("titlemodifier"),
                                      mWindowTitleModifier);
        docShellElement->GetAttribute(NS_LITERAL_STRING("titlepreface"),
                                      mTitlePreface);

#if defined(XP_MACOSX)
        // On OS X, treat the titlemodifier like it's the titledefault, and
        // don't ever append the separator + appname.
        if (mTitleDefault.IsEmpty()) {
          NS_DispatchToCurrentThread(new nsContentTitleSettingEvent(
              docShellElement, mWindowTitleModifier));
          mTitleDefault = mWindowTitleModifier;
          mWindowTitleModifier.Truncate();
        }
#endif
        docShellElement->GetAttribute(NS_LITERAL_STRING("titlemenuseparator"),
                                      mTitleSeparator);
      }
    } else {
      NS_ERROR(
          "This condition should never happen.  If it does, "
          "we just won't get a modifier, but it still shouldn't happen.");
    }
  }
}

nsXULWindow* nsContentTreeOwner::XULWindow() { return mXULWindow; }

//*****************************************************************************
//*** nsSiteWindow implementation
//*****************************************************************************

nsSiteWindow::nsSiteWindow(nsContentTreeOwner* aAggregator) {
  mAggregator = aAggregator;
}

nsSiteWindow::~nsSiteWindow() {}

NS_IMPL_ADDREF_USING_AGGREGATOR(nsSiteWindow, mAggregator)
NS_IMPL_RELEASE_USING_AGGREGATOR(nsSiteWindow, mAggregator)

NS_INTERFACE_MAP_BEGIN(nsSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
NS_INTERFACE_MAP_END_AGGREGATED(mAggregator)

NS_IMETHODIMP
nsSiteWindow::SetDimensions(uint32_t aFlags, int32_t aX, int32_t aY,
                            int32_t aCX, int32_t aCY) {
  // XXX we're ignoring aFlags
  return mAggregator->SetPositionAndSize(aX, aY, aCX, aCY,
                                         nsIBaseWindow::eRepaint);
}

NS_IMETHODIMP
nsSiteWindow::GetDimensions(uint32_t aFlags, int32_t* aX, int32_t* aY,
                            int32_t* aCX, int32_t* aCY) {
  // XXX we're ignoring aFlags
  return mAggregator->GetPositionAndSize(aX, aY, aCX, aCY);
}

NS_IMETHODIMP
nsSiteWindow::SetFocus(void) {
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
    if (docShell) {
      nsCOMPtr<nsPIDOMWindowOuter> domWindow(docShell->GetWindow());
      if (domWindow)
        domWindow->Focus();
    }
  }
#endif
  return NS_OK;
}

/* this implementation focuses another window. if there isn't another
   window to focus, we do nothing. */
NS_IMETHODIMP
nsSiteWindow::Blur(void) {
  NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  nsCOMPtr<nsIXULWindow> xulWindow;
  bool more, foundUs;
  nsXULWindow* ourWindow = mAggregator->XULWindow();

  {
    nsCOMPtr<nsIWindowMediator> windowMediator(
        do_GetService(kWindowMediatorCID));
    if (windowMediator)
      windowMediator->GetZOrderXULWindowEnumerator(
          0, true, getter_AddRefs(windowEnumerator));
  }

  if (!windowEnumerator) return NS_ERROR_FAILURE;

  // step through the top-level windows
  foundUs = false;
  windowEnumerator->HasMoreElements(&more);
  while (more) {
    nsCOMPtr<nsISupports> nextWindow;
    nsCOMPtr<nsIXULWindow> nextXULWindow;

    windowEnumerator->GetNext(getter_AddRefs(nextWindow));
    nextXULWindow = do_QueryInterface(nextWindow);

    // got it!(?)
    if (foundUs) {
      xulWindow = nextXULWindow;
      break;
    }

    // remember the very first one, in case we have to wrap
    if (!xulWindow) xulWindow = nextXULWindow;

    // look for us
    if (nextXULWindow == ourWindow) foundUs = true;

    windowEnumerator->HasMoreElements(&more);
  }

  // change focus to the window we just found
  if (xulWindow) {
    nsCOMPtr<nsIDocShell> docshell;
    xulWindow->GetDocShell(getter_AddRefs(docshell));
    if (!docshell) {
      return NS_OK;
    }

    nsCOMPtr<nsPIDOMWindowOuter> domWindow = docshell->GetWindow();
    if (domWindow) domWindow->Focus();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSiteWindow::GetVisibility(bool* aVisibility) {
  return mAggregator->GetVisibility(aVisibility);
}

NS_IMETHODIMP
nsSiteWindow::SetVisibility(bool aVisibility) {
  return mAggregator->SetVisibility(aVisibility);
}

NS_IMETHODIMP
nsSiteWindow::GetTitle(nsAString& aTitle) {
  return mAggregator->GetTitle(aTitle);
}

NS_IMETHODIMP
nsSiteWindow::SetTitle(const nsAString& aTitle) {
  return mAggregator->SetTitle(aTitle);
}

NS_IMETHODIMP
nsSiteWindow::GetSiteWindow(void** aSiteWindow) {
  return mAggregator->GetParentNativeWindow(aSiteWindow);
}
