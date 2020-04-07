/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set expandtab ts=4 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Classifier.h"
#include "mozilla/Components.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/net/AsyncUrlChannelClassifier.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/net/UrlClassifierFeatureResult.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"
#include "nsUrlClassifierDBService.h"
#include "nsUrlClassifierUtils.h"

namespace mozilla {
namespace net {

namespace {

// Big picture comment
// -----------------------------------------------------------------------------
// nsUrlClassifierDBService::channelClassify() classifies a channel using a set
// of URL-Classifier features. This method minimizes the number of lookups and
// URI parsing and this is done using the classes here described.
//
// The first class is 'FeatureTask' which is able to retrieve the list of
// features for this channel using the feature-factory. See
// UrlClassifierFeatureFactory.
// For each feature, it creates a FeatureData object, which contains the
// whitelist and blacklist prefs and tables. The reason why we create
// FeatureData is because:
// - features are not thread-safe.
// - we want to store the state of the classification in the FeatureData
//   object.
//
// It can happen that multiple features share the same tables. In order to do
// the lookup just once, we have TableData class. When multiple features
// contain the same table, they have references to the same couple TableData +
// URIData objects.
//
// During the classification, the channel's URIs are fragmented. In order to
// create these fragments just once, we use the URIData class, which is pointed
// by TableData classes.
//
// The creation of these classes happens on the main-thread. The classification
// happens on the worker thread.

// URIData
// -----------------------------------------------------------------------------

// In order to avoid multiple URI parsing, we have this class which contains
// nsIURI and its fragments.
class URIData {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(URIData);

  static nsresult Create(nsIURI* aURI, nsIURI* aInnermostURI,
                         nsIUrlClassifierFeature::URIType aURIType,
                         URIData** aData);

  bool IsEqual(nsIURI* aURI) const;

  const nsTArray<nsCString>& Fragments();

  nsIURI* URI() const;

 private:
  URIData();
  ~URIData();

  nsCOMPtr<nsIURI> mURI;
  nsCString mURISpec;
  nsTArray<nsCString> mFragments;
  nsIUrlClassifierFeature::URIType mURIType;
};

/* static */
nsresult URIData::Create(nsIURI* aURI, nsIURI* aInnermostURI,
                         nsIUrlClassifierFeature::URIType aURIType,
                         URIData** aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aInnermostURI);

  RefPtr<URIData> data = new URIData();
  data->mURI = aURI;
  data->mURIType = aURIType;

  nsUrlClassifierUtils* utilsService = nsUrlClassifierUtils::GetInstance();
  if (NS_WARN_IF(!utilsService)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = utilsService->GetKeyForURI(aInnermostURI, data->mURISpec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  UC_LOG(("URIData::Create[%p] - new URIData created for spec %s", data.get(),
          data->mURISpec.get()));

  data.forget(aData);
  return NS_OK;
}

URIData::URIData() { MOZ_ASSERT(NS_IsMainThread()); }

URIData::~URIData() { NS_ReleaseOnMainThread("URIData:mURI", mURI.forget()); }

bool URIData::IsEqual(nsIURI* aURI) const {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI);

  bool isEqual = false;
  nsresult rv = mURI->Equals(aURI, &isEqual);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return isEqual;
}

const nsTArray<nsCString>& URIData::Fragments() {
  MOZ_ASSERT(!NS_IsMainThread());

  if (mFragments.IsEmpty()) {
    nsresult rv;

    if (mURIType == nsIUrlClassifierFeature::pairwiseWhitelistURI) {
      rv = LookupCache::GetLookupWhitelistFragments(mURISpec, &mFragments);
    } else {
      rv = LookupCache::GetLookupFragments(mURISpec, &mFragments);
    }

    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

  return mFragments;
}

nsIURI* URIData::URI() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mURI;
}

// TableData
// ----------------------------------------------------------------------------

// In order to avoid multiple lookups on the same table + URI, we have this
// class.
class TableData {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TableData);

  enum State {
    eUnclassified,
    eNoMatch,
    eMatch,
  };

  TableData(URIData* aURIData, const nsACString& aTable);

  nsIURI* URI() const;

  const nsACString& Table() const;

  const LookupResultArray& Result() const;

  State MatchState() const;

  bool IsEqual(URIData* aURIData, const nsACString& aTable) const;

  // Returns true if the table classifies the URI. This method must be called
  // on hte classifier worker thread.
  bool DoLookup(nsUrlClassifierDBServiceWorker* aWorkerClassifier);

 private:
  ~TableData();

  RefPtr<URIData> mURIData;
  State mState;

