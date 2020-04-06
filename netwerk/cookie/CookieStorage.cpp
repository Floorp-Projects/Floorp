/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieCommons.h"
#include "CookieLogging.h"
#include "CookieStorage.h"

#include "mozStorageHelper.h"
#include "nsILineInputStream.h"
#include "nsIMutableArray.h"
#include "nsTPriorityQueue.h"
#include "prprf.h"

#undef ADD_TEN_PERCENT
#define ADD_TEN_PERCENT(i) static_cast<uint32_t>((i) + (i) / 10)

#undef LIMIT
#define LIMIT(x, low, high, default) \
  ((x) >= (low) && (x) <= (high) ? (x) : (default))

// XXX_hack. See bug 178993.
// This is a hack to hide HttpOnly cookies from older browsers
#define HTTP_ONLY_PREFIX "#HttpOnly_"

namespace mozilla {
namespace net {

namespace {

// comparator class for lastaccessed times of cookies.
class CompareCookiesByAge {
 public:
  bool Equals(const CookieListIter& a, const CookieListIter& b) const {
    return a.Cookie()->LastAccessed() == b.Cookie()->LastAccessed() &&
           a.Cookie()->CreationTime() == b.Cookie()->CreationTime();
  }

  bool LessThan(const CookieListIter& a, const CookieListIter& b) const {
    // compare by lastAccessed time, and tiebreak by creationTime.
    int64_t result = a.Cookie()->LastAccessed() - b.Cookie()->LastAccessed();
    if (result != 0) return result < 0;

    return a.Cookie()->CreationTime() < b.Cookie()->CreationTime();
  }
};

// Cookie comparator for the priority queue used in FindStaleCookies.
// Note that the expired cookie has the highest priority.
// Other non-expired cookies are sorted by their age.
class CookieIterComparator {
 private:
  CompareCookiesByAge mAgeComparator;
  int64_t mCurrentTime;

 public:
  explicit CookieIterComparator(int64_t aTime) : mCurrentTime(aTime) {}

  bool LessThan(const CookieListIter& lhs, const CookieListIter& rhs) {
    bool lExpired = lhs.Cookie()->Expiry() <= mCurrentTime;
    bool rExpired = rhs.Cookie()->Expiry() <= mCurrentTime;
    if (lExpired && !rExpired) {
      return true;
    }

    if (!lExpired && rExpired) {
      return false;
    }

    return mAgeComparator.LessThan(lhs, rhs);
  }
};

// comparator class for sorting cookies by entry and index.
class CompareCookiesByIndex {
 public:
  bool Equals(const CookieListIter& a, const CookieListIter& b) const {
    NS_ASSERTION(a.entry != b.entry || a.index != b.index,
                 "cookie indexes should never be equal");
    return false;
  }

  bool LessThan(const CookieListIter& a, const CookieListIter& b) const {
    // compare by entryclass pointer, then by index.
    if (a.entry != b.entry) return a.entry < b.entry;

    return a.index < b.index;
  }
};

void BindCookieParameters(mozIStorageBindingParamsArray* aParamsArray,
                          const CookieKey& aKey, const Cookie* aCookie) {
  NS_ASSERTION(aParamsArray,
               "Null params array passed to BindCookieParameters!");
  NS_ASSERTION(aCookie, "Null cookie passed to BindCookieParameters!");

  // Use the asynchronous binding methods to ensure that we do not acquire the
  // database lock.
  nsCOMPtr<mozIStorageBindingParams> params;
  DebugOnly<nsresult> rv =
      aParamsArray->NewBindingParams(getter_AddRefs(params));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsAutoCString suffix;
  aKey.mOriginAttributes.CreateSuffix(suffix);
  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("originAttributes"),
                                    suffix);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv =
      params->BindUTF8StringByName(NS_LITERAL_CSTRING("name"), aCookie->Name());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("value"),
                                    aCookie->Value());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv =
      params->BindUTF8StringByName(NS_LITERAL_CSTRING("host"), aCookie->Host());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv =
      params->BindUTF8StringByName(NS_LITERAL_CSTRING("path"), aCookie->Path());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = params->BindInt64ByName(NS_LITERAL_CSTRING("expiry"), aCookie->Expiry());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = params->BindInt64ByName(NS_LITERAL_CSTRING("lastAccessed"),
                               aCookie->LastAccessed());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = params->BindInt64ByName(NS_LITERAL_CSTRING("creationTime"),
                               aCookie->CreationTime());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = params->BindInt32ByName(NS_LITERAL_CSTRING("isSecure"),
                               aCookie->IsSecure());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = params->BindInt32ByName(NS_LITERAL_CSTRING("isHttpOnly"),
                               aCookie->IsHttpOnly());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = params->BindInt32ByName(NS_LITERAL_CSTRING("sameSite"),
                               aCookie->SameSite());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = params->BindInt32ByName(NS_LITERAL_CSTRING("rawSameSite"),
                               aCookie->RawSameSite());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Bind the params to the array.
  rv = aParamsArray->AddParams(params);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

}  // namespace

// ---------------------------------------------------------------------------
// CookieEntry

size_t CookieEntry::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = CookieKey::SizeOfExcludingThis(aMallocSizeOf);

  amount += mCookies.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < mCookies.Length(); ++i) {
    amount += mCookies[i]->SizeOfIncludingThis(aMallocSizeOf);
  }

  return amount;
}

// ---------------------------------------------------------------------------
// CookieStorage

NS_IMPL_ISUPPORTS(CookieStorage, nsIObserver, nsISupportsWeakReference)

CookieStorage::CookieStorage()
    : cookieCount(0),
      cookieOldestTime(INT64_MAX),
      mMaxNumberOfCookies(kMaxNumberOfCookies),
      mMaxCookiesPerHost(kMaxCookiesPerHost),
      mCookieQuotaPerHost(kCookieQuotaPerHost),
      mCookiePurgeAge(kCookiePurgeAge) {}

CookieStorage::~CookieStorage() = default;

void CookieStorage::Init() {
  // init our pref and observer
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    prefBranch->AddObserver(kPrefMaxNumberOfCookies, this, true);
    prefBranch->AddObserver(kPrefMaxCookiesPerHost, this, true);
    prefBranch->AddObserver(kPrefCookiePurgeAge, this, true);
    PrefChanged(prefBranch);
  }
}

