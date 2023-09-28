/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/extensions/WebNavigationContent.h"

#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ContentFrameMessageManager.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/extensions/ExtensionsChild.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/Try.h"
#include "nsCRT.h"
#include "nsDocShellLoadTypes.h"
#include "nsPIWindowRoot.h"
#include "nsIChannel.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserverService.h"
#include "nsIPropertyBag2.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsQueryObject.h"

namespace mozilla {
namespace extensions {

/* static */
already_AddRefed<WebNavigationContent> WebNavigationContent::GetSingleton() {
  static RefPtr<WebNavigationContent> sSingleton;
  if (!sSingleton) {
    sSingleton = new WebNavigationContent();
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }
  return do_AddRef(sSingleton);
}

NS_IMPL_ISUPPORTS(WebNavigationContent, nsIObserver, nsIDOMEventListener,
                  nsIWebProgressListener, nsISupportsWeakReference)

void WebNavigationContent::Init() {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  obs->AddObserver(this, "chrome-event-target-created", true);
  obs->AddObserver(this, "webNavigation-createdNavigationTarget-from-js", true);
}

NS_IMETHODIMP WebNavigationContent::Observe(nsISupports* aSubject,
                                            char const* aTopic,
                                            char16_t const* aData) {
  if (!nsCRT::strcmp(aTopic, "chrome-event-target-created")) {
    // This notification is sent whenever a new window root is created, with the
    // subject being an EventTarget corresponding to either an nsWindowRoot, or
    // additionally also an InProcessBrowserChildMessageManager in the parent.
    // This is the same entry point used to register listeners for the JS window
    // actor API.
    if (RefPtr<dom::EventTarget> eventTarget = do_QueryObject(aSubject)) {
      AttachListeners(eventTarget);
    }

    nsCOMPtr<nsIDocShell> docShell;
    if (nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(aSubject)) {
      docShell = root->GetWindow()->GetDocShell();
    } else if (RefPtr<dom::ContentFrameMessageManager> mm =
                   do_QueryObject(aSubject)) {
      docShell = mm->GetDocShell(IgnoreErrors());
    }
    if (docShell && docShell->GetBrowsingContext()->IsContent()) {
      nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(docShell));

      webProgress->AddProgressListener(this,
                                       nsIWebProgress::NOTIFY_STATE_WINDOW |
                                           nsIWebProgress::NOTIFY_LOCATION);
    }
  } else if (!nsCRT::strcmp(aTopic,
                            "webNavigation-createdNavigationTarget-from-js")) {
    if (nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject)) {
      return OnCreatedNavigationTargetFromJS(props);
    }
  }
  return NS_OK;
}

void WebNavigationContent::AttachListeners(dom::EventTarget* aEventTarget) {
  EventListenerManager* elm = aEventTarget->GetOrCreateListenerManager();
  NS_ENSURE_TRUE_VOID(elm);

  elm->AddEventListenerByType(this, u"DOMContentLoaded"_ns,
                              TrustedEventsAtCapture());
}

NS_IMETHODIMP
WebNavigationContent::HandleEvent(dom::Event* aEvent) {
  if (aEvent->ShouldIgnoreChromeEventTargetListener()) {
    return NS_OK;
  }

#ifdef DEBUG
  {
    nsAutoString type;
    aEvent->GetType(type);
    MOZ_ASSERT(type.EqualsLiteral("DOMContentLoaded"));
  }
#endif

  if (RefPtr<dom::Document> doc = do_QueryObject(aEvent->GetTarget())) {
    dom::BrowsingContext* bc = doc->GetBrowsingContext();
    if (bc && bc->IsContent()) {
      ExtensionsChild::Get().SendDOMContentLoaded(bc, doc->GetDocumentURI());
    }
  }

  return NS_OK;
}

static dom::BrowsingContext* GetBrowsingContext(nsIWebProgress* aWebProgress) {
  // FIXME: Get this via nsIWebNavigation instead.
  nsCOMPtr<nsIDocShell> docShell(do_GetInterface(aWebProgress));
  return docShell->GetBrowsingContext();
}

