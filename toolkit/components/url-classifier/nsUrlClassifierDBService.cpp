//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsArrayUtils.h"
#include "nsCRT.h"
#include "nsICryptoHash.h"
#include "nsICryptoHMAC.h"
#include "nsIDirectoryService.h"
#include "nsIKeyModule.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIProperties.h"
#include "nsToolkitCompsCID.h"
#include "nsIXULRuntime.h"
#include "nsUrlClassifierDBService.h"
#include "nsUrlClassifierUtils.h"
#include "nsUrlClassifierProxies.h"
#include "nsURILoader.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsTArray.h"
#include "nsNetCID.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Mutex.h"
#include "mozilla/Preferences.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Logging.h"
#include "prnetdb.h"
#include "Entries.h"
#include "HashStore.h"
#include "Classifier.h"
#include "ProtocolParser.h"
#include "mozilla/Attributes.h"
#include "nsIPrincipal.h"
#include "Classifier.h"
#include "ProtocolParser.h"
#include "nsContentUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/URLClassifierChild.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsProxyRelease.h"
#include "SBTelemetryUtils.h"

namespace mozilla {
namespace safebrowsing {

nsresult
TablesToResponse(const nsACString& tables)
{
  if (tables.IsEmpty()) {
    return NS_OK;
  }

  // We don't check mCheckMalware and friends because disabled tables are
  // never included
  if (FindInReadable(NS_LITERAL_CSTRING("-malware-"), tables)) {
    return NS_ERROR_MALWARE_URI;
  }
  if (FindInReadable(NS_LITERAL_CSTRING("-phish-"), tables)) {
    return NS_ERROR_PHISHING_URI;
  }
  if (FindInReadable(NS_LITERAL_CSTRING("-unwanted-"), tables)) {
    return NS_ERROR_UNWANTED_URI;
  }
  if (FindInReadable(NS_LITERAL_CSTRING("-track-"), tables)) {
    return NS_ERROR_TRACKING_URI;
  }
  if (FindInReadable(NS_LITERAL_CSTRING("-block-"), tables)) {
    return NS_ERROR_BLOCKED_URI;
  }
  return NS_OK;
}

} // namespace safebrowsing
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::safebrowsing;

// MOZ_LOG=UrlClassifierDbService:5
LazyLogModule gUrlClassifierDbServiceLog("UrlClassifierDbService");
#define LOG(args) MOZ_LOG(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gUrlClassifierDbServiceLog, mozilla::LogLevel::Debug)

#define GETHASH_NOISE_PREF      "urlclassifier.gethashnoise"
#define GETHASH_NOISE_DEFAULT   4

#define CONFIRM_AGE_PREF        "urlclassifier.max-complete-age"
#define CONFIRM_AGE_DEFAULT_SEC (45 * 60)

// 30 minutes as the maximum negative cache duration.
#define MAXIMUM_NEGATIVE_CACHE_DURATION_SEC (30 * 60 * 1000)

// TODO: The following two prefs are to be removed after we
//       roll out full v4 hash completion. See Bug 1331534.
#define TAKE_V4_COMPLETION_RESULT_PREF    "browser.safebrowsing.temporary.take_v4_completion_result"
#define TAKE_V4_COMPLETION_RESULT_DEFAULT false

class nsUrlClassifierDBServiceWorker;

// Singleton instance.
static nsUrlClassifierDBService* sUrlClassifierDBService;

nsIThread* nsUrlClassifierDBService::gDbBackgroundThread = nullptr;

// Once we've committed to shutting down, don't do work in the background
// thread.
static bool gShuttingDownThread = false;

static mozilla::Atomic<uint32_t, Relaxed> gFreshnessGuarantee(CONFIRM_AGE_DEFAULT_SEC);
static uint32_t sGethashNoise = GETHASH_NOISE_DEFAULT;

NS_IMPL_ISUPPORTS(nsUrlClassifierDBServiceWorker,
                  nsIUrlClassifierDBService)

nsUrlClassifierDBServiceWorker::nsUrlClassifierDBServiceWorker()
  : mInStream(false)
  , mGethashNoise(0)
  , mPendingLookupLock("nsUrlClassifierDBServerWorker.mPendingLookupLock")
{
}

nsUrlClassifierDBServiceWorker::~nsUrlClassifierDBServiceWorker()
{
  NS_ASSERTION(!mClassifier,
               "Db connection not closed, leaking memory!  Call CloseDb "
               "to close the connection.");
}

nsresult
nsUrlClassifierDBServiceWorker::Init(uint32_t aGethashNoise,
                                     nsCOMPtr<nsIFile> aCacheDir)
{
  mGethashNoise = aGethashNoise;
  mCacheDir = aCacheDir;

  ResetUpdate();

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::QueueLookup(const nsACString& spec,
                                            const nsACString& tables,
                                            nsIUrlClassifierLookupCallback* callback)
{
  MutexAutoLock lock(mPendingLookupLock);
  if (gShuttingDownThread) {
      return NS_ERROR_ABORT;
  }

  PendingLookup* lookup = mPendingLookups.AppendElement();
  if (!lookup) return NS_ERROR_OUT_OF_MEMORY;

  lookup->mStartTime = TimeStamp::Now();
  lookup->mKey = spec;
  lookup->mCallback = callback;
  lookup->mTables = tables;

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::DoLocalLookup(const nsACString& spec,
                                              const nsACString& tables,
                                              LookupResultArray* results)
{
  if (gShuttingDownThread) {
    return NS_ERROR_ABORT;
  }

  MOZ_ASSERT(!NS_IsMainThread(), "DoLocalLookup must be on background thread");
  if (!results) {
    return NS_ERROR_FAILURE;
  }
  // Bail if we haven't been initialized on the background thread.
  if (!mClassifier) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // We ignore failures from Check because we'd rather return the
  // results that were found than fail.
  mClassifier->Check(spec, tables, gFreshnessGuarantee, *results);

  LOG(("Found %" PRIuSIZE " results.", results->Length()));
  return NS_OK;
}

static nsresult
ProcessLookupResults(LookupResultArray* results, nsTArray<nsCString>& tables)
{
  // Build the result array.
  for (uint32_t i = 0; i < results->Length(); i++) {
    LookupResult& result = results->ElementAt(i);
    MOZ_ASSERT(!result.mNoise, "Lookup results should not have noise added");
    LOG(("Found result from table %s", result.mTableName.get()));
    if (tables.IndexOf(result.mTableName) == nsTArray<nsCString>::NoIndex) {
      tables.AppendElement(result.mTableName);
    }
  }
  return NS_OK;
}

/**
 * Lookup up a key in the database is a two step process:
 *
 * a) First we look for any Entries in the database that might apply to this
 *    url.  For each URL there are one or two possible domain names to check:
 *    the two-part domain name (example.com) and the three-part name
 *    (www.example.com).  We check the database for both of these.
 * b) If we find any entries, we check the list of fragments for that entry
 *    against the possible subfragments of the URL as described in the
 *    "Simplified Regular Expression Lookup" section of the protocol doc.
 */
nsresult
nsUrlClassifierDBServiceWorker::DoLookup(const nsACString& spec,
                                         const nsACString& tables,
                                         nsIUrlClassifierLookupCallback* c)
{
  if (gShuttingDownThread) {
    c->LookupComplete(nullptr);
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRIntervalTime clockStart = 0;
  if (LOG_ENABLED()) {
    clockStart = PR_IntervalNow();
  }

  nsAutoPtr<LookupResultArray> results(new LookupResultArray());
  if (!results) {
    c->LookupComplete(nullptr);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = DoLocalLookup(spec, tables, results);
  if (NS_FAILED(rv)) {
    c->LookupComplete(nullptr);
    return rv;
  }

  LOG(("Found %" PRIuSIZE " results.", results->Length()));


  if (LOG_ENABLED()) {
    PRIntervalTime clockEnd = PR_IntervalNow();
    LOG(("query took %dms\n",
         PR_IntervalToMilliseconds(clockEnd - clockStart)));
  }

  nsAutoPtr<LookupResultArray> completes(new LookupResultArray());

  for (uint32_t i = 0; i < results->Length(); i++) {
    const LookupResult& lookupResult = results->ElementAt(i);

    // mMissCache should only be used in V2.
    if (!lookupResult.mProtocolV2 ||
        !mMissCache.Contains(lookupResult.hash.fixedLengthPrefix)) {
      completes->AppendElement(lookupResult);
    }
  }

  for (uint32_t i = 0; i < completes->Length(); i++) {
    if (!completes->ElementAt(i).Confirmed()) {
      // We're going to be doing a gethash request, add some extra entries.
      // Note that we cannot pass the first two by reference, because we
      // add to completes, whicah can cause completes to reallocate and move.
      AddNoise(completes->ElementAt(i).hash.fixedLengthPrefix,
               completes->ElementAt(i).mTableName,
               mGethashNoise, *completes);
      break;
    }
  }

  // At this point ownership of 'results' is handed to the callback.
  c->LookupComplete(completes.forget());

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::HandlePendingLookups()
{
  if (gShuttingDownThread) {
    return NS_ERROR_ABORT;
  }

  MutexAutoLock lock(mPendingLookupLock);
  while (mPendingLookups.Length() > 0) {
    PendingLookup lookup = mPendingLookups[0];
    mPendingLookups.RemoveElementAt(0);
    {
      MutexAutoUnlock unlock(mPendingLookupLock);
      DoLookup(lookup.mKey, lookup.mTables, lookup.mCallback);
    }
    double lookupTime = (TimeStamp::Now() - lookup.mStartTime).ToMilliseconds();
    Telemetry::Accumulate(Telemetry::URLCLASSIFIER_LOOKUP_TIME_2,
                          static_cast<uint32_t>(lookupTime));
  }

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::AddNoise(const Prefix aPrefix,
                                         const nsCString tableName,
                                         uint32_t aCount,
                                         LookupResultArray& results)
{
  if (gShuttingDownThread) {
    return NS_ERROR_ABORT;
  }

  if (aCount < 1) {
    return NS_OK;
  }

  PrefixArray noiseEntries;
  nsresult rv = mClassifier->ReadNoiseEntries(aPrefix, tableName,
                                              aCount, &noiseEntries);
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < noiseEntries.Length(); i++) {
    LookupResult *result = results.AppendElement();
    if (!result)
      return NS_ERROR_OUT_OF_MEMORY;

    result->hash.fixedLengthPrefix = noiseEntries[i];
    result->mNoise = true;
    result->mPartialHashLength = PREFIX_SIZE; // Noise is always 4-byte,
    result->mTableName.Assign(tableName);
  }

  return NS_OK;
}

// Lookup a key in the db.
NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::Lookup(nsIPrincipal* aPrincipal,
                                       const nsACString& aTables,
                                       nsIUrlClassifierCallback* c)
{
  if (gShuttingDownThread) {
    return NS_ERROR_ABORT;
  }

  return HandlePendingLookups();
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::GetTables(nsIUrlClassifierCallback* c)
{
  if (gShuttingDownThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsresult rv = OpenDb();
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to open SafeBrowsing database");
    return NS_ERROR_FAILURE;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString response;
  mClassifier->TableRequest(response);
  LOG(("GetTables: %s", response.get()));
  c->HandleEvent(response);

  return rv;
}

void
nsUrlClassifierDBServiceWorker::ResetStream()
{
  LOG(("ResetStream"));
  mInStream = false;
  mProtocolParser = nullptr;
}

void
nsUrlClassifierDBServiceWorker::ResetUpdate()
{
  LOG(("ResetUpdate"));
  mUpdateWaitSec = 0;
  mUpdateStatus = NS_OK;
  mUpdateObserver = nullptr;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::SetHashCompleter(const nsACString &tableName,
                                                 nsIUrlClassifierHashCompleter *completer)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::BeginUpdate(nsIUrlClassifierUpdateObserver *observer,
                                            const nsACString &tables)
{
  LOG(("nsUrlClassifierDBServiceWorker::BeginUpdate [%s]", PromiseFlatCString(tables).get()));

  if (gShuttingDownThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ENSURE_STATE(!mUpdateObserver);

  nsresult rv = OpenDb();
  if (NS_FAILED(rv)) {
    NS_ERROR("Unable to open SafeBrowsing database");
    return NS_ERROR_FAILURE;
  }

  mUpdateStatus = NS_OK;
  mUpdateObserver = observer;
  Classifier::SplitTables(tables, mUpdateTables);

  return NS_OK;
}

// Called from the stream updater.
NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::BeginStream(const nsACString &table)
{
  LOG(("nsUrlClassifierDBServiceWorker::BeginStream"));
  MOZ_ASSERT(!NS_IsMainThread(), "Streaming must be on the background thread");

  if (gShuttingDownThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ENSURE_STATE(mUpdateObserver);
  NS_ENSURE_STATE(!mInStream);

  mInStream = true;

  NS_ASSERTION(!mProtocolParser, "Should not have a protocol parser.");

  // Check if we should use protobuf to parse the update.
  bool useProtobuf = false;
  for (size_t i = 0; i < mUpdateTables.Length(); i++) {
    bool isCurProtobuf =
      StringEndsWith(mUpdateTables[i], NS_LITERAL_CSTRING("-proto"));

    if (0 == i) {
      // Use the first table name to decice if all the subsequent tables
      // should be '-proto'.
      useProtobuf = isCurProtobuf;
      continue;
    }

    if (useProtobuf != isCurProtobuf) {
      NS_WARNING("Cannot mix 'proto' tables with other types "
                 "within the same provider.");
      break;
    }
  }

  mProtocolParser = (useProtobuf ? static_cast<ProtocolParser*>(new ProtocolParserProtobuf())
                                 : static_cast<ProtocolParser*>(new ProtocolParserV2()));
  if (!mProtocolParser)
    return NS_ERROR_OUT_OF_MEMORY;

  mProtocolParser->Init(mCryptoHash);

  if (!table.IsEmpty()) {
    mProtocolParser->SetCurrentTable(table);
  }

  mProtocolParser->SetRequestedTables(mUpdateTables);

  return NS_OK;
}

/**
 * Updating the database:
 *
 * The Update() method takes a series of chunks separated with control data,
 * as described in
 * http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec
 *
 * It will iterate through the control data until it reaches a chunk.  By
 * the time it reaches a chunk, it should have received
 * a) the table to which this chunk applies
 * b) the type of chunk (add, delete, expire add, expire delete).
 * c) the chunk ID
 * d) the length of the chunk.
 *
 * For add and subtract chunks, it needs to read the chunk data (expires
 * don't have any data).  Chunk data is a list of URI fragments whose
 * encoding depends on the type of table (which is indicated by the end
 * of the table name):
 * a) tables ending with -exp are a zlib-compressed list of URI fragments
 *    separated by newlines.
 * b) tables ending with -sha128 have the form
 *    [domain][N][frag0]...[fragN]
 *       16    1   16        16
 *    If N is 0, the domain is reused as a fragment.
 * c) any other tables are assumed to be a plaintext list of URI fragments
 *    separated by newlines.
 *
 * Update() can be fed partial data;  It will accumulate data until there is
 * enough to act on.  Finish() should be called when there will be no more
 * data.
 */
NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::UpdateStream(const nsACString& chunk)
{
  if (gShuttingDownThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ENSURE_STATE(mInStream);

  HandlePendingLookups();

  // Feed the chunk to the parser.
  return mProtocolParser->AppendStream(chunk);
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::FinishStream()
{
  if (gShuttingDownThread) {
    LOG(("shutting down"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ENSURE_STATE(mInStream);
  NS_ENSURE_STATE(mUpdateObserver);

  mInStream = false;

  mProtocolParser->End();

  if (NS_SUCCEEDED(mProtocolParser->Status())) {
    if (mProtocolParser->UpdateWaitSec()) {
      mUpdateWaitSec = mProtocolParser->UpdateWaitSec();
    }
    // XXX: Only allow forwards from the initial update?
    const nsTArray<ProtocolParser::ForwardedUpdate> &forwards =
      mProtocolParser->Forwards();
    for (uint32_t i = 0; i < forwards.Length(); i++) {
      const ProtocolParser::ForwardedUpdate &forward = forwards[i];
      mUpdateObserver->UpdateUrlRequested(forward.url, forward.table);
    }
    // Hold on to any TableUpdate objects that were created by the
    // parser.
    mTableUpdates.AppendElements(mProtocolParser->GetTableUpdates());
    mProtocolParser->ForgetTableUpdates();

#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES
    // The assignment involves no string copy since the source string is sharable.
    mRawTableUpdates = mProtocolParser->GetRawTableUpdates();
#endif
  } else {
    LOG(("nsUrlClassifierDBService::FinishStream Failed to parse the stream "
         "using mProtocolParser."));
    mUpdateStatus = mProtocolParser->Status();
  }
  mUpdateObserver->StreamFinished(mProtocolParser->Status(), 0);

  if (NS_SUCCEEDED(mUpdateStatus)) {
    if (mProtocolParser->ResetRequested()) {
      mClassifier->ResetTables(Classifier::Clear_All, mUpdateTables);
    }
  } else {
    mUpdateStatus = NS_ERROR_UC_UPDATE_PROTOCOL_PARSER_ERROR;
  }

  mProtocolParser = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::FinishUpdate()
{
  LOG(("nsUrlClassifierDBServiceWorker::FinishUpdate"));

  MOZ_ASSERT(!NS_IsMainThread(), "nsUrlClassifierDBServiceWorker::FinishUpdate "
                                 "NUST NOT be on the main thread.");

  if (gShuttingDownThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(!mProtocolParser, "Should have been nulled out in FinishStream() "
                               "or never created.");

  NS_ENSURE_STATE(mUpdateObserver);

  if (NS_FAILED(mUpdateStatus)) {
    LOG(("nsUrlClassifierDBServiceWorker::FinishUpdate() Not running "
         "ApplyUpdate() since the update has already failed."));
    return NotifyUpdateObserver(mUpdateStatus);
  }

  if (mTableUpdates.IsEmpty()) {
    LOG(("Nothing to update. Just notify update observer."));
    return NotifyUpdateObserver(NS_OK);
  }

  RefPtr<nsUrlClassifierDBServiceWorker> self = this;
  nsresult rv = mClassifier->AsyncApplyUpdates(&mTableUpdates,
                                               [=] (nsresult aRv) -> void {
#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES
    if (NS_FAILED(aRv) &&
        NS_ERROR_OUT_OF_MEMORY != aRv &&
        NS_ERROR_UC_UPDATE_SHUTDOWNING != aRv) {
      self->mClassifier->DumpRawTableUpdates(mRawTableUpdates);
    }
    // Invalidate the raw table updates.
    self->mRawTableUpdates = EmptyCString();
#endif

    self->NotifyUpdateObserver(aRv);
  });

  if (NS_FAILED(rv)) {
    LOG(("Failed to start async update. Notify immediately."));
    NotifyUpdateObserver(rv);
  }

  return rv;
}

nsresult
nsUrlClassifierDBServiceWorker::NotifyUpdateObserver(nsresult aUpdateStatus)
{
  MOZ_ASSERT(!NS_IsMainThread(), "nsUrlClassifierDBServiceWorker::NotifyUpdateObserver "
                                 "NUST NOT be on the main thread.");

  LOG(("nsUrlClassifierDBServiceWorker::NotifyUpdateObserver"));

  // We've either
  //  1) failed starting a download stream
  //  2) succeeded in starting a download stream but failed to obtain
  //     table updates
  //  3) succeeded in obtaining table updates but failed to build new
  //     tables.
  //  4) succeeded in building new tables but failed to take them.
  //  5) succeeded in taking new tables.

  mUpdateStatus = aUpdateStatus;

  nsCOMPtr<nsIUrlClassifierUtils> urlUtil =
    do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID);

  nsCString provider;
  // Assume that all the tables in update should have the same provider.
  urlUtil->GetTelemetryProvider(mUpdateTables.SafeElementAt(0, EmptyCString()), provider);

  nsresult updateStatus = mUpdateStatus;
  if (NS_FAILED(mUpdateStatus)) {
   updateStatus = NS_ERROR_GET_MODULE(mUpdateStatus) == NS_ERROR_MODULE_URL_CLASSIFIER ?
     mUpdateStatus : NS_ERROR_UC_UPDATE_UNKNOWN;
  }

  // Do not record telemetry for testing tables.
  if (!provider.Equals(TESTING_TABLE_PROVIDER_NAME)) {
    Telemetry::Accumulate(Telemetry::URLCLASSIFIER_UPDATE_ERROR, provider,
                          NS_ERROR_GET_CODE(updateStatus));
  }

  mMissCache.Clear();

  // Null out mUpdateObserver before notifying so that BeginUpdate()
  // becomes available prior to callback.
  nsCOMPtr<nsIUrlClassifierUpdateObserver> updateObserver = nullptr;
  updateObserver.swap(mUpdateObserver);

  if (NS_SUCCEEDED(mUpdateStatus)) {
    LOG(("Notifying success: %d", mUpdateWaitSec));
    updateObserver->UpdateSuccess(mUpdateWaitSec);
  } else if (NS_ERROR_NOT_IMPLEMENTED == mUpdateStatus) {
    LOG(("Treating NS_ERROR_NOT_IMPLEMENTED a successful update "
         "but still mark it spoiled."));
    updateObserver->UpdateSuccess(0);
    mClassifier->ResetTables(Classifier::Clear_Cache, mUpdateTables);
  } else {
    if (LOG_ENABLED()) {
      nsAutoCString errorName;
      mozilla::GetErrorName(mUpdateStatus, errorName);
      LOG(("Notifying error: %s (%" PRIu32 ")", errorName.get(),
           static_cast<uint32_t>(mUpdateStatus)));
    }

    updateObserver->UpdateError(mUpdateStatus);
    /*
     * mark the tables as spoiled(clear cache in LookupCache), we don't want to
     * block hosts longer than normal because our update failed
    */
    mClassifier->ResetTables(Classifier::Clear_Cache, mUpdateTables);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::ResetDatabase()
{
  nsresult rv = OpenDb();

  if (NS_SUCCEEDED(rv)) {
    mClassifier->Reset();
  }

  rv = CloseDb();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::ReloadDatabase()
{
  nsTArray<nsCString> tables;
  nsTArray<int64_t> lastUpdateTimes;
  nsresult rv = mClassifier->ActiveTables(tables);
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to make sure lastupdatetime is set after reload database
  // Otherwise request will be skipped if it is not confirmed.
  for (uint32_t table = 0; table < tables.Length(); table++) {
    lastUpdateTimes.AppendElement(mClassifier->GetLastUpdateTime(tables[table]));
  }

  // This will null out mClassifier
  rv = CloseDb();
  NS_ENSURE_SUCCESS(rv, rv);

  // Create new mClassifier and load prefixset and completions from disk.
  rv = OpenDb();
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t table = 0; table < tables.Length(); table++) {
    int64_t time = lastUpdateTimes[table];
    if (time) {
      mClassifier->SetLastUpdateTime(tables[table], lastUpdateTimes[table]);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::CancelUpdate()
{
  LOG(("nsUrlClassifierDBServiceWorker::CancelUpdate"));

  if (mUpdateObserver) {
    LOG(("UpdateObserver exists, cancelling"));

    mUpdateStatus = NS_BINDING_ABORTED;

    mUpdateObserver->UpdateError(mUpdateStatus);

    /*
     * mark the tables as spoiled(clear cache in LookupCache), we don't want to
     * block hosts longer than normal because our update failed
    */
    mClassifier->ResetTables(Classifier::Clear_Cache, mUpdateTables);

    ResetStream();
    ResetUpdate();
  } else {
    LOG(("No UpdateObserver, nothing to cancel"));
  }

  return NS_OK;
}

void
nsUrlClassifierDBServiceWorker::FlushAndDisableAsyncUpdate()
{
  LOG(("nsUrlClassifierDBServiceWorker::FlushAndDisableAsyncUpdate()"));

  if (mClassifier) {
    mClassifier->FlushAndDisableAsyncUpdate();
  }
}

// Allows the main thread to delete the connection which may be in
// a background thread.
// XXX This could be turned into a single shutdown event so the logic
// is simpler in nsUrlClassifierDBService::Shutdown.
nsresult
nsUrlClassifierDBServiceWorker::CloseDb()
{
  if (mClassifier) {
    mClassifier->Close();
    mClassifier = nullptr;
  }

  mCryptoHash = nullptr;
  LOG(("urlclassifier db closed\n"));

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::CacheCompletions(CacheResultArray *results)
{
  if (gShuttingDownThread) {
    return NS_ERROR_ABORT;
  }

  LOG(("nsUrlClassifierDBServiceWorker::CacheCompletions [%p]", this));
  if (!mClassifier) {
    return NS_OK;
  }

  // Ownership is transferred in to us
  nsAutoPtr<CacheResultArray> resultsPtr(results);

  if (resultsPtr->Length() == 0) {
    return NS_OK;
  }

  if (IsSameAsLastResults(*resultsPtr)) {
    LOG(("Skipping completions that have just been cached already."));
    return NS_OK;
  }

  // Only cache results for tables that we have, don't take
  // in tables we might accidentally have hit during a completion.
  // This happens due to goog vs googpub lists existing.
  nsTArray<nsCString> tables;
  nsresult rv = mClassifier->ActiveTables(tables);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<TableUpdate*> updates;

  for (uint32_t i = 0; i < resultsPtr->Length(); i++) {
    bool activeTable = false;
    CacheResult* result = resultsPtr->ElementAt(i).get();

    for (uint32_t table = 0; table < tables.Length(); table++) {
      if (tables[table].Equals(result->table)) {
        activeTable = true;
        break;
      }
    }
    if (activeTable) {
      nsAutoPtr<ProtocolParser> pParse;
      pParse = resultsPtr->ElementAt(i)->Ver() == CacheResult::V2 ?
                 static_cast<ProtocolParser*>(new ProtocolParserV2()) :
                 static_cast<ProtocolParser*>(new ProtocolParserProtobuf());

      TableUpdate* tu = pParse->GetTableUpdate(result->table);

      rv = CacheResultToTableUpdate(result, tu);
      if (NS_FAILED(rv)) {
        // We can bail without leaking here because ForgetTableUpdates
        // hasn't been called yet.
        return rv;
      }
      updates.AppendElement(tu);
      pParse->ForgetTableUpdates();
    } else {
      LOG(("Completion received, but table is not active, so not caching."));
    }
   }

  mClassifier->ApplyFullHashes(&updates);
  mLastResults = Move(resultsPtr);
  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::CacheResultToTableUpdate(CacheResult* aCacheResult,
                                                         TableUpdate* aUpdate)
{
  auto tuV2 = TableUpdate::Cast<TableUpdateV2>(aUpdate);
  if (tuV2) {
    auto result = CacheResult::Cast<CacheResultV2>(aCacheResult);
    MOZ_ASSERT(result);

    LOG(("CacheCompletion hash %X, Addchunk %d", result->completion.ToUint32(),
         result->addChunk));

    nsresult rv = tuV2->NewAddComplete(result->addChunk, result->completion);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = tuV2->NewAddChunk(result->addChunk);
    return rv;
  }

  auto tuV4 = TableUpdate::Cast<TableUpdateV4>(aUpdate);
  if (tuV4) {
    auto result = CacheResult::Cast<CacheResultV4>(aCacheResult);
    MOZ_ASSERT(result);

    if (LOG_ENABLED()) {
      const FullHashExpiryCache& fullHashes = result->response.fullHashes;
      for (auto iter = fullHashes.ConstIter(); !iter.Done(); iter.Next()) {
        Completion completion;
        completion.Assign(iter.Key());
        LOG(("CacheCompletion(v4) hash %X, CacheExpireTime %" PRId64,
             completion.ToUint32(), iter.Data()));
      }
    }

    tuV4->NewFullHashResponse(result->prefix, result->response);
    return NS_OK;
  }

  // tableUpdate object should be either V2 or V4.
  return NS_ERROR_FAILURE;
}

nsresult
nsUrlClassifierDBServiceWorker::CacheMisses(PrefixArray *results)
{
  LOG(("nsUrlClassifierDBServiceWorker::CacheMisses [%p] %" PRIuSIZE,
       this, results->Length()));

  // Ownership is transferred in to us
  nsAutoPtr<PrefixArray> resultsPtr(results);

  for (uint32_t i = 0; i < resultsPtr->Length(); i++) {
    mMissCache.AppendElement(resultsPtr->ElementAt(i));
  }
  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::OpenDb()
{
  if (gShuttingDownThread) {
    return NS_ERROR_ABORT;
  }

  MOZ_ASSERT(!NS_IsMainThread(), "Must initialize DB on background thread");
  // Connection already open, don't do anything.
  if (mClassifier) {
    return NS_OK;
  }

  nsresult rv;
  mCryptoHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoPtr<Classifier> classifier(new Classifier());
  if (!classifier) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = classifier->Open(*mCacheDir);
  NS_ENSURE_SUCCESS(rv, rv);

  mClassifier = classifier;

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::SetLastUpdateTime(const nsACString &table,
                                                  uint64_t updateTime)
{
  MOZ_ASSERT(!NS_IsMainThread(), "Must be on the background thread");
  MOZ_ASSERT(mClassifier, "Classifier connection must be opened");

  mClassifier->SetLastUpdateTime(table, updateTime);

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::ClearLastResults()
{
  MOZ_ASSERT(!NS_IsMainThread(), "Must be on the background thread");
  if (mLastResults) {
    mLastResults->Clear();
  }
  return NS_OK;
}

bool
nsUrlClassifierDBServiceWorker::IsSameAsLastResults(CacheResultArray& aResult)
{
  if (!mLastResults || mLastResults->Length() != aResult.Length()) {
    return false;
  }

  bool equal = true;
  for (uint32_t i = 0; i < mLastResults->Length() && equal; i++) {
    CacheResult* lhs = mLastResults->ElementAt(i).get();
    CacheResult* rhs = aResult[i].get();

    if (lhs->Ver() != rhs->Ver()) {
      return false;
    }

    if (lhs->Ver() == CacheResult::V2) {
      equal = *(CacheResult::Cast<CacheResultV2>(lhs)) ==
              *(CacheResult::Cast<CacheResultV2>(rhs));
    } else if (lhs->Ver() == CacheResult::V4) {
      equal = *(CacheResult::Cast<CacheResultV4>(lhs)) ==
              *(CacheResult::Cast<CacheResultV4>(rhs));
    }
  }

  return equal;
}

// -------------------------------------------------------------------------
// nsUrlClassifierLookupCallback
//
// This class takes the results of a lookup found on the worker thread
// and handles any necessary partial hash expansions before calling
// the client callback.

class nsUrlClassifierLookupCallback final : public nsIUrlClassifierLookupCallback
                                          , public nsIUrlClassifierHashCompleterCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERLOOKUPCALLBACK
  NS_DECL_NSIURLCLASSIFIERHASHCOMPLETERCALLBACK

  nsUrlClassifierLookupCallback(nsUrlClassifierDBService *dbservice,
                                nsIUrlClassifierCallback *c)
    : mDBService(dbservice)
    , mResults(nullptr)
    , mPendingCompletions(0)
    , mCallback(c)
    {}

private:
  ~nsUrlClassifierLookupCallback();

  nsresult HandleResults();
  nsresult ProcessComplete(CacheResult* aCacheResult);

  RefPtr<nsUrlClassifierDBService> mDBService;
  nsAutoPtr<LookupResultArray> mResults;

  // Completed results to send back to the worker for caching.
  nsAutoPtr<CacheResultArray> mCacheResults;

  uint32_t mPendingCompletions;
  nsCOMPtr<nsIUrlClassifierCallback> mCallback;
};

NS_IMPL_ISUPPORTS(nsUrlClassifierLookupCallback,
                  nsIUrlClassifierLookupCallback,
                  nsIUrlClassifierHashCompleterCallback)

nsUrlClassifierLookupCallback::~nsUrlClassifierLookupCallback()
{
  if (mCallback) {
    NS_ReleaseOnMainThread(mCallback.forget());
  }
}

NS_IMETHODIMP
nsUrlClassifierLookupCallback::LookupComplete(nsTArray<LookupResult>* results)
{
  NS_ASSERTION(mResults == nullptr,
               "Should only get one set of results per nsUrlClassifierLookupCallback!");

  if (!results) {
    HandleResults();
    return NS_OK;
  }

  mResults = results;

  // Check the results entries that need to be completed.
  for (uint32_t i = 0; i < results->Length(); i++) {
    LookupResult& result = results->ElementAt(i);

    // We will complete partial matches and matches that are stale.
    if (!result.Confirmed()) {
      nsCOMPtr<nsIUrlClassifierHashCompleter> completer;
      nsCString gethashUrl;
      nsresult rv;
      nsCOMPtr<nsIUrlListManager> listManager = do_GetService(
        "@mozilla.org/url-classifier/listmanager;1", &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = listManager->GetGethashUrl(result.mTableName, gethashUrl);
      NS_ENSURE_SUCCESS(rv, rv);
      LOG(("The match from %s needs to be completed at %s",
           result.mTableName.get(), gethashUrl.get()));
      // gethashUrls may be empty in 2 cases: test tables, and on startup where
      // we may have found a prefix in an existing table before the listmanager
      // has registered the table. In the second case we should not call
      // complete.
      if ((!gethashUrl.IsEmpty() ||
           StringBeginsWith(result.mTableName, NS_LITERAL_CSTRING("test"))) &&
          mDBService->GetCompleter(result.mTableName,
                                   getter_AddRefs(completer))) {

        // Bug 1323953 - Send the first 4 bytes for completion no matter how
        // long we matched the prefix.
        nsAutoCString partialHash;
        partialHash.Assign(reinterpret_cast<char*>(&result.hash.fixedLengthPrefix),
                           PREFIX_SIZE);
        nsresult rv = completer->Complete(partialHash,
                                          gethashUrl,
                                          result.mTableName,
                                          this);
        if (NS_SUCCEEDED(rv)) {
          mPendingCompletions++;
        }
      } else {
        // For tables with no hash completer, a complete hash match is
        // good enough, we'll consider it is valid.
        if (result.Complete()) {
          result.mConfirmed = true;
          LOG(("Skipping completion in a table without a valid completer (%s).",
               result.mTableName.get()));
        } else {
          NS_WARNING("Partial match in a table without a valid completer, ignoring partial match.");
        }
      }
    }
  }

  LOG(("nsUrlClassifierLookupCallback::LookupComplete [%p] "
       "%u pending completions", this, mPendingCompletions));
  if (mPendingCompletions == 0) {
    // All results were complete, we're ready!
    HandleResults();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierLookupCallback::CompletionFinished(nsresult status)
{
  if (LOG_ENABLED()) {
    nsAutoCString errorName;
    mozilla::GetErrorName(status, errorName);
    LOG(("nsUrlClassifierLookupCallback::CompletionFinished [%p, %s]",
         this, errorName.get()));
  }

  mPendingCompletions--;
  if (mPendingCompletions == 0) {
    HandleResults();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierLookupCallback::CompletionV2(const nsACString& aCompleteHash,
                                            const nsACString& aTableName,
                                            uint32_t aChunkId)
{
  LOG(("nsUrlClassifierLookupCallback::Completion [%p, %s, %d]",
       this, PromiseFlatCString(aTableName).get(), aChunkId));

  MOZ_ASSERT(!StringEndsWith(aTableName, NS_LITERAL_CSTRING("-proto")));

  nsAutoPtr<CacheResultV2> result(new CacheResultV2);

  result->table = aTableName;
  result->prefix.Assign(aCompleteHash);
  result->completion.Assign(aCompleteHash);
  result->addChunk = aChunkId;

  return ProcessComplete(result.forget());
}

NS_IMETHODIMP
nsUrlClassifierLookupCallback::CompletionV4(const nsACString& aPartialHash,
                                            const nsACString& aTableName,
                                            uint32_t aNegativeCacheDuration,
                                            nsIArray* aFullHashes)
{
  LOG(("nsUrlClassifierLookupCallback::CompletionV4 [%p, %s, %d]",
       this, PromiseFlatCString(aTableName).get(), aNegativeCacheDuration));

  MOZ_ASSERT(StringEndsWith(aTableName, NS_LITERAL_CSTRING("-proto")));

  if(!aFullHashes) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aNegativeCacheDuration > MAXIMUM_NEGATIVE_CACHE_DURATION_SEC) {
    LOG(("Negative cache duration too large, clamping it down to"
         "a reasonable value."));
    aNegativeCacheDuration = MAXIMUM_NEGATIVE_CACHE_DURATION_SEC;
  }

  nsAutoPtr<CacheResultV4> result(new CacheResultV4);

  int64_t nowSec = PR_Now() / PR_USEC_PER_SEC;

  result->table = aTableName;
  result->prefix.Assign(aPartialHash);
  result->response.negativeCacheExpirySec = nowSec + aNegativeCacheDuration;

  // Fill in positive cache entries.
  uint32_t fullHashCount = 0;
  nsresult rv = aFullHashes->GetLength(&fullHashCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (uint32_t i = 0; i < fullHashCount; i++) {
    nsCOMPtr<nsIFullHashMatch> match = do_QueryElementAt(aFullHashes, i);

    nsCString fullHash;
    match->GetFullHash(fullHash);

    uint32_t duration;
    match->GetCacheDuration(&duration);

    result->response.fullHashes.Put(fullHash, nowSec + duration);
  }

  return ProcessComplete(result.forget());
}

nsresult
nsUrlClassifierLookupCallback::ProcessComplete(CacheResult* aCacheResult)
{
  // Send this completion to the store for caching.
  if (!mCacheResults) {
    mCacheResults = new CacheResultArray();
    if (!mCacheResults) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // OK if this fails, we just won't cache the item.
  mCacheResults->AppendElement(aCacheResult);

  // Check if this matched any of our results.
  for (uint32_t i = 0; i < mResults->Length(); i++) {
    LookupResult& result = mResults->ElementAt(i);

    // Now, see if it verifies a lookup
    if (!result.mNoise
        && result.mTableName.Equals(aCacheResult->table)
        && aCacheResult->findCompletion(result.CompleteHash())) {
      result.mProtocolConfirmed = true;
    }
  }

  return NS_OK;
}

nsresult
nsUrlClassifierLookupCallback::HandleResults()
{
  if (!mResults) {
    // No results, this URI is clean.
    LOG(("nsUrlClassifierLookupCallback::HandleResults [%p, no results]", this));
    return mCallback->HandleEvent(NS_LITERAL_CSTRING(""));
  }
  MOZ_ASSERT(mPendingCompletions == 0, "HandleResults() should never be "
             "called while there are pending completions");

  LOG(("nsUrlClassifierLookupCallback::HandleResults [%p, %" PRIuSIZE " results]",
       this, mResults->Length()));

  nsCOMPtr<nsIUrlClassifierClassifyCallback> classifyCallback =
    do_QueryInterface(mCallback);

  MatchResult matchResult = MatchResult::eTelemetryDisabled;

  nsTArray<nsCString> tables;
  // Build a stringified list of result tables.
  for (uint32_t i = 0; i < mResults->Length(); i++) {
    LookupResult& result = mResults->ElementAt(i);

    // Leave out results that weren't confirmed, as their existence on
    // the list can't be verified.  Also leave out randomly-generated
    // noise.
    if (result.mNoise) {
      LOG(("Skipping result %s from table %s (noise)",
           result.PartialHashHex().get(), result.mTableName.get()));
      continue;
    }

    bool confirmed = result.Confirmed();

    // If mMatchResult is set to eTelemetryDisabled, then we don't need to
    // set |matchResult| for this lookup.
    if (result.mMatchResult != MatchResult::eTelemetryDisabled) {
      matchResult &= ~(MatchResult::eTelemetryDisabled);
      if (result.mProtocolV2) {
        matchResult |=
          confirmed ? MatchResult::eV2PreAndCom : MatchResult::eV2Prefix;
      } else {
        matchResult |=
          confirmed ? MatchResult::eV4PreAndCom : MatchResult::eV4Prefix;
      }
    }

    if (!confirmed) {
      LOG(("Skipping result %s from table %s (not confirmed)",
           result.PartialHashHex().get(), result.mTableName.get()));
      continue;
    }

    if (StringEndsWith(result.mTableName, NS_LITERAL_CSTRING("-proto")) &&
        !Preferences::GetBool(TAKE_V4_COMPLETION_RESULT_PREF,
                              TAKE_V4_COMPLETION_RESULT_DEFAULT)) {
      // Bug 1331534 - We temporarily ignore hash completion result
      // for v4 tables.
      continue;
    }

    LOG(("Confirmed result %s from table %s",
         result.PartialHashHex().get(), result.mTableName.get()));

    if (tables.IndexOf(result.mTableName) == nsTArray<nsCString>::NoIndex) {
      tables.AppendElement(result.mTableName);
    }

    if (classifyCallback) {
      nsCString prefixString;
      result.hash.fixedLengthPrefix.ToString(prefixString);
      classifyCallback->HandleResult(result.mTableName, prefixString);
    }
  }

  // Only record threat type telemetry when completion is found in V2 & V4.
  if (matchResult == MatchResult::eAll && mCacheResults) {
    MatchThreatType types = MatchThreatType::eIdentical;

    // Check all the results because there may be multiple matches being returned.
    bool foundV2Result = false, foundV4Result = false;
    for (uint32_t i = 0; i < mCacheResults->Length(); i++) {
      CacheResult* c = mCacheResults->ElementAt(i).get();
      bool isV2 = CacheResult::V2 == c->Ver();
      if (isV2) {
        foundV2Result = true;
      } else {
        foundV4Result = true;
      }
      for (LookupResult& l : *(mResults.get())) {
        if (l.mProtocolV2 != isV2 || l.hash.fixedLengthPrefix != c->prefix) {
          continue;
        }

        // Ignore unconfirmed results.
        if (l.Confirmed()) {
          types |= TableNameToThreatType(CacheResult::V2 == c->Ver(), c->table);
        }
        break;
      }
    }

    // We don't want to record telemetry when one of the results is from cache
    // because finding an unexpired cache entry will prevent us from doing gethash
    // requests that would otherwise be required.
    if (foundV2Result && foundV4Result) {
      auto fnIsMatchSameThreatType = [&](const MatchThreatType& aTypeMask) {
        uint8_t val = static_cast<uint8_t>(types & aTypeMask);
        return val == 0 || val == static_cast<uint8_t>(aTypeMask);
      };
      if (fnIsMatchSameThreatType(MatchThreatType::ePhishingMask) &&
          fnIsMatchSameThreatType(MatchThreatType::eMalwareMask) &&
          fnIsMatchSameThreatType(MatchThreatType::eUnwantedMask)) {
        types = MatchThreatType::eIdentical;
      }

      Telemetry::Accumulate(Telemetry::URLCLASSIFIER_MATCH_THREAT_TYPE_RESULT,
                            static_cast<uint8_t>(types));
    }
  }

  if (matchResult != MatchResult::eTelemetryDisabled) {
    Telemetry::Accumulate(Telemetry::URLCLASSIFIER_MATCH_RESULT,
                          MatchResultToUint(matchResult));
  }

  // TODO: Bug 1333328, Refactor cache miss mechanism for v2.
  // Some parts of this gethash request generated no hits at all.
  // Prefixes must have been removed from the database since our last update.
  // Save the prefixes we checked to prevent repeated requests
  // until the next update.
  nsAutoPtr<PrefixArray> cacheMisses(new PrefixArray());
  if (cacheMisses) {
    for (uint32_t i = 0; i < mResults->Length(); i++) {
      LookupResult &result = mResults->ElementAt(i);
      if (result.mProtocolV2 && !result.Confirmed() && !result.mNoise) {
        cacheMisses->AppendElement(result.hash.fixedLengthPrefix);
      }
    }
    // Hands ownership of the miss array back to the worker thread.
    mDBService->CacheMisses(cacheMisses.forget());
  }

  if (mCacheResults) {
    // This hands ownership of the cache results array back to the worker
    // thread.
    mDBService->CacheCompletions(mCacheResults.forget());
  }

  nsAutoCString tableStr;
  for (uint32_t i = 0; i < tables.Length(); i++) {
    if (i != 0)
      tableStr.Append(',');
    tableStr.Append(tables[i]);
  }

  return mCallback->HandleEvent(tableStr);
}

struct Provider {
  nsCString name;
  uint8_t priority;
};

// Order matters
// Provider which is not included in this table has the lowest priority 0
static const Provider kBuiltInProviders[] = {
  { NS_LITERAL_CSTRING("mozilla"), 1 },
  { NS_LITERAL_CSTRING("google4"), 2 },
  { NS_LITERAL_CSTRING("google"), 3 },
};

// -------------------------------------------------------------------------
// Helper class for nsIURIClassifier implementation, handle classify result and
// send back to nsIURIClassifier

class nsUrlClassifierClassifyCallback final : public nsIUrlClassifierCallback,
                                              public nsIUrlClassifierClassifyCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCALLBACK
  NS_DECL_NSIURLCLASSIFIERCLASSIFYCALLBACK

  explicit nsUrlClassifierClassifyCallback(nsIURIClassifierCallback *c)
    : mCallback(c)
    {}

private:

  struct ClassifyMatchedInfo {
    nsCString table;
    nsCString prefix;
    Provider provider;
    nsresult errorCode;
  };

  ~nsUrlClassifierClassifyCallback() {};

  nsCOMPtr<nsIURIClassifierCallback> mCallback;
  nsTArray<ClassifyMatchedInfo> mMatchedArray;
};

NS_IMPL_ISUPPORTS(nsUrlClassifierClassifyCallback,
                  nsIUrlClassifierCallback,
                  nsIUrlClassifierClassifyCallback)

NS_IMETHODIMP
nsUrlClassifierClassifyCallback::HandleEvent(const nsACString& tables)
{
  nsresult response = TablesToResponse(tables);
  ClassifyMatchedInfo* matchedInfo = nullptr;

  if (NS_FAILED(response)) {
    // Filter all matched info which has correct response
    // In the case multiple tables found, use the higher priority provider
    nsTArray<ClassifyMatchedInfo> matches;
    for (uint32_t i = 0; i < mMatchedArray.Length(); i++) {
      if (mMatchedArray[i].errorCode == response &&
          (!matchedInfo ||
           matchedInfo->provider.priority < mMatchedArray[i].provider.priority)) {
        matchedInfo = &mMatchedArray[i];
      }
    }
  }

  nsCString provider = matchedInfo ? matchedInfo->provider.name : EmptyCString();
  nsCString prefix = matchedInfo ? matchedInfo->prefix : EmptyCString();
  nsCString table = matchedInfo ? matchedInfo->table : EmptyCString();

  mCallback->OnClassifyComplete(response, table, provider, prefix);
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierClassifyCallback::HandleResult(const nsACString& aTable,
                                              const nsACString& aPrefix)
{
  LOG(("nsUrlClassifierClassifyCallback::HandleResult [%p, table %s prefix %s]",
        this, PromiseFlatCString(aTable).get(), PromiseFlatCString(aPrefix).get()));

  if (NS_WARN_IF(aTable.IsEmpty()) || NS_WARN_IF(aPrefix.IsEmpty())) {
    return NS_ERROR_INVALID_ARG;
  }

  ClassifyMatchedInfo* matchedInfo = mMatchedArray.AppendElement();
  matchedInfo->table = aTable;
  matchedInfo->prefix = aPrefix;

  nsCOMPtr<nsIUrlClassifierUtils> urlUtil =
    do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID);

  nsCString provider;
  nsresult rv = urlUtil->GetProvider(aTable, provider);

  matchedInfo->provider.name = NS_SUCCEEDED(rv) ? provider : EmptyCString();
  matchedInfo->provider.priority = 0;
  for (uint8_t i = 0; i < ArrayLength(kBuiltInProviders); i++) {
    if (kBuiltInProviders[i].name.Equals(matchedInfo->provider.name)) {
      matchedInfo->provider.priority = kBuiltInProviders[i].priority;
    }
  }
  matchedInfo->errorCode = TablesToResponse(aTable);

  return NS_OK;
}

// -------------------------------------------------------------------------
// Proxy class implementation

NS_IMPL_ADDREF(nsUrlClassifierDBService)
NS_IMPL_RELEASE(nsUrlClassifierDBService)
NS_INTERFACE_MAP_BEGIN(nsUrlClassifierDBService)
  // Only nsIURIClassifier is supported in the content process!
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIUrlClassifierDBService, XRE_IsParentProcess())
  NS_INTERFACE_MAP_ENTRY(nsIURIClassifier)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIObserver, XRE_IsParentProcess())
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIClassifier)
NS_INTERFACE_MAP_END

/* static */ nsUrlClassifierDBService*
nsUrlClassifierDBService::GetInstance(nsresult *result)
{
  *result = NS_OK;
  if (!sUrlClassifierDBService) {
    sUrlClassifierDBService = new nsUrlClassifierDBService();
    if (!sUrlClassifierDBService) {
      *result = NS_ERROR_OUT_OF_MEMORY;
      return nullptr;
    }

    NS_ADDREF(sUrlClassifierDBService);   // addref the global

    *result = sUrlClassifierDBService->Init();
    if (NS_FAILED(*result)) {
      NS_RELEASE(sUrlClassifierDBService);
      return nullptr;
    }
  } else {
    // Already exists, just add a ref
    NS_ADDREF(sUrlClassifierDBService);   // addref the return result
  }
  return sUrlClassifierDBService;
}


nsUrlClassifierDBService::nsUrlClassifierDBService()
 : mCheckMalware(CHECK_MALWARE_DEFAULT)
 , mCheckPhishing(CHECK_PHISHING_DEFAULT)
 , mCheckBlockedURIs(CHECK_BLOCKED_DEFAULT)
 , mInUpdate(false)
{
}

nsUrlClassifierDBService::~nsUrlClassifierDBService()
{
  sUrlClassifierDBService = nullptr;
}

void
AppendTables(const nsCString& aTables, nsCString &outTables)
{
  if (!aTables.IsEmpty()) {
    if (!outTables.IsEmpty()) {
      outTables.Append(',');
    }
    outTables.Append(aTables);
  }
}

nsresult
nsUrlClassifierDBService::ReadTablesFromPrefs()
{
  mCheckMalware = Preferences::GetBool(CHECK_MALWARE_PREF,
    CHECK_MALWARE_DEFAULT);
  mCheckPhishing = Preferences::GetBool(CHECK_PHISHING_PREF,
    CHECK_PHISHING_DEFAULT);
  mCheckBlockedURIs = Preferences::GetBool(CHECK_BLOCKED_PREF,
    CHECK_BLOCKED_DEFAULT);

  nsCString allTables;
  nsCString tables;

  mBaseTables.Truncate();
  mTrackingProtectionTables.Truncate();

  Preferences::GetCString(PHISH_TABLE_PREF, &allTables);
  if (mCheckPhishing) {
    AppendTables(allTables, mBaseTables);
  }

  Preferences::GetCString(MALWARE_TABLE_PREF, &tables);
  AppendTables(tables, allTables);
  if (mCheckMalware) {
    AppendTables(tables, mBaseTables);
  }

  Preferences::GetCString(BLOCKED_TABLE_PREF, &tables);
  AppendTables(tables, allTables);
  if (mCheckBlockedURIs) {
    AppendTables(tables, mBaseTables);
  }

  Preferences::GetCString(DOWNLOAD_BLOCK_TABLE_PREF, &tables);
  AppendTables(tables, allTables);

  Preferences::GetCString(DOWNLOAD_ALLOW_TABLE_PREF, &tables);
  AppendTables(tables, allTables);

  Preferences::GetCString(TRACKING_TABLE_PREF, &tables);
  AppendTables(tables, allTables);
  AppendTables(tables, mTrackingProtectionTables);

  Preferences::GetCString(TRACKING_WHITELIST_TABLE_PREF, &tables);
  AppendTables(tables, allTables);
  AppendTables(tables, mTrackingProtectionTables);

  Classifier::SplitTables(allTables, mGethashTables);

  Preferences::GetCString(DISALLOW_COMPLETION_TABLE_PREF, &tables);
  Classifier::SplitTables(tables, mDisallowCompletionsTables);

  return NS_OK;
}

nsresult
nsUrlClassifierDBService::Init()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must initialize DB service on main thread");
  nsCOMPtr<nsIXULRuntime> appInfo = do_GetService("@mozilla.org/xre/app-info;1");
  if (appInfo) {
    bool inSafeMode = false;
    appInfo->GetInSafeMode(&inSafeMode);
    if (inSafeMode) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  switch (XRE_GetProcessType()) {
  case GeckoProcessType_Default:
    // The parent process is supported.
    break;
  case GeckoProcessType_Content:
    // In a content process, we simply forward all requests to the parent process,
    // so we can skip the initialization steps here.
    // Note that since we never register an observer, Shutdown() will also never
    // be called in the content process.
    return NS_OK;
  default:
    // No other process type is supported!
    return NS_ERROR_NOT_AVAILABLE;
  }

  sGethashNoise = Preferences::GetUint(GETHASH_NOISE_PREF,
    GETHASH_NOISE_DEFAULT);
  gFreshnessGuarantee = Preferences::GetInt(CONFIRM_AGE_PREF,
    CONFIRM_AGE_DEFAULT_SEC);
  ReadTablesFromPrefs();
  nsresult rv;

  {
    // Force PSM loading on main thread
    nsCOMPtr<nsICryptoHash> dummy = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  {
    // Force nsIUrlClassifierUtils loading on main thread.
    nsCOMPtr<nsIUrlClassifierUtils> dummy =
      do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Directory providers must also be accessed on the main thread.
  nsCOMPtr<nsIFile> cacheDir;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                              getter_AddRefs(cacheDir));
  if (NS_FAILED(rv)) {
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(cacheDir));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Start the background thread.
  rv = NS_NewNamedThread("URL Classifier", &gDbBackgroundThread);
  if (NS_FAILED(rv))
    return rv;

  mWorker = new nsUrlClassifierDBServiceWorker();
  if (!mWorker)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = mWorker->Init(sGethashNoise, cacheDir);
  if (NS_FAILED(rv)) {
    mWorker = nullptr;
    return rv;
  }

  // Proxy for calling the worker on the background thread
  mWorkerProxy = new UrlClassifierDBServiceWorkerProxy(mWorker);
  rv = mWorkerProxy->OpenDb();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Add an observer for shutdown
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (!observerService)
    return NS_ERROR_FAILURE;

  // The application is about to quit
  observerService->AddObserver(this, "quit-application", false);
  observerService->AddObserver(this, "profile-before-change", false);

  // XXX: Do we *really* need to be able to change all of these at runtime?
  // Note: These observers should only be added when everything else above has
  //       succeeded. Failing to do so can cause long shutdown times in certain
  //       situations. See Bug 1247798 and Bug 1244803.
  Preferences::AddUintVarCache(&sGethashNoise, GETHASH_NOISE_PREF,
    GETHASH_NOISE_DEFAULT);
  Preferences::AddAtomicUintVarCache(&gFreshnessGuarantee, CONFIRM_AGE_PREF,
    CONFIRM_AGE_DEFAULT_SEC);

  for (uint8_t i = 0; i < kObservedPrefs.Length(); i++) {
    Preferences::AddStrongObserver(this, kObservedPrefs[i].get());
  }

  return NS_OK;
}

// nsChannelClassifier is the only consumer of this interface.
NS_IMETHODIMP
nsUrlClassifierDBService::Classify(nsIPrincipal* aPrincipal,
                                   nsIEventTarget* aEventTarget,
                                   bool aTrackingProtectionEnabled,
                                   nsIURIClassifierCallback* c,
                                   bool* result)
{
  NS_ENSURE_ARG(aPrincipal);

  if (XRE_IsContentProcess()) {
    using namespace mozilla::dom;

    ContentChild* content = ContentChild::GetSingleton();
    MOZ_ASSERT(content);

    auto actor = static_cast<URLClassifierChild*>
      (content->AllocPURLClassifierChild(IPC::Principal(aPrincipal),
                                         aTrackingProtectionEnabled,
                                         result));
    MOZ_ASSERT(actor);

    if (aEventTarget) {
      content->SetEventTargetForActor(actor, aEventTarget);
    } else {
      // In the case null event target we should use systemgroup event target
      NS_WARNING(("Null event target, we should use SystemGroup to do labelling"));
      nsCOMPtr<nsIEventTarget> systemGroupEventTarget
        = mozilla::SystemGroup::EventTargetFor(mozilla::TaskCategory::Other);
      content->SetEventTargetForActor(actor, systemGroupEventTarget);
    }
    if (!content->SendPURLClassifierConstructor(actor, IPC::Principal(aPrincipal),
                  aTrackingProtectionEnabled,
                  result)) {
      *result = false;
      return NS_ERROR_FAILURE;
    }

    actor->SetCallback(c);
    return NS_OK;
  }

  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  if (!(mCheckMalware || mCheckPhishing || aTrackingProtectionEnabled ||
        mCheckBlockedURIs)) {
    *result = false;
    return NS_OK;
  }

  RefPtr<nsUrlClassifierClassifyCallback> callback =
    new nsUrlClassifierClassifyCallback(c);

  if (!callback) return NS_ERROR_OUT_OF_MEMORY;

  nsCString tables = mBaseTables;
  if (aTrackingProtectionEnabled) {
    AppendTables(mTrackingProtectionTables, tables);
  }

  nsresult rv = LookupURI(aPrincipal, tables, callback, false, result);
  if (rv == NS_ERROR_MALFORMED_URI) {
    *result = false;
    // The URI had no hostname, don't try to classify it.
    return NS_OK;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBService::ClassifyLocal(nsIURI *aURI,
                                        const nsACString& aTables,
                                        nsACString& aTableResults)
{
  nsTArray<nsCString> results;
  ClassifyLocalWithTables(aURI, aTables, results);

  // Convert the result array to a comma separated string
  aTableResults.AssignLiteral("");
  bool first = true;
  for (nsCString& result : results) {
    if (first) {
      first = false;
    } else {
      aTableResults.AppendLiteral(",");
    }
    aTableResults.Append(result);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBService::AsyncClassifyLocalWithTables(nsIURI *aURI,
                                                       const nsACString& aTables,
                                                       nsIURIClassifierCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread(), "AsyncClassifyLocalWithTables must be called "
                                "on main thread");

  if (XRE_IsContentProcess()) {
    using namespace mozilla::dom;
    using namespace mozilla::ipc;

    ContentChild* content = ContentChild::GetSingleton();
    MOZ_ASSERT(content);

    auto actor = new URLClassifierLocalChild();

    // TODO: Bug 1353701 - Supports custom event target for labelling.
    nsCOMPtr<nsIEventTarget> systemGroupEventTarget
      = mozilla::SystemGroup::EventTargetFor(mozilla::TaskCategory::Other);
    content->SetEventTargetForActor(actor, systemGroupEventTarget);

    URIParams uri;
    SerializeURI(aURI, uri);
    nsAutoCString tables(aTables);
    if (!content->SendPURLClassifierLocalConstructor(actor, uri, tables)) {
      return NS_ERROR_FAILURE;
    }

    actor->SetCallback(aCallback);
    return NS_OK;
  }

  if (gShuttingDownThread) {
    return NS_ERROR_ABORT;
  }

  using namespace mozilla::Telemetry;
  auto startTime = TimeStamp::Now(); // For telemetry.

  nsCOMPtr<nsIURI> uri = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

  nsAutoCString key;
  // Canonicalize the url
  nsCOMPtr<nsIUrlClassifierUtils> utilsService =
    do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID);
  nsresult rv = utilsService->GetKeyForURI(uri, key);
  NS_ENSURE_SUCCESS(rv, rv);

  auto worker = mWorker;
  nsCString tables(aTables);

  // Since aCallback will be passed around threads...
  nsMainThreadPtrHandle<nsIURIClassifierCallback> callback(
    new nsMainThreadPtrHolder<nsIURIClassifierCallback>(aCallback));

  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction([worker, key, tables, callback, startTime] () -> void {

    nsCString matchedLists;
    nsAutoPtr<LookupResultArray> results(new LookupResultArray());
    if (results) {
      nsresult rv = worker->DoLocalLookup(key, tables, results);
      if (NS_SUCCEEDED(rv)) {
        for (uint32_t i = 0; i < results->Length(); i++) {
          if (i > 0) {
            matchedLists.AppendLiteral(",");
          }
          matchedLists.Append(results->ElementAt(i).mTableName);
        }
      }
    }

    nsCOMPtr<nsIRunnable> cbRunnable =
      NS_NewRunnableFunction([callback, matchedLists, startTime] () -> void {
        // Measure the time diff between calling and callback.
        AccumulateDelta_impl<Millisecond>::compute(
          Telemetry::URLCLASSIFIER_ASYNC_CLASSIFYLOCAL_TIME, startTime);

        // |callback| is captured as const value so ...
        auto cb = const_cast<nsIURIClassifierCallback*>(callback.get());
        cb->OnClassifyComplete(NS_OK,           // Not used.
                               matchedLists,
                               EmptyCString(),  // provider. (Not used)
                               EmptyCString()); // prefix. (Not used)
      });

    NS_DispatchToMainThread(cbRunnable);
  });

  return gDbBackgroundThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
nsUrlClassifierDBService::ClassifyLocalWithTables(nsIURI *aURI,
                                                  const nsACString& aTables,
                                                  nsTArray<nsCString>& aTableResults)
{
  MOZ_ASSERT(NS_IsMainThread(), "ClassifyLocalWithTables must be on main thread");
  if (gShuttingDownThread) {
    return NS_ERROR_ABORT;
  }

  nsresult rv;
  if (XRE_IsContentProcess()) {
    using namespace mozilla::dom;
    using namespace mozilla::ipc;
    URIParams uri;
    SerializeURI(aURI, uri);
    nsAutoCString tables(aTables);
    bool result = ContentChild::GetSingleton()->SendClassifyLocal(uri, tables,
                                                                  &rv,
                                                                  &aTableResults);
    if (result) {
      return rv;
    }
    return NS_ERROR_FAILURE;
  }

  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);
  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_CLASSIFYLOCAL_TIME> timer;

  nsCOMPtr<nsIURI> uri = NS_GetInnermostURI(aURI);
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

  nsAutoCString key;
  // Canonicalize the url
  nsCOMPtr<nsIUrlClassifierUtils> utilsService =
    do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID);
  rv = utilsService->GetKeyForURI(uri, key);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoPtr<LookupResultArray> results(new LookupResultArray());
  if (!results) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // In unittests, we may not have been initalized, so don't crash.
  rv = mWorkerProxy->DoLocalLookup(key, aTables, results);
  if (NS_SUCCEEDED(rv)) {
    rv = ProcessLookupResults(results, aTableResults);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBService::Lookup(nsIPrincipal* aPrincipal,
                                 const nsACString& tables,
                                 nsIUrlClassifierCallback* c)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  bool dummy;
  return LookupURI(aPrincipal, tables, c, true, &dummy);
}

nsresult
nsUrlClassifierDBService::LookupURI(nsIPrincipal* aPrincipal,
                                    const nsACString& tables,
                                    nsIUrlClassifierCallback* c,
                                    bool forceLookup,
                                    bool *didLookup)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG(aPrincipal);

  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    *didLookup = false;
    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

  uri = NS_GetInnermostURI(uri);
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

  nsAutoCString key;
  // Canonicalize the url
  nsCOMPtr<nsIUrlClassifierUtils> utilsService =
    do_GetService(NS_URLCLASSIFIERUTILS_CONTRACTID);
  rv = utilsService->GetKeyForURI(uri, key);
  if (NS_FAILED(rv))
    return rv;

  if (forceLookup) {
    *didLookup = true;
  } else {
    bool clean = false;

    if (!clean) {
      nsCOMPtr<nsIPermissionManager> permissionManager =
        services::GetPermissionManager();

      if (permissionManager) {
        uint32_t perm;
        rv = permissionManager->TestPermissionFromPrincipal(aPrincipal,
                                                           "safe-browsing", &perm);
        NS_ENSURE_SUCCESS(rv, rv);

        clean |= (perm == nsIPermissionManager::ALLOW_ACTION);
      }
    }

    *didLookup = !clean;
    if (clean) {
      return NS_OK;
    }
  }

  // Create an nsUrlClassifierLookupCallback object.  This object will
  // take care of confirming partial hash matches if necessary before
  // calling the client's callback.
  nsCOMPtr<nsIUrlClassifierLookupCallback> callback =
    new nsUrlClassifierLookupCallback(this, c);
  if (!callback)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIUrlClassifierLookupCallback> proxyCallback =
    new UrlClassifierLookupCallbackProxy(callback);

  // Queue this lookup and call the lookup function to flush the queue if
  // necessary.
  rv = mWorker->QueueLookup(key, tables, proxyCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  // This seems to just call HandlePendingLookups.
  nsAutoCString dummy;
  return mWorkerProxy->Lookup(nullptr, dummy, nullptr);
}

NS_IMETHODIMP
nsUrlClassifierDBService::GetTables(nsIUrlClassifierCallback* c)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  // The proxy callback uses the current thread.
  nsCOMPtr<nsIUrlClassifierCallback> proxyCallback =
    new UrlClassifierCallbackProxy(c);

  return mWorkerProxy->GetTables(proxyCallback);
}

NS_IMETHODIMP
nsUrlClassifierDBService::SetHashCompleter(const nsACString &tableName,
                                           nsIUrlClassifierHashCompleter *completer)
{
  if (completer) {
    mCompleters.Put(tableName, completer);
  } else {
    mCompleters.Remove(tableName);
  }
  ClearLastResults();
  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBService::SetLastUpdateTime(const nsACString &tableName,
                                            uint64_t lastUpdateTime)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  return mWorkerProxy->SetLastUpdateTime(tableName, lastUpdateTime);
}

NS_IMETHODIMP
nsUrlClassifierDBService::ClearLastResults()
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  return mWorkerProxy->ClearLastResults();
}

NS_IMETHODIMP
nsUrlClassifierDBService::BeginUpdate(nsIUrlClassifierUpdateObserver *observer,
                                      const nsACString &updateTables)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  if (mInUpdate) {
    LOG(("Already updating, not available"));
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (mWorker->IsBusyUpdating()) {
    // |mInUpdate| used to work well because "notifying update observer"
    // is synchronously done in Worker::FinishUpdate(). Even if the
    // update observer hasn't been notified at this point, we can still
    // dispatch BeginUpdate() since it will NOT be run until the
    // previous Worker::FinishUpdate() returns.
    //
    // However, some tasks in Worker::FinishUpdate() have been moved to
    // another thread. The update observer will NOT be notified when
    // Worker::FinishUpdate() returns. If we only check |mInUpdate|,
    // the following sequence might happen on worker thread:
    //
    // Worker::FinishUpdate() // for update 1
    // Worker::BeginUpdate()  // for update 2
    // Worker::NotifyUpdateObserver() // for update 1
    //
    // So, we have to find out a way to reject BeginUpdate() right here
    // if the previous update observer hasn't been notified.
    //
    // Directly probing the worker's state is the most lightweight solution.
    // No lock is required since Worker::BeginUpdate() and
    // Worker::NotifyUpdateObserver() are by nature mutual exclusive.
    // (both run on worker thread.)
    LOG(("The previous update observer hasn't been notified."));
    return NS_ERROR_NOT_AVAILABLE;
  }

  mInUpdate = true;

  // The proxy observer uses the current thread
  nsCOMPtr<nsIUrlClassifierUpdateObserver> proxyObserver =
    new UrlClassifierUpdateObserverProxy(observer);

  return mWorkerProxy->BeginUpdate(proxyObserver, updateTables);
}

NS_IMETHODIMP
nsUrlClassifierDBService::BeginStream(const nsACString &table)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  return mWorkerProxy->BeginStream(table);
}

NS_IMETHODIMP
nsUrlClassifierDBService::UpdateStream(const nsACString& aUpdateChunk)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  return mWorkerProxy->UpdateStream(aUpdateChunk);
}

NS_IMETHODIMP
nsUrlClassifierDBService::FinishStream()
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  return mWorkerProxy->FinishStream();
}

NS_IMETHODIMP
nsUrlClassifierDBService::FinishUpdate()
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  mInUpdate = false;

  return mWorkerProxy->FinishUpdate();
}


NS_IMETHODIMP
nsUrlClassifierDBService::CancelUpdate()
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  mInUpdate = false;

  return mWorkerProxy->CancelUpdate();
}

NS_IMETHODIMP
nsUrlClassifierDBService::ResetDatabase()
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  if (mWorker->IsBusyUpdating()) {
    LOG(("Failed to ResetDatabase because of the unfinished update."));
    return NS_ERROR_FAILURE;
  }

  return mWorkerProxy->ResetDatabase();
}

NS_IMETHODIMP
nsUrlClassifierDBService::ReloadDatabase()
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  if (mWorker->IsBusyUpdating()) {
    LOG(("Failed to ReloadDatabase because of the unfinished update."));
    return NS_ERROR_FAILURE;
  }

  return mWorkerProxy->ReloadDatabase();
}

nsresult
nsUrlClassifierDBService::CacheCompletions(CacheResultArray *results)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  return mWorkerProxy->CacheCompletions(results);
}

nsresult
nsUrlClassifierDBService::CacheMisses(PrefixArray *results)
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

  return mWorkerProxy->CacheMisses(results);
}

bool
nsUrlClassifierDBService::GetCompleter(const nsACString &tableName,
                                       nsIUrlClassifierHashCompleter **completer)
{
  // If we have specified a completer, go ahead and query it. This is only
  // used by tests.
  if (mCompleters.Get(tableName, completer)) {
    return true;
  }

  // If we don't know about this table at all, or are disallowing completions
  // for it, skip completion checks.
  if (!mGethashTables.Contains(tableName) ||
      mDisallowCompletionsTables.Contains(tableName)) {
    return false;
  }

  // Otherwise, call gethash to find the hash completions.
  return NS_SUCCEEDED(CallGetService(NS_URLCLASSIFIERHASHCOMPLETER_CONTRACTID,
                                     completer));
}

NS_IMETHODIMP
nsUrlClassifierDBService::Observe(nsISupports *aSubject, const char *aTopic,
                                  const char16_t *aData)
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> prefs(do_QueryInterface(aSubject, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    Unused << prefs;

    if (kObservedPrefs.Contains(NS_ConvertUTF16toUTF8(aData))) {
      ReadTablesFromPrefs();
    }
  } else if (!strcmp(aTopic, "quit-application")) {
    // Tell the update thread to finish as soon as possible.
    gShuttingDownThread = true;
  } else if (!strcmp(aTopic, "profile-before-change")) {
    gShuttingDownThread = true;
    Shutdown();
  } else {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

// Join the background thread if it exists.
nsresult
nsUrlClassifierDBService::Shutdown()
{
  LOG(("shutting down db service\n"));
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!gDbBackgroundThread) {
    return NS_OK;
  }

  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_SHUTDOWN_TIME> timer;

  mCompleters.Clear();

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    for (uint8_t i = 0; i < kObservedPrefs.Length(); i++) {
      prefs->RemoveObserver(kObservedPrefs[i].get(), this);
    }
  }

  // 1. Synchronize with worker thread and update thread by
  //    *synchronously* dispatching an event to worker thread
  //    for shutting down the update thread. The reason not
  //    shutting down update thread directly from main thread
  //    is to avoid racing for Classifier::mUpdateThread
  //    between main thread and the worker thread. (Both threads
  //    would access Classifier::mUpdateThread.)
  using Worker = nsUrlClassifierDBServiceWorker;
  RefPtr<nsIRunnable> r =
    NewRunnableMethod(mWorker, &Worker::FlushAndDisableAsyncUpdate);
  SyncRunnable::DispatchToThread(gDbBackgroundThread, r);

  // At this point the update thread has been shut down and
  // the worker thread should only have at most one event,
  // which is the callback event.

  // 2. Send CancelUpdate() event to notify the dangling update.
  //    (i.e. BeginUpdate is called but FinishUpdate is not.)
  //    and CloseDb() to clear mClassifier. They will be the last two
  //    events on the worker thread in the shutdown process.
  DebugOnly<nsresult> rv;
  rv = mWorkerProxy->CancelUpdate();
  MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to post 'cancel update' event");
  rv = mWorkerProxy->CloseDb();
  MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to post 'close db' event");
  mWorkerProxy = nullptr;

  // 3. Invalidate XPCOM APIs by nulling out gDbBackgroundThread
  //    since every API checks gDbBackgroundThread first. This has
  //    to be done before calling nsIThread.shutdown because it
  //    will cause the pending events on the joining thread to
  //    be processed.
  nsIThread *backgroundThread = nullptr;
  Swap(backgroundThread, gDbBackgroundThread);

  // 4. Wait until the worker thread is down.
  if (backgroundThread) {
    backgroundThread->Shutdown();
    NS_RELEASE(backgroundThread);
  }
  return NS_OK;
}

nsIThread*
nsUrlClassifierDBService::BackgroundThread()
{
  return gDbBackgroundThread;
}

// static
bool
nsUrlClassifierDBService::ShutdownHasStarted()
{
  return gShuttingDownThread;
}
