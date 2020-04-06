/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieLogging.h"
#include "CookieCommons.h"

#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ContentBlockingNotifier.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/Printf.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/Unused.h"

#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/net/CookiePermission.h"
#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/NeckoCommon.h"

#include "nsCookieService.h"
#include "nsContentUtils.h"
#include "nsIClassifiedChannel.h"
#include "nsIWebProgressListener.h"
#include "nsIHttpChannel.h"
#include "nsIScriptError.h"

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsILineInputStream.h"
#include "nsIEffectiveTLDService.h"
#include "nsIIDNService.h"
#include "mozIStorageBindingParamsArray.h"
#include "mozIStorageError.h"
#include "mozIStorageFunction.h"
#include "mozIStorageService.h"
#include "mozIThirdPartyUtil.h"
#include "prprf.h"

#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsReadableUtils.h"
#include "nsQueryObject.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsIInputStream.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsNetCID.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/storage.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TextUtils.h"
#include "nsIConsoleService.h"
#include "nsVariant.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::net;

/******************************************************************************
 * nsCookieService impl:
 * useful types & constants
 ******************************************************************************/

static StaticRefPtr<nsCookieService> gCookieService;

#define COOKIES_FILE "cookies.sqlite"
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

static const int64_t kCookiePurgeAge =
    int64_t(30 * 24 * 60 * 60) * PR_USEC_PER_SEC;  // 30 days in microseconds

#define OLD_COOKIE_FILE_NAME "cookies.txt"

#undef LIMIT
#define LIMIT(x, low, high, default) \
  ((x) >= (low) && (x) <= (high) ? (x) : (default))

#define CONSOLE_SAMESITE_CATEGORY NS_LITERAL_CSTRING("cookieSameSite")
#define CONSOLE_GENERIC_CATEGORY NS_LITERAL_CSTRING("cookies")

// default limits for the cookie list. these can be tuned by the
// network.cookie.maxNumber and network.cookie.maxPerHost prefs respectively.
static const uint32_t kMaxCookiesPerHost = 180;
static const uint32_t kCookieQuotaPerHost = 150;
static const uint32_t kMaxBytesPerCookie = 4096;
static const uint32_t kMaxBytesPerPath = 1024;

// XXX This is not the final URL. See bug 1620334.
#define SAMESITE_MDN_URL \
  NS_LITERAL_STRING("https://developer.mozilla.org/docs/Web/HTTP/Cookies")

// pref string constants
static const char kPrefMaxNumberOfCookies[] = "network.cookie.maxNumber";
static const char kPrefMaxCookiesPerHost[] = "network.cookie.maxPerHost";
static const char kPrefCookieQuotaPerHost[] = "network.cookie.quotaPerHost";
static const char kPrefCookiePurgeAge[] = "network.cookie.purgeAge";

static void bindCookieParameters(mozIStorageBindingParamsArray* aParamsArray,
                                 const CookieKey& aKey, const Cookie* aCookie);

#ifdef DEBUG
#  define NS_ASSERT_SUCCESS(res)                                       \
    PR_BEGIN_MACRO                                                     \
    nsresult __rv = res; /* Do not evaluate |res| more than once! */   \
    if (NS_FAILED(__rv)) {                                             \
      SmprintfPointer msg = mozilla::Smprintf(                         \
          "NS_ASSERT_SUCCESS(%s) failed with result 0x%" PRIX32, #res, \
          static_cast<uint32_t>(__rv));                                \
      NS_ASSERTION(NS_SUCCEEDED(__rv), msg.get());                     \
    }                                                                  \
    PR_END_MACRO
