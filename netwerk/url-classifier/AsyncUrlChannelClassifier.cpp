/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set expandtab ts=4 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ErrorNames.h"
#include "mozilla/net/AsyncUrlChannelClassifier.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/net/UrlClassifierFeatureResult.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIURIClassifier.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

namespace {

nsresult TrackerFound(
    const nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>& aResults,
    nsIChannel* aChannel, const std::function<void()>& aCallback) {
  // Let's ask the features to do the magic.
  for (nsIUrlClassifierFeatureResult* result : aResults) {
    UrlClassifierFeatureResult* r =
        static_cast<UrlClassifierFeatureResult*>(result);

    bool shouldContinue = false;
    nsresult rv =
        r->Feature()->ProcessChannel(aChannel, r->List(), &shouldContinue);
    // Don't return here! We want to process all the channel and execute the
    // callback.
    Unused << NS_WARN_IF(NS_FAILED(rv));

    if (!shouldContinue) {
      break;
    }
  }

  aCallback();
  return NS_OK;
}

nsresult CreateWhiteListURI(nsIChannel* aChannel, nsIURI** aURI) {
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(aURI);

  nsresult rv;
  nsCOMPtr<nsIHttpChannelInternal> chan = do_QueryInterface(aChannel, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!chan) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> topWinURI;
  rv = chan->GetTopWindowURI(getter_AddRefs(topWinURI));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!topWinURI) {
    if (UC_LOG_ENABLED()) {
      nsresult rv;
      nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(aChannel, &rv);
      nsCOMPtr<nsIURI> uri;
      rv = httpChan->GetURI(getter_AddRefs(uri));
      nsAutoCString spec;
      uri->GetAsciiSpec(spec);
      spec.Truncate(
          std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
      UC_LOG(
          ("CreateWhiteListURI: No window URI associated with %s", spec.get()));
    }
    return NS_OK;
  }

  nsCOMPtr<nsIScriptSecurityManager> securityManager =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIPrincipal> chanPrincipal;
  rv = securityManager->GetChannelURIPrincipal(aChannel,
                                               getter_AddRefs(chanPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Craft a whitelist URL like "toplevel.page/?resource=third.party.domain"
  nsAutoCString pageHostname, resourceDomain;
  rv = topWinURI->GetHost(pageHostname);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = chanPrincipal->GetBaseDomain(resourceDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString whitelistEntry = NS_LITERAL_CSTRING("http://") + pageHostname +
                                 NS_LITERAL_CSTRING("/?resource=") +
                                 resourceDomain;
  UC_LOG(("CreateWhiteListURI: Looking for %s in the whitelist (channel=%p)",
          whitelistEntry.get(), aChannel));

  nsCOMPtr<nsIURI> whitelistURI;
  rv = NS_NewURI(getter_AddRefs(whitelistURI), whitelistEntry);
  NS_ENSURE_SUCCESS(rv, rv);

  whitelistURI.forget(aURI);
  return NS_OK;
}

nsresult IsTrackerWhitelisted(
    nsIURI* aWhiteListURI,
    const nsTArray<RefPtr<nsIUrlClassifierFeature>>& aFeatures,
    nsIUrlClassifierFeatureCallback* aCallback) {
  MOZ_ASSERT(aWhiteListURI);
  MOZ_ASSERT(!aFeatures.IsEmpty());
  MOZ_ASSERT(aCallback);

  nsresult rv;
  nsCOMPtr<nsIURIClassifier> uriClassifier =
      do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return uriClassifier->AsyncClassifyLocalWithFeatures(
      aWhiteListURI, aFeatures, nsIUrlClassifierFeature::whitelist, aCallback);
}

// This class is designed to get the results of checking whitelist.
class WhitelistClassifierCallback final
    : public nsIUrlClassifierFeatureCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERFEATURECALLBACK

  WhitelistClassifierCallback(
      nsIChannel* aChannel, nsIURI* aURI,
      const nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>& aBlacklistResults,
      std::function<void()>& aCallback)
      : mChannel(aChannel),
        mURI(aURI),
        mBlacklistResults(aBlacklistResults),
        mChannelCallback(aCallback) {
    MOZ_ASSERT(mChannel);
    MOZ_ASSERT(mURI);
    MOZ_ASSERT(!mBlacklistResults.IsEmpty());
  }

 private:
  ~WhitelistClassifierCallback() = default;

  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIURI> mURI;
  nsTArray<RefPtr<nsIUrlClassifierFeatureResult>> mBlacklistResults;
  std::function<void()> mChannelCallback;
};

NS_IMPL_ISUPPORTS(WhitelistClassifierCallback, nsIUrlClassifierFeatureCallback)

NS_IMETHODIMP
WhitelistClassifierCallback::OnClassifyComplete(
    const nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>& aWhitelistResults) {
  UC_LOG(("WhitelistClassifierCallback[%p]:OnClassifyComplete channel=%p", this,
          mChannel.get()));

  nsTArray<RefPtr<nsIUrlClassifierFeatureResult>> remainingResults;

  for (nsIUrlClassifierFeatureResult* blacklistResult : mBlacklistResults) {
    nsIUrlClassifierFeature* blacklistFeature =
        static_cast<UrlClassifierFeatureResult*>(blacklistResult)->Feature();
    MOZ_ASSERT(blacklistFeature);

    bool found = false;
    for (nsIUrlClassifierFeatureResult* whitelistResult : aWhitelistResults) {
      // We can do pointer comparison because Features are singletons.
      if (static_cast<UrlClassifierFeatureResult*>(whitelistResult)
              ->Feature() == blacklistFeature) {
        found = true;
        break;
      }
    }

    if (found) {
      continue;
    }

    // Maybe we have to skip this host
    nsAutoCString skipList;
    nsresult rv = blacklistFeature->GetSkipHostList(skipList);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    if (nsContentUtils::IsURIInList(mURI, skipList)) {
      if (UC_LOG_ENABLED()) {
        nsCString spec = mURI->GetSpecOrDefault();
        UC_LOG(
            ("WhitelistClassifierCallback[%p]::OnClassifyComplete uri %s found "
             "in skiplist",
             this, spec.get()));
      }

      continue;
    }

    remainingResults.AppendElement(blacklistResult);
  }

  // Whitelist lookup results

  if (remainingResults.IsEmpty()) {
    if (UC_LOG_ENABLED()) {
      nsCString spec = mURI->GetSpecOrDefault();
      spec.Truncate(
          std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
      UC_LOG(
          ("WhitelistClassifierCallback[%p]::OnClassifyComplete uri %s fully "
           "whitelisted",
           this, spec.get()));
    }

    mChannelCallback();
    return NS_OK;
  }

  if (UC_LOG_ENABLED()) {
    nsCString spec = mURI->GetSpecOrDefault();
    spec.Truncate(std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
    UC_LOG(
        ("WhitelistClassifierCallback[%p]::OnClassifyComplete "
         "channel[%p] uri=%s, should not be whitelisted",
         this, mChannel.get(), spec.get()));
  }

  return TrackerFound(remainingResults, mChannel, mChannelCallback);
}

// This class is designed to get the results of checking blacklist.
class BlacklistClassifierCallback final
    : public nsIUrlClassifierFeatureCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERFEATURECALLBACK

  BlacklistClassifierCallback(nsIChannel* aChannel, nsIURI* aURI,
                              std::function<void()>&& aCallback)
      : mChannel(aChannel), mURI(aURI), mChannelCallback(std::move(aCallback)) {
    MOZ_ASSERT(mChannel);
    MOZ_ASSERT(mURI);
  }

 private:
  ~BlacklistClassifierCallback() = default;

  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIURI> mURI;
  std::function<void()> mChannelCallback;
};

NS_IMPL_ISUPPORTS(BlacklistClassifierCallback, nsIUrlClassifierFeatureCallback)

NS_IMETHODIMP
BlacklistClassifierCallback::OnClassifyComplete(
    const nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>& aResults) {
  UC_LOG(("BlacklistClassifierCallback[%p]:OnClassifyComplete", this));

  // All good! The URL has not been classified.
  if (aResults.IsEmpty()) {
    if (UC_LOG_ENABLED()) {
      nsCString spec = mURI->GetSpecOrDefault();
      spec.Truncate(
          std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
      UC_LOG(
          ("BlacklistClassifierCallback[%p]::OnClassifyComplete uri %s not "
           "found in blacklist",
           this, spec.get()));
    }

    mChannelCallback();
    return NS_OK;
  }

  if (UC_LOG_ENABLED()) {
    nsCString spec = mURI->GetSpecOrDefault();
    spec.Truncate(std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
    UC_LOG(
        ("BlacklistClassifierCallback[%p]::OnClassifyComplete uri %s is in "
         "blacklist. Start checking whitelist.",
         this, spec.get()));
  }

  nsCOMPtr<nsIURI> whitelistURI;
  nsresult rv = CreateWhiteListURI(mChannel, getter_AddRefs(whitelistURI));
  if (NS_FAILED(rv)) {
    nsAutoCString errorName;
    GetErrorName(rv, errorName);
    NS_WARNING(
        nsPrintfCString("BlacklistClassifierCallback[%p]:OnClassifyComplete "
                        "got an unexpected error (rv=%s) while trying to "
                        "create a whitelist URI. Allowing tracker.",
                        this, errorName.get())
            .get());
    return TrackerFound(aResults, mChannel, mChannelCallback);
  }

  if (!whitelistURI) {
    UC_LOG(
        ("BlacklistClassifierCallback[%p]:OnClassifyComplete could not create "
         "a whitelist URI. Ignoring whitelist.",
         this));

    return TrackerFound(aResults, mChannel, mChannelCallback);
  }

  nsCOMPtr<nsIUrlClassifierFeatureCallback> callback =
      new WhitelistClassifierCallback(mChannel, mURI, aResults,
                                      mChannelCallback);

  // xpcom parser creates array of interfaces using RefPtr<>.
  nsTArray<RefPtr<nsIUrlClassifierFeature>> refPtrFeatures;
  for (nsIUrlClassifierFeatureResult* result : aResults) {
    refPtrFeatures.AppendElement(
        static_cast<UrlClassifierFeatureResult*>(result)->Feature());
  }

  rv = IsTrackerWhitelisted(whitelistURI, refPtrFeatures, callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (UC_LOG_ENABLED()) {
      nsAutoCString errorName;
      GetErrorName(rv, errorName);
      UC_LOG(
          ("BlacklistClassifierCallback[%p]:OnClassifyComplete "
           "IsTrackerWhitelisted has failed with rv=%s.",
           this, errorName.get()));
    }

    return TrackerFound(aResults, mChannel, mChannelCallback);
  }

  // Nothing else do here. Let's wait for the WhitelistClassifierCallback.
  return NS_OK;
}

}  // namespace

/* static */ nsresult AsyncUrlChannelClassifier::CheckChannel(
    nsIChannel* aChannel, std::function<void()>&& aCallback) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aChannel);

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv) || !uri) {
    return rv;
  }

  nsTArray<nsCOMPtr<nsIUrlClassifierFeature>> features;
  UrlClassifierFeatureFactory::GetFeaturesFromChannel(aChannel, features);
  if (features.IsEmpty()) {
    UC_LOG(("AsyncUrlChannelClassifier: Feature list is empty for channel %p",
            aChannel));
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURIClassifier> uriClassifier =
      do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIUrlClassifierFeatureCallback> callback =
      new BlacklistClassifierCallback(aChannel, uri, std::move(aCallback));

  if (UC_LOG_ENABLED()) {
    nsCString spec = uri->GetSpecOrDefault();
    spec.Truncate(std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
    UC_LOG(("AsyncUrlChannelClassifier: Checking blacklist for uri=%s\n",
            spec.get()));
  }

  // xpcom parser creates array of interfaces using RefPtr<>.
  nsTArray<RefPtr<nsIUrlClassifierFeature>> refPtrFeatures;
  for (nsIUrlClassifierFeature* feature : features) {
    refPtrFeatures.AppendElement(feature);
  }

  return uriClassifier->AsyncClassifyLocalWithFeatures(
      uri, refPtrFeatures, nsIUrlClassifierFeature::blacklist, callback);
}

}  // namespace net
}  // namespace mozilla
