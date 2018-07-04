/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsChannelClassifier.h"

#include "mozIThirdPartyUtil.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsIAddonPolicyService.h"
#include "nsICacheEntry.h"
#include "nsICachingChannel.h"
#include "nsIChannel.h"
#include "nsIClassOfService.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIHttpChannel.h"
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
#include "nsISupportsPriority.h"
#include "nsIURL.h"
#include "nsIWebProgressListener.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsXULAppAPI.h"
#include "nsQueryObject.h"
#include "nsIUrlClassifierDBService.h"
#include "nsIURLFormatter.h"

#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace net {

//
// MOZ_LOG=nsChannelClassifier:5
//
static LazyLogModule gChannelClassifierLog("nsChannelClassifier");


#undef LOG
#define LOG(args)     MOZ_LOG(gChannelClassifierLog, LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gChannelClassifierLog, LogLevel::Debug)

#define URLCLASSIFIER_SKIP_HOSTNAMES       "urlclassifier.skipHostnames"
#define URLCLASSIFIER_TRACKING_WHITELIST   "urlclassifier.trackingWhitelistTable"
#define URLCLASSIFIER_TRACKING_TABLE       "urlclassifier.trackingTable"

// Put CachedPrefs in anonymous namespace to avoid any collision from outside of
// this file.
namespace {

/**
 * It is not recommended to read from Preference everytime a channel is
 * connected.
 * That is not fast and we should cache preference values and reuse them
 */
class CachedPrefs final
{
public:
  static CachedPrefs* GetInstance();

  void Init();
  bool IsAllowListExample() { return sAllowListExample;}
  bool IsLowerNetworkPriority() { return sLowerNetworkPriority;}
  bool IsAnnotateChannelEnabled() { return sAnnotateChannelEnabled;}
  nsCString GetTrackingWhiteList() { return mTrackingWhitelist; }
  void SetTrackingWhiteList(const nsACString& aList) { mTrackingWhitelist = aList; }
  nsCString GetSkipHostnames() { return mSkipHostnames; }
  void SetSkipHostnames(const nsACString& aHostnames) { mSkipHostnames = aHostnames; }
  void SetTrackingBlackList(const nsACString& aList) { mTrackingBlacklist = aList; }
  nsCString GetTrackingBlackList() { return mTrackingBlacklist; }

private:
  friend class StaticAutoPtr<CachedPrefs>;
  CachedPrefs();
  ~CachedPrefs();

  static void OnPrefsChange(const char* aPrefName, void* );

  // Whether channels should be annotated as being on the tracking protection
  // list.
  static bool sAnnotateChannelEnabled;
  // Whether the priority of the channels annotated as being on the tracking
  // protection list should be lowered.
  static bool sLowerNetworkPriority;
  static bool sAllowListExample;

  nsCString mTrackingWhitelist;
  nsCString mSkipHostnames;
  nsCString mTrackingBlacklist;