#else
#  define NS_ASSERT_SUCCESS(res) PR_BEGIN_MACRO /* nothing */ PR_END_MACRO
#endif

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
    // If we were rebuilding the db and we succeeded, make our corruptFlag say
    // so.
    if (mStorage->corruptFlag == CookieDefaultStorage::REBUILDING &&
        aReason == mozIStorageStatementCallback::REASON_FINISHED) {
      COOKIE_LOGSTRING(
          LogLevel::Debug,
          ("InsertCookieDBListener::HandleCompletion(): rebuild complete"));
      mStorage->corruptFlag = CookieDefaultStorage::OK;
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
    gCookieService->HandleDBClosed(mStorage);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(CloseCookieDBListener, mozIStorageCompletionCallback)

namespace {

// Return false if the cookie should be ignored for the current channel.
bool ProcessSameSiteCookieForForeignRequest(nsIChannel* aChannel,
                                            Cookie* aCookie,
                                            bool aIsSafeTopLevelNav,
                                            bool aLaxByDefault) {
  int32_t sameSiteAttr = 0;
  aCookie->GetSameSite(&sameSiteAttr);

  // it if's a cross origin request and the cookie is same site only (strict)
  // don't send it
  if (sameSiteAttr == nsICookie::SAMESITE_STRICT) {
    return false;
  }

  int64_t currentTimeInUsec = PR_Now();

  // 2 minutes of tolerance for 'sameSite=lax by default' for cookies set
  // without a sameSite value when used for unsafe http methods.
  if (StaticPrefs::network_cookie_sameSite_laxPlusPOST_timeout() > 0 &&
      aLaxByDefault && sameSiteAttr == nsICookie::SAMESITE_LAX &&
      aCookie->RawSameSite() == nsICookie::SAMESITE_NONE &&
      currentTimeInUsec - aCookie->CreationTime() <=
          (StaticPrefs::network_cookie_sameSite_laxPlusPOST_timeout() *
           PR_USEC_PER_SEC) &&
      !NS_IsSafeMethodNav(aChannel)) {
    return true;
  }

  // if it's a cross origin request, the cookie is same site lax, but it's not a
  // top-level navigation, don't send it
  return sameSiteAttr != nsICookie::SAMESITE_LAX || aIsSafeTopLevelNav;
}

}  // namespace

/******************************************************************************
 * nsCookieService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/

already_AddRefed<nsICookieService> nsCookieService::GetXPCOMSingleton() {
  if (IsNeckoChild()) return CookieServiceChild::GetSingleton();

  return GetSingleton();
}

already_AddRefed<nsCookieService> nsCookieService::GetSingleton() {
  NS_ASSERTION(!IsNeckoChild(), "not a parent process");

  if (gCookieService) {
    return do_AddRef(gCookieService);
  }

  // Create a new singleton nsCookieService.
  // We AddRef only once since XPCOM has rules about the ordering of module
  // teardowns - by the time our module destructor is called, it's too late to
  // Release our members (e.g. nsIObserverService and nsIPrefBranch), since GC
  // cycles have already been completed and would result in serious leaks.
  // See bug 209571.
  gCookieService = new nsCookieService();
  if (gCookieService) {
    if (NS_SUCCEEDED(gCookieService->Init())) {
      ClearOnShutdown(&gCookieService);
    } else {
      gCookieService = nullptr;
    }
  }

  return do_AddRef(gCookieService);
}

/******************************************************************************
 * nsCookieService impl:
 * public methods
 ******************************************************************************/

NS_IMPL_ISUPPORTS(nsCookieService, nsICookieService, nsICookieManager,
                  nsIObserver, nsISupportsWeakReference, nsIMemoryReporter)

nsCookieService::nsCookieService()
    : mMaxNumberOfCookies(kMaxNumberOfCookies),
      mMaxCookiesPerHost(kMaxCookiesPerHost),
      mCookieQuotaPerHost(kCookieQuotaPerHost),
      mCookiePurgeAge(kCookiePurgeAge),
      mThread(nullptr),
      mMonitor("CookieThread"),
      mInitializedCookieStorages(false),
      mInitializedDBConn(false) {}

nsresult nsCookieService::Init() {
  nsresult rv;
  mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mIDNService = do_GetService(NS_IDNSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mThirdPartyUtil = do_GetService(THIRDPARTYUTIL_CONTRACTID);
  NS_ENSURE_SUCCESS(rv, rv);

  // init our pref and observer
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    prefBranch->AddObserver(kPrefMaxNumberOfCookies, this, true);
    prefBranch->AddObserver(kPrefMaxCookiesPerHost, this, true);
    prefBranch->AddObserver(kPrefCookiePurgeAge, this, true);
    PrefChanged(prefBranch);
  }

  mStorageService = do_GetService("@mozilla.org/storage/service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Init our default, and possibly private CookieStorages.
  InitCookieStorages();

  RegisterWeakMemoryReporter(this);

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  NS_ENSURE_STATE(os);
  os->AddObserver(this, "profile-before-change", true);
  os->AddObserver(this, "profile-do-change", true);
  os->AddObserver(this, "last-pb-context-exited", true);

  mPermissionService = CookiePermission::GetOrCreate();

  return NS_OK;
}

void nsCookieService::InitCookieStorages() {
  NS_ASSERTION(!mDefaultStorage, "already have a default CookieStorage");
  NS_ASSERTION(!mPrivateStorage, "already have a private CookieStorage");
  NS_ASSERTION(!mInitializedCookieStorages, "already initialized");
  NS_ASSERTION(!mThread, "already have a cookie thread");

  // Create two new CookieStorages.
  mDefaultStorage = CookieDefaultStorage::Create();
  mPrivateStorage = CookiePrivateStorage::Create();

  // Get our cookie file.
  nsresult rv = NS_GetSpecialDirectory(
      NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mDefaultStorage->cookieFile));
  if (NS_FAILED(rv)) {
    // We've already set up our CookieStorages appropriately; nothing more to
    // do.
    COOKIE_LOGSTRING(LogLevel::Warning,
                     ("InitCookieStorages(): couldn't get cookie file"));

    mInitializedDBConn = true;
    mInitializedCookieStorages = true;
    return;
  }
  mDefaultStorage->cookieFile->AppendNative(NS_LITERAL_CSTRING(COOKIES_FILE));

  NS_ENSURE_SUCCESS_VOID(NS_NewNamedThread("Cookie", getter_AddRefs(mThread)));

  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("InitCookieStorages.TryInitDB", [] {
        NS_ENSURE_TRUE_VOID(gCookieService);

        MonitorAutoLock lock(gCookieService->mMonitor);

        // Attempt to open and read the database. If TryInitDB() returns
        // RESULT_RETRY, do so.
        OpenDBResult result = gCookieService->TryInitDB(false);
        if (result == RESULT_RETRY) {
          // Database may be corrupt. Synchronously close the connection, clean
          // up the default CookieStorage, and try again.
          COOKIE_LOGSTRING(LogLevel::Warning,
                           ("InitCookieStorages(): retrying TryInitDB()"));
          gCookieService->mDefaultStorage->CleanupCachedStatements();
          gCookieService->mDefaultStorage->CleanupDefaultDBConnection();
          result = gCookieService->TryInitDB(true);
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
          gCookieService->mDefaultStorage->CleanupCachedStatements();
          gCookieService->mDefaultStorage->CleanupDefaultDBConnection();

          // No need to initialize dbConn
          gCookieService->mInitializedDBConn = true;
        }

        gCookieService->mInitializedCookieStorages = true;

        NS_DispatchToMainThread(
            NS_NewRunnableFunction("TryInitDB.InitDBConn", [] {
              NS_ENSURE_TRUE_VOID(gCookieService);
              gCookieService->InitDBConn();
            }));
        gCookieService->mMonitor.Notify();
      });

  mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

namespace {

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

}  // namespace

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
OpenDBResult nsCookieService::TryInitDB(bool aRecreateDB) {
  NS_ASSERTION(!mDefaultStorage->dbConn, "nonnull dbConn");
  NS_ASSERTION(!mDefaultStorage->stmtInsert, "nonnull stmtInsert");
  NS_ASSERTION(!mDefaultStorage->insertListener, "nonnull insertListener");
  NS_ASSERTION(!mDefaultStorage->syncConn, "nonnull syncConn");
  NS_ASSERTION(NS_GetCurrentThread() == mThread, "non cookie thread");

  // Ditch an existing db, if we've been told to (i.e. it's corrupt). We don't
  // want to delete it outright, since it may be useful for debugging purposes,
  // so we move it out of the way.
  nsresult rv;
  if (aRecreateDB) {
    nsCOMPtr<nsIFile> backupFile;
    mDefaultStorage->cookieFile->Clone(getter_AddRefs(backupFile));
    rv = backupFile->MoveToNative(nullptr,
                                  NS_LITERAL_CSTRING(COOKIES_FILE ".bak"));
    NS_ENSURE_SUCCESS(rv, RESULT_FAILURE);
  }

  // This block provides scope for the Telemetry AutoTimer
  {
    Telemetry::AutoTimer<Telemetry::MOZ_SQLITE_COOKIES_OPEN_READAHEAD_MS>
        telemetry;
    ReadAheadFile(mDefaultStorage->cookieFile);

    // open a connection to the cookie database, and only cache our connection
    // and statements upon success. The connection is opened unshared to
    // eliminate cache contention between the main and background threads.
    rv = mStorageService->OpenUnsharedDatabase(
        mDefaultStorage->cookieFile, getter_AddRefs(mDefaultStorage->syncConn));
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
  }

  auto guard = MakeScopeExit([&] { mDefaultStorage->syncConn = nullptr; });

  bool tableExists = false;
  mDefaultStorage->syncConn->TableExists(NS_LITERAL_CSTRING("moz_cookies"),
                                         &tableExists);
  if (!tableExists) {
    rv = CreateTable();
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  } else {
    // table already exists; check the schema version before reading
    int32_t dbSchemaVersion;
    rv = mDefaultStorage->syncConn->GetSchemaVersion(&dbSchemaVersion);
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

    // Start a transaction for the whole migration block.
    mozStorageTransaction transaction(mDefaultStorage->syncConn, true);

    switch (dbSchemaVersion) {
      // Upgrading.
      // Every time you increment the database schema, you need to implement
      // the upgrading code from the previous version to the new one. If
      // migration fails for any reason, it's a bug -- so we return RESULT_RETRY
      // such that the original database will be saved, in the hopes that we
      // might one day see it and fix it.
      case 1: {
        // Add the lastAccessed column to the table.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies ADD lastAccessed INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
        // Fall through to the next upgrade.
        [[fallthrough]];

      case 2: {
        // Add the baseDomain column and index to the table.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("ALTER TABLE moz_cookies ADD baseDomain TEXT"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Compute the baseDomains for the table. This must be done eagerly
        // otherwise we won't be able to synchronously read in individual
        // domains on demand.
        const int64_t SCHEMA2_IDX_ID = 0;
        const int64_t SCHEMA2_IDX_HOST = 1;
        nsCOMPtr<mozIStorageStatement> select;
        rv = mDefaultStorage->syncConn->CreateStatement(
            NS_LITERAL_CSTRING("SELECT id, host FROM moz_cookies"),
            getter_AddRefs(select));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageStatement> update;
        rv = mDefaultStorage->syncConn->CreateStatement(
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

          rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
          NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

          mozStorageStatementScoper scoper(update);

          rv = update->BindUTF8StringByName(NS_LITERAL_CSTRING("baseDomain"),
                                            baseDomain);
          NS_ASSERT_SUCCESS(rv);
          rv = update->BindInt64ByName(NS_LITERAL_CSTRING("id"), id);
          NS_ASSERT_SUCCESS(rv);

          rv = update->ExecuteStep(&hasResult);
          NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
        }

        // Create an index on baseDomain.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
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
        rv = mDefaultStorage->syncConn->CreateStatement(
            NS_LITERAL_CSTRING(
                "SELECT id, name, host, path FROM moz_cookies "
                "ORDER BY name ASC, host ASC, path ASC, expiry ASC"),
            getter_AddRefs(select));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageStatement> deleteExpired;
        rv = mDefaultStorage->syncConn->CreateStatement(
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
              NS_ASSERT_SUCCESS(rv);

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
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies ADD creationTime INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy the id of each row into the new creationTime column.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("UPDATE moz_cookies SET creationTime = "
                               "(SELECT id WHERE id = moz_cookies.id)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create a unique index on (name, host, path) to allow fast lookup.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
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
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies RENAME TO moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop existing index (CreateTable will create new one for new table)
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP INDEX moz_basedomain"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create new table (with new fields and new unique constraint)
        rv = CreateTableForSchemaVersion5();
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy data from old table, using appId/inBrowser=0 for existing rows
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "INSERT INTO moz_cookies "
            "(baseDomain, appId, inBrowserElement, name, value, host, path, "
            "expiry,"
            " lastAccessed, creationTime, isSecure, isHttpOnly) "
            "SELECT baseDomain, 0, 0, name, value, host, path, expiry,"
            " lastAccessed, creationTime, isSecure, isHttpOnly "
            "FROM moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop old table
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
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
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies RENAME TO moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop existing index (CreateTable will create new one for new table).
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
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

        rv = mDefaultStorage->syncConn->CreateFunction(convertToOriginAttrsName,
                                                       2, convertToOriginAttrs);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "INSERT INTO moz_cookies "
            "(baseDomain, originAttributes, name, value, host, path, expiry,"
            " lastAccessed, creationTime, isSecure, isHttpOnly) "
            "SELECT baseDomain, "
            " CONVERT_TO_ORIGIN_ATTRIBUTES(appId, inBrowserElement),"
            " name, value, host, path, expiry, lastAccessed, creationTime, "
            " isSecure, isHttpOnly "
            "FROM moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv =
            mDefaultStorage->syncConn->RemoveFunction(convertToOriginAttrsName);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop old table
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
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
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies ADD appId INTEGER DEFAULT 0;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies ADD inBrowserElement INTEGER DEFAULT 0;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Compute and populate the values of appId and inBrwoserElement from
        // originAttributes.
        nsCOMPtr<mozIStorageFunction> setAppId(
            new SetAppIdFromOriginAttributesSQLFunction());
        NS_ENSURE_TRUE(setAppId, RESULT_RETRY);

        NS_NAMED_LITERAL_CSTRING(setAppIdName, "SET_APP_ID");

        rv = mDefaultStorage->syncConn->CreateFunction(setAppIdName, 1,
                                                       setAppId);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageFunction> setInBrowser(
            new SetInBrowserFromOriginAttributesSQLFunction());
        NS_ENSURE_TRUE(setInBrowser, RESULT_RETRY);

        NS_NAMED_LITERAL_CSTRING(setInBrowserName, "SET_IN_BROWSER");

        rv = mDefaultStorage->syncConn->CreateFunction(setInBrowserName, 1,
                                                       setInBrowser);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "UPDATE moz_cookies SET appId = SET_APP_ID(originAttributes), "
            "inBrowserElement = SET_IN_BROWSER(originAttributes);"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultStorage->syncConn->RemoveFunction(setAppIdName);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultStorage->syncConn->RemoveFunction(setInBrowserName);
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
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP INDEX moz_basedomain"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create a new_moz_cookies table without the appId field.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
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
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
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
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP TABLE moz_cookies;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Rename new_moz_cookies to moz_cookies.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE new_moz_cookies RENAME TO moz_cookies;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Recreate our index.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("CREATE INDEX moz_basedomain ON moz_cookies "
                               "(baseDomain, originAttributes)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 8"));
      }
        [[fallthrough]];

      case 8: {
        // Add the sameSite column to the table.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("ALTER TABLE moz_cookies ADD sameSite INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 9"));
      }
        [[fallthrough]];

      case 9: {
        // Add the rawSameSite column to the table.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies ADD rawSameSite INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy the current sameSite value into rawSameSite.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "UPDATE moz_cookies SET rawSameSite = sameSite"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 10"));
      }
        [[fallthrough]];

      case 10: {
        // Rename existing table
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_cookies RENAME TO moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create a new moz_cookies table without the baseDomain field.
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
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
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
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
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP TABLE moz_cookies_old;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop the moz_basedomain index from the database (if it hasn't been
        // removed already by removing the table).
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP INDEX IF EXISTS moz_basedomain;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
                         ("Upgraded database to schema version 11"));

        // No more upgrades. Update the schema version.
        rv =
            mDefaultStorage->syncConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
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
        rv =
            mDefaultStorage->syncConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
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
        rv = mDefaultStorage->syncConn->CreateStatement(
            NS_LITERAL_CSTRING("SELECT "
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
        rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
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

  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("TryInitDB.ImportCookies", [] {
        NS_ENSURE_TRUE_VOID(gCookieService);
        NS_ENSURE_TRUE_VOID(gCookieService->mDefaultStorage);
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
        gCookieService->ImportCookies(oldCookieFile);
        oldCookieFile->Remove(false);
      });

  NS_DispatchToMainThread(runnable);

  return RESULT_OK;
}

void nsCookieService::InitDBConn() {
  MOZ_ASSERT(NS_IsMainThread());

  // We should skip InitDBConn if we close profile during initializing
  // CookieStorages and then InitDBConn is called after we close the
  // CookieStorages.
  if (!mInitializedCookieStorages || mInitializedDBConn || !mDefaultStorage) {
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

    mDefaultStorage->AddCookieToList(tuple.key.mBaseDomain,
                                     tuple.key.mOriginAttributes, cookie,
                                     nullptr, false);
  }

  if (NS_FAILED(InitDBConnInternal())) {
    COOKIE_LOGSTRING(LogLevel::Warning,
                     ("InitDBConn(): retrying InitDBConnInternal()"));
    mDefaultStorage->CleanupCachedStatements();
    mDefaultStorage->CleanupDefaultDBConnection();
    if (NS_FAILED(InitDBConnInternal())) {
      COOKIE_LOGSTRING(
          LogLevel::Warning,
          ("InitDBConn(): InitDBConnInternal() failed, closing connection"));

      // Game over, clean the connections.
      mDefaultStorage->CleanupCachedStatements();
      mDefaultStorage->CleanupDefaultDBConnection();
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

nsresult nsCookieService::InitDBConnInternal() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = mStorageService->OpenUnsharedDatabase(
      mDefaultStorage->cookieFile, getter_AddRefs(mDefaultStorage->dbConn));
  NS_ENSURE_SUCCESS(rv, rv);

  // Set up our listeners.
  mDefaultStorage->insertListener = new InsertCookieDBListener(mDefaultStorage);
  mDefaultStorage->updateListener = new UpdateCookieDBListener(mDefaultStorage);
  mDefaultStorage->removeListener = new RemoveCookieDBListener(mDefaultStorage);
  mDefaultStorage->closeListener = new CloseCookieDBListener(mDefaultStorage);

  // Grow cookie db in 512KB increments
  mDefaultStorage->dbConn->SetGrowthIncrement(512 * 1024, EmptyCString());

  // make operations on the table asynchronous, for performance
  mDefaultStorage->dbConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("PRAGMA synchronous = OFF"));

  // Use write-ahead-logging for performance. We cap the autocheckpoint limit at
  // 16 pages (around 500KB).
  mDefaultStorage->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA journal_mode = WAL"));
  mDefaultStorage->dbConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("PRAGMA wal_autocheckpoint = 16"));

  // cache frequently used statements (for insertion, deletion, and updating)
  rv = mDefaultStorage->dbConn->CreateAsyncStatement(
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
      getter_AddRefs(mDefaultStorage->stmtInsert));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDefaultStorage->dbConn->CreateAsyncStatement(
      NS_LITERAL_CSTRING("DELETE FROM moz_cookies "
                         "WHERE name = :name AND host = :host AND path = :path "
                         "AND originAttributes = :originAttributes"),
      getter_AddRefs(mDefaultStorage->stmtDelete));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDefaultStorage->dbConn->CreateAsyncStatement(
      NS_LITERAL_CSTRING("UPDATE moz_cookies SET lastAccessed = :lastAccessed "
                         "WHERE name = :name AND host = :host AND path = :path "
                         "AND originAttributes = :originAttributes"),
      getter_AddRefs(mDefaultStorage->stmtUpdate));
  return rv;
}

// Sets the schema version and creates the moz_cookies table.
nsresult nsCookieService::CreateTableWorker(const char* aName) {
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
  return mDefaultStorage->syncConn->ExecuteSimpleSQL(command);
}

// Sets the schema version and creates the moz_cookies table.
nsresult nsCookieService::CreateTable() {
  // Set the schema version, before creating the table.
  nsresult rv =
      mDefaultStorage->syncConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
  if (NS_FAILED(rv)) return rv;

  rv = CreateTableWorker("moz_cookies");
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

// Sets the schema version and creates the moz_cookies table.
nsresult nsCookieService::CreateTableForSchemaVersion6() {
  // Set the schema version, before creating the table.
  nsresult rv = mDefaultStorage->syncConn->SetSchemaVersion(6);
  if (NS_FAILED(rv)) return rv;

  // Create the table.
  // We default originAttributes to empty string: this is so if users revert to
  // an older Firefox version that doesn't know about this field, any cookies
  // set will still work once they upgrade back.
  rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
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
  return mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, "
      "originAttributes)"));
}

// Sets the schema version and creates the moz_cookies table.
nsresult nsCookieService::CreateTableForSchemaVersion5() {
  // Set the schema version, before creating the table.
  nsresult rv = mDefaultStorage->syncConn->SetSchemaVersion(5);
  if (NS_FAILED(rv)) return rv;

  // Create the table. We default appId/inBrowserElement to 0: this is so if
  // users revert to an older Firefox version that doesn't know about these
  // fields, any cookies set will still work once they upgrade back.
  rv = mDefaultStorage->syncConn->ExecuteSimpleSQL(
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
  return mDefaultStorage->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, "
      "appId, "
      "inBrowserElement)"));
}

void nsCookieService::CloseCookieStorages() {
  // return if we already closed
  if (!mDefaultStorage) {
    return;
  }

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }

  if (mPrivateStorage) {
    mPrivateStorage->Close();
    mPrivateStorage = nullptr;
  }

  mDefaultStorage->Close();
  mDefaultStorage = nullptr;

  mInitializedDBConn = false;
  mInitializedCookieStorages = false;
}

void nsCookieService::HandleDBClosed(CookieDefaultStorage* aCookieStorage) {
  COOKIE_LOGSTRING(
      LogLevel::Debug,
      ("HandleDBClosed(): CookieStorage %p closed", aCookieStorage));

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();

  switch (aCookieStorage->corruptFlag) {
    case CookieDefaultStorage::OK: {
      // Database is healthy. Notify of closure.
      if (os) {
        os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
      }
      break;
    }
    case CookieDefaultStorage::CLOSING_FOR_REBUILD: {
      // Our close finished. Start the rebuild, and notify of db closure later.
      RebuildCorruptDB(aCookieStorage);
      break;
    }
    case CookieDefaultStorage::REBUILDING: {
      // We encountered an error during rebuild, closed the database, and now
      // here we are. We already have a 'cookies.sqlite.bak' from the original
      // dead database; we don't want to overwrite it, so let's move this one to
      // 'cookies.sqlite.bak-rebuild'.
      nsCOMPtr<nsIFile> backupFile;
      aCookieStorage->cookieFile->Clone(getter_AddRefs(backupFile));
      nsresult rv = backupFile->MoveToNative(
          nullptr, NS_LITERAL_CSTRING(COOKIES_FILE ".bak-rebuild"));

      COOKIE_LOGSTRING(LogLevel::Warning,
                       ("HandleDBClosed(): CookieStorage %p encountered error "
                        "rebuilding db; move to "
                        "'cookies.sqlite.bak-rebuild' gave rv 0x%" PRIx32,
                        aCookieStorage, static_cast<uint32_t>(rv)));
      if (os) {
        os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
      }
      break;
    }
  }
}

void nsCookieService::RebuildCorruptDB(CookieDefaultStorage* aCookieStorage) {
  NS_ASSERTION(!aCookieStorage->dbConn, "shouldn't have an open db connection");
  NS_ASSERTION(
      aCookieStorage->corruptFlag == CookieDefaultStorage::CLOSING_FOR_REBUILD,
      "should be in CLOSING_FOR_REBUILD state");

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();

  aCookieStorage->corruptFlag = CookieDefaultStorage::REBUILDING;

  if (mDefaultStorage != aCookieStorage) {
    // We've either closed the state or we've switched profiles. It's getting
    // a bit late to rebuild -- bail instead. In any case, we were waiting
    // on rebuild completion to notify of the db closure, which won't happen --
    // do so now.
    COOKIE_LOGSTRING(LogLevel::Warning,
                     ("RebuildCorruptDB(): CookieStorage %p is stale, aborting",
                      aCookieStorage));
    if (os) {
      os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
    }
    return;
  }

  COOKIE_LOGSTRING(LogLevel::Debug,
                   ("RebuildCorruptDB(): creating new database"));

  nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableFunction("RebuildCorruptDB.TryInitDB", [] {
        NS_ENSURE_TRUE_VOID(gCookieService && gCookieService->mDefaultStorage);

        // The database has been closed, and we're ready to rebuild. Open a
        // connection.
        OpenDBResult result = gCookieService->TryInitDB(true);

        nsCOMPtr<nsIRunnable> innerRunnable = NS_NewRunnableFunction(
            "RebuildCorruptDB.TryInitDBComplete", [result] {
              NS_ENSURE_TRUE_VOID(gCookieService &&
                                  gCookieService->mDefaultStorage);

              nsCOMPtr<nsIObserverService> os =
                  mozilla::services::GetObserverService();
              if (result != RESULT_OK) {
                // We're done. Reset our DB connection and statements, and
                // notify of closure.
                COOKIE_LOGSTRING(
                    LogLevel::Warning,
                    ("RebuildCorruptDB(): TryInitDB() failed with result %u",
                     result));
                gCookieService->mDefaultStorage->CleanupCachedStatements();
                gCookieService->mDefaultStorage->CleanupDefaultDBConnection();
                gCookieService->mDefaultStorage->corruptFlag =
                    CookieDefaultStorage::OK;
                if (os) {
                  os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
                }
                return;
              }

              // Notify observers that we're beginning the rebuild.
              if (os) {
                os->NotifyObservers(nullptr, "cookie-db-rebuilding", nullptr);
              }

              gCookieService->InitDBConnInternal();

              // Enumerate the hash, and add cookies to the params array.
              mozIStorageAsyncStatement* stmt =
                  gCookieService->mDefaultStorage->stmtInsert;
              nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
              stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
              for (auto iter =
                       gCookieService->mDefaultStorage->hostTable.Iter();
                   !iter.Done(); iter.Next()) {
                CookieEntry* entry = iter.Get();

                const CookieEntry::ArrayType& cookies = entry->GetCookies();
                for (CookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
                  Cookie* cookie = cookies[i];

                  if (!cookie->IsSession()) {
                    bindCookieParameters(paramsArray, CookieKey(entry), cookie);
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
                gCookieService->mDefaultStorage->corruptFlag =
                    CookieDefaultStorage::OK;
                return;
              }

              // Execute the statement. If any errors crop up, we won't try
              // again.
              DebugOnly<nsresult> rv = stmt->BindParameters(paramsArray);
              NS_ASSERT_SUCCESS(rv);
              nsCOMPtr<mozIStoragePendingStatement> handle;
              rv = stmt->ExecuteAsync(
                  gCookieService->mDefaultStorage->insertListener,
                  getter_AddRefs(handle));
              NS_ASSERT_SUCCESS(rv);
            });
        NS_DispatchToMainThread(innerRunnable);
      });
  mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

nsCookieService::~nsCookieService() {
  CloseCookieStorages();

  UnregisterWeakMemoryReporter(this);

  gCookieService = nullptr;
}

NS_IMETHODIMP
nsCookieService::Observe(nsISupports* aSubject, const char* aTopic,
                         const char16_t* aData) {
  // check the topic
  if (!strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.

    // Close the default DB connection and null out our CookieStorages before
    // changing.
    CloseCookieStorages();

  } else if (!strcmp(aTopic, "profile-do-change")) {
    NS_ASSERTION(!mDefaultStorage, "shouldn't have a default CookieStorage");
    NS_ASSERTION(!mPrivateStorage, "shouldn't have a private CookieStorage");

    // the profile has already changed; init the db from the new location.
    // if we are in the private browsing state, however, we do not want to read
    // data into it - we should instead put it into the default state, so it's
    // ready for us if and when we switch back to it.
    InitCookieStorages();

  } else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
    if (prefBranch) PrefChanged(prefBranch);

  } else if (!strcmp(aTopic, "last-pb-context-exited")) {
    // Flush all the cookies stored by private browsing contexts
    mozilla::OriginAttributesPattern pattern;
    pattern.mPrivateBrowsingId.Construct(1);
    RemoveCookiesWithOriginAttributes(pattern, EmptyCString());
    mPrivateStorage = CookiePrivateStorage::Create();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookieString(nsIURI* aHostURI, nsIChannel* aChannel,
                                 nsACString& aCookie) {
  return GetCookieStringCommon(aHostURI, aChannel, false, aCookie);
}

NS_IMETHODIMP
nsCookieService::GetCookieStringFromHttp(nsIURI* aHostURI, nsIURI* aFirstURI,
                                         nsIChannel* aChannel,
                                         nsACString& aCookie) {
  return GetCookieStringCommon(aHostURI, aChannel, true, aCookie);
}

nsresult nsCookieService::GetCookieStringCommon(nsIURI* aHostURI,
                                                nsIChannel* aChannel,
                                                bool aHttpBound,
                                                nsACString& aCookie) {
  NS_ENSURE_ARG(aHostURI);

  aCookie.Truncate();

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, false, aHostURI, nullptr, &rejectedReason);

  OriginAttributes attrs;
  if (aChannel) {
    NS_GetOriginAttributes(aChannel, attrs,
                           true /* considering storage principal */);
  }

  bool isSafeTopLevelNav = NS_IsSafeTopLevelNav(aChannel);
  bool isSameSiteForeign = NS_IsSameSiteForeign(aChannel, aHostURI);
  GetCookieStringInternal(
      aHostURI, aChannel, result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted),
      rejectedReason, isSafeTopLevelNav, isSameSiteForeign, aHttpBound, attrs,
      aCookie);
  return NS_OK;
}

// static
already_AddRefed<nsICookieJarSettings> nsCookieService::GetCookieJarSettings(
    nsIChannel* aChannel) {
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  if (aChannel) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    nsresult rv =
        loadInfo->GetCookieJarSettings(getter_AddRefs(cookieJarSettings));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      cookieJarSettings = CookieJarSettings::GetBlockingAll();
    }
  } else {
    cookieJarSettings = CookieJarSettings::Create();
  }

  MOZ_ASSERT(cookieJarSettings);
  return cookieJarSettings.forget();
}

NS_IMETHODIMP
nsCookieService::SetCookieString(nsIURI* aHostURI,
                                 const nsACString& aCookieHeader,
                                 nsIChannel* aChannel) {
  return SetCookieStringCommon(aHostURI, aCookieHeader, VoidCString(), aChannel,
                               false);
}

NS_IMETHODIMP
nsCookieService::SetCookieStringFromHttp(nsIURI* aHostURI, nsIURI* aFirstURI,
                                         const nsACString& aCookieHeader,
                                         const nsACString& aServerTime,
                                         nsIChannel* aChannel) {
  return SetCookieStringCommon(aHostURI, aCookieHeader, aServerTime, aChannel,
                               true);
}

int64_t nsCookieService::ParseServerTime(const nsACString& aServerTime) {
  // parse server local time. this is not just done here for efficiency
  // reasons - if there's an error parsing it, and we need to default it
  // to the current time, we must do it here since the current time in
  // SetCookieInternal() will change for each cookie processed.
  PRTime tempServerTime;
  int64_t serverTime;
  PRStatus result =
      PR_ParseTimeString(aServerTime.BeginReading(), true, &tempServerTime);
  if (result == PR_SUCCESS) {
    serverTime = tempServerTime / int64_t(PR_USEC_PER_SEC);
  } else {
    serverTime = PR_Now() / PR_USEC_PER_SEC;
  }

  return serverTime;
}

nsresult nsCookieService::SetCookieStringCommon(nsIURI* aHostURI,
                                                const nsACString& aCookieHeader,
                                                const nsACString& aServerTime,
                                                nsIChannel* aChannel,
                                                bool aFromHttp) {
  NS_ENSURE_ARG(aHostURI);

  uint32_t rejectedReason = 0;
  ThirdPartyAnalysisResult result = mThirdPartyUtil->AnalyzeChannel(
      aChannel, false, aHostURI, nullptr, &rejectedReason);

  OriginAttributes attrs;
  if (aChannel) {
    NS_GetOriginAttributes(aChannel, attrs,
                           true /* considering storage principal */);
  }

  nsCString cookieString(aCookieHeader);
  SetCookieStringInternal(
      aHostURI, result.contains(ThirdPartyAnalysis::IsForeign),
      result.contains(ThirdPartyAnalysis::IsThirdPartyTrackingResource),
      result.contains(ThirdPartyAnalysis::IsThirdPartySocialTrackingResource),
      result.contains(ThirdPartyAnalysis::IsFirstPartyStorageAccessGranted),
      rejectedReason, cookieString, aServerTime, aFromHttp, attrs, aChannel);
  return NS_OK;
}

void nsCookieService::SetCookieStringInternal(
    nsIURI* aHostURI, bool aIsForeign, bool aIsThirdPartyTrackingResource,
    bool aIsThirdPartySocialTrackingResource,
    bool aFirstPartyStorageAccessGranted, uint32_t aRejectedReason,
    nsCString& aCookieHeader, const nsACString& aServerTime, bool aFromHttp,
    const OriginAttributes& aOriginAttrs, nsIChannel* aChannel) {
  NS_ASSERTION(aHostURI, "null host!");

  if (!IsInitialized()) {
    return;
  }

  CookieStorage* storage = PickStorage(aOriginAttrs);

  // get the base domain for the host URI.
  // e.g. for "www.bbc.co.uk", this would be "bbc.co.uk".
  // file:// URI's (i.e. with an empty host) are allowed, but any other
  // scheme must have a non-empty host. A trailing dot in the host
  // is acceptable.
  bool requireHostMatch;
  nsAutoCString baseDomain;
  nsresult rv =
      GetBaseDomain(mTLDService, aHostURI, baseDomain, requireHostMatch);
  if (NS_FAILED(rv)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                      "couldn't get base domain from URI");
    return;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      GetCookieJarSettings(aChannel);

  nsAutoCString hostFromURI;
  aHostURI->GetHost(hostFromURI);
  rv = NormalizeHost(hostFromURI);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsAutoCString baseDomainFromURI;
  rv = GetBaseDomainFromHost(mTLDService, hostFromURI, baseDomainFromURI);
  NS_ENSURE_SUCCESS_VOID(rv);

  // check default prefs
  uint32_t priorCookieCount = storage->CountCookiesFromHost(
      baseDomainFromURI, aOriginAttrs.mPrivateBrowsingId);
  uint32_t rejectedReason = aRejectedReason;

  CookieStatus cookieStatus = CheckPrefs(
      cookieJarSettings, aHostURI, aIsForeign, aIsThirdPartyTrackingResource,
      aIsThirdPartySocialTrackingResource, aFirstPartyStorageAccessGranted,
      aCookieHeader, priorCookieCount, aOriginAttrs, &rejectedReason);

  MOZ_ASSERT_IF(rejectedReason, cookieStatus == STATUS_REJECTED);

  // fire a notification if third party or if cookie was rejected
  // (but not if there was an error)
  switch (cookieStatus) {
    case STATUS_REJECTED:
      NotifyRejected(aHostURI, aChannel, rejectedReason, OPERATION_WRITE);
      return;  // Stop here
    case STATUS_REJECTED_WITH_ERROR:
      NotifyRejected(aHostURI, aChannel, rejectedReason, OPERATION_WRITE);
      return;
    case STATUS_ACCEPTED:  // Fallthrough
    case STATUS_ACCEPT_SESSION:
      NotifyAccepted(aChannel);
      break;
    default:
      break;
  }

  int64_t serverTime = ParseServerTime(aServerTime);

  // process each cookie in the header
  while (SetCookieInternal(storage, aHostURI, baseDomain, aOriginAttrs,
                           requireHostMatch, cookieStatus, aCookieHeader,
                           serverTime, aFromHttp, aChannel)) {
    // document.cookie can only set one cookie at a time
    if (!aFromHttp) break;
  }
}

void nsCookieService::NotifyAccepted(nsIChannel* aChannel) {
  ContentBlockingNotifier::OnDecision(
      aChannel, ContentBlockingNotifier::BlockingDecision::eAllow, 0);
}

// notify observers that a cookie was rejected due to the users' prefs.
void nsCookieService::NotifyRejected(nsIURI* aHostURI, nsIChannel* aChannel,
                                     uint32_t aRejectedReason,
                                     CookieOperation aOperation) {
  if (aOperation == OPERATION_WRITE) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->NotifyObservers(aHostURI, "cookie-rejected", nullptr);
    }
  } else {
    MOZ_ASSERT(aOperation == OPERATION_READ);
  }

  ContentBlockingNotifier::OnDecision(
      aChannel, ContentBlockingNotifier::BlockingDecision::eBlock,
      aRejectedReason);
}

/******************************************************************************
 * nsCookieService:
 * public transaction helper impl
 ******************************************************************************/

NS_IMETHODIMP
nsCookieService::RunInTransaction(nsICookieTransactionCallback* aCallback) {
  NS_ENSURE_ARG(aCallback);

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  if (NS_WARN_IF(!mDefaultStorage->dbConn)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mozStorageTransaction transaction(mDefaultStorage->dbConn, true);

  if (NS_FAILED(aCallback->Callback())) {
    Unused << transaction.Rollback();
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/******************************************************************************
 * nsCookieService:
 * pref observer impl
 ******************************************************************************/

void nsCookieService::PrefChanged(nsIPrefBranch* aPrefBranch) {
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

/******************************************************************************
 * nsICookieManager impl:
 * nsICookieManager
 ******************************************************************************/

NS_IMETHODIMP
nsCookieService::RemoveAll() {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  mDefaultStorage->RemoveAll();
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookies(nsTArray<RefPtr<nsICookie>>& aCookies) {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  // We expose only non-private cookies.
  mDefaultStorage->GetCookies(aCookies);

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetSessionCookies(nsTArray<RefPtr<nsICookie>>& aCookies) {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  // We expose only non-private cookies.
  mDefaultStorage->GetCookies(aCookies);

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::Add(const nsACString& aHost, const nsACString& aPath,
                     const nsACString& aName, const nsACString& aValue,
                     bool aIsSecure, bool aIsHttpOnly, bool aIsSession,
                     int64_t aExpiry, JS::HandleValue aOriginAttributes,
                     int32_t aSameSite, JSContext* aCx) {
  OriginAttributes attrs;

  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return AddNative(aHost, aPath, aName, aValue, aIsSecure, aIsHttpOnly,
                   aIsSession, aExpiry, &attrs, aSameSite);
}

NS_IMETHODIMP_(nsresult)
nsCookieService::AddNative(const nsACString& aHost, const nsACString& aPath,
                           const nsACString& aName, const nsACString& aValue,
                           bool aIsSecure, bool aIsHttpOnly, bool aIsSession,
                           int64_t aExpiry, OriginAttributes* aOriginAttributes,
                           int32_t aSameSite) {
  if (NS_WARN_IF(!aOriginAttributes)) {
    return NS_ERROR_FAILURE;
  }

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the base domain for the host URI.
  // e.g. for "www.bbc.co.uk", this would be "bbc.co.uk".
  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t currentTimeInUsec = PR_Now();
  CookieKey key = CookieKey(baseDomain, *aOriginAttributes);

  RefPtr<Cookie> cookie = Cookie::Create(
      aName, aValue, host, aPath, aExpiry, currentTimeInUsec,
      Cookie::GenerateUniqueCreationTime(currentTimeInUsec), aIsSession,
      aIsSecure, aIsHttpOnly, key.mOriginAttributes, aSameSite, aSameSite);
  if (!cookie) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CookieStorage* storage = PickStorage(*aOriginAttributes);
  storage->AddCookie(baseDomain, *aOriginAttributes, cookie, currentTimeInUsec,
                     nullptr, VoidCString(), true, mMaxNumberOfCookies,
                     mMaxCookiesPerHost, mCookieQuotaPerHost, mCookiePurgeAge);
  return NS_OK;
}

nsresult nsCookieService::Remove(const nsACString& aHost,
                                 const OriginAttributes& aAttrs,
                                 const nsACString& aName,
                                 const nsACString& aPath) {
  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  if (!host.IsEmpty()) {
    rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CookieStorage* storage = PickStorage(aAttrs);
  storage->RemoveCookie(baseDomain, aAttrs, host, PromiseFlatCString(aName),
                        PromiseFlatCString(aPath));

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::Remove(const nsACString& aHost, const nsACString& aName,
                        const nsACString& aPath,
                        JS::HandleValue aOriginAttributes, JSContext* aCx) {
  OriginAttributes attrs;

  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  return RemoveNative(aHost, aName, aPath, &attrs);
}

NS_IMETHODIMP_(nsresult)
nsCookieService::RemoveNative(const nsACString& aHost, const nsACString& aName,
                              const nsACString& aPath,
                              OriginAttributes* aOriginAttributes) {
  if (NS_WARN_IF(!aOriginAttributes)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = Remove(aHost, *aOriginAttributes, aName, aPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private file I/O functions
 ******************************************************************************/

// Extract data from a single result row and create an Cookie.
mozilla::UniquePtr<CookieStruct> nsCookieService::GetCookieFromRow(
    mozIStorageStatement* aRow) {
  nsCString name, value, host, path;
  DebugOnly<nsresult> rv = aRow->GetUTF8String(IDX_NAME, name);
  NS_ASSERT_SUCCESS(rv);
  rv = aRow->GetUTF8String(IDX_VALUE, value);
  NS_ASSERT_SUCCESS(rv);
  rv = aRow->GetUTF8String(IDX_HOST, host);
  NS_ASSERT_SUCCESS(rv);
  rv = aRow->GetUTF8String(IDX_PATH, path);
  NS_ASSERT_SUCCESS(rv);

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

void nsCookieService::EnsureReadComplete(bool aInitDBConn) {
  MOZ_ASSERT(NS_IsMainThread());

  bool isAccumulated = false;

  if (!mInitializedCookieStorages) {
    TimeStamp startBlockTime = TimeStamp::Now();
    MonitorAutoLock lock(mMonitor);

    while (!mInitializedCookieStorages) {
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
  } else if (!mInitializedDBConn && aInitDBConn) {
    // A request comes while we finished cookie thread task and InitDBConn is
    // on the way from cookie thread to main thread. We're very close to block
    // main thread.
    Telemetry::Accumulate(
        Telemetry::MOZ_SQLITE_COOKIES_TIME_TO_BLOCK_MAIN_THREAD_MS, 0);
    isAccumulated = true;
  }

  if (!mInitializedDBConn && aInitDBConn && mDefaultStorage) {
    InitDBConn();
    if (isAccumulated) {
      // Nullify the timestamp so wo don't accumulate this telemetry probe
      // again.
      mEndInitDBConn = TimeStamp();
    }
  }
}

OpenDBResult nsCookieService::Read() {
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);

  // Read in the data synchronously.
  // see IDX_NAME, etc. for parameter indexes
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDefaultStorage->syncConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT "
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

    rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
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

NS_IMETHODIMP
nsCookieService::ImportCookies(nsIFile* aCookieFile) {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  return mDefaultStorage->ImportCookies(aCookieFile, mTLDService,
                                        mMaxNumberOfCookies, mMaxCookiesPerHost,
                                        mCookieQuotaPerHost, mCookiePurgeAge);
}

/******************************************************************************
 * nsCookieService impl:
 * private GetCookie/SetCookie helpers
 ******************************************************************************/

bool nsCookieService::DomainMatches(Cookie* aCookie, const nsACString& aHost) {
  // first, check for an exact host or domain cookie match, e.g. "google.com"
  // or ".google.com"; second a subdomain match, e.g.
  // host = "mail.google.com", cookie domain = ".google.com".
  return aCookie->RawHost() == aHost ||
         (aCookie->IsDomain() && StringEndsWith(aHost, aCookie->Host()));
}

bool nsCookieService::PathMatches(Cookie* aCookie, const nsACString& aPath) {
  nsCString cookiePath(aCookie->GetFilePath());

  // if our cookie path is empty we can't really perform our prefix check, and
  // also we can't check the last character of the cookie path, so we would
  // never return a successful match.
  if (cookiePath.IsEmpty()) return false;

  // if the cookie path and the request path are identical, they match.
  if (cookiePath.Equals(aPath)) return true;

  // if the cookie path is a prefix of the request path, and the last character
  // of the cookie path is %x2F ("/"), they match.
  bool isPrefix = StringBeginsWith(aPath, cookiePath);
  if (isPrefix && cookiePath.Last() == '/') return true;

  // if the cookie path is a prefix of the request path, and the first character
  // of the request path that is not included in the cookie path is a %x2F ("/")
  // character, they match.
  uint32_t cookiePathLen = cookiePath.Length();
  if (isPrefix && aPath[cookiePathLen] == '/') return true;

  return false;
}

void nsCookieService::GetCookiesForURI(
    nsIURI* aHostURI, nsIChannel* aChannel, bool aIsForeign,
    bool aIsThirdPartyTrackingResource,
    bool aIsThirdPartySocialTrackingResource,
    bool aFirstPartyStorageAccessGranted, uint32_t aRejectedReason,
    bool aIsSafeTopLevelNav, bool aIsSameSiteForeign, bool aHttpBound,
    const OriginAttributes& aOriginAttrs, nsTArray<Cookie*>& aCookieList) {
  NS_ASSERTION(aHostURI, "null host!");

  if (!IsInitialized()) {
    return;
  }

  CookieStorage* storage = PickStorage(aOriginAttrs);

  // get the base domain, host, and path from the URI.
  // e.g. for "www.bbc.co.uk", the base domain would be "bbc.co.uk".
  // file:// URI's (i.e. with an empty host) are allowed, but any other
  // scheme must have a non-empty host. A trailing dot in the host
  // is acceptable.
  bool requireHostMatch;
  nsAutoCString baseDomain, hostFromURI, pathFromURI;
  nsresult rv =
      GetBaseDomain(mTLDService, aHostURI, baseDomain, requireHostMatch);
  if (NS_SUCCEEDED(rv)) rv = aHostURI->GetAsciiHost(hostFromURI);
  if (NS_SUCCEEDED(rv)) rv = aHostURI->GetFilePath(pathFromURI);
  if (NS_FAILED(rv)) {
    COOKIE_LOGFAILURE(GET_COOKIE, aHostURI, VoidCString(),
                      "invalid host/path from URI");
    return;
  }

  nsCOMPtr<nsICookieJarSettings> cookieJarSettings =
      GetCookieJarSettings(aChannel);

  nsAutoCString normalizedHostFromURI(hostFromURI);
  rv = NormalizeHost(normalizedHostFromURI);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsAutoCString baseDomainFromURI;
  rv = GetBaseDomainFromHost(mTLDService, normalizedHostFromURI,
                             baseDomainFromURI);
  NS_ENSURE_SUCCESS_VOID(rv);

  // check default prefs
  uint32_t rejectedReason = aRejectedReason;
  uint32_t priorCookieCount = storage->CountCookiesFromHost(
      baseDomainFromURI, aOriginAttrs.mPrivateBrowsingId);

  CookieStatus cookieStatus = CheckPrefs(
      cookieJarSettings, aHostURI, aIsForeign, aIsThirdPartyTrackingResource,
      aIsThirdPartySocialTrackingResource, aFirstPartyStorageAccessGranted,
      VoidCString(), priorCookieCount, aOriginAttrs, &rejectedReason);

  MOZ_ASSERT_IF(rejectedReason, cookieStatus == STATUS_REJECTED);

  // for GetCookie(), we only fire acceptance/rejection notifications
  // (but not if there was an error)
  switch (cookieStatus) {
    case STATUS_REJECTED:
      // If we don't have any cookies from this host, fail silently.
      if (priorCookieCount) {
        NotifyRejected(aHostURI, aChannel, rejectedReason, OPERATION_READ);
      }
      return;
    default:
      break;
  }

  // Note: The following permissions logic is mirrored in
  // extensions::MatchPattern::MatchesCookie.
  // If it changes, please update that function, or file a bug for someone
  // else to do so.

  // check if aHostURI is using an https secure protocol.
  // if it isn't, then we can't send a secure cookie over the connection.
  // if SchemeIs fails, assume an insecure connection, to be on the safe side
  bool potentiallyTurstworthy =
      nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(aHostURI);

  int64_t currentTimeInUsec = PR_Now();
  int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;
  bool stale = false;

  const nsTArray<RefPtr<Cookie>>* cookies =
      storage->GetCookiesFromHost(baseDomain, aOriginAttrs);
  if (!cookies) return;

  bool laxByDefault =
      StaticPrefs::network_cookie_sameSite_laxByDefault() &&
      !nsContentUtils::IsURIInPrefList(
          aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");

  // iterate the cookies!
  for (Cookie* cookie : *cookies) {
    // check the host, since the base domain lookup is conservative.
    if (!DomainMatches(cookie, hostFromURI)) continue;

    // if the cookie is secure and the host scheme isn't, we can't send it
    if (cookie->IsSecure() && !potentiallyTurstworthy) continue;

    if (aIsSameSiteForeign &&
        !ProcessSameSiteCookieForForeignRequest(
            aChannel, cookie, aIsSafeTopLevelNav, laxByDefault)) {
      continue;
    }

    // if the cookie is httpOnly and it's not going directly to the HTTP
    // connection, don't send it
    if (cookie->IsHttpOnly() && !aHttpBound) continue;

    // if the nsIURI path doesn't match the cookie path, don't send it back
    if (!PathMatches(cookie, pathFromURI)) continue;

    // check if the cookie has expired
    if (cookie->Expiry() <= currentTime) {
      continue;
    }

    // all checks passed - add to list and check if lastAccessed stamp needs
    // updating
    aCookieList.AppendElement(cookie);
    if (cookie->IsStale()) {
      stale = true;
    }
  }

  if (aCookieList.IsEmpty()) {
    return;
  }

  // Send a notification about the acceptance of the cookies now that we found
  // some.
  NotifyAccepted(aChannel);

  // update lastAccessed timestamps. we only do this if the timestamp is stale
  // by a certain amount, to avoid thrashing the db during pageload.
  if (stale) {
    storage->StaleCookies(aCookieList, currentTimeInUsec);
  }

  // return cookies in order of path length; longest to shortest.
  // this is required per RFC2109.  if cookies match in length,
  // then sort by creation time (see bug 236772).
  aCookieList.Sort(CompareCookiesForSending());
}

void nsCookieService::GetCookieStringInternal(
    nsIURI* aHostURI, nsIChannel* aChannel, bool aIsForeign,
    bool aIsThirdPartyTrackingResource,
    bool aIsThirdPartySocialTrackingResource,
    bool aFirstPartyStorageAccessGranted, uint32_t aRejectedReason,
    bool aIsSafeTopLevelNav, bool aIsSameSiteForeign, bool aHttpBound,
    const OriginAttributes& aOriginAttrs, nsACString& aCookieString) {
  AutoTArray<Cookie*, 8> foundCookieList;
  GetCookiesForURI(
      aHostURI, aChannel, aIsForeign, aIsThirdPartyTrackingResource,
      aIsThirdPartySocialTrackingResource, aFirstPartyStorageAccessGranted,
      aRejectedReason, aIsSafeTopLevelNav, aIsSameSiteForeign, aHttpBound,
      aOriginAttrs, foundCookieList);

  Cookie* cookie;
  for (uint32_t i = 0; i < foundCookieList.Length(); ++i) {
    cookie = foundCookieList.ElementAt(i);

    // check if we have anything to write
    if (!cookie->Name().IsEmpty() || !cookie->Value().IsEmpty()) {
      // if we've already added a cookie to the return list, append a "; " so
      // that subsequent cookies are delimited in the final list.
      if (!aCookieString.IsEmpty()) {
        aCookieString.AppendLiteral("; ");
      }

      if (!cookie->Name().IsEmpty()) {
        // we have a name and value - write both
        aCookieString +=
            cookie->Name() + NS_LITERAL_CSTRING("=") + cookie->Value();
      } else {
        // just write value
        aCookieString += cookie->Value();
      }
    }
  }

  if (!aCookieString.IsEmpty())
    COOKIE_LOGSUCCESS(GET_COOKIE, aHostURI, aCookieString, nullptr, false);
}

// processes a single cookie, and returns true if there are more cookies
// to be processed
bool nsCookieService::CanSetCookie(
    nsIURI* aHostURI, const nsACString& aBaseDomain, CookieStruct& aCookieData,
    bool aRequireHostMatch, CookieStatus aStatus, nsCString& aCookieHeader,
    int64_t aServerTime, bool aFromHttp, nsIChannel* aChannel, bool& aSetCookie,
    mozIThirdPartyUtil* aThirdPartyUtil) {
  NS_ASSERTION(aHostURI, "null host!");

  aSetCookie = false;

  // init expiryTime such that session cookies won't prematurely expire
  aCookieData.expiry() = INT64_MAX;

  // aCookieHeader is an in/out param to point to the next cookie, if
  // there is one. Save the present value for logging purposes
  nsCString savedCookieHeader(aCookieHeader);

  // newCookie says whether there are multiple cookies in the header;
  // so we can handle them separately.
  nsAutoCString expires;
  nsAutoCString maxage;
  bool acceptedByParser = false;
  bool newCookie =
      ParseAttributes(aChannel, aHostURI, aCookieHeader, aCookieData, expires,
                      maxage, acceptedByParser);
  if (!acceptedByParser) {
    return newCookie;
  }

  // Collect telemetry on how often secure cookies are set from non-secure
  // origins, and vice-versa.
  //
  // 0 = nonsecure and "http:"
  // 1 = nonsecure and "https:"
  // 2 = secure and "http:"
  // 3 = secure and "https:"
  bool potentiallyTurstworthy =
      nsMixedContentBlocker::IsPotentiallyTrustworthyOrigin(aHostURI);

  int64_t currentTimeInUsec = PR_Now();

  // calculate expiry time of cookie.
  aCookieData.isSession() =
      GetExpiry(aCookieData, expires, maxage, aServerTime,
                currentTimeInUsec / PR_USEC_PER_SEC, aFromHttp);
  if (aStatus == STATUS_ACCEPT_SESSION) {
    // force lifetime to session. note that the expiration time, if set above,
    // will still apply.
    aCookieData.isSession() = true;
  }

  // reject cookie if it's over the size limit, per RFC2109
  if ((aCookieData.name().Length() + aCookieData.value().Length()) >
      kMaxBytesPerCookie) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "cookie too big (> 4kb)");

    AutoTArray<nsString, 2> params = {
        NS_ConvertUTF8toUTF16(aCookieData.name())};

    nsString size;
    size.AppendInt(kMaxBytesPerCookie);
    params.AppendElement(size);

    LogMessageToConsole(aChannel, aHostURI, nsIScriptError::warningFlag,
                        CONSOLE_GENERIC_CATEGORY,
                        NS_LITERAL_CSTRING("CookieOversize"), params);
    return newCookie;
  }

  const char illegalNameCharacters[] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
      0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
      0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x00};

  if (aCookieData.name().FindCharInSet(illegalNameCharacters, 0) != -1) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "invalid name character");
    return newCookie;
  }

  // domain & path checks
  if (!CheckDomain(aCookieData, aHostURI, aBaseDomain, aRequireHostMatch)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the domain tests");
    return newCookie;
  }

  if (!CheckPath(aCookieData, aChannel, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the path tests");
    return newCookie;
  }

  // magic prefix checks. MUST be run after CheckDomain() and CheckPath()
  if (!CheckPrefixes(aCookieData, potentiallyTurstworthy)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "failed the prefix tests");
    return newCookie;
  }

  // reject cookie if value contains an RFC 6265 disallowed character - see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1191423
  // NOTE: this is not the full set of characters disallowed by 6265 - notably
  // 0x09, 0x20, 0x22, 0x2C, 0x5C, and 0x7F are missing from this list. This is
  // for parity with Chrome. This only applies to cookies set via the Set-Cookie
  // header, as document.cookie is defined to be UTF-8. Hooray for
  // symmetry!</sarcasm>
  const char illegalCharacters[] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C,
      0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x3B, 0x00};
  if (aFromHttp &&
      (aCookieData.value().FindCharInSet(illegalCharacters, 0) != -1)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "invalid value character");
    return newCookie;
  }

  // if the new cookie is httponly, make sure we're not coming from script
  if (!aFromHttp && aCookieData.isHttpOnly()) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                      "cookie is httponly; coming from script");
    return newCookie;
  }

  // If the new cookie is non-https and wants to set secure flag,
  // browser have to ignore this new cookie.
  // (draft-ietf-httpbis-cookie-alone section 3.1)
  if (aCookieData.isSecure() && !potentiallyTurstworthy) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                      "non-https cookie can't set secure flag");
    return newCookie;
  }

  // If the new cookie is same-site but in a cross site context,
  // browser must ignore the cookie.
  if ((aCookieData.sameSite() != nsICookie::SAMESITE_NONE) && aThirdPartyUtil) {
    // Do not treat loads triggered by web extensions as foreign
    bool addonAllowsLoad = false;
    if (aChannel) {
      nsCOMPtr<nsIURI> channelURI;
      NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
      nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
      addonAllowsLoad = BasePrincipal::Cast(loadInfo->TriggeringPrincipal())
                            ->AddonAllowsLoad(channelURI);
    }

    if (!addonAllowsLoad) {
      bool isThirdParty = false;
      nsresult rv = aThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI,
                                                         &isThirdParty);
      if (NS_FAILED(rv) || isThirdParty) {
        COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                          "failed the samesite tests");
        return newCookie;
      }
    }
  }

  aSetCookie = true;
  return newCookie;
}

// processes a single cookie, and returns true if there are more cookies
// to be processed
bool nsCookieService::SetCookieInternal(
    CookieStorage* aStorage, nsIURI* aHostURI, const nsACString& aBaseDomain,
    const OriginAttributes& aOriginAttributes, bool aRequireHostMatch,
    CookieStatus aStatus, nsCString& aCookieHeader, int64_t aServerTime,
    bool aFromHttp, nsIChannel* aChannel) {
  NS_ASSERTION(aHostURI, "null host!");
  bool canSetCookie = false;
  nsCString savedCookieHeader(aCookieHeader);
  CookieStruct cookieData;
  bool newCookie =
      CanSetCookie(aHostURI, aBaseDomain, cookieData, aRequireHostMatch,
                   aStatus, aCookieHeader, aServerTime, aFromHttp, aChannel,
                   canSetCookie, mThirdPartyUtil);

  if (!canSetCookie) {
    return newCookie;
  }

  int64_t currentTimeInUsec = PR_Now();
  // create a new Cookie and copy attributes
  RefPtr<Cookie> cookie = Cookie::Create(
      cookieData.name(), cookieData.value(), cookieData.host(),
      cookieData.path(), cookieData.expiry(), currentTimeInUsec,
      Cookie::GenerateUniqueCreationTime(currentTimeInUsec),
      cookieData.isSession(), cookieData.isSecure(), cookieData.isHttpOnly(),
      aOriginAttributes, cookieData.sameSite(), cookieData.rawSameSite());
  if (!cookie) return newCookie;

  // check permissions from site permission list, or ask the user,
  // to determine if we can set the cookie
  if (mPermissionService) {
    bool permission;
    mPermissionService->CanSetCookie(
        aHostURI, aChannel,
        static_cast<nsICookie*>(static_cast<Cookie*>(cookie)),
        &cookieData.isSession(), &cookieData.expiry(), &permission);
    if (!permission) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
                        "cookie rejected by permission manager");
      NotifyRejected(
          aHostURI, aChannel,
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION,
          OPERATION_WRITE);
      return newCookie;
    }

    // update isSession and expiry attributes, in case they changed
    cookie->SetIsSession(cookieData.isSession());
    cookie->SetExpiry(cookieData.expiry());
  }

  // add the cookie to the list. AddCookie() takes care of logging.
  // we get the current time again here, since it may have changed during
  // prompting
  aStorage->AddCookie(aBaseDomain, aOriginAttributes, cookie, PR_Now(),
                      aHostURI, savedCookieHeader, aFromHttp,
                      mMaxNumberOfCookies, mMaxCookiesPerHost,
                      mCookieQuotaPerHost, mCookiePurgeAge);
  return newCookie;
}

/******************************************************************************
 * nsCookieService impl:
 * private cookie header parsing functions
 ******************************************************************************/

// clang-format off
// The following comment block elucidates the function of ParseAttributes.
/******************************************************************************
 ** Augmented BNF, modified from RFC2109 Section 4.2.2 and RFC2616 Section 2.1
 ** please note: this BNF deviates from both specifications, and reflects this
 ** implementation. <bnf> indicates a reference to the defined grammar "bnf".

 ** Differences from RFC2109/2616 and explanations:
    1. implied *LWS
         The grammar described by this specification is word-based. Except
         where noted otherwise, linear white space (<LWS>) can be included
         between any two adjacent words (token or quoted-string), and
         between adjacent words and separators, without changing the
         interpretation of a field.
       <LWS> according to spec is SP|HT|CR|LF, but here, we allow only SP | HT.

    2. We use CR | LF as cookie separators, not ',' per spec, since ',' is in
       common use inside values.

    3. tokens and values have looser restrictions on allowed characters than
       spec. This is also due to certain characters being in common use inside
       values. We allow only '=' to separate token/value pairs, and ';' to
       terminate tokens or values. <LWS> is allowed within tokens and values
       (see bug 206022).

    4. where appropriate, full <OCTET>s are allowed, where the spec dictates to
       reject control chars or non-ASCII chars. This is erring on the loose
       side, since there's probably no good reason to enforce this strictness.

    5. Attribute "HttpOnly", not covered in the RFCs, is supported
       (see bug 178993).

 ** Begin BNF:
    token         = 1*<any allowed-chars except separators>
    value         = 1*<any allowed-chars except value-sep>
    separators    = ";" | "="
    value-sep     = ";"
    cookie-sep    = CR | LF
    allowed-chars = <any OCTET except NUL or cookie-sep>
    OCTET         = <any 8-bit sequence of data>
    LWS           = SP | HT
    NUL           = <US-ASCII NUL, null control character (0)>
    CR            = <US-ASCII CR, carriage return (13)>
    LF            = <US-ASCII LF, linefeed (10)>
    SP            = <US-ASCII SP, space (32)>
    HT            = <US-ASCII HT, horizontal-tab (9)>

    set-cookie    = "Set-Cookie:" cookies
    cookies       = cookie *( cookie-sep cookie )
    cookie        = [NAME "="] VALUE *(";" cookie-av)    ; cookie NAME/VALUE must come first
    NAME          = token                                ; cookie name
    VALUE         = value                                ; cookie value
    cookie-av     = token ["=" value]

    valid values for cookie-av (checked post-parsing) are:
    cookie-av     = "Path"    "=" value
                  | "Domain"  "=" value
                  | "Expires" "=" value
                  | "Max-Age" "=" value
                  | "Comment" "=" value
                  | "Version" "=" value
                  | "Secure"
                  | "HttpOnly"

******************************************************************************/
// clang-format on

// helper functions for GetTokenValue
static inline bool isnull(char c) { return c == 0; }
static inline bool iswhitespace(char c) { return c == ' ' || c == '\t'; }
static inline bool isterminator(char c) { return c == '\n' || c == '\r'; }
static inline bool isvalueseparator(char c) {
  return isterminator(c) || c == ';';
}
static inline bool istokenseparator(char c) {
  return isvalueseparator(c) || c == '=';
}

// Parse a single token/value pair.
// Returns true if a cookie terminator is found, so caller can parse new cookie.
bool nsCookieService::GetTokenValue(nsACString::const_char_iterator& aIter,
                                    nsACString::const_char_iterator& aEndIter,
                                    nsDependentCSubstring& aTokenString,
                                    nsDependentCSubstring& aTokenValue,
                                    bool& aEqualsFound) {
  nsACString::const_char_iterator start, lastSpace;
  // initialize value string to clear garbage
  aTokenValue.Rebind(aIter, aIter);

  // find <token>, including any <LWS> between the end-of-token and the
  // token separator. we'll remove trailing <LWS> next
  while (aIter != aEndIter && iswhitespace(*aIter)) ++aIter;
  start = aIter;
  while (aIter != aEndIter && !isnull(*aIter) && !istokenseparator(*aIter))
    ++aIter;

  // remove trailing <LWS>; first check we're not at the beginning
  lastSpace = aIter;
  if (lastSpace != start) {
    while (--lastSpace != start && iswhitespace(*lastSpace)) continue;
    ++lastSpace;
  }
  aTokenString.Rebind(start, lastSpace);

  aEqualsFound = (*aIter == '=');
  if (aEqualsFound) {
    // find <value>
    while (++aIter != aEndIter && iswhitespace(*aIter)) continue;

    start = aIter;

    // process <token>
    // just look for ';' to terminate ('=' allowed)
    while (aIter != aEndIter && !isnull(*aIter) && !isvalueseparator(*aIter))
      ++aIter;

    // remove trailing <LWS>; first check we're not at the beginning
    if (aIter != start) {
      lastSpace = aIter;
      while (--lastSpace != start && iswhitespace(*lastSpace)) continue;
      aTokenValue.Rebind(start, ++lastSpace);
    }
  }

  // aIter is on ';', or terminator, or EOS
  if (aIter != aEndIter) {
    // if on terminator, increment past & return true to process new cookie
    if (isterminator(*aIter)) {
      ++aIter;
      return true;
    }
    // fall-through: aIter is on ';', increment and return false
    ++aIter;
  }
  return false;
}

// Parses attributes from cookie header. expires/max-age attributes aren't
// folded into the cookie struct here, because we don't know which one to use
// until we've parsed the header.
bool nsCookieService::ParseAttributes(nsIChannel* aChannel, nsIURI* aHostURI,
                                      nsCString& aCookieHeader,
                                      mozilla::net::CookieStruct& aCookieData,
                                      nsACString& aExpires, nsACString& aMaxage,
                                      bool& aAcceptedByParser) {
  aAcceptedByParser = false;

  static const char kPath[] = "path";
  static const char kDomain[] = "domain";
  static const char kExpires[] = "expires";
  static const char kMaxage[] = "max-age";
  static const char kSecure[] = "secure";
  static const char kHttpOnly[] = "httponly";
  static const char kSameSite[] = "samesite";
  static const char kSameSiteLax[] = "lax";
  static const char kSameSiteNone[] = "none";
  static const char kSameSiteStrict[] = "strict";

  nsACString::const_char_iterator tempBegin, tempEnd;
  nsACString::const_char_iterator cookieStart, cookieEnd;
  aCookieHeader.BeginReading(cookieStart);
  aCookieHeader.EndReading(cookieEnd);

  aCookieData.isSecure() = false;
  aCookieData.isHttpOnly() = false;
  aCookieData.sameSite() = nsICookie::SAMESITE_NONE;
  aCookieData.rawSameSite() = nsICookie::SAMESITE_NONE;

  bool laxByDefault =
      StaticPrefs::network_cookie_sameSite_laxByDefault() &&
      !nsContentUtils::IsURIInPrefList(
          aHostURI, "network.cookie.sameSite.laxByDefault.disabledHosts");

  if (laxByDefault) {
    aCookieData.sameSite() = nsICookie::SAMESITE_LAX;
  }

  nsDependentCSubstring tokenString(cookieStart, cookieStart);
  nsDependentCSubstring tokenValue(cookieStart, cookieStart);
  bool newCookie, equalsFound;

  // extract cookie <NAME> & <VALUE> (first attribute), and copy the strings.
  // if we find multiple cookies, return for processing
  // note: if there's no '=', we assume token is <VALUE>. this is required by
  //       some sites (see bug 169091).
  // XXX fix the parser to parse according to <VALUE> grammar for this case
  newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue,
                            equalsFound);
  if (equalsFound) {
    aCookieData.name() = tokenString;
    aCookieData.value() = tokenValue;
  } else {
    aCookieData.value() = tokenString;
  }

  bool sameSiteSet = false;

  // extract remaining attributes
  while (cookieStart != cookieEnd && !newCookie) {
    newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue,
                              equalsFound);

    if (!tokenValue.IsEmpty()) {
      tokenValue.BeginReading(tempBegin);
      tokenValue.EndReading(tempEnd);
    }

    // decide which attribute we have, and copy the string
    if (tokenString.LowerCaseEqualsLiteral(kPath))
      aCookieData.path() = tokenValue;

    else if (tokenString.LowerCaseEqualsLiteral(kDomain))
      aCookieData.host() = tokenValue;

    else if (tokenString.LowerCaseEqualsLiteral(kExpires))
      aExpires = tokenValue;

    else if (tokenString.LowerCaseEqualsLiteral(kMaxage))
      aMaxage = tokenValue;

    // ignore any tokenValue for isSecure; just set the boolean
    else if (tokenString.LowerCaseEqualsLiteral(kSecure))
      aCookieData.isSecure() = true;

    // ignore any tokenValue for isHttpOnly (see bug 178993);
    // just set the boolean
    else if (tokenString.LowerCaseEqualsLiteral(kHttpOnly))
      aCookieData.isHttpOnly() = true;

    else if (tokenString.LowerCaseEqualsLiteral(kSameSite)) {
      if (tokenValue.LowerCaseEqualsLiteral(kSameSiteLax)) {
        aCookieData.sameSite() = nsICookie::SAMESITE_LAX;
        aCookieData.rawSameSite() = nsICookie::SAMESITE_LAX;
        sameSiteSet = true;
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteStrict)) {
        aCookieData.sameSite() = nsICookie::SAMESITE_STRICT;
        aCookieData.rawSameSite() = nsICookie::SAMESITE_STRICT;
        sameSiteSet = true;
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteNone)) {
        aCookieData.sameSite() = nsICookie::SAMESITE_NONE;
        aCookieData.rawSameSite() = nsICookie::SAMESITE_NONE;
        sameSiteSet = true;
      } else {
        LogMessageToConsole(
            aChannel, aHostURI, nsIScriptError::infoFlag,
            CONSOLE_GENERIC_CATEGORY,
            NS_LITERAL_CSTRING("CookieSameSiteValueInvalid"),
            AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
      }
    }
  }

  Telemetry::Accumulate(Telemetry::COOKIE_SAMESITE_SET_VS_UNSET,
                        sameSiteSet ? 1 : 0);

  // re-assign aCookieHeader, in case we need to process another cookie
  aCookieHeader.Assign(Substring(cookieStart, cookieEnd));

  // If same-site is set to 'none' but this is not a secure context, let's abort
  // the parsing.
  if (!aCookieData.isSecure() &&
      aCookieData.sameSite() == nsICookie::SAMESITE_NONE) {
    if (laxByDefault &&
        StaticPrefs::network_cookie_sameSite_noneRequiresSecure()) {
      LogMessageToConsole(
          aChannel, aHostURI, nsIScriptError::infoFlag,
          CONSOLE_SAMESITE_CATEGORY,
          NS_LITERAL_CSTRING("CookieRejectedNonRequiresSecure"),
          AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
      return newCookie;
    }

    // if sameSite=lax by default is disabled, we want to warn the user.
    LogMessageToConsole(
        aChannel, aHostURI, nsIScriptError::warningFlag,
        CONSOLE_SAMESITE_CATEGORY,
        NS_LITERAL_CSTRING("CookieRejectedNonRequiresSecureForBeta"),
        AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(aCookieData.name()),
                                SAMESITE_MDN_URL});
  }

  if (aCookieData.rawSameSite() == nsICookie::SAMESITE_NONE &&
      aCookieData.sameSite() == nsICookie::SAMESITE_LAX) {
    if (laxByDefault) {
      LogMessageToConsole(
          aChannel, aHostURI, nsIScriptError::infoFlag,
          CONSOLE_SAMESITE_CATEGORY, NS_LITERAL_CSTRING("CookieLaxForced"),
          AutoTArray<nsString, 1>{NS_ConvertUTF8toUTF16(aCookieData.name())});
    } else {
      LogMessageToConsole(
          aChannel, aHostURI, nsIScriptError::warningFlag,
          CONSOLE_SAMESITE_CATEGORY,
          NS_LITERAL_CSTRING("CookieLaxForcedForBeta"),
          AutoTArray<nsString, 2>{NS_ConvertUTF8toUTF16(aCookieData.name()),
                                  SAMESITE_MDN_URL});
    }
  }

  // Cookie accepted.
  aAcceptedByParser = true;

  MOZ_ASSERT(Cookie::ValidateRawSame(aCookieData));
  return newCookie;
}

// static
void nsCookieService::LogMessageToConsole(nsIChannel* aChannel, nsIURI* aURI,
                                          uint32_t aErrorFlags,
                                          const nsACString& aCategory,
                                          const nsACString& aMsg,
                                          const nsTArray<nsString>& aParams) {
  MOZ_ASSERT(aURI);

  nsCOMPtr<HttpBaseChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    return;
  }

  nsAutoCString uri;
  nsresult rv = aURI->GetSpec(uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  httpChannel->AddConsoleReport(aErrorFlags, aCategory,
                                nsContentUtils::eNECKO_PROPERTIES, uri, 0, 0,
                                aMsg, aParams);
}

/******************************************************************************
 * nsCookieService impl:
 * private domain & permission compliance enforcement functions
 ******************************************************************************/

// Get the base domain for aHostURI; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". Only properly-formed URI's are tolerated, though a trailing
// dot may be present. If aHostURI is an IP address, an alias such as
// 'localhost', an eTLD such as 'co.uk', or the empty string, aBaseDomain will
// be the exact host, and aRequireHostMatch will be true to indicate that
// substring matches should not be performed.
nsresult nsCookieService::GetBaseDomain(nsIEffectiveTLDService* aTLDService,
                                        nsIURI* aHostURI,
                                        nsCString& aBaseDomain,
                                        bool& aRequireHostMatch) {
  // get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = aTLDService->GetBaseDomain(aHostURI, 0, aBaseDomain);
  aRequireHostMatch = rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
                      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS;
  if (aRequireHostMatch) {
    // aHostURI is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. use the host as a key in such
    // cases.
    rv = aHostURI->GetAsciiHost(aBaseDomain);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // aHost (and thus aBaseDomain) may be the string '.'. If so, fail.
  if (aBaseDomain.Length() == 1 && aBaseDomain.Last() == '.')
    return NS_ERROR_INVALID_ARG;

  // block any URIs without a host that aren't file:// URIs.
  if (aBaseDomain.IsEmpty() && !aHostURI->SchemeIs("file")) {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

// Get the base domain for aHost; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". This is done differently than GetBaseDomain(mTLDService, ): it
// is assumed that aHost is already normalized, and it may contain a leading dot
// (indicating that it represents a domain). A trailing dot may be present.
// If aHost is an IP address, an alias such as 'localhost', an eTLD such as
// 'co.uk', or the empty string, aBaseDomain will be the exact host, and a
// leading dot will be treated as an error.
nsresult nsCookieService::GetBaseDomainFromHost(
    nsIEffectiveTLDService* aTLDService, const nsACString& aHost,
    nsCString& aBaseDomain) {
  // aHost must not be the string '.'.
  if (aHost.Length() == 1 && aHost.Last() == '.') return NS_ERROR_INVALID_ARG;

  // aHost may contain a leading dot; if so, strip it now.
  bool domain = !aHost.IsEmpty() && aHost.First() == '.';

  // get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = aTLDService->GetBaseDomainFromHost(Substring(aHost, domain), 0,
                                                   aBaseDomain);
  if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
    // aHost is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. use the host as a key in such
    // cases; however, we reject any such hosts with a leading dot, since it
    // doesn't make sense for them to be domain cookies.
    if (domain) return NS_ERROR_INVALID_ARG;

    aBaseDomain = aHost;
    return NS_OK;
  }
  return rv;
}

// Normalizes the given hostname, component by component. ASCII/ACE
// components are lower-cased, and UTF-8 components are normalized per
// RFC 3454 and converted to ACE.
nsresult nsCookieService::NormalizeHost(nsCString& aHost) {
  if (!IsAscii(aHost)) {
    nsAutoCString host;
    nsresult rv = mIDNService->ConvertUTF8toACE(aHost, host);
    if (NS_FAILED(rv)) return rv;

    aHost = host;
  }

  ToLowerCase(aHost);
  return NS_OK;
}

// returns true if 'a' is equal to or a subdomain of 'b',
// assuming no leading dots are present.
static inline bool IsSubdomainOf(const nsACString& a, const nsACString& b) {
  if (a == b) return true;
  if (a.Length() > b.Length())
    return a[a.Length() - b.Length() - 1] == '.' && StringEndsWith(a, b);
  return false;
}

CookieStatus nsCookieService::CheckPrefs(
    nsICookieJarSettings* aCookieJarSettings, nsIURI* aHostURI, bool aIsForeign,
    bool aIsThirdPartyTrackingResource,
    bool aIsThirdPartySocialTrackingResource,
    bool aFirstPartyStorageAccessGranted, const nsACString& aCookieHeader,
    const int aNumOfCookies, const OriginAttributes& aOriginAttrs,
    uint32_t* aRejectedReason) {
  nsresult rv;

  MOZ_ASSERT(aRejectedReason);

  *aRejectedReason = 0;

  // don't let ftp sites get/set cookies (could be a security issue)
  if (aHostURI->SchemeIs("ftp")) {
    COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                      aHostURI, aCookieHeader, "ftp sites cannot read cookies");
    return STATUS_REJECTED_WITH_ERROR;
  }

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(aHostURI, aOriginAttrs);

  if (!principal) {
    COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                      aHostURI, aCookieHeader,
                      "non-content principals cannot get/set cookies");
    return STATUS_REJECTED_WITH_ERROR;
  }

  // check the permission list first; if we find an entry, it overrides
  // default prefs. see bug 184059.
  uint32_t cookiePermission = nsICookiePermission::ACCESS_DEFAULT;
  rv = aCookieJarSettings->CookiePermission(principal, &cookiePermission);
  if (NS_SUCCEEDED(rv)) {
    switch (cookiePermission) {
      case nsICookiePermission::ACCESS_DENY:
        COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                          aHostURI, aCookieHeader,
                          "cookies are blocked for this site");
        *aRejectedReason =
            nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION;
        return STATUS_REJECTED;

      case nsICookiePermission::ACCESS_ALLOW:
        return STATUS_ACCEPTED;
    }
  }

  // No cookies allowed if this request comes from a resource in a 3rd party
  // context, when anti-tracking protection is enabled and when we don't have
  // access to the first-party cookie jar.
  if (aIsForeign && aIsThirdPartyTrackingResource &&
      !aFirstPartyStorageAccessGranted &&
      aCookieJarSettings->GetRejectThirdPartyContexts()) {
    bool rejectThirdPartyWithExceptions =
        CookieJarSettings::IsRejectThirdPartyWithExceptions(
            aCookieJarSettings->GetCookieBehavior());

    uint32_t rejectReason =
        rejectThirdPartyWithExceptions
            ? nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN
            : nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    if (StoragePartitioningEnabled(rejectReason, aCookieJarSettings)) {
      MOZ_ASSERT(!aOriginAttrs.mFirstPartyDomain.IsEmpty(),
                 "We must have a StoragePrincipal here!");
      return STATUS_ACCEPTED;
    }

    COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                      aHostURI, aCookieHeader,
                      "cookies are disabled in trackers");
    if (aIsThirdPartySocialTrackingResource) {
      *aRejectedReason =
          nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER;
    } else if (rejectThirdPartyWithExceptions) {
      *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
    } else {
      *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER;
    }
    return STATUS_REJECTED;
  }

  // check default prefs.
  // Check aFirstPartyStorageAccessGranted when checking aCookieBehavior
  // so that we take things such as the content blocking allow list into
  // account.
  if (aCookieJarSettings->GetCookieBehavior() ==
          nsICookieService::BEHAVIOR_REJECT &&
      !aFirstPartyStorageAccessGranted) {
    COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                      aHostURI, aCookieHeader, "cookies are disabled");
    *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL;
    return STATUS_REJECTED;
  }

  // check if cookie is foreign
  if (aIsForeign) {
    if (aCookieJarSettings->GetCookieBehavior() ==
            nsICookieService::BEHAVIOR_REJECT_FOREIGN &&
        !aFirstPartyStorageAccessGranted) {
      COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                        aHostURI, aCookieHeader, "context is third party");
      *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
      return STATUS_REJECTED;
    }

    if (aCookieJarSettings->GetLimitForeignContexts() &&
        !aFirstPartyStorageAccessGranted && aNumOfCookies == 0) {
      COOKIE_LOGFAILURE(aCookieHeader.IsVoid() ? GET_COOKIE : SET_COOKIE,
                        aHostURI, aCookieHeader, "context is third party");
      *aRejectedReason = nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN;
      return STATUS_REJECTED;
    }

    if (StaticPrefs::network_cookie_thirdparty_sessionOnly()) {
      return STATUS_ACCEPT_SESSION;
    }

    if (StaticPrefs::network_cookie_thirdparty_nonsecureSessionOnly()) {
      if (!aHostURI->SchemeIs("https")) {
        return STATUS_ACCEPT_SESSION;
      }
    }
  }

  // if nothing has complained, accept cookie
  return STATUS_ACCEPTED;
}

