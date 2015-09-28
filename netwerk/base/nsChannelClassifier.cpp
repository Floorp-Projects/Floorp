/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsChannelClassifier.h"

#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsICacheEntry.h"
#include "nsICachingChannel.h"
#include "nsIChannel.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIHttpChannelInternal.h"
#include "nsIIOService.h"
#include "nsIParentChannel.h"
#include "nsIPermissionManager.h"
#include "nsIPrivateBrowsingTrackingProtectionWhitelist.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsISecureBrowserUI.h"
#include "nsISecurityEventSink.h"
#include "nsIURL.h"
#include "nsIWebProgressListener.h"
#include "nsPIDOMWindow.h"
#include "nsXULAppAPI.h"

#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"

using mozilla::ArrayLength;
using mozilla::Preferences;

//
// NSPR_LOG_MODULES=nsChannelClassifier:5
//
static PRLogModuleInfo *gChannelClassifierLog;
#undef LOG
#define LOG(args)     MOZ_LOG(gChannelClassifierLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gChannelClassifierLog, mozilla::LogLevel::Debug)

NS_IMPL_ISUPPORTS(nsChannelClassifier,
                  nsIURIClassifierCallback)

nsChannelClassifier::nsChannelClassifier()
  : mIsAllowListed(false),
    mSuspendedChannel(false)
{
    if (!gChannelClassifierLog)
        gChannelClassifierLog = PR_NewLogModule("nsChannelClassifier");
}

