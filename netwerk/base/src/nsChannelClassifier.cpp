/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsChannelClassifier.h"

#include "mozIThirdPartyUtil.h"
#include "nsNetUtil.h"
#include "nsICacheEntry.h"
#include "nsICachingChannel.h"
#include "nsIChannel.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIIOService.h"
#include "nsIPermissionManager.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsISecureBrowserUI.h"
#include "nsISecurityEventSink.h"
#include "nsIWebProgressListener.h"
#include "nsPIDOMWindow.h"

#include "mozilla/Preferences.h"

#include "prlog.h"

using mozilla::Preferences;

#if defined(PR_LOGGING)
//
// NSPR_LOG_MODULES=nsChannelClassifier:5
//
static PRLogModuleInfo *gChannelClassifierLog;
#endif
#undef LOG
#define LOG(args)     PR_LOG(gChannelClassifierLog, PR_LOG_DEBUG, args)

NS_IMPL_ISUPPORTS(nsChannelClassifier,
                  nsIURIClassifierCallback)

nsChannelClassifier::nsChannelClassifier()
  : mIsAllowListed(false)
{
#if defined(PR_LOGGING)
    if (!gChannelClassifierLog)
        gChannelClassifierLog = PR_NewLogModule("nsChannelClassifier");
#endif
}