FrameTransitionData WebNavigationContent::GetFrameTransitionData(
    nsIWebProgress* aWebProgress, nsIRequest* aRequest) {
  FrameTransitionData result;

  uint32_t loadType = 0;
  Unused << aWebProgress->GetLoadType(&loadType);

  if (loadType & nsIDocShell::LOAD_CMD_HISTORY) {
    result.forwardBack() = true;
  }

  if (loadType & nsIDocShell::LOAD_CMD_RELOAD) {
    result.reload() = true;
  }

  if (LOAD_TYPE_HAS_FLAGS(loadType, nsIWebNavigation::LOAD_FLAGS_IS_REFRESH)) {
    result.clientRedirect() = true;
  }

  if (nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest)) {
    nsCOMPtr<nsILoadInfo> loadInfo(channel->LoadInfo());
    if (loadInfo->RedirectChain().Length()) {
      result.serverRedirect() = true;
    }
    if (loadInfo->GetIsFormSubmission() &&
        !(loadType & (nsIDocShell::LOAD_CMD_HISTORY |

                      nsIDocShell::LOAD_CMD_RELOAD))) {
      result.formSubmit() = true;
    }
  }

  return result;
}

nsresult WebNavigationContent::OnCreatedNavigationTargetFromJS(
    nsIPropertyBag2* aProps) {
  nsCOMPtr<nsIDocShell> createdDocShell(
      do_GetProperty(aProps, u"createdTabDocShell"_ns));
  nsCOMPtr<nsIDocShell> sourceDocShell(
      do_GetProperty(aProps, u"sourceTabDocShell"_ns));

  NS_ENSURE_ARG_POINTER(createdDocShell);
  NS_ENSURE_ARG_POINTER(sourceDocShell);

  dom::BrowsingContext* createdBC = createdDocShell->GetBrowsingContext();
  dom::BrowsingContext* sourceBC = sourceDocShell->GetBrowsingContext();
  if (createdBC->IsContent() && sourceBC->IsContent()) {
    nsCString url;
    Unused << aProps->GetPropertyAsACString(u"url"_ns, url);

    ExtensionsChild::Get().SendCreatedNavigationTarget(createdBC, sourceBC,
                                                       url);
  }
  return NS_OK;
}

