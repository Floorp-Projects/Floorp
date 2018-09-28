/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSecureBrowserUIImpl.h"

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"
#include "nsIChannel.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISecurityEventSink.h"
#include "nsITransportSecurityInfo.h"
#include "nsIWebProgress.h"

using namespace mozilla;

LazyLogModule gSecureBrowserUILog("nsSecureBrowserUI");

nsSecureBrowserUIImpl::nsSecureBrowserUIImpl()
  : mState(0)
{
  MOZ_ASSERT(NS_IsMainThread());
}

NS_IMPL_ISUPPORTS(nsSecureBrowserUIImpl,
                  nsISecureBrowserUI,
                  nsIWebProgressListener,
                  nsISupportsWeakReference)

NS_IMETHODIMP
nsSecureBrowserUIImpl::Init(nsIDocShell* aDocShell)
{
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
nsSecureBrowserUIImpl::GetState(uint32_t* aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aState);

  MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug, ("GetState %p", this));
  // With respect to mixed content and tracking protection, we won't know when
  // the state of our document (or a subdocument) has changed, so we ask the
  // docShell.
  CheckForBlockedContent();
  MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug, ("  mState: %x", mState));

  *aState = mState;
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::GetSecInfo(nsITransportSecurityInfo** result)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(result);

  *result = mTopLevelSecurityInfo;
  NS_IF_ADDREF(*result);

  return NS_OK;
}

// Ask the docShell if we've blocked or loaded any mixed or tracking content.
void
nsSecureBrowserUIImpl::CheckForBlockedContent()
{
  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShell);
  if (!docShell) {
    return;
  }

  // For content docShells, the mixed content security state is set on the root
  // docShell.
  if (docShell->ItemType() == nsIDocShellTreeItem::typeContent) {
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem(do_QueryInterface(docShell));
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    Unused << docShellTreeItem->GetSameTypeRootTreeItem(
      getter_AddRefs(sameTypeRoot));
    MOZ_ASSERT(
      sameTypeRoot,
      "No document shell root tree item from document shell tree item!");
    docShell = do_QueryInterface(sameTypeRoot);
    if (!docShell) {
      return;
    }
  }

  // Has mixed content been loaded or blocked in nsMixedContentBlocker?
  // This only applies to secure documents.
  if (mState & STATE_IS_SECURE) {
    if (docShell->GetHasMixedActiveContentLoaded()) {
      mState |= STATE_IS_BROKEN | STATE_LOADED_MIXED_ACTIVE_CONTENT;
      mState &= ~STATE_IS_SECURE;
      mState &= ~STATE_SECURE_HIGH;
    }

    if (docShell->GetHasMixedDisplayContentLoaded()) {
      mState |= STATE_IS_BROKEN | STATE_LOADED_MIXED_DISPLAY_CONTENT;
      mState &= ~STATE_IS_SECURE;
      mState &= ~STATE_SECURE_HIGH;
    }

    if (docShell->GetHasMixedActiveContentBlocked()) {
      mState |= STATE_BLOCKED_MIXED_ACTIVE_CONTENT;
    }

    if (docShell->GetHasMixedDisplayContentBlocked()) {
      mState |= STATE_BLOCKED_MIXED_DISPLAY_CONTENT;
    }
  }

  // Has tracking content been blocked or loaded?
  if (docShell->GetHasTrackingContentBlocked()) {
    mState |= STATE_BLOCKED_TRACKING_CONTENT;
  }

  if (docShell->GetHasSlowTrackingContentBlocked()) {
    mState |= STATE_BLOCKED_SLOW_TRACKING_CONTENT;
  }

  if (docShell->GetHasTrackingContentLoaded()) {
    mState |= STATE_LOADED_TRACKING_CONTENT;
  }

  if (docShell->GetHasCookiesBlockedByPermission()) {
    mState |= STATE_COOKIES_BLOCKED_BY_PERMISSION;
  }

  if (docShell->GetHasCookiesBlockedDueToTrackers()) {
    mState |= STATE_COOKIES_BLOCKED_TRACKER;
  }

  if (docShell->GetHasForeignCookiesBeenBlocked()) {
    mState |= STATE_COOKIES_BLOCKED_FOREIGN;
  }

  if (docShell->GetHasAllCookiesBeenBlocked()) {
    mState |= STATE_COOKIES_BLOCKED_ALL;
  }
}

