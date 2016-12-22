//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsAppDirectoryServiceDefs.h"
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
#include "nsIUrlClassifierUtils.h"
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
#include "nsXPCOMStrings.h"
#include "nsProxyRelease.h"
#include "nsString.h"
#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Mutex.h"
#include "mozilla/Preferences.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Logging.h"
#include "prprf.h"
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

namespace mozilla {
namespace safebrowsing {

nsresult
TablesToResponse(const nsACString& tables)
{
  if (tables.IsEmpty()) {
    return NS_OK;
  }

  // We don't check mCheckMalware and friends because BuildTables never
  // includes a table that is not enabled.
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

// Prefs for implementing nsIURIClassifier to block page loads
#define CHECK_MALWARE_PREF      "browser.safebrowsing.malware.enabled"
#define CHECK_MALWARE_DEFAULT   false

#define CHECK_PHISHING_PREF     "browser.safebrowsing.phishing.enabled"
#define CHECK_PHISHING_DEFAULT  false

#define CHECK_BLOCKED_PREF    "browser.safebrowsing.blockedURIs.enabled"
#define CHECK_BLOCKED_DEFAULT false

#define GETHASH_NOISE_PREF      "urlclassifier.gethashnoise"
#define GETHASH_NOISE_DEFAULT   4

// Comma-separated lists
#define MALWARE_TABLE_PREF              "urlclassifier.malwareTable"
#define PHISH_TABLE_PREF                "urlclassifier.phishTable"
#define TRACKING_TABLE_PREF             "urlclassifier.trackingTable"
#define TRACKING_WHITELIST_TABLE_PREF   "urlclassifier.trackingWhitelistTable"
#define BLOCKED_TABLE_PREF              "urlclassifier.blockedTable"
#define DOWNLOAD_BLOCK_TABLE_PREF       "urlclassifier.downloadBlockTable"
#define DOWNLOAD_ALLOW_TABLE_PREF       "urlclassifier.downloadAllowTable"
#define DISALLOW_COMPLETION_TABLE_PREF  "urlclassifier.disallow_completions"

#define CONFIRM_AGE_PREF        "urlclassifier.max-complete-age"
#define CONFIRM_AGE_DEFAULT_SEC (45 * 60)

class nsUrlClassifierDBServiceWorker;

// Singleton instance.
static nsUrlClassifierDBService* sUrlClassifierDBService;

nsIThread* nsUrlClassifierDBService::gDbBackgroundThread = nullptr;

// Once we've committed to shutting down, don't do work in the background
// thread.
static bool gShuttingDownThread = false;

static mozilla::Atomic<int32_t> gFreshnessGuarantee(CONFIRM_AGE_DEFAULT_SEC);

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

  LOG(("Found %d results.", results->Length()));
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

  LOG(("Found %d results.", results->Length()));


  if (LOG_ENABLED()) {
    PRIntervalTime clockEnd = PR_IntervalNow();
    LOG(("query took %dms\n",
         PR_IntervalToMilliseconds(clockEnd - clockStart)));
  }

  nsAutoPtr<LookupResultArray> completes(new LookupResultArray());

  for (uint32_t i = 0; i < results->Length(); i++) {
    if (!mMissCache.Contains(results->ElementAt(i).hash.fixedLengthPrefix)) {
      completes->AppendElement(results->ElementAt(i));
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
    Telemetry::Accumulate(Telemetry::URLCLASSIFIER_LOOKUP_TIME,
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
  }

  mProtocolParser = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsUrlClassifierDBServiceWorker::FinishUpdate()
{
  if (gShuttingDownThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_ENSURE_STATE(mUpdateObserver);

  if (NS_SUCCEEDED(mUpdateStatus)) {
    mUpdateStatus = ApplyUpdate();
  } else {
    LOG(("nsUrlClassifierDBServiceWorker::FinishUpdate() Not running "
         "ApplyUpdate() since the update has already failed."));
  }

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

  Telemetry::Accumulate(Telemetry::URLCLASSIFIER_UPDATE_ERROR, provider,
                        NS_ERROR_GET_CODE(updateStatus));

  mMissCache.Clear();

  if (NS_SUCCEEDED(mUpdateStatus)) {
    LOG(("Notifying success: %d", mUpdateWaitSec));
    mUpdateObserver->UpdateSuccess(mUpdateWaitSec);
  } else if (NS_ERROR_NOT_IMPLEMENTED == mUpdateStatus) {
    LOG(("Treating NS_ERROR_NOT_IMPLEMENTED a successful update "
         "but still mark it spoiled."));
    mUpdateObserver->UpdateSuccess(0);
    mClassifier->ResetTables(Classifier::Clear_Cache, mUpdateTables);
  } else {
    if (LOG_ENABLED()) {
      nsAutoCString errorName;
      mozilla::GetErrorName(mUpdateStatus, errorName);
      LOG(("Notifying error: %s (%d)", errorName.get(), mUpdateStatus));
    }

    mUpdateObserver->UpdateError(mUpdateStatus);
    /*
     * mark the tables as spoiled(clear cache in LookupCache), we don't want to
     * block hosts longer than normal because our update failed
    */
    mClassifier->ResetTables(Classifier::Clear_Cache, mUpdateTables);
  }
  mUpdateObserver = nullptr;

  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::ApplyUpdate()
{
  LOG(("nsUrlClassifierDBServiceWorker::ApplyUpdate()"));
  nsresult rv = mClassifier->ApplyUpdates(&mTableUpdates);

#ifdef MOZ_SAFEBROWSING_DUMP_FAILED_UPDATES
  if (NS_FAILED(rv) && NS_ERROR_OUT_OF_MEMORY != rv) {
    mClassifier->DumpRawTableUpdates(mRawTableUpdates);
  }
  // Invalidate the raw table updates.
  mRawTableUpdates = EmptyCString();
#endif

  return rv;
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
  if (!mClassifier)
    return NS_OK;

  // Ownership is transferred in to us
  nsAutoPtr<CacheResultArray> resultsPtr(results);

  if (mLastResults == *resultsPtr) {
    LOG(("Skipping completions that have just been cached already."));
    return NS_OK;
  }

  nsAutoPtr<ProtocolParserV2> pParse(new ProtocolParserV2());
  nsTArray<TableUpdate*> updates;

  // Only cache results for tables that we have, don't take
  // in tables we might accidentally have hit during a completion.
  // This happens due to goog vs googpub lists existing.
  nsTArray<nsCString> tables;
  nsresult rv = mClassifier->ActiveTables(tables);
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < resultsPtr->Length(); i++) {
    bool activeTable = false;
    for (uint32_t table = 0; table < tables.Length(); table++) {
      if (tables[table].Equals(resultsPtr->ElementAt(i).table)) {
        activeTable = true;
        break;
      }
    }
    if (activeTable) {
      TableUpdateV2* tuV2 = TableUpdate::Cast<TableUpdateV2>(
        pParse->GetTableUpdate(resultsPtr->ElementAt(i).table));

      NS_ENSURE_TRUE(tuV2, NS_ERROR_FAILURE);

      LOG(("CacheCompletion Addchunk %d hash %X", resultsPtr->ElementAt(i).entry.addChunk,
           resultsPtr->ElementAt(i).entry.ToUint32()));
      rv = tuV2->NewAddComplete(resultsPtr->ElementAt(i).entry.addChunk,
                                resultsPtr->ElementAt(i).entry.complete);
      if (NS_FAILED(rv)) {
        // We can bail without leaking here because ForgetTableUpdates
        // hasn't been called yet.
        return rv;
      }
      rv = tuV2->NewAddChunk(resultsPtr->ElementAt(i).entry.addChunk);
      if (NS_FAILED(rv)) {
        return rv;
      }
      updates.AppendElement(tuV2);
      pParse->ForgetTableUpdates();
    } else {
      LOG(("Completion received, but table is not active, so not caching."));
    }
   }

  mClassifier->ApplyFullHashes(&updates);
  mLastResults = *resultsPtr;
  return NS_OK;
}

nsresult
nsUrlClassifierDBServiceWorker::CacheMisses(PrefixArray *results)
{
  LOG(("nsUrlClassifierDBServiceWorker::CacheMisses [%p] %d",
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
  mLastResults.Clear();
  return NS_OK;
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
           StringBeginsWith(result.mTableName, NS_LITERAL_CSTRING("test-"))) &&
          mDBService->GetCompleter(result.mTableName,
                                   getter_AddRefs(completer))) {

        // TODO: Figure out how long the partial hash should be sent
        //       for completion. See Bug 1323953.
        nsAutoCString partialHash;
        if (StringEndsWith(result.mTableName, NS_LITERAL_CSTRING("-proto"))) {
          // We send the complete partial hash for v4 at the moment.
          partialHash = result.PartialHash();
        } else {
          // We always send the first 4 bytes of the partial hash for
          // non-v4 tables. This matters when we have 32-byte prefix
          // in "test-xxx-simple" test data.
          partialHash.Assign(reinterpret_cast<char*>(&result.hash.fixedLengthPrefix),
                             PREFIX_SIZE);
        }

        nsresult rv = completer->Complete(partialHash,
                                          gethashUrl,
                                          result.mTableName,
                                          this);
        if (NS_SUCCEEDED(rv)) {
          mPendingCompletions++;
        }
      } else {
        // For tables with no hash completer, a complete hash match is
        // good enough, we'll consider it fresh, even if it hasn't been updated
        // in 45 minutes.
        if (result.Complete()) {
          result.mFresh = true;
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
nsUrlClassifierLookupCallback::Completion(const nsACString& completeHash,
                                          const nsACString& tableName,
                                          uint32_t chunkId)
{
  LOG(("nsUrlClassifierLookupCallback::Completion [%p, %s, %d]",
       this, PromiseFlatCString(tableName).get(), chunkId));
  mozilla::safebrowsing::Completion hash;
  hash.Assign(completeHash);

  // Send this completion to the store for caching.
  if (!mCacheResults) {
    mCacheResults = new CacheResultArray();
    if (!mCacheResults)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  CacheResult result;
  result.entry.addChunk = chunkId;
  result.entry.complete = hash;
  result.table = tableName;

  // OK if this fails, we just won't cache the item.
  mCacheResults->AppendElement(result);

  // Check if this matched any of our results.
  for (uint32_t i = 0; i < mResults->Length(); i++) {
    LookupResult& result = mResults->ElementAt(i);

    // Now, see if it verifies a lookup
    if (!result.mNoise
        && result.CompleteHash() == hash
        && result.mTableName.Equals(tableName)) {
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

  LOG(("nsUrlClassifierLookupCallback::HandleResults [%p, %u results]",
       this, mResults->Length()));

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
    } else if (!result.Confirmed()) {
      LOG(("Skipping result %X from table %s (not confirmed)",
           result.PartialHashHex().get(), result.mTableName.get()));
      continue;
    }

    LOG(("Confirmed result %X from table %s",
         result.PartialHashHex().get(), result.mTableName.get()));

    if (tables.IndexOf(result.mTableName) == nsTArray<nsCString>::NoIndex) {
      tables.AppendElement(result.mTableName);
    }
  }

  // Some parts of this gethash request generated no hits at all.
  // Prefixes must have been removed from the database since our last update.
  // Save the prefixes we checked to prevent repeated requests
  // until the next update.
  nsAutoPtr<PrefixArray> cacheMisses(new PrefixArray());
  if (cacheMisses) {
    for (uint32_t i = 0; i < mResults->Length(); i++) {
      LookupResult &result = mResults->ElementAt(i);
      if (!result.Confirmed() && !result.mNoise) {
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


// -------------------------------------------------------------------------
// Helper class for nsIURIClassifier implementation, translates table names
// to nsIURIClassifier enums.

class nsUrlClassifierClassifyCallback final : public nsIUrlClassifierCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERCALLBACK

  explicit nsUrlClassifierClassifyCallback(nsIURIClassifierCallback *c)
    : mCallback(c)
    {}

private:
  ~nsUrlClassifierClassifyCallback() {}

  nsCOMPtr<nsIURIClassifierCallback> mCallback;
};

NS_IMPL_ISUPPORTS(nsUrlClassifierClassifyCallback,
                  nsIUrlClassifierCallback)

NS_IMETHODIMP
nsUrlClassifierClassifyCallback::HandleEvent(const nsACString& tables)
{
  nsresult response = TablesToResponse(tables);
  mCallback->OnClassifyComplete(response);
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

nsresult
nsUrlClassifierDBService::ReadTablesFromPrefs()
{
  nsCString allTables;
  nsCString tables;
  Preferences::GetCString(PHISH_TABLE_PREF, &allTables);

  Preferences::GetCString(MALWARE_TABLE_PREF, &tables);
  if (!tables.IsEmpty()) {
    allTables.Append(',');
    allTables.Append(tables);
  }

  Preferences::GetCString(DOWNLOAD_BLOCK_TABLE_PREF, &tables);
  if (!tables.IsEmpty()) {
    allTables.Append(',');
    allTables.Append(tables);
  }

  Preferences::GetCString(DOWNLOAD_ALLOW_TABLE_PREF, &tables);
  if (!tables.IsEmpty()) {
    allTables.Append(',');
    allTables.Append(tables);
  }

  Preferences::GetCString(TRACKING_TABLE_PREF, &tables);
  if (!tables.IsEmpty()) {
    allTables.Append(',');
    allTables.Append(tables);
  }

  Preferences::GetCString(TRACKING_WHITELIST_TABLE_PREF, &tables);
  if (!tables.IsEmpty()) {
    allTables.Append(',');
    allTables.Append(tables);
  }

  Preferences::GetCString(BLOCKED_TABLE_PREF, &tables);
  if (!tables.IsEmpty()) {
    allTables.Append(',');
    allTables.Append(tables);
  }

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

  // Retrieve all the preferences.
  mCheckMalware = Preferences::GetBool(CHECK_MALWARE_PREF,
    CHECK_MALWARE_DEFAULT);
  mCheckPhishing = Preferences::GetBool(CHECK_PHISHING_PREF,
    CHECK_PHISHING_DEFAULT);
  mCheckBlockedURIs = Preferences::GetBool(CHECK_BLOCKED_PREF,
    CHECK_BLOCKED_DEFAULT);
  uint32_t gethashNoise = Preferences::GetUint(GETHASH_NOISE_PREF,
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

  rv = mWorker->Init(gethashNoise, cacheDir);
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
  Preferences::AddStrongObserver(this, CHECK_MALWARE_PREF);
  Preferences::AddStrongObserver(this, CHECK_PHISHING_PREF);
  Preferences::AddStrongObserver(this, CHECK_BLOCKED_PREF);
  Preferences::AddStrongObserver(this, GETHASH_NOISE_PREF);
  Preferences::AddStrongObserver(this, CONFIRM_AGE_PREF);
  Preferences::AddStrongObserver(this, PHISH_TABLE_PREF);
  Preferences::AddStrongObserver(this, MALWARE_TABLE_PREF);
  Preferences::AddStrongObserver(this, TRACKING_TABLE_PREF);
  Preferences::AddStrongObserver(this, TRACKING_WHITELIST_TABLE_PREF);
  Preferences::AddStrongObserver(this, BLOCKED_TABLE_PREF);
  Preferences::AddStrongObserver(this, DOWNLOAD_BLOCK_TABLE_PREF);
  Preferences::AddStrongObserver(this, DOWNLOAD_ALLOW_TABLE_PREF);
  Preferences::AddStrongObserver(this, DISALLOW_COMPLETION_TABLE_PREF);

  return NS_OK;
}

void
nsUrlClassifierDBService::BuildTables(bool aTrackingProtectionEnabled,
                                      nsCString &tables)
{
  nsAutoCString malware;
  // LookupURI takes a comma-separated list already.
  Preferences::GetCString(MALWARE_TABLE_PREF, &malware);
  if (mCheckMalware && !malware.IsEmpty()) {
    tables.Append(malware);
  }
  nsAutoCString phishing;
  Preferences::GetCString(PHISH_TABLE_PREF, &phishing);
  if (mCheckPhishing && !phishing.IsEmpty()) {
    tables.Append(',');
    tables.Append(phishing);
  }
  if (aTrackingProtectionEnabled) {
    nsAutoCString tracking, trackingWhitelist;
    Preferences::GetCString(TRACKING_TABLE_PREF, &tracking);
    if (!tracking.IsEmpty()) {
      tables.Append(',');
      tables.Append(tracking);
    }
    Preferences::GetCString(TRACKING_WHITELIST_TABLE_PREF, &trackingWhitelist);
    if (!trackingWhitelist.IsEmpty()) {
      tables.Append(',');
      tables.Append(trackingWhitelist);
    }
  }
  nsAutoCString blocked;
  Preferences::GetCString(BLOCKED_TABLE_PREF, &blocked);
  if (mCheckBlockedURIs && !blocked.IsEmpty()) {
    tables.Append(',');
    tables.Append(blocked);
  }

  if (StringBeginsWith(tables, NS_LITERAL_CSTRING(","))) {
    tables.Cut(0, 1);
  }
}

// nsChannelClassifier is the only consumer of this interface.
NS_IMETHODIMP
nsUrlClassifierDBService::Classify(nsIPrincipal* aPrincipal,
                                   bool aTrackingProtectionEnabled,
                                   nsIURIClassifierCallback* c,
                                   bool* result)
{
  NS_ENSURE_ARG(aPrincipal);

  if (XRE_IsContentProcess()) {
    using namespace mozilla::dom;
    auto actor = static_cast<URLClassifierChild*>
      (ContentChild::GetSingleton()->
         SendPURLClassifierConstructor(IPC::Principal(aPrincipal),
                                       aTrackingProtectionEnabled,
                                       result));
    if (actor) {
      actor->SetCallback(c);
    }
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

  nsAutoCString tables;
  BuildTables(aTrackingProtectionEnabled, tables);

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

  return mWorkerProxy->ResetDatabase();
}

NS_IMETHODIMP
nsUrlClassifierDBService::ReloadDatabase()
{
  NS_ENSURE_TRUE(gDbBackgroundThread, NS_ERROR_NOT_INITIALIZED);

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

    if (NS_LITERAL_STRING(CHECK_MALWARE_PREF).Equals(aData)) {
      mCheckMalware = Preferences::GetBool(CHECK_MALWARE_PREF,
        CHECK_MALWARE_DEFAULT);
    } else if (NS_LITERAL_STRING(CHECK_PHISHING_PREF).Equals(aData)) {
      mCheckPhishing = Preferences::GetBool(CHECK_PHISHING_PREF,
        CHECK_PHISHING_DEFAULT);
    } else if (NS_LITERAL_STRING(CHECK_BLOCKED_PREF).Equals(aData)) {
      mCheckBlockedURIs = Preferences::GetBool(CHECK_BLOCKED_PREF,
        CHECK_BLOCKED_DEFAULT);
    } else if (
      NS_LITERAL_STRING(PHISH_TABLE_PREF).Equals(aData) ||
      NS_LITERAL_STRING(MALWARE_TABLE_PREF).Equals(aData) ||
      NS_LITERAL_STRING(TRACKING_TABLE_PREF).Equals(aData) ||
      NS_LITERAL_STRING(TRACKING_WHITELIST_TABLE_PREF).Equals(aData) ||
      NS_LITERAL_STRING(BLOCKED_TABLE_PREF).Equals(aData) ||
      NS_LITERAL_STRING(DOWNLOAD_BLOCK_TABLE_PREF).Equals(aData) ||
      NS_LITERAL_STRING(DOWNLOAD_ALLOW_TABLE_PREF).Equals(aData) ||
      NS_LITERAL_STRING(DISALLOW_COMPLETION_TABLE_PREF).Equals(aData)) {
      // Just read everything again.
      ReadTablesFromPrefs();
    } else if (NS_LITERAL_STRING(CONFIRM_AGE_PREF).Equals(aData)) {
      gFreshnessGuarantee = Preferences::GetInt(CONFIRM_AGE_PREF,
        CONFIRM_AGE_DEFAULT_SEC);
    }
  } else if (!strcmp(aTopic, "quit-application")) {
    Shutdown();
  } else if (!strcmp(aTopic, "profile-before-change")) {
    // Unit test does not receive "quit-application",
    // need call shutdown in this case
    Shutdown();
    LOG(("joining background thread"));
    mWorkerProxy = nullptr;
    nsIThread *backgroundThread = gDbBackgroundThread;
    gDbBackgroundThread = nullptr;
    backgroundThread->Shutdown();
    NS_RELEASE(backgroundThread);
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

  if (!gDbBackgroundThread || gShuttingDownThread)
    return NS_OK;

  gShuttingDownThread = true;

  Telemetry::AutoTimer<Telemetry::URLCLASSIFIER_SHUTDOWN_TIME> timer;

  mCompleters.Clear();

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->RemoveObserver(CHECK_MALWARE_PREF, this);
    prefs->RemoveObserver(CHECK_PHISHING_PREF, this);
    prefs->RemoveObserver(CHECK_BLOCKED_PREF, this);
    prefs->RemoveObserver(PHISH_TABLE_PREF, this);
    prefs->RemoveObserver(MALWARE_TABLE_PREF, this);
    prefs->RemoveObserver(TRACKING_TABLE_PREF, this);
    prefs->RemoveObserver(TRACKING_WHITELIST_TABLE_PREF, this);
    prefs->RemoveObserver(BLOCKED_TABLE_PREF, this);
    prefs->RemoveObserver(DOWNLOAD_BLOCK_TABLE_PREF, this);
    prefs->RemoveObserver(DOWNLOAD_ALLOW_TABLE_PREF, this);
    prefs->RemoveObserver(DISALLOW_COMPLETION_TABLE_PREF, this);
    prefs->RemoveObserver(CONFIRM_AGE_PREF, this);
  }

  DebugOnly<nsresult> rv;
  // First close the db connection.
  if (mWorker) {
    rv = mWorkerProxy->CancelUpdate();
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to post cancel update event");

    rv = mWorkerProxy->CloseDb();
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to post close db event");
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