  static StaticAutoPtr<CachedPrefs> sInstance;
};

bool CachedPrefs::sAllowListExample = false;
bool CachedPrefs::sLowerNetworkPriority = false;
bool CachedPrefs::sAnnotateChannelEnabled = false;

StaticAutoPtr<CachedPrefs> CachedPrefs::sInstance;

// static
void
CachedPrefs::OnPrefsChange(const char* aPref, void* aClosure)
{
  CachedPrefs* prefs = static_cast<CachedPrefs*> (aClosure);

  if (!strcmp(aPref, URLCLASSIFIER_SKIP_HOSTNAMES)) {
    nsCString skipHostnames;
    Preferences::GetCString(URLCLASSIFIER_SKIP_HOSTNAMES, skipHostnames);
    ToLowerCase(skipHostnames);
    prefs->SetSkipHostnames(skipHostnames);
  } else if (!strcmp(aPref, URLCLASSIFIER_TRACKING_WHITELIST)) {
    nsCString trackingWhitelist;
    Preferences::GetCString(URLCLASSIFIER_TRACKING_WHITELIST,
                            trackingWhitelist);
    prefs->SetTrackingWhiteList(trackingWhitelist);
  } else if (!strcmp(aPref, URLCLASSIFIER_TRACKING_TABLE)) {
    nsCString trackingBlacklist;
    Preferences::GetCString(URLCLASSIFIER_TRACKING_TABLE, trackingBlacklist);
    prefs->SetTrackingBlackList(trackingBlacklist);
  }
}

void
CachedPrefs::Init()
{
  Preferences::AddBoolVarCache(&sAnnotateChannelEnabled,
                               "privacy.trackingprotection.annotate_channels");
  Preferences::AddBoolVarCache(&sLowerNetworkPriority,
                               "privacy.trackingprotection.lower_network_priority");
  Preferences::AddBoolVarCache(&sAllowListExample,
                               "channelclassifier.allowlist_example");
  Preferences::RegisterCallbackAndCall(CachedPrefs::OnPrefsChange,
                                       URLCLASSIFIER_SKIP_HOSTNAMES, this);
  Preferences::RegisterCallbackAndCall(CachedPrefs::OnPrefsChange,
                                       URLCLASSIFIER_TRACKING_WHITELIST, this);
  Preferences::RegisterCallbackAndCall(CachedPrefs::OnPrefsChange,
                                       URLCLASSIFIER_TRACKING_TABLE, this);

}

// static
CachedPrefs*
CachedPrefs::GetInstance()
{
  if (!sInstance) {
    sInstance = new CachedPrefs();
    sInstance->Init();
    ClearOnShutdown(&sInstance);
  }
  MOZ_ASSERT(sInstance);
  return sInstance;
}

CachedPrefs::CachedPrefs()
{
  MOZ_COUNT_CTOR(CachedPrefs);
}

CachedPrefs::~CachedPrefs()
{
  MOZ_COUNT_DTOR(CachedPrefs);

  Preferences::UnregisterCallback(CachedPrefs::OnPrefsChange, URLCLASSIFIER_SKIP_HOSTNAMES, this);
  Preferences::UnregisterCallback(CachedPrefs::OnPrefsChange, URLCLASSIFIER_TRACKING_WHITELIST, this);
  Preferences::UnregisterCallback(CachedPrefs::OnPrefsChange, URLCLASSIFIER_TRACKING_TABLE, this);
}
} // anonymous namespace

static void
SetIsTrackingResourceHelper(nsIChannel* aChannel)
{
  MOZ_ASSERT(aChannel);

  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(aChannel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process
    // request. We should notify the child process as well.
    parentChannel->NotifyTrackingResource();
  }

  RefPtr<HttpBaseChannel> httpChannel = do_QueryObject(aChannel);
  if (httpChannel) {
    httpChannel->SetIsTrackingResource();
  }
}

static void
LowerPriorityHelper(nsIChannel* aChannel)
{
  MOZ_ASSERT(aChannel);

  bool isBlockingResource = false;

  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(aChannel));
  if (cos) {
    if (nsContentUtils::IsTailingEnabled()) {
      uint32_t cosFlags = 0;
      cos->GetClassFlags(&cosFlags);
      isBlockingResource = cosFlags & (nsIClassOfService::UrgentStart |
                                       nsIClassOfService::Leader |
                                       nsIClassOfService::Unblocked);

      // Requests not allowed to be tailed are usually those with higher
      // prioritization.  That overweights being a tracker: don't throttle
      // them when not in background.
      if (!(cosFlags & nsIClassOfService::TailForbidden)) {
        cos->AddClassFlags(nsIClassOfService::Throttleable);
      }
    } else {
      // Yes, we even don't want to evaluate the isBlockingResource when tailing is off
      // see bug 1395525.

      cos->AddClassFlags(nsIClassOfService::Throttleable);
    }
  }

  if (!isBlockingResource) {
    nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(aChannel);
    if (p) {
      p->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
    }
  }
}

NS_IMPL_ISUPPORTS(nsChannelClassifier,
                  nsIURIClassifierCallback,
                  nsIObserver)