// processes domain attribute, and returns true if host has permission to set
// for this domain.
bool nsCookieService::CheckDomain(CookieStruct& aCookieData, nsIURI* aHostURI,
                                  const nsACString& aBaseDomain,
                                  bool aRequireHostMatch) {
  // Note: The logic in this function is mirrored in
  // toolkit/components/extensions/ext-cookies.js:checkSetCookiePermissions().
  // If it changes, please update that function, or file a bug for someone
  // else to do so.

  // get host from aHostURI
  nsAutoCString hostFromURI;
  aHostURI->GetAsciiHost(hostFromURI);

  // if a domain is given, check the host has permission
  if (!aCookieData.host().IsEmpty()) {
    // Tolerate leading '.' characters, but not if it's otherwise an empty host.
    if (aCookieData.host().Length() > 1 && aCookieData.host().First() == '.') {
      aCookieData.host().Cut(0, 1);
    }

    // switch to lowercase now, to avoid case-insensitive compares everywhere
    ToLowerCase(aCookieData.host());

    // check whether the host is either an IP address, an alias such as
    // 'localhost', an eTLD such as 'co.uk', or the empty string. in these
    // cases, require an exact string match for the domain, and leave the cookie
    // as a non-domain one. bug 105917 originally noted the requirement to deal
    // with IP addresses.
    if (aRequireHostMatch) return hostFromURI.Equals(aCookieData.host());

    // ensure the proposed domain is derived from the base domain; and also
    // that the host domain is derived from the proposed domain (per RFC2109).
    if (IsSubdomainOf(aCookieData.host(), aBaseDomain) &&
        IsSubdomainOf(hostFromURI, aCookieData.host())) {
      // prepend a dot to indicate a domain cookie
      aCookieData.host().InsertLiteral(".", 0);
      return true;
    }

    /*
     * note: RFC2109 section 4.3.2 requires that we check the following:
     * that the portion of host not in domain does not contain a dot.
     * this prevents hosts of the form x.y.co.nz from setting cookies in the
     * entire .co.nz domain. however, it's only a only a partial solution and
     * it breaks sites (IE doesn't enforce it), so we don't perform this check.
     */
    return false;
  }

  // no domain specified, use hostFromURI
  aCookieData.host() = hostFromURI;
  return true;
}

