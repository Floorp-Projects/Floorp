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

// When we do blacklist/whitelist classification, from a list of features, we
// need to aggregate them per URI, because not all the features work with the
// same channel's URI.
// This struct contains only the features able to deal with a particular URI.
// See more in GetFeatureTasks().
struct FeatureTask {
  nsCOMPtr<nsIURI> mURI;
  // Let's use RefPtr<> here, because this needs to be used with methods which
  // require it.
  nsTArray<RefPtr<nsIUrlClassifierFeature>> mFeatures;
};

// Features are able to classify particular URIs from a channel. For instance,
// tracking-annotation feature uses the top-level URI to whitelist the current
// channel's URI; flash feature always uses the channel's URI.  Because of
// this, this function aggregates feature per URI in an array of FeatureTask
// object.
nsresult GetFeatureTasks(
    nsIChannel* aChannel,
    const nsTArray<nsCOMPtr<nsIUrlClassifierFeature>>& aFeatures,
    nsIUrlClassifierFeature::listType aListType,
    nsTArray<FeatureTask>& aTasks) {
  MOZ_ASSERT(!aFeatures.IsEmpty());

  // Let's unify features per nsIURI.
  for (nsIUrlClassifierFeature* feature : aFeatures) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv =
        feature->GetURIByListType(aChannel, aListType, getter_AddRefs(uri));
    if (NS_WARN_IF(NS_FAILED(rv)) || !uri) {
      if (UC_LOG_ENABLED()) {
        nsAutoCString errorName;
        GetErrorName(rv, errorName);
        UC_LOG(
            ("GetFeatureTasks got an unexpected error (rv=%s) while trying to "
             "create a whitelist URI. Allowing tracker.",
             errorName.get()));
      }
      return rv;
    }

    MOZ_ASSERT(uri);

    bool found = false;
    for (FeatureTask& task : aTasks) {
      bool equal = false;
      rv = task.mURI->Equals(uri, &equal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (equal) {
        task.mFeatures.AppendElement(feature);
        found = true;
        break;
      }
    }

    if (!found) {
      FeatureTask* task = aTasks.AppendElement();
      task->mURI = uri;
      task->mFeatures.AppendElement(feature);
    }
  }

  return NS_OK;
}

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

// This class is designed to get the results of checking whitelist.
class WhitelistClassifierCallback final
    : public nsIUrlClassifierFeatureCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERFEATURECALLBACK

  WhitelistClassifierCallback(
      nsIChannel* aChannel,
      const nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>& aBlacklistResults,
      std::function<void()>& aCallback)
      : mChannel(aChannel),
        mTaskCount(0),
        mBlacklistResults(aBlacklistResults),
        mChannelCallback(aCallback) {
    MOZ_ASSERT(mChannel);
    MOZ_ASSERT(!mBlacklistResults.IsEmpty());
  }

  void SetTaskCount(uint32_t aTaskCount) {
    MOZ_ASSERT(aTaskCount > 0);
    mTaskCount = aTaskCount;
  }

 private:
  ~WhitelistClassifierCallback() = default;

  nsresult OnClassifyCompleteInternal();

  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIURI> mURI;
  uint32_t mTaskCount;
  nsTArray<RefPtr<nsIUrlClassifierFeatureResult>> mBlacklistResults;
  std::function<void()> mChannelCallback;

  nsTArray<RefPtr<nsIUrlClassifierFeatureResult>> mWhitelistResults;
};

NS_IMPL_ISUPPORTS(WhitelistClassifierCallback, nsIUrlClassifierFeatureCallback)

NS_IMETHODIMP
WhitelistClassifierCallback::OnClassifyComplete(
    const nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>& aWhitelistResults) {
  MOZ_ASSERT(mTaskCount > 0);

  UC_LOG(("WhitelistClassifierCallback[%p]:OnClassifyComplete channel=%p", this,
          mChannel.get()));

  mWhitelistResults.AppendElements(aWhitelistResults);

  if (--mTaskCount) {
    // More callbacks will come.
    return NS_OK;
  }

  return OnClassifyCompleteInternal();
}

