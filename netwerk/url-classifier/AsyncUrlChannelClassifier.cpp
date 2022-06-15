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
// entitylist and blocklist prefs and tables. The reason why we create
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
  ~URIData() = default;

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

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::URIData::Create new URIData created for spec "
       "%s [this=%p]",
       data->mURISpec.get(), data.get()));

  data.forget(aData);
  return NS_OK;
}

URIData::URIData() { MOZ_ASSERT(NS_IsMainThread()); }

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

    if (mURIType == nsIUrlClassifierFeature::pairwiseEntitylistURI) {
      rv = LookupCache::GetLookupEntitylistFragments(mURISpec, &mFragments);
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

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::TableData CTOR - new TableData created %s "
       "[this=%p]",
       aTable.BeginReading(), this));
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
    UC_LOG_LEAK(
        ("AsyncChannelClassifier::TableData::DoLookup - starting lookup "
         "[this=%p]",
         this));

    const nsTArray<nsCString>& fragments = mURIData->Fragments();
    nsresult rv = aWorkerClassifier->DoSingleLocalLookupWithURIFragments(
        fragments, mTable, mResults);
    Unused << NS_WARN_IF(NS_FAILED(rv));

    mState = mResults.IsEmpty() ? TableData::eNoMatch : TableData::eMatch;

    UC_LOG_LEAK(
        ("AsyncChannelClassifier::TableData::DoLookup - lookup completed. "
         "Matches: %d [this=%p]",
         (int)mResults.Length(), this));
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
    eMatchBlocklist,
    eMatchEntitylist,
  };

 public:
  FeatureData() = default;
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

  State mState{eUnclassified};
  nsCOMPtr<nsIUrlClassifierFeature> mFeature;
  nsCOMPtr<nsIChannel> mChannel;

  nsTArray<RefPtr<TableData>> mBlocklistTables;
  nsTArray<RefPtr<TableData>> mEntitylistTables;

  // blocklist + entitylist.
  nsCString mHostInPrefTables[2];
};

FeatureData::~FeatureData() {
  NS_ReleaseOnMainThread("FeatureData:mFeature", mFeature.forget());
}

nsresult FeatureData::Initialize(FeatureTask* aTask, nsIChannel* aChannel,
                                 nsIUrlClassifierFeature* aFeature) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTask);
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(aFeature);

  if (UC_LOG_ENABLED()) {
    nsAutoCString name;
    aFeature->GetName(name);
    UC_LOG_LEAK(
        ("AsyncChannelClassifier::FeatureData::Initialize - Feature %s "
         "[this=%p, channel=%p]",
         name.get(), this, aChannel));
  }

  mFeature = aFeature;
  mChannel = aChannel;

  nsresult rv = InitializeList(
      aTask, aChannel, nsIUrlClassifierFeature::blocklist, mBlocklistTables);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = InitializeList(aTask, aChannel, nsIUrlClassifierFeature::entitylist,
                      mEntitylistTables);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

void FeatureData::DoLookup(nsUrlClassifierDBServiceWorker* aWorkerClassifier) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aWorkerClassifier);
  MOZ_ASSERT(mState == eUnclassified);

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureData::DoLookup - lookup starting "
       "[this=%p]",
       this));

  // This is wrong, but it's fast: we don't want to check if the host is in the
  // blocklist table if we know that it's going to be entitylisted by pref.
  // So, also if maybe it's not blocklisted, let's consider it 'entitylisted'.
  if (!mHostInPrefTables[nsIUrlClassifierFeature::entitylist].IsEmpty()) {
    UC_LOG_LEAK(
        ("AsyncChannelClassifier::FeatureData::DoLookup - entitylisted by pref "
         "[this=%p]",
         this));
    mState = eMatchEntitylist;
    return;
  }

  // Let's check if this feature blocklists the URI.

  bool isBlocklisted =
      !mHostInPrefTables[nsIUrlClassifierFeature::blocklist].IsEmpty();

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureData::DoLookup - blocklisted by pref: "
       "%d [this=%p]",
       isBlocklisted, this));

  if (!isBlocklisted) {
    // If one of the blocklist table matches the URI, we don't need to continue
    // with the others: the feature is blocklisted (but maybe also
    // entitylisted).
    for (TableData* tableData : mBlocklistTables) {
      if (tableData->DoLookup(aWorkerClassifier)) {
        isBlocklisted = true;
        break;
      }
    }
  }

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureData::DoLookup - blocklisted before "
       "entitylisting: %d [this=%p]",
       isBlocklisted, this));

  if (!isBlocklisted) {
    mState = eNoMatch;
    return;
  }

  // Now, let's check if we need to entitylist the same URI.

  for (TableData* tableData : mEntitylistTables) {
    // If one of the entitylist table matches the URI, we don't need to continue
    // with the others: the feature is entitylisted.
    if (tableData->DoLookup(aWorkerClassifier)) {
      UC_LOG_LEAK(
          ("AsyncChannelClassifier::FeatureData::DoLookup - entitylisted by "
           "table [this=%p]",
           this));
      mState = eMatchEntitylist;
      return;
    }
  }

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureData::DoLookup - blocklisted [this=%p]",
       this));
  mState = eMatchBlocklist;
}

