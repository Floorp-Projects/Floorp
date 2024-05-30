/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsChannelClassifier.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsICacheEntry.h"
#include "nsICachingChannel.h"
#include "nsIChannel.h"
#include "nsIObserverService.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsXULAppAPI.h"
#include "nsQueryObject.h"
#include "nsPrintfCString.h"

#include "mozilla/Components.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"

namespace mozilla {
namespace net {

#define URLCLASSIFIER_EXCEPTION_HOSTNAMES "urlclassifier.skipHostnames"

// Put CachedPrefs in anonymous namespace to avoid any collision from outside of
// this file.
namespace {

/**
 * It is not recommended to read from Preference everytime a channel is
 * connected.
 * That is not fast and we should cache preference values and reuse them
 */
class CachedPrefs final {
 public:
  static CachedPrefs* GetInstance();

  void Init();

  nsCString GetExceptionHostnames() const { return mExceptionHostnames; }
  void SetExceptionHostnames(const nsACString& aHostnames) {
    mExceptionHostnames = aHostnames;
  }

 private:
  friend class StaticAutoPtr<CachedPrefs>;
  CachedPrefs();
  ~CachedPrefs();

  static void OnPrefsChange(const char* aPrefName, void*);

  nsCString mExceptionHostnames;

  static StaticAutoPtr<CachedPrefs> sInstance;
};

StaticAutoPtr<CachedPrefs> CachedPrefs::sInstance;

// static
void CachedPrefs::OnPrefsChange(const char* aPref, void* aPrefs) {
  auto* prefs = static_cast<CachedPrefs*>(aPrefs);

  if (!strcmp(aPref, URLCLASSIFIER_EXCEPTION_HOSTNAMES)) {
    nsCString exceptionHostnames;
    Preferences::GetCString(URLCLASSIFIER_EXCEPTION_HOSTNAMES,
                            exceptionHostnames);
    ToLowerCase(exceptionHostnames);
    prefs->SetExceptionHostnames(exceptionHostnames);
  }
}

void CachedPrefs::Init() {
  Preferences::RegisterCallbackAndCall(CachedPrefs::OnPrefsChange,
                                       URLCLASSIFIER_EXCEPTION_HOSTNAMES, this);
}

// static
CachedPrefs* CachedPrefs::GetInstance() {
  if (!sInstance) {
    sInstance = new CachedPrefs();
    sInstance->Init();
    ClearOnShutdown(&sInstance);
  }
  MOZ_ASSERT(sInstance);
  return sInstance;
}

CachedPrefs::CachedPrefs() { MOZ_COUNT_CTOR(CachedPrefs); }

CachedPrefs::~CachedPrefs() {
  MOZ_COUNT_DTOR(CachedPrefs);

  Preferences::UnregisterCallback(CachedPrefs::OnPrefsChange,
                                  URLCLASSIFIER_EXCEPTION_HOSTNAMES, this);
}

}  // anonymous namespace

NS_IMPL_ISUPPORTS(nsChannelClassifier, nsIURIClassifierCallback, nsIObserver)

nsChannelClassifier::nsChannelClassifier(nsIChannel* aChannel)
    : mIsAllowListed(false), mSuspendedChannel(false), mChannel(aChannel) {
  UC_LOG_LEAK(("nsChannelClassifier::nsChannelClassifier [this=%p]", this));
  MOZ_ASSERT(mChannel);
}

nsChannelClassifier::~nsChannelClassifier() {
  UC_LOG_LEAK(("nsChannelClassifier::~nsChannelClassifier [this=%p]", this));
}

void nsChannelClassifier::Start() {
  nsresult rv = StartInternal();
  if (NS_FAILED(rv)) {
    // If we aren't getting a callback for any reason, assume a good verdict and
    // make sure we resume the channel if necessary.
    OnClassifyComplete(NS_OK, ""_ns, ""_ns, ""_ns);
  }
}

nsresult nsChannelClassifier::StartInternal() {
  // Should only be called in the parent process.
  MOZ_ASSERT(XRE_IsParentProcess());

  // Don't bother to run the classifier on a load that has already failed.
  // (this might happen after a redirect)
  nsresult status;
  mChannel->GetStatus(&status);
  if (NS_FAILED(status)) return status;

  // Don't bother to run the classifier on a cached load that was
  // previously classified as good.
  if (HasBeenClassified(mChannel)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = mChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't bother checking certain types of URIs.
  if (uri->SchemeIs("about")) {
    return NS_ERROR_UNEXPECTED;
  }

  bool hasFlags;
  rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_DANGEROUS_TO_LOAD,
                           &hasFlags);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasFlags) return NS_ERROR_UNEXPECTED;

  rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_IS_LOCAL_FILE,
                           &hasFlags);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasFlags) return NS_ERROR_UNEXPECTED;

  rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_IS_UI_RESOURCE,
                           &hasFlags);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasFlags) return NS_ERROR_UNEXPECTED;

  rv = NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                           &hasFlags);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasFlags) return NS_ERROR_UNEXPECTED;

  nsCString exceptionHostnames =
      CachedPrefs::GetInstance()->GetExceptionHostnames();
  if (!exceptionHostnames.IsEmpty()) {
    UC_LOG(
        ("nsChannelClassifier::StartInternal - entitylisted hostnames = %s "
         "[this=%p]",
         exceptionHostnames.get(), this));
    if (IsHostnameEntitylisted(uri, exceptionHostnames)) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  nsCOMPtr<nsIURIClassifier> uriClassifier =
      mozilla::components::UrlClassifierDB::Service(&rv);
  if (rv == NS_ERROR_FACTORY_NOT_REGISTERED || rv == NS_ERROR_NOT_AVAILABLE) {
    // no URI classifier, ignore this failure.
    return NS_ERROR_NOT_AVAILABLE;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptSecurityManager> securityManager =
      mozilla::components::ScriptSecurityManager::Service(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> principal;
  rv = securityManager->GetChannelURIPrincipal(mChannel,
                                               getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  bool expectCallback;
  if (UC_LOG_ENABLED()) {
    nsCOMPtr<nsIURI> principalURI;
    nsCString spec;
    principal->GetAsciiSpec(spec);
    spec.Truncate(std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
    UC_LOG(
        ("nsChannelClassifier::StartInternal - classifying principal %s on "
         "channel %p [this=%p]",
         spec.get(), mChannel.get(), this));
  }
  // The classify is running in parent process, no need to give a valid event
  // target
  rv = uriClassifier->Classify(principal, this, &expectCallback);
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
      UC_LOG_WARN(
          ("nsChannelClassifier::StartInternal - couldn't suspend channel "
           "[this=%p]",
           this));
      return rv;
    }

    mSuspendedChannel = true;
    UC_LOG(
        ("nsChannelClassifier::StartInternal - suspended channel %p [this=%p]",
         mChannel.get(), this));
  } else {
    UC_LOG_WARN((
        "nsChannelClassifier::StartInternal - not expecting callback [this=%p]",
        this));
    return NS_ERROR_FAILURE;
  }

  // Add an observer for shutdown
  AddShutdownObserver();
  return NS_OK;
}

bool nsChannelClassifier::IsHostnameEntitylisted(
    nsIURI* aUri, const nsACString& aEntitylisted) {
  nsAutoCString host;
  nsresult rv = aUri->GetHost(host);
  if (NS_FAILED(rv) || host.IsEmpty()) {
    return false;
  }
  ToLowerCase(host);

  for (const nsACString& token :
       nsCCharSeparatedTokenizer(aEntitylisted, ',').ToRange()) {
    if (token.Equals(host)) {
      UC_LOG(
          ("nsChannelClassifier::StartInternal - skipping %s (entitylisted) "
           "[this=%p]",
           host.get(), this));
      return true;
    }
  }

  return false;
}

// Note in the cache entry that this URL was classified, so that future
// cached loads don't need to be checked.
void nsChannelClassifier::MarkEntryClassified(nsresult status) {
  // Should only be called in the parent process.
  MOZ_ASSERT(XRE_IsParentProcess());

  // Don't cache tracking classifications because we support allowlisting.
  if (UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(status) ||
      mIsAllowListed) {
    return;
  }

  if (UC_LOG_ENABLED()) {
    nsAutoCString errorName;
    GetErrorName(status, errorName);
    nsCOMPtr<nsIURI> uri;
    mChannel->GetURI(getter_AddRefs(uri));
    nsAutoCString spec;
    uri->GetAsciiSpec(spec);
    spec.Truncate(std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
    UC_LOG(
        ("nsChannelClassifier::MarkEntryClassified - result is %s "
         "for uri %s [this=%p, channel=%p]",
         errorName.get(), spec.get(), this, mChannel.get()));
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

  nsCOMPtr<nsICacheEntry> cacheEntry = do_QueryInterface(cacheToken);
  if (!cacheEntry) {
    return;
  }

  cacheEntry->SetMetaDataElement("necko:classified",
                                 NS_SUCCEEDED(status) ? "1" : nullptr);
}

bool nsChannelClassifier::HasBeenClassified(nsIChannel* aChannel) {
  // Should only be called in the parent process.
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsICachingChannel> cachingChannel = do_QueryInterface(aChannel);
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

  nsCOMPtr<nsICacheEntry> cacheEntry = do_QueryInterface(cacheToken);
  if (!cacheEntry) {
    return false;
  }

  nsCString tag;
  cacheEntry->GetMetaDataElement("necko:classified", getter_Copies(tag));
  return tag.EqualsLiteral("1");
}

/* static */
nsresult nsChannelClassifier::SendThreatHitReport(nsIChannel* aChannel,
                                                  const nsACString& aProvider,
                                                  const nsACString& aList,
                                                  const nsACString& aFullHash) {
  NS_ENSURE_ARG_POINTER(aChannel);

  nsAutoCString provider(aProvider);
  nsPrintfCString reportEnablePref(
      "browser.safebrowsing.provider.%s.dataSharing.enabled", provider.get());
  if (!Preferences::GetBool(reportEnablePref.get(), false)) {
    UC_LOG(
        ("nsChannelClassifier::SendThreatHitReport - data sharing disabled for "
         "%s",
         provider.get()));
    return NS_OK;
  }

  nsCOMPtr<nsIURIClassifier> uriClassifier =
      components::UrlClassifierDB::Service();
  if (!uriClassifier) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv =
      uriClassifier->SendThreatHitReport(aChannel, aProvider, aList, aFullHash);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsChannelClassifier::OnClassifyComplete(nsresult aErrorCode,
                                        const nsACString& aList,
                                        const nsACString& aProvider,
                                        const nsACString& aFullHash) {
  // Should only be called in the parent process.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(
      !UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(aErrorCode));

  if (mSuspendedChannel) {
    MarkEntryClassified(aErrorCode);

    if (NS_FAILED(aErrorCode)) {
      if (UC_LOG_ENABLED()) {
        nsAutoCString errorName;
        GetErrorName(aErrorCode, errorName);

        nsCOMPtr<nsIURI> uri;
        mChannel->GetURI(getter_AddRefs(uri));
        nsCString spec = uri->GetSpecOrDefault();
        spec.Truncate(
            std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
        UC_LOG(
            ("nsChannelClassifier::OnClassifyComplete - cancelling channel %p "
             "for %s "
             "with error code %s [this=%p]",
             mChannel.get(), spec.get(), errorName.get(), this));
      }

      // Channel will be cancelled (page element blocked) due to Safe Browsing.
      // Do update the security state of the document and fire a security
      // change event.
      UrlClassifierCommon::SetBlockedContent(mChannel, aErrorCode, aList,
                                             aProvider, aFullHash);

      if (aErrorCode == NS_ERROR_MALWARE_URI ||
          aErrorCode == NS_ERROR_PHISHING_URI ||
          aErrorCode == NS_ERROR_UNWANTED_URI ||
          aErrorCode == NS_ERROR_HARMFUL_URI) {
        SendThreatHitReport(mChannel, aProvider, aList, aFullHash);
      }

      mChannel->Cancel(aErrorCode);
    }
    UC_LOG(
        ("nsChannelClassifier::OnClassifyComplete - resuming channel %p "
         "[this=%p]",
         mChannel.get(), this));
    mChannel->Resume();
  }

  mChannel = nullptr;
  RemoveShutdownObserver();

  return NS_OK;
}

void nsChannelClassifier::AddShutdownObserver() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "profile-change-net-teardown", false);
  }
}

void nsChannelClassifier::RemoveShutdownObserver() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, "profile-change-net-teardown");
  }
}

///////////////////////////////////////////////////////////////////////////////
// nsIObserver implementation
NS_IMETHODIMP
nsChannelClassifier::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
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

}  // namespace net
}  // namespace mozilla
