/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Local Includes
#include "nsWebBrowser.h"

// Helper Classes
#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"

// Interfaces Needed
#include "gfxContext.h"
#include "nsReadableUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWebBrowserChrome.h"
#include "nsPIDOMWindow.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIURI.h"
#include "nsIWebBrowserPersist.h"
#include "nsFocusManager.h"
#include "Layers.h"
#include "nsILoadContext.h"
#include "nsComponentManagerUtils.h"
#include "nsDocShell.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/dom/WindowGlobalChild.h"

// for painting the background window
#include "mozilla/LookAndFeel.h"
#include "mozilla/ServoStyleConsts.h"

// Printing Includes
#ifdef NS_PRINTING
#  include "nsIWebBrowserPrint.h"
#  include "nsIContentViewer.h"
#endif

// PSM2 includes
#include "nsISecureBrowserUI.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

nsWebBrowser::nsWebBrowser(int aItemType)
    : mContentType(aItemType),
      mShouldEnableHistory(true),
      mWillChangeProcess(false),
      mParentNativeWindow(nullptr),
      mProgressListener(nullptr),
      mWidgetListenerDelegate(this),
      mBackgroundColor(0),
      mPersistCurrentState(nsIWebBrowserPersist::PERSIST_STATE_READY),
      mPersistResult(NS_OK),
      mPersistFlags(nsIWebBrowserPersist::PERSIST_FLAGS_NONE),
      mParentWidget(nullptr) {
  mWWatch = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  NS_ASSERTION(mWWatch, "failed to get WindowWatcher");
}

nsWebBrowser::~nsWebBrowser() { InternalDestroy(); }

nsIWidget* nsWebBrowser::EnsureWidget() {
  if (mParentWidget) {
    return mParentWidget;
  }

  mInternalWidget = nsIWidget::CreateChildWindow();
  if (NS_WARN_IF(!mInternalWidget)) {
    return nullptr;
  }

  nsWidgetInitData widgetInit;
  widgetInit.clipChildren = true;
  widgetInit.mWindowType = eWindowType_child;
  LayoutDeviceIntRect bounds(0, 0, 0, 0);

  mInternalWidget->SetWidgetListener(&mWidgetListenerDelegate);
  NS_ENSURE_SUCCESS(mInternalWidget->Create(nullptr, mParentNativeWindow,
                                            bounds, &widgetInit),
                    nullptr);

  return mInternalWidget;
}

/* static */
already_AddRefed<nsWebBrowser> nsWebBrowser::Create(
    nsIWebBrowserChrome* aContainerWindow, nsIWidget* aParentWidget,
    dom::BrowsingContext* aBrowsingContext,
    dom::WindowGlobalChild* aInitialWindowChild) {
  MOZ_ASSERT_IF(aInitialWindowChild,
                aInitialWindowChild->BrowsingContext() == aBrowsingContext);

  RefPtr<nsWebBrowser> browser = new nsWebBrowser(
      aBrowsingContext->IsContent() ? typeContentWrapper : typeChromeWrapper);

  // nsWebBrowser::SetContainer also calls nsWebBrowser::EnsureDocShellTreeOwner
  NS_ENSURE_SUCCESS(browser->SetContainerWindow(aContainerWindow), nullptr);
  NS_ENSURE_SUCCESS(browser->SetParentWidget(aParentWidget), nullptr);

  nsCOMPtr<nsIWidget> docShellParentWidget = browser->EnsureWidget();
  if (NS_WARN_IF(!docShellParentWidget)) {
    return nullptr;
  }

  uint64_t outerWindowId =
      aInitialWindowChild ? aInitialWindowChild->OuterWindowId() : 0;

  RefPtr<nsDocShell> docShell =
      nsDocShell::Create(aBrowsingContext, outerWindowId);
  if (NS_WARN_IF(!docShell)) {
    return nullptr;
  }
  browser->SetDocShell(docShell);
  MOZ_ASSERT(browser->mDocShell == docShell);

  // get the system default window background colour
  //
  // TODO(emilio): Can we get the color-scheme from somewhere here?
  browser->mBackgroundColor = LookAndFeel::Color(
      LookAndFeel::ColorID::WindowBackground, LookAndFeel::ColorScheme::Light,
      LookAndFeel::UseStandins::No);

  // HACK ALERT - this registration registers the nsDocShellTreeOwner as a
  // nsIWebBrowserListener so it can setup its MouseListener in one of the
  // progress callbacks. If we can register the MouseListener another way, this
  // registration can go away, and nsDocShellTreeOwner can stop implementing
  // nsIWebProgressListener.
  RefPtr<nsDocShellTreeOwner> docShellTreeOwner = browser->mDocShellTreeOwner;
  Unused << docShell->AddProgressListener(docShellTreeOwner,
                                          nsIWebProgress::NOTIFY_ALL);

  docShell->SetTreeOwner(docShellTreeOwner);

  // If the webbrowser is a content docshell item then we won't hear any
  // events from subframes. To solve that we install our own chrome event
  // handler that always gets called (even for subframes) for any bubbling
  // event.

  nsresult rv = docShell->InitWindow(nullptr, docShellParentWidget, 0, 0, 0, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  docShellTreeOwner->AddToWatcher();  // evil twin of Remove in SetDocShell(0)
  docShellTreeOwner->AddChromeListeners();

  if (aInitialWindowChild) {
    docShell->CreateContentViewerForActor(aInitialWindowChild);
  }

  return browser.forget();
}

void nsWebBrowser::InternalDestroy() {
  if (mInternalWidget) {
    mInternalWidget->SetWidgetListener(nullptr);
    mInternalWidget->Destroy();
    mInternalWidget = nullptr;  // Force release here.
  }

  SetDocShell(nullptr);

  if (mDocShellTreeOwner) {
    mDocShellTreeOwner->WebBrowser(nullptr);
    mDocShellTreeOwner = nullptr;
  }
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsWebBrowser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsWebBrowser)

NS_IMPL_CYCLE_COLLECTION_WEAK(nsWebBrowser, mDocShell)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsWebBrowser)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowser)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowser)
  NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
  NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
  NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPersist)
  NS_INTERFACE_MAP_ENTRY(nsICancelable)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