nsAutoCString nsCookieService::GetPathFromURI(nsIURI* aHostURI) {
  // strip down everything after the last slash to get the path,
  // ignoring slashes in the query string part.
  // if we can QI to nsIURL, that'll take care of the query string portion.
  // otherwise, it's not an nsIURL and can't have a query string, so just find
  // the last slash.
  nsAutoCString path;
  nsCOMPtr<nsIURL> hostURL = do_QueryInterface(aHostURI);
  if (hostURL) {
    hostURL->GetDirectory(path);
  } else {
    aHostURI->GetPathQueryRef(path);
    int32_t slash = path.RFindChar('/');
    if (slash != kNotFound) {
      path.Truncate(slash + 1);
    }
  }

  // strip the right-most %x2F ("/") if the path doesn't contain only 1 '/'.
  int32_t lastSlash = path.RFindChar('/');
  int32_t firstSlash = path.FindChar('/');
  if (lastSlash != firstSlash && lastSlash != kNotFound &&
      lastSlash == (int32_t)(path.Length() - 1)) {
    path.Truncate(lastSlash);
  }

  return path;
}

bool nsCookieService::CheckPath(CookieStruct& aCookieData, nsIChannel* aChannel,
                                nsIURI* aHostURI) {
  // if a path is given, check the host has permission
  if (aCookieData.path().IsEmpty() || aCookieData.path().First() != '/') {
    aCookieData.path() = GetPathFromURI(aHostURI);

#if 0
  } else {
    /**
     * The following test is part of the RFC2109 spec.  Loosely speaking, it says that a site
     * cannot set a cookie for a path that it is not on.  See bug 155083.  However this patch
     * broke several sites -- nordea (bug 155768) and citibank (bug 156725).  So this test has
     * been disabled, unless we can evangelize these sites.
     */
    // get path from aHostURI
    nsAutoCString pathFromURI;
    if (NS_FAILED(aHostURI->GetPathQueryRef(pathFromURI)) ||
        !StringBeginsWith(pathFromURI, aCookieData.path())) {
      return false;
    }
#endif
  }

  if (aCookieData.path().Length() > kMaxBytesPerPath) {
    AutoTArray<nsString, 2> params = {
        NS_ConvertUTF8toUTF16(aCookieData.name())};

    nsString size;
    size.AppendInt(kMaxBytesPerPath);
    params.AppendElement(size);

    LogMessageToConsole(aChannel, aHostURI, nsIScriptError::warningFlag,
                        CONSOLE_GENERIC_CATEGORY,
                        NS_LITERAL_CSTRING("CookiePathOversize"), params);
    return false;
  }

  if (aCookieData.path().Contains('\t')) {
    return false;
  }

  return true;
}