  nsCString mTable;
  LookupResultArray mResults;
};

TableData::TableData(URIData* aURIData, const nsACString& aTable)
    : mURIData(aURIData), mState(eUnclassified), mTable(aTable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURIData);

  UC_LOG(("TableData CTOR[%p] - new TableData created %s", this,
          aTable.BeginReading()));
}

TableData::~TableData() = default;

nsIURI* TableData::URI() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mURIData->URI();
}

const nsACString& TableData::Table() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mTable;
}

const LookupResultArray& TableData::Result() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mResults;
}

TableData::State TableData::MatchState() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mState;
}

bool TableData::IsEqual(URIData* aURIData, const nsACString& aTable) const {
  MOZ_ASSERT(NS_IsMainThread());
  return mURIData == aURIData && mTable == aTable;
}

bool TableData::DoLookup(nsUrlClassifierDBServiceWorker* aWorkerClassifier) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aWorkerClassifier);

  if (mState == TableData::eUnclassified) {
    UC_LOG(("TableData::DoLookup[%p] - starting lookup", this));

    const nsTArray<nsCString>& fragments = mURIData->Fragments();
    nsresult rv = aWorkerClassifier->DoSingleLocalLookupWithURIFragments(
        fragments, mTable, mResults);
    Unused << NS_WARN_IF(NS_FAILED(rv));

    mState = mResults.IsEmpty() ? TableData::eNoMatch : TableData::eMatch;

    UC_LOG(("TableData::DoLookup[%p] - lookup completed. Matches: %d", this,
            (int)mResults.Length()));
  }

  return !mResults.IsEmpty();
}

// FeatureData
// ----------------------------------------------------------------------------

class FeatureTask;

// This is class contains all the Feature data.
class FeatureData {
  enum State {
    eUnclassified,
    eNoMatch,
    eMatchBlacklist,
    eMatchWhitelist,
  };

 public:
  FeatureData();
  ~FeatureData();

  nsresult Initialize(FeatureTask* aTask, nsIChannel* aChannel,
                      nsIUrlClassifierFeature* aFeature);

  void DoLookup(nsUrlClassifierDBServiceWorker* aWorkerClassifier);

  // Returns true if the next feature should be processed.
  bool MaybeCompleteClassification(nsIChannel* aChannel);

 private:
  nsresult InitializeList(FeatureTask* aTask, nsIChannel* aChannel,
                          nsIUrlClassifierFeature::listType aListType,
                          nsTArray<RefPtr<TableData>>& aList);

  State mState;
  nsCOMPtr<nsIUrlClassifierFeature> mFeature;

  nsTArray<RefPtr<TableData>> mBlacklistTables;
  nsTArray<RefPtr<TableData>> mWhitelistTables;

  // blacklist + whitelist.
  nsCString mHostInPrefTables[2];
};

FeatureData::FeatureData() : mState(eUnclassified) {}

FeatureData::~FeatureData() {
  NS_ReleaseOnMainThread("FeatureData:mFeature", mFeature.forget());
}

nsresult FeatureData::Initialize(FeatureTask* aTask, nsIChannel* aChannel,
                                 nsIUrlClassifierFeature* aFeature) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTask);
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(aFeature);

  nsAutoCString featureName;
  aFeature->GetName(featureName);
  UC_LOG(("FeatureData::Initialize[%p] - Feature %s - Channel %p", this,
          featureName.get(), aChannel));

  mFeature = aFeature;

  nsresult rv = InitializeList(
      aTask, aChannel, nsIUrlClassifierFeature::blacklist, mBlacklistTables);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = InitializeList(aTask, aChannel, nsIUrlClassifierFeature::whitelist,
                      mWhitelistTables);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void FeatureData::DoLookup(nsUrlClassifierDBServiceWorker* aWorkerClassifier) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aWorkerClassifier);
  MOZ_ASSERT(mState == eUnclassified);

  UC_LOG(("FeatureData::DoLookup[%p] - lookup starting", this));

  // This is wrong, but it's fast: we don't want to check if the host is in the
  // blacklist table if we know that it's going to be whitelisted by pref.
  // So, also if maybe it's not blacklisted, let's consider it 'whitelisted'.
  if (!mHostInPrefTables[nsIUrlClassifierFeature::whitelist].IsEmpty()) {
    UC_LOG(("FeatureData::DoLookup[%p] - whitelisted by pref", this));
    mState = eMatchWhitelist;
    return;
  }

  // Let's check if this feature blacklists the URI.

  bool isBlacklisted =
      !mHostInPrefTables[nsIUrlClassifierFeature::blacklist].IsEmpty();

  UC_LOG(("FeatureData::DoLookup[%p] - blacklisted by pref: %d", this,
          isBlacklisted));

  if (isBlacklisted == false) {
    // If one of the blacklist table matches the URI, we don't need to continue
    // with the others: the feature is blacklisted (but maybe also
    // whitelisted).
    for (TableData* tableData : mBlacklistTables) {
      if (tableData->DoLookup(aWorkerClassifier)) {
        isBlacklisted = true;
        break;
      }
    }
  }

  UC_LOG(("FeatureData::DoLookup[%p] - blacklisted before whitelisting: %d",
          this, isBlacklisted));

  if (!isBlacklisted) {
    mState = eNoMatch;
    return;
  }

  // Now, let's check if we need to whitelist the same URI.

  for (TableData* tableData : mWhitelistTables) {
    // If one of the whitelist table matches the URI, we don't need to continue
    // with the others: the feature is whitelisted.
    if (tableData->DoLookup(aWorkerClassifier)) {
      UC_LOG(("FeatureData::DoLookup[%p] - whitelisted by table", this));
      mState = eMatchWhitelist;
      return;
    }
  }

  UC_LOG(("FeatureData::DoLookup[%p] - blacklisted", this));
  mState = eMatchBlacklist;
}