nsresult
nsChannelClassifier::ShouldEnableTrackingProtection(nsIChannel *aChannel,
                                                    bool *result)
{
    // Should only be called in the parent process.
    MOZ_ASSERT(XRE_IsParentProcess());

    NS_ENSURE_ARG(result);
    *result = false;

    if (!Preferences::GetBool("privacy.trackingprotection.enabled", false) &&
        (!Preferences::GetBool("privacy.trackingprotection.pbmode.enabled",
                               false) || !NS_UsePrivateBrowsing(aChannel))) {
      return NS_OK;
    }

    nsresult rv;
    nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
        do_GetService(THIRDPARTYUTIL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIHttpChannelInternal> chan = do_QueryInterface(aChannel, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> topWinURI;
    rv = chan->GetTopWindowURI(getter_AddRefs(topWinURI));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!topWinURI) {
      LOG(("nsChannelClassifier[%p]: No window URI\n", this));
    }

    nsCOMPtr<nsIURI> chanURI;
    rv = aChannel->GetURI(getter_AddRefs(chanURI));
    NS_ENSURE_SUCCESS(rv, rv);

    // Third party checks don't work for chrome:// URIs in mochitests, so just
    // default to isThirdParty = true. We check isThirdPartyWindow to expand
    // the list of domains that are considered first party (e.g., if
    // facebook.com includes an iframe from fatratgames.com, all subsources
    // included in that iframe are considered third-party with
    // isThirdPartyChannel, even if they are not third-party w.r.t.
    // facebook.com), and isThirdPartyChannel to prevent top-level navigations
    // from being detected as third-party.
    bool isThirdPartyChannel = true;
    bool isThirdPartyWindow = true;
    thirdPartyUtil->IsThirdPartyURI(chanURI, topWinURI, &isThirdPartyWindow);
    thirdPartyUtil->IsThirdPartyChannel(aChannel, nullptr, &isThirdPartyChannel);
    if (!isThirdPartyWindow || !isThirdPartyChannel) {
      *result = false;
      if (LOG_ENABLED()) {
        nsAutoCString spec;
        chanURI->GetSpec(spec);
        LOG(("nsChannelClassifier[%p]: Skipping tracking protection checks "
             "for first party or top-level load channel[%p] with uri %s",
             this, aChannel, spec.get()));
      }
      return NS_OK;
    }

    nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    const char ALLOWLIST_EXAMPLE_PREF[] = "channelclassifier.allowlist_example";
    if (!topWinURI && Preferences::GetBool(ALLOWLIST_EXAMPLE_PREF, false)) {
      LOG(("nsChannelClassifier[%p]: Allowlisting test domain\n", this));
      rv = ios->NewURI(NS_LITERAL_CSTRING("http://allowlisted.example.com"),
                       nullptr, nullptr, getter_AddRefs(topWinURI));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Take the host/port portion so we can allowlist by site. Also ignore the
    // scheme, since users who put sites on the allowlist probably don't expect
    // allowlisting to depend on scheme.
    nsCOMPtr<nsIURL> url = do_QueryInterface(topWinURI, &rv);
    if (NS_FAILED(rv)) {
      return rv; // normal for some loads, no need to print a warning
    }

    nsCString escaped(NS_LITERAL_CSTRING("https://"));
    nsAutoCString temp;
    rv = url->GetHostPort(temp);
    NS_ENSURE_SUCCESS(rv, rv);
    escaped.Append(temp);

    // Stuff the whole thing back into a URI for the permission manager.
    rv = ios->NewURI(escaped, nullptr, nullptr, getter_AddRefs(topWinURI));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPermissionManager> permMgr =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t permissions = nsIPermissionManager::UNKNOWN_ACTION;
    rv = permMgr->TestPermission(topWinURI, "trackingprotection", &permissions);
    NS_ENSURE_SUCCESS(rv, rv);

    if (permissions == nsIPermissionManager::ALLOW_ACTION) {
      LOG(("nsChannelClassifier[%p]: Allowlisting channel[%p] for %s", this,
           aChannel, escaped.get()));
      mIsAllowListed = true;
      *result = false;
    } else {
      *result = true;
    }

    // In Private Browsing Mode we also check against an in-memory list.
    if (NS_UsePrivateBrowsing(aChannel)) {
      nsCOMPtr<nsIPrivateBrowsingTrackingProtectionWhitelist> pbmtpWhitelist =
          do_GetService(NS_PBTRACKINGPROTECTIONWHITELIST_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      bool exists = false;
      rv = pbmtpWhitelist->ExistsInAllowList(topWinURI, &exists);
      NS_ENSURE_SUCCESS(rv, rv);

      if (exists) {
        mIsAllowListed = true;
        LOG(("nsChannelClassifier[%p]: Allowlisting channel[%p] in PBM for %s",
             this, aChannel, escaped.get()));
      }

      *result = !exists;
    }

    // Tracking protection will be enabled so return without updating
    // the security state. If any channels are subsequently cancelled
    // (page elements blocked) the state will be then updated.
    if (*result) {
      if (LOG_ENABLED()) {
        nsAutoCString topspec, spec;
        topWinURI->GetSpec(topspec);
        chanURI->GetSpec(spec);
        LOG(("nsChannelClassifier[%p]: Enabling tracking protection checks on "
             "channel[%p] with uri %s for toplevel window %s", this, aChannel,
             spec.get(), topspec.get()));
      }
      return NS_OK;
    }

    // Tracking protection will be disabled so update the security state
    // of the document and fire a secure change event. If we can't get the
    // window for the channel, then the shield won't show up so we can't send
    // an event to the securityUI anyway.
    return NotifyTrackingProtectionDisabled(aChannel);
}

// static
nsresult
nsChannelClassifier::NotifyTrackingProtectionDisabled(nsIChannel *aChannel)
{
    // Can be called in EITHER the parent or child process.
    nsCOMPtr<nsIParentChannel> parentChannel;
    NS_QueryNotificationCallbacks(aChannel, parentChannel);
    if (parentChannel) {
      // This channel is a parent-process proxy for a child process request.
      // Tell the child process channel to do this instead.
      parentChannel->NotifyTrackingProtectionDisabled();
      return NS_OK;
    }

    nsresult rv;
    nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
        do_GetService(THIRDPARTYUTIL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMWindow> win;
    rv = thirdPartyUtil->GetTopWindowForChannel(aChannel, getter_AddRefs(win));
    NS_ENSURE_SUCCESS(rv, rv);

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

void
nsChannelClassifier::Start(nsIChannel *aChannel)
{
  mChannel = aChannel;

  nsresult rv = StartInternal();
  if (NS_FAILED(rv)) {
    // If we aren't getting a callback for any reason, assume a good verdict and
    // make sure we resume the channel if necessary.
    OnClassifyComplete(NS_OK);
  }
}

nsresult
nsChannelClassifier::StartInternal()
{
    // Should only be called in the parent process.
    MOZ_ASSERT(XRE_IsParentProcess());

    // Don't bother to run the classifier on a load that has already failed.
    // (this might happen after a redirect)
    nsresult status;
    mChannel->GetStatus(&status);
    if (NS_FAILED(status))
        return status;

    // Don't bother to run the classifier on a cached load that was
    // previously classified as good.
    if (HasBeenClassified(mChannel)) {
        return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = mChannel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    // Don't bother checking certain types of URIs.
    bool hasFlags;
    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_DANGEROUS_TO_LOAD,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_ERROR_UNEXPECTED;

    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_IS_LOCAL_FILE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_ERROR_UNEXPECTED;

    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_IS_UI_RESOURCE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_ERROR_UNEXPECTED;

    rv = NS_URIChainHasFlags(uri,
                             nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                             &hasFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasFlags) return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIURIClassifier> uriClassifier =
        do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
    if (rv == NS_ERROR_FACTORY_NOT_REGISTERED ||
        rv == NS_ERROR_NOT_AVAILABLE) {
        // no URI classifier, ignore this failure.
        return NS_ERROR_NOT_AVAILABLE;
    }
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> principal;
    rv = securityManager->GetChannelURIPrincipal(mChannel, getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    bool expectCallback;
    bool trackingProtectionEnabled = false;
    (void)ShouldEnableTrackingProtection(mChannel, &trackingProtectionEnabled);

    if (LOG_ENABLED()) {
      nsAutoCString uriSpec, principalSpec;
      uri->GetSpec(uriSpec);
      nsCOMPtr<nsIURI> principalURI;
      principal->GetURI(getter_AddRefs(principalURI));
      principalURI->GetSpec(principalSpec);
      LOG(("nsChannelClassifier[%p]: Classifying principal %s on channel with "
           "uri %s", this, principalSpec.get(), uriSpec.get()));
    }
    rv = uriClassifier->Classify(principal, trackingProtectionEnabled, this,
                                 &expectCallback);
    if (NS_FAILED(rv)) {
        return rv;
    }

    if (expectCallback) {
        // Suspend the channel, it will be resumed when we get the classifier
        // callback.
        rv = mChannel->Suspend();
        if (NS_FAILED(rv)) {
            // Some channels (including nsJSChannel) fail on Suspend.  This
            // shouldn't be fatal, but will prevent malware from being
            // blocked on these channels.
            LOG(("nsChannelClassifier[%p]: Couldn't suspend channel", this));
            return rv;
        }

        mSuspendedChannel = true;
        LOG(("nsChannelClassifier[%p]: suspended channel %p",
             this, mChannel.get()));
    } else {
        LOG(("nsChannelClassifier[%p]: not expecting callback", this));
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

// Note in the cache entry that this URL was classified, so that future
// cached loads don't need to be checked.
void
nsChannelClassifier::MarkEntryClassified(nsresult status)
{
    // Should only be called in the parent process.
    MOZ_ASSERT(XRE_IsParentProcess());

    // Don't cache tracking classifications because we support allowlisting.
    if (status == NS_ERROR_TRACKING_URI || mIsAllowListed) {
        return;
    }

    nsCOMPtr<nsICachingChannel> cachingChannel = do_QueryInterface(mChannel);
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
    // Should only be called in the parent process.
    MOZ_ASSERT(XRE_IsParentProcess());

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

//static
bool
nsChannelClassifier::SameLoadingURI(nsIDocument *aDoc, nsIChannel *aChannel)
{
  nsCOMPtr<nsIURI> docURI = aDoc->GetDocumentURI();
  nsCOMPtr<nsILoadInfo> channelLoadInfo = aChannel->GetLoadInfo();
  if (!channelLoadInfo || !docURI) {
    return false;
  }
  nsCOMPtr<nsIPrincipal> channelLoadingPrincipal = channelLoadInfo->LoadingPrincipal();
  if (!channelLoadingPrincipal) {
    return false;
  }
  nsCOMPtr<nsIURI> channelLoadingURI;
  channelLoadingPrincipal->GetURI(getter_AddRefs(channelLoadingURI));
  if (!channelLoadingURI) {
    return false;
  }
  bool equals = false;
  nsresult rv = docURI->EqualsExceptRef(channelLoadingURI, &equals);
  return NS_SUCCEEDED(rv) && equals;
}

// static
nsresult
nsChannelClassifier::SetBlockedTrackingContent(nsIChannel *channel)
{
  // Can be called in EITHER the parent or child process.
  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(channel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process request. The
    // actual channel will be notified via the status passed to
    // nsIRequest::Cancel and do this for us.
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIDOMWindow> win;
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
    do_GetService(THIRDPARTYUTIL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  rv = thirdPartyUtil->GetTopWindowForChannel(channel, getter_AddRefs(win));
  NS_ENSURE_SUCCESS(rv, NS_OK);
  nsCOMPtr<nsPIDOMWindow> pwin = do_QueryInterface(win, &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
  if (!docShell) {
    return NS_OK;
  }
  nsCOMPtr<nsIDocument> doc = do_GetInterface(docShell, &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  // This event might come after the user has navigated to another page.
  // To prevent showing the TrackingProtection UI on the wrong page, we need to
  // check that the loading URI for the channel is the same as the URI currently
  // loaded in the document.
  if (!SameLoadingURI(doc, channel)) {
    return NS_OK;
  }

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

  // Log a warning to the web console.
  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));
  nsCString utf8spec;
  uri->GetSpec(utf8spec);
  NS_ConvertUTF8toUTF16 spec(utf8spec);
  const char16_t* params[] = { spec.get() };
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Tracking Protection"),
                                  doc,
                                  nsContentUtils::eNECKO_PROPERTIES,
                                  "TrackingUriBlocked",
                                  params, ArrayLength(params));

  return NS_OK;
}

nsresult
nsChannelClassifier::IsTrackerWhitelisted()
{
  nsresult rv;
  nsCOMPtr<nsIURIClassifier> uriClassifier =
    do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString tables;
  Preferences::GetCString("urlclassifier.trackingWhitelistTable", &tables);

  if (tables.IsEmpty()) {
    LOG(("nsChannelClassifier[%p]:IsTrackerWhitelisted whitelist disabled",
         this));
    return NS_ERROR_TRACKING_URI;
  }

  nsCOMPtr<nsIHttpChannelInternal> chan = do_QueryInterface(mChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> topWinURI;
  rv = chan->GetTopWindowURI(getter_AddRefs(topWinURI));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!topWinURI) {
    LOG(("nsChannelClassifier[%p]: No window URI", this));
    return NS_ERROR_TRACKING_URI;
  }

  nsCOMPtr<nsIScriptSecurityManager> securityManager =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> chanPrincipal;
  rv = securityManager->GetChannelURIPrincipal(mChannel,
                                               getter_AddRefs(chanPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Craft a whitelist URL like "toplevel.page/?resource=third.party.domain"
  nsAutoCString pageHostname, resourceDomain;
  rv = topWinURI->GetHost(pageHostname);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = chanPrincipal->GetBaseDomain(resourceDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString whitelistEntry = NS_LITERAL_CSTRING("http://") +
    pageHostname + NS_LITERAL_CSTRING("/?resource=") + resourceDomain;
  LOG(("nsChannelClassifier[%p]: Looking for %s in the whitelist",
       this, whitelistEntry.get()));

  nsCOMPtr<nsIURI> whitelistURI;
  rv = NS_NewURI(getter_AddRefs(whitelistURI), whitelistEntry);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check whether or not the tracker is in the entity whitelist
  nsAutoCString results;
  rv = uriClassifier->ClassifyLocalWithTables(whitelistURI, tables, results);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!results.IsEmpty()) {
    return NS_OK; // found it on the whitelist, must not be blocked
  }

  LOG(("nsChannelClassifier[%p]: %s is not in the whitelist",
       this, whitelistEntry.get()));
  return NS_ERROR_TRACKING_URI;
}

NS_IMETHODIMP
nsChannelClassifier::OnClassifyComplete(nsresult aErrorCode)
{
    // Should only be called in the parent process.
    MOZ_ASSERT(XRE_IsParentProcess());

    if (aErrorCode == NS_ERROR_TRACKING_URI &&
        NS_SUCCEEDED(IsTrackerWhitelisted())) {
      LOG(("nsChannelClassifier[%p]:OnClassifyComplete tracker found "
           "in whitelist so we won't block it", this));
      aErrorCode = NS_OK;
    }

    if (mSuspendedChannel) {
      nsAutoCString errorName;
      if (LOG_ENABLED()) {
        mozilla::GetErrorName(aErrorCode, errorName);
        LOG(("nsChannelClassifier[%p]:OnClassifyComplete %s (suspended channel)",
             this, errorName.get()));
      }
      MarkEntryClassified(aErrorCode);

      if (NS_FAILED(aErrorCode)) {
        if (LOG_ENABLED()) {
          nsCOMPtr<nsIURI> uri;
          mChannel->GetURI(getter_AddRefs(uri));
          nsAutoCString spec;
          uri->GetSpec(spec);
          LOG(("nsChannelClassifier[%p]: cancelling channel %p for %s "
               "with error code %s", this, mChannel.get(),
               spec.get(), errorName.get()));
        }

        // Channel will be cancelled (page element blocked) due to tracking.
        // Do update the security state of the document and fire a security
        // change event.
        if (aErrorCode == NS_ERROR_TRACKING_URI) {
          SetBlockedTrackingContent(mChannel);
        }

        mChannel->Cancel(aErrorCode);
      }
      LOG(("nsChannelClassifier[%p]: resuming channel %p from "
           "OnClassifyComplete", this, mChannel.get()));
      mChannel->Resume();
    }

    mChannel = nullptr;

    return NS_OK;
}