nsresult WhitelistClassifierCallback::OnClassifyCompleteInternal() {
  nsTArray<RefPtr<nsIUrlClassifierFeatureResult>> remainingResults;

  for (nsIUrlClassifierFeatureResult* blacklistResult : mBlacklistResults) {
    UrlClassifierFeatureResult* result =
        static_cast<UrlClassifierFeatureResult*>(blacklistResult);

    nsIUrlClassifierFeature* blacklistFeature = result->Feature();
    MOZ_ASSERT(blacklistFeature);

    bool found = false;
    for (nsIUrlClassifierFeatureResult* whitelistResult : mWhitelistResults) {
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

    if (nsContentUtils::IsURIInList(result->URI(), skipList)) {
      if (UC_LOG_ENABLED()) {
        UC_LOG(
            ("WhitelistClassifierCallback[%p]::OnClassifyComplete uri found in "
             "skiplist",
             this));
      }

      continue;
    }

    remainingResults.AppendElement(blacklistResult);
  }

  // Whitelist lookup results

  if (remainingResults.IsEmpty()) {
    if (UC_LOG_ENABLED()) {
      UC_LOG(
          ("WhitelistClassifierCallback[%p]::OnClassifyComplete uri fully "
           "whitelisted",
           this));
    }

    mChannelCallback();
    return NS_OK;
  }

  if (UC_LOG_ENABLED()) {
    UC_LOG(
        ("WhitelistClassifierCallback[%p]::OnClassifyComplete channel[%p] "
         "should not be whitelisted",
         this, mChannel.get()));
  }

  return TrackerFound(remainingResults, mChannel, mChannelCallback);
}

// This class is designed to get the results of checking blacklist.
class BlacklistClassifierCallback final
    : public nsIUrlClassifierFeatureCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERFEATURECALLBACK

  BlacklistClassifierCallback(nsIChannel* aChannel,
                              std::function<void()>&& aCallback)
      : mChannel(aChannel),
        mTaskCount(0),
        mChannelCallback(std::move(aCallback)) {
    MOZ_ASSERT(mChannel);
  }

  void SetTaskCount(uint32_t aTaskCount) {
    MOZ_ASSERT(aTaskCount > 0);
    mTaskCount = aTaskCount;
  }

 private:
  ~BlacklistClassifierCallback() = default;

  nsresult OnClassifyCompleteInternal();

  nsCOMPtr<nsIChannel> mChannel;
  uint32_t mTaskCount;
  std::function<void()> mChannelCallback;

  nsTArray<RefPtr<nsIUrlClassifierFeatureResult>> mResults;
};

NS_IMPL_ISUPPORTS(BlacklistClassifierCallback, nsIUrlClassifierFeatureCallback)

NS_IMETHODIMP
BlacklistClassifierCallback::OnClassifyComplete(
    const nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>& aResults) {
  MOZ_ASSERT(mTaskCount > 0);

  UC_LOG(("BlacklistClassifierCallback[%p]:OnClassifyComplete - remaining %d",
          this, mTaskCount));

  mResults.AppendElements(aResults);

  if (--mTaskCount) {
    // More callbacks will come.
    return NS_OK;
  }

  return OnClassifyCompleteInternal();
}