// CheckPrefixes
//
// Reject cookies whose name starts with the magic prefixes from
// https://tools.ietf.org/html/draft-ietf-httpbis-cookie-prefixes-00
// if they do not meet the criteria required by the prefix.
//
// Must not be called until after CheckDomain() and CheckPath() have
// regularized and validated the CookieStruct values!
bool nsCookieService::CheckPrefixes(CookieStruct& aCookieData,
                                    bool aSecureRequest) {
  static const char kSecure[] = "__Secure-";
  static const char kHost[] = "__Host-";
  static const int kSecureLen = sizeof(kSecure) - 1;
  static const int kHostLen = sizeof(kHost) - 1;

  bool isSecure = strncmp(aCookieData.name().get(), kSecure, kSecureLen) == 0;
  bool isHost = strncmp(aCookieData.name().get(), kHost, kHostLen) == 0;

  if (!isSecure && !isHost) {
    // not one of the magic prefixes: carry on
    return true;
  }

  if (!aSecureRequest || !aCookieData.isSecure()) {
    // the magic prefixes may only be used from a secure request and
    // the secure attribute must be set on the cookie
    return false;
  }

  if (isHost) {
    // The host prefix requires that the path is "/" and that the cookie
    // had no domain attribute. CheckDomain() and CheckPath() MUST be run
    // first to make sure invalid attributes are rejected and to regularlize
    // them. In particular all explicit domain attributes result in a host
    // that starts with a dot, and if the host doesn't start with a dot it
    // correctly matches the true host.
    if (aCookieData.host()[0] == '.' ||
        !aCookieData.path().EqualsLiteral("/")) {
      return false;
    }
  }

  return true;
}

