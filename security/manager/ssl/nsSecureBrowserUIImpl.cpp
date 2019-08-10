/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSecureBrowserUIImpl.h"

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Document.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsGlobalWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsITransportSecurityInfo.h"
#include "nsIWebProgress.h"

using namespace mozilla;

LazyLogModule gSecureBrowserUILog("nsSecureBrowserUI");

nsSecureBrowserUIImpl::nsSecureBrowserUIImpl()
    : mState(0), mEvent(0), mIsSecureContext(false) {
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMPL_ISUPPORTS(nsSecureBrowserUIImpl, nsISecureBrowserUI,
                  nsIWebProgressListener, nsISupportsWeakReference)

NS_IMETHODIMP
nsSecureBrowserUIImpl::Init(nsIDocShell* aDocShell) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aDocShell);

  aDocShell->SetSecurityUI(this);

  // The Docshell will own the SecureBrowserUI object, we keep a weak ref.
  nsresult rv;
  mDocShell = do_GetWeakReference(aDocShell, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // hook up to the webprogress notifications.
  nsCOMPtr<nsIWebProgress> wp(do_GetInterface(aDocShell));
  if (!wp) {
    return NS_ERROR_FAILURE;
  }

  // Save this so we can compare it to the web progress in OnLocationChange.
  mWebProgress = do_GetWeakReference(wp, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return wp->AddProgressListener(this, nsIWebProgress::NOTIFY_LOCATION);
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::GetState(uint32_t* aState) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aState);

  MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug, ("GetState %p", this));
  // With respect to mixed content and tracking protection, we won't know when
  // the state of our document (or a subdocument) has changed, so we ask the
  // docShell.
  CheckForMixedContent();
  MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug, ("  mState: %x", mState));

  *aState = mState;
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::GetIsSecureContext(bool* aIsSecureContext) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aIsSecureContext);

  *aIsSecureContext = mIsSecureContext;
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::GetContentBlockingEvent(uint32_t* aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aEvent);

  MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
          ("GetContentBlockingEvent %p", this));
  // With respect to mixed content and tracking protection, we won't know when
  // the state of our document (or a subdocument) has changed, so we ask the
  // docShell.
  CheckForContentBlockingEvents();
  MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug, ("  mEvent: %x", mEvent));

  *aEvent = mEvent;
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::GetSecInfo(nsITransportSecurityInfo** result) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(result);

  *result = mTopLevelSecurityInfo;
  NS_IF_ADDREF(*result);

  return NS_OK;
}

already_AddRefed<dom::Document>
nsSecureBrowserUIImpl::PrepareForContentChecks() {
  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShell);
  if (!docShell) {
    return nullptr;
  }

  // For content docShells, the mixed content security state is set on the root
  // docShell.
  if (docShell->ItemType() == nsIDocShellTreeItem::typeContent) {
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem(docShell);
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    Unused << docShellTreeItem->GetInProcessSameTypeRootTreeItem(
        getter_AddRefs(sameTypeRoot));
    MOZ_ASSERT(
        sameTypeRoot,
        "No document shell root tree item from document shell tree item!");
    docShell = do_QueryInterface(sameTypeRoot);
    if (!docShell) {
      return nullptr;
    }
  }

  RefPtr<dom::Document> doc = docShell->GetDocument();
  return doc.forget();
}