bool FeatureData::MaybeCompleteClassification(nsIChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());

  UC_LOG(
      ("FeatureData::MaybeCompleteClassification[%p] - completing "
       "classification for channel %p",
       this, aChannel));

  switch (mState) {
    case eNoMatch:
      UC_LOG(
          ("FeatureData::MaybeCompleteClassification[%p] - no match. Let's "
           "move on",
           this));
      return true;

    case eMatchWhitelist:
      UC_LOG(
          ("FeatureData::MaybeCompleteClassification[%p] - whitelisted. Let's "
           "move on",
           this));
      return true;

    case eMatchBlacklist:
      UC_LOG(
          ("FeatureData::MaybeCompleteClassification[%p] - blacklisted", this));
      break;

    case eUnclassified:
      MOZ_CRASH("We should not be here!");
      break;
  }

  MOZ_ASSERT(mState == eMatchBlacklist);

  // Maybe we have to skip this host
  nsAutoCString skipList;
  nsresult rv = mFeature->GetSkipHostList(skipList);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    UC_LOG(
        ("FeatureData::MaybeCompleteClassification[%p] - error. Let's move on",
         this));
    return true;
  }

  if (!mBlacklistTables.IsEmpty() &&
      nsContentUtils::IsURIInList(mBlacklistTables[0]->URI(), skipList)) {
    UC_LOG(
        ("FeatureData::MaybeCompleteClassification[%p] - uri found in skiplist",
         this));
    return true;
  }

  nsTArray<nsCString> list;
  nsTArray<nsCString> hashes;
  if (!mHostInPrefTables[nsIUrlClassifierFeature::blacklist].IsEmpty()) {
    list.AppendElement(mHostInPrefTables[nsIUrlClassifierFeature::blacklist]);

    // Telemetry expects every tracking channel has hash, create it for test
    // entry
    Completion complete;
    complete.FromPlaintext(
        mHostInPrefTables[nsIUrlClassifierFeature::blacklist]);
    hashes.AppendElement(complete.ToString());
  }

  for (TableData* tableData : mBlacklistTables) {
    if (tableData->MatchState() == TableData::eMatch) {
      list.AppendElement(tableData->Table());

      for (const auto& r : tableData->Result()) {
        hashes.AppendElement(r->hash.complete.ToString());
      }
    }
  }

  UC_LOG(("FeatureData::MaybeCompleteClassification[%p] - process channel %p",
          this, aChannel));

  bool shouldContinue = false;
  rv = mFeature->ProcessChannel(aChannel, list, hashes, &shouldContinue);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  return shouldContinue;
}

// CallbackHolder
// ----------------------------------------------------------------------------

// This class keeps the callback alive and makes sure that we release it on the
// correct thread.
class CallbackHolder final {
 public:
  NS_INLINE_DECL_REFCOUNTING(CallbackHolder);

  explicit CallbackHolder(std::function<void()>&& aCallback)
      : mCallback(std::move(aCallback)) {}

  void Exec() const { mCallback(); }

 private:
  ~CallbackHolder() = default;

  std::function<void()> mCallback;
};

// FeatureTask
// ----------------------------------------------------------------------------