size_t CookieStorage::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = 0;

  amount += aMallocSizeOf(this);
  amount += hostTable.SizeOfExcludingThis(aMallocSizeOf);

  return amount;
}

void CookieStorage::GetCookies(nsTArray<RefPtr<nsICookie>>& aCookies) const {
  aCookies.SetCapacity(cookieCount);
  for (auto iter = hostTable.ConstIter(); !iter.Done(); iter.Next()) {
    const CookieEntry::ArrayType& cookies = iter.Get()->GetCookies();
    for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
      aCookies.AppendElement(cookies[i]);
    }
  }
}

void CookieStorage::GetSessionCookies(
    nsTArray<RefPtr<nsICookie>>& aCookies) const {
  aCookies.SetCapacity(cookieCount);
  for (auto iter = hostTable.ConstIter(); !iter.Done(); iter.Next()) {
    const CookieEntry::ArrayType& cookies = iter.Get()->GetCookies();
    for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
      Cookie* cookie = cookies[i];
      // Filter out non-session cookies.
      if (cookie->IsSession()) {
        aCookies.AppendElement(cookie);
      }
    }
  }
}

// find an exact cookie specified by host, name, and path that hasn't expired.
bool CookieStorage::FindCookie(const nsACString& aBaseDomain,
                               const OriginAttributes& aOriginAttributes,
                               const nsACString& aHost, const nsACString& aName,
                               const nsACString& aPath, CookieListIter& aIter) {
  CookieEntry* entry =
      hostTable.GetEntry(CookieKey(aBaseDomain, aOriginAttributes));
  if (!entry) {
    return false;
  }

  const CookieEntry::ArrayType& cookies = entry->GetCookies();
  for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    Cookie* cookie = cookies[i];

    if (aHost.Equals(cookie->Host()) && aPath.Equals(cookie->Path()) &&
        aName.Equals(cookie->Name())) {
      aIter = CookieListIter(entry, i);
      return true;
    }
  }

  return false;
}

// find an secure cookie specified by host and name
bool CookieStorage::FindSecureCookie(const nsACString& aBaseDomain,
                                     const OriginAttributes& aOriginAttributes,
                                     Cookie* aCookie) {
  CookieEntry* entry =
      hostTable.GetEntry(CookieKey(aBaseDomain, aOriginAttributes));
  if (!entry) return false;

  const CookieEntry::ArrayType& cookies = entry->GetCookies();
  for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    Cookie* cookie = cookies[i];
    // isn't a match if insecure or a different name
    if (!cookie->IsSecure() || !aCookie->Name().Equals(cookie->Name()))
      continue;

    // The host must "domain-match" an existing cookie or vice-versa
    if (nsCookieService::DomainMatches(cookie, aCookie->Host()) ||
        nsCookieService::DomainMatches(aCookie, cookie->Host())) {
      // If the path of new cookie and the path of existing cookie
      // aren't "/", then this situation needs to compare paths to
      // ensure only that a newly-created non-secure cookie does not
      // overlay an existing secure cookie.
      if (nsCookieService::PathMatches(cookie, aCookie->GetFilePath())) {
        return true;
      }
    }
  }

  return false;
}

uint32_t CookieStorage::CountCookiesFromHost(const nsACString& aBaseDomain,
                                             uint32_t aPrivateBrowsingId) {
  OriginAttributes attrs;
  attrs.mPrivateBrowsingId = aPrivateBrowsingId;

  // Return a count of all cookies, including expired.
  CookieEntry* entry = hostTable.GetEntry(CookieKey(aBaseDomain, attrs));
  return entry ? entry->GetCookies().Length() : 0;
}

void CookieStorage::GetAll(nsTArray<RefPtr<nsICookie>>& aResult) const {
  aResult.SetCapacity(cookieCount);

  for (auto iter = hostTable.ConstIter(); !iter.Done(); iter.Next()) {
    const CookieEntry::ArrayType& cookies = iter.Get()->GetCookies();
    for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
      aResult.AppendElement(cookies[i]);
    }
  }
}

const nsTArray<RefPtr<Cookie>>* CookieStorage::GetCookiesFromHost(
    const nsACString& aBaseDomain, const OriginAttributes& aOriginAttributes) {
  CookieEntry* entry =
      hostTable.GetEntry(CookieKey(aBaseDomain, aOriginAttributes));
  return entry ? &entry->GetCookies() : nullptr;
}

void CookieStorage::GetCookiesWithOriginAttributes(
    const mozilla::OriginAttributesPattern& aPattern,
    const nsACString& aBaseDomain, nsTArray<RefPtr<nsICookie>>& aResult) {
  for (auto iter = hostTable.Iter(); !iter.Done(); iter.Next()) {
    CookieEntry* entry = iter.Get();

    if (!aBaseDomain.IsEmpty() && !aBaseDomain.Equals(entry->mBaseDomain)) {
      continue;
    }

    if (!aPattern.Matches(entry->mOriginAttributes)) {
      continue;
    }

    const CookieEntry::ArrayType& entryCookies = entry->GetCookies();

    for (CookieEntry::IndexType i = 0; i < entryCookies.Length(); ++i) {
      aResult.AppendElement(entryCookies[i]);
    }
  }
}

void CookieStorage::RemoveCookie(const nsACString& aBaseDomain,
                                 const OriginAttributes& aOriginAttributes,
                                 const nsACString& aHost,
                                 const nsACString& aName,
                                 const nsACString& aPath) {
  CookieListIter matchIter;
  RefPtr<Cookie> cookie;
  if (FindCookie(aBaseDomain, aOriginAttributes, aHost, aName, aPath,
                 matchIter)) {
    cookie = matchIter.Cookie();
    RemoveCookieFromList(matchIter);
  }

  if (cookie) {
    // Everything's done. Notify observers.
    NotifyChanged(cookie, u"deleted");
  }
}