nsChannelClassifier::nsChannelClassifier(nsIChannel *aChannel)
  : mIsAllowListed(false),
    mSuspendedChannel(false),
    mChannel(aChannel),
    mTrackingProtectionEnabled(Nothing()),
    mTrackingAnnotationEnabled(Nothing())
{
  LOG(("nsChannelClassifier::nsChannelClassifier %p", this));
  MOZ_ASSERT(mChannel);
}

nsChannelClassifier::~nsChannelClassifier()
{
  LOG(("nsChannelClassifier::~nsChannelClassifier %p", this));
}

bool
nsChannelClassifier::ShouldEnableTrackingProtection()
{
  if (mTrackingProtectionEnabled) {
    return mTrackingProtectionEnabled.value();
  }

  mTrackingProtectionEnabled = Some(false);

  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(mChannel, loadContext);
  if (loadContext && loadContext->UseTrackingProtection()) {
    Unused << ShouldEnableTrackingProtectionInternal(
      mChannel, false, mTrackingProtectionEnabled.ptr());
  }

  return mTrackingProtectionEnabled.value();
}

bool
nsChannelClassifier::ShouldEnableTrackingAnnotation()
{
  if (mTrackingAnnotationEnabled) {
    return mTrackingAnnotationEnabled.value();
  }

  mTrackingAnnotationEnabled = Some(false);

  if (!CachedPrefs::GetInstance()->IsAnnotateChannelEnabled()) {
    return mTrackingAnnotationEnabled.value();
  }

  // If tracking protection is enabled, no need to do channel annotation.
  if (ShouldEnableTrackingProtection()) {
    return mTrackingAnnotationEnabled.value();
  }

  // To prevent calling ShouldEnableTrackingProtectionInternal() again,
  // check loadContext->UseTrackingProtection() here.
  // If loadContext->UseTrackingProtection() is true, here it means
  // ShouldEnableTrackingProtectionInternal() has been called before in
  // ShouldEnableTrackingProtection() above and the result is false.
  // So, we can just return false.
  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(mChannel, loadContext);
  if (loadContext && loadContext->UseTrackingProtection()) {
    return mTrackingAnnotationEnabled.value();
  }

  Unused << ShouldEnableTrackingProtectionInternal(
      mChannel, true, mTrackingAnnotationEnabled.ptr());

  return mTrackingAnnotationEnabled.value();
}

nsresult
nsChannelClassifier::ShouldEnableTrackingProtectionInternal(
                                                         nsIChannel *aChannel,
                                                         bool aAnnotationsOnly,
                                                         bool *result)
{
    // Should only be called in the parent process.
    MOZ_ASSERT(XRE_IsParentProcess());

    NS_ENSURE_ARG(result);
    *result = false;

    nsresult rv;
    nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
        do_GetService(THIRDPARTYUTIL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIHttpChannelInternal> chan = do_QueryInterface(aChannel, &rv);
    if (NS_FAILED(rv) || !chan) {
      LOG(("nsChannelClassifier[%p]: Not an HTTP channel", this));
      return NS_OK;
    }

    nsCOMPtr<nsIURI> topWinURI;
    rv = chan->GetTopWindowURI(getter_AddRefs(topWinURI));
    if (NS_FAILED(rv)) {
      return rv;
    }

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
        LOG(("nsChannelClassifier[%p]: Skipping tracking protection checks "
             "for first party or top-level load channel[%p] with uri %s",
             this, aChannel, chanURI->GetSpecOrDefault().get()));
      }
      return NS_OK;
    }

    if (AddonMayLoad(aChannel, chanURI)) {
        return NS_OK;
    }

    nsCOMPtr<nsIIOService> ios = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!topWinURI && CachedPrefs::GetInstance()->IsAllowListExample()) {
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
        LOG(("nsChannelClassifier[%p]: Enabling tracking protection checks on "
             "channel[%p] with uri %s for toplevel window %s", this, aChannel,
             chanURI->GetSpecOrDefault().get(),
             topWinURI->GetSpecOrDefault().get()));
      }
      return NS_OK;
    }

    // Tracking protection will be disabled so update the security state
    // of the document and fire a secure change event. If we can't get the
    // window for the channel, then the shield won't show up so we can't send
    // an event to the securityUI anyway.
    return NotifyTrackingProtectionDisabled(aChannel);
}

