/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_History_h_
#define mozilla_places_History_h_

#include <utility>

#include "Database.h"
#include "mozIAsyncHistory.h"
#include "mozIStorageConnection.h"
#include "mozilla/BaseHistory.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/dom/Link.h"
#include "mozilla/dom/PContentChild.h"
#include "nsTHashMap.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsString.h"
#include "nsTHashtable.h"
#include "nsTObserverArray.h"
#include "nsURIHashKey.h"

namespace mozilla {
namespace places {

struct VisitData;
class ConcurrentStatementsHolder;
class VisitedQuery;

// Initial size of mRecentlyVisitedURIs.
#define RECENTLY_VISITED_URIS_SIZE 64
// Microseconds after which a visit can be expired from mRecentlyVisitedURIs.
// When an URI is reloaded we only take into account the first visit to it, and
// ignore any subsequent visits, if they happen before this time has elapsed.
// A commonly found case is to reload a page every 5 minutes, so we pick a time
// larger than that.
#define RECENTLY_VISITED_URIS_MAX_AGE 6 * 60 * PR_USEC_PER_SEC
// When notifying the main thread after inserting visits, we chunk the visits
// into medium-sized groups so that we can amortize the cost of the runnable
// without janking the main thread by expecting it to process hundreds at once.
#define NOTIFY_VISITS_CHUNK_SIZE 100

class History final : public BaseHistory,
                      public mozIAsyncHistory,
                      public nsIObserver,
                      public nsIMemoryReporter {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZIASYNCHISTORY
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  // IHistory
  NS_IMETHOD VisitURI(nsIWidget*, nsIURI*, nsIURI* aLastVisitedURI,
                      uint32_t aFlags) final;
  NS_IMETHOD SetURITitle(nsIURI*, const nsAString&) final;

  // BaseHistory
  void StartPendingVisitedQueries(const PendingVisitedQueries&) final;

  History();

  nsresult QueueVisitedStatement(RefPtr<VisitedQuery>);

  /**
   * Adds an entry in moz_places with the data in aVisitData.
   *
   * @param aVisitData
   *        The visit data to use to populate a new row in moz_places.
   */
  nsresult InsertPlace(VisitData& aVisitData);

  /**
   * Updates an entry in moz_places with the data in aVisitData.
   *
   * @param aVisitData
   *        The visit data to use to update the existing row in moz_places.
   */
  nsresult UpdatePlace(const VisitData& aVisitData);

  /**
   * Loads information about the page into _place from moz_places.
   *
   * @param _place
   *        The VisitData for the place we need to know information about.
   * @param [out] _exists
   *        Whether or the page was recorded in moz_places, false otherwise.
   */
  nsresult FetchPageInfo(VisitData& _place, bool* _exists);

  /**
   * Get the number of bytes of memory this History object is using,
   * including sizeof(*this))
   */
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  /**
   * Obtains a pointer to this service.
   */
  static History* GetService();

  /**
   * Used by the service manager only.
   */
  static already_AddRefed<History> GetSingleton();

  template <int N>
  already_AddRefed<mozIStorageStatement> GetStatement(const char (&aQuery)[N]) {
    // May be invoked on both threads.
    const mozIStorageConnection* dbConn = GetConstDBConn();
    NS_ENSURE_TRUE(dbConn, nullptr);
    return mDB->GetStatement(aQuery);
  }

  already_AddRefed<mozIStorageStatement> GetStatement(
      const nsACString& aQuery) {
    // May be invoked on both threads.
    const mozIStorageConnection* dbConn = GetConstDBConn();
    NS_ENSURE_TRUE(dbConn, nullptr);
    return mDB->GetStatement(aQuery);
  }

  bool IsShuttingDown() {
    MutexAutoLock lockedScope(mShuttingDownMutex);
    return mShuttingDown;
  }

  /**
   * Helper function to append a new URI to mRecentlyVisitedURIs. See
   * mRecentlyVisitedURIs.
   * @param {nsIURI} aURI The URI to append
   * @param {bool} aHidden The hidden status of the visit being appended.
   */
  void AppendToRecentlyVisitedURIs(nsIURI* aURI, bool aHidden);

 private:
  virtual ~History();

  void InitMemoryReporter();

  /**
   * Obtains a read-write database connection, initializing the connection
   * if needed. Must be invoked on the main thread.
   */
  mozIStorageConnection* GetDBConn();

  /**
   * Obtains a read-write database connection, but won't try to initialize it.
   * May be invoked on both threads, but first one must invoke GetDBConn() on
   * the main-thread at least once.
   */
  const mozIStorageConnection* GetConstDBConn();

  /**
   * The database handle.  This is initialized lazily by the first call to
   * GetDBConn(), so never use it directly, or, if you really need, always
   * invoke GetDBConn() before.
   */
  RefPtr<mozilla::places::Database> mDB;

  RefPtr<ConcurrentStatementsHolder> mConcurrentStatementsHolder;

  /**
   * Remove any memory references to tasks and do not take on any more.
   */
  void Shutdown();

  static History* gService;

  // Ensures new tasks aren't started on destruction. Should only be changed on
  // the main thread.
  bool mShuttingDown;
  // This mutex guards mShuttingDown and should be acquired on the helper
  // thread.
  Mutex mShuttingDownMutex;
  // Code running in the helper thread can acquire this mutex to block shutdown
  // from proceeding until done, otherwise it may be impossible to get
  // statements to execute and an insert operation could be interrupted in the
  // middle.
  Mutex mBlockShutdownMutex;

  // Allow private access from the helper thread to acquire mutexes.
  friend class InsertVisitedURIs;

  /**
   * mRecentlyVisitedURIs remembers URIs which have been recently added to
   * history, to avoid saving these locations repeatedly in a short period.
   */
  struct RecentURIVisit {
    PRTime mTime;
    bool mHidden;
  };

  nsTHashMap<nsURIHashKey, RecentURIVisit> mRecentlyVisitedURIs;
};

}  // namespace places
}  // namespace mozilla

#endif  // mozilla_places_History_h_
