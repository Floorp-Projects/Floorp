//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierDBService_h_
#define nsUrlClassifierDBService_h_

#include <nsISupportsUtils.h>

#include "nsID.h"
#include "nsInterfaceHashtable.h"
#include "nsIObserver.h"
#include "nsUrlClassifierPrefixSet.h"
#include "nsIUrlClassifierHashCompleter.h"
#include "nsIUrlListManager.h"
#include "nsIUrlClassifierDBService.h"
#include "nsIURIClassifier.h"
#include "nsToolkitCompsCID.h"
#include "nsICryptoHMAC.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/TimeStamp.h"

#include "Entries.h"
#include "LookupCache.h"

// The hash length for a domain key.
#define DOMAIN_LENGTH 4

// The hash length of a partial hash entry.
#define PARTIAL_LENGTH 4

// The hash length of a complete hash entry.
#define COMPLETE_LENGTH 32

using namespace mozilla::safebrowsing;

class nsUrlClassifierDBServiceWorker;
class nsIThread;
class nsIURI;
class UrlClassifierDBServiceWorkerProxy;
namespace mozilla {
namespace safebrowsing {
class Classifier;
class ProtocolParser;
class TableUpdate;
} // namespace safebrowsing
} // namespace mozilla

// This is a proxy class that just creates a background thread and delagates
// calls to the background thread.
class nsUrlClassifierDBService final : public nsIUrlClassifierDBService,
                                       public nsIURIClassifier,
                                       public nsIObserver
{
public:
  // This is thread safe. It throws an exception if the thread is busy.
  nsUrlClassifierDBService();

  nsresult Init();

  static nsUrlClassifierDBService* GetInstance(nsresult *result);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_URLCLASSIFIERDBSERVICE_CID)

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERDBSERVICE
  NS_DECL_NSIURICLASSIFIER
  NS_DECL_NSIOBSERVER

  bool GetCompleter(const nsACString& tableName,
                      nsIUrlClassifierHashCompleter** completer);
  nsresult CacheCompletions(mozilla::safebrowsing::CacheResultArray *results);
  nsresult CacheMisses(mozilla::safebrowsing::PrefixArray *results);

  static nsIThread* BackgroundThread();

private:
  // No subclassing
  ~nsUrlClassifierDBService();

  // Disallow copy constructor
  nsUrlClassifierDBService(nsUrlClassifierDBService&);

  nsresult LookupURI(nsIPrincipal* aPrincipal,
                     const nsACString& tables,
                     nsIUrlClassifierCallback* c,
                     bool forceCheck, bool *didCheck);

  // Close db connection and join the background thread if it exists.
  nsresult Shutdown();

  // Check if the key is on a known-clean host.
  nsresult CheckClean(const nsACString &lookupKey,
                      bool *clean);

  // Read everything into mGethashTables and mDisallowCompletionTables
  nsresult ReadTablesFromPrefs();

  // Build a comma-separated list of tables to check
  void BuildTables(bool trackingProtectionEnabled, nsCString& tables);

  RefPtr<nsUrlClassifierDBServiceWorker> mWorker;
  RefPtr<UrlClassifierDBServiceWorkerProxy> mWorkerProxy;

  nsInterfaceHashtable<nsCStringHashKey, nsIUrlClassifierHashCompleter> mCompleters;

  // TRUE if the nsURIClassifier implementation should check for malware
  // uris on document loads.
  bool mCheckMalware;

  // TRUE if the nsURIClassifier implementation should check for phishing
  // uris on document loads.
  bool mCheckPhishing;

  // TRUE if the nsURIClassifier implementation should check for tracking
  // uris on document loads.
  bool mCheckTracking;

  // TRUE if the nsURIClassifier implementation should check for blocked
  // uris on document loads.
  bool mCheckBlockedURIs;

  // TRUE if a BeginUpdate() has been called without an accompanying
  // CancelUpdate()/FinishUpdate().  This is used to prevent competing
  // updates, not to determine whether an update is still being
  // processed.
  bool mInUpdate;

  // The list of tables that can use the default hash completer object.
  nsTArray<nsCString> mGethashTables;

  // The list of tables that should never be hash completed.
  nsTArray<nsCString> mDisallowCompletionsTables;

  // Thread that we do the updates on.
  static nsIThread* gDbBackgroundThread;
};