void CookieStorage::RemoveCookiesWithOriginAttributes(
    const mozilla::OriginAttributesPattern& aPattern,
    const nsACString& aBaseDomain) {
  // Iterate the hash table of CookieEntry.
  for (auto iter = hostTable.Iter(); !iter.Done(); iter.Next()) {
    CookieEntry* entry = iter.Get();

    if (!aBaseDomain.IsEmpty() && !aBaseDomain.Equals(entry->mBaseDomain)) {
      continue;
    }

    if (!aPattern.Matches(entry->mOriginAttributes)) {
      continue;
    }

    // Pattern matches. Delete all cookies within this CookieEntry.
    uint32_t cookiesCount = entry->GetCookies().Length();

    for (CookieEntry::IndexType i = 0; i < cookiesCount; ++i) {
      // Remove the first cookie from the list.
      CookieListIter iter(entry, 0);
      RefPtr<Cookie> cookie = iter.Cookie();

      // Remove the cookie.
      RemoveCookieFromList(iter);

      if (cookie) {
        NotifyChanged(cookie, u"deleted");
      }
    }
  }
}

void CookieStorage::RemoveCookiesFromExactHost(
    const nsACString& aHost, const nsACString& aBaseDomain,
    const mozilla::OriginAttributesPattern& aPattern) {
  // Iterate the hash table of CookieEntry.
  for (auto iter = hostTable.Iter(); !iter.Done(); iter.Next()) {
    CookieEntry* entry = iter.Get();

    if (!aBaseDomain.Equals(entry->mBaseDomain)) {
      continue;
    }

    if (!aPattern.Matches(entry->mOriginAttributes)) {
      continue;
    }

    uint32_t cookiesCount = entry->GetCookies().Length();
    for (CookieEntry::IndexType i = cookiesCount; i != 0; --i) {
      CookieListIter iter(entry, i - 1);
      RefPtr<Cookie> cookie = iter.Cookie();

      if (!aHost.Equals(cookie->RawHost())) {
        continue;
      }

      // Remove the cookie.
      RemoveCookieFromList(iter);

      if (cookie) {
        NotifyChanged(cookie, u"deleted");
      }
    }
  }
}

void CookieStorage::RemoveAll() {
  // clearing the hashtable will call each CookieEntry's dtor,
  // which releases all their respective children.
  hostTable.Clear();
  cookieCount = 0;
  cookieOldestTime = INT64_MAX;

  RemoveAllInternal();

  NotifyChanged(nullptr, u"cleared");
}

// notify observers that the cookie list changed. there are five possible
// values for aData:
// "deleted" means a cookie was deleted. aSubject is the deleted cookie.
// "added"   means a cookie was added. aSubject is the added cookie.
// "changed" means a cookie was altered. aSubject is the new cookie.
// "cleared" means the entire cookie list was cleared. aSubject is null.
// "batch-deleted" means a set of cookies was purged. aSubject is the list of
// cookies.
void CookieStorage::NotifyChanged(nsISupports* aSubject, const char16_t* aData,
                                  bool aOldCookieIsSession, bool aFromHttp) {
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return;
  }
  // Notify for topic "private-cookie-changed" or "cookie-changed"
  os->NotifyObservers(aSubject, NotificationTopic(), aData);

  NotifyChangedInternal(aSubject, aData, aOldCookieIsSession);
}

