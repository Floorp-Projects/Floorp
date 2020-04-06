/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieStorage_h
#define mozilla_net_CookieStorage_h

#include "CookieKey.h"

#include "mozIStorageBindingParamsArray.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageStatement.h"
#include "mozIStorageStatementCallback.h"
#include "nsTHashtable.h"

namespace mozilla {
namespace net {

// Inherit from CookieKey so this can be stored in nsTHashTable
// TODO: why aren't we using nsClassHashTable<CookieKey, ArrayType>?
class CookieEntry : public CookieKey {
 public:
  // Hash methods
  typedef nsTArray<RefPtr<Cookie>> ArrayType;
  typedef ArrayType::index_type IndexType;

  explicit CookieEntry(KeyTypePointer aKey) : CookieKey(aKey) {}

  CookieEntry(const CookieEntry& toCopy) {
    // if we end up here, things will break. nsTHashtable shouldn't
    // allow this, since we set ALLOW_MEMMOVE to true.
    MOZ_ASSERT_UNREACHABLE("CookieEntry copy constructor is forbidden!");
  }

  ~CookieEntry() = default;

  inline ArrayType& GetCookies() { return mCookies; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;

 private:
  ArrayType mCookies;
};

// stores the CookieEntry entryclass and an index into the cookie array within
// that entryclass, for purposes of storing an iteration state that points to a
// certain cookie.
struct CookieListIter {
  // default (non-initializing) constructor.
  CookieListIter() = default;

  // explicit constructor to a given iterator state with entryclass 'aEntry'
  // and index 'aIndex'.
  explicit CookieListIter(CookieEntry* aEntry, CookieEntry::IndexType aIndex)
      : entry(aEntry), index(aIndex) {}

  // get the Cookie * the iterator currently points to.
  Cookie* Cookie() const { return entry->GetCookies()[index]; }

  CookieEntry* entry;
  CookieEntry::IndexType index;
};

class CookieStorage final {
 public:
  NS_INLINE_DECL_REFCOUNTING(CookieStorage)

  explicit CookieStorage(bool aPrivateStorage);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  void GetCookies(nsTArray<RefPtr<nsICookie>>& aCookies) const;

  void GetSessionCookies(nsTArray<RefPtr<nsICookie>>& aCookies) const;

  bool FindCookie(const nsACString& aBaseDomain,
                  const OriginAttributes& aOriginAttributes,
                  const nsACString& aHost, const nsACString& aName,
                  const nsACString& aPath, CookieListIter& aIter);

  uint32_t CountCookiesFromHost(const nsACString& aBaseDomain,
                                uint32_t aPrivateBrowsingId);

  void GetAll(nsTArray<RefPtr<nsICookie>>& aResult) const;

  const nsTArray<RefPtr<Cookie>>* GetCookiesFromHost(
      const nsACString& aBaseDomain, const OriginAttributes& aOriginAttributes);

  void GetCookiesWithOriginAttributes(
      const mozilla::OriginAttributesPattern& aPattern,
      const nsACString& aBaseDomain, nsTArray<RefPtr<nsICookie>>& aResult);

  void RemoveCookie(const nsACString& aBaseDomain,
                    const OriginAttributes& aOriginAttributes,
                    const nsACString& aHost, const nsACString& aName,
                    const nsACString& aPath);

  void RemoveCookiesWithOriginAttributes(
      const mozilla::OriginAttributesPattern& aPattern,
      const nsACString& aBaseDomain);

  void RemoveCookiesFromExactHost(
      const nsACString& aHost, const nsACString& aBaseDomain,
      const mozilla::OriginAttributesPattern& aPattern);

  void RemoveAll();

  void NotifyChanged(nsISupports* aSubject, const char16_t* aData,
                     bool aOldCookieIsSession = false, bool aFromHttp = false);

  void HandleCorruptDB();
  void CleanupCachedStatements();
  void CleanupDefaultDBConnection();

  void AddCookie(const nsACString& aBaseDomain,
                 const OriginAttributes& aOriginAttributes, Cookie* aCookie,
                 int64_t aCurrentTimeInUsec, nsIURI* aHostURI,
                 const nsACString& aCookieHeader, bool aFromHttp,
                 uint16_t aMaxNumberOfCookies, uint16_t aMaxCookiesPerHost,
                 uint16_t aCookieQuotaPerHost, int64_t aCookiePurgeAge);

  void AddCookieToList(const nsACString& aBaseDomain,
                       const OriginAttributes& aOriginAttributes,
                       mozilla::net::Cookie* aCookie,
                       mozIStorageBindingParamsArray* aParamsArray,
                       bool aWriteToDB = true);

  void RemoveCookieFromList(
      const CookieListIter& aIter,
      mozIStorageBindingParamsArray* aParamsArray = nullptr);

  void FindStaleCookies(CookieEntry* aEntry, int64_t aCurrentTime,
                        bool aIsSecure, nsTArray<CookieListIter>& aOutput,
                        uint32_t aLimit);

  void CreateOrUpdatePurgeList(nsIArray** aPurgeList, nsICookie* aCookie);

  already_AddRefed<nsIArray> PurgeCookies(int64_t aCurrentTimeInUsec,
                                          uint16_t aMaxNumberOfCookies,
                                          int64_t aCookiePurgeAge);

  nsresult ImportCookies(nsIFile* aCookieFile,
                         nsIEffectiveTLDService* aTLDService,
                         uint16_t aMaxNumberOfCookies,
                         uint16_t aMaxCookiesPerHost,
                         uint16_t aCookieQuotaPerHost, int64_t aCookiePurgeAge);

  void StaleCookies(const nsTArray<Cookie*>& aCookieList,
                    int64_t aCurrentTimeInUsec);

  void Close();

  // TODO: private:

  // State of the database connection.
  enum CorruptFlag {
    OK,                   // normal
    CLOSING_FOR_REBUILD,  // corruption detected, connection closing
    REBUILDING            // close complete, rebuilding database from memory
  };

  nsTHashtable<CookieEntry> hostTable;

  uint32_t cookieCount;
  int64_t cookieOldestTime;

  nsCOMPtr<nsIFile> cookieFile;
  nsCOMPtr<mozIStorageConnection> dbConn;
  nsCOMPtr<mozIStorageAsyncStatement> stmtInsert;
  nsCOMPtr<mozIStorageAsyncStatement> stmtDelete;
  nsCOMPtr<mozIStorageAsyncStatement> stmtUpdate;

  CorruptFlag corruptFlag;

  // Various parts representing asynchronous read state. These are useful
  // while the background read is taking place.
  nsCOMPtr<mozIStorageConnection> syncConn;
  nsCOMPtr<mozIStorageStatement> stmtReadDomain;

  // DB completion handlers.
  nsCOMPtr<mozIStorageStatementCallback> insertListener;
  nsCOMPtr<mozIStorageStatementCallback> updateListener;
  nsCOMPtr<mozIStorageStatementCallback> removeListener;
  nsCOMPtr<mozIStorageCompletionCallback> closeListener;

 private:
  ~CookieStorage();

  bool FindSecureCookie(const nsACString& aBaseDomain,
                        const OriginAttributes& aOriginAttributes,
                        Cookie* aCookie);

  void UpdateCookieOldestTime(Cookie* aCookie);

  already_AddRefed<nsIArray> CreatePurgeList(nsICookie* aCookie);

  void UpdateCookieInList(Cookie* aCookie, int64_t aLastAccessed,
                          mozIStorageBindingParamsArray* aParamsArray);

  bool mPrivateStorage;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieStorage_h