class nsUrlClassifierDBServiceWorker final : public nsIUrlClassifierDBService
{
public:
  nsUrlClassifierDBServiceWorker();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERDBSERVICE

  nsresult Init(uint32_t aGethashNoise, nsCOMPtr<nsIFile> aCacheDir);

  // Queue a lookup for the worker to perform, called in the main thread.
  // tables is a comma-separated list of tables to query
  nsresult QueueLookup(const nsACString& lookupKey,
                       const nsACString& tables,
                       nsIUrlClassifierLookupCallback* callback);

  // Handle any queued-up lookups.  We call this function during long-running
  // update operations to prevent lookups from blocking for too long.
  nsresult HandlePendingLookups();

  // Perform a blocking classifier lookup for a given url. Can be called on
  // either the main thread or the worker thread.
  nsresult DoLocalLookup(const nsACString& spec,
                         const nsACString& tables,
                         LookupResultArray* results);

  // Open the DB connection
  nsresult OpenDb();

  // Provide a way to forcibly close the db connection.
  nsresult CloseDb();

  nsresult CacheCompletions(CacheResultArray * aEntries);
  nsresult CacheMisses(PrefixArray * aEntries);

private:
  // No subclassing
  ~nsUrlClassifierDBServiceWorker();

  // Disallow copy constructor
  nsUrlClassifierDBServiceWorker(nsUrlClassifierDBServiceWorker&);

  // Applies the current transaction and resets the update/working times.
  nsresult ApplyUpdate();

  // Reset the in-progress update stream
  void ResetStream();

  // Reset the in-progress update
  void ResetUpdate();

  // Perform a classifier lookup for a given url.
  nsresult DoLookup(const nsACString& spec,
                    const nsACString& tables,
                    nsIUrlClassifierLookupCallback* c);

  nsresult AddNoise(const Prefix aPrefix,
                    const nsCString tableName,
                    uint32_t aCount,
                    LookupResultArray& results);

  // Can only be used on the background thread
  nsCOMPtr<nsICryptoHash> mCryptoHash;

  nsAutoPtr<mozilla::safebrowsing::Classifier> mClassifier;
  // The class that actually parses the update chunks.
  nsAutoPtr<ProtocolParser> mProtocolParser;

  // Directory where to store the SB databases.
  nsCOMPtr<nsIFile> mCacheDir;

  // XXX: maybe an array of autoptrs.  Or maybe a class specifically
  // storing a series of updates.
  nsTArray<mozilla::safebrowsing::TableUpdate*> mTableUpdates;

  int32_t mUpdateWait;

  // Entries that cannot be completed. We expect them to die at
  // the next update
  PrefixArray mMissCache;

  // Stores the last results that triggered a table update.
  CacheResultArray mLastResults;

  nsresult mUpdateStatus;
  nsTArray<nsCString> mUpdateTables;

  nsCOMPtr<nsIUrlClassifierUpdateObserver> mUpdateObserver;
  bool mInStream;

  // The number of noise entries to add to the set of lookup results.
  uint32_t mGethashNoise;

  // Pending lookups are stored in a queue for processing.  The queue
  // is protected by mPendingLookupLock.
  mozilla::Mutex mPendingLookupLock;

  class PendingLookup {
  public:
    mozilla::TimeStamp mStartTime;
    nsCString mKey;
    nsCString mTables;
    nsCOMPtr<nsIUrlClassifierLookupCallback> mCallback;
  };

  // list of pending lookups
  nsTArray<PendingLookup> mPendingLookups;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsUrlClassifierDBService, NS_URLCLASSIFIERDBSERVICE_CID)

#endif // nsUrlClassifierDBService_h_