nsresult BlacklistClassifierCallback::OnClassifyCompleteInternal() {
  // All good! The URL has not been classified.
  if (mResults.IsEmpty()) {
    if (UC_LOG_ENABLED()) {
      UC_LOG(
          ("BlacklistClassifierCallback[%p]::OnClassifyComplete uri not found "
           "in blacklist",
           this));
    }

    mChannelCallback();
    return NS_OK;
  }

  if (UC_LOG_ENABLED()) {
    UC_LOG(
        ("BlacklistClassifierCallback[%p]::OnClassifyComplete uri is in "
         "blacklist. Start checking whitelist.",
         this));
  }

  nsTArray<nsCOMPtr<nsIUrlClassifierFeature>> features;
  for (nsIUrlClassifierFeatureResult* result : mResults) {
    features.AppendElement(
        static_cast<UrlClassifierFeatureResult*>(result)->Feature());
  }

  nsTArray<FeatureTask> tasks;
  nsresult rv = GetFeatureTasks(mChannel, features,
                                nsIUrlClassifierFeature::whitelist, tasks);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return TrackerFound(mResults, mChannel, mChannelCallback);
  }

  if (tasks.IsEmpty()) {
    UC_LOG(
        ("BlacklistClassifierCallback[%p]:OnClassifyComplete could not create "
         "a whitelist URI. Ignoring whitelist.",
         this));

    return TrackerFound(mResults, mChannel, mChannelCallback);
  }

  RefPtr<WhitelistClassifierCallback> callback =
      new WhitelistClassifierCallback(mChannel, mResults, mChannelCallback);

  nsCOMPtr<nsIURIClassifier> uriClassifier =
      do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t pendingCallbacks = 0;
  for (FeatureTask& task : tasks) {
    rv = uriClassifier->AsyncClassifyLocalWithFeatures(
        task.mURI, task.mFeatures, nsIUrlClassifierFeature::whitelist,
        callback);

    if (NS_WARN_IF(NS_FAILED(rv))) {
      if (UC_LOG_ENABLED()) {
        nsAutoCString errorName;
        GetErrorName(rv, errorName);
        UC_LOG((
            "BlacklistClassifierCallback[%p]:OnClassifyComplete Failed "
            "calling AsyncClassifyLocalWithFeatures with rv=%s. Let's move on.",
            this, errorName.get()));
      }

      continue;
    }

    ++pendingCallbacks;
  }

  // All the AsyncClassifyLocalWithFeatures() calls return error. We do not
  // expect callbacks.
  if (pendingCallbacks == 0) {
    if (UC_LOG_ENABLED()) {
      UC_LOG(
          ("BlacklistClassifierCallback[%p]:OnClassifyComplete All "
           "AsyncClassifyLocalWithFeatures() calls return errors. We cannot "
           "continue.",
           this));
    }

    return TrackerFound(mResults, mChannel, mChannelCallback);
  }

  // Nothing else do here. Let's wait for the WhitelistClassifierCallback.
  callback->SetTaskCount(pendingCallbacks);
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

  // We need to obtain the list of nsIUrlClassifierFeature objects able to
  // classify this channel. If the list is empty, we do an early return.
  nsTArray<nsCOMPtr<nsIUrlClassifierFeature>> features;
  UrlClassifierFeatureFactory::GetFeaturesFromChannel(aChannel, features);
  if (features.IsEmpty()) {
    UC_LOG(
        ("AsyncUrlChannelClassifier: Nothing to do for channel %p", aChannel));
    return NS_ERROR_FAILURE;
  }

  nsTArray<FeatureTask> tasks;
  nsresult rv = GetFeatureTasks(aChannel, features,
                                nsIUrlClassifierFeature::blacklist, tasks);
  if (NS_WARN_IF(NS_FAILED(rv)) || tasks.IsEmpty()) {
    return rv;
  }

  MOZ_ASSERT(!tasks.IsEmpty());

  nsCOMPtr<nsIURIClassifier> uriClassifier =
      do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<BlacklistClassifierCallback> callback =
      new BlacklistClassifierCallback(aChannel, std::move(aCallback));

  uint32_t pendingCallbacks = 0;
  for (FeatureTask& task : tasks) {
    if (UC_LOG_ENABLED()) {
      nsCString spec = task.mURI->GetSpecOrDefault();
      spec.Truncate(
          std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
      UC_LOG(("AsyncUrlChannelClassifier: Checking blacklist for uri=%s\n",
              spec.get()));
    }

    rv = uriClassifier->AsyncClassifyLocalWithFeatures(
        task.mURI, task.mFeatures, nsIUrlClassifierFeature::blacklist,
        callback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    ++pendingCallbacks;
  }

  // All the AsyncClassifyLocalWithFeatures() calls return error. We do not
  // expect callbacks.
  if (pendingCallbacks == 0) {
    return NS_ERROR_FAILURE;
  }

  callback->SetTaskCount(pendingCallbacks);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