// this is a backend function for adding a cookie to the list, via SetCookie.
// also used in the cookie manager, for profile migration from IE.  it either
// replaces an existing cookie; or adds the cookie to the hashtable, and
// deletes a cookie (if maximum number of cookies has been reached). also
// performs list maintenance by removing expired cookies.
void CookieStorage::AddCookie(const nsACString& aBaseDomain,
                              const OriginAttributes& aOriginAttributes,
                              Cookie* aCookie, int64_t aCurrentTimeInUsec,
                              nsIURI* aHostURI, const nsACString& aCookieHeader,
                              bool aFromHttp) {
  int64_t currentTime = aCurrentTimeInUsec / PR_USEC_PER_SEC;

  CookieListIter exactIter;
  bool foundCookie = false;
  foundCookie = FindCookie(aBaseDomain, aOriginAttributes, aCookie->Host(),
                           aCookie->Name(), aCookie->Path(), exactIter);
  bool foundSecureExact = foundCookie && exactIter.Cookie()->IsSecure();
  bool isSecure = true;
  if (aHostURI) {
    isSecure = aHostURI->SchemeIs("https");
  }
  bool oldCookieIsSession = false;
  // Step1, call FindSecureCookie(). FindSecureCookie() would
  // find the existing cookie with the security flag and has
  // the same name, host and path of the new cookie, if there is any.
  // Step2, Confirm new cookie's security setting. If any targeted
  // cookie had been found in Step1, then confirm whether the
  // new cookie could modify it. If the new created cookie’s
  // "secure-only-flag" is not set, and the "scheme" component
  // of the "request-uri" does not denote a "secure" protocol,
  // then ignore the new cookie.
  // (draft-ietf-httpbis-cookie-alone section 3.2)
  if (!aCookie->IsSecure() &&
      (foundSecureExact ||
       FindSecureCookie(aBaseDomain, aOriginAttributes, aCookie)) &&
      !isSecure) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                      "cookie can't save because older cookie is secure "
                      "cookie but newer cookie is non-secure cookie");
    return;
  }

  RefPtr<Cookie> oldCookie;
  nsCOMPtr<nsIArray> purgedList;
  if (foundCookie) {
    oldCookie = exactIter.Cookie();
    oldCookieIsSession = oldCookie->IsSession();

    // Check if the old cookie is stale (i.e. has already expired). If so, we
    // need to be careful about the semantics of removing it and adding the new
    // cookie: we want the behavior wrt adding the new cookie to be the same as
    // if it didn't exist, but we still want to fire a removal notification.
    if (oldCookie->Expiry() <= currentTime) {
      if (aCookie->Expiry() <= currentTime) {
        // The new cookie has expired and the old one is stale. Nothing to do.
        COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                          "cookie has already expired");
        return;
      }

      // Remove the stale cookie. We save notification for later, once all list
      // modifications are complete.
      RemoveCookieFromList(exactIter);
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                        "stale cookie was purged");
      purgedList = CreatePurgeList(oldCookie);

      // We've done all we need to wrt removing and notifying the stale cookie.
      // From here on out, we pretend pretend it didn't exist, so that we
      // preserve expected notification semantics when adding the new cookie.
      foundCookie = false;

    } else {
      // If the old cookie is httponly, make sure we're not coming from script.
      if (!aFromHttp && oldCookie->IsHttpOnly()) {
        COOKIE_LOGFAILURE(
            SET_COOKIE, aHostURI, aCookieHeader,
            "previously stored cookie is httponly; coming from script");
        return;
      }

      // If the new cookie has the same value, expiry date, isSecure, isSession,
      // isHttpOnly and sameSite flags then we can just keep the old one.
      // Only if any of these differ we would want to override the cookie.
      if (oldCookie->Value().Equals(aCookie->Value()) &&
          oldCookie->Expiry() == aCookie->Expiry() &&
          oldCookie->IsSecure() == aCookie->IsSecure() &&
          oldCookie->IsSession() == aCookie->IsSession() &&
          oldCookie->IsHttpOnly() == aCookie->IsHttpOnly() &&
          oldCookie->SameSite() == aCookie->SameSite() &&
          // We don't want to perform this optimization if the cookie is
          // considered stale, since in this case we would need to update the
          // database.
          !oldCookie->IsStale()) {
        // Update the last access time on the old cookie.
        oldCookie->SetLastAccessed(aCookie->LastAccessed());
        UpdateCookieOldestTime(oldCookie);
        return;
      }

      // Remove the old cookie.
      RemoveCookieFromList(exactIter);

      // If the new cookie has expired -- i.e. the intent was simply to delete
      // the old cookie -- then we're done.
      if (aCookie->Expiry() <= currentTime) {
        COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                          "previously stored cookie was deleted");
        NotifyChanged(oldCookie, u"deleted", oldCookieIsSession, aFromHttp);
        return;
      }

      // Preserve creation time of cookie for ordering purposes.
      aCookie->SetCreationTime(oldCookie->CreationTime());
    }

  } else {
    // check if cookie has already expired
    if (aCookie->Expiry() <= currentTime) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                        "cookie has already expired");
      return;
    }

    // check if we have to delete an old cookie.
    CookieEntry* entry =
        hostTable.GetEntry(CookieKey(aBaseDomain, aOriginAttributes));
    if (entry && entry->GetCookies().Length() >= mMaxCookiesPerHost) {
      nsTArray<CookieListIter> removedIterList;
      // Prioritize evicting insecure cookies.
      // (draft-ietf-httpbis-cookie-alone section 3.3)
      uint32_t limit = mMaxCookiesPerHost - mCookieQuotaPerHost;
      FindStaleCookies(entry, currentTime, false, removedIterList, limit);
      if (removedIterList.Length() == 0) {
        if (aCookie->IsSecure()) {
          // It's valid to evict a secure cookie for another secure cookie.
          FindStaleCookies(entry, currentTime, true, removedIterList, limit);
        } else {
          COOKIE_LOGEVICTED(aCookie,
                            "Too many cookies for this domain and the new "
                            "cookie is not a secure cookie");
          return;
        }
      }

      MOZ_ASSERT(!removedIterList.IsEmpty());
      // Sort |removedIterList| by index again, since we have to remove the
      // cookie in the reverse order.
      removedIterList.Sort(CompareCookiesByIndex());
      for (auto it = removedIterList.rbegin(); it != removedIterList.rend();
           it++) {
        RefPtr<Cookie> evictedCookie = (*it).Cookie();
        COOKIE_LOGEVICTED(evictedCookie, "Too many cookies for this domain");
        RemoveCookieFromList(*it);
        CreateOrUpdatePurgeList(getter_AddRefs(purgedList), evictedCookie);
        MOZ_ASSERT((*it).entry);
      }

    } else if (cookieCount >= ADD_TEN_PERCENT(mMaxNumberOfCookies)) {
      int64_t maxAge = aCurrentTimeInUsec - cookieOldestTime;
      int64_t purgeAge = ADD_TEN_PERCENT(mCookiePurgeAge);
      if (maxAge >= purgeAge) {
        // we're over both size and age limits by 10%; time to purge the table!
        // do this by:
        // 1) removing expired cookies;
        // 2) evicting the balance of old cookies until we reach the size limit.
        // note that the cookieOldestTime indicator can be pessimistic - if it's
        // older than the actual oldest cookie, we'll just purge more eagerly.
        purgedList = PurgeCookies(aCurrentTimeInUsec, mMaxNumberOfCookies,
                                  mCookiePurgeAge);
      }
    }
  }

  // Add the cookie to the db. We do not supply a params array for batching
  // because this might result in removals and additions being out of order.
  AddCookieToList(aBaseDomain, aOriginAttributes, aCookie, nullptr);
  COOKIE_LOGSUCCESS(SET_COOKIE, aHostURI, aCookieHeader, aCookie, foundCookie);

  // Now that list mutations are complete, notify observers. We do it here
  // because observers may themselves attempt to mutate the list.
  if (purgedList) {
    NotifyChanged(purgedList, u"batch-deleted");
  }

  NotifyChanged(aCookie, foundCookie ? u"changed" : u"added",
                oldCookieIsSession, aFromHttp);
}

void CookieStorage::UpdateCookieOldestTime(Cookie* aCookie) {
  if (aCookie->LastAccessed() < cookieOldestTime) {
    cookieOldestTime = aCookie->LastAccessed();
  }
}

void CookieStorage::AddCookieToList(const nsACString& aBaseDomain,
                                    const OriginAttributes& aOriginAttributes,
                                    Cookie* aCookie,
                                    mozIStorageBindingParamsArray* aParamsArray,
                                    bool aWriteToDB) {
  if (!aCookie) {
    NS_WARNING("Attempting to AddCookieToList with null cookie");
    return;
  }

  CookieKey key(aBaseDomain, aOriginAttributes);

  CookieEntry* entry = hostTable.PutEntry(key);
  NS_ASSERTION(entry, "can't insert element into a null entry!");

  entry->GetCookies().AppendElement(aCookie);
  ++cookieCount;

  // keep track of the oldest cookie, for when it comes time to purge
  UpdateCookieOldestTime(aCookie);

  // if it's a non-session cookie and hasn't just been read from the db, write
  // it out.
  if (aWriteToDB && !aCookie->IsSession()) {
    WriteCookieToDB(aBaseDomain, aOriginAttributes, aCookie, aParamsArray);
  }
}

already_AddRefed<nsIArray> CookieStorage::CreatePurgeList(nsICookie* aCookie) {
  nsCOMPtr<nsIMutableArray> removedList =
      do_CreateInstance(NS_ARRAY_CONTRACTID);
  removedList->AppendElement(aCookie);
  return removedList.forget();
}