///*****************************************************************************
// nsWebBrowser::nsIInterfaceRequestor
//*****************************************************************************

NS_IMETHODIMP
nsWebBrowser::GetInterface(const nsIID& aIID, void** aSink) {
  NS_ENSURE_ARG_POINTER(aSink);

  if (NS_SUCCEEDED(QueryInterface(aIID, aSink))) {
    return NS_OK;
  }

  if (mDocShell) {
#ifdef NS_PRINTING
    if (aIID.Equals(NS_GET_IID(nsIWebBrowserPrint))) {
      nsCOMPtr<nsIContentViewer> viewer;
      mDocShell->GetContentViewer(getter_AddRefs(viewer));
      if (!viewer) {
        return NS_NOINTERFACE;
      }

      nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint(do_QueryInterface(viewer));
      nsIWebBrowserPrint* print = (nsIWebBrowserPrint*)webBrowserPrint.get();
      NS_ASSERTION(print, "This MUST support this interface!");
      NS_ADDREF(print);
      *aSink = print;
      return NS_OK;
    }
#endif
    return mDocShell->GetInterface(aIID, aSink);
  }

  return NS_NOINTERFACE;
}

//*****************************************************************************
// nsWebBrowser::nsIWebBrowser
//*****************************************************************************

NS_IMETHODIMP
nsWebBrowser::GetContainerWindow(nsIWebBrowserChrome** aTopWindow) {
  NS_ENSURE_ARG_POINTER(aTopWindow);

  nsCOMPtr<nsIWebBrowserChrome> top;
  if (mDocShellTreeOwner) {
    top = mDocShellTreeOwner->GetWebBrowserChrome();
  }

  top.forget(aTopWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::SetContainerWindow(nsIWebBrowserChrome* aTopWindow) {
  EnsureDocShellTreeOwner();
  return mDocShellTreeOwner->SetWebBrowserChrome(aTopWindow);
}

NS_IMETHODIMP
nsWebBrowser::GetContentDOMWindow(mozIDOMWindowProxy** aResult) {
  if (!mDocShell) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsPIDOMWindowOuter> retval = mDocShell->GetWindow();
  retval.forget(aResult);
  return *aResult ? NS_OK : NS_ERROR_FAILURE;
}

void nsWebBrowser::SetOriginAttributes(const OriginAttributes& aAttrs) {
  mOriginAttributes = aAttrs;
}

//*****************************************************************************
// nsWebBrowser::nsIDocShellTreeItem
//*****************************************************************************

NS_IMETHODIMP
nsWebBrowser::GetName(nsAString& aName) {
  if (mDocShell) {
    mDocShell->GetName(aName);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::SetName(const nsAString& aName) {
  if (mDocShell) {
    return mDocShell->SetName(aName);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::NameEquals(const nsAString& aName, bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  if (mDocShell) {
    return mDocShell->NameEquals(aName, aResult);
  }

  return NS_OK;
}

/* virtual */
int32_t nsWebBrowser::ItemType() { return mContentType; }

NS_IMETHODIMP
nsWebBrowser::GetItemType(int32_t* aItemType) {
  NS_ENSURE_ARG_POINTER(aItemType);

  *aItemType = ItemType();
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetInProcessParent(nsIDocShellTreeItem** aParent) {
  *aParent = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetInProcessSameTypeParent(nsIDocShellTreeItem** aParent) {
  *aParent = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetInProcessRootTreeItem(nsIDocShellTreeItem** aRootTreeItem) {
  NS_ENSURE_ARG_POINTER(aRootTreeItem);
  *aRootTreeItem = static_cast<nsIDocShellTreeItem*>(this);

  nsCOMPtr<nsIDocShellTreeItem> parent;
  NS_ENSURE_SUCCESS(GetInProcessParent(getter_AddRefs(parent)),
                    NS_ERROR_FAILURE);
  while (parent) {
    *aRootTreeItem = parent;
    NS_ENSURE_SUCCESS(
        (*aRootTreeItem)->GetInProcessParent(getter_AddRefs(parent)),
        NS_ERROR_FAILURE);
  }
  NS_ADDREF(*aRootTreeItem);
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetInProcessSameTypeRootTreeItem(
    nsIDocShellTreeItem** aRootTreeItem) {
  NS_ENSURE_ARG_POINTER(aRootTreeItem);
  *aRootTreeItem = static_cast<nsIDocShellTreeItem*>(this);

  nsCOMPtr<nsIDocShellTreeItem> parent;
  NS_ENSURE_SUCCESS(GetInProcessSameTypeParent(getter_AddRefs(parent)),
                    NS_ERROR_FAILURE);
  while (parent) {
    *aRootTreeItem = parent;
    NS_ENSURE_SUCCESS(
        (*aRootTreeItem)->GetInProcessSameTypeParent(getter_AddRefs(parent)),
        NS_ERROR_FAILURE);
  }
  NS_ADDREF(*aRootTreeItem);
  return NS_OK;
}

dom::Document* nsWebBrowser::GetDocument() {
  return mDocShell ? mDocShell->GetDocument() : nullptr;
}

nsPIDOMWindowOuter* nsWebBrowser::GetWindow() {
  return mDocShell ? mDocShell->GetWindow() : nullptr;
}

NS_IMETHODIMP
nsWebBrowser::GetBrowsingContextXPCOM(dom::BrowsingContext** aBrowsingContext) {
  NS_ENSURE_STATE(mDocShell);
  return mDocShell->GetBrowsingContextXPCOM(aBrowsingContext);
}

dom::BrowsingContext* nsWebBrowser::GetBrowsingContext() {
  return mDocShell->GetBrowsingContext();
}

NS_IMETHODIMP
nsWebBrowser::GetDomWindow(mozIDOMWindowProxy** aWindow) {
  if (!mDocShell) return NS_ERROR_NOT_INITIALIZED;
  return mDocShell->GetDomWindow(aWindow);
}

NS_IMETHODIMP
nsWebBrowser::GetTreeOwner(nsIDocShellTreeOwner** aTreeOwner) {
  NS_ENSURE_ARG_POINTER(aTreeOwner);
  *aTreeOwner = nullptr;
  if (mDocShellTreeOwner) {
    if (mDocShellTreeOwner->mTreeOwner) {
      *aTreeOwner = mDocShellTreeOwner->mTreeOwner;
    } else {
      *aTreeOwner = mDocShellTreeOwner;
    }
  }
  NS_IF_ADDREF(*aTreeOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::SetTreeOwner(nsIDocShellTreeOwner* aTreeOwner) {
  EnsureDocShellTreeOwner();
  return mDocShellTreeOwner->SetTreeOwner(aTreeOwner);
}

//*****************************************************************************
// nsWebBrowser::nsIDocShellTreeItem
//*****************************************************************************

NS_IMETHODIMP
nsWebBrowser::GetInProcessChildCount(int32_t* aChildCount) {
  NS_ENSURE_ARG_POINTER(aChildCount);
  *aChildCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::AddChild(nsIDocShellTreeItem* aChild) {
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsWebBrowser::RemoveChild(nsIDocShellTreeItem* aChild) {
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsWebBrowser::GetInProcessChildAt(int32_t aIndex,
                                  nsIDocShellTreeItem** aChild) {
  return NS_ERROR_UNEXPECTED;
}

//*****************************************************************************
// nsWebBrowser::nsIWebNavigation
//*****************************************************************************

NS_IMETHODIMP
nsWebBrowser::GetCanGoBack(bool* aCanGoBack) {
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->GetCanGoBack(aCanGoBack);
}

NS_IMETHODIMP
nsWebBrowser::GetCanGoForward(bool* aCanGoForward) {
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->GetCanGoForward(aCanGoForward);
}

NS_IMETHODIMP
nsWebBrowser::GoBack(bool aRequireUserInteraction, bool aUserActivation) {
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->GoBack(aRequireUserInteraction, aUserActivation);
}

NS_IMETHODIMP
nsWebBrowser::GoForward(bool aRequireUserInteraction, bool aUserActivation) {
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->GoForward(aRequireUserInteraction, aUserActivation);
}

nsresult nsWebBrowser::LoadURI(const nsAString& aURI,
                               const dom::LoadURIOptions& aLoadURIOptions) {
#ifndef ANDROID
  MOZ_ASSERT(aLoadURIOptions.mTriggeringPrincipal,
             "nsWebBrowser::LoadURI - Need a valid triggeringPrincipal");
#endif
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->LoadURI(aURI, aLoadURIOptions);
}

NS_IMETHODIMP
nsWebBrowser::LoadURIFromScript(const nsAString& aURI,
                                JS::Handle<JS::Value> aLoadURIOptions,
                                JSContext* aCx) {
  // generate dictionary for loadURIOptions and forward call
  dom::LoadURIOptions loadURIOptions;
  if (!loadURIOptions.Init(aCx, aLoadURIOptions)) {
    return NS_ERROR_INVALID_ARG;
  }
  return LoadURI(aURI, loadURIOptions);
}

NS_IMETHODIMP
nsWebBrowser::ResumeRedirectedLoad(uint64_t aIdentifier,
                                   int32_t aHistoryIndex) {
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->ResumeRedirectedLoad(aIdentifier, aHistoryIndex);
}

NS_IMETHODIMP
nsWebBrowser::Reload(uint32_t aReloadFlags) {
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->Reload(aReloadFlags);
}

NS_IMETHODIMP
nsWebBrowser::GotoIndex(int32_t aIndex, bool aUserActivation) {
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->GotoIndex(aIndex, aUserActivation);
}

NS_IMETHODIMP
nsWebBrowser::Stop(uint32_t aStopFlags) {
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->Stop(aStopFlags);
}

NS_IMETHODIMP
nsWebBrowser::GetCurrentURI(nsIURI** aURI) {
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->GetCurrentURI(aURI);
}

// XXX(nika): Consider making the mozilla::dom::ChildSHistory version the
// canonical one?
NS_IMETHODIMP
nsWebBrowser::GetSessionHistoryXPCOM(nsISupports** aSessionHistory) {
  NS_ENSURE_ARG_POINTER(aSessionHistory);
  *aSessionHistory = nullptr;
  if (mDocShell) {
    return mDocShell->GetSessionHistoryXPCOM(aSessionHistory);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetDocument(dom::Document** aDocument) {
  NS_ENSURE_STATE(mDocShell);

  return mDocShell->GetDocument(aDocument);
}

void nsWebBrowser::SetAllowDNSPrefetch(bool aAllowPrefetch) {
  MOZ_ASSERT(mDocShell);
  mDocShell->SetAllowDNSPrefetch(aAllowPrefetch);
}

//*****************************************************************************
// nsWebBrowser::nsIWebProgressListener
//*****************************************************************************

NS_IMETHODIMP
nsWebBrowser::OnStateChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                            uint32_t aStateFlags, nsresult aStatus) {
  if (mPersist) {
    mPersist->GetCurrentState(&mPersistCurrentState);
  }
  if (aStateFlags & STATE_IS_NETWORK && aStateFlags & STATE_STOP) {
    mPersist = nullptr;
  }
  if (mProgressListener) {
    return mProgressListener->OnStateChange(aWebProgress, aRequest, aStateFlags,
                                            aStatus);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::OnProgressChange(nsIWebProgress* aWebProgress,
                               nsIRequest* aRequest, int32_t aCurSelfProgress,
                               int32_t aMaxSelfProgress,
                               int32_t aCurTotalProgress,
                               int32_t aMaxTotalProgress) {
  if (mPersist) {
    mPersist->GetCurrentState(&mPersistCurrentState);
  }
  if (mProgressListener) {
    return mProgressListener->OnProgressChange(
        aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
        aCurTotalProgress, aMaxTotalProgress);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::OnLocationChange(nsIWebProgress* aWebProgress,
                               nsIRequest* aRequest, nsIURI* aLocation,
                               uint32_t aFlags) {
  if (mProgressListener) {
    return mProgressListener->OnLocationChange(aWebProgress, aRequest,
                                               aLocation, aFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                             nsresult aStatus, const char16_t* aMessage) {
  if (mProgressListener) {
    return mProgressListener->OnStatusChange(aWebProgress, aRequest, aStatus,
                                             aMessage);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::OnSecurityChange(nsIWebProgress* aWebProgress,
                               nsIRequest* aRequest, uint32_t aState) {
  if (mProgressListener) {
    return mProgressListener->OnSecurityChange(aWebProgress, aRequest, aState);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                     nsIRequest* aRequest, uint32_t aEvent) {
  if (mProgressListener) {
    return mProgressListener->OnContentBlockingEvent(aWebProgress, aRequest,
                                                     aEvent);
  }
  return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIWebBrowserPersist
//*****************************************************************************

NS_IMETHODIMP
nsWebBrowser::GetPersistFlags(uint32_t* aPersistFlags) {
  NS_ENSURE_ARG_POINTER(aPersistFlags);
  nsresult rv = NS_OK;
  if (mPersist) {
    rv = mPersist->GetPersistFlags(&mPersistFlags);
  }
  *aPersistFlags = mPersistFlags;
  return rv;
}

NS_IMETHODIMP
nsWebBrowser::SetPersistFlags(uint32_t aPersistFlags) {
  nsresult rv = NS_OK;
  mPersistFlags = aPersistFlags;
  if (mPersist) {
    rv = mPersist->SetPersistFlags(mPersistFlags);
    mPersist->GetPersistFlags(&mPersistFlags);
  }
  return rv;
}

NS_IMETHODIMP
nsWebBrowser::GetCurrentState(uint32_t* aCurrentState) {
  NS_ENSURE_ARG_POINTER(aCurrentState);
  if (mPersist) {
    mPersist->GetCurrentState(&mPersistCurrentState);
  }
  *aCurrentState = mPersistCurrentState;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetResult(nsresult* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  if (mPersist) {
    mPersist->GetResult(&mPersistResult);
  }
  *aResult = mPersistResult;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetProgressListener(nsIWebProgressListener** aProgressListener) {
  NS_ENSURE_ARG_POINTER(aProgressListener);
  *aProgressListener = mProgressListener;
  NS_IF_ADDREF(*aProgressListener);
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::SetProgressListener(nsIWebProgressListener* aProgressListener) {
  mProgressListener = aProgressListener;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::SaveURI(nsIURI* aURI, nsIPrincipal* aPrincipal,
                      uint32_t aCacheKey, nsIReferrerInfo* aReferrerInfo,
                      nsICookieJarSettings* aCookieJarSettings,
                      nsIInputStream* aPostData, const char* aExtraHeaders,
                      nsISupports* aFile,
                      nsContentPolicyType aContentPolicyType,
                      nsILoadContext* aPrivacyContext) {
  return SavePrivacyAwareURI(
      aURI, aPrincipal, aCacheKey, aReferrerInfo, aCookieJarSettings, aPostData,
      aExtraHeaders, aFile, aContentPolicyType,
      aPrivacyContext && aPrivacyContext->UsePrivateBrowsing());
}

NS_IMETHODIMP
nsWebBrowser::SavePrivacyAwareURI(
    nsIURI* aURI, nsIPrincipal* aPrincipal, uint32_t aCacheKey,
    nsIReferrerInfo* aReferrerInfo, nsICookieJarSettings* aCookieJarSettings,
    nsIInputStream* aPostData, const char* aExtraHeaders, nsISupports* aFile,
    nsContentPolicyType aContentPolicyType, bool aIsPrivate) {
  if (mPersist) {
    uint32_t currentState;
    mPersist->GetCurrentState(&currentState);
    if (currentState == PERSIST_STATE_FINISHED) {
      mPersist = nullptr;
    } else {
      // You can't save again until the last save has completed
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIURI> uri;
  if (aURI) {
    uri = aURI;
  } else {
    nsresult rv = GetCurrentURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Create a throwaway persistence object to do the work
  nsresult rv;
  mPersist = do_CreateInstance(NS_WEBBROWSERPERSIST_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mPersist->SetProgressListener(this);
  mPersist->SetPersistFlags(mPersistFlags);
  mPersist->GetCurrentState(&mPersistCurrentState);

  rv = mPersist->SavePrivacyAwareURI(
      uri, aPrincipal, aCacheKey, aReferrerInfo, aCookieJarSettings, aPostData,
      aExtraHeaders, aFile, aContentPolicyType, aIsPrivate);
  if (NS_FAILED(rv)) {
    mPersist = nullptr;
  }
  return rv;
}

NS_IMETHODIMP
nsWebBrowser::SaveChannel(nsIChannel* aChannel, nsISupports* aFile) {
  if (mPersist) {
    uint32_t currentState;
    mPersist->GetCurrentState(&currentState);
    if (currentState == PERSIST_STATE_FINISHED) {
      mPersist = nullptr;
    } else {
      // You can't save again until the last save has completed
      return NS_ERROR_FAILURE;
    }
  }

  // Create a throwaway persistence object to do the work
  nsresult rv;
  mPersist = do_CreateInstance(NS_WEBBROWSERPERSIST_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mPersist->SetProgressListener(this);
  mPersist->SetPersistFlags(mPersistFlags);
  mPersist->GetCurrentState(&mPersistCurrentState);
  rv = mPersist->SaveChannel(aChannel, aFile);
  if (NS_FAILED(rv)) {
    mPersist = nullptr;
  }
  return rv;
}

NS_IMETHODIMP
nsWebBrowser::SaveDocument(nsISupports* aDocumentish, nsISupports* aFile,
                           nsISupports* aDataPath,
                           const char* aOutputContentType,
                           uint32_t aEncodingFlags, uint32_t aWrapColumn) {
  if (mPersist) {
    uint32_t currentState;
    mPersist->GetCurrentState(&currentState);
    if (currentState == PERSIST_STATE_FINISHED) {
      mPersist = nullptr;
    } else {
      // You can't save again until the last save has completed
      return NS_ERROR_FAILURE;
    }
  }

  // Use the specified DOM document, or if none is specified, the one
  // attached to the web browser.

  nsCOMPtr<nsISupports> doc;
  if (aDocumentish) {
    doc = aDocumentish;
  } else {
    RefPtr<dom::Document> domDoc;
    GetDocument(getter_AddRefs(domDoc));
    doc = already_AddRefed<nsISupports>(ToSupports(domDoc.forget().take()));
  }
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  // Create a throwaway persistence object to do the work
  nsresult rv;
  mPersist = do_CreateInstance(NS_WEBBROWSERPERSIST_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mPersist->SetProgressListener(this);
  mPersist->SetPersistFlags(mPersistFlags);
  mPersist->GetCurrentState(&mPersistCurrentState);
  rv = mPersist->SaveDocument(doc, aFile, aDataPath, aOutputContentType,
                              aEncodingFlags, aWrapColumn);
  if (NS_FAILED(rv)) {
    mPersist = nullptr;
  }
  return rv;
}

NS_IMETHODIMP
nsWebBrowser::CancelSave() {
  if (mPersist) {
    return mPersist->CancelSave();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::Cancel(nsresult aReason) {
  if (mPersist) {
    return mPersist->Cancel(aReason);
  }
  return NS_OK;
}

//*****************************************************************************
// nsWebBrowser::nsIBaseWindow
//*****************************************************************************

NS_IMETHODIMP
nsWebBrowser::InitWindow(nativeWindow aParentNativeWindow,
                         nsIWidget* aParentWidget, int32_t aX, int32_t aY,
                         int32_t aCX, int32_t aCY) {
  // nsIBaseWindow::InitWindow and nsIBaseWindow::Create
  // implementations have been merged into nsWebBrowser::Create
  MOZ_DIAGNOSTIC_ASSERT(false);
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsWebBrowser::Destroy() {
  InternalDestroy();

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetUnscaledDevicePixelsPerCSSPixel(double* aScale) {
  *aScale = mParentWidget ? mParentWidget->GetDefaultScale().scale : 1.0;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetDevicePixelsPerDesktopPixel(double* aScale) {
  *aScale =
      mParentWidget ? mParentWidget->GetDesktopToDeviceScale().scale : 1.0;
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::SetPositionDesktopPix(int32_t aX, int32_t aY) {
  // XXX jfkthame
  // It's not clear to me whether this will be fully correct across
  // potential multi-screen, mixed-DPI configurations for all platforms;
  // we might need to add code paths that make it possible to pass the
  // desktop-pix parameters all the way through to the native widget,
  // to avoid the risk of device-pixel coords mapping to the wrong
  // display on OS X with mixed retina/non-retina screens.
  double scale = 1.0;
  GetDevicePixelsPerDesktopPixel(&scale);
  return SetPosition(NSToIntRound(aX * scale), NSToIntRound(aY * scale));
}

NS_IMETHODIMP
nsWebBrowser::SetPosition(int32_t aX, int32_t aY) {
  int32_t cx = 0;
  int32_t cy = 0;

  GetSize(&cx, &cy);

  return SetPositionAndSize(aX, aY, cx, cy, 0);
}

NS_IMETHODIMP
nsWebBrowser::GetPosition(int32_t* aX, int32_t* aY) {
  return GetPositionAndSize(aX, aY, nullptr, nullptr);
}

NS_IMETHODIMP
nsWebBrowser::SetSize(int32_t aCX, int32_t aCY, bool aRepaint) {
  int32_t x = 0;
  int32_t y = 0;

  GetPosition(&x, &y);

  return SetPositionAndSize(x, y, aCX, aCY,
                            aRepaint ? nsIBaseWindow::eRepaint : 0);
}

NS_IMETHODIMP
nsWebBrowser::GetSize(int32_t* aCX, int32_t* aCY) {
  return GetPositionAndSize(nullptr, nullptr, aCX, aCY);
}

NS_IMETHODIMP
nsWebBrowser::SetPositionAndSize(int32_t aX, int32_t aY, int32_t aCX,
                                 int32_t aCY, uint32_t aFlags) {
  int32_t doc_x = aX;
  int32_t doc_y = aY;

  // If there is an internal widget we need to make the docShell coordinates
  // relative to the internal widget rather than the calling app's parent.
  // We also need to resize our widget then.
  if (mInternalWidget) {
    doc_x = doc_y = 0;
    mInternalWidget->Resize(aX, aY, aCX, aCY,
                            !!(aFlags & nsIBaseWindow::eRepaint));
  }
  // Now reposition/ resize the doc
  NS_ENSURE_SUCCESS(
      mDocShell->SetPositionAndSize(doc_x, doc_y, aCX, aCY, aFlags),
      NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetPositionAndSize(int32_t* aX, int32_t* aY, int32_t* aCX,
                                 int32_t* aCY) {
  if (mInternalWidget) {
    LayoutDeviceIntRect bounds = mInternalWidget->GetBounds();

    if (aX) {
      *aX = bounds.X();
    }
    if (aY) {
      *aY = bounds.Y();
    }
    if (aCX) {
      *aCX = bounds.Width();
    }
    if (aCY) {
      *aCY = bounds.Height();
    }
    return NS_OK;
  } else {
    // Can directly return this as it is the
    // same interface, thus same returns.
    return mDocShell->GetPositionAndSize(aX, aY, aCX, aCY);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::Repaint(bool aForce) {
  NS_ENSURE_STATE(mDocShell);
  // Can directly return this as it is the
  // same interface, thus same returns.
  return mDocShell->Repaint(aForce);
}

NS_IMETHODIMP
nsWebBrowser::GetParentWidget(nsIWidget** aParentWidget) {
  NS_ENSURE_ARG_POINTER(aParentWidget);

  *aParentWidget = mParentWidget;

  NS_IF_ADDREF(*aParentWidget);

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::SetParentWidget(nsIWidget* aParentWidget) {
  NS_ENSURE_STATE(!mDocShell);

  mParentWidget = aParentWidget;
  if (mParentWidget) {
    mParentNativeWindow = mParentWidget->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    mParentNativeWindow = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetParentNativeWindow(nativeWindow* aParentNativeWindow) {
  NS_ENSURE_ARG_POINTER(aParentNativeWindow);

  *aParentNativeWindow = mParentNativeWindow;

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::SetParentNativeWindow(nativeWindow aParentNativeWindow) {
  NS_ENSURE_STATE(!mDocShell);

  mParentNativeWindow = aParentNativeWindow;

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetNativeHandle(nsAString& aNativeHandle) {
  // the nativeHandle should be accessed from nsIAppWindow
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebBrowser::GetVisibility(bool* aVisibility) {
  NS_ENSURE_ARG_POINTER(aVisibility);

  if (mDocShell) {
    NS_ENSURE_SUCCESS(mDocShell->GetVisibility(aVisibility), NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::SetVisibility(bool aVisibility) {
  if (mDocShell) {
    NS_ENSURE_SUCCESS(mDocShell->SetVisibility(aVisibility), NS_ERROR_FAILURE);
    if (mInternalWidget) {
      mInternalWidget->Show(aVisibility);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetEnabled(bool* aEnabled) {
  if (mInternalWidget) {
    *aEnabled = mInternalWidget->IsEnabled();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebBrowser::SetEnabled(bool aEnabled) {
  if (mInternalWidget) {
    mInternalWidget->Enable(aEnabled);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebBrowser::GetMainWidget(nsIWidget** aMainWidget) {
  NS_ENSURE_ARG_POINTER(aMainWidget);

  if (mInternalWidget) {
    *aMainWidget = mInternalWidget;
  } else {
    *aMainWidget = mParentWidget;
  }

  NS_IF_ADDREF(*aMainWidget);

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::GetTitle(nsAString& aTitle) {
  NS_ENSURE_STATE(mDocShell);

  NS_ENSURE_SUCCESS(mDocShell->GetTitle(aTitle), NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP
nsWebBrowser::SetTitle(const nsAString& aTitle) {
  NS_ENSURE_STATE(mDocShell);

  NS_ENSURE_SUCCESS(mDocShell->SetTitle(aTitle), NS_ERROR_FAILURE);

  return NS_OK;
}

//*****************************************************************************
// nsWebBrowser: Listener Helpers
//*****************************************************************************

void nsWebBrowser::SetDocShell(nsDocShell* aDocShell) {
  // We need to keep the docshell alive while we perform the changes, but we
  // don't need to call any methods on it.
  nsCOMPtr<nsIDocShell> kungFuDeathGrip(mDocShell);
  mozilla::Unused << kungFuDeathGrip;

  if (aDocShell) {
    MOZ_ASSERT(!mDocShell, "Should not overwrite an existing value!");

    mDocShell = aDocShell;

    // By default, do not allow DNS prefetch, so we don't break our frozen
    // API.  Embeddors who decide to enable it should do so manually.
    mDocShell->SetAllowDNSPrefetch(false);
  } else {
    if (mDocShellTreeOwner) {
      mDocShellTreeOwner->RemoveFromWatcher();  // evil twin of Add in Create()
    }
    if (mDocShell) {
      mDocShell->Destroy();
    }
    if (!mWillChangeProcess && mDocShell) {
      mDocShell->GetBrowsingContext()->Detach(/* aFromIPC */ true);
    }

    mDocShell = nullptr;
  }
}

void nsWebBrowser::EnsureDocShellTreeOwner() {
  if (mDocShellTreeOwner) {
    return;
  }

  mDocShellTreeOwner = new nsDocShellTreeOwner();
  mDocShellTreeOwner->WebBrowser(this);
}

static void DrawPaintedLayer(PaintedLayer* aLayer, gfxContext* aContext,
                             const nsIntRegion& aRegionToDraw,
                             const nsIntRegion& aDirtyRegion,
                             DrawRegionClip aClip,
                             const nsIntRegion& aRegionToInvalidate,
                             void* aCallbackData) {
  DrawTarget& aDrawTarget = *aContext->GetDrawTarget();

  ColorPattern color(ToDeviceColor(*static_cast<nscolor*>(aCallbackData)));
  nsIntRect dirtyRect = aRegionToDraw.GetBounds();
  aDrawTarget.FillRect(
      Rect(dirtyRect.X(), dirtyRect.Y(), dirtyRect.Width(), dirtyRect.Height()),
      color);
}

void nsWebBrowser::WindowActivated() {
#if defined(DEBUG_smaug)
  RefPtr<dom::Document> document = mDocShell->GetDocument();
  nsAutoString documentURI;
  document->GetDocumentURI(documentURI);
  printf("nsWebBrowser::NS_ACTIVATE %p %s\n", (void*)this,
         NS_ConvertUTF16toUTF8(documentURI).get());
#endif
  FocusActivate(nsFocusManager::GenerateFocusActionId());
}

void nsWebBrowser::WindowDeactivated() {
#if defined(DEBUG_smaug)
  RefPtr<dom::Document> document = mDocShell->GetDocument();
  nsAutoString documentURI;
  document->GetDocumentURI(documentURI);
  printf("nsWebBrowser::NS_DEACTIVATE %p %s\n", (void*)this,
         NS_ConvertUTF16toUTF8(documentURI).get());
#endif
  FocusDeactivate(nsFocusManager::GenerateFocusActionId());
}

bool nsWebBrowser::PaintWindow(nsIWidget* aWidget,
                               LayoutDeviceIntRegion aRegion) {
  LayerManager* layerManager = aWidget->GetLayerManager();
  NS_ASSERTION(layerManager, "Must be in paint event");

  layerManager->BeginTransaction();
  RefPtr<PaintedLayer> root = layerManager->CreatePaintedLayer();
  if (root) {
    nsIntRect dirtyRect = aRegion.GetBounds().ToUnknownRect();
    root->SetVisibleRegion(LayerIntRegion::FromUnknownRegion(dirtyRect));
    layerManager->SetRoot(root);
  }

  layerManager->EndTransaction(DrawPaintedLayer, &mBackgroundColor);
  return true;
}

void nsWebBrowser::FocusActivate(uint64_t aActionId) {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
  if (fm && window) {
    fm->WindowRaised(window, aActionId);
  }
}

void nsWebBrowser::FocusDeactivate(uint64_t aActionId) {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
  if (fm && window) {
    fm->WindowLowered(window, aActionId);
  }
}

void nsWebBrowser::SetWillChangeProcess() {
  mWillChangeProcess = true;
  if (mDocShell) {
    nsDocShell::Cast(mDocShell)->SetWillChangeProcess();
  }
}

void nsWebBrowser::WidgetListenerDelegate::WindowActivated() {
  RefPtr<nsWebBrowser> holder = mWebBrowser;
  holder->WindowActivated();
}

void nsWebBrowser::WidgetListenerDelegate::WindowDeactivated() {
  RefPtr<nsWebBrowser> holder = mWebBrowser;
  holder->WindowDeactivated();
}

bool nsWebBrowser::WidgetListenerDelegate::PaintWindow(
    nsIWidget* aWidget, mozilla::LayoutDeviceIntRegion aRegion) {
  RefPtr<nsWebBrowser> holder = mWebBrowser;
  return holder->PaintWindow(aWidget, aRegion);
}
