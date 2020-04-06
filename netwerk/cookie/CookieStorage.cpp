/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieCommons.h"
#include "CookieLogging.h"
#include "CookieStorage.h"

#include "mozIStorageError.h"
#include "mozIStorageFunction.h"
#include "mozIStorageService.h"
#include "mozStorageHelper.h"
#include "nsAppDirectoryServiceDefs.h"
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

#define COOKIES_SCHEMA_VERSION 11

// parameter indexes; see |Read|
#define IDX_NAME 0
#define IDX_VALUE 1
#define IDX_HOST 2
#define IDX_PATH 3
#define IDX_EXPIRY 4
#define IDX_LAST_ACCESSED 5
#define IDX_CREATION_TIME 6
#define IDX_SECURE 7
#define IDX_HTTPONLY 8
#define IDX_ORIGIN_ATTRIBUTES 9
#define IDX_SAME_SITE 10
#define IDX_RAW_SAME_SITE 11

#define COOKIES_FILE "cookies.sqlite"
#define OLD_COOKIE_FILE_NAME "cookies.txt"

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

class ConvertAppIdToOriginAttrsSQLFunction final : public mozIStorageFunction {
  ~ConvertAppIdToOriginAttrsSQLFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(ConvertAppIdToOriginAttrsSQLFunction, mozIStorageFunction);

NS_IMETHODIMP
ConvertAppIdToOriginAttrsSQLFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  nsresult rv;
  int32_t inIsolatedMozBrowser;