bool
nsChannelClassifier::AddonMayLoad(nsIChannel *aChannel, nsIURI *aUri)
{
    nsCOMPtr<nsILoadInfo> channelLoadInfo = aChannel->GetLoadInfo();
    if (!channelLoadInfo)
        return false;

    // loadingPrincipal is used here to ensure we are loading into an
    // addon principal.  This allows an addon, with explicit permission, to
    // call out to API endpoints that may otherwise get blocked.
    nsIPrincipal* loadingPrincipal = channelLoadInfo->LoadingPrincipal();
    if (!loadingPrincipal)
        return false;

    return BasePrincipal::Cast(loadingPrincipal)->AddonAllowsLoad(aUri, true);
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

    nsCOMPtr<mozIDOMWindowProxy> win;
    rv = thirdPartyUtil->GetTopWindowForChannel(aChannel, getter_AddRefs(win));
    NS_ENSURE_SUCCESS(rv, rv);

    auto* pwin = nsPIDOMWindowOuter::From(win);
    nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
    if (!docShell) {
      return NS_OK;
    }
    nsCOMPtr<nsIDocument> doc = docShell->GetDocument();
    NS_ENSURE_TRUE(doc, NS_OK);

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
nsChannelClassifier::Start()
{
  nsresult rv = StartInternal();
  if (NS_FAILED(rv)) {
    // If we aren't getting a callback for any reason, assume a good verdict and
    // make sure we resume the channel if necessary.
    OnClassifyComplete(NS_OK, NS_LITERAL_CSTRING(""),NS_LITERAL_CSTRING(""),
                       NS_LITERAL_CSTRING(""));
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
    bool isAbout = false;
    rv = uri->SchemeIs("about", &isAbout);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isAbout) return NS_ERROR_UNEXPECTED;

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

    nsCString skipHostnames = CachedPrefs::GetInstance()->GetSkipHostnames();
    if (!skipHostnames.IsEmpty()) {
      LOG(("nsChannelClassifier[%p]:StartInternal whitelisted hostnames = %s",
           this, skipHostnames.get()));
      if (IsHostnameWhitelisted(uri, skipHostnames)) {
        return NS_ERROR_UNEXPECTED;
      }
    }

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
    if (LOG_ENABLED()) {
      nsCOMPtr<nsIURI> principalURI;
      principal->GetURI(getter_AddRefs(principalURI));
      LOG(("nsChannelClassifier[%p]: Classifying principal %s on channel with "
           "uri %s", this, principalURI->GetSpecOrDefault().get(),
           uri->GetSpecOrDefault().get()));
    }
    // The classify is running in parent process, no need to give a valid event
    // target
    rv = uriClassifier->Classify(principal, nullptr,
                                 false,
                                 this, &expectCallback);
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

    // Add an observer for shutdown
    AddShutdownObserver();
    return NS_OK;
}

bool
nsChannelClassifier::IsHostnameWhitelisted(nsIURI *aUri,
                                           const nsACString &aWhitelisted)
{
  nsAutoCString host;
  nsresult rv = aUri->GetHost(host);
  if (NS_FAILED(rv) || host.IsEmpty()) {
    return false;
  }
  ToLowerCase(host);

  nsCCharSeparatedTokenizer tokenizer(aWhitelisted, ',');
  while (tokenizer.hasMoreTokens()) {
    const nsACString& token = tokenizer.nextToken();
    if (token.Equals(host)) {
      LOG(("nsChannelClassifier[%p]:StartInternal skipping %s (whitelisted)",
           this, host.get()));
      return true;
    }
  }

  return false;
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

    if (LOG_ENABLED()) {
      nsAutoCString errorName;
      GetErrorName(status, errorName);
      nsCOMPtr<nsIURI> uri;
      mChannel->GetURI(getter_AddRefs(uri));
      nsAutoCString spec;
      uri->GetAsciiSpec(spec);
      LOG(("nsChannelClassifier::MarkEntryClassified[%s] %s",
           errorName.get(), spec.get()));
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

    nsCString tag;
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
    // TYPE_DOCUMENT loads will not have a channelLoadingPrincipal. But top level
    // loads should not be blocked by Tracking Protection, so we will return
    // false
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
nsChannelClassifier::SetBlockedContent(nsIChannel *channel,
                                       nsresult aErrorCode,
                                       const nsACString& aList,
                                       const nsACString& aProvider,
                                       const nsACString& aFullHash)
{
  NS_ENSURE_ARG(!aList.IsEmpty());

  // Can be called in EITHER the parent or child process.
  nsCOMPtr<nsIParentChannel> parentChannel;
  NS_QueryNotificationCallbacks(channel, parentChannel);
  if (parentChannel) {
    // This channel is a parent-process proxy for a child process request.
    // Tell the child process channel to do this instead.
    parentChannel->SetClassifierMatchedInfo(aList, aProvider, aFullHash);
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIClassifiedChannel> classifiedChannel = do_QueryInterface(channel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (classifiedChannel) {
    classifiedChannel->SetMatchedInfo(aList, aProvider, aFullHash);
  }

  nsCOMPtr<mozIDOMWindowProxy> win;
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
    do_GetService(THIRDPARTYUTIL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  rv = thirdPartyUtil->GetTopWindowForChannel(channel, getter_AddRefs(win));
  NS_ENSURE_SUCCESS(rv, NS_OK);
  auto* pwin = nsPIDOMWindowOuter::From(win);
  nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
  if (!docShell) {
    return NS_OK;
  }
  nsCOMPtr<nsIDocument> doc = docShell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_OK);

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
  securityUI->GetState(&state);
  if (aErrorCode == NS_ERROR_TRACKING_URI) {
    doc->SetHasTrackingContentBlocked(true);
    state |= nsIWebProgressListener::STATE_BLOCKED_TRACKING_CONTENT;
  } else {
    state |= nsIWebProgressListener::STATE_BLOCKED_UNSAFE_CONTENT;
  }

  eventSink->OnSecurityChange(channel, state);

  // Log a warning to the web console.
  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));
  NS_ConvertUTF8toUTF16 spec(uri->GetSpecOrDefault());
  const char16_t* params[] = { spec.get() };
  const char* message = (aErrorCode == NS_ERROR_TRACKING_URI) ?
    "TrackingUriBlocked" : "UnsafeUriBlocked";
  nsCString category = (aErrorCode == NS_ERROR_TRACKING_URI) ?
    NS_LITERAL_CSTRING("Tracking Protection") :
    NS_LITERAL_CSTRING("Safe Browsing");

  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  category,
                                  doc,
                                  nsContentUtils::eNECKO_PROPERTIES,
                                  message,
                                  params, ArrayLength(params));

  return NS_OK;
}

namespace {

// This class is designed to get the results of checking blacklist and whitelist.
// |mExpectWhitelistResult| is used to indicate that |OnClassifyComplete| is called
// for the result of blacklist or whitelist check.
class TrackingURICallback final : public nsIURIClassifierCallback {
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURICLASSIFIERCALLBACK

  explicit TrackingURICallback(nsChannelClassifier* aChannelClassifier,
                               std::function<void()>&& aCallback)
    : mChannelClassifier(aChannelClassifier)
    , mChannelCallback(std::move(aCallback))
    , mExpectWhitelistResult(false)
  {
    MOZ_ASSERT(mChannelClassifier);
  }

private:
  ~TrackingURICallback() = default;
  nsresult OnBlacklistResult(nsresult aErrorCode);
  nsresult OnWhitelistResult(nsresult aErrorCode);
  void OnTrackerFound(nsresult aErrorCode);

  RefPtr<nsChannelClassifier> mChannelClassifier;
  std::function<void()> mChannelCallback;
  bool mExpectWhitelistResult;

  // Only need to save the result of blacklist check.
  nsCString mList;
  nsCString mProvider;
  nsCString mFullHash;
};

NS_IMPL_ISUPPORTS(TrackingURICallback, nsIURIClassifierCallback)

/*virtual*/ nsresult
TrackingURICallback::OnClassifyComplete(nsresult aErrorCode,
                                        const nsACString& aLists,
                                        const nsACString& aProvider,
                                        const nsACString& aFullHash)
{
  if (!mExpectWhitelistResult) {
    nsresult status = aLists.IsEmpty() ? NS_OK : NS_ERROR_TRACKING_URI;
    mList = aLists;
    mProvider = aProvider;
    mFullHash = aFullHash;
    return OnBlacklistResult(status);
  }

  nsresult status = aLists.IsEmpty() ? NS_ERROR_TRACKING_URI : NS_OK;
  return OnWhitelistResult(status);
}

nsresult
TrackingURICallback::OnBlacklistResult(nsresult aErrorCode)
{
  LOG(("TrackingURICallback[%p]::OnBlacklistResult aErrorCode=0x%" PRIx32,
       mChannelClassifier.get(), static_cast<uint32_t>(aErrorCode)));

  if (NS_SUCCEEDED(aErrorCode)) {
    mChannelCallback();
    return NS_OK;
  }

  if (LOG_ENABLED()) {
    nsCOMPtr<nsIChannel> channel = mChannelClassifier->GetChannel();
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    LOG(("TrackingURICallback[%p]::OnBlacklistResult channel [%p] "
         "uri=%s, is in blacklist. Start checking whitelist.",
         mChannelClassifier.get(), channel.get(),
         uri->GetSpecOrDefault().get()));
  }

  nsCOMPtr<nsIURI> whitelistURI = mChannelClassifier->CreateWhiteListURI();

  // If IsTrackerWhitelisted has failed, it means the uri is not in whitelist.
  if (NS_FAILED(mChannelClassifier->IsTrackerWhitelisted(whitelistURI, this))) {
    LOG(("TrackingURICallback[%p]:OnBlacklistResult "
         "IsTrackerWhitelisted has failed.",
         mChannelClassifier.get()));

    OnTrackerFound(aErrorCode);
    mChannelCallback();
  } else {
    mExpectWhitelistResult = true;
  }

  return NS_OK;
}

nsresult
TrackingURICallback::OnWhitelistResult(nsresult aErrorCode)
{
  LOG(("TrackingURICallback[%p]::OnWhitelistResult aErrorCode=0x%" PRIx32,
       mChannelClassifier.get(), static_cast<uint32_t>(aErrorCode)));

  if (NS_SUCCEEDED(aErrorCode)) {
    LOG(("TrackingURICallback[%p]::OnWhitelistResult tracker found "
         "in whitelist so we won't block it", mChannelClassifier.get()));
    mChannelCallback();
    return NS_OK;
  }

  if (LOG_ENABLED()) {
    nsCOMPtr<nsIChannel> channel = mChannelClassifier->GetChannel();
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    LOG(("TrackingURICallback[%p]::OnWhitelistResult "
         "channel [%p] uri=%s, is not in whitelist",
         mChannelClassifier.get(), channel.get(),
         uri->GetSpecOrDefault().get()));
  }

  OnTrackerFound(aErrorCode);
  mChannelCallback();
  return NS_OK;
}

void
TrackingURICallback::OnTrackerFound(nsresult aErrorCode)
{
  nsCOMPtr<nsIChannel> channel = mChannelClassifier->GetChannel();
  if (mChannelClassifier->ShouldEnableTrackingProtection()) {
    mChannelClassifier->SetBlockedContent(channel, aErrorCode,
                                          mList, mProvider, mFullHash);
    channel->Cancel(aErrorCode);
  } else {
    MOZ_ASSERT(mChannelClassifier->ShouldEnableTrackingAnnotation());

    // Even with TP disabled, we still want to show the user that there
    // are unblocked trackers on the site, so notify the UI that we loaded
    // tracking content. UI code can treat this notification differently
    // depending on whether TP is enabled or disabled.
    mChannelClassifier->NotifyTrackingProtectionDisabled(channel);

    SetIsTrackingResourceHelper(channel);
    if (CachedPrefs::GetInstance()->IsLowerNetworkPriority()) {
      LowerPriorityHelper(channel);
    }
  }
}

} // end of unnamed namespace/

already_AddRefed<nsIURI>
nsChannelClassifier::CreateWhiteListURI() const
{
  nsresult rv;
  nsCOMPtr<nsIHttpChannelInternal> chan = do_QueryInterface(mChannel, &rv);
  if (!chan) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> topWinURI;
  rv = chan->GetTopWindowURI(getter_AddRefs(topWinURI));
  NS_ENSURE_SUCCESS(rv, nullptr);
  if (!topWinURI) {
    LOG(("nsChannelClassifier[%p]: No window URI", this));
    return nullptr;
  }

  nsCOMPtr<nsIScriptSecurityManager> securityManager =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);
  nsCOMPtr<nsIPrincipal> chanPrincipal;
  rv = securityManager->GetChannelURIPrincipal(mChannel,
                                               getter_AddRefs(chanPrincipal));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  // Craft a whitelist URL like "toplevel.page/?resource=third.party.domain"
  nsAutoCString pageHostname, resourceDomain;
  rv = topWinURI->GetHost(pageHostname);
  NS_ENSURE_SUCCESS(rv, nullptr);
  rv = chanPrincipal->GetBaseDomain(resourceDomain);
  NS_ENSURE_SUCCESS(rv, nullptr);
  nsAutoCString whitelistEntry = NS_LITERAL_CSTRING("http://") +
    pageHostname + NS_LITERAL_CSTRING("/?resource=") + resourceDomain;
  LOG(("nsChannelClassifier[%p]: Looking for %s in the whitelist",
       this, whitelistEntry.get()));

  nsCOMPtr<nsIURI> whitelistURI;
  rv = NS_NewURI(getter_AddRefs(whitelistURI), whitelistEntry);

  return NS_SUCCEEDED(rv) ? whitelistURI.forget() : nullptr;
}

nsresult
nsChannelClassifier::IsTrackerWhitelisted(nsIURI* aWhiteListURI,
                                          nsIURIClassifierCallback *aCallback)
{
  if (!aCallback || !aWhiteListURI) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsIURIClassifier> uriClassifier =
    do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString trackingWhitelist = CachedPrefs::GetInstance()->GetTrackingWhiteList();
  if (trackingWhitelist.IsEmpty()) {
    LOG(("nsChannelClassifier[%p]:IsTrackerWhitelisted whitelist disabled",
         this));
    return NS_ERROR_TRACKING_URI;
  }

  return uriClassifier->AsyncClassifyLocalWithTables(aWhiteListURI, trackingWhitelist, aCallback);
}

/* static */
nsresult
nsChannelClassifier::SendThreatHitReport(nsIChannel *aChannel,
                                         const nsACString& aProvider,
                                         const nsACString& aList,
                                         const nsACString& aFullHash)
{
  NS_ENSURE_ARG_POINTER(aChannel);

  nsAutoCString provider(aProvider);
  nsPrintfCString reportEnablePref("browser.safebrowsing.provider.%s.dataSharing.enabled",
                                   provider.get());
  if (!Preferences::GetBool(reportEnablePref.get(), false)) {
    LOG(("nsChannelClassifier::SendThreatHitReport data sharing disabled for %s",
         provider.get()));
    return NS_OK;
  }

  nsCOMPtr<nsIURIClassifier> uriClassifier =
    do_GetService(NS_URLCLASSIFIERDBSERVICE_CONTRACTID);
  if (!uriClassifier) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = uriClassifier->SendThreatHitReport(aChannel, aProvider, aList,
                                                   aFullHash);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsChannelClassifier::OnClassifyComplete(nsresult aErrorCode,
                                        const nsACString& aList,
                                        const nsACString& aProvider,
                                        const nsACString& aFullHash)
{
  // Should only be called in the parent process.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aErrorCode != NS_ERROR_TRACKING_URI);

  if (mSuspendedChannel) {
    nsAutoCString errorName;
    if (LOG_ENABLED()) {
      GetErrorName(aErrorCode, errorName);
      LOG(("nsChannelClassifier[%p]:OnClassifyComplete %s (suspended channel)",
           this, errorName.get()));
    }
    MarkEntryClassified(aErrorCode);

    if (NS_FAILED(aErrorCode)) {
      if (LOG_ENABLED()) {
        nsCOMPtr<nsIURI> uri;
        mChannel->GetURI(getter_AddRefs(uri));
        LOG(("nsChannelClassifier[%p]: cancelling channel %p for %s "
             "with error code %s", this, mChannel.get(),
              uri->GetSpecOrDefault().get(), errorName.get()));
      }

      // Channel will be cancelled (page element blocked) due to Safe Browsing.
      // Do update the security state of the document and fire a security
      // change event.
      SetBlockedContent(mChannel, aErrorCode, aList, aProvider, aFullHash);

      if (aErrorCode == NS_ERROR_MALWARE_URI ||
          aErrorCode == NS_ERROR_PHISHING_URI ||
          aErrorCode == NS_ERROR_UNWANTED_URI ||
          aErrorCode == NS_ERROR_HARMFUL_URI) {
        SendThreatHitReport(mChannel, aProvider, aList, aFullHash);
      }

      mChannel->Cancel(aErrorCode);
    }
    LOG(("nsChannelClassifier[%p]: resuming channel %p from "
         "OnClassifyComplete", this, mChannel.get()));
    mChannel->Resume();
  }

  mChannel = nullptr;
  RemoveShutdownObserver();

  return NS_OK;
}

nsresult
nsChannelClassifier::CheckIsTrackerWithLocalTable(std::function<void()>&& aCallback)
{
  nsresult rv;

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIURIClassifier> uriClassifier =
    do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!ShouldEnableTrackingProtection() && !ShouldEnableTrackingAnnotation()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> uri;
  rv = mChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv) || !uri) {
    return rv;
  }

  nsCString trackingBlacklist =
    CachedPrefs::GetInstance()->GetTrackingBlackList();
  if (trackingBlacklist.IsEmpty()) {
    LOG(("nsChannelClassifier[%p]:CheckIsTrackerWithLocalTable blacklist is empty",
         this));
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURIClassifierCallback> callback =
    new TrackingURICallback(this, std::move(aCallback));

  LOG(("nsChannelClassifier[%p]:CheckIsTrackerWithLocalTable for uri=%s\n",
       this, uri->GetSpecOrDefault().get()));
  return uriClassifier->AsyncClassifyLocalWithTables(uri,
                                                     trackingBlacklist,
                                                     callback);
}

already_AddRefed<nsIChannel>
nsChannelClassifier::GetChannel()
{
  nsCOMPtr<nsIChannel> channel = mChannel;
  return channel.forget();
}

void
nsChannelClassifier::AddShutdownObserver()
{
  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "profile-change-net-teardown", false);
  }
}

void
nsChannelClassifier::RemoveShutdownObserver()
{
  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, "profile-change-net-teardown");
  }
}

///////////////////////////////////////////////////////////////////////////////
// nsIObserver implementation
NS_IMETHODIMP
nsChannelClassifier::Observe(nsISupports *aSubject, const char *aTopic,
                             const char16_t *aData)
{
  if (!strcmp(aTopic, "profile-change-net-teardown")) {
    // If we aren't getting a callback for any reason, make sure
    // we resume the channel.

    if (mChannel && mSuspendedChannel) {
      mSuspendedChannel = false;
      mChannel->Cancel(NS_ERROR_ABORT);
      mChannel->Resume();
      mChannel = nullptr;
    }

    RemoveShutdownObserver();
  }

  return NS_OK;
}

#undef LOG_ENABLED

} // namespace net
} // namespace mozilla