bool nsCookieService::GetExpiry(CookieStruct& aCookieData,
                                const nsACString& aExpires,
                                const nsACString& aMaxage, int64_t aServerTime,
                                int64_t aCurrentTime, bool aFromHttp) {
  // maxageCap is in seconds.
  // Disabled for HTTP cookies.
  int64_t maxageCap =
      aFromHttp ? 0 : StaticPrefs::privacy_documentCookies_maxage();

  /* Determine when the cookie should expire. This is done by taking the
   * difference between the server time and the time the server wants the cookie
   * to expire, and adding that difference to the client time. This localizes
   * the client time regardless of whether or not the TZ environment variable
   * was set on the client.
   *
   * Note: We need to consider accounting for network lag here, per RFC.
   */
  // check for max-age attribute first; this overrides expires attribute
  if (!aMaxage.IsEmpty()) {
    // obtain numeric value of maxageAttribute
    int64_t maxage;
    int32_t numInts = PR_sscanf(aMaxage.BeginReading(), "%lld", &maxage);

    // default to session cookie if the conversion failed
    if (numInts != 1) {
      return true;
    }

    // if this addition overflows, expiryTime will be less than currentTime
    // and the cookie will be expired - that's okay.
    if (maxageCap) {
      aCookieData.expiry() = aCurrentTime + std::min(maxage, maxageCap);
    } else {
      aCookieData.expiry() = aCurrentTime + maxage;
    }

    // check for expires attribute
  } else if (!aExpires.IsEmpty()) {
    PRTime expires;

    // parse expiry time
    if (PR_ParseTimeString(aExpires.BeginReading(), true, &expires) !=
        PR_SUCCESS) {
      return true;
    }

    // If set-cookie used absolute time to set expiration, and it can't use
    // client time to set expiration.
    // Because if current time be set in the future, but the cookie expire
    // time be set less than current time and more than server time.
    // The cookie item have to be used to the expired cookie.
    if (maxageCap) {
      aCookieData.expiry() = std::min(expires / int64_t(PR_USEC_PER_SEC),
                                      aCurrentTime + maxageCap);
    } else {
      aCookieData.expiry() = expires / int64_t(PR_USEC_PER_SEC);
    }

    // default to session cookie if no attributes found.  Here we don't need to
    // enforce the maxage cap, because session cookies are short-lived by
    // definition.
  } else {
    return true;
  }

  return false;
}