  rv = aFunctionArguments->GetInt32(1, &inIsolatedMozBrowser);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create an originAttributes object by inIsolatedMozBrowser.
  // Then create the originSuffix string from this object.
  OriginAttributes attrs(inIsolatedMozBrowser ? true : false);
  nsAutoCString suffix;
  attrs.CreateSuffix(suffix);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsAUTF8String(suffix);
  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

class SetAppIdFromOriginAttributesSQLFunction final
    : public mozIStorageFunction {
  ~SetAppIdFromOriginAttributesSQLFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(SetAppIdFromOriginAttributesSQLFunction, mozIStorageFunction);

NS_IMETHODIMP
SetAppIdFromOriginAttributesSQLFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  nsresult rv;
  nsAutoCString suffix;
  OriginAttributes attrs;

  rv = aFunctionArguments->GetUTF8String(0, suffix);
  NS_ENSURE_SUCCESS(rv, rv);
  bool success = attrs.PopulateFromSuffix(suffix);
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsInt32(0);  // deprecated appId!
  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

class SetInBrowserFromOriginAttributesSQLFunction final
    : public mozIStorageFunction {
  ~SetInBrowserFromOriginAttributesSQLFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(SetInBrowserFromOriginAttributesSQLFunction,
                  mozIStorageFunction);

NS_IMETHODIMP
SetInBrowserFromOriginAttributesSQLFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  nsresult rv;
  nsAutoCString suffix;
  OriginAttributes attrs;

  rv = aFunctionArguments->GetUTF8String(0, suffix);
  NS_ENSURE_SUCCESS(rv, rv);
  bool success = attrs.PopulateFromSuffix(suffix);
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsInt32(attrs.mInIsolatedMozBrowser);
  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

/******************************************************************************
 * DBListenerErrorHandler impl:
 * Parent class for our async storage listeners that handles the logging of
 * errors.
 ******************************************************************************/
class DBListenerErrorHandler : public mozIStorageStatementCallback {
 protected:
  explicit DBListenerErrorHandler(CookieDefaultStorage* dbState)
      : mStorage(dbState) {}
  RefPtr<CookieDefaultStorage> mStorage;
  virtual const char* GetOpType() = 0;

 public:
  NS_IMETHOD HandleError(mozIStorageError* aError) override {
    if (MOZ_LOG_TEST(gCookieLog, LogLevel::Warning)) {
      int32_t result = -1;
      aError->GetResult(&result);

      nsAutoCString message;
      aError->GetMessage(message);
      COOKIE_LOGSTRING(
          LogLevel::Warning,
          ("DBListenerErrorHandler::HandleError(): Error %d occurred while "
           "performing operation '%s' with message '%s'; rebuilding database.",
           result, GetOpType(), message.get()));
    }

    // Rebuild the database.
    mStorage->HandleCorruptDB();

    return NS_OK;
  }
};

/******************************************************************************
 * InsertCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous insertion operations.
 ******************************************************************************/
class InsertCookieDBListener final : public DBListenerErrorHandler {
 private:
  const char* GetOpType() override { return "INSERT"; }

  ~InsertCookieDBListener() = default;

 public:
  NS_DECL_ISUPPORTS

  explicit InsertCookieDBListener(CookieDefaultStorage* dbState)
      : DBListenerErrorHandler(dbState) {}
  NS_IMETHOD HandleResult(mozIStorageResultSet*) override {
    MOZ_ASSERT_UNREACHABLE(
        "Unexpected call to "
        "InsertCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(uint16_t aReason) override {
    // If we were rebuilding the db and we succeeded, make our mCorruptFlag say
    // so.
    if (mStorage->GetCorruptFlag() == CookieDefaultStorage::REBUILDING &&
        aReason == mozIStorageStatementCallback::REASON_FINISHED) {
      COOKIE_LOGSTRING(
          LogLevel::Debug,
          ("InsertCookieDBListener::HandleCompletion(): rebuild complete"));
      mStorage->SetCorruptFlag(CookieDefaultStorage::OK);
    }

    // This notification is just for testing.
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(nullptr, "cookie-saved-on-disk", nullptr);
    }

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(InsertCookieDBListener, mozIStorageStatementCallback)

/******************************************************************************
 * UpdateCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous update operations.
 ******************************************************************************/
class UpdateCookieDBListener final : public DBListenerErrorHandler {
 private:
  const char* GetOpType() override { return "UPDATE"; }

  ~UpdateCookieDBListener() = default;

 public:
  NS_DECL_ISUPPORTS

  explicit UpdateCookieDBListener(CookieDefaultStorage* dbState)
      : DBListenerErrorHandler(dbState) {}
  NS_IMETHOD HandleResult(mozIStorageResultSet*) override {
    MOZ_ASSERT_UNREACHABLE(
        "Unexpected call to "
        "UpdateCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(uint16_t aReason) override { return NS_OK; }
};

NS_IMPL_ISUPPORTS(UpdateCookieDBListener, mozIStorageStatementCallback)

/******************************************************************************
 * RemoveCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous removal operations.
 ******************************************************************************/
class RemoveCookieDBListener final : public DBListenerErrorHandler {
 private:
  const char* GetOpType() override { return "REMOVE"; }

  ~RemoveCookieDBListener() = default;

 public:
  NS_DECL_ISUPPORTS

  explicit RemoveCookieDBListener(CookieDefaultStorage* dbState)
      : DBListenerErrorHandler(dbState) {}
  NS_IMETHOD HandleResult(mozIStorageResultSet*) override {
    MOZ_ASSERT_UNREACHABLE(
        "Unexpected call to "
        "RemoveCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(uint16_t aReason) override { return NS_OK; }
};

NS_IMPL_ISUPPORTS(RemoveCookieDBListener, mozIStorageStatementCallback)

/******************************************************************************
 * CloseCookieDBListener imp:
 * Static mozIStorageCompletionCallback used to notify when the database is
 * successfully closed.
 ******************************************************************************/
class CloseCookieDBListener final : public mozIStorageCompletionCallback {
  ~CloseCookieDBListener() = default;

 public:
  explicit CloseCookieDBListener(CookieDefaultStorage* dbState)
      : mStorage(dbState) {}
  RefPtr<CookieDefaultStorage> mStorage;
  NS_DECL_ISUPPORTS

  NS_IMETHOD Complete(nsresult, nsISupports*) override {
    mStorage->HandleDBClosed();
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(CloseCookieDBListener, mozIStorageCompletionCallback)

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

CookieDefaultStorage::CookieDefaultStorage()
    : mMonitor("CookieDefaultStorage"), mInitialized(false), mCorruptFlag(OK) {}

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
  if (mDBConn) {
    nsCOMPtr<mozIStorageAsyncStatement> stmt;
    nsresult rv = mDBConn->CreateAsyncStatement(
        NS_LITERAL_CSTRING("DELETE FROM moz_cookies"), getter_AddRefs(stmt));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(mRemoveListener, getter_AddRefs(handle));
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
  if (mDBConn) {
    mozIStorageAsyncStatement* stmt = mStmtInsert;
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
      rv = stmt->ExecuteAsync(mInsertListener, getter_AddRefs(handle));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
}

void CookieDefaultStorage::HandleCorruptDB() {
  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("HandleCorruptDB(): CookieStorage %p has mCorruptFlag %u",
                    this, mCorruptFlag));

  // Mark the database corrupt, so the close listener can begin reconstructing
  // it.
  switch (mCorruptFlag) {
    case OK: {
      // Move to 'closing' state.
      mCorruptFlag = CLOSING_FOR_REBUILD;

      CleanupCachedStatements();
      mDBConn->AsyncClose(mCloseListener);
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
      if (mDBConn) {
        mDBConn->AsyncClose(mCloseListener);
      }
      CleanupDefaultDBConnection();
      break;
    }
  }
}

void CookieDefaultStorage::RemoveCookiesWithOriginAttributes(
    const mozilla::OriginAttributesPattern& aPattern,
    const nsACString& aBaseDomain) {
  mozStorageTransaction transaction(mDBConn, false);

  CookieStorage::RemoveCookiesWithOriginAttributes(aPattern, aBaseDomain);

  DebugOnly<nsresult> rv = transaction.Commit();
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void CookieDefaultStorage::RemoveCookiesFromExactHost(
    const nsACString& aHost, const nsACString& aBaseDomain,
    const mozilla::OriginAttributesPattern& aPattern) {
  mozStorageTransaction transaction(mDBConn, false);

  CookieStorage::RemoveCookiesFromExactHost(aHost, aBaseDomain, aPattern);

  DebugOnly<nsresult> rv = transaction.Commit();
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void CookieDefaultStorage::RemoveCookieFromListInternal(
    const CookieListIter& aIter, mozIStorageBindingParamsArray* aParamsArray) {
  // if it's a non-session cookie, remove it from the db
  if (!aIter.Cookie()->IsSession() && mDBConn) {
    // Use the asynchronous binding methods to ensure that we do not acquire
    // the database lock.
    mozIStorageAsyncStatement* stmt = mStmtDelete;
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
      rv = stmt->ExecuteAsync(mRemoveListener, getter_AddRefs(handle));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
}

// Null out the statements.
// This must be done before closing the connection.
void CookieDefaultStorage::CleanupCachedStatements() {
  mStmtInsert = nullptr;
  mStmtDelete = nullptr;
  mStmtUpdate = nullptr;
}

// Null out the listeners, and the database connection itself. This
// will not null out the statements, cancel a pending read or
// asynchronously close the connection -- these must be done
// beforehand if necessary.
void CookieDefaultStorage::CleanupDefaultDBConnection() {
  MOZ_ASSERT(!mStmtInsert, "mStmtInsert has been cleaned up");
  MOZ_ASSERT(!mStmtDelete, "mStmtDelete has been cleaned up");
  MOZ_ASSERT(!mStmtUpdate, "mStmtUpdate has been cleaned up");

  // Null out the database connections. If 'mDBConn' has not been used for any
  // asynchronous operations yet, this will synchronously close it; otherwise,
  // it's expected that the caller has performed an AsyncClose prior.
  mDBConn = nullptr;

  // Manually null out our listeners. This is necessary because they hold a
  // strong ref to the CookieStorage itself. They'll stay alive until whatever
  // statements are still executing complete.
  mInsertListener = nullptr;
  mUpdateListener = nullptr;
  mRemoveListener = nullptr;
  mCloseListener = nullptr;
}

void CookieDefaultStorage::Close() {
  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }

  // Cleanup cached statements before we can close anything.
  CleanupCachedStatements();

  if (mDBConn) {
    // Asynchronously close the connection. We will null it below.
    mDBConn->AsyncClose(mCloseListener);
  }

  CleanupDefaultDBConnection();

  mInitialized = false;
  mInitializedDBConn = false;
}

nsresult CookieDefaultStorage::ImportCookies(nsIFile* aCookieFile) {
  MOZ_ASSERT(aCookieFile);

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
  uint32_t originalCookieCount = mCookieCount;

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
  if (originalCookieCount == 0 && mDBConn) {
    mStmtInsert->NewBindingParamsArray(getter_AddRefs(paramsArray));
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
    rv = nsCookieService::GetBaseDomainFromHost(mTLDService, host, baseDomain);
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
      rv = mStmtInsert->BindParameters(paramsArray);
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = mStmtInsert->ExecuteAsync(mInsertListener, getter_AddRefs(handle));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  COOKIE_LOGSTRING(
      LogLevel::Debug,
      ("ImportCookies(): %" PRIu32 " cookies imported", mCookieCount));

  return NS_OK;
}

void CookieDefaultStorage::StaleCookies(const nsTArray<Cookie*>& aCookieList,
                                        int64_t aCurrentTimeInUsec) {
  // Create an array of parameters to bind to our update statement. Batching
  // is OK here since we're updating cookies with no interleaved operations.
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  mozIStorageAsyncStatement* stmt = mStmtUpdate;
  if (mDBConn) {
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
      rv = stmt->ExecuteAsync(mUpdateListener, getter_AddRefs(handle));
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
  if (mDBConn) {
    mStmtDelete->NewBindingParamsArray(aParamsArray);
  }
}

void CookieDefaultStorage::DeleteFromDB(
    mozIStorageBindingParamsArray* aParamsArray) {
  uint32_t length;
  aParamsArray->GetLength(&length);
  if (length) {
    DebugOnly<nsresult> rv = mStmtDelete->BindParameters(aParamsArray);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsCOMPtr<mozIStoragePendingStatement> handle;
    rv = mStmtDelete->ExecuteAsync(mRemoveListener, getter_AddRefs(handle));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

void CookieDefaultStorage::Activate() {
  MOZ_ASSERT(!mThread, "already have a cookie thread");

  mStorageService = do_GetService("@mozilla.org/storage/service;1");
  MOZ_ASSERT(mStorageService);

  mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  MOZ_ASSERT(mTLDService);

  // Get our cookie file.
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(mCookieFile));
  if (NS_FAILED(rv)) {
    // We've already set up our CookieStorages appropriately; nothing more to
    // do.
    COOKIE_LOGSTRING(LogLevel::Warning,
                     ("InitCookieStorages(): couldn't get cookie file"));

    mInitializedDBConn = true;
    mInitialized = true;
    return;
  }

  mCookieFile->AppendNative(NS_LITERAL_CSTRING(COOKIES_FILE));

  NS_ENSURE_SUCCESS_VOID(NS_NewNamedThread("Cookie", getter_AddRefs(mThread)));

  RefPtr<CookieDefaultStorage> self = this;
  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("CookieDefaultStorage::Activate", [self] {
        MonitorAutoLock lock(self->mMonitor);

        // Attempt to open and read the database. If TryInitDB() returns
        // RESULT_RETRY, do so.
        OpenDBResult result = self->TryInitDB(false);
        if (result == RESULT_RETRY) {
          // Database may be corrupt. Synchronously close the connection, clean
          // up the default CookieStorage, and try again.
          COOKIE_LOGSTRING(LogLevel::Warning,
                           ("InitCookieStorages(): retrying TryInitDB()"));
          self->CleanupCachedStatements();
          self->CleanupDefaultDBConnection();
          result = self->TryInitDB(true);
          if (result == RESULT_RETRY) {
            // We're done. Change the code to failure so we clean up below.
            result = RESULT_FAILURE;
          }
        }

        if (result == RESULT_FAILURE) {
          COOKIE_LOGSTRING(
              LogLevel::Warning,
              ("InitCookieStorages(): TryInitDB() failed, closing connection"));

          // Connection failure is unrecoverable. Clean up our connection. We
          // can run fine without persistent storage -- e.g. if there's no
          // profile.
          self->CleanupCachedStatements();
          self->CleanupDefaultDBConnection();

          // No need to initialize mDBConn
          self->mInitializedDBConn = true;
        }

        self->mInitialized = true;

        NS_DispatchToMainThread(
            NS_NewRunnableFunction("CookieDefaultStorage::InitDBConn",
                                   [self] { self->InitDBConn(); }));
        self->mMonitor.Notify();
      });

  mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

/* Attempt to open and read the database. If 'aRecreateDB' is true, try to
 * move the existing database file out of the way and create a new one.
 *
 * @returns RESULT_OK if opening or creating the database succeeded;
 *          RESULT_RETRY if the database cannot be opened, is corrupt, or some
 *          other failure occurred that might be resolved by recreating the
 *          database; or RESULT_FAILED if there was an unrecoverable error and
 *          we must run without a database.
 *
 * If RESULT_RETRY or RESULT_FAILED is returned, the caller should perform
 * cleanup of the default CookieStorage.
 */
CookieDefaultStorage::OpenDBResult CookieDefaultStorage::TryInitDB(
    bool aRecreateDB) {
  NS_ASSERTION(!mDBConn, "nonnull mDBConn");
  NS_ASSERTION(!mStmtInsert, "nonnull mStmtInsert");
  NS_ASSERTION(!mInsertListener, "nonnull mInsertListener");
  NS_ASSERTION(!mSyncConn, "nonnull mSyncConn");
  NS_ASSERTION(NS_GetCurrentThread() == mThread, "non cookie thread");

  // Ditch an existing db, if we've been told to (i.e. it's corrupt). We don't
  // want to delete it outright, since it may be useful for debugging purposes,
  // so we move it out of the way.
  nsresult rv;
  if (aRecreateDB) {
    nsCOMPtr<nsIFile> backupFile;
    mCookieFile->Clone(getter_AddRefs(backupFile));
    rv = backupFile->MoveToNative(nullptr,
                                  NS_LITERAL_CSTRING(COOKIES_FILE ".bak"));
    NS_ENSURE_SUCCESS(rv, RESULT_FAILURE);
  }

  // This block provides scope for the Telemetry AutoTimer
  {
    Telemetry::AutoTimer<Telemetry::MOZ_SQLITE_COOKIES_OPEN_READAHEAD_MS>
        telemetry;
    ReadAheadFile(mCookieFile);

    // open a connection to the cookie database, and only cache our connection
    // and statements upon success. The connection is opened unshared to
    // eliminate cache contention between the main and background threads.
    rv = mStorageService->OpenUnsharedDatabase(mCookieFile,
                                               getter_AddRefs(mSyncConn));
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
  }

  auto guard = MakeScopeExit([&] { mSyncConn = nullptr; });

  bool tableExists = false;
  mSyncConn->TableExists(NS_LITERAL_CSTRING("moz_cookies"), &tableExists);
  if (!tableExists) {
    rv = CreateTable();
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  } else {
    // table already exists; check the schema version before reading
    int32_t dbSchemaVersion;
    rv = mSyncConn->GetSchemaVersion(&dbSchemaVersion);
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

    // Start a transaction for the whole migration block.
    mozStorageTransaction transaction(mSyncConn, true);

    switch (dbSchemaVersion) {
      // Upgrading.
      // Every time you increment the database schema, you need to implement
      // the upgrading code from the previous version to the new one. If
      // migration fails for any reason, it's a bug -- so we return RESULT_RETRY
      // such that the original database will be saved, in the hopes that we
      // might one day see it and fix it.
      case 1: {
        // Add the lastAccessed column to the table.
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies ADD lastAccessed INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
        // Fall through to the next upgrade.
        [[fallthrough]];

      case 2: {
        // Add the baseDomain column and index to the table.
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("ALTER TABLE moz_cookies ADD baseDomain TEXT"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Compute the baseDomains for the table. This must be done eagerly
        // otherwise we won't be able to synchronously read in individual
        // domains on demand.
        const int64_t SCHEMA2_IDX_ID = 0;
        const int64_t SCHEMA2_IDX_HOST = 1;
        nsCOMPtr<mozIStorageStatement> select;
        rv = mSyncConn->CreateStatement(
            NS_LITERAL_CSTRING("SELECT id, host FROM moz_cookies"),
            getter_AddRefs(select));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageStatement> update;
        rv = mSyncConn->CreateStatement(
            NS_LITERAL_CSTRING("UPDATE moz_cookies SET baseDomain = "
                               ":baseDomain WHERE id = :id"),
            getter_AddRefs(update));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCString baseDomain, host;
        bool hasResult;
        while (true) {
          rv = select->ExecuteStep(&hasResult);
          NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

          if (!hasResult) break;

          int64_t id = select->AsInt64(SCHEMA2_IDX_ID);
          select->GetUTF8String(SCHEMA2_IDX_HOST, host);

          rv = nsCookieService::GetBaseDomainFromHost(mTLDService, host,
                                                      baseDomain);
          NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

          mozStorageStatementScoper scoper(update);

          rv = update->BindUTF8StringByName(NS_LITERAL_CSTRING("baseDomain"),
                                            baseDomain);
          MOZ_ASSERT(NS_SUCCEEDED(rv));
          rv = update->BindInt64ByName(NS_LITERAL_CSTRING("id"), id);
          MOZ_ASSERT(NS_SUCCEEDED(rv));

          rv = update->ExecuteStep(&hasResult);
          NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
        }

        // Create an index on baseDomain.
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
        // Fall through to the next upgrade.
        [[fallthrough]];

      case 3: {
        // Add the creationTime column to the table, and create a unique index
        // on (name, host, path). Before we do this, we have to purge the table
        // of expired cookies such that we know that the (name, host, path)
        // index is truly unique -- otherwise we can't create the index. Note
        // that we can't just execute a statement to delete all rows where the
        // expiry column is in the past -- doing so would rely on the clock
        // (both now and when previous cookies were set) being monotonic.

        // Select the whole table, and order by the fields we're interested in.
        // This means we can simply do a linear traversal of the results and
        // check for duplicates as we go.
        const int64_t SCHEMA3_IDX_ID = 0;
        const int64_t SCHEMA3_IDX_NAME = 1;
        const int64_t SCHEMA3_IDX_HOST = 2;
        const int64_t SCHEMA3_IDX_PATH = 3;
        nsCOMPtr<mozIStorageStatement> select;
        rv = mSyncConn->CreateStatement(
            NS_LITERAL_CSTRING(
                "SELECT id, name, host, path FROM moz_cookies "
                "ORDER BY name ASC, host ASC, path ASC, expiry ASC"),
            getter_AddRefs(select));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageStatement> deleteExpired;
        rv = mSyncConn->CreateStatement(
            NS_LITERAL_CSTRING("DELETE FROM moz_cookies WHERE id = :id"),
            getter_AddRefs(deleteExpired));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Read the first row.
        bool hasResult;
        rv = select->ExecuteStep(&hasResult);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        if (hasResult) {
          nsCString name1, host1, path1;
          int64_t id1 = select->AsInt64(SCHEMA3_IDX_ID);
          select->GetUTF8String(SCHEMA3_IDX_NAME, name1);
          select->GetUTF8String(SCHEMA3_IDX_HOST, host1);
          select->GetUTF8String(SCHEMA3_IDX_PATH, path1);

          nsCString name2, host2, path2;
          while (true) {
            // Read the second row.
            rv = select->ExecuteStep(&hasResult);
            NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

            if (!hasResult) break;

            int64_t id2 = select->AsInt64(SCHEMA3_IDX_ID);
            select->GetUTF8String(SCHEMA3_IDX_NAME, name2);
            select->GetUTF8String(SCHEMA3_IDX_HOST, host2);
            select->GetUTF8String(SCHEMA3_IDX_PATH, path2);

            // If the two rows match in (name, host, path), we know the earlier
            // row has an earlier expiry time. Delete it.
            if (name1 == name2 && host1 == host2 && path1 == path2) {
              mozStorageStatementScoper scoper(deleteExpired);

              rv =
                  deleteExpired->BindInt64ByName(NS_LITERAL_CSTRING("id"), id1);
              MOZ_ASSERT(NS_SUCCEEDED(rv));

              rv = deleteExpired->ExecuteStep(&hasResult);
              NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
            }

            // Make the second row the first for the next iteration.
            name1 = name2;
            host1 = host2;
            path1 = path2;
            id1 = id2;
          }
        }

        // Add the creationTime column to the table.
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies ADD creationTime INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy the id of each row into the new creationTime column.
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("UPDATE moz_cookies SET creationTime = "
                               "(SELECT id WHERE id = moz_cookies.id)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create a unique index on (name, host, path) to allow fast lookup.
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("CREATE UNIQUE INDEX moz_uniqueid "
                               "ON moz_cookies (name, host, path)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
        // Fall through to the next upgrade.
        [[fallthrough]];

      case 4: {
        // We need to add appId/inBrowserElement, plus change a constraint on
        // the table (unique entries now include appId/inBrowserElement):
        // this requires creating a new table and copying the data to it.  We
        // then rename the new table to the old name.
        //
        // Why we made this change: appId/inBrowserElement allow "cookie jars"
        // for Firefox OS. We create a separate cookie namespace per {appId,
        // inBrowserElement}.  When upgrading, we convert existing cookies
        // (which imply we're on desktop/mobile) to use {0, false}, as that is
        // the only namespace used by a non-Firefox-OS implementation.

        // Rename existing table
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies RENAME TO moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop existing index (CreateTable will create new one for new table)
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP INDEX moz_basedomain"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create new table (with new fields and new unique constraint)
        rv = CreateTableForSchemaVersion5();
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy data from old table, using appId/inBrowser=0 for existing rows
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "INSERT INTO moz_cookies "
            "(baseDomain, appId, inBrowserElement, name, value, host, path, "
            "expiry,"
            " lastAccessed, creationTime, isSecure, isHttpOnly) "
            "SELECT baseDomain, 0, 0, name, value, host, path, expiry,"
            " lastAccessed, creationTime, isSecure, isHttpOnly "
            "FROM moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop old table
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP TABLE moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 5"));
      }
        // Fall through to the next upgrade.
        [[fallthrough]];

      case 5: {
        // Change in the version: Replace the columns |appId| and
        // |inBrowserElement| by a single column |originAttributes|.
        //
        // Why we made this change: FxOS new security model (NSec) encapsulates
        // "appId/inIsolatedMozBrowser" in nsIPrincipal::originAttributes to
        // make it easier to modify the contents of this structure in the
        // future.
        //
        // We do the migration in several steps:
        // 1. Rename the old table.
        // 2. Create a new table.
        // 3. Copy data from the old table to the new table; convert appId and
        //    inBrowserElement to originAttributes in the meantime.

        // Rename existing table.
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies RENAME TO moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop existing index (CreateTable will create new one for new table).
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP INDEX moz_basedomain"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create new table with new fields and new unique constraint.
        rv = CreateTableForSchemaVersion6();
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy data from old table without the two deprecated columns appId and
        // inBrowserElement.
        nsCOMPtr<mozIStorageFunction> convertToOriginAttrs(
            new ConvertAppIdToOriginAttrsSQLFunction());
        NS_ENSURE_TRUE(convertToOriginAttrs, RESULT_RETRY);

        NS_NAMED_LITERAL_CSTRING(convertToOriginAttrsName,
                                 "CONVERT_TO_ORIGIN_ATTRIBUTES");

        rv = mSyncConn->CreateFunction(convertToOriginAttrsName, 2,
                                       convertToOriginAttrs);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "INSERT INTO moz_cookies "
            "(baseDomain, originAttributes, name, value, host, path, expiry,"
            " lastAccessed, creationTime, isSecure, isHttpOnly) "
            "SELECT baseDomain, "
            " CONVERT_TO_ORIGIN_ATTRIBUTES(appId, inBrowserElement),"
            " name, value, host, path, expiry, lastAccessed, creationTime, "
            " isSecure, isHttpOnly "
            "FROM moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mSyncConn->RemoveFunction(convertToOriginAttrsName);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop old table
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP TABLE moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 6"));
      }
        [[fallthrough]];

      case 6: {
        // We made a mistake in schema version 6. We cannot remove expected
        // columns of any version (checked in the default case) from cookie
        // database, because doing this would destroy the possibility of
        // downgrading database.
        //
        // This version simply restores appId and inBrowserElement columns in
        // order to fix downgrading issue even though these two columns are no
        // longer used in the latest schema.
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies ADD appId INTEGER DEFAULT 0;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies ADD inBrowserElement INTEGER DEFAULT 0;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Compute and populate the values of appId and inBrwoserElement from
        // originAttributes.
        nsCOMPtr<mozIStorageFunction> setAppId(
            new SetAppIdFromOriginAttributesSQLFunction());
        NS_ENSURE_TRUE(setAppId, RESULT_RETRY);

        NS_NAMED_LITERAL_CSTRING(setAppIdName, "SET_APP_ID");

        rv = mSyncConn->CreateFunction(setAppIdName, 1, setAppId);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageFunction> setInBrowser(
            new SetInBrowserFromOriginAttributesSQLFunction());
        NS_ENSURE_TRUE(setInBrowser, RESULT_RETRY);

        NS_NAMED_LITERAL_CSTRING(setInBrowserName, "SET_IN_BROWSER");

        rv = mSyncConn->CreateFunction(setInBrowserName, 1, setInBrowser);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "UPDATE moz_cookies SET appId = SET_APP_ID(originAttributes), "
            "inBrowserElement = SET_IN_BROWSER(originAttributes);"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mSyncConn->RemoveFunction(setAppIdName);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mSyncConn->RemoveFunction(setInBrowserName);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 7"));
      }
        [[fallthrough]];

      case 7: {
        // Remove the appId field from moz_cookies.
        //
        // Unfortunately sqlite doesn't support dropping columns using ALTER
        // TABLE, so we need to go through the procedure documented in
        // https://www.sqlite.org/lang_altertable.html.

        // Drop existing index
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP INDEX moz_basedomain"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create a new_moz_cookies table without the appId field.
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("CREATE TABLE new_moz_cookies("
                               "id INTEGER PRIMARY KEY, "
                               "baseDomain TEXT, "
                               "originAttributes TEXT NOT NULL DEFAULT '', "
                               "name TEXT, "
                               "value TEXT, "
                               "host TEXT, "
                               "path TEXT, "
                               "expiry INTEGER, "
                               "lastAccessed INTEGER, "
                               "creationTime INTEGER, "
                               "isSecure INTEGER, "
                               "isHttpOnly INTEGER, "
                               "inBrowserElement INTEGER DEFAULT 0, "
                               "CONSTRAINT moz_uniqueid UNIQUE (name, host, "
                               "path, originAttributes)"
                               ")"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Move the data over.
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("INSERT INTO new_moz_cookies ("
                               "id, "
                               "baseDomain, "
                               "originAttributes, "
                               "name, "
                               "value, "
                               "host, "
                               "path, "
                               "expiry, "
                               "lastAccessed, "
                               "creationTime, "
                               "isSecure, "
                               "isHttpOnly, "
                               "inBrowserElement "
                               ") SELECT "
                               "id, "
                               "baseDomain, "
                               "originAttributes, "
                               "name, "
                               "value, "
                               "host, "
                               "path, "
                               "expiry, "
                               "lastAccessed, "
                               "creationTime, "
                               "isSecure, "
                               "isHttpOnly, "
                               "inBrowserElement "
                               "FROM moz_cookies;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop the old table
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP TABLE moz_cookies;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Rename new_moz_cookies to moz_cookies.
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE new_moz_cookies RENAME TO moz_cookies;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Recreate our index.
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("CREATE INDEX moz_basedomain ON moz_cookies "
                               "(baseDomain, originAttributes)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 8"));
      }
        [[fallthrough]];

      case 8: {
        // Add the sameSite column to the table.
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("ALTER TABLE moz_cookies ADD sameSite INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 9"));
      }
        [[fallthrough]];

      case 9: {
        // Add the rawSameSite column to the table.
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies ADD rawSameSite INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy the current sameSite value into rawSameSite.
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "UPDATE moz_cookies SET rawSameSite = sameSite"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 10"));
      }
        [[fallthrough]];

      case 10: {
        // Rename existing table
        rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies RENAME TO moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create a new moz_cookies table without the baseDomain field.
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("CREATE TABLE moz_cookies("
                               "id INTEGER PRIMARY KEY, "
                               "originAttributes TEXT NOT NULL DEFAULT '', "
                               "name TEXT, "
                               "value TEXT, "
                               "host TEXT, "
                               "path TEXT, "
                               "expiry INTEGER, "
                               "lastAccessed INTEGER, "
                               "creationTime INTEGER, "
                               "isSecure INTEGER, "
                               "isHttpOnly INTEGER, "
                               "inBrowserElement INTEGER DEFAULT 0, "
                               "sameSite INTEGER DEFAULT 0, "
                               "rawSameSite INTEGER DEFAULT 0, "
                               "CONSTRAINT moz_uniqueid UNIQUE (name, host, "
                               "path, originAttributes)"
                               ")"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Move the data over.
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("INSERT INTO moz_cookies ("
                               "id, "
                               "originAttributes, "
                               "name, "
                               "value, "
                               "host, "
                               "path, "
                               "expiry, "
                               "lastAccessed, "
                               "creationTime, "
                               "isSecure, "
                               "isHttpOnly, "
                               "inBrowserElement, "
                               "sameSite, "
                               "rawSameSite "
                               ") SELECT "
                               "id, "
                               "originAttributes, "
                               "name, "
                               "value, "
                               "host, "
                               "path, "
                               "expiry, "
                               "lastAccessed, "
                               "creationTime, "
                               "isSecure, "
                               "isHttpOnly, "
                               "inBrowserElement, "
                               "sameSite, "
                               "rawSameSite "
                               "FROM moz_cookies_old "
                               "WHERE baseDomain NOTNULL;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop the old table
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP TABLE moz_cookies_old;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop the moz_basedomain index from the database (if it hasn't been
        // removed already by removing the table).
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP INDEX IF EXISTS moz_basedomain;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 11"));

        // No more upgrades. Update the schema version.
        rv = mSyncConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
        [[fallthrough]];

      case COOKIES_SCHEMA_VERSION:
        break;

      case 0: {
        NS_WARNING("couldn't get schema version!");

        // the table may be usable; someone might've just clobbered the schema
        // version. we can treat this case like a downgrade using the codepath
        // below, by verifying the columns we care about are all there. for now,
        // re-set the schema version in the db, in case the checks succeed (if
        // they don't, we're dropping the table anyway).
        rv = mSyncConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
        // fall through to downgrade check
        [[fallthrough]];

      // downgrading.
      // if columns have been added to the table, we can still use the ones we
      // understand safely. if columns have been deleted or altered, just
      // blow away the table and start from scratch! if you change the way
      // a column is interpreted, make sure you also change its name so this
      // check will catch it.
      default: {
        // check if all the expected columns exist
        nsCOMPtr<mozIStorageStatement> stmt;
        rv = mSyncConn->CreateStatement(NS_LITERAL_CSTRING("SELECT "
                                                           "id, "
                                                           "originAttributes, "
                                                           "name, "
                                                           "value, "
                                                           "host, "
                                                           "path, "
                                                           "expiry, "
                                                           "lastAccessed, "
                                                           "creationTime, "
                                                           "isSecure, "
                                                           "isHttpOnly, "
                                                           "sameSite, "
                                                           "rawSameSite "
                                                           "FROM moz_cookies"),
                                        getter_AddRefs(stmt));
        if (NS_SUCCEEDED(rv)) break;

        // our columns aren't there - drop the table!
        rv = mSyncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP TABLE moz_cookies"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = CreateTable();
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      } break;
    }
  }

  // if we deleted a corrupt db, don't attempt to import - return now
  if (aRecreateDB) {
    return RESULT_OK;
  }

  // check whether to import or just read in the db
  if (tableExists) {
    return Read();
  }

  RefPtr<CookieDefaultStorage> self = this;
  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("TryInitDB.ImportCookies", [self] {
        nsCOMPtr<nsIFile> oldCookieFile;
        nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                             getter_AddRefs(oldCookieFile));
        if (NS_FAILED(rv)) {
          return;
        }

        // Import cookies, and clean up the old file regardless of success or
        // failure. Note that we have to switch out our CookieStorage
        // temporarily, in case we're in private browsing mode; otherwise
        // ImportCookies() won't be happy.
        oldCookieFile->AppendNative(NS_LITERAL_CSTRING(OLD_COOKIE_FILE_NAME));
        self->ImportCookies(oldCookieFile);
        oldCookieFile->Remove(false);
      });

  NS_DispatchToMainThread(runnable);

  return RESULT_OK;
}

void CookieDefaultStorage::RebuildCorruptDB() {
  NS_ASSERTION(!mDBConn, "shouldn't have an open db connection");
  NS_ASSERTION(mCorruptFlag == CookieDefaultStorage::CLOSING_FOR_REBUILD,
               "should be in CLOSING_FOR_REBUILD state");

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();

  mCorruptFlag = CookieDefaultStorage::REBUILDING;

  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("RebuildCorruptDB(): creating new database"));

  RefPtr<CookieDefaultStorage> self = this;
  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("RebuildCorruptDB.TryInitDB", [self] {
        // The database has been closed, and we're ready to rebuild. Open a
        // connection.
        OpenDBResult result = self->TryInitDB(true);

        nsCOMPtr<nsIRunnable> innerRunnable = NS_NewRunnableFunction(
            "RebuildCorruptDB.TryInitDBComplete", [self, result] {
              nsCOMPtr<nsIObserverService> os =
                  mozilla::services::GetObserverService();
              if (result != RESULT_OK) {
                // We're done. Reset our DB connection and statements, and
                // notify of closure.
                COOKIE_LOGSTRING(
                    LogLevel::Warning,
                    ("RebuildCorruptDB(): TryInitDB() failed with result %u",
                     result));
                self->CleanupCachedStatements();
                self->CleanupDefaultDBConnection();
                self->mCorruptFlag = CookieDefaultStorage::OK;
                if (os) {
                  os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
                }
                return;
              }

              // Notify observers that we're beginning the rebuild.
              if (os) {
                os->NotifyObservers(nullptr, "cookie-db-rebuilding", nullptr);
              }

              self->InitDBConnInternal();

              // Enumerate the hash, and add cookies to the params array.
              mozIStorageAsyncStatement* stmt = self->mStmtInsert;
              nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
              stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
              for (auto iter = self->mHostTable.Iter(); !iter.Done();
                   iter.Next()) {
                CookieEntry* entry = iter.Get();

                const CookieEntry::ArrayType& cookies = entry->GetCookies();
                for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
                  Cookie* cookie = cookies[i];

                  if (!cookie->IsSession()) {
                    BindCookieParameters(paramsArray, CookieKey(entry), cookie);
                  }
                }
              }

              // Make sure we've got something to write. If we don't, we're
              // done.
              uint32_t length;
              paramsArray->GetLength(&length);
              if (length == 0) {
                COOKIE_LOGSTRING(
                    LogLevel::Debug,
                    ("RebuildCorruptDB(): nothing to write, rebuild complete"));
                self->mCorruptFlag = CookieDefaultStorage::OK;
                return;
              }

              // Execute the statement. If any errors crop up, we won't try
              // again.
              DebugOnly<nsresult> rv = stmt->BindParameters(paramsArray);
              MOZ_ASSERT(NS_SUCCEEDED(rv));
              nsCOMPtr<mozIStoragePendingStatement> handle;
              rv = stmt->ExecuteAsync(self->mInsertListener,
                                      getter_AddRefs(handle));
              MOZ_ASSERT(NS_SUCCEEDED(rv));
            });
        NS_DispatchToMainThread(innerRunnable);
      });
  mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

void CookieDefaultStorage::HandleDBClosed() {
  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("HandleDBClosed(): CookieStorage %p closed", this));

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();

  switch (mCorruptFlag) {
    case CookieDefaultStorage::OK: {
      // Database is healthy. Notify of closure.
      if (os) {
        os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
      }
      break;
    }
    case CookieDefaultStorage::CLOSING_FOR_REBUILD: {
      // Our close finished. Start the rebuild, and notify of db closure later.
      RebuildCorruptDB();
      break;
    }
    case CookieDefaultStorage::REBUILDING: {
      // We encountered an error during rebuild, closed the database, and now
      // here we are. We already have a 'cookies.sqlite.bak' from the original
      // dead database; we don't want to overwrite it, so let's move this one to
      // 'cookies.sqlite.bak-rebuild'.
      nsCOMPtr<nsIFile> backupFile;
      mCookieFile->Clone(getter_AddRefs(backupFile));
      nsresult rv = backupFile->MoveToNative(
          nullptr, NS_LITERAL_CSTRING(COOKIES_FILE ".bak-rebuild"));

      COOKIE_LOGSTRING(LogLevel::Warning,
                       ("HandleDBClosed(): CookieStorage %p encountered error "
                        "rebuilding db; move to "
                        "'cookies.sqlite.bak-rebuild' gave rv 0x%" PRIx32,
                        this, static_cast<uint32_t>(rv)));
      if (os) {
        os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
      }
      break;
    }
  }
}

CookieDefaultStorage::OpenDBResult CookieDefaultStorage::Read() {
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);

  // Read in the data synchronously.
  // see IDX_NAME, etc. for parameter indexes
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv =
      mSyncConn->CreateStatement(NS_LITERAL_CSTRING("SELECT "
                                                    "name, "
                                                    "value, "
                                                    "host, "
                                                    "path, "
                                                    "expiry, "
                                                    "lastAccessed, "
                                                    "creationTime, "
                                                    "isSecure, "
                                                    "isHttpOnly, "
                                                    "originAttributes, "
                                                    "sameSite, "
                                                    "rawSameSite "
                                                    "FROM moz_cookies"),
                                 getter_AddRefs(stmt));

  NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  if (NS_WARN_IF(!mReadArray.IsEmpty())) {
    mReadArray.Clear();
  }
  mReadArray.SetCapacity(kMaxNumberOfCookies);

  nsCString baseDomain, name, value, host, path;
  bool hasResult;
  while (true) {
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mReadArray.Clear();
      return RESULT_RETRY;
    }

    if (!hasResult) break;

    stmt->GetUTF8String(IDX_HOST, host);

    rv = nsCookieService::GetBaseDomainFromHost(mTLDService, host, baseDomain);
    if (NS_FAILED(rv)) {
      COOKIE_LOGSTRING(LogLevel::Debug,
                       ("Read(): Ignoring invalid host '%s'", host.get()));
      continue;
    }

    nsAutoCString suffix;
    OriginAttributes attrs;
    stmt->GetUTF8String(IDX_ORIGIN_ATTRIBUTES, suffix);
    // If PopulateFromSuffix failed we just ignore the OA attributes
    // that we don't support
    Unused << attrs.PopulateFromSuffix(suffix);

    CookieKey key(baseDomain, attrs);
    CookieDomainTuple* tuple = mReadArray.AppendElement();
    tuple->key = std::move(key);
    tuple->originAttributes = attrs;
    tuple->cookie = GetCookieFromRow(stmt);
  }

  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("Read(): %zu cookies read", mReadArray.Length()));

  return RESULT_OK;
}

// Extract data from a single result row and create an Cookie.
mozilla::UniquePtr<CookieStruct> CookieDefaultStorage::GetCookieFromRow(
    mozIStorageStatement* aRow) {
  nsCString name, value, host, path;
  DebugOnly<nsresult> rv = aRow->GetUTF8String(IDX_NAME, name);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  rv = aRow->GetUTF8String(IDX_VALUE, value);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  rv = aRow->GetUTF8String(IDX_HOST, host);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  rv = aRow->GetUTF8String(IDX_PATH, path);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  int64_t expiry = aRow->AsInt64(IDX_EXPIRY);
  int64_t lastAccessed = aRow->AsInt64(IDX_LAST_ACCESSED);
  int64_t creationTime = aRow->AsInt64(IDX_CREATION_TIME);
  bool isSecure = 0 != aRow->AsInt32(IDX_SECURE);
  bool isHttpOnly = 0 != aRow->AsInt32(IDX_HTTPONLY);
  int32_t sameSite = aRow->AsInt32(IDX_SAME_SITE);
  int32_t rawSameSite = aRow->AsInt32(IDX_RAW_SAME_SITE);

  // Create a new constCookie and assign the data.
  return mozilla::MakeUnique<CookieStruct>(
      name, value, host, path, expiry, lastAccessed, creationTime, isHttpOnly,
      false, isSecure, sameSite, rawSameSite);
}

void CookieDefaultStorage::EnsureReadComplete() {
  MOZ_ASSERT(NS_IsMainThread());

  bool isAccumulated = false;

  if (!mInitialized) {
    TimeStamp startBlockTime = TimeStamp::Now();
    MonitorAutoLock lock(mMonitor);

    while (!mInitialized) {
      mMonitor.Wait();
    }

    Telemetry::AccumulateTimeDelta(
        Telemetry::MOZ_SQLITE_COOKIES_BLOCK_MAIN_THREAD_MS_V2, startBlockTime);
    Telemetry::Accumulate(
        Telemetry::MOZ_SQLITE_COOKIES_TIME_TO_BLOCK_MAIN_THREAD_MS, 0);
    isAccumulated = true;
  } else if (!mEndInitDBConn.IsNull()) {
    // We didn't block main thread, and here comes the first cookie request.
    // Collect how close we're going to block main thread.
    Telemetry::Accumulate(
        Telemetry::MOZ_SQLITE_COOKIES_TIME_TO_BLOCK_MAIN_THREAD_MS,
        (TimeStamp::Now() - mEndInitDBConn).ToMilliseconds());
    // Nullify the timestamp so wo don't accumulate this telemetry probe again.
    mEndInitDBConn = TimeStamp();
    isAccumulated = true;
  } else if (!mInitializedDBConn) {
    // A request comes while we finished cookie thread task and InitDBConn is
    // on the way from cookie thread to main thread. We're very close to block
    // main thread.
    Telemetry::Accumulate(
        Telemetry::MOZ_SQLITE_COOKIES_TIME_TO_BLOCK_MAIN_THREAD_MS, 0);
    isAccumulated = true;
  }

  if (!mInitializedDBConn) {
    InitDBConn();
    if (isAccumulated) {
      // Nullify the timestamp so wo don't accumulate this telemetry probe
      // again.
      mEndInitDBConn = TimeStamp();
    }
  }
}

void CookieDefaultStorage::InitDBConn() {
  MOZ_ASSERT(NS_IsMainThread());

  // We should skip InitDBConn if we close profile during initializing
  // CookieStorages and then InitDBConn is called after we close the
  // CookieStorages.
  if (!mInitialized || mInitializedDBConn) {
    return;
  }

  for (uint32_t i = 0; i < mReadArray.Length(); ++i) {
    CookieDomainTuple& tuple = mReadArray[i];
    MOZ_ASSERT(!tuple.cookie->isSession());

    RefPtr<Cookie> cookie = Cookie::Create(
        tuple.cookie->name(), tuple.cookie->value(), tuple.cookie->host(),
        tuple.cookie->path(), tuple.cookie->expiry(),
        tuple.cookie->lastAccessed(), tuple.cookie->creationTime(),
        tuple.cookie->isSession(), tuple.cookie->isSecure(),
        tuple.cookie->isHttpOnly(), tuple.originAttributes,
        tuple.cookie->sameSite(), tuple.cookie->rawSameSite());

    AddCookieToList(tuple.key.mBaseDomain, tuple.key.mOriginAttributes, cookie,
                    nullptr, false);
  }

  if (NS_FAILED(InitDBConnInternal())) {
    COOKIE_LOGSTRING(LogLevel::Warning,
                     ("InitDBConn(): retrying InitDBConnInternal()"));
    CleanupCachedStatements();
    CleanupDefaultDBConnection();
    if (NS_FAILED(InitDBConnInternal())) {
      COOKIE_LOGSTRING(
          LogLevel::Warning,
          ("InitDBConn(): InitDBConnInternal() failed, closing connection"));

      // Game over, clean the connections.
      CleanupCachedStatements();
      CleanupDefaultDBConnection();
    }
  }
  mInitializedDBConn = true;

  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("InitDBConn(): mInitializedDBConn = true"));
  mEndInitDBConn = mozilla::TimeStamp::Now();

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, "cookie-db-read", nullptr);
    mReadArray.Clear();
  }
}

nsresult CookieDefaultStorage::InitDBConnInternal() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = mStorageService->OpenUnsharedDatabase(mCookieFile,
                                                      getter_AddRefs(mDBConn));
  NS_ENSURE_SUCCESS(rv, rv);

  // Set up our listeners.
  mInsertListener = new InsertCookieDBListener(this);
  mUpdateListener = new UpdateCookieDBListener(this);
  mRemoveListener = new RemoveCookieDBListener(this);
  mCloseListener = new CloseCookieDBListener(this);

  // Grow cookie db in 512KB increments
  mDBConn->SetGrowthIncrement(512 * 1024, EmptyCString());

  // make operations on the table asynchronous, for performance
  mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA synchronous = OFF"));

  // Use write-ahead-logging for performance. We cap the autocheckpoint limit at
  // 16 pages (around 500KB).
  mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(MOZ_STORAGE_UNIQUIFY_QUERY_STR
                                               "PRAGMA journal_mode = WAL"));
  mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("PRAGMA wal_autocheckpoint = 16"));

  // cache frequently used statements (for insertion, deletion, and updating)
  rv = mDBConn->CreateAsyncStatement(
      NS_LITERAL_CSTRING("INSERT INTO moz_cookies ("
                         "originAttributes, "
                         "name, "
                         "value, "
                         "host, "
                         "path, "
                         "expiry, "
                         "lastAccessed, "
                         "creationTime, "
                         "isSecure, "
                         "isHttpOnly, "
                         "sameSite, "
                         "rawSameSite "
                         ") VALUES ("
                         ":originAttributes, "
                         ":name, "
                         ":value, "
                         ":host, "
                         ":path, "
                         ":expiry, "
                         ":lastAccessed, "
                         ":creationTime, "
                         ":isSecure, "
                         ":isHttpOnly, "
                         ":sameSite, "
                         ":rawSameSite "
                         ")"),
      getter_AddRefs(mStmtInsert));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateAsyncStatement(
      NS_LITERAL_CSTRING("DELETE FROM moz_cookies "
                         "WHERE name = :name AND host = :host AND path = :path "
                         "AND originAttributes = :originAttributes"),
      getter_AddRefs(mStmtDelete));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateAsyncStatement(
      NS_LITERAL_CSTRING("UPDATE moz_cookies SET lastAccessed = :lastAccessed "
                         "WHERE name = :name AND host = :host AND path = :path "
                         "AND originAttributes = :originAttributes"),
      getter_AddRefs(mStmtUpdate));
  return rv;
}

// Sets the schema version and creates the moz_cookies table.
nsresult CookieDefaultStorage::CreateTableWorker(const char* aName) {
  // Create the table.
  // We default originAttributes to empty string: this is so if users revert to
  // an older Firefox version that doesn't know about this field, any cookies
  // set will still work once they upgrade back.
  nsAutoCString command("CREATE TABLE ");
  command.Append(aName);
  command.AppendLiteral(
      " ("
      "id INTEGER PRIMARY KEY, "
      "originAttributes TEXT NOT NULL DEFAULT '', "
      "name TEXT, "
      "value TEXT, "
      "host TEXT, "
      "path TEXT, "
      "expiry INTEGER, "
      "lastAccessed INTEGER, "
      "creationTime INTEGER, "
      "isSecure INTEGER, "
      "isHttpOnly INTEGER, "
      "inBrowserElement INTEGER DEFAULT 0, "
      "sameSite INTEGER DEFAULT 0, "
      "rawSameSite INTEGER DEFAULT 0, "
      "CONSTRAINT moz_uniqueid UNIQUE (name, host, path, originAttributes)"
      ")");
  return mSyncConn->ExecuteSimpleSQL(command);
}

// Sets the schema version and creates the moz_cookies table.
nsresult CookieDefaultStorage::CreateTable() {
  // Set the schema version, before creating the table.
  nsresult rv = mSyncConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
  if (NS_FAILED(rv)) return rv;

  rv = CreateTableWorker("moz_cookies");
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

// Sets the schema version and creates the moz_cookies table.
nsresult CookieDefaultStorage::CreateTableForSchemaVersion6() {
  // Set the schema version, before creating the table.
  nsresult rv = mSyncConn->SetSchemaVersion(6);
  if (NS_FAILED(rv)) return rv;

  // Create the table.
  // We default originAttributes to empty string: this is so if users revert to
  // an older Firefox version that doesn't know about this field, any cookies
  // set will still work once they upgrade back.
  rv = mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE moz_cookies ("
      "id INTEGER PRIMARY KEY, "
      "baseDomain TEXT, "
      "originAttributes TEXT NOT NULL DEFAULT '', "
      "name TEXT, "
      "value TEXT, "
      "host TEXT, "
      "path TEXT, "
      "expiry INTEGER, "
      "lastAccessed INTEGER, "
      "creationTime INTEGER, "
      "isSecure INTEGER, "
      "isHttpOnly INTEGER, "
      "CONSTRAINT moz_uniqueid UNIQUE (name, host, path, originAttributes)"
      ")"));
  if (NS_FAILED(rv)) return rv;

  // Create an index on baseDomain.
  return mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, "
      "originAttributes)"));
}

// Sets the schema version and creates the moz_cookies table.
nsresult CookieDefaultStorage::CreateTableForSchemaVersion5() {
  // Set the schema version, before creating the table.
  nsresult rv = mSyncConn->SetSchemaVersion(5);
  if (NS_FAILED(rv)) return rv;

  // Create the table. We default appId/inBrowserElement to 0: this is so if
  // users revert to an older Firefox version that doesn't know about these
  // fields, any cookies set will still work once they upgrade back.
  rv = mSyncConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE moz_cookies ("
                         "id INTEGER PRIMARY KEY, "
                         "baseDomain TEXT, "
                         "appId INTEGER DEFAULT 0, "
                         "inBrowserElement INTEGER DEFAULT 0, "
                         "name TEXT, "
                         "value TEXT, "
                         "host TEXT, "
                         "path TEXT, "
                         "expiry INTEGER, "
                         "lastAccessed INTEGER, "
                         "creationTime INTEGER, "
                         "isSecure INTEGER, "
                         "isHttpOnly INTEGER, "
                         "CONSTRAINT moz_uniqueid UNIQUE (name, host, path, "
                         "appId, inBrowserElement)"
                         ")"));
  if (NS_FAILED(rv)) return rv;

  // Create an index on baseDomain.
  return mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, "
      "appId, "
      "inBrowserElement)"));
}

nsresult CookieDefaultStorage::RunInTransaction(
    nsICookieTransactionCallback* aCallback) {
  if (NS_WARN_IF(!mDBConn)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mozStorageTransaction transaction(mDBConn, true);

  if (NS_FAILED(aCallback->Callback())) {
    Unused << transaction.Rollback();
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