// nsIWebProgressListener
NS_IMETHODIMP
WebNavigationContent::OnStateChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest, uint32_t aStateFlags,
                                    nsresult aStatus) {
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  NS_ENSURE_TRUE(channel, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIURI> uri;
  MOZ_TRY(channel->GetURI(getter_AddRefs(uri)));

  // Prevents "about", "chrome", "resource" and "moz-extension" URI schemes to
  // be reported with the resolved "file" or "jar" URIs (see bug 1246125)
  if (uri->SchemeIs("file") || uri->SchemeIs("jar")) {
    nsCOMPtr<nsIURI> originalURI;
    MOZ_TRY(channel->GetOriginalURI(getter_AddRefs(originalURI)));
    // FIXME: We probably actually want NS_GetFinalChannelURI here.
    if (originalURI->SchemeIs("about") || originalURI->SchemeIs("chrome") ||
        originalURI->SchemeIs("resource") ||
        originalURI->SchemeIs("moz-extension")) {
      uri = originalURI.forget();
    }
  }

  RefPtr<dom::BrowsingContext> bc(GetBrowsingContext(aWebProgress));
  NS_ENSURE_ARG_POINTER(bc);

  ExtensionsChild::Get().SendStateChange(bc, uri, aStatus, aStateFlags);

  // Based on the docs of the webNavigation.onCommitted event, it should be
  // raised when: "The document  might still be downloading, but at least part
  // of the document has been received" and for some reason we don't fire
  // onLocationChange for the initial navigation of a sub-frame. For the above
  // two reasons, when the navigation event is related to a sub-frame we process
  // the document change here and then send an OnDocumentChange message to the
  // main process, where it will be turned into a webNavigation.onCommitted
  // event. (bug 1264936 and bug 125662)
  if (!bc->IsTop() && aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT) {
    ExtensionsChild::Get().SendDocumentChange(
        bc, GetFrameTransitionData(aWebProgress, aRequest), uri);
  }
  return NS_OK;
}

NS_IMETHODIMP
WebNavigationContent::OnProgressChange(nsIWebProgress* aWebProgress,
                                       nsIRequest* aRequest,
                                       int32_t aCurSelfProgress,
                                       int32_t aMaxSelfProgress,
                                       int32_t aCurTotalProgress,
                                       int32_t aMaxTotalProgress) {
  MOZ_ASSERT_UNREACHABLE("Listener did not request ProgressChange events");
  return NS_OK;
}

NS_IMETHODIMP
WebNavigationContent::OnLocationChange(nsIWebProgress* aWebProgress,
                                       nsIRequest* aRequest, nsIURI* aLocation,
                                       uint32_t aFlags) {
  RefPtr<dom::BrowsingContext> bc(GetBrowsingContext(aWebProgress));
  NS_ENSURE_ARG_POINTER(bc);

  // When a frame navigation doesn't change the current loaded document
  // (which can be due to history.pushState/replaceState or to a changed hash in
  // the url), it is reported only to the onLocationChange, for this reason we
  // process the history change here and then we are going to send an
  // OnHistoryChange message to the main process, where it will be turned into
  // a webNavigation.onHistoryStateUpdated/onReferenceFragmentUpdated event.
  if (aFlags & nsIWebProgressListener::LOCATION_CHANGE_SAME_DOCUMENT) {
    uint32_t loadType = 0;
    MOZ_TRY(aWebProgress->GetLoadType(&loadType));

    // When the location changes but the document is the same:
    // - path not changed and hash changed -> |onReferenceFragmentUpdated|
    //   (even if it changed using |history.pushState|)
    // - path not changed and hash not changed -> |onHistoryStateUpdated|
    //   (only if it changes using |history.pushState|)
    // - path changed -> |onHistoryStateUpdated|
    bool isHistoryStateUpdated = false;
    bool isReferenceFragmentUpdated = false;
    if (aFlags & nsIWebProgressListener::LOCATION_CHANGE_HASHCHANGE) {
      isReferenceFragmentUpdated = true;
    } else if (loadType & nsIDocShell::LOAD_CMD_PUSHSTATE) {
      isHistoryStateUpdated = true;
    } else if (loadType & nsIDocShell::LOAD_CMD_HISTORY) {
      isHistoryStateUpdated = true;
    }

    if (isHistoryStateUpdated || isReferenceFragmentUpdated) {
      ExtensionsChild::Get().SendHistoryChange(
          bc, GetFrameTransitionData(aWebProgress, aRequest), aLocation,
          isHistoryStateUpdated, isReferenceFragmentUpdated);
    }
  } else if (bc->IsTop()) {
    MOZ_ASSERT(bc->IsInProcess());
    if (RefPtr browserChild = dom::BrowserChild::GetFrom(bc->GetDocShell())) {
      // Only send progress events which happen after we've started loading
      // things into the BrowserChild. This matches the behavior of the remote
      // WebProgress implementation.
      if (browserChild->ShouldSendWebProgressEventsToParent()) {
        // We have to catch the document changes from top level frames here,
        // where we can detect the "server redirect" transition.
        // (bug 1264936 and bug 125662)
        ExtensionsChild::Get().SendDocumentChange(
            bc, GetFrameTransitionData(aWebProgress, aRequest), aLocation);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
WebNavigationContent::OnStatusChange(nsIWebProgress* aWebProgress,
                                     nsIRequest* aRequest, nsresult aStatus,
                                     const char16_t* aMessage) {
  MOZ_ASSERT_UNREACHABLE("Listener did not request StatusChange events");
  return NS_OK;
}

NS_IMETHODIMP
WebNavigationContent::OnSecurityChange(nsIWebProgress* aWebProgress,
                                       nsIRequest* aRequest, uint32_t aState) {
  MOZ_ASSERT_UNREACHABLE("Listener did not request SecurityChange events");
  return NS_OK;
}

NS_IMETHODIMP
WebNavigationContent::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                             nsIRequest* aRequest,
                                             uint32_t aEvent) {
  MOZ_ASSERT_UNREACHABLE("Listener did not request ContentBlocking events");
  return NS_OK;
}

}  // namespace extensions
}  // namespace mozilla
