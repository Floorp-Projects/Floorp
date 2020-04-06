/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieStorage_h
#define mozilla_net_CookieStorage_h

#include "CookieKey.h"

#include "nsIObserver.h"
#include "nsTHashtable.h"
#include "nsWeakReference.h"
#include <functional>

class nsIArray;
class nsICookie;
class nsIPrefBranch;

namespace mozilla {
namespace net {

class Cookie;

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
  mozilla::net::Cookie* Cookie() const { return entry->GetCookies()[index]; }

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

  void GetCookiesWithOriginAttributes(const OriginAttributesPattern& aPattern,
                                      const nsACString& aBaseDomain,
                                      nsTArray<RefPtr<nsICookie>>& aResult);

  void RemoveCookie(const nsACString& aBaseDomain,
                    const OriginAttributes& aOriginAttributes,
                    const nsACString& aHost, const nsACString& aName,
                    const nsACString& aPath);

  virtual void RemoveCookiesWithOriginAttributes(
      const OriginAttributesPattern& aPattern, const nsACString& aBaseDomain);

  virtual void RemoveCookiesFromExactHost(
      const nsACString& aHost, const nsACString& aBaseDomain,
      const OriginAttributesPattern& aPattern);

  void RemoveAll();

  void NotifyChanged(nsISupports* aSubject, const char16_t* aData,
                     bool aOldCookieIsSession = false);

  void AddCookie(const nsACString& aBaseDomain,
                 const OriginAttributes& aOriginAttributes, Cookie* aCookie,
                 int64_t aCurrentTimeInUsec, nsIURI* aHostURI,
                 const nsACString& aCookieHeader, bool aFromHttp);

  static void CreateOrUpdatePurgeList(nsIArray** aPurgedList,
                                      nsICookie* aCookie);

  virtual void StaleCookies(const nsTArray<Cookie*>& aCookieList,
                            int64_t aCurrentTimeInUsec) = 0;

  virtual void Close() = 0;

 protected:
  CookieStorage();
  virtual ~CookieStorage();

  void Init();

  void AddCookieToList(const nsACString& aBaseDomain,
                       const OriginAttributes& aOriginAttributes,
                       Cookie* aCookie);

  virtual void StoreCookie(const nsACString& aBaseDomain,
                           const OriginAttributes& aOriginAttributes,
                           Cookie* aCookie) = 0;

  virtual const char* NotificationTopic() const = 0;

  virtual void NotifyChangedInternal(nsISupports* aSubject,
                                     const char16_t* aData,
                                     bool aOldCookieIsSession) = 0;

  virtual void RemoveAllInternal() = 0;

  // This method calls RemoveCookieFromDB + RemoveCookieFromListInternal.
  void RemoveCookieFromList(const CookieListIter& aIter);

  void RemoveCookieFromListInternal(const CookieListIter& aIter);

  virtual void RemoveCookieFromDB(const CookieListIter& aIter) = 0;

  already_AddRefed<nsIArray> PurgeCookiesWithCallbacks(
      int64_t aCurrentTimeInUsec, uint16_t aMaxNumberOfCookies,
      int64_t aCookiePurgeAge,
      std::function<void(const CookieListIter&)>&& aRemoveCookieCallback,
      std::function<void()>&& aFinalizeCallback);

  nsTHashtable<CookieEntry> mHostTable;

  uint32_t mCookieCount;

 private:
  void PrefChanged(nsIPrefBranch* aPrefBranch);

  bool FindSecureCookie(const nsACString& aBaseDomain,
                        const OriginAttributes& aOriginAttributes,
                        Cookie* aCookie);

  static void FindStaleCookies(CookieEntry* aEntry, int64_t aCurrentTime,
                               bool aIsSecure,
                               nsTArray<CookieListIter>& aOutput,
                               uint32_t aLimit);

  void UpdateCookieOldestTime(Cookie* aCookie);

  static already_AddRefed<nsIArray> CreatePurgeList(nsICookie* aCookie);

  virtual already_AddRefed<nsIArray> PurgeCookies(int64_t aCurrentTimeInUsec,
                                                  uint16_t aMaxNumberOfCookies,
                                                  int64_t aCookiePurgeAge) = 0;

  int64_t mCookieOldestTime;

  uint16_t mMaxNumberOfCookies;
  uint16_t mMaxCookiesPerHost;
  uint16_t mCookieQuotaPerHost;
  int64_t mCookiePurgeAge;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieStorage_h
