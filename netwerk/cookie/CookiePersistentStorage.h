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
      const mozilla::OriginAttributesPattern& aPattern,
      const nsACString& aBaseDomain) override;

  void RemoveCookiesFromExactHost(
      const nsACString& aHost, const nsACString& aBaseDomain,
      const mozilla::OriginAttributesPattern& aPattern) override;

  void StaleCookies(const nsTArray<Cookie*>& aCookieList,
                    int64_t aCurrentTimeInUsec) override;

  void Close() override;

  void EnsureReadComplete();

  void CleanupCachedStatements();
  void CleanupDBConnection();

  nsresult ImportCookies(nsIFile* aCookieFile);

  void Activate();

  void RebuildCorruptDB();
  void HandleDBClosed();

  nsresult RunInTransaction(nsICookieTransactionCallback* aCallback);

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

  void NotifyChangedInternal(nsISupports* aSubject, const char16_t* aData,
                             bool aOldCOokieIsSession) override;

  void WriteCookieToDB(const nsACString& aBaseDomain,
                       const OriginAttributes& aOriginAttributes,
                       mozilla::net::Cookie* aCookie,
                       mozIStorageBindingParamsArray* aParamsArray) override;

  void RemoveAllInternal() override;

  void RemoveCookieFromListInternal(
      const CookieListIter& aIter,
      mozIStorageBindingParamsArray* aParamsArray = nullptr) override;

  void MaybeCreateDeleteBindingParamsArray(
      mozIStorageBindingParamsArray** aParamsArray) override;

  void DeleteFromDB(mozIStorageBindingParamsArray* aParamsArray) override;

 private:
  CookiePersistentStorage();

  void UpdateCookieInList(Cookie* aCookie, int64_t aLastAccessed,
                          mozIStorageBindingParamsArray* aParamsArray);

  void InitDBConn();
  nsresult InitDBConnInternal();

  OpenDBResult TryInitDB(bool aDeleteExistingDB);
  OpenDBResult Read();

  nsresult CreateTableWorker(const char* aName);
  nsresult CreateTable();
  nsresult CreateTableForSchemaVersion6();
  nsresult CreateTableForSchemaVersion5();

  mozilla::UniquePtr<CookieStruct> GetCookieFromRow(mozIStorageStatement* aRow);

  nsCOMPtr<nsIThread> mThread;
  nsCOMPtr<mozIStorageService> mStorageService;
  nsCOMPtr<nsIEffectiveTLDService> mTLDService;

  // encapsulates a (key, Cookie) tuple for temporary storage purposes.
  struct CookieDomainTuple {
    CookieKey key;
    OriginAttributes originAttributes;
    mozilla::UniquePtr<mozilla::net::CookieStruct> cookie;
  };

  // thread
  mozilla::TimeStamp mEndInitDBConn;
  nsTArray<CookieDomainTuple> mReadArray;

  mozilla::Monitor mMonitor;

  mozilla::Atomic<bool> mInitialized;
  mozilla::Atomic<bool> mInitializedDBConn;

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