bool FeatureData::MaybeCompleteClassification(nsIChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString name;
  mFeature->GetName(name);

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureData::MaybeCompleteClassification - "
       "completing "
       "classification [this=%p channel=%p]",
       this, aChannel));

  switch (mState) {
    case eNoMatch:
      UC_LOG(
          ("AsyncChannelClassifier::FeatureData::MaybeCompleteClassification - "
           "no match for feature %s. Let's "
           "move on [this=%p channel=%p]",
           name.get(), this, aChannel));
      return true;

    case eMatchEntitylist:
      UC_LOG(
          ("AsyncChannelClassifier::FeatureData::MayebeCompleteClassification "
           "- entitylisted by feature %s. Let's "
           "move on [this=%p channel=%p]",
           name.get(), this, aChannel));
      return true;

    case eMatchBlocklist:
      UC_LOG(
          ("AsyncChannelClassifier::FeatureData::MaybeCompleteClassification - "
           "blocklisted by feature %s [this=%p channel=%p]",
           name.get(), this, aChannel));
      break;

    case eUnclassified:
      MOZ_CRASH("We should not be here!");
      break;
  }

  MOZ_ASSERT(mState == eMatchBlocklist);

  // Maybe we have to ignore this host
  nsAutoCString exceptionList;
  nsresult rv = mFeature->GetExceptionHostList(exceptionList);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    UC_LOG_WARN(
        ("AsyncChannelClassifier::FeatureData::MayebeCompleteClassification - "
         "error. Let's move on [this=%p channel=%p]",
         this, aChannel));
    return true;
  }

  if (!mBlocklistTables.IsEmpty() &&
      nsContentUtils::IsURIInList(mBlocklistTables[0]->URI(), exceptionList)) {
    nsCString spec = mBlocklistTables[0]->URI()->GetSpecOrDefault();
    spec.Truncate(std::min(spec.Length(), UrlClassifierCommon::sMaxSpecLength));
    UC_LOG(
        ("AsyncChannelClassifier::FeatureData::MaybeCompleteClassification - "
         "uri %s found in "
         "exceptionlist of feature %s [this=%p channel=%p]",
         spec.get(), name.get(), this, aChannel));
    return true;
  }

  nsTArray<nsCString> list;
  nsTArray<nsCString> hashes;
  if (!mHostInPrefTables[nsIUrlClassifierFeature::blocklist].IsEmpty()) {
    list.AppendElement(mHostInPrefTables[nsIUrlClassifierFeature::blocklist]);

    // Telemetry expects every tracking channel has hash, create it for test
    // entry
    Completion complete;
    complete.FromPlaintext(
        mHostInPrefTables[nsIUrlClassifierFeature::blocklist]);
    hashes.AppendElement(complete.ToString());
  }

  for (TableData* tableData : mBlocklistTables) {
    if (tableData->MatchState() == TableData::eMatch) {
      list.AppendElement(tableData->Table());

      for (const auto& r : tableData->Result()) {
        hashes.AppendElement(r->hash.complete.ToString());
      }
    }
  }

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureData::MaybeCompleteClassification - "
       "process channel [this=%p channel=%p]",
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
// tracking-annotation feature uses the top-level URI to entitylist the current
// channel's URI.  Because of
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
    UC_LOG(
        ("AsyncChannelClassifier::FeatureTask::Create - no task is needed for "
         "channel %p",
         aChannel));
    return NS_OK;
  }

  RefPtr<FeatureTask> task = new FeatureTask(aChannel, std::move(aCallback));

  UC_LOG(
      ("AsyncChannelClassifier::FeatureTask::Create - FeatureTask %p created "
       "for channel %p",
       task.get(), aChannel));

  for (nsIUrlClassifierFeature* feature : features) {
    FeatureData* featureData = task->mFeatures.AppendElement();
    nsresult rv = featureData->Initialize(task, aChannel, feature);
    if (NS_FAILED(rv)) {
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

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureTask::GetOrCreateURIData - checking if "
       "a URIData must be "
       "created [this=%p]",
       this));

  for (URIData* data : mURIs) {
    if (data->IsEqual(aURI)) {
      UC_LOG_LEAK(
          ("AsyncChannelClassifier::FeatureTask::GetOrCreateURIData - reuse "
           "existing URIData %p [this=%p]",
           data, this));

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

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureTask::GetOrCreateURIData - create new "
       "URIData %p [this=%p]",
       data.get(), this));

  data.forget(aData);
  return NS_OK;
}

