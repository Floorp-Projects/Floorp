/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookiePersistentStorage_h
#define mozilla_net_CookiePersistentStorage_h

#include "CookieStorage.h"

#include "mozilla/Atomics.h"
#include "mozilla/Monitor.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozIStorageBindingParamsArray.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageStatement.h"
#include "mozIStorageStatementCallback.h"

class mozIStorageAsyncStatement;
class mozIStorageService;
class nsICookieTransactionCallback;
class nsIEffectiveTLDService;

namespace mozilla {
namespace net {

class CookiePersistentStorage final : public CookieStorage {
 public:
  // Result codes for TryInitDB() and Read().
  enum OpenDBResult { RESULT_OK, RESULT_RETRY, RESULT_FAILURE };

  static already_AddRefed<CookiePersistentStorage> Create();

  void HandleCorruptDB();

  void RemoveCookiesWithOriginAttributes(
      const OriginAttributesPattern& aPattern,
      const nsACString& aBaseDomain) override;

  void RemoveCookiesFromExactHost(
      const nsACString& aHost, const nsACString& aBaseDomain,
      const OriginAttributesPattern& aPattern) override;

  void StaleCookies(const nsTArray<Cookie*>& aCookieList,
                    int64_t aCurrentTimeInUsec) override;

  void Close() override;

  void EnsureInitialized() override;

  void CleanupCachedStatements();
  void CleanupDBConnection();

  void Activate();

  void RebuildCorruptDB();
  void HandleDBClosed();

  nsresult RunInTransaction(nsICookieTransactionCallback* aCallback) override;

  // State of the database connection.
  enum CorruptFlag {
    OK,                   // normal
    CLOSING_FOR_REBUILD,  // corruption detected, connection closing
    REBUILDING            // close complete, rebuilding database from memory
  };

  CorruptFlag GetCorruptFlag() const { return mCorruptFlag; }

  void SetCorruptFlag(CorruptFlag aFlag) { mCorruptFlag = aFlag; }

 protected:
  const char* NotificationTopic() const override { return "cookie-changed"; }

  void NotifyChangedInternal(nsICookieNotification* aNotification,
                             bool aOldCookieIsSession) override;

  void RemoveAllInternal() override;

  void RemoveCookieFromDB(const Cookie& aCookie) override;

  void StoreCookie(const nsACString& aBaseDomain,
                   const OriginAttributes& aOriginAttributes,
                   Cookie* aCookie) override;

 private:
  CookiePersistentStorage();

  static void UpdateCookieInList(Cookie* aCookie, int64_t aLastAccessed,
                                 mozIStorageBindingParamsArray* aParamsArray);

  void PrepareCookieRemoval(const Cookie& aCookie,
                            mozIStorageBindingParamsArray* aParamsArray);

  void InitDBConn();
  nsresult InitDBConnInternal();

  OpenDBResult TryInitDB(bool aRecreateDB);
  OpenDBResult Read();

  nsresult CreateTableWorker(const char* aName);
  nsresult CreateTable();
  nsresult CreateTableForSchemaVersion6();
  nsresult CreateTableForSchemaVersion5();

  static UniquePtr<CookieStruct> GetCookieFromRow(mozIStorageStatement* aRow);

  already_AddRefed<nsIArray> PurgeCookies(int64_t aCurrentTimeInUsec,
                                          uint16_t aMaxNumberOfCookies,
                                          int64_t aCookiePurgeAge) override;

  void DeleteFromDB(mozIStorageBindingParamsArray* aParamsArray);

  void MaybeStoreCookiesToDB(mozIStorageBindingParamsArray* aParamsArray);

  nsCOMPtr<nsIThread> mThread;
  nsCOMPtr<mozIStorageService> mStorageService;
  nsCOMPtr<nsIEffectiveTLDService> mTLDService;

  // encapsulates a (key, Cookie) tuple for temporary storage purposes.
  struct CookieDomainTuple {
    CookieKey key;
    OriginAttributes originAttributes;
    UniquePtr<CookieStruct> cookie;
  };

  // thread
  TimeStamp mEndInitDBConn;
  nsTArray<CookieDomainTuple> mReadArray;

  Monitor mMonitor MOZ_UNANNOTATED;

  Atomic<bool> mInitialized;
  Atomic<bool> mInitializedDBConn;

  nsCOMPtr<nsIFile> mCookieFile;
  nsCOMPtr<mozIStorageConnection> mDBConn;
  nsCOMPtr<mozIStorageAsyncStatement> mStmtInsert;
  nsCOMPtr<mozIStorageAsyncStatement> mStmtDelete;
  nsCOMPtr<mozIStorageAsyncStatement> mStmtUpdate;

  CorruptFlag mCorruptFlag;

  // Various parts representing asynchronous read state. These are useful
  // while the background read is taking place.
  nsCOMPtr<mozIStorageConnection> mSyncConn;

  // DB completion handlers.
  nsCOMPtr<mozIStorageStatementCallback> mInsertListener;
  nsCOMPtr<mozIStorageStatementCallback> mUpdateListener;
  nsCOMPtr<mozIStorageStatementCallback> mRemoveListener;
  nsCOMPtr<mozIStorageCompletionCallback> mCloseListener;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookiePersistentStorage_h