// Given the output iter array and the count limit, find cookies
// sort by expiry and lastAccessed time.
void CookieStorage::FindStaleCookies(CookieEntry* aEntry, int64_t aCurrentTime,
                                     bool aIsSecure,
                                     nsTArray<CookieListIter>& aOutput,
                                     uint32_t aLimit) {
  MOZ_ASSERT(aLimit);

  const CookieEntry::ArrayType& cookies = aEntry->GetCookies();
  aOutput.Clear();

  CookieIterComparator comp(aCurrentTime);
  nsTPriorityQueue<CookieListIter, CookieIterComparator> queue(comp);

  for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    Cookie* cookie = cookies[i];

    if (cookie->Expiry() <= aCurrentTime) {
      queue.Push(CookieListIter(aEntry, i));
      continue;
    }

    if (!aIsSecure) {
      // We want to look for the non-secure cookie first time through,
      // then find the secure cookie the second time this function is called.
      if (cookie->IsSecure()) {
        continue;
      }
    }

    queue.Push(CookieListIter(aEntry, i));
  }

  uint32_t count = 0;
  while (!queue.IsEmpty() && count < aLimit) {
    aOutput.AppendElement(queue.Pop());
    count++;
  }
}

void CookieStorage::CreateOrUpdatePurgeList(nsIArray** aPurgedList,
                                            nsICookie* aCookie) {
  if (!*aPurgedList) {
    COOKIE_LOGSTRING(LogLevel::Debug, ("Creating new purge list"));
    nsCOMPtr<nsIArray> purgedList = CreatePurgeList(aCookie);
    purgedList.forget(aPurgedList);
    return;
  }

  nsCOMPtr<nsIMutableArray> purgedList = do_QueryInterface(*aPurgedList);
  if (purgedList) {
    COOKIE_LOGSTRING(LogLevel::Debug, ("Updating existing purge list"));
    purgedList->AppendElement(aCookie);
  } else {
    COOKIE_LOGSTRING(LogLevel::Debug, ("Could not QI aPurgedList!"));
  }
}

// purges expired and old cookies in a batch operation.
already_AddRefed<nsIArray> CookieStorage::PurgeCookies(
    int64_t aCurrentTimeInUsec, uint16_t aMaxNumberOfCookies,
    int64_t aCookiePurgeAge) {
  NS_ASSERTION(hostTable.Count() > 0, "table is empty");

  uint32_t initialCookieCount = cookieCount;
  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("PurgeCookies(): beginning purge with %" PRIu32
                    " cookies and %" PRId64 " oldest age",
                    cookieCount, aCurrentTimeInUsec - cookieOldestTime));

  typedef nsTArray<CookieListIter> PurgeList;
  PurgeList purgeList(kMaxNumberOfCookies);

  nsCOMPtr<nsIMutableArray> removedList =
      do_CreateInstance(NS_ARRAY_CONTRACTID);

  // Create a params array to batch the removals. This is OK here because
  // all the removals are in order, and there are no interleaved additions.
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  MaybeCreateDeleteBindingParamsArray(getter_AddRefs(paramsArray));

  int64_t currentTime = aCurrentTimeInUsec / PR_USEC_PER_SEC;
  int64_t purgeTime = aCurrentTimeInUsec - aCookiePurgeAge;
  int64_t oldestTime = INT64_MAX;

  for (auto iter = hostTable.Iter(); !iter.Done(); iter.Next()) {
    CookieEntry* entry = iter.Get();

    const CookieEntry::ArrayType& cookies = entry->GetCookies();
    auto length = cookies.Length();
    for (CookieEntry::IndexType i = 0; i < length;) {
      CookieListIter iter(entry, i);
      Cookie* cookie = cookies[i];

      // check if the cookie has expired
      if (cookie->Expiry() <= currentTime) {
        removedList->AppendElement(cookie);
        COOKIE_LOGEVICTED(cookie, "Cookie expired");

        // remove from list; do not increment our iterator, but stop if we're
        // done already.
        RemoveCookieFromList(iter, paramsArray);
        if (i == --length) {
          break;
        }
      } else {
        // check if the cookie is over the age limit
        if (cookie->LastAccessed() <= purgeTime) {
          purgeList.AppendElement(iter);

        } else if (cookie->LastAccessed() < oldestTime) {
          // reset our indicator
          oldestTime = cookie->LastAccessed();
        }

        ++i;
      }
      MOZ_ASSERT(length == cookies.Length());
    }
  }

  uint32_t postExpiryCookieCount = cookieCount;

  // now we have a list of iterators for cookies over the age limit.
  // sort them by age, and then we'll see how many to remove...
  purgeList.Sort(CompareCookiesByAge());

  // only remove old cookies until we reach the max cookie limit, no more.
  uint32_t excess =
      cookieCount > aMaxNumberOfCookies ? cookieCount - aMaxNumberOfCookies : 0;
  if (purgeList.Length() > excess) {
    // We're not purging everything in the list, so update our indicator.
    oldestTime = purgeList[excess].Cookie()->LastAccessed();

    purgeList.SetLength(excess);
  }

  // sort the list again, this time grouping cookies with a common entryclass
  // together, and with ascending index. this allows us to iterate backwards
  // over the list removing cookies, without having to adjust indexes as we go.
  purgeList.Sort(CompareCookiesByIndex());
  for (PurgeList::index_type i = purgeList.Length(); i--;) {
    Cookie* cookie = purgeList[i].Cookie();
    removedList->AppendElement(cookie);
    COOKIE_LOGEVICTED(cookie, "Cookie too old");

    RemoveCookieFromList(purgeList[i], paramsArray);
  }

  // Update the database if we have entries to purge.
  DeleteFromDB(paramsArray);

  // reset the oldest time indicator
  cookieOldestTime = oldestTime;

  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("PurgeCookies(): %" PRIu32 " expired; %" PRIu32
                    " purged; %" PRIu32 " remain; %" PRId64 " oldest age",
                    initialCookieCount - postExpiryCookieCount,
                    postExpiryCookieCount - cookieCount, cookieCount,
                    aCurrentTimeInUsec - cookieOldestTime));

  return removedList.forget();
}

