/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieStorage_h
#define mozilla_net_CookieStorage_h

#include "CookieKey.h"

#include "mozIStorageBindingParamsArray.h"
#include "nsIObserver.h"
#include "nsTHashtable.h"
#include "nsWeakReference.h"

class nsIPrefBranch;

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

class CookieStorage : public nsIObserver, public nsSupportsWeakReference {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

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

  virtual void RemoveCookiesWithOriginAttributes(
      const mozilla::OriginAttributesPattern& aPattern,
      const nsACString& aBaseDomain);

  virtual void RemoveCookiesFromExactHost(
      const nsACString& aHost, const nsACString& aBaseDomain,
      const mozilla::OriginAttributesPattern& aPattern);

  void RemoveAll();

  void NotifyChanged(nsISupports* aSubject, const char16_t* aData,
                     bool aOldCookieIsSession = false, bool aFromHttp = false);

  void AddCookie(const nsACString& aBaseDomain,
                 const OriginAttributes& aOriginAttributes, Cookie* aCookie,
                 int64_t aCurrentTimeInUsec, nsIURI* aHostURI,
                 const nsACString& aCookieHeader, bool aFromHttp);

  void CreateOrUpdatePurgeList(nsIArray** aPurgeList, nsICookie* aCookie);

  virtual void StaleCookies(const nsTArray<Cookie*>& aCookieList,
                            int64_t aCurrentTimeInUsec) = 0;

  virtual void Close() = 0;

 protected:
  CookieStorage();
  virtual ~CookieStorage();

  void Init();

  void AddCookieToList(const nsACString& aBaseDomain,
                       const OriginAttributes& aOriginAttributes,
                       mozilla::net::Cookie* aCookie,
                       mozIStorageBindingParamsArray* aParamsArray,
                       bool aWriteToDB = true);

  virtual const char* NotificationTopic() const = 0;

  virtual void NotifyChangedInternal(nsISupports* aSubject,
                                     const char16_t* aData,
                                     bool aOldCookieIsSession) = 0;

  virtual void WriteCookieToDB(const nsACString& aBaseDomain,
                               const OriginAttributes& aOriginAttributes,
                               mozilla::net::Cookie* aCookie,
                               mozIStorageBindingParamsArray* aParamsArray) = 0;

  virtual void RemoveAllInternal() = 0;

  void RemoveCookieFromList(
      const CookieListIter& aIter,
      mozIStorageBindingParamsArray* aParamsArray = nullptr);

  virtual void RemoveCookieFromListInternal(
      const CookieListIter& aIter,
      mozIStorageBindingParamsArray* aParamsArray = nullptr) = 0;

  virtual void MaybeCreateDeleteBindingParamsArray(
      mozIStorageBindingParamsArray** aParamsArray) = 0;

  virtual void DeleteFromDB(mozIStorageBindingParamsArray* aParamsArray) = 0;

  nsTHashtable<CookieEntry> mHostTable;

  uint32_t mCookieCount;

 private:
  void PrefChanged(nsIPrefBranch* aPrefBranch);

  bool FindSecureCookie(const nsACString& aBaseDomain,
                        const OriginAttributes& aOriginAttributes,
                        Cookie* aCookie);

  void FindStaleCookies(CookieEntry* aEntry, int64_t aCurrentTime,
                        bool aIsSecure, nsTArray<CookieListIter>& aOutput,
                        uint32_t aLimit);

  void UpdateCookieOldestTime(Cookie* aCookie);

  already_AddRefed<nsIArray> CreatePurgeList(nsICookie* aCookie);

  already_AddRefed<nsIArray> PurgeCookies(int64_t aCurrentTimeInUsec,
                                          uint16_t aMaxNumberOfCookies,
                                          int64_t aCookiePurgeAge);

  int64_t mCookieOldestTime;

  uint16_t mMaxNumberOfCookies;
  uint16_t mMaxCookiesPerHost;
  uint16_t mCookieQuotaPerHost;
  int64_t mCookiePurgeAge;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieStorage_h
