/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_BounceTrackingProtectionStorage_h__
#define mozilla_BounceTrackingProtectionStorage_h__

#include "mozIStorageFunction.h"
#include "mozilla/Logging.h"
#include "mozilla/Monitor.h"
#include "mozilla/ThreadSafety.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/FlippedOnce.h"
#include "nsIAsyncShutdown.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsISupports.h"
#include "nsTHashMap.h"
#include "mozIStorageConnection.h"
#include "mozilla/OriginAttributesHashKey.h"

class nsIPrincipal;
class mozIStorageConnection;

namespace mozilla {

class BounceTrackingStateGlobal;
class BounceTrackingState;
class OriginAttributes;

extern LazyLogModule gBounceTrackingProtectionLog;

class BounceTrackingProtectionStorage final : public nsIObserver,
                                              public nsIAsyncShutdownBlocker,
                                              public SupportsWeakPtr {
  friend class BounceTrackingStateGlobal;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

 public:
  BounceTrackingProtectionStorage()
      : mMonitor("mozilla::BounceTrackingProtectionStorage::mMonitor"),
        mPendingWrites(0){};

  // Initialises the storage including the on-disk database.
  [[nodiscard]] nsresult Init();

  // Getters for mStateGlobal.
  BounceTrackingStateGlobal* GetOrCreateStateGlobal(
      const OriginAttributes& aOriginAttributes);

  BounceTrackingStateGlobal* GetOrCreateStateGlobal(nsIPrincipal* aPrincipal);

  BounceTrackingStateGlobal* GetOrCreateStateGlobal(
      BounceTrackingState* aBounceTrackingState);

  using StateGlobalMap =
      nsTHashMap<OriginAttributesHashKey, RefPtr<BounceTrackingStateGlobal>>;
  // Provides a read-only reference to the state global map.
  const StateGlobalMap& StateGlobalMapRef() { return mStateGlobal; }

  // The enum values match the database type field. Updating them requires a DB
  // migration.
  enum class EntryType : uint8_t { BounceTracker = 0, UserActivation = 1 };

  // Clear all state for a given site host. If aOriginAttributes is passed, only
  // entries for that OA will be deleted.
  [[nodiscard]] nsresult ClearBySiteHost(const nsACString& aSiteHost,
                                         OriginAttributes* aOriginAttributes);

  // Clear all state within a given time range.
  [[nodiscard]] nsresult ClearByTimeRange(PRTime aFrom, PRTime aTo);

  // Clear all state for a given OriginAttributesPattern.
  [[nodiscard]] nsresult ClearByOriginAttributesPattern(
      const OriginAttributesPattern& aOriginAttributesPattern);

  // Clear all state.
  [[nodiscard]] nsresult Clear();

 private:
  ~BounceTrackingProtectionStorage() = default;

  // Worker thread. This should be a valid thread after Init() returns and be
  // destroyed when we finalize
  nsCOMPtr<nsISerialEventTarget> mBackgroundThread;  // main thread only

  // Database connections. Guaranteed to be non-null and working once
  // initialized and not-yet finalized
  RefPtr<mozIStorageConnection> mDatabaseConnection;  // Worker thread only

  // Wait (non-blocking) until the service is fully initialized. We may be
  // waiting for that async work started by Init().
  [[nodiscard]] nsresult WaitForInitialization();

  // Called to indicate to the async shutdown service that we are all wrapped
  // up. This also spins down the worker thread, since it is called after all
  // disk database connections are closed.
  void Finalize();

  // Utility function to grab the correct barrier this service needs to shut
  // down by
  already_AddRefed<nsIAsyncShutdownClient> GetAsyncShutdownBarrier() const;

  // Initialises the DB connection on the worker thread.
  [[nodiscard]] nsresult CreateDatabaseConnection();

  // Creates amd initialises the database table if needed. Worker thread only.
  [[nodiscard]] nsresult EnsureTable();

  // Temporary data structure used to import db data into memory.
  struct ImportEntry {
    OriginAttributes mOriginAttributes;
    nsCString mSiteHost;
    EntryType mEntryType;
    PRTime mTimeStamp;
  };

  // Imports state from the database on disk into memory.
  [[nodiscard]] nsresult LoadMemoryStateFromDisk();

  // Used to (thread-safely) track how many operations have been launched to the
  // worker thread so that we can wait for it to hit zero before close the disk
  // database connection
  void IncrementPendingWrites();
  void DecrementPendingWrites();

  // Update or create database entry. Worker thread only.
  [[nodiscard]] static nsresult UpsertData(
      mozIStorageConnection* aDatabaseConnection,
      const OriginAttributes& aOriginAttributes, const nsACString& aSiteHost,
      EntryType aEntryType, PRTime aTimeStamp);

  // Delete database entries. Worker thread only.
  [[nodiscard]] static nsresult DeleteData(
      mozIStorageConnection* aDatabaseConnection,
      Maybe<OriginAttributes> aOriginAttributes, const nsACString& aSiteHost);

  // Delete all entries before a given time. Worker thread only.
  // If aEntryType is passed only entries of that type will be deleted.
  [[nodiscard]] static nsresult DeleteDataInTimeRange(
      mozIStorageConnection* aDatabaseConnection,
      Maybe<OriginAttributes> aOriginAttributes, PRTime aFrom,
      Maybe<PRTime> aTo,
      Maybe<BounceTrackingProtectionStorage::EntryType> aEntryType = Nothing{});

  // Delete all entries matching the given OriginAttributesPattern. Worker
  // thread only.
  [[nodiscard]] static nsresult DeleteDataByOriginAttributesPattern(
      mozIStorageConnection* aDatabaseConnection,
      const OriginAttributesPattern& aOriginAttributesPattern);

  // Clear all entries from the database.
  [[nodiscard]] static nsresult ClearData(
      mozIStorageConnection* aDatabaseConnection);

  // Service state management. We protect these variables with a monitor. This
  // monitor is also used to signal the completion of initialization and
  // finalization performed in the worker thread.
  Monitor mMonitor;

  FlippedOnce<false> mInitialized MOZ_GUARDED_BY(mMonitor);
  FlippedOnce<false> mErrored MOZ_GUARDED_BY(mMonitor);
  FlippedOnce<false> mShuttingDown MOZ_GUARDED_BY(mMonitor);
  FlippedOnce<false> mFinalized MOZ_GUARDED_BY(mMonitor);
  uint32_t mPendingWrites MOZ_GUARDED_BY(mMonitor);

  // The database file handle. We can only create this in the main thread and
  // need it in the worker to perform blocking disk IO. So we put it on this,
  // since we pass this to the worker anyway
  nsCOMPtr<nsIFile> mDatabaseFile;

  // Map of origin attributes to global state object. This enables us to track
  // bounce tracking state per OA, e.g. to separate private browsing from normal
  // browsing.
  StateGlobalMap mStateGlobal{};

  // Helpers used to sync updates to BounceTrackingStateGlobal with the
  // database.

  // Updates or inserts a DB entry keyed by OA + site host.
  [[nodiscard]] nsresult UpdateDBEntry(
      const OriginAttributes& aOriginAttributes, const nsACString& aSiteHost,
      EntryType aEntryType, PRTime aTimeStamp);

  // Deletes a DB entry keyed by OA + site host. If only aSiteHost is passed,
  // all entries for that host will be deleted across OriginAttributes.
  [[nodiscard]] nsresult DeleteDBEntries(OriginAttributes* aOriginAttributes,
                                         const nsACString& aSiteHost);

  // Delete all DB entries before a given time.
  // If aEntryType is passed only entries of that type will be deleted.
  [[nodiscard]] nsresult DeleteDBEntriesInTimeRange(
      OriginAttributes* aOriginAttributes, PRTime aFrom,
      Maybe<PRTime> aTo = Nothing{}, Maybe<EntryType> aEntryType = Nothing{});

  // Deletes all DB entries matching the given OriginAttributesPattern.
  [[nodiscard]] nsresult DeleteDBEntriesByOriginAttributesPattern(
      const OriginAttributesPattern& aOriginAttributesPattern);
};

// A SQL function to match DB entries by OriginAttributesPattern.
class OriginAttrsPatternMatchOASuffixSQLFunction final
    : public mozIStorageFunction {
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  explicit OriginAttrsPatternMatchOASuffixSQLFunction(
      OriginAttributesPattern const& aPattern)
      : mPattern(aPattern) {}
  OriginAttrsPatternMatchOASuffixSQLFunction() = delete;

 private:
  ~OriginAttrsPatternMatchOASuffixSQLFunction() = default;

  OriginAttributesPattern mPattern;
};

}  // namespace mozilla

#endif  // mozilla_BounceTrackingProtectionStorage_h__