// remove a cookie from the hashtable, and update the iterator state.
void CookieStorage::RemoveCookieFromList(
    const CookieListIter& aIter, mozIStorageBindingParamsArray* aParamsArray) {
  RemoveCookieFromListInternal(aIter, aParamsArray);

  if (aIter.entry->GetCookies().Length() == 1) {
    // we're removing the last element in the array - so just remove the entry
    // from the hash. note that the entryclass' dtor will take care of
    // releasing this last element for us!
    hostTable.RawRemoveEntry(aIter.entry);

  } else {
    // just remove the element from the list
    aIter.entry->GetCookies().RemoveElementAt(aIter.index);
  }

  --cookieCount;
}

void CookieStorage::PrefChanged(nsIPrefBranch* aPrefBranch) {
  int32_t val;
  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefMaxNumberOfCookies, &val)))
    mMaxNumberOfCookies = (uint16_t)LIMIT(val, 1, 0xFFFF, kMaxNumberOfCookies);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefCookieQuotaPerHost, &val))) {
    mCookieQuotaPerHost =
        (uint16_t)LIMIT(val, 1, mMaxCookiesPerHost - 1, kCookieQuotaPerHost);
  }

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefMaxCookiesPerHost, &val))) {
    mMaxCookiesPerHost = (uint16_t)LIMIT(val, mCookieQuotaPerHost + 1, 0xFFFF,
                                         kMaxCookiesPerHost);
  }

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefCookiePurgeAge, &val))) {
    mCookiePurgeAge =
        int64_t(LIMIT(val, 0, INT32_MAX, INT32_MAX)) * PR_USEC_PER_SEC;
  }
}

NS_IMETHODIMP
CookieStorage::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) {
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
    if (prefBranch) {
      PrefChanged(prefBranch);
    }
  }

  return NS_OK;
}

// ---------------------------------------------------------------------------
// CookiePrivateStorage

// static
already_AddRefed<CookiePrivateStorage> CookiePrivateStorage::Create() {
  RefPtr<CookiePrivateStorage> storage = new CookiePrivateStorage();
  storage->Init();

  return storage.forget();
}

void CookiePrivateStorage::StaleCookies(const nsTArray<Cookie*>& aCookieList,
                                        int64_t aCurrentTimeInUsec) {
  int32_t count = aCookieList.Length();
  for (int32_t i = 0; i < count; ++i) {
    Cookie* cookie = aCookieList.ElementAt(i);

    if (cookie->IsStale()) {
      cookie->SetLastAccessed(aCurrentTimeInUsec);
    }
  }
}

// ---------------------------------------------------------------------------
// CookieDefaultStorage

// static
already_AddRefed<CookieDefaultStorage> CookieDefaultStorage::Create() {
  RefPtr<CookieDefaultStorage> storage = new CookieDefaultStorage();
  storage->Init();

  return storage.forget();
}

CookieDefaultStorage::CookieDefaultStorage() : corruptFlag(OK) {}

void CookieDefaultStorage::NotifyChangedInternal(nsISupports* aSubject,
                                                 const char16_t* aData,
                                                 bool aOldCookieIsSession) {
  // Notify for topic "session-cookie-changed" to update the copy of session
  // cookies in session restore component.

  // Filter out notifications for individual non-session cookies.
  if (NS_LITERAL_STRING("changed").Equals(aData) ||
      NS_LITERAL_STRING("deleted").Equals(aData) ||
      NS_LITERAL_STRING("added").Equals(aData)) {
    nsCOMPtr<nsICookie> xpcCookie = do_QueryInterface(aSubject);
    MOZ_ASSERT(xpcCookie);
    auto cookie = static_cast<Cookie*>(xpcCookie.get());
    if (!cookie->IsSession() && !aOldCookieIsSession) {
      return;
    }
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(aSubject, "session-cookie-changed", aData);
  }
}

void CookieDefaultStorage::RemoveAllInternal() {
  // clear the cookie file
  if (dbConn) {
    nsCOMPtr<mozIStorageAsyncStatement> stmt;
    nsresult rv = dbConn->CreateAsyncStatement(
        NS_LITERAL_CSTRING("DELETE FROM moz_cookies"), getter_AddRefs(stmt));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(removeListener, getter_AddRefs(handle));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    } else {
      // Recreate the database.
      COOKIE_LOGSTRING(LogLevel::Debug,
                       ("RemoveAll(): corruption detected with rv 0x%" PRIx32,
                        static_cast<uint32_t>(rv)));
      HandleCorruptDB();
    }
  }
}