// A FeatureTask is a class that is able to classify a channel using a set of
// features. The features are grouped by:
// - URIs - to avoid extra URI parsing.
// - Tables - to avoid multiple lookup on the same table.
class FeatureTask {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FeatureTask);

  static nsresult Create(nsIChannel* aChannel,
                         std::function<void()>&& aCallback,
                         FeatureTask** aTask);

  // Called on the classifier thread.
  void DoLookup(nsUrlClassifierDBServiceWorker* aWorkerClassifier);

  // Called on the main-thread to process the channel.
  void CompleteClassification();

  nsresult GetOrCreateURIData(nsIURI* aURI, nsIURI* aInnermostURI,
                              nsIUrlClassifierFeature::URIType aURIType,
                              URIData** aData);

  nsresult GetOrCreateTableData(URIData* aURIData, const nsACString& aTable,
                                TableData** aData);

 private:
  FeatureTask(nsIChannel* aChannel, std::function<void()>&& aCallback);
  ~FeatureTask();

  nsCOMPtr<nsIChannel> mChannel;
  RefPtr<CallbackHolder> mCallbackHolder;

  nsTArray<FeatureData> mFeatures;
  nsTArray<RefPtr<URIData>> mURIs;
  nsTArray<RefPtr<TableData>> mTables;
};

// Features are able to classify particular URIs from a channel. For instance,
// tracking-annotation feature uses the top-level URI to whitelist the current
// channel's URI; flash feature always uses the channel's URI.  Because of
// this, this function aggregates feature per URI and tables.
/* static */
nsresult FeatureTask::Create(nsIChannel* aChannel,
                             std::function<void()>&& aCallback,
                             FeatureTask** aTask) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(aTask);

  // We need to obtain the list of nsIUrlClassifierFeature objects able to
  // classify this channel. If the list is empty, we do an early return.
  nsTArray<nsCOMPtr<nsIUrlClassifierFeature>> features;
  UrlClassifierFeatureFactory::GetFeaturesFromChannel(aChannel, features);
  if (features.IsEmpty()) {
    UC_LOG(("FeatureTask::Create: Nothing to do for channel %p", aChannel));
    return NS_OK;
  }

  RefPtr<FeatureTask> task = new FeatureTask(aChannel, std::move(aCallback));

  UC_LOG(("FeatureTask::Create[%p] - FeatureTask created for channel %p",
          task.get(), aChannel));

  for (nsIUrlClassifierFeature* feature : features) {
    FeatureData* featureData = task->mFeatures.AppendElement();
    nsresult rv = featureData->Initialize(task, aChannel, feature);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  task.forget(aTask);
  return NS_OK;
}

FeatureTask::FeatureTask(nsIChannel* aChannel,
                         std::function<void()>&& aCallback)
    : mChannel(aChannel) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mChannel);

  std::function<void()> callback = std::move(aCallback);
  mCallbackHolder = new CallbackHolder(std::move(callback));
}

FeatureTask::~FeatureTask() {
  NS_ReleaseOnMainThread("FeatureTask::mChannel", mChannel.forget());
  NS_ReleaseOnMainThread("FeatureTask::mCallbackHolder",
                         mCallbackHolder.forget());
}

nsresult FeatureTask::GetOrCreateURIData(
    nsIURI* aURI, nsIURI* aInnermostURI,
    nsIUrlClassifierFeature::URIType aURIType, URIData** aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI);
  MOZ_ASSERT(aInnermostURI);
  MOZ_ASSERT(aData);

  UC_LOG(
      ("FeatureTask::GetOrCreateURIData[%p] - Checking if a URIData must be "
       "created",
       this));

  for (URIData* data : mURIs) {
    if (data->IsEqual(aURI)) {
      UC_LOG(("FeatureTask::GetOrCreateURIData[%p] - Reuse existing URIData %p",
              this, data));

      RefPtr<URIData> uriData = data;
      uriData.forget(aData);
      return NS_OK;
    }
  }

  RefPtr<URIData> data;
  nsresult rv =
      URIData::Create(aURI, aInnermostURI, aURIType, getter_AddRefs(data));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mURIs.AppendElement(data);

  UC_LOG(("FeatureTask::GetOrCreateURIData[%p] - Create new URIData %p", this,
          data.get()));

  data.forget(aData);
  return NS_OK;
}

nsresult FeatureTask::GetOrCreateTableData(URIData* aURIData,
                                           const nsACString& aTable,
                                           TableData** aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURIData);
  MOZ_ASSERT(aData);

  UC_LOG(
      ("FeatureTask::GetOrCreateTableData[%p] - Checking if TableData must be "
       "created",
       this));

  for (TableData* data : mTables) {
    if (data->IsEqual(aURIData, aTable)) {
      UC_LOG((
          "FeatureTask::GetOrCreateTableData[%p] - Reuse existing TableData %p",
          this, data));

      RefPtr<TableData> tableData = data;
      tableData.forget(aData);
      return NS_OK;
    }
  }

  RefPtr<TableData> data = new TableData(aURIData, aTable);
  mTables.AppendElement(data);

  UC_LOG(("FeatureTask::GetOrCreateTableData[%p] - Create new TableData %p",
          this, data.get()));

  data.forget(aData);
  return NS_OK;
}

