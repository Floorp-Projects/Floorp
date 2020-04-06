/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Cookie.h"
#include "CookieCommons.h"
#include "CookieLogging.h"
#include "CookiePersistentStorage.h"

#include "mozilla/FileUtils.h"
#include "mozilla/Telemetry.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStorageError.h"
#include "mozIStorageFunction.h"
#include "mozIStorageService.h"
#include "mozStorageHelper.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsICookieService.h"
#include "nsIEffectiveTLDService.h"
#include "nsILineInputStream.h"
#include "nsNetUtil.h"
#include "nsVariant.h"
#include "prprf.h"

// XXX_hack. See bug 178993.
// This is a hack to hide HttpOnly cookies from older browsers
#define HTTP_ONLY_PREFIX "#HttpOnly_"

constexpr auto COOKIES_SCHEMA_VERSION = 11;

// parameter indexes; see |Read|
constexpr auto IDX_NAME = 0;
constexpr auto IDX_VALUE = 1;
constexpr auto IDX_HOST = 2;
constexpr auto IDX_PATH = 3;
constexpr auto IDX_EXPIRY = 4;
constexpr auto IDX_LAST_ACCESSED = 5;
constexpr auto IDX_CREATION_TIME = 6;
constexpr auto IDX_SECURE = 7;
constexpr auto IDX_HTTPONLY = 8;
constexpr auto IDX_ORIGIN_ATTRIBUTES = 9;
constexpr auto IDX_SAME_SITE = 10;
constexpr auto IDX_RAW_SAME_SITE = 11;

#define COOKIES_FILE "cookies.sqlite"
#define OLD_COOKIE_FILE_NAME "cookies.txt"

namespace mozilla {
namespace net {

namespace {

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
  explicit DBListenerErrorHandler(CookiePersistentStorage* dbState)
      : mStorage(dbState) {}
  RefPtr<CookiePersistentStorage> mStorage;
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