// Ask the docShell if we've blocked or loaded any mixed content.
void nsSecureBrowserUIImpl::CheckForMixedContent() {
  RefPtr<dom::Document> doc = PrepareForContentChecks();
  if (!doc) {
    // If the docshell has no document, then there is no need to update mState.
    return;
  }

  // Has mixed content been loaded or blocked in nsMixedContentBlocker?
  // This only applies to secure documents even if they're affected by mixed
  // content blocking in which case the STATE_IS_BROKEN bit would be set rather
  // than STATE_IS_SECURE.
  if (((mState & STATE_IS_SECURE) != 0) || ((mState & STATE_IS_BROKEN) != 0)) {
    if (doc->GetHasMixedActiveContentLoaded()) {
      mState |= STATE_IS_BROKEN | STATE_LOADED_MIXED_ACTIVE_CONTENT;
      mState &= ~STATE_IS_SECURE;
    }

    if (doc->GetHasMixedDisplayContentLoaded()) {
      mState |= STATE_IS_BROKEN | STATE_LOADED_MIXED_DISPLAY_CONTENT;
      mState &= ~STATE_IS_SECURE;
    }

    if (doc->GetHasMixedActiveContentBlocked()) {
      mState |= STATE_BLOCKED_MIXED_ACTIVE_CONTENT;
    }

    if (doc->GetHasMixedDisplayContentBlocked()) {
      mState |= STATE_BLOCKED_MIXED_DISPLAY_CONTENT;
    }
  }
}

// Ask the docShell if we have any content blocking events.
void nsSecureBrowserUIImpl::CheckForContentBlockingEvents() {
  RefPtr<dom::Document> doc = PrepareForContentChecks();
  if (!doc) {
    // If the docshell has no document, then there is no need to update mState.
    return;
  }

  // Has tracking content been blocked or loaded?
  if (doc->GetHasTrackingContentBlocked()) {
    mEvent |= STATE_BLOCKED_TRACKING_CONTENT;
  }

  if (doc->GetHasTrackingContentLoaded()) {
    mEvent |= STATE_LOADED_TRACKING_CONTENT;
  }

  // Has fingerprinting content been blocked or loaded?
  if (doc->GetHasFingerprintingContentBlocked()) {
    mEvent |= STATE_BLOCKED_FINGERPRINTING_CONTENT;
  }

  if (doc->GetHasFingerprintingContentLoaded()) {
    mEvent |= STATE_LOADED_FINGERPRINTING_CONTENT;
  }

  // Has cryptomining content been blocked or loaded?
  if (doc->GetHasCryptominingContentBlocked()) {
    mEvent |= STATE_BLOCKED_CRYPTOMINING_CONTENT;
  }

  if (doc->GetHasCryptominingContentLoaded()) {
    mEvent |= STATE_LOADED_CRYPTOMINING_CONTENT;
  }

  // Has socialtracking content been blocked or loaded?
  if (doc->GetHasSocialTrackingContentBlocked()) {
    mEvent |= STATE_BLOCKED_SOCIALTRACKING_CONTENT;
  }

  if (doc->GetHasSocialTrackingContentLoaded()) {
    mEvent |= STATE_LOADED_SOCIALTRACKING_CONTENT;
  }

  // Other block types.
  if (doc->GetHasCookiesBlockedByPermission()) {
    mEvent |= STATE_COOKIES_BLOCKED_BY_PERMISSION;
  }

  if (doc->GetHasTrackingCookiesBlocked()) {
    mEvent |= STATE_COOKIES_BLOCKED_TRACKER;
  }

  if (doc->GetHasForeignCookiesBlocked()) {
    mEvent |= STATE_COOKIES_BLOCKED_FOREIGN;
  }

  if (doc->GetHasAllCookiesBlocked()) {
    mEvent |= STATE_COOKIES_BLOCKED_ALL;
  }

  if (doc->GetHasCookiesLoaded()) {
    mEvent |= STATE_COOKIES_LOADED;
  }
}

// Helper function to determine if the given URI can be considered secure.
// Essentially, only "https" URIs can be considered secure. However, the URI we
// have may be e.g. view-source:https://example.com, in which case we have to
// evaluate the innermost URI.
static nsresult URICanBeConsideredSecure(
    nsIURI* uri, /* out */ bool& canBeConsideredSecure) {
  MOZ_ASSERT(uri);
  NS_ENSURE_ARG(uri);

  canBeConsideredSecure = false;

  nsCOMPtr<nsIURI> innermostURI = NS_GetInnermostURI(uri);
  if (!innermostURI) {
    MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
            ("  couldn't get innermost URI"));
    return NS_ERROR_FAILURE;
  }
  MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
          ("  innermost URI is '%s'", innermostURI->GetSpecOrDefault().get()));

  canBeConsideredSecure = innermostURI->SchemeIs("https");

  return NS_OK;
}