void FeatureTask::DoLookup(nsUrlClassifierDBServiceWorker* aWorkerClassifier) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aWorkerClassifier);

  UC_LOG(("FeatureTask::DoLookup[%p] - starting lookup", this));

  for (FeatureData& feature : mFeatures) {
    feature.DoLookup(aWorkerClassifier);
  }

  UC_LOG(("FeatureTask::DoLookup[%p] - lookup completed", this));
}

void FeatureTask::CompleteClassification() {
  MOZ_ASSERT(NS_IsMainThread());

  for (FeatureData& feature : mFeatures) {
    if (!feature.MaybeCompleteClassification(mChannel)) {
      break;
    }
  }

  UC_LOG(("FeatureTask::CompleteClassification[%p] - exec callback", this));

  mCallbackHolder->Exec();
}

nsresult FeatureData::InitializeList(
    FeatureTask* aTask, nsIChannel* aChannel,
    nsIUrlClassifierFeature::listType aListType,
    nsTArray<RefPtr<TableData>>& aList) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTask);
  MOZ_ASSERT(aChannel);

  UC_LOG(("FeatureData::InitializeList[%p] - Initialize list %d for channel %p",
          this, aListType, aChannel));

  nsCOMPtr<nsIURI> uri;
  nsIUrlClassifierFeature::URIType URIType;
  nsresult rv = mFeature->GetURIByListType(aChannel, aListType, &URIType,
                                           getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (UC_LOG_ENABLED()) {
      nsAutoCString errorName;
      GetErrorName(rv, errorName);
      UC_LOG(("FeatureData::InitializeList[%p] got an unexpected error (rv=%s)",
              this, errorName.get()));
    }
    return rv;
  }

  if (!uri) {
    // Return success when the URI is empty to conitnue to do the lookup.
    UC_LOG(("FeatureData::InitializeList[%p] got an empty URL", this));
    return NS_OK;
  }

  nsCOMPtr<nsIURI> innermostURI = NS_GetInnermostURI(uri);
  if (NS_WARN_IF(!innermostURI)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString host;
  rv = innermostURI->GetHost(host);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool found = false;
  nsAutoCString tableName;
  rv = mFeature->HasHostInPreferences(host, aListType, tableName, &found);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (found) {
    mHostInPrefTables[aListType] = tableName;
  }

  RefPtr<URIData> uriData;
  rv = aTask->GetOrCreateURIData(uri, innermostURI, URIType,
                                 getter_AddRefs(uriData));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(uriData);

  nsTArray<nsCString> tables;
  rv = mFeature->GetTables(aListType, tables);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (const nsCString& table : tables) {
    RefPtr<TableData> data;
    rv = aTask->GetOrCreateTableData(uriData, table, getter_AddRefs(data));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(data);
    aList.AppendElement(data);
  }

  return NS_OK;
}

}  // namespace

/* static */
nsresult AsyncUrlChannelClassifier::CheckChannel(
    nsIChannel* aChannel, std::function<void()>&& aCallback) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aChannel);

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  UC_LOG(
      ("AsyncUrlChannelClassifier::CheckChannel starting the classification "
       "for channel %p",
       aChannel));

  RefPtr<FeatureTask> task;
  nsresult rv =
      FeatureTask::Create(aChannel, std::move(aCallback), getter_AddRefs(task));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!task) {
    // No task is needed for this channel, return an error so the caller won't
    // wait for a callback.
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsUrlClassifierDBServiceWorker> workerClassifier =
      nsUrlClassifierDBService::GetWorker();
  if (NS_WARN_IF(!workerClassifier)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "AsyncUrlChannelClassifier::CheckChannel",
      [task, workerClassifier]() -> void {
        MOZ_ASSERT(!NS_IsMainThread());
        task->DoLookup(workerClassifier);

        nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
            "AsyncUrlChannelClassifier::CheckChannel - return",
            [task]() -> void { task->CompleteClassification(); });

        NS_DispatchToMainThread(r);
      });

  return nsUrlClassifierDBService::BackgroundThread()->Dispatch(
      r, NS_DISPATCH_NORMAL);
}

}  // namespace net
}  // namespace mozilla