/******************************************************************************
 * nsCookieService impl:
 * private cookielist management functions
 ******************************************************************************/

// find whether a given cookie has been previously set. this is provided by the
// nsICookieManager interface.
NS_IMETHODIMP
nsCookieService::CookieExists(const nsACString& aHost, const nsACString& aPath,
                              const nsACString& aName,
                              JS::HandleValue aOriginAttributes, JSContext* aCx,
                              bool* aFoundCookie) {
  NS_ENSURE_ARG_POINTER(aCx);
  NS_ENSURE_ARG_POINTER(aFoundCookie);

  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }
  return CookieExistsNative(aHost, aPath, aName, &attrs, aFoundCookie);
}

NS_IMETHODIMP_(nsresult)
nsCookieService::CookieExistsNative(const nsACString& aHost,
                                    const nsACString& aPath,
                                    const nsACString& aName,
                                    OriginAttributes* aOriginAttributes,
                                    bool* aFoundCookie) {
  NS_ENSURE_ARG_POINTER(aOriginAttributes);
  NS_ENSURE_ARG_POINTER(aFoundCookie);

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString baseDomain;
  nsresult rv = GetBaseDomainFromHost(mTLDService, aHost, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  CookieListIter iter;
  CookieStorage* storage = PickStorage(*aOriginAttributes);
  *aFoundCookie = storage->FindCookie(baseDomain, *aOriginAttributes, aHost,
                                      aName, aPath, iter);
  return NS_OK;
}

// count the number of cookies stored by a particular host. this is provided by
// the nsICookieManager interface.
NS_IMETHODIMP
nsCookieService::CountCookiesFromHost(const nsACString& aHost,
                                      uint32_t* aCountFromHost) {
  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  *aCountFromHost = mDefaultStorage->CountCookiesFromHost(baseDomain, 0);

  return NS_OK;
}

// get an enumerator of cookies stored by a particular host. this is provided by
// the nsICookieManager interface.
NS_IMETHODIMP
nsCookieService::GetCookiesFromHost(const nsACString& aHost,
                                    JS::HandleValue aOriginAttributes,
                                    JSContext* aCx,
                                    nsTArray<RefPtr<nsICookie>>& aResult) {
  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes attrs;
  if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CookieStorage* storage = PickStorage(attrs);

  const nsTArray<RefPtr<Cookie>>* cookies =
      storage->GetCookiesFromHost(baseDomain, attrs);

  if (cookies) {
    aResult.SetCapacity(mMaxCookiesPerHost);
    for (Cookie* cookie : *cookies) {
      aResult.AppendElement(cookie);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookiesWithOriginAttributes(
    const nsAString& aPattern, const nsACString& aHost,
    nsTArray<RefPtr<nsICookie>>& aResult) {
  mozilla::OriginAttributesPattern pattern;
  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetCookiesWithOriginAttributes(pattern, baseDomain, aResult);
}

nsresult nsCookieService::GetCookiesWithOriginAttributes(
    const mozilla::OriginAttributesPattern& aPattern,
    const nsCString& aBaseDomain, nsTArray<RefPtr<nsICookie>>& aResult) {
  CookieStorage* storage = PickStorage(aPattern);
  storage->GetCookiesWithOriginAttributes(aPattern, aBaseDomain, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::RemoveCookiesWithOriginAttributes(const nsAString& aPattern,
                                                   const nsACString& aHost) {
  MOZ_ASSERT(XRE_IsParentProcess());

  mozilla::OriginAttributesPattern pattern;
  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  return RemoveCookiesWithOriginAttributes(pattern, baseDomain);
}

nsresult nsCookieService::RemoveCookiesWithOriginAttributes(
    const mozilla::OriginAttributesPattern& aPattern,
    const nsCString& aBaseDomain) {
  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CookieStorage* storage = PickStorage(aPattern);
  storage->RemoveCookiesWithOriginAttributes(aPattern, aBaseDomain);

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::RemoveCookiesFromExactHost(const nsACString& aHost,
                                            const nsAString& aPattern) {
  MOZ_ASSERT(XRE_IsParentProcess());

  mozilla::OriginAttributesPattern pattern;
  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  return RemoveCookiesFromExactHost(aHost, pattern);
}

nsresult nsCookieService::RemoveCookiesFromExactHost(
    const nsACString& aHost, const mozilla::OriginAttributesPattern& aPattern) {
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!IsInitialized()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CookieStorage* storage = PickStorage(aPattern);
  storage->RemoveCookiesFromExactHost(aHost, baseDomain, aPattern);

  return NS_OK;
}

namespace {

class RemoveAllSinceRunnable : public Runnable {
 public:
  typedef nsTArray<RefPtr<nsICookie>> CookieArray;
  RemoveAllSinceRunnable(Promise* aPromise, nsCookieService* aSelf,
                         CookieArray&& aCookieArray, int64_t aSinceWhen)
      : Runnable("RemoveAllSinceRunnable"),
        mPromise(aPromise),
        mSelf(aSelf),
        mList(std::move(aCookieArray)),
        mIndex(0),
        mSinceWhen(aSinceWhen) {}

  NS_IMETHODIMP Run() {
    RemoveSome();

    if (mIndex < mList.Length()) {
      return NS_DispatchToCurrentThread(this);
    } else {
      mPromise->MaybeResolveWithUndefined();
    }
    return NS_OK;
  }

 private:
  void RemoveSome() {
    for (CookieArray::size_type iter = 0;
         iter < kYieldPeriod && mIndex < mList.Length(); ++mIndex, ++iter) {
      Cookie* cookie = static_cast<Cookie*>(mList[mIndex].get());
      if (cookie->CreationTime() > mSinceWhen &&
          NS_FAILED(mSelf->Remove(cookie->Host(), cookie->OriginAttributesRef(),
                                  cookie->Name(), cookie->Path()))) {
        continue;
      }
    }
  }

 private:
  RefPtr<Promise> mPromise;
  RefPtr<nsCookieService> mSelf;
  CookieArray mList;
  CookieArray::size_type mIndex;
  int64_t mSinceWhen;
  static const CookieArray::size_type kYieldPeriod = 10;
};

}  // namespace

NS_IMETHODIMP
nsCookieService::RemoveAllSince(int64_t aSinceWhen, JSContext* aCx,
                                Promise** aRetVal) {
  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  EnsureReadComplete(true);

  nsTArray<RefPtr<nsICookie>> cookieList;

  // We delete only non-private cookies.
  mDefaultStorage->GetAll(cookieList);

  RefPtr<RemoveAllSinceRunnable> runMe = new RemoveAllSinceRunnable(
      promise, this, std::move(cookieList), aSinceWhen);

  promise.forget(aRetVal);

  return runMe->Run();
}

// TODO: to remove - next patch
void bindCookieParameters(mozIStorageBindingParamsArray* aParamsArray,
                          const CookieKey& aKey, const Cookie* aCookie) {
  NS_ASSERTION(aParamsArray,
               "Null params array passed to bindCookieParameters!");
  NS_ASSERTION(aCookie, "Null cookie passed to bindCookieParameters!");

  // Use the asynchronous binding methods to ensure that we do not acquire the
  // database lock.
  nsCOMPtr<mozIStorageBindingParams> params;
  DebugOnly<nsresult> rv =
      aParamsArray->NewBindingParams(getter_AddRefs(params));
  NS_ASSERT_SUCCESS(rv);

  nsAutoCString suffix;
  aKey.mOriginAttributes.CreateSuffix(suffix);
  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("originAttributes"),
                                    suffix);
  NS_ASSERT_SUCCESS(rv);

  rv =
      params->BindUTF8StringByName(NS_LITERAL_CSTRING("name"), aCookie->Name());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("value"),
                                    aCookie->Value());
  NS_ASSERT_SUCCESS(rv);

  rv =
      params->BindUTF8StringByName(NS_LITERAL_CSTRING("host"), aCookie->Host());
  NS_ASSERT_SUCCESS(rv);

  rv =
      params->BindUTF8StringByName(NS_LITERAL_CSTRING("path"), aCookie->Path());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindInt64ByName(NS_LITERAL_CSTRING("expiry"), aCookie->Expiry());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindInt64ByName(NS_LITERAL_CSTRING("lastAccessed"),
                               aCookie->LastAccessed());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindInt64ByName(NS_LITERAL_CSTRING("creationTime"),
                               aCookie->CreationTime());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindInt32ByName(NS_LITERAL_CSTRING("isSecure"),
                               aCookie->IsSecure());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindInt32ByName(NS_LITERAL_CSTRING("isHttpOnly"),
                               aCookie->IsHttpOnly());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindInt32ByName(NS_LITERAL_CSTRING("sameSite"),
                               aCookie->SameSite());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindInt32ByName(NS_LITERAL_CSTRING("rawSameSite"),
                               aCookie->RawSameSite());
  NS_ASSERT_SUCCESS(rv);

  // Bind the params to the array.
  rv = aParamsArray->AddParams(params);
  NS_ASSERT_SUCCESS(rv);
}

size_t nsCookieService::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  if (mDefaultStorage) {
    n += mDefaultStorage->SizeOfIncludingThis(aMallocSizeOf);
  }
  if (mPrivateStorage) {
    n += mPrivateStorage->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

MOZ_DEFINE_MALLOC_SIZE_OF(CookieServiceMallocSizeOf)

NS_IMETHODIMP
nsCookieService::CollectReports(nsIHandleReportCallback* aHandleReport,
                                nsISupports* aData, bool aAnonymize) {
  MOZ_COLLECT_REPORT("explicit/cookie-service", KIND_HEAP, UNITS_BYTES,
                     SizeOfIncludingThis(CookieServiceMallocSizeOf),
                     "Memory used by the cookie service.");

  return NS_OK;
}

bool nsCookieService::IsInitialized() const {
  if (!mDefaultStorage) {
    NS_WARNING("No CookieStorage! Profile already close?");
    return false;
  }

  MOZ_ASSERT(mPrivateStorage);
  return true;
}

CookieStorage* nsCookieService::PickStorage(const OriginAttributes& aAttrs) {
  MOZ_ASSERT(IsInitialized());
  EnsureReadComplete(true);

  if (aAttrs.mPrivateBrowsingId > 0) {
    return mPrivateStorage;
  }

  return mDefaultStorage;
}

CookieStorage* nsCookieService::PickStorage(
    const OriginAttributesPattern& aAttrs) {
  MOZ_ASSERT(IsInitialized());
  EnsureReadComplete(true);

  if (aAttrs.mPrivateBrowsingId.WasPassed() &&
      aAttrs.mPrivateBrowsingId.Value() > 0) {
    return mPrivateStorage;
  }

  return mDefaultStorage;
}