// Helper function to get the securityInfo from a channel as a
// nsITransportSecurityInfo. The out parameter will be set to null if there is
// no securityInfo set.
static void GetSecurityInfoFromChannel(
    nsIChannel* channel, nsITransportSecurityInfo** securityInfoOut) {
  MOZ_ASSERT(channel);
  MOZ_ASSERT(securityInfoOut);

  NS_ENSURE_TRUE_VOID(channel);
  NS_ENSURE_TRUE_VOID(securityInfoOut);

  *securityInfoOut = nullptr;

  nsCOMPtr<nsISupports> securityInfoSupports;
  nsresult rv = channel->GetSecurityInfo(getter_AddRefs(securityInfoSupports));
  // GetSecurityInfo may return an error, but it's not necessarily fatal - the
  // underlying channel may simply not have a securityInfo.
  if (NS_FAILED(rv)) {
    return;
  }
  nsCOMPtr<nsITransportSecurityInfo> securityInfo(
      do_QueryInterface(securityInfoSupports));
  securityInfo.forget(securityInfoOut);
}

nsresult nsSecureBrowserUIImpl::UpdateStateAndSecurityInfo(nsIChannel* channel,
                                                           nsIURI* uri) {
  MOZ_ASSERT(channel);
  MOZ_ASSERT(uri);

  NS_ENSURE_ARG(channel);
  NS_ENSURE_ARG(uri);

  mState = STATE_IS_INSECURE;
  mTopLevelSecurityInfo = nullptr;

  // Only https is considered secure (it is possible to have e.g. an http URI
  // with a channel that has a securityInfo that indicates the connection is
  // secure - e.g. h2/alt-svc or by visiting an http URI over an https proxy).
  bool canBeConsideredSecure;
  nsresult rv = URICanBeConsideredSecure(uri, canBeConsideredSecure);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!canBeConsideredSecure) {
    MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
            ("  URI can't be considered secure"));
    return NS_OK;
  }

  nsCOMPtr<nsITransportSecurityInfo> securityInfo;
  GetSecurityInfoFromChannel(channel, getter_AddRefs(securityInfo));
  if (securityInfo) {
    MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
            ("  we have a security info %p", securityInfo.get()));

    rv = securityInfo->GetSecurityState(&mState);
    if (NS_FAILED(rv)) {
      return rv;
    }
    // If the security state is STATE_IS_INSECURE, the TLS handshake never
    // completed. Don't set any further state.
    if (mState == STATE_IS_INSECURE) {
      return NS_OK;
    }

    mTopLevelSecurityInfo = securityInfo;
    MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
            ("  set mTopLevelSecurityInfo"));
    bool isEV;
    rv = mTopLevelSecurityInfo->GetIsExtendedValidation(&isEV);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (isEV) {
      MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug, ("  is EV"));
      mState |= STATE_IDENTITY_EV_TOPLEVEL;
    }
    // Proactively check for mixed content in case GetState() is never called
    // (this can happen when loading from the BF cache).
    CheckForMixedContent();
  }
  return NS_OK;
}

