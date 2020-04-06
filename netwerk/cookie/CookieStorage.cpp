/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieCommons.h"
#include "CookieLogging.h"
#include "CookieStorage.h"

#include "nsIMutableArray.h"
#include "nsTPriorityQueue.h"
#include "prprf.h"

#undef ADD_TEN_PERCENT
#define ADD_TEN_PERCENT(i) static_cast<uint32_t>((i) + (i) / 10)

#undef LIMIT
#define LIMIT(x, low, high, default) \
  ((x) >= (low) && (x) <= (high) ? (x) : (default))

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
    : mCookieCount(0),
      mCookieOldestTime(INT64_MAX),
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
  amount += mHostTable.SizeOfExcludingThis(aMallocSizeOf);

  return amount;
}

void CookieStorage::GetCookies(nsTArray<RefPtr<nsICookie>>& aCookies) const {
  aCookies.SetCapacity(mCookieCount);
  for (auto iter = mHostTable.ConstIter(); !iter.Done(); iter.Next()) {
    const CookieEntry::ArrayType& cookies = iter.Get()->GetCookies();
    for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
      aCookies.AppendElement(cookies[i]);
    }
  }
}

void CookieStorage::GetSessionCookies(
    nsTArray<RefPtr<nsICookie>>& aCookies) const {
  aCookies.SetCapacity(mCookieCount);
  for (auto iter = mHostTable.ConstIter(); !iter.Done(); iter.Next()) {
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
      mHostTable.GetEntry(CookieKey(aBaseDomain, aOriginAttributes));
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
      mHostTable.GetEntry(CookieKey(aBaseDomain, aOriginAttributes));
  if (!entry) return false;

  const CookieEntry::ArrayType& cookies = entry->GetCookies();
  for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    Cookie* cookie = cookies[i];
    // isn't a match if insecure or a different name
    if (!cookie->IsSecure() || !aCookie->Name().Equals(cookie->Name()))
      continue;

    // The host must "domain-match" an existing cookie or vice-versa
    if (CookieService::DomainMatches(cookie, aCookie->Host()) ||
        CookieService::DomainMatches(aCookie, cookie->Host())) {
      // If the path of new cookie and the path of existing cookie
      // aren't "/", then this situation needs to compare paths to
      // ensure only that a newly-created non-secure cookie does not
      // overlay an existing secure cookie.
      if (CookieService::PathMatches(cookie, aCookie->GetFilePath())) {
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
  CookieEntry* entry = mHostTable.GetEntry(CookieKey(aBaseDomain, attrs));
  return entry ? entry->GetCookies().Length() : 0;
}

void CookieStorage::GetAll(nsTArray<RefPtr<nsICookie>>& aResult) const {
  aResult.SetCapacity(mCookieCount);

  for (auto iter = mHostTable.ConstIter(); !iter.Done(); iter.Next()) {
    const CookieEntry::ArrayType& cookies = iter.Get()->GetCookies();
    for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
      aResult.AppendElement(cookies[i]);
    }
  }
}

const nsTArray<RefPtr<Cookie>>* CookieStorage::GetCookiesFromHost(
    const nsACString& aBaseDomain, const OriginAttributes& aOriginAttributes) {
  CookieEntry* entry =
      mHostTable.GetEntry(CookieKey(aBaseDomain, aOriginAttributes));
  return entry ? &entry->GetCookies() : nullptr;
}

void CookieStorage::GetCookiesWithOriginAttributes(
    const mozilla::OriginAttributesPattern& aPattern,
    const nsACString& aBaseDomain, nsTArray<RefPtr<nsICookie>>& aResult) {
  for (auto iter = mHostTable.Iter(); !iter.Done(); iter.Next()) {
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
  for (auto iter = mHostTable.Iter(); !iter.Done(); iter.Next()) {
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
  for (auto iter = mHostTable.Iter(); !iter.Done(); iter.Next()) {
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
  mHostTable.Clear();
  mCookieCount = 0;
  mCookieOldestTime = INT64_MAX;

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
        mHostTable.GetEntry(CookieKey(aBaseDomain, aOriginAttributes));
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

    } else if (mCookieCount >= ADD_TEN_PERCENT(mMaxNumberOfCookies)) {
      int64_t maxAge = aCurrentTimeInUsec - mCookieOldestTime;
      int64_t purgeAge = ADD_TEN_PERCENT(mCookiePurgeAge);
      if (maxAge >= purgeAge) {
        // we're over both size and age limits by 10%; time to purge the table!
        // do this by:
        // 1) removing expired cookies;
        // 2) evicting the balance of old cookies until we reach the size limit.
        // note that the mCookieOldestTime indicator can be pessimistic - if
        // it's older than the actual oldest cookie, we'll just purge more
        // eagerly.
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
  if (aCookie->LastAccessed() < mCookieOldestTime) {
    mCookieOldestTime = aCookie->LastAccessed();
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

  CookieEntry* entry = mHostTable.PutEntry(key);
  NS_ASSERTION(entry, "can't insert element into a null entry!");

  entry->GetCookies().AppendElement(aCookie);
  ++mCookieCount;

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
  NS_ASSERTION(mHostTable.Count() > 0, "table is empty");

  uint32_t initialCookieCount = mCookieCount;
  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("PurgeCookies(): beginning purge with %" PRIu32
                    " cookies and %" PRId64 " oldest age",
                    mCookieCount, aCurrentTimeInUsec - mCookieOldestTime));

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

  for (auto iter = mHostTable.Iter(); !iter.Done(); iter.Next()) {
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

  uint32_t postExpiryCookieCount = mCookieCount;

  // now we have a list of iterators for cookies over the age limit.
  // sort them by age, and then we'll see how many to remove...
  purgeList.Sort(CompareCookiesByAge());

  // only remove old cookies until we reach the max cookie limit, no more.
  uint32_t excess = mCookieCount > aMaxNumberOfCookies
                        ? mCookieCount - aMaxNumberOfCookies
                        : 0;
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
  mCookieOldestTime = oldestTime;

  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("PurgeCookies(): %" PRIu32 " expired; %" PRIu32
                    " purged; %" PRIu32 " remain; %" PRId64 " oldest age",
                    initialCookieCount - postExpiryCookieCount,
                    postExpiryCookieCount - mCookieCount, mCookieCount,
                    aCurrentTimeInUsec - mCookieOldestTime));

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
    mHostTable.RawRemoveEntry(aIter.entry);

  } else {
    // just remove the element from the list
    aIter.entry->GetCookies().RemoveElementAt(aIter.index);
  }

  --mCookieCount;
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

}  // namespace net
}  // namespace mozilla