// Helper function to get the securityInfo from a channel as a
// nsITransportSecurityInfo. The out parameter will be set to null if there is
// no securityInfo set.
static void
GetSecurityInfoFromChannel(nsIChannel* channel,
                           nsITransportSecurityInfo** securityInfoOut)
{
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

nsresult
nsSecureBrowserUIImpl::UpdateStateAndSecurityInfo(nsIChannel* channel,
                                                  nsIURI* uri)
{
  MOZ_ASSERT(channel);
  MOZ_ASSERT(uri);

  NS_ENSURE_ARG(channel);
  NS_ENSURE_ARG(uri);

  mState = STATE_IS_INSECURE;
  mTopLevelSecurityInfo = nullptr;

  nsCOMPtr<nsITransportSecurityInfo> securityInfo;
  GetSecurityInfoFromChannel(channel, getter_AddRefs(securityInfo));
  if (securityInfo) {
    MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
            ("  we have a security info %p", securityInfo.get()));

    nsresult rv = securityInfo->GetSecurityState(&mState);
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
// CheckForBlockedContent).
// When we receive a notification from the top-level nsIWebProgress, we extract
// any relevant security information and set our state accordingly. We then call
// OnSecurityChange to notify any downstream listeners of the security state.
NS_IMETHODIMP
nsSecureBrowserUIImpl::OnLocationChange(nsIWebProgress* aWebProgress,
                                        nsIRequest* aRequest,
                                        nsIURI* aLocation,
                                        uint32_t aFlags)
{
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

  // Clear any state that varies by location.
  if (!(aFlags & LOCATION_CHANGE_SAME_DOCUMENT)) {
    mState = 0;
    mTopLevelSecurityInfo = nullptr;
  }

  // NB: aRequest may be null. It may also not be QI-able to nsIChannel.
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel) {
    MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
            ("  we have a channel %p", channel.get()));
    nsresult rv = UpdateStateAndSecurityInfo(channel, aLocation);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsISecurityEventSink> eventSink;
    NS_QueryNotificationCallbacks(channel, eventSink);
    if (NS_WARN_IF(!eventSink)) {
      return NS_ERROR_INVALID_ARG;
    }
    MOZ_LOG(gSecureBrowserUILog, LogLevel::Debug,
            ("  calling OnSecurityChange %p %x\n", aRequest, mState));
    Unused << eventSink->OnSecurityChange(aRequest, mState);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStateChange(nsIWebProgress*,
                                     nsIRequest*,
                                     uint32_t,
                                     nsresult)
{
  MOZ_ASSERT_UNREACHABLE("Should have been excluded in AddProgressListener()");
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnProgressChange(nsIWebProgress*,
                                        nsIRequest*,
                                        int32_t,
                                        int32_t,
                                        int32_t,
                                        int32_t)
{
  MOZ_ASSERT_UNREACHABLE("Should have been excluded in AddProgressListener()");
  return NS_OK;
}

NS_IMETHODIMP
nsSecureBrowserUIImpl::OnStatusChange(nsIWebProgress*,
                                      nsIRequest*,
                                      nsresult,
                                      const char16_t*)
{
  MOZ_ASSERT_UNREACHABLE("Should have been excluded in AddProgressListener()");
  return NS_OK;
}

nsresult
nsSecureBrowserUIImpl::OnSecurityChange(nsIWebProgress*, nsIRequest*, uint32_t)
{
  MOZ_ASSERT_UNREACHABLE("Should have been excluded in AddProgressListener()");
  return NS_OK;
}