  explicit InsertCookieDBListener(CookiePersistentStorage* dbState)
      : DBListenerErrorHandler(dbState) {}
  NS_IMETHOD HandleResult(mozIStorageResultSet* /*aResultSet*/) override {
    MOZ_ASSERT_UNREACHABLE(
        "Unexpected call to "
        "InsertCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(uint16_t aReason) override {
    // If we were rebuilding the db and we succeeded, make our mCorruptFlag say
    // so.
    if (mStorage->GetCorruptFlag() == CookiePersistentStorage::REBUILDING &&
        aReason == mozIStorageStatementCallback::REASON_FINISHED) {
      COOKIE_LOGSTRING(
          LogLevel::Debug,
          ("InsertCookieDBListener::HandleCompletion(): rebuild complete"));
      mStorage->SetCorruptFlag(CookiePersistentStorage::OK);
    }

    // This notification is just for testing.
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
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

  explicit UpdateCookieDBListener(CookiePersistentStorage* dbState)
      : DBListenerErrorHandler(dbState) {}
  NS_IMETHOD HandleResult(mozIStorageResultSet* /*aResultSet*/) override {
    MOZ_ASSERT_UNREACHABLE(
        "Unexpected call to "
        "UpdateCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(uint16_t /*aReason*/) override { return NS_OK; }
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

  explicit RemoveCookieDBListener(CookiePersistentStorage* dbState)
      : DBListenerErrorHandler(dbState) {}
  NS_IMETHOD HandleResult(mozIStorageResultSet* /*aResultSet*/) override {
    MOZ_ASSERT_UNREACHABLE(
        "Unexpected call to "
        "RemoveCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(uint16_t /*aReason*/) override { return NS_OK; }
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
  explicit CloseCookieDBListener(CookiePersistentStorage* dbState)
      : mStorage(dbState) {}
  RefPtr<CookiePersistentStorage> mStorage;
  NS_DECL_ISUPPORTS

  NS_IMETHOD Complete(nsresult /*status*/, nsISupports* /*value*/) override {
    mStorage->HandleDBClosed();
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(CloseCookieDBListener, mozIStorageCompletionCallback)

}  // namespace

// static
already_AddRefed<CookiePersistentStorage> CookiePersistentStorage::Create() {
  RefPtr<CookiePersistentStorage> storage = new CookiePersistentStorage();
  storage->Init();

  return storage.forget();
}

CookiePersistentStorage::CookiePersistentStorage()
    : mMonitor("CookiePersistentStorage"),
      mInitialized(false),
      mCorruptFlag(OK) {}

void CookiePersistentStorage::NotifyChangedInternal(nsISupports* aSubject,
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

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(aSubject, "session-cookie-changed", aData);
  }
}

void CookiePersistentStorage::RemoveAllInternal() {
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

void CookiePersistentStorage::HandleCorruptDB() {
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
      CleanupDBConnection();
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
      CleanupDBConnection();
      break;
    }
  }
}

void CookiePersistentStorage::RemoveCookiesWithOriginAttributes(
    const OriginAttributesPattern& aPattern, const nsACString& aBaseDomain) {
  mozStorageTransaction transaction(mDBConn, false);

  CookieStorage::RemoveCookiesWithOriginAttributes(aPattern, aBaseDomain);

  DebugOnly<nsresult> rv = transaction.Commit();
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void CookiePersistentStorage::RemoveCookiesFromExactHost(
    const nsACString& aHost, const nsACString& aBaseDomain,
    const OriginAttributesPattern& aPattern) {
  mozStorageTransaction transaction(mDBConn, false);

  CookieStorage::RemoveCookiesFromExactHost(aHost, aBaseDomain, aPattern);

  DebugOnly<nsresult> rv = transaction.Commit();
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void CookiePersistentStorage::RemoveCookieFromDB(const CookieListIter& aIter) {
  // if it's a non-session cookie, remove it from the db
  if (aIter.Cookie()->IsSession() || !mDBConn) {
    return;
  }

  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  mStmtDelete->NewBindingParamsArray(getter_AddRefs(paramsArray));

  PrepareCookieRemoval(aIter, paramsArray);

  DebugOnly<nsresult> rv = mStmtDelete->BindParameters(paramsArray);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<mozIStoragePendingStatement> handle;
  rv = mStmtDelete->ExecuteAsync(mRemoveListener, getter_AddRefs(handle));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void CookiePersistentStorage::PrepareCookieRemoval(
    const CookieListIter& aIter, mozIStorageBindingParamsArray* aParamsArray) {
  // if it's a non-session cookie, remove it from the db
  if (aIter.Cookie()->IsSession() || !mDBConn) {
    return;
  }

  nsCOMPtr<mozIStorageBindingParams> params;
  aParamsArray->NewBindingParams(getter_AddRefs(params));

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

  rv = aParamsArray->AddParams(params);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

// Null out the statements.
// This must be done before closing the connection.
void CookiePersistentStorage::CleanupCachedStatements() {
  mStmtInsert = nullptr;
  mStmtDelete = nullptr;
  mStmtUpdate = nullptr;
}

// Null out the listeners, and the database connection itself. This
// will not null out the statements, cancel a pending read or
// asynchronously close the connection -- these must be done
// beforehand if necessary.
void CookiePersistentStorage::CleanupDBConnection() {
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

void CookiePersistentStorage::Close() {
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

  CleanupDBConnection();

  mInitialized = false;
  mInitializedDBConn = false;
}

nsresult CookiePersistentStorage::ImportCookies(nsIFile* aCookieFile) {
  MOZ_ASSERT(aCookieFile);

  nsresult rv;
  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), aCookieFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream =
      do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  static const char kTrue[] = "TRUE";

  nsAutoCString buffer;
  nsAutoCString baseDomain;
  bool isMore = true;
  int32_t hostIndex;
  int32_t isDomainIndex;
  int32_t pathIndex;
  int32_t secureIndex;
  int32_t expiresIndex;
  int32_t nameIndex;
  int32_t cookieIndex;
  int32_t numInts;
  int64_t expires;
  bool isDomain;
  bool isHttpOnly = false;
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
    rv = CookieCommons::GetBaseDomainFromHost(mTLDService, host, baseDomain);
    if (NS_FAILED(rv)) {
      continue;
    }

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
      AddCookieToList(baseDomain, OriginAttributes(), newCookie);
      if (!newCookie->IsSession() && mDBConn) {
        CookieKey key(baseDomain, OriginAttributes());
        BindCookieParameters(paramsArray, key, newCookie);
      }
    } else {
      AddCookie(baseDomain, OriginAttributes(), newCookie, currentTimeInUsec,
                nullptr, VoidCString(), true);
    }
  }

  // If we need to write to disk, do so now.
  MaybeStoreCookiesToDB(paramsArray);

  COOKIE_LOGSTRING(
      LogLevel::Debug,
      ("ImportCookies(): %" PRIu32 " cookies imported", mCookieCount));

  return NS_OK;
}

void CookiePersistentStorage::StoreCookie(
    const nsACString& aBaseDomain, const OriginAttributes& aOriginAttributes,
    Cookie* aCookie) {
  // if it's a non-session cookie and hasn't just been read from the db, write
  // it out.
  if (aCookie->IsSession() || !mDBConn) {
    return;
  }

  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  mStmtInsert->NewBindingParamsArray(getter_AddRefs(paramsArray));

  CookieKey key(aBaseDomain, aOriginAttributes);
  BindCookieParameters(paramsArray, key, aCookie);

  MaybeStoreCookiesToDB(paramsArray);
}

void CookiePersistentStorage::MaybeStoreCookiesToDB(
    mozIStorageBindingParamsArray* aParamsArray) {
  if (!aParamsArray) {
    return;
  }

  uint32_t length;
  aParamsArray->GetLength(&length);
  if (!length) {
    return;
  }

  DebugOnly<nsresult> rv = mStmtInsert->BindParameters(aParamsArray);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<mozIStoragePendingStatement> handle;
  rv = mStmtInsert->ExecuteAsync(mInsertListener, getter_AddRefs(handle));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void CookiePersistentStorage::StaleCookies(const nsTArray<Cookie*>& aCookieList,
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

void CookiePersistentStorage::UpdateCookieInList(
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

void CookiePersistentStorage::DeleteFromDB(
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

void CookiePersistentStorage::Activate() {
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

  RefPtr<CookiePersistentStorage> self = this;
  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("CookiePersistentStorage::Activate", [self] {
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
          self->CleanupDBConnection();
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
          self->CleanupDBConnection();

          // No need to initialize mDBConn
          self->mInitializedDBConn = true;
        }

        self->mInitialized = true;

        NS_DispatchToMainThread(
            NS_NewRunnableFunction("CookiePersistentStorage::InitDBConn",
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
CookiePersistentStorage::OpenDBResult CookiePersistentStorage::TryInitDB(
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

        nsCString baseDomain;
        nsCString host;
        bool hasResult;
        while (true) {
          rv = select->ExecuteStep(&hasResult);
          NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

          if (!hasResult) {
            break;
          }

          int64_t id = select->AsInt64(SCHEMA2_IDX_ID);
          select->GetUTF8String(SCHEMA2_IDX_HOST, host);

          rv = CookieCommons::GetBaseDomainFromHost(mTLDService, host,
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
          nsCString name1;
          nsCString host1;
          nsCString path1;
          int64_t id1 = select->AsInt64(SCHEMA3_IDX_ID);
          select->GetUTF8String(SCHEMA3_IDX_NAME, name1);
          select->GetUTF8String(SCHEMA3_IDX_HOST, host1);
          select->GetUTF8String(SCHEMA3_IDX_PATH, path1);

          nsCString name2;
          nsCString host2;
          nsCString path2;
          while (true) {
            // Read the second row.
            rv = select->ExecuteStep(&hasResult);
            NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

            if (!hasResult) {
              break;
            }

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
        if (NS_SUCCEEDED(rv)) {
          break;
        }

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

  RefPtr<CookiePersistentStorage> self = this;
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

void CookiePersistentStorage::RebuildCorruptDB() {
  NS_ASSERTION(!mDBConn, "shouldn't have an open db connection");
  NS_ASSERTION(mCorruptFlag == CookiePersistentStorage::CLOSING_FOR_REBUILD,
               "should be in CLOSING_FOR_REBUILD state");

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();

  mCorruptFlag = CookiePersistentStorage::REBUILDING;

  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("RebuildCorruptDB(): creating new database"));

  RefPtr<CookiePersistentStorage> self = this;
  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("RebuildCorruptDB.TryInitDB", [self] {
        // The database has been closed, and we're ready to rebuild. Open a
        // connection.
        OpenDBResult result = self->TryInitDB(true);

        nsCOMPtr<nsIRunnable> innerRunnable = NS_NewRunnableFunction(
            "RebuildCorruptDB.TryInitDBComplete", [self, result] {
              nsCOMPtr<nsIObserverService> os = services::GetObserverService();
              if (result != RESULT_OK) {
                // We're done. Reset our DB connection and statements, and
                // notify of closure.
                COOKIE_LOGSTRING(
                    LogLevel::Warning,
                    ("RebuildCorruptDB(): TryInitDB() failed with result %u",
                     result));
                self->CleanupCachedStatements();
                self->CleanupDBConnection();
                self->mCorruptFlag = CookiePersistentStorage::OK;
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
                self->mCorruptFlag = CookiePersistentStorage::OK;
                return;
              }

              self->MaybeStoreCookiesToDB(paramsArray);
            });
        NS_DispatchToMainThread(innerRunnable);
      });
  mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

void CookiePersistentStorage::HandleDBClosed() {
  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("HandleDBClosed(): CookieStorage %p closed", this));

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();

  switch (mCorruptFlag) {
    case CookiePersistentStorage::OK: {
      // Database is healthy. Notify of closure.
      if (os) {
        os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
      }
      break;
    }
    case CookiePersistentStorage::CLOSING_FOR_REBUILD: {
      // Our close finished. Start the rebuild, and notify of db closure later.
      RebuildCorruptDB();
      break;
    }
    case CookiePersistentStorage::REBUILDING: {
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

CookiePersistentStorage::OpenDBResult CookiePersistentStorage::Read() {
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

  nsCString baseDomain;
  nsCString name;
  nsCString value;
  nsCString host;
  nsCString path;
  bool hasResult;
  while (true) {
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mReadArray.Clear();
      return RESULT_RETRY;
    }

    if (!hasResult) {
      break;
    }

    stmt->GetUTF8String(IDX_HOST, host);

    rv = CookieCommons::GetBaseDomainFromHost(mTLDService, host, baseDomain);
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
UniquePtr<CookieStruct> CookiePersistentStorage::GetCookieFromRow(
    mozIStorageStatement* aRow) {
  nsCString name;
  nsCString value;
  nsCString host;
  nsCString path;
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
  return MakeUnique<CookieStruct>(name, value, host, path, expiry, lastAccessed,
                                  creationTime, isHttpOnly, false, isSecure,
                                  sameSite, rawSameSite);
}

void CookiePersistentStorage::EnsureReadComplete() {
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

void CookiePersistentStorage::InitDBConn() {
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

    AddCookieToList(tuple.key.mBaseDomain, tuple.key.mOriginAttributes, cookie);
  }

  if (NS_FAILED(InitDBConnInternal())) {
    COOKIE_LOGSTRING(LogLevel::Warning,
                     ("InitDBConn(): retrying InitDBConnInternal()"));
    CleanupCachedStatements();
    CleanupDBConnection();
    if (NS_FAILED(InitDBConnInternal())) {
      COOKIE_LOGSTRING(
          LogLevel::Warning,
          ("InitDBConn(): InitDBConnInternal() failed, closing connection"));

      // Game over, clean the connections.
      CleanupCachedStatements();
      CleanupDBConnection();
    }
  }
  mInitializedDBConn = true;

  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("InitDBConn(): mInitializedDBConn = true"));
  mEndInitDBConn = TimeStamp::Now();

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, "cookie-db-read", nullptr);
    mReadArray.Clear();
  }
}

nsresult CookiePersistentStorage::InitDBConnInternal() {
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
nsresult CookiePersistentStorage::CreateTableWorker(const char* aName) {
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
nsresult CookiePersistentStorage::CreateTable() {
  // Set the schema version, before creating the table.
  nsresult rv = mSyncConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = CreateTableWorker("moz_cookies");
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

// Sets the schema version and creates the moz_cookies table.
nsresult CookiePersistentStorage::CreateTableForSchemaVersion6() {
  // Set the schema version, before creating the table.
  nsresult rv = mSyncConn->SetSchemaVersion(6);
  if (NS_FAILED(rv)) {
    return rv;
  }

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
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Create an index on baseDomain.
  return mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, "
      "originAttributes)"));
}

// Sets the schema version and creates the moz_cookies table.
nsresult CookiePersistentStorage::CreateTableForSchemaVersion5() {
  // Set the schema version, before creating the table.
  nsresult rv = mSyncConn->SetSchemaVersion(5);
  if (NS_FAILED(rv)) {
    return rv;
  }

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
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Create an index on baseDomain.
  return mSyncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, "
      "appId, "
      "inBrowserElement)"));
}

nsresult CookiePersistentStorage::RunInTransaction(
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

// purges expired and old cookies in a batch operation.
already_AddRefed<nsIArray> CookiePersistentStorage::PurgeCookies(
    int64_t aCurrentTimeInUsec, uint16_t aMaxNumberOfCookies,
    int64_t aCookiePurgeAge) {
  // Create a params array to batch the removals. This is OK here because
  // all the removals are in order, and there are no interleaved additions.
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  if (mDBConn) {
    mStmtDelete->NewBindingParamsArray(getter_AddRefs(paramsArray));
  }

  RefPtr<CookiePersistentStorage> self = this;

  return PurgeCookiesWithCallbacks(
      aCurrentTimeInUsec, aMaxNumberOfCookies, aCookiePurgeAge,
      [paramsArray, self](const CookieListIter& aIter) {
        self->PrepareCookieRemoval(aIter, paramsArray);
        self->RemoveCookieFromListInternal(aIter);
      },
      [paramsArray, self]() {
        if (paramsArray) {
          self->DeleteFromDB(paramsArray);
        }
      });
}

}  // namespace net
}  // namespace mozilla