// We receive this notification for the nsIWebProgress we added ourselves to
// (i.e. the window we were passed in Init, which should be the top-level
// window or whatever corresponds to an <iframe mozbrowser> element). In some
// cases, we also receive it from nsIWebProgress instances that are children of
// that nsIWebProgress. We ignore notifications from children because they don't
// change the top-level state (if children load mixed or tracking content, the
// docShell will know and will tell us in GetState when we call
// CheckForMixedContent).
// When we receive a notification from the top-level nsIWebProgress, we extract
// any relevant security information and set our state accordingly. We then call
// OnSecurityChange on the docShell corresponding to the nsIWebProgress we were
// initialized with to notify any downstream listeners of the security state.
NS_IMETHODIMP
nsSecureBrowserUIImpl::OnLocationChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest, nsIURI* aLocation,
                                        uint32_t aFlags) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_ARG(aWebProgress);
  NS_ENSURE_ARG(aLocation);

  MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
          ("%p OnLocationChange: %p %p %s %x", this, aWebProgress, aRequest,
           aLocation->GetSpecOrDefault().get(), aFlags));

  // Filter out events from children. See comment at the top of this function.
  // It would be nice if the attribute isTopLevel worked for this, but that
  // filters out events for <iframe mozbrowser> elements, which means they don't
  // get OnSecurityChange events from this implementation. Instead, we check to
  // see if the web progress we were handed here is the same one as we were
  // initialized with.
  nsCOMPtr<nsIWebProgress> originalWebProgress = do_QueryReferent(mWebProgress);
  if (aWebProgress != originalWebProgress) {
    return NS_OK;
  }

  // If this is a same-document location change, we don't need to update our
  // state or notify anyone.
  if (aFlags & LOCATION_CHANGE_SAME_DOCUMENT) {
    return NS_OK;
  }

  mState = 0;
  mEvent = 0;
  mIsSecureContext = false;
  mTopLevelSecurityInfo = nullptr;

  if (aFlags & LOCATION_CHANGE_ERROR_PAGE) {
    mState = STATE_IS_INSECURE;
    mTopLevelSecurityInfo = nullptr;
  } else {
    // NB: aRequest may be null. It may also not be QI-able to nsIChannel.
    nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
    if (channel) {
      MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
              ("  we have a channel %p", channel.get()));
      nsresult rv = UpdateStateAndSecurityInfo(channel, aLocation);
      // Even if this failed, we still want to notify downstream so that we
      // don't leave a stale security indicator. We set everything to "not
      // secure" to be safe.
      if (NS_WARN_IF(NS_FAILED(rv))) {
        MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
                ("  Failed to update security info. "
                 "Setting everything to 'not secure' to be safe."));
        mState = STATE_IS_INSECURE;
        mTopLevelSecurityInfo = nullptr;
      }
    }

    nsCOMPtr<mozIDOMWindowProxy> domWindow;
    aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
    if (domWindow) {
      nsPIDOMWindowOuter* outerWindow = nsPIDOMWindowOuter::From(domWindow);
      if (outerWindow) {
        nsGlobalWindowInner* innerWindow =
            nsGlobalWindowInner::Cast(outerWindow->GetCurrentInnerWindow());
        if (innerWindow) {
          mIsSecureContext = innerWindow->IsSecureContext();
        }
      }
    }
  }

  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShell);
  if (docShell) {
    MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
            ("  calling OnSecurityChange %p %x", aRequest, mState));
    nsDocShell::Cast(docShell)->nsDocLoader::OnSecurityChange(aRequest, mState);
  } else {
    MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug, ("  no docShell?"));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStateChange(nsIWebProgress*, nsIRequest*, uint32_t,
                                     nsresult) {
  MOZ_ASSERT_UNREACHABLE("Should have been excluded in AddProgressListener()");
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnProgressChange(nsIWebProgress*, nsIRequest*, int32_t,
                                        int32_t, int32_t, int32_t) {
  MOZ_ASSERT_UNREACHABLE("Should have been excluded in AddProgressListener()");
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStatusChange(nsIWebProgress*, nsIRequest*, nsresult,
                                      const char16_t*) {
  MOZ_ASSERT_UNREACHABLE("Should have been excluded in AddProgressListener()");
  return NS_OK;
}

nsresult nsSecureBrowserUIImpl::OnSecurityChange(nsIWebProgress*, nsIRequest*,
                                                 uint32_t) {
  MOZ_ASSERT_UNREACHABLE("Should have been excluded in AddProgressListener()");
  return NS_OK;
}

nsresult nsSecureBrowserUIImpl::OnContentBlockingEvent(nsIWebProgress*,
                                                       nsIRequest*, uint32_t) {
  MOZ_ASSERT_UNREACHABLE("Should have been excluded in AddProgressListener()");
  return NS_OK;
}