void CookieDefaultStorage::WriteCookieToDB(
    const nsACString& aBaseDomain, const OriginAttributes& aOriginAttributes,
    mozilla::net::Cookie* aCookie,
    mozIStorageBindingParamsArray* aParamsArray) {
  if (dbConn) {
    mozIStorageAsyncStatement* stmt = stmtInsert;
    nsCOMPtr<mozIStorageBindingParamsArray> paramsArray(aParamsArray);
    if (!paramsArray) {
      stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
    }

    CookieKey key(aBaseDomain, aOriginAttributes);
    BindCookieParameters(paramsArray, key, aCookie);

    // If we were supplied an array to store parameters, we shouldn't call
    // executeAsync - someone up the stack will do this for us.
    if (!aParamsArray) {
      DebugOnly<nsresult> rv = stmt->BindParameters(paramsArray);
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(insertListener, getter_AddRefs(handle));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
}

void CookieDefaultStorage::HandleCorruptDB() {
  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("HandleCorruptDB(): CookieStorage %p has corruptFlag %u",
                    this, corruptFlag));

  // Mark the database corrupt, so the close listener can begin reconstructing
  // it.
  switch (corruptFlag) {
    case OK: {
      // Move to 'closing' state.
      corruptFlag = CLOSING_FOR_REBUILD;

      CleanupCachedStatements();
      dbConn->AsyncClose(closeListener);
      CleanupDefaultDBConnection();
      break;
    }
    case CLOSING_FOR_REBUILD: {
      // We had an error while waiting for close completion. That's OK, just
      // ignore it -- we're rebuilding anyway.
      return;
    }
    case REBUILDING: {
      // We had an error while rebuilding the DB. Game over. Close the database
      // and let the close handler do nothing; then we'll move it out of the
      // way.
      CleanupCachedStatements();
      if (dbConn) {
        dbConn->AsyncClose(closeListener);
      }
      CleanupDefaultDBConnection();
      break;
    }
  }
}

void CookieDefaultStorage::RemoveCookiesWithOriginAttributes(
    const mozilla::OriginAttributesPattern& aPattern,
    const nsACString& aBaseDomain) {
  mozStorageTransaction transaction(dbConn, false);

  CookieStorage::RemoveCookiesWithOriginAttributes(aPattern, aBaseDomain);

  DebugOnly<nsresult> rv = transaction.Commit();
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void CookieDefaultStorage::RemoveCookiesFromExactHost(
    const nsACString& aHost, const nsACString& aBaseDomain,
    const mozilla::OriginAttributesPattern& aPattern) {
  mozStorageTransaction transaction(dbConn, false);

  CookieStorage::RemoveCookiesFromExactHost(aHost, aBaseDomain, aPattern);

  DebugOnly<nsresult> rv = transaction.Commit();
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void CookieDefaultStorage::RemoveCookieFromListInternal(
    const CookieListIter& aIter, mozIStorageBindingParamsArray* aParamsArray) {
  // if it's a non-session cookie, remove it from the db
  if (!aIter.Cookie()->IsSession() && dbConn) {
    // Use the asynchronous binding methods to ensure that we do not acquire
    // the database lock.
    mozIStorageAsyncStatement* stmt = stmtDelete;
    nsCOMPtr<mozIStorageBindingParamsArray> paramsArray(aParamsArray);
    if (!paramsArray) {
      stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
    }

    nsCOMPtr<mozIStorageBindingParams> params;
    paramsArray->NewBindingParams(getter_AddRefs(params));

    DebugOnly<nsresult> rv = params->BindUTF8StringByName(
        NS_LITERAL_CSTRING("name"), aIter.Cookie()->Name());
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("host"),
                                      aIter.Cookie()->Host());
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("path"),
                                      aIter.Cookie()->Path());
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsAutoCString suffix;
    aIter.Cookie()->OriginAttributesRef().CreateSuffix(suffix);
    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("originAttributes"),
                                      suffix);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = paramsArray->AddParams(params);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // If we weren't given a params array, we'll need to remove it ourselves.
    if (!aParamsArray) {
      rv = stmt->BindParameters(paramsArray);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(removeListener, getter_AddRefs(handle));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
}

// Null out the statements.
// This must be done before closing the connection.
void CookieDefaultStorage::CleanupCachedStatements() {
  stmtInsert = nullptr;
  stmtDelete = nullptr;
  stmtUpdate = nullptr;
}

// Null out the listeners, and the database connection itself. This
// will not null out the statements, cancel a pending read or
// asynchronously close the connection -- these must be done
// beforehand if necessary.
void CookieDefaultStorage::CleanupDefaultDBConnection() {
  MOZ_ASSERT(!stmtInsert, "stmtInsert has been cleaned up");
  MOZ_ASSERT(!stmtDelete, "stmtDelete has been cleaned up");
  MOZ_ASSERT(!stmtUpdate, "stmtUpdate has been cleaned up");

  // Null out the database connections. If 'dbConn' has not been used for any
  // asynchronous operations yet, this will synchronously close it; otherwise,
  // it's expected that the caller has performed an AsyncClose prior.
  dbConn = nullptr;

  // Manually null out our listeners. This is necessary because they hold a
  // strong ref to the CookieStorage itself. They'll stay alive until whatever
  // statements are still executing complete.
  insertListener = nullptr;
  updateListener = nullptr;
  removeListener = nullptr;
  closeListener = nullptr;
}

void CookieDefaultStorage::Close() {
  // Cleanup cached statements before we can close anything.
  CleanupCachedStatements();

  if (dbConn) {
    // Asynchronously close the connection. We will null it below.
    dbConn->AsyncClose(closeListener);
  }

  CleanupDefaultDBConnection();
}

nsresult CookieDefaultStorage::ImportCookies(
    nsIFile* aCookieFile, nsIEffectiveTLDService* aTLDService) {
  MOZ_ASSERT(aCookieFile);
  MOZ_ASSERT(aTLDService);

  nsresult rv;
  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), aCookieFile);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILineInputStream> lineInputStream =
      do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) return rv;

  static const char kTrue[] = "TRUE";

  nsAutoCString buffer, baseDomain;
  bool isMore = true;
  int32_t hostIndex, isDomainIndex, pathIndex, secureIndex, expiresIndex,
      nameIndex, cookieIndex;
  int32_t numInts;
  int64_t expires;
  bool isDomain, isHttpOnly = false;
  uint32_t originalCookieCount = cookieCount;

  int64_t currentTimeInUsec = PR_Now();
  int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;
  // we use lastAccessedCounter to keep cookies in recently-used order,
  // so we start by initializing to currentTime (somewhat arbitrary)
  int64_t lastAccessedCounter = currentTimeInUsec;

  /* file format is:
   *
   * host \t isDomain \t path \t secure \t expires \t name \t cookie
   *
   * if this format isn't respected we move onto the next line in the file.
   * isDomain is "TRUE" or "FALSE" (default to "FALSE")
   * isSecure is "TRUE" or "FALSE" (default to "TRUE")
   * expires is a int64_t integer
   * note 1: cookie can contain tabs.
   * note 2: cookies will be stored in order of lastAccessed time:
   *         most-recently used come first; least-recently-used come last.
   */

  /*
   * ...but due to bug 178933, we hide HttpOnly cookies from older code
   * in a comment, so they don't expose HttpOnly cookies to JS.
   *
   * The format for HttpOnly cookies is
   *
   * #HttpOnly_host \t isDomain \t path \t secure \t expires \t name \t cookie
   *
   */

  // We will likely be adding a bunch of cookies to the DB, so we use async
  // batching with storage to make this super fast.
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  if (originalCookieCount == 0 && dbConn) {
    stmtInsert->NewBindingParamsArray(getter_AddRefs(paramsArray));
  }

  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(buffer, &isMore))) {
    if (StringBeginsWith(buffer, NS_LITERAL_CSTRING(HTTP_ONLY_PREFIX))) {
      isHttpOnly = true;
      hostIndex = sizeof(HTTP_ONLY_PREFIX) - 1;
    } else if (buffer.IsEmpty() || buffer.First() == '#') {
      continue;
    } else {
      isHttpOnly = false;
      hostIndex = 0;
    }

    // this is a cheap, cheesy way of parsing a tab-delimited line into
    // string indexes, which can be lopped off into substrings. just for
    // purposes of obfuscation, it also checks that each token was found.
    // todo: use iterators?
    if ((isDomainIndex = buffer.FindChar('\t', hostIndex) + 1) == 0 ||
        (pathIndex = buffer.FindChar('\t', isDomainIndex) + 1) == 0 ||
        (secureIndex = buffer.FindChar('\t', pathIndex) + 1) == 0 ||
        (expiresIndex = buffer.FindChar('\t', secureIndex) + 1) == 0 ||
        (nameIndex = buffer.FindChar('\t', expiresIndex) + 1) == 0 ||
        (cookieIndex = buffer.FindChar('\t', nameIndex) + 1) == 0) {
      continue;
    }

    // check the expirytime first - if it's expired, ignore
    // nullstomp the trailing tab, to avoid copying the string
    auto iter = buffer.BeginWriting() + nameIndex - 1;
    *iter = char(0);
    numInts = PR_sscanf(buffer.get() + expiresIndex, "%lld", &expires);
    if (numInts != 1 || expires < currentTime) {
      continue;
    }

    isDomain = Substring(buffer, isDomainIndex, pathIndex - isDomainIndex - 1)
                   .EqualsLiteral(kTrue);
    const nsACString& host =
        Substring(buffer, hostIndex, isDomainIndex - hostIndex - 1);
    // check for bad legacy cookies (domain not starting with a dot, or
    // containing a port), and discard
    if ((isDomain && !host.IsEmpty() && host.First() != '.') ||
        host.Contains(':')) {
      continue;
    }

    // compute the baseDomain from the host
    rv = nsCookieService::GetBaseDomainFromHost(aTLDService, host, baseDomain);
    if (NS_FAILED(rv)) continue;

    // pre-existing cookies have inIsolatedMozBrowser=false set by default
    // constructor of OriginAttributes().

    // Create a new Cookie and assign the data. We don't know the cookie
    // creation time, so just use the current time to generate a unique one.
    RefPtr<Cookie> newCookie = Cookie::Create(
        Substring(buffer, nameIndex, cookieIndex - nameIndex - 1),
        Substring(buffer, cookieIndex, buffer.Length() - cookieIndex), host,
        Substring(buffer, pathIndex, secureIndex - pathIndex - 1), expires,
        lastAccessedCounter,
        Cookie::GenerateUniqueCreationTime(currentTimeInUsec), false,
        Substring(buffer, secureIndex, expiresIndex - secureIndex - 1)
            .EqualsLiteral(kTrue),
        isHttpOnly, OriginAttributes(), nsICookie::SAMESITE_NONE,
        nsICookie::SAMESITE_NONE);
    if (!newCookie) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // trick: preserve the most-recently-used cookie ordering,
    // by successively decrementing the lastAccessed time
    lastAccessedCounter--;

    if (originalCookieCount == 0) {
      AddCookieToList(baseDomain, OriginAttributes(), newCookie, paramsArray);
    } else {
      AddCookie(baseDomain, OriginAttributes(), newCookie, currentTimeInUsec,
                nullptr, VoidCString(), true);
    }
  }

  // If we need to write to disk, do so now.
  if (paramsArray) {
    uint32_t length;
    paramsArray->GetLength(&length);
    if (length) {
      rv = stmtInsert->BindParameters(paramsArray);
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmtInsert->ExecuteAsync(insertListener, getter_AddRefs(handle));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  COOKIE_LOGSTRING(
      LogLevel::Debug,
      ("ImportCookies(): %" PRIu32 " cookies imported", cookieCount));

  return NS_OK;
}

void CookieDefaultStorage::StaleCookies(const nsTArray<Cookie*>& aCookieList,
                                        int64_t aCurrentTimeInUsec) {
  // Create an array of parameters to bind to our update statement. Batching
  // is OK here since we're updating cookies with no interleaved operations.
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  mozIStorageAsyncStatement* stmt = stmtUpdate;
  if (dbConn) {
    stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  }

  int32_t count = aCookieList.Length();
  for (int32_t i = 0; i < count; ++i) {
    Cookie* cookie = aCookieList.ElementAt(i);

    if (cookie->IsStale()) {
      UpdateCookieInList(cookie, aCurrentTimeInUsec, paramsArray);
    }
  }
  // Update the database now if necessary.
  if (paramsArray) {
    uint32_t length;
    paramsArray->GetLength(&length);
    if (length) {
      DebugOnly<nsresult> rv = stmt->BindParameters(paramsArray);
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(updateListener, getter_AddRefs(handle));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
}

void CookieDefaultStorage::UpdateCookieInList(
    Cookie* aCookie, int64_t aLastAccessed,
    mozIStorageBindingParamsArray* aParamsArray) {
  MOZ_ASSERT(aCookie);

  // udpate the lastAccessed timestamp
  aCookie->SetLastAccessed(aLastAccessed);

  // if it's a non-session cookie, update it in the db too
  if (!aCookie->IsSession() && aParamsArray) {
    // Create our params holder.
    nsCOMPtr<mozIStorageBindingParams> params;
    aParamsArray->NewBindingParams(getter_AddRefs(params));

    // Bind our parameters.
    DebugOnly<nsresult> rv = params->BindInt64ByName(
        NS_LITERAL_CSTRING("lastAccessed"), aLastAccessed);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("name"),
                                      aCookie->Name());
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("host"),
                                      aCookie->Host());
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("path"),
                                      aCookie->Path());
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsAutoCString suffix;
    aCookie->OriginAttributesRef().CreateSuffix(suffix);
    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("originAttributes"),
                                      suffix);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // Add our bound parameters to the array.
    rv = aParamsArray->AddParams(params);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

void CookieDefaultStorage::MaybeCreateDeleteBindingParamsArray(
    mozIStorageBindingParamsArray** aParamsArray) {
  if (dbConn) {
    stmtDelete->NewBindingParamsArray(aParamsArray);
  }
}

void CookieDefaultStorage::DeleteFromDB(
    mozIStorageBindingParamsArray* aParamsArray) {
  uint32_t length;
  aParamsArray->GetLength(&length);
  if (length) {
    DebugOnly<nsresult> rv = stmtDelete->BindParameters(aParamsArray);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsCOMPtr<mozIStoragePendingStatement> handle;
    rv = stmtDelete->ExecuteAsync(removeListener, getter_AddRefs(handle));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

}  // namespace net
}  // namespace mozilla
