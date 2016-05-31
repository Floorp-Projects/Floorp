/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_History_h_
#define mozilla_places_History_h_

#include "mozilla/IHistory.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozIAsyncHistory.h"
#include "nsIDownloadHistory.h"
#include "Database.h"

#include "mozilla/dom/Link.h"
#include "nsTHashtable.h"
#include "nsString.h"
#include "nsURIHashKey.h"
#include "nsTObserverArray.h"
#include "nsDeque.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "mozIStorageConnection.h"

namespace mozilla {
namespace places {

struct VisitData;
class ConcurrentStatementsHolder;

#define NS_HISTORYSERVICE_CID \
  {0x0937a705, 0x91a6, 0x417a, {0x82, 0x92, 0xb2, 0x2e, 0xb1, 0x0d, 0xa8, 0x6c}}

// Initial size of mRecentlyVisitedURIs.
#define RECENTLY_VISITED_URIS_SIZE 64
// Microseconds after which a visit can be expired from mRecentlyVisitedURIs.
// When an URI is reloaded we only take into account the first visit to it, and
// ignore any subsequent visits, if they happen before this time has elapsed.
// A commonly found case is to reload a page every 5 minutes, so we pick a time
// larger than that.
#define RECENTLY_VISITED_URIS_MAX_AGE 6 * 60 * PR_USEC_PER_SEC

class History final : public IHistory
                    , public nsIDownloadHistory
                    , public mozIAsyncHistory
                    , public nsIObserver
                    , public nsIMemoryReporter
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_IHISTORY
  NS_DECL_NSIDOWNLOADHISTORY
  NS_DECL_MOZIASYNCHISTORY
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  History();

  /**
   * Obtains the statement to use to check if a URI is visited or not.
   */
  nsresult GetIsVisitedStatement(mozIStorageCompletionCallback* aCallback);

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
   * Obtains a pointer that has had AddRef called on it.  Used by the service
   * manager only.
   */
  static History* GetSingleton();

  template<int N>
  already_AddRefed<mozIStorageStatement>
  GetStatement(const char (&aQuery)[N])
  {
    mozIStorageConnection* dbConn = GetDBConn();
    NS_ENSURE_TRUE(dbConn, nullptr);
    return mDB->GetStatement(aQuery);
  }

  already_AddRefed<mozIStorageStatement>
  GetStatement(const nsACString& aQuery)
  {
    mozIStorageConnection* dbConn = GetDBConn();
    NS_ENSURE_TRUE(dbConn, nullptr);
    return mDB->GetStatement(aQuery);
  }

  bool IsShuttingDown() const {
    return mShuttingDown;
  }
  Mutex& GetShutdownMutex() {
    return mShutdownMutex;
  }

  /**
   * Helper function to append a new URI to mRecentlyVisitedURIs. See
   * mRecentlyVisitedURIs.
   */
  void AppendToRecentlyVisitedURIs(nsIURI* aURI);

private:
  virtual ~History();

  void InitMemoryReporter();

  /**
   * Obtains a read-write database connection.
   */
  mozIStorageConnection* GetDBConn();

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

  // Ensures new tasks aren't started on destruction.
  bool mShuttingDown;
  // This mutex guards mShuttingDown. Code running in other threads that might
  // schedule tasks that use the database should grab it and check the value of
  // mShuttingDown. If we are already shutting down, the code must gracefully
  // avoid using the db. If we are not, the lock will prevent shutdown from
  // starting in an unexpected moment.
  Mutex mShutdownMutex;

  typedef nsTObserverArray<mozilla::dom::Link* > ObserverArray;

  class KeyClass : public nsURIHashKey
  {
  public:
    explicit KeyClass(const nsIURI* aURI)
    : nsURIHashKey(aURI)
    {
    }
    KeyClass(const KeyClass& aOther)
    : nsURIHashKey(aOther)
    {
      NS_NOTREACHED("Do not call me!");
    }
    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
    {
      return array.ShallowSizeOfExcludingThis(aMallocSizeOf);
    }
    ObserverArray array;
  };

  nsTHashtable<KeyClass> mObservers;

  /**
   * mRecentlyVisitedURIs remembers URIs which have been recently added to
   * history, to avoid saving these locations repeatedly in a short period.
   */
  class RecentURIKey : public nsURIHashKey
  {
  public:
    explicit RecentURIKey(const nsIURI* aURI) : nsURIHashKey(aURI)
    {
    }
    RecentURIKey(const RecentURIKey& aOther) : nsURIHashKey(aOther)
    {
      NS_NOTREACHED("Do not call me!");
    }
    PRTime time;
  };
  nsTHashtable<RecentURIKey> mRecentlyVisitedURIs;
  /**
   * Whether aURI has been visited "recently".
   * See RECENTLY_VISITED_URIS_MAX_AGE.
   */
  bool IsRecentlyVisitedURI(nsIURI* aURI);
};

} // namespace places
} // namespace mozilla

#endif // mozilla_places_History_h_