nsresult
nsChannelClassifier::ShouldEnableTrackingProtection(nsIChannel *aChannel,
                                                    bool *result)
{
    NS_ENSURE_ARG(result);
    *result = false;

    if (!Preferences::GetBool("privacy.trackingprotection.enabled", false)) {
      return NS_OK;
    }

    nsresult rv;
    nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
        do_GetService(THIRDPARTYUTIL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    // Third party checks don't work for chrome:// URIs in mochitests, so just
    // default to isThirdParty = true
    bool isThirdParty = true;
    (void)thirdPartyUtil->IsThirdPartyChannel(aChannel, nullptr, &isThirdParty);
    if (!isThirdParty) {
        *result = false;
        return NS_OK;
    }

    nsCOMPtr<nsIDOMWindow> win;
    rv = thirdPartyUtil->GetTopWindowForChannel(aChannel, getter_AddRefs(win));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    rv = thirdPartyUtil->GetURIFromWindow(win, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    const char ALLOWLIST_EXAMPLE_PREF[] = "channelclassifier.allowlist_example";
    if (!uri && Preferences::GetBool(ALLOWLIST_EXAMPLE_PREF, false)) {
      LOG(("nsChannelClassifier[%p]: Allowlisting test domain", this));
      rv = ios->NewURI(NS_LITERAL_CSTRING("http://allowlisted.example.com"),
                       nullptr, nullptr, getter_AddRefs(uri));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Take the host/port portion so we can allowlist by site. Also ignore the
    // scheme, since users who put sites on the allowlist probably don't expect
    // allowlisting to depend on scheme.
    nsCOMPtr<nsIURL> url = do_QueryInterface(uri, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString escaped(NS_LITERAL_CSTRING("https://"));
    nsAutoCString temp;
    rv = url->GetHostPort(temp);
    NS_ENSURE_SUCCESS(rv, rv);
    escaped.Append(temp);

    // Stuff the whole thing back into a URI for the permission manager.
    rv = ios->NewURI(escaped, nullptr, nullptr, getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPermissionManager> permMgr =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t permissions = nsIPermissionManager::UNKNOWN_ACTION;
    rv = permMgr->TestPermission(uri, "trackingprotection", &permissions);
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
    if (permissions == nsIPermissionManager::ALLOW_ACTION) {
        LOG(("nsChannelClassifier[%p]: Allowlisting channel[%p] for %s", this,
             aChannel, escaped.get()));
    }
#endif

    if (permissions == nsIPermissionManager::ALLOW_ACTION) {
      mIsAllowListed = true;
      *result = false;
    } else {
      *result = true;
    }

    // Tracking protection will be enabled so return without updating
    // the security state. If any channels are subsequently cancelled
    // (page elements blocked) the state will be then updated.
    if (*result) {
      return NS_OK;
    }

    // Tracking protection will be disabled so update the security state
    // of the document and fire a secure change event.
    nsCOMPtr<nsPIDOMWindow> pwin = do_QueryInterface(win, &rv);
    NS_ENSURE_SUCCESS(rv, NS_OK);
    nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
    if (!docShell) {
      return NS_OK;
    }
    nsCOMPtr<nsIDocument> doc = do_GetInterface(docShell, &rv);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    // Notify nsIWebProgressListeners of this security event.
    // Can be used to change the UI state.
    nsCOMPtr<nsISecurityEventSink> eventSink = do_QueryInterface(docShell, &rv);
    NS_ENSURE_SUCCESS(rv, NS_OK);
    uint32_t state = 0;
    nsCOMPtr<nsISecureBrowserUI> securityUI;
    docShell->GetSecurityUI(getter_AddRefs(securityUI));
    if (!securityUI) {
      return NS_OK;
    }
    doc->SetHasTrackingContentLoaded(true);
    securityUI->GetState(&state);
    state |= nsIWebProgressListener::STATE_LOADED_TRACKING_CONTENT;
    eventSink->OnSecurityChange(nullptr, state);

    return NS_OK;
}

nsresult
nsChannelClassifier::Start(nsIChannel *aChannel)
{
    // Don't bother to run the classifier on a load that has already failed.
    // (this might happen after a redirect)
    nsresult status;
    aChannel->GetStatus(&status);
    if (NS_FAILED(status))
        return NS_OK;

    // Don't bother to run the classifier on a cached load that was
    // previously classified.
    if (HasBeenClassified(aChannel)) {
        return NS_OK;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    // Don't bother checking certain types of URIs.
    bool hasFlags;
    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_DANGEROUS_TO_LOAD,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_OK;

    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_IS_LOCAL_FILE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_OK;

    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_IS_UI_RESOURCE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_OK;

    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_OK;

    nsCOMPtr<nsIURIClassifier> uriClassifier =
        do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
    if (rv == NS_ERROR_FACTORY_NOT_REGISTERED ||
        rv == NS_ERROR_NOT_AVAILABLE) {
        // no URI classifier, ignore this failure.
        return NS_OK;
    }
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> principal;
    rv = securityManager->GetChannelPrincipal(aChannel,
                                              getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    bool expectCallback;
    bool trackingProtectionEnabled = false;
    (void)ShouldEnableTrackingProtection(aChannel, &trackingProtectionEnabled);

    rv = uriClassifier->Classify(principal, trackingProtectionEnabled, this,
                                 &expectCallback);
    if (NS_FAILED(rv)) return rv;

    if (expectCallback) {
        // Suspend the channel, it will be resumed when we get the classifier
        // callback.
        rv = aChannel->Suspend();
        if (NS_FAILED(rv)) {
            // Some channels (including nsJSChannel) fail on Suspend.  This
            // shouldn't be fatal, but will prevent malware from being
            // blocked on these channels.
            return NS_OK;
        }

        mSuspendedChannel = aChannel;
#ifdef DEBUG
        LOG(("nsChannelClassifier[%p]: suspended channel %p",
             this, mSuspendedChannel.get()));
#endif
    }

    return NS_OK;
}

// Note in the cache entry that this URL was classified, so that future
// cached loads don't need to be checked.
void
nsChannelClassifier::MarkEntryClassified(nsresult status)
{
    // Don't cache tracking classifications because we support allowlisting.
    if (status == NS_ERROR_TRACKING_URI || mIsAllowListed) {
        return;
    }

    nsCOMPtr<nsICachingChannel> cachingChannel =
        do_QueryInterface(mSuspendedChannel);
    if (!cachingChannel) {
        return;
    }

    nsCOMPtr<nsISupports> cacheToken;
    cachingChannel->GetCacheToken(getter_AddRefs(cacheToken));
    if (!cacheToken) {
        return;
    }

    nsCOMPtr<nsICacheEntry> cacheEntry =
        do_QueryInterface(cacheToken);
    if (!cacheEntry) {
        return;
    }

    cacheEntry->SetMetaDataElement("necko:classified",
                                   NS_SUCCEEDED(status) ? "1" : nullptr);
}

bool
nsChannelClassifier::HasBeenClassified(nsIChannel *aChannel)
{
    nsCOMPtr<nsICachingChannel> cachingChannel =
        do_QueryInterface(aChannel);
    if (!cachingChannel) {
        return false;
    }

    // Only check the tag if we are loading from the cache without
    // validation.
    bool fromCache;
    if (NS_FAILED(cachingChannel->IsFromCache(&fromCache)) || !fromCache) {
        return false;
    }

    nsCOMPtr<nsISupports> cacheToken;
    cachingChannel->GetCacheToken(getter_AddRefs(cacheToken));
    if (!cacheToken) {
        return false;
    }

    nsCOMPtr<nsICacheEntry> cacheEntry =
        do_QueryInterface(cacheToken);
    if (!cacheEntry) {
        return false;
    }

    nsXPIDLCString tag;
    cacheEntry->GetMetaDataElement("necko:classified", getter_Copies(tag));
    return tag.EqualsLiteral("1");
}

nsresult
nsChannelClassifier::SetBlockedTrackingContent(nsIChannel *channel)
{
  nsresult rv;
  nsCOMPtr<nsIDOMWindow> win;
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
    do_GetService(THIRDPARTYUTIL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  rv = thirdPartyUtil->GetTopWindowForChannel(
    mSuspendedChannel, getter_AddRefs(win));
  NS_ENSURE_SUCCESS(rv, NS_OK);
  nsCOMPtr<nsPIDOMWindow> pwin = do_QueryInterface(win, &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
  if (!docShell) {
    return NS_OK;
  }
  nsCOMPtr<nsIDocument> doc = do_GetInterface(docShell, &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  // Notify nsIWebProgressListeners of this security event.
  // Can be used to change the UI state.
  nsCOMPtr<nsISecurityEventSink> eventSink = do_QueryInterface(docShell, &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  uint32_t state = 0;
  nsCOMPtr<nsISecureBrowserUI> securityUI;
  docShell->GetSecurityUI(getter_AddRefs(securityUI));
  if (!securityUI) {
    return NS_OK;
  }
  doc->SetHasTrackingContentBlocked(true);
  securityUI->GetState(&state);
  state |= nsIWebProgressListener::STATE_BLOCKED_TRACKING_CONTENT;
  eventSink->OnSecurityChange(nullptr, state);

  return NS_OK;
}

NS_IMETHODIMP
nsChannelClassifier::OnClassifyComplete(nsresult aErrorCode)
{
    if (mSuspendedChannel) {
        MarkEntryClassified(aErrorCode);

        if (NS_FAILED(aErrorCode)) {
#ifdef DEBUG
            nsCOMPtr<nsIURI> uri;
            mSuspendedChannel->GetURI(getter_AddRefs(uri));
            nsCString spec;
            uri->GetSpec(spec);
            LOG(("nsChannelClassifier[%p]: cancelling channel %p for %s "
                 "with error code: %x", this, mSuspendedChannel.get(),
                 spec.get(), aErrorCode));
#endif

            // Channel will be cancelled (page element blocked) due to tracking.
            // Do update the security state of the document and fire a security
            // change event.
            if (aErrorCode == NS_ERROR_TRACKING_URI) {
              SetBlockedTrackingContent(mSuspendedChannel);
            }

            mSuspendedChannel->Cancel(aErrorCode);
          }
#ifdef DEBUG
        LOG(("nsChannelClassifier[%p]: resuming channel %p from "
             "OnClassifyComplete", this, mSuspendedChannel.get()));
#endif
        mSuspendedChannel->Resume();
        mSuspendedChannel = nullptr;
    }

    return NS_OK;
}