nsresult FeatureTask::GetOrCreateTableData(URIData* aURIData,
                                           const nsACString& aTable,
                                           TableData** aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURIData);
  MOZ_ASSERT(aData);

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureTask::GetOrCreateTableData - checking "
       "if TableData must be "
       "created [this=%p]",
       this));

  for (TableData* data : mTables) {
    if (data->IsEqual(aURIData, aTable)) {
      UC_LOG_LEAK(
          ("FeatureTask::GetOrCreateTableData - reuse existing TableData %p "
           "[this=%p]",
           data, this));

      RefPtr<TableData> tableData = data;
      tableData.forget(aData);
      return NS_OK;
    }
  }

  RefPtr<TableData> data = new TableData(aURIData, aTable);
  mTables.AppendElement(data);

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureTask::GetOrCreateTableData - create new "
       "TableData %p [this=%p]",
       data.get(), this));

  data.forget(aData);
  return NS_OK;
}

void FeatureTask::DoLookup(nsUrlClassifierDBServiceWorker* aWorkerClassifier) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aWorkerClassifier);

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureTask::DoLookup - starting lookup "
       "[this=%p]",
       this));

  for (FeatureData& feature : mFeatures) {
    feature.DoLookup(aWorkerClassifier);
  }

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureTask::DoLookup - lookup completed "
       "[this=%p]",
       this));
}

void FeatureTask::CompleteClassification() {
  MOZ_ASSERT(NS_IsMainThread());

  for (FeatureData& feature : mFeatures) {
    if (!feature.MaybeCompleteClassification(mChannel)) {
      break;
    }
  }

  UC_LOG(
      ("AsyncChannelClassifier::FeatureTask::CompleteClassification - complete "
       "classification for "
       "channel %p [this=%p]",
       mChannel.get(), this));

  mCallbackHolder->Exec();
}

nsresult FeatureData::InitializeList(
    FeatureTask* aTask, nsIChannel* aChannel,
    nsIUrlClassifierFeature::listType aListType,
    nsTArray<RefPtr<TableData>>& aList) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTask);
  MOZ_ASSERT(aChannel);

  UC_LOG_LEAK(
      ("AsyncChannelClassifier::FeatureData::InitializeList - initialize list "
       "%d for channel %p [this=%p]",
       aListType, aChannel, this));

  nsCOMPtr<nsIURI> uri;
  nsIUrlClassifierFeature::URIType URIType;
  nsresult rv = mFeature->GetURIByListType(aChannel, aListType, &URIType,
                                           getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    if (UC_LOG_ENABLED()) {
      nsAutoCString errorName;
      GetErrorName(rv, errorName);
      UC_LOG_LEAK(
          ("AsyncChannelClassifier::FeatureData::InitializeList - Got an "
           "unexpected error (rv=%s) [this=%p]",
           errorName.get(), this));
    }
    return rv;
  }

  if (!uri) {
    // Return success when the URI is empty to conitnue to do the lookup.
    UC_LOG_LEAK(
        ("AsyncChannelClassifier::FeatureData::InitializeList - got an empty "
         "URL [this=%p]",
         this));
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

  if (UC_LOG_ENABLED()) {
    nsCOMPtr<nsIURI> chanURI;
    if (NS_SUCCEEDED(aChannel->GetURI(getter_AddRefs(chanURI)))) {
      nsCString chanSpec = chanURI->GetSpecOrDefault();
      chanSpec.Truncate(
          std::min(chanSpec.Length(), UrlClassifierCommon::sMaxSpecLength));

      nsCOMPtr<nsIURI> topWinURI;
      Unused << UrlClassifierCommon::GetTopWindowURI(aChannel,
                                                     getter_AddRefs(topWinURI));
      nsCString topWinSpec =
          topWinURI ? topWinURI->GetSpecOrDefault() : "(null)"_ns;

      topWinSpec.Truncate(
          std::min(topWinSpec.Length(), UrlClassifierCommon::sMaxSpecLength));

      UC_LOG(
          ("AsyncUrlChannelClassifier::CheckChannel - starting the "
           "classification on channel %p",
           aChannel));
      UC_LOG(("    uri is %s [channel=%p]", chanSpec.get(), aChannel));
      UC_LOG(
          ("    top-level uri is %s [channel=%p]", topWinSpec.get(), aChannel));
    }
  }

  RefPtr<FeatureTask> task;
  nsresult rv =
      FeatureTask::Create(aChannel, std::move(aCallback), getter_AddRefs(task));
  if (NS_FAILED(rv)) {
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
