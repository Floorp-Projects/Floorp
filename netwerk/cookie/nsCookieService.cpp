/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/Printf.h"
#include "mozilla/Unused.h"

#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/NeckoCommon.h"

#include "nsCookieService.h"
#include "nsContentUtils.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"

#include "nsIIOService.h"
#include "nsIPermissionManager.h"
#include "nsIProtocolHandler.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIScriptError.h"
#include "nsICookiePermission.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsILineInputStream.h"
#include "nsIEffectiveTLDService.h"
#include "nsIIDNService.h"
#include "nsIThread.h"
#include "mozIThirdPartyUtil.h"

#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsIMutableArray.h"
#include "nsArrayEnumerator.h"
#include "nsEnumeratorUtils.h"
#include "nsAutoPtr.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "prprf.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsISimpleEnumerator.h"
#include "nsIInputStream.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsNetCID.h"
#include "mozilla/storage.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Telemetry.h"
#include "nsIConsoleService.h"
#include "nsVariant.h"

using namespace mozilla;
using namespace mozilla::net;

// Create key from baseDomain that will access the default cookie namespace.
// TODO: When we figure out what the API will look like for nsICookieManager{2}
// on content processes (see bug 777620), change to use the appropriate app
// namespace.  For now those IDLs aren't supported on child processes.
#define DEFAULT_APP_KEY(baseDomain) \
        nsCookieKey(baseDomain, OriginAttributes())

/******************************************************************************
 * nsCookieService impl:
 * useful types & constants
 ******************************************************************************/

static StaticRefPtr<nsCookieService> gCookieService;
bool nsCookieService::sSameSiteEnabled = false;

// XXX_hack. See bug 178993.
// This is a hack to hide HttpOnly cookies from older browsers
#define HTTP_ONLY_PREFIX "#HttpOnly_"

#define COOKIES_FILE "cookies.sqlite"
#define COOKIES_SCHEMA_VERSION 9

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
#define IDX_BASE_DOMAIN 9
#define IDX_ORIGIN_ATTRIBUTES 10
#define IDX_SAME_SITE 11

#define TOPIC_CLEAR_ORIGIN_DATA "clear-origin-attributes-data"

static const int64_t kCookiePurgeAge =
  int64_t(30 * 24 * 60 * 60) * PR_USEC_PER_SEC; // 30 days in microseconds

#define OLD_COOKIE_FILE_NAME "cookies.txt"

#undef  LIMIT
#define LIMIT(x, low, high, default) ((x) >= (low) && (x) <= (high) ? (x) : (default))

#undef  ADD_TEN_PERCENT
#define ADD_TEN_PERCENT(i) static_cast<uint32_t>((i) + (i)/10)

// default limits for the cookie list. these can be tuned by the
// network.cookie.maxNumber and network.cookie.maxPerHost prefs respectively.
static const uint32_t kMaxNumberOfCookies = 3000;
static const uint32_t kMaxCookiesPerHost  = 180;
static const uint32_t kMaxBytesPerCookie  = 4096;
static const uint32_t kMaxBytesPerPath    = 1024;

// pref string constants
static const char kPrefCookieBehavior[]       = "network.cookie.cookieBehavior";
static const char kPrefMaxNumberOfCookies[]   = "network.cookie.maxNumber";
static const char kPrefMaxCookiesPerHost[]    = "network.cookie.maxPerHost";
static const char kPrefCookiePurgeAge[]       = "network.cookie.purgeAge";
static const char kPrefThirdPartySession[]    = "network.cookie.thirdparty.sessionOnly";
static const char kPrefThirdPartyNonsecureSession[] = "network.cookie.thirdparty.nonsecureSessionOnly";
static const char kCookieLeaveSecurityAlone[] = "network.cookie.leave-secure-alone";

// For telemetry COOKIE_LEAVE_SECURE_ALONE
#define BLOCKED_SECURE_SET_FROM_HTTP          0
#define BLOCKED_DOWNGRADE_SECURE_INEXACT      1
#define DOWNGRADE_SECURE_FROM_SECURE_INEXACT  2
#define EVICTED_NEWER_INSECURE                3
#define EVICTED_OLDEST_COOKIE                 4
#define EVICTED_PREFERRED_COOKIE              5
#define EVICTING_SECURE_BLOCKED               6
#define BLOCKED_DOWNGRADE_SECURE_EXACT        7
#define DOWNGRADE_SECURE_FROM_SECURE_EXACT    8

static void
bindCookieParameters(mozIStorageBindingParamsArray *aParamsArray,
                     const nsCookieKey &aKey,
                     const nsCookie *aCookie);

// stores the nsCookieEntry entryclass and an index into the cookie array
// within that entryclass, for purposes of storing an iteration state that
// points to a certain cookie.
struct nsListIter
{
  // default (non-initializing) constructor.
  nsListIter() = default;

  // explicit constructor to a given iterator state with entryclass 'aEntry'
  // and index 'aIndex'.
  explicit
  nsListIter(nsCookieEntry *aEntry, nsCookieEntry::IndexType aIndex)
   : entry(aEntry)
   , index(aIndex)
  {
  }

  // get the nsCookie * the iterator currently points to.
  nsCookie * Cookie() const
  {
    return entry->GetCookies()[index];
  }

  nsCookieEntry            *entry;
  nsCookieEntry::IndexType  index;
};

/******************************************************************************
 * Cookie logging handlers
 * used for logging in nsCookieService
 ******************************************************************************/

// logging handlers
#ifdef MOZ_LOGGING
// in order to do logging, the following environment variables need to be set:
//
//    set MOZ_LOG=cookie:3 -- shows rejected cookies
//    set MOZ_LOG=cookie:4 -- shows accepted and rejected cookies
//    set MOZ_LOG_FILE=cookie.log
//
#include "mozilla/Logging.h"
#endif

// define logging macros for convenience
#define SET_COOKIE true
#define GET_COOKIE false

static LazyLogModule gCookieLog("cookie");

#define COOKIE_LOGFAILURE(a, b, c, d)    LogFailure(a, b, c, d)
#define COOKIE_LOGSUCCESS(a, b, c, d, e) LogSuccess(a, b, c, d, e)

#define COOKIE_LOGEVICTED(a, details)          \
  PR_BEGIN_MACRO                               \
  if (MOZ_LOG_TEST(gCookieLog, LogLevel::Debug))  \
      LogEvicted(a, details);                  \
  PR_END_MACRO

#define COOKIE_LOGSTRING(lvl, fmt)   \
  PR_BEGIN_MACRO                     \
    MOZ_LOG(gCookieLog, lvl, fmt);  \
    MOZ_LOG(gCookieLog, lvl, ("\n")); \
  PR_END_MACRO

static void
LogFailure(bool aSetCookie, nsIURI *aHostURI, const char *aCookieString, const char *aReason)
{
  // if logging isn't enabled, return now to save cycles
  if (!MOZ_LOG_TEST(gCookieLog, LogLevel::Warning))
    return;

  nsAutoCString spec;
  if (aHostURI)
    aHostURI->GetAsciiSpec(spec);

  MOZ_LOG(gCookieLog, LogLevel::Warning,
    ("===== %s =====\n", aSetCookie ? "COOKIE NOT ACCEPTED" : "COOKIE NOT SENT"));
  MOZ_LOG(gCookieLog, LogLevel::Warning,("request URL: %s\n", spec.get()));
  if (aSetCookie)
    MOZ_LOG(gCookieLog, LogLevel::Warning,("cookie string: %s\n", aCookieString));

  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  MOZ_LOG(gCookieLog, LogLevel::Warning,("current time: %s", timeString));
  MOZ_LOG(gCookieLog, LogLevel::Warning,("rejected because %s\n", aReason));
  MOZ_LOG(gCookieLog, LogLevel::Warning,("\n"));
}

static void
LogCookie(nsCookie *aCookie)
{
  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  MOZ_LOG(gCookieLog, LogLevel::Debug,("current time: %s", timeString));

  if (aCookie) {
    MOZ_LOG(gCookieLog, LogLevel::Debug,("----------------\n"));
    MOZ_LOG(gCookieLog, LogLevel::Debug,("name: %s\n", aCookie->Name().get()));
    MOZ_LOG(gCookieLog, LogLevel::Debug,("value: %s\n", aCookie->Value().get()));
    MOZ_LOG(gCookieLog, LogLevel::Debug,("%s: %s\n", aCookie->IsDomain() ? "domain" : "host", aCookie->Host().get()));
    MOZ_LOG(gCookieLog, LogLevel::Debug,("path: %s\n", aCookie->Path().get()));

    PR_ExplodeTime(aCookie->Expiry() * int64_t(PR_USEC_PER_SEC),
                   PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);
    MOZ_LOG(gCookieLog, LogLevel::Debug,
      ("expires: %s%s", timeString, aCookie->IsSession() ? " (at end of session)" : ""));

    PR_ExplodeTime(aCookie->CreationTime(), PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);
    MOZ_LOG(gCookieLog, LogLevel::Debug,("created: %s", timeString));

    MOZ_LOG(gCookieLog, LogLevel::Debug,("is secure: %s\n", aCookie->IsSecure() ? "true" : "false"));
    MOZ_LOG(gCookieLog, LogLevel::Debug,("is httpOnly: %s\n", aCookie->IsHttpOnly() ? "true" : "false"));

    nsAutoCString suffix;
    aCookie->OriginAttributesRef().CreateSuffix(suffix);
    MOZ_LOG(gCookieLog, LogLevel::Debug,("origin attributes: %s\n",
            suffix.IsEmpty() ? "{empty}" : suffix.get()));
  }
}

static void
LogSuccess(bool aSetCookie, nsIURI *aHostURI, const char *aCookieString, nsCookie *aCookie, bool aReplacing)
{
  // if logging isn't enabled, return now to save cycles
  if (!MOZ_LOG_TEST(gCookieLog, LogLevel::Debug)) {
    return;
  }

  nsAutoCString spec;
  if (aHostURI)
    aHostURI->GetAsciiSpec(spec);

  MOZ_LOG(gCookieLog, LogLevel::Debug,
    ("===== %s =====\n", aSetCookie ? "COOKIE ACCEPTED" : "COOKIE SENT"));
  MOZ_LOG(gCookieLog, LogLevel::Debug,("request URL: %s\n", spec.get()));
  MOZ_LOG(gCookieLog, LogLevel::Debug,("cookie string: %s\n", aCookieString));
  if (aSetCookie)
    MOZ_LOG(gCookieLog, LogLevel::Debug,("replaces existing cookie: %s\n", aReplacing ? "true" : "false"));

  LogCookie(aCookie);

  MOZ_LOG(gCookieLog, LogLevel::Debug,("\n"));
}

static void
LogEvicted(nsCookie *aCookie, const char* details)
{
  MOZ_LOG(gCookieLog, LogLevel::Debug,("===== COOKIE EVICTED =====\n"));
  MOZ_LOG(gCookieLog, LogLevel::Debug,("%s\n", details));

  LogCookie(aCookie);

  MOZ_LOG(gCookieLog, LogLevel::Debug,("\n"));
}

// inline wrappers to make passing in nsCStrings easier
static inline void
LogFailure(bool aSetCookie, nsIURI *aHostURI, const nsCString& aCookieString, const char *aReason)
{
  LogFailure(aSetCookie, aHostURI, aCookieString.get(), aReason);
}

static inline void
LogSuccess(bool aSetCookie, nsIURI *aHostURI, const nsCString& aCookieString, nsCookie *aCookie, bool aReplacing)
{
  LogSuccess(aSetCookie, aHostURI, aCookieString.get(), aCookie, aReplacing);
}

#ifdef DEBUG
#define NS_ASSERT_SUCCESS(res)                                               \
  PR_BEGIN_MACRO                                                             \
  nsresult __rv = res; /* Do not evaluate |res| more than once! */           \
  if (NS_FAILED(__rv)) {                                                     \
    SmprintfPointer msg = mozilla::Smprintf("NS_ASSERT_SUCCESS(%s) failed with result 0x%" PRIX32, \
                           #res, static_cast<uint32_t>(__rv));               \
    NS_ASSERTION(NS_SUCCEEDED(__rv), msg.get());                             \
  }                                                                          \
  PR_END_MACRO
#else
#define NS_ASSERT_SUCCESS(res) PR_BEGIN_MACRO /* nothing */ PR_END_MACRO
#endif

/******************************************************************************
 * DBListenerErrorHandler impl:
 * Parent class for our async storage listeners that handles the logging of
 * errors.
 ******************************************************************************/
class DBListenerErrorHandler : public mozIStorageStatementCallback
{
protected:
  explicit DBListenerErrorHandler(DBState* dbState) : mDBState(dbState) { }
  RefPtr<DBState> mDBState;
  virtual const char *GetOpType() = 0;

public:
  NS_IMETHOD HandleError(mozIStorageError* aError) override
  {
    if (MOZ_LOG_TEST(gCookieLog, LogLevel::Warning)) {
      int32_t result = -1;
      aError->GetResult(&result);

      nsAutoCString message;
      aError->GetMessage(message);
      COOKIE_LOGSTRING(LogLevel::Warning,
        ("DBListenerErrorHandler::HandleError(): Error %d occurred while "
         "performing operation '%s' with message '%s'; rebuilding database.",
         result, GetOpType(), message.get()));
    }

    // Rebuild the database.
    gCookieService->HandleCorruptDB(mDBState);

    return NS_OK;
  }
};

/******************************************************************************
 * InsertCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous insertion operations.
 ******************************************************************************/
class InsertCookieDBListener final : public DBListenerErrorHandler
{
private:
  const char *GetOpType() override { return "INSERT"; }

  ~InsertCookieDBListener() = default;

public:
  NS_DECL_ISUPPORTS

  explicit InsertCookieDBListener(DBState* dbState) : DBListenerErrorHandler(dbState) { }
  NS_IMETHOD HandleResult(mozIStorageResultSet*) override
  {
    NS_NOTREACHED("Unexpected call to InsertCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(uint16_t aReason) override
  {
    // If we were rebuilding the db and we succeeded, make our corruptFlag say
    // so.
    if (mDBState->corruptFlag == DBState::REBUILDING &&
        aReason == mozIStorageStatementCallback::REASON_FINISHED) {
      COOKIE_LOGSTRING(LogLevel::Debug,
        ("InsertCookieDBListener::HandleCompletion(): rebuild complete"));
      mDBState->corruptFlag = DBState::OK;
    }
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(InsertCookieDBListener, mozIStorageStatementCallback)

/******************************************************************************
 * UpdateCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous update operations.
 ******************************************************************************/
class UpdateCookieDBListener final : public DBListenerErrorHandler
{
private:
  const char *GetOpType() override { return "UPDATE"; }

  ~UpdateCookieDBListener() = default;

public:
  NS_DECL_ISUPPORTS

  explicit UpdateCookieDBListener(DBState* dbState) : DBListenerErrorHandler(dbState) { }
  NS_IMETHOD HandleResult(mozIStorageResultSet*) override
  {
    NS_NOTREACHED("Unexpected call to UpdateCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(uint16_t aReason) override
  {
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(UpdateCookieDBListener, mozIStorageStatementCallback)

/******************************************************************************
 * RemoveCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous removal operations.
 ******************************************************************************/
class RemoveCookieDBListener final : public DBListenerErrorHandler
{
private:
  const char *GetOpType() override { return "REMOVE"; }

  ~RemoveCookieDBListener() = default;

public:
  NS_DECL_ISUPPORTS

  explicit RemoveCookieDBListener(DBState* dbState) : DBListenerErrorHandler(dbState) { }
  NS_IMETHOD HandleResult(mozIStorageResultSet*) override
  {
    NS_NOTREACHED("Unexpected call to RemoveCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(uint16_t aReason) override
  {
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(RemoveCookieDBListener, mozIStorageStatementCallback)

/******************************************************************************
 * CloseCookieDBListener imp:
 * Static mozIStorageCompletionCallback used to notify when the database is
 * successfully closed.
 ******************************************************************************/
class CloseCookieDBListener final :  public mozIStorageCompletionCallback
{
  ~CloseCookieDBListener() = default;

public:
  explicit CloseCookieDBListener(DBState* dbState) : mDBState(dbState) { }
  RefPtr<DBState> mDBState;
  NS_DECL_ISUPPORTS

  NS_IMETHOD Complete(nsresult, nsISupports*) override
  {
    gCookieService->HandleDBClosed(mDBState);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(CloseCookieDBListener, mozIStorageCompletionCallback)

namespace {

class AppClearDataObserver final : public nsIObserver {

  ~AppClearDataObserver() = default;

public:
  NS_DECL_ISUPPORTS

  // nsIObserver implementation.
  NS_IMETHOD
  Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData) override
  {
    MOZ_ASSERT(!nsCRT::strcmp(aTopic, TOPIC_CLEAR_ORIGIN_DATA));

    MOZ_ASSERT(XRE_IsParentProcess());

    nsCOMPtr<nsICookieManager> cookieManager
      = do_GetService(NS_COOKIEMANAGER_CONTRACTID);
    MOZ_ASSERT(cookieManager);

    return cookieManager->RemoveCookiesWithOriginAttributes(nsDependentString(aData), EmptyCString());
  }
};

NS_IMPL_ISUPPORTS(AppClearDataObserver, nsIObserver)

} // namespace

size_t
nsCookieEntry::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = nsCookieKey::SizeOfExcludingThis(aMallocSizeOf);

  amount += mCookies.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < mCookies.Length(); ++i) {
    amount += mCookies[i]->SizeOfIncludingThis(aMallocSizeOf);
  }

  return amount;
}

size_t
DBState::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = 0;

  amount += aMallocSizeOf(this);
  amount += hostTable.SizeOfExcludingThis(aMallocSizeOf);

  return amount;
}

/******************************************************************************
 * nsCookieService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/

already_AddRefed<nsICookieService>
nsCookieService::GetXPCOMSingleton()
{
  if (IsNeckoChild())
    return CookieServiceChild::GetSingleton();

  return GetSingleton();
}

already_AddRefed<nsCookieService>
nsCookieService::GetSingleton()
{
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

/* static */ void
nsCookieService::AppClearDataObserverInit()
{
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  nsCOMPtr<nsIObserver> obs = new AppClearDataObserver();
  observerService->AddObserver(obs, TOPIC_CLEAR_ORIGIN_DATA,
                               /* ownsWeak= */ false);
}

/******************************************************************************
 * nsCookieService impl:
 * public methods
 ******************************************************************************/

NS_IMPL_ISUPPORTS(nsCookieService,
                  nsICookieService,
                  nsICookieManager,
                  nsIObserver,
                  nsISupportsWeakReference,
                  nsIMemoryReporter)

nsCookieService::nsCookieService()
 : mDBState(nullptr)
 , mCookieBehavior(nsICookieService::BEHAVIOR_ACCEPT)
 , mThirdPartySession(false)
 , mThirdPartyNonsecureSession(false)
 , mLeaveSecureAlone(true)
 , mMaxNumberOfCookies(kMaxNumberOfCookies)
 , mMaxCookiesPerHost(kMaxCookiesPerHost)
 , mCookiePurgeAge(kCookiePurgeAge)
 , mThread(nullptr)
 , mMonitor("CookieThread")
 , mInitializedDBStates(false)
 , mInitializedDBConn(false)
{
}

nsresult
nsCookieService::Init()
{
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
    prefBranch->AddObserver(kPrefCookieBehavior,        this, true);
    prefBranch->AddObserver(kPrefMaxNumberOfCookies,    this, true);
    prefBranch->AddObserver(kPrefMaxCookiesPerHost,     this, true);
    prefBranch->AddObserver(kPrefCookiePurgeAge,        this, true);
    prefBranch->AddObserver(kPrefThirdPartySession,     this, true);
    prefBranch->AddObserver(kPrefThirdPartyNonsecureSession, this, true);
    prefBranch->AddObserver(kCookieLeaveSecurityAlone,  this, true);
    PrefChanged(prefBranch);
  }

  mStorageService = do_GetService("@mozilla.org/storage/service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Init our default, and possibly private DBStates.
  InitDBStates();

  RegisterWeakMemoryReporter(this);

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  NS_ENSURE_STATE(os);
  os->AddObserver(this, "profile-before-change", true);
  os->AddObserver(this, "profile-do-change", true);
  os->AddObserver(this, "last-pb-context-exited", true);

  mPermissionService = do_GetService(NS_COOKIEPERMISSION_CONTRACTID);
  if (!mPermissionService) {
    NS_WARNING("nsICookiePermission implementation not available - some features won't work!");
    COOKIE_LOGSTRING(LogLevel::Warning, ("Init(): nsICookiePermission implementation not available"));
  }

  return NS_OK;
}

void
nsCookieService::InitDBStates()
{
  NS_ASSERTION(!mDBState, "already have a DBState");
  NS_ASSERTION(!mDefaultDBState, "already have a default DBState");
  NS_ASSERTION(!mPrivateDBState, "already have a private DBState");
  NS_ASSERTION(!mInitializedDBStates, "already initialized");
  NS_ASSERTION(!mThread, "already have a cookie thread");

  // Create a new default DBState and set our current one.
  mDefaultDBState = new DBState();
  mDBState = mDefaultDBState;

  mPrivateDBState = new DBState();

  // Get our cookie file.
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
    getter_AddRefs(mDefaultDBState->cookieFile));
  if (NS_FAILED(rv)) {
    // We've already set up our DBStates appropriately; nothing more to do.
    COOKIE_LOGSTRING(LogLevel::Warning,
      ("InitDBStates(): couldn't get cookie file"));

    mInitializedDBConn = true;
    mInitializedDBStates = true;
    return;
  }
  mDefaultDBState->cookieFile->AppendNative(NS_LITERAL_CSTRING(COOKIES_FILE));

  NS_ENSURE_SUCCESS_VOID(NS_NewNamedThread("Cookie", getter_AddRefs(mThread)));

  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction("InitDBStates.TryInitDB", [] {
    NS_ENSURE_TRUE_VOID(gCookieService &&
                        gCookieService->mDBState &&
                        gCookieService->mDefaultDBState);

    MonitorAutoLock lock(gCookieService->mMonitor);

    // Attempt to open and read the database. If TryInitDB() returns RESULT_RETRY,
    // do so.
    OpenDBResult result = gCookieService->TryInitDB(false);
    if (result == RESULT_RETRY) {
      // Database may be corrupt. Synchronously close the connection, clean up the
      // default DBState, and try again.
      COOKIE_LOGSTRING(LogLevel::Warning, ("InitDBStates(): retrying TryInitDB()"));
      gCookieService->CleanupCachedStatements();
      gCookieService->CleanupDefaultDBConnection();
      result = gCookieService->TryInitDB(true);
      if (result == RESULT_RETRY) {
        // We're done. Change the code to failure so we clean up below.
        result = RESULT_FAILURE;
      }
    }

    if (result == RESULT_FAILURE) {
      COOKIE_LOGSTRING(LogLevel::Warning,
        ("InitDBStates(): TryInitDB() failed, closing connection"));

      // Connection failure is unrecoverable. Clean up our connection. We can run
      // fine without persistent storage -- e.g. if there's no profile.
      gCookieService->CleanupCachedStatements();
      gCookieService->CleanupDefaultDBConnection();

      // No need to initialize dbConn
      gCookieService->mInitializedDBConn = true;
    }

    gCookieService->mInitializedDBStates = true;

    NS_DispatchToMainThread(
      NS_NewRunnableFunction("TryInitDB.InitDBConn", [] {
        NS_ENSURE_TRUE_VOID(gCookieService);
        gCookieService->InitDBConn();
      })
    );
    gCookieService->mMonitor.Notify();
  });

  mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

namespace {

class ConvertAppIdToOriginAttrsSQLFunction final : public mozIStorageFunction
{
  ~ConvertAppIdToOriginAttrsSQLFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(ConvertAppIdToOriginAttrsSQLFunction, mozIStorageFunction);

NS_IMETHODIMP
ConvertAppIdToOriginAttrsSQLFunction::OnFunctionCall(
  mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult)
{
  nsresult rv;
  int32_t inIsolatedMozBrowser;

  rv = aFunctionArguments->GetInt32(1, &inIsolatedMozBrowser);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create an originAttributes object by inIsolatedMozBrowser.
  // Then create the originSuffix string from this object.
  OriginAttributes attrs(nsIScriptSecurityManager::NO_APP_ID,
                         (inIsolatedMozBrowser ? true : false));
  nsAutoCString suffix;
  attrs.CreateSuffix(suffix);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsAUTF8String(suffix);
  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

class SetAppIdFromOriginAttributesSQLFunction final : public mozIStorageFunction
{
  ~SetAppIdFromOriginAttributesSQLFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(SetAppIdFromOriginAttributesSQLFunction, mozIStorageFunction);

NS_IMETHODIMP
SetAppIdFromOriginAttributesSQLFunction::OnFunctionCall(
  mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult)
{
  nsresult rv;
  nsAutoCString suffix;
  OriginAttributes attrs;

  rv = aFunctionArguments->GetUTF8String(0, suffix);
  NS_ENSURE_SUCCESS(rv, rv);
  bool success = attrs.PopulateFromSuffix(suffix);
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsInt32(attrs.mAppId);
  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

class SetInBrowserFromOriginAttributesSQLFunction final :
  public mozIStorageFunction
{
  ~SetInBrowserFromOriginAttributesSQLFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(SetInBrowserFromOriginAttributesSQLFunction,
                  mozIStorageFunction);

NS_IMETHODIMP
SetInBrowserFromOriginAttributesSQLFunction::OnFunctionCall(
  mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult)
{
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

} // namespace

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
 * cleanup of the default DBState.
 */
OpenDBResult
nsCookieService::TryInitDB(bool aRecreateDB)
{
  NS_ASSERTION(!mDefaultDBState->dbConn, "nonnull dbConn");
  NS_ASSERTION(!mDefaultDBState->stmtInsert, "nonnull stmtInsert");
  NS_ASSERTION(!mDefaultDBState->insertListener, "nonnull insertListener");
  NS_ASSERTION(!mDefaultDBState->syncConn, "nonnull syncConn");
  NS_ASSERTION(NS_GetCurrentThread() == mThread, "non cookie thread");

  // Ditch an existing db, if we've been told to (i.e. it's corrupt). We don't
  // want to delete it outright, since it may be useful for debugging purposes,
  // so we move it out of the way.
  nsresult rv;
  if (aRecreateDB) {
    nsCOMPtr<nsIFile> backupFile;
    mDefaultDBState->cookieFile->Clone(getter_AddRefs(backupFile));
    rv = backupFile->MoveToNative(nullptr,
      NS_LITERAL_CSTRING(COOKIES_FILE ".bak"));
    NS_ENSURE_SUCCESS(rv, RESULT_FAILURE);
  }

  // This block provides scope for the Telemetry AutoTimer
  {
    Telemetry::AutoTimer<Telemetry::MOZ_SQLITE_COOKIES_OPEN_READAHEAD_MS>
      telemetry;
    ReadAheadFile(mDefaultDBState->cookieFile);

    // open a connection to the cookie database, and only cache our connection
    // and statements upon success. The connection is opened unshared to eliminate
    // cache contention between the main and background threads.
    rv = mStorageService->OpenUnsharedDatabase(mDefaultDBState->cookieFile,
      getter_AddRefs(mDefaultDBState->syncConn));
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
  }

  auto guard = MakeScopeExit([&] {
    mDefaultDBState->syncConn = nullptr;
  });

  bool tableExists = false;
  mDefaultDBState->syncConn->TableExists(NS_LITERAL_CSTRING("moz_cookies"),
    &tableExists);
  if (!tableExists) {
    rv = CreateTable();
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  } else {
    // table already exists; check the schema version before reading
    int32_t dbSchemaVersion;
    rv = mDefaultDBState->syncConn->GetSchemaVersion(&dbSchemaVersion);
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

    // Start a transaction for the whole migration block.
    mozStorageTransaction transaction(mDefaultDBState->syncConn, true);

    switch (dbSchemaVersion) {
    // Upgrading.
    // Every time you increment the database schema, you need to implement
    // the upgrading code from the previous version to the new one. If migration
    // fails for any reason, it's a bug -- so we return RESULT_RETRY such that
    // the original database will be saved, in the hopes that we might one day
    // see it and fix it.
    case 1:
      {
        // Add the lastAccessed column to the table.
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD lastAccessed INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
      // Fall through to the next upgrade.
      MOZ_FALLTHROUGH;

    case 2:
      {
        // Add the baseDomain column and index to the table.
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD baseDomain TEXT"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Compute the baseDomains for the table. This must be done eagerly
        // otherwise we won't be able to synchronously read in individual
        // domains on demand.
        const int64_t SCHEMA2_IDX_ID  =  0;
        const int64_t SCHEMA2_IDX_HOST = 1;
        nsCOMPtr<mozIStorageStatement> select;
        rv = mDefaultDBState->syncConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT id, host FROM moz_cookies"), getter_AddRefs(select));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageStatement> update;
        rv = mDefaultDBState->syncConn->CreateStatement(NS_LITERAL_CSTRING(
          "UPDATE moz_cookies SET baseDomain = :baseDomain WHERE id = :id"),
          getter_AddRefs(update));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCString baseDomain, host;
        bool hasResult;
        while (true) {
          rv = select->ExecuteStep(&hasResult);
          NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

          if (!hasResult)
            break;

          int64_t id = select->AsInt64(SCHEMA2_IDX_ID);
          select->GetUTF8String(SCHEMA2_IDX_HOST, host);

          rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
          NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

          mozStorageStatementScoper scoper(update);

          rv = update->BindUTF8StringByName(NS_LITERAL_CSTRING("baseDomain"),
                                            baseDomain);
          NS_ASSERT_SUCCESS(rv);
          rv = update->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                                       id);
          NS_ASSERT_SUCCESS(rv);

          rv = update->ExecuteStep(&hasResult);
          NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
        }

        // Create an index on baseDomain.
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
      // Fall through to the next upgrade.
      MOZ_FALLTHROUGH;

    case 3:
      {
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
        const int64_t SCHEMA3_IDX_ID =   0;
        const int64_t SCHEMA3_IDX_NAME = 1;
        const int64_t SCHEMA3_IDX_HOST = 2;
        const int64_t SCHEMA3_IDX_PATH = 3;
        nsCOMPtr<mozIStorageStatement> select;
        rv = mDefaultDBState->syncConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT id, name, host, path FROM moz_cookies "
            "ORDER BY name ASC, host ASC, path ASC, expiry ASC"),
          getter_AddRefs(select));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageStatement> deleteExpired;
        rv = mDefaultDBState->syncConn->CreateStatement(NS_LITERAL_CSTRING(
          "DELETE FROM moz_cookies WHERE id = :id"),
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

            if (!hasResult)
              break;

            int64_t id2 = select->AsInt64(SCHEMA3_IDX_ID);
            select->GetUTF8String(SCHEMA3_IDX_NAME, name2);
            select->GetUTF8String(SCHEMA3_IDX_HOST, host2);
            select->GetUTF8String(SCHEMA3_IDX_PATH, path2);

            // If the two rows match in (name, host, path), we know the earlier
            // row has an earlier expiry time. Delete it.
            if (name1 == name2 && host1 == host2 && path1 == path2) {
              mozStorageStatementScoper scoper(deleteExpired);

              rv = deleteExpired->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                id1);
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
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD creationTime INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy the id of each row into the new creationTime column.
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "UPDATE moz_cookies SET creationTime = "
            "(SELECT id WHERE id = moz_cookies.id)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create a unique index on (name, host, path) to allow fast lookup.
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "CREATE UNIQUE INDEX moz_uniqueid "
          "ON moz_cookies (name, host, path)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
      // Fall through to the next upgrade.
      MOZ_FALLTHROUGH;

    case 4:
      {
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
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies RENAME TO moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop existing index (CreateTable will create new one for new table)
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "DROP INDEX moz_basedomain"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create new table (with new fields and new unique constraint)
        rv = CreateTableForSchemaVersion5();
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy data from old table, using appId/inBrowser=0 for existing rows
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "INSERT INTO moz_cookies "
          "(baseDomain, appId, inBrowserElement, name, value, host, path, expiry,"
          " lastAccessed, creationTime, isSecure, isHttpOnly) "
          "SELECT baseDomain, 0, 0, name, value, host, path, expiry,"
          " lastAccessed, creationTime, isSecure, isHttpOnly "
          "FROM moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop old table
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "DROP TABLE moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
          ("Upgraded database to schema version 5"));
      }
      // Fall through to the next upgrade.
      MOZ_FALLTHROUGH;

    case 5:
      {
        // Change in the version: Replace the columns |appId| and
        // |inBrowserElement| by a single column |originAttributes|.
        //
        // Why we made this change: FxOS new security model (NSec) encapsulates
        // "appId/inIsolatedMozBrowser" in nsIPrincipal::originAttributes to make
        // it easier to modify the contents of this structure in the future.
        //
        // We do the migration in several steps:
        // 1. Rename the old table.
        // 2. Create a new table.
        // 3. Copy data from the old table to the new table; convert appId and
        //    inBrowserElement to originAttributes in the meantime.

        // Rename existing table.
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
             "ALTER TABLE moz_cookies RENAME TO moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop existing index (CreateTable will create new one for new table).
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
             "DROP INDEX moz_basedomain"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create new table with new fields and new unique constraint.
        rv = CreateTableForSchemaVersion6();
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy data from old table without the two deprecated columns appId and
        // inBrowserElement.
        nsCOMPtr<mozIStorageFunction>
          convertToOriginAttrs(new ConvertAppIdToOriginAttrsSQLFunction());
        NS_ENSURE_TRUE(convertToOriginAttrs, RESULT_RETRY);

        NS_NAMED_LITERAL_CSTRING(convertToOriginAttrsName,
                                 "CONVERT_TO_ORIGIN_ATTRIBUTES");

        rv = mDefaultDBState->syncConn->CreateFunction(convertToOriginAttrsName,
                                                     2, convertToOriginAttrs);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "INSERT INTO moz_cookies "
          "(baseDomain, originAttributes, name, value, host, path, expiry,"
          " lastAccessed, creationTime, isSecure, isHttpOnly) "
          "SELECT baseDomain, "
          " CONVERT_TO_ORIGIN_ATTRIBUTES(appId, inBrowserElement),"
          " name, value, host, path, expiry, lastAccessed, creationTime, "
          " isSecure, isHttpOnly "
          "FROM moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultDBState->syncConn->RemoveFunction(convertToOriginAttrsName);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop old table
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
             "DROP TABLE moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
          ("Upgraded database to schema version 6"));
      }
      MOZ_FALLTHROUGH;

    case 6:
      {
        // We made a mistake in schema version 6. We cannot remove expected
        // columns of any version (checked in the default case) from cookie
        // database, because doing this would destroy the possibility of
        // downgrading database.
        //
        // This version simply restores appId and inBrowserElement columns in
        // order to fix downgrading issue even though these two columns are no
        // longer used in the latest schema.
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD appId INTEGER DEFAULT 0;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD inBrowserElement INTEGER DEFAULT 0;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Compute and populate the values of appId and inBrwoserElement from
        // originAttributes.
        nsCOMPtr<mozIStorageFunction>
          setAppId(new SetAppIdFromOriginAttributesSQLFunction());
        NS_ENSURE_TRUE(setAppId, RESULT_RETRY);

        NS_NAMED_LITERAL_CSTRING(setAppIdName, "SET_APP_ID");

        rv = mDefaultDBState->syncConn->CreateFunction(setAppIdName, 1, setAppId);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageFunction>
          setInBrowser(new SetInBrowserFromOriginAttributesSQLFunction());
        NS_ENSURE_TRUE(setInBrowser, RESULT_RETRY);

        NS_NAMED_LITERAL_CSTRING(setInBrowserName, "SET_IN_BROWSER");

        rv = mDefaultDBState->syncConn->CreateFunction(setInBrowserName, 1,
                                                     setInBrowser);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "UPDATE moz_cookies SET appId = SET_APP_ID(originAttributes), "
          "inBrowserElement = SET_IN_BROWSER(originAttributes);"
        ));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultDBState->syncConn->RemoveFunction(setAppIdName);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = mDefaultDBState->syncConn->RemoveFunction(setInBrowserName);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
          ("Upgraded database to schema version 7"));
      }
      MOZ_FALLTHROUGH;

    case 7:
      {
        // Remove the appId field from moz_cookies.
        //
        // Unfortunately sqlite doesn't support dropping columns using ALTER
        // TABLE, so we need to go through the procedure documented in
        // https://www.sqlite.org/lang_altertable.html.

        // Drop existing index
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
             "DROP INDEX moz_basedomain"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create a new_moz_cookies table without the appId field.
        rv = CreateTableWorker("new_moz_cookies");
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Move the data over.
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "INSERT INTO new_moz_cookies ("
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
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "DROP TABLE moz_cookies;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Rename new_moz_cookies to moz_cookies.
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE new_moz_cookies RENAME TO moz_cookies;"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Recreate our index.
        rv = CreateIndex();
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug,
          ("Upgraded database to schema version 8"));
      }
      MOZ_FALLTHROUGH;

    case 8:
      {
        // Add the sameSite column to the table.
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(
          NS_LITERAL_CSTRING("ALTER TABLE moz_cookies ADD sameSite INTEGER"));
        COOKIE_LOGSTRING(LogLevel::Debug,
          ("Upgraded database to schema version 9"));
      }

      // No more upgrades. Update the schema version.
      rv = mDefaultDBState->syncConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
      NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

      Telemetry::Accumulate(Telemetry::MOZ_SQLITE_COOKIES_OLD_SCHEMA, dbSchemaVersion);
      MOZ_FALLTHROUGH;

    case COOKIES_SCHEMA_VERSION:
      break;

    case 0:
      {
        NS_WARNING("couldn't get schema version!");

        // the table may be usable; someone might've just clobbered the schema
        // version. we can treat this case like a downgrade using the codepath
        // below, by verifying the columns we care about are all there. for now,
        // re-set the schema version in the db, in case the checks succeed (if
        // they don't, we're dropping the table anyway).
        rv = mDefaultDBState->syncConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
      // fall through to downgrade check
      MOZ_FALLTHROUGH;

    // downgrading.
    // if columns have been added to the table, we can still use the ones we
    // understand safely. if columns have been deleted or altered, just
    // blow away the table and start from scratch! if you change the way
    // a column is interpreted, make sure you also change its name so this
    // check will catch it.
    default:
      {
        // check if all the expected columns exist
        nsCOMPtr<mozIStorageStatement> stmt;
        rv = mDefaultDBState->syncConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT "
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
            "sameSite "
          "FROM moz_cookies"), getter_AddRefs(stmt));
        if (NS_SUCCEEDED(rv))
          break;

        // our columns aren't there - drop the table!
        rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "DROP TABLE moz_cookies"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = CreateTable();
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
      break;
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
      NS_ENSURE_TRUE_VOID(gCookieService->mDefaultDBState);
      nsCOMPtr<nsIFile> oldCookieFile;
      nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
        getter_AddRefs(oldCookieFile));
      if (NS_FAILED(rv)) {
        return;
      }

      // Import cookies, and clean up the old file regardless of success or failure.
      // Note that we have to switch out our DBState temporarily, in case we're in
      // private browsing mode; otherwise ImportCookies() won't be happy.
      DBState* initialState = gCookieService->mDBState;
      gCookieService->mDBState = gCookieService->mDefaultDBState;
      oldCookieFile->AppendNative(NS_LITERAL_CSTRING(OLD_COOKIE_FILE_NAME));
      gCookieService->ImportCookies(oldCookieFile);
      oldCookieFile->Remove(false);
      gCookieService->mDBState = initialState;
    });

  NS_DispatchToMainThread(runnable);

  return RESULT_OK;
}

void
nsCookieService::InitDBConn()
{
  MOZ_ASSERT(NS_IsMainThread());

  // We should skip InitDBConn if we close profile during initializing DBStates
  // and then InitDBConn is called after we close the DBStates.
  if (!mInitializedDBStates || mInitializedDBConn || !mDefaultDBState) {
    return;
  }

  for (uint32_t i = 0; i < mReadArray.Length(); ++i) {
    CookieDomainTuple& tuple = mReadArray[i];
    RefPtr<nsCookie> cookie = nsCookie::Create(tuple.cookie->name,
                                               tuple.cookie->value,
                                               tuple.cookie->host,
                                               tuple.cookie->path,
                                               tuple.cookie->expiry,
                                               tuple.cookie->lastAccessed,
                                               tuple.cookie->creationTime,
                                               false,
                                               tuple.cookie->isSecure,
                                               tuple.cookie->isHttpOnly,
                                               tuple.cookie->originAttributes,
                                               tuple.cookie->sameSite);

    AddCookieToList(tuple.key, cookie, mDefaultDBState, nullptr, false);
  }

  if (NS_FAILED(InitDBConnInternal())) {
    COOKIE_LOGSTRING(LogLevel::Warning, ("InitDBConn(): retrying InitDBConnInternal()"));
    CleanupCachedStatements();
    CleanupDefaultDBConnection();
    if (NS_FAILED(InitDBConnInternal())) {
      COOKIE_LOGSTRING(LogLevel::Warning,
        ("InitDBConn(): InitDBConnInternal() failed, closing connection"));

      // Game over, clean the connections.
      CleanupCachedStatements();
      CleanupDefaultDBConnection();
    }
  }
  mInitializedDBConn = true;

  COOKIE_LOGSTRING(LogLevel::Debug, ("InitDBConn(): mInitializedDBConn = true"));
  mEndInitDBConn = mozilla::TimeStamp::Now();

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, "cookie-db-read", nullptr);
    mReadArray.Clear();
  }
}

nsresult
nsCookieService::InitDBConnInternal()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = mStorageService->OpenUnsharedDatabase(mDefaultDBState->cookieFile,
    getter_AddRefs(mDefaultDBState->dbConn));
  NS_ENSURE_SUCCESS(rv, rv);

  // Set up our listeners.
  mDefaultDBState->insertListener = new InsertCookieDBListener(mDefaultDBState);
  mDefaultDBState->updateListener = new UpdateCookieDBListener(mDefaultDBState);
  mDefaultDBState->removeListener = new RemoveCookieDBListener(mDefaultDBState);
  mDefaultDBState->closeListener = new CloseCookieDBListener(mDefaultDBState);

  // Grow cookie db in 512KB increments
  mDefaultDBState->dbConn->SetGrowthIncrement(512 * 1024, EmptyCString());

  // make operations on the table asynchronous, for performance
  mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA synchronous = OFF"));

  // Use write-ahead-logging for performance. We cap the autocheckpoint limit at
  // 16 pages (around 500KB).
  mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA journal_mode = WAL"));
  mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA wal_autocheckpoint = 16"));

  // cache frequently used statements (for insertion, deletion, and updating)
  rv = mDefaultDBState->dbConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_cookies ("
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
      "sameSite "
    ") VALUES ("
      ":baseDomain, "
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
      ":sameSite"
    ")"),
    getter_AddRefs(mDefaultDBState->stmtInsert));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDefaultDBState->dbConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_cookies "
    "WHERE name = :name AND host = :host AND path = :path AND originAttributes = :originAttributes"),
    getter_AddRefs(mDefaultDBState->stmtDelete));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDefaultDBState->dbConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_cookies SET lastAccessed = :lastAccessed "
    "WHERE name = :name AND host = :host AND path = :path AND originAttributes = :originAttributes"),
    getter_AddRefs(mDefaultDBState->stmtUpdate));
  return rv;
}

// Sets the schema version and creates the moz_cookies table.
nsresult
nsCookieService::CreateTableWorker(const char* aName)
{
  // Create the table.
  // We default originAttributes to empty string: this is so if users revert to
  // an older Firefox version that doesn't know about this field, any cookies
  // set will still work once they upgrade back.
  nsAutoCString command("CREATE TABLE ");
  command.Append(aName);
  command.AppendLiteral(" ("
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
      "sameSite INTEGER DEFAULT 0, "
      "CONSTRAINT moz_uniqueid UNIQUE (name, host, path, originAttributes)"
    ")");
  return mDefaultDBState->syncConn->ExecuteSimpleSQL(command);
}

// Sets the schema version and creates the moz_cookies table.
nsresult
nsCookieService::CreateTable()
{
  // Set the schema version, before creating the table.
  nsresult rv = mDefaultDBState->syncConn->SetSchemaVersion(
    COOKIES_SCHEMA_VERSION);
  if (NS_FAILED(rv)) return rv;

  rv = CreateTableWorker("moz_cookies");
  if (NS_FAILED(rv)) return rv;

  return CreateIndex();
}

nsresult
nsCookieService::CreateIndex()
{
  // Create an index on baseDomain.
  return mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, "
                                                "originAttributes)"));
}

// Sets the schema version and creates the moz_cookies table.
nsresult
nsCookieService::CreateTableForSchemaVersion6()
{
  // Set the schema version, before creating the table.
  nsresult rv = mDefaultDBState->syncConn->SetSchemaVersion(6);
  if (NS_FAILED(rv)) return rv;

  // Create the table.
  // We default originAttributes to empty string: this is so if users revert to
  // an older Firefox version that doesn't know about this field, any cookies
  // set will still work once they upgrade back.
  rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
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
  return mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, "
                                                "originAttributes)"));
}

// Sets the schema version and creates the moz_cookies table.
nsresult
nsCookieService::CreateTableForSchemaVersion5()
{
  // Set the schema version, before creating the table.
  nsresult rv = mDefaultDBState->syncConn->SetSchemaVersion(5);
  if (NS_FAILED(rv)) return rv;

  // Create the table. We default appId/inBrowserElement to 0: this is so if
  // users revert to an older Firefox version that doesn't know about these
  // fields, any cookies set will still work once they upgrade back.
  rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE moz_cookies ("
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
      "CONSTRAINT moz_uniqueid UNIQUE (name, host, path, appId, inBrowserElement)"
    ")"));
  if (NS_FAILED(rv)) return rv;

  // Create an index on baseDomain.
  return mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, "
                                                "appId, "
                                                "inBrowserElement)"));
}

void
nsCookieService::CloseDBStates()
{
  // return if we already closed
  if (!mDBState) {
    return;
  }

  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }

  // Null out our private and pointer DBStates regardless.
  mPrivateDBState = nullptr;
  mDBState = nullptr;

  // If we don't have a default DBState, we're done.
  if (!mDefaultDBState)
    return;

  // Cleanup cached statements before we can close anything.
  CleanupCachedStatements();

  if (mDefaultDBState->dbConn) {
    // Asynchronously close the connection. We will null it below.
    mDefaultDBState->dbConn->AsyncClose(mDefaultDBState->closeListener);
  }

  CleanupDefaultDBConnection();

  mDefaultDBState = nullptr;
  mInitializedDBConn = false;
  mInitializedDBStates = false;
}

// Null out the statements.
// This must be done before closing the connection.
void
nsCookieService::CleanupCachedStatements()
{
  mDefaultDBState->stmtInsert = nullptr;
  mDefaultDBState->stmtDelete = nullptr;
  mDefaultDBState->stmtUpdate = nullptr;
}

// Null out the listeners, and the database connection itself. This
// will not null out the statements, cancel a pending read or
// asynchronously close the connection -- these must be done
// beforehand if necessary.
void
nsCookieService::CleanupDefaultDBConnection()
{
  MOZ_ASSERT(!mDefaultDBState->stmtInsert, "stmtInsert has been cleaned up");
  MOZ_ASSERT(!mDefaultDBState->stmtDelete, "stmtDelete has been cleaned up");
  MOZ_ASSERT(!mDefaultDBState->stmtUpdate, "stmtUpdate has been cleaned up");

  // Null out the database connections. If 'dbConn' has not been used for any
  // asynchronous operations yet, this will synchronously close it; otherwise,
  // it's expected that the caller has performed an AsyncClose prior.
  mDefaultDBState->dbConn = nullptr;

  // Manually null out our listeners. This is necessary because they hold a
  // strong ref to the DBState itself. They'll stay alive until whatever
  // statements are still executing complete.
  mDefaultDBState->insertListener = nullptr;
  mDefaultDBState->updateListener = nullptr;
  mDefaultDBState->removeListener = nullptr;
  mDefaultDBState->closeListener = nullptr;
}

void
nsCookieService::HandleDBClosed(DBState* aDBState)
{
  COOKIE_LOGSTRING(LogLevel::Debug,
    ("HandleDBClosed(): DBState %p closed", aDBState));

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();

  switch (aDBState->corruptFlag) {
  case DBState::OK: {
    // Database is healthy. Notify of closure.
    if (os) {
      os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
    }
    break;
  }
  case DBState::CLOSING_FOR_REBUILD: {
    // Our close finished. Start the rebuild, and notify of db closure later.
    RebuildCorruptDB(aDBState);
    break;
  }
  case DBState::REBUILDING: {
    // We encountered an error during rebuild, closed the database, and now
    // here we are. We already have a 'cookies.sqlite.bak' from the original
    // dead database; we don't want to overwrite it, so let's move this one to
    // 'cookies.sqlite.bak-rebuild'.
    nsCOMPtr<nsIFile> backupFile;
    aDBState->cookieFile->Clone(getter_AddRefs(backupFile));
    nsresult rv = backupFile->MoveToNative(nullptr,
      NS_LITERAL_CSTRING(COOKIES_FILE ".bak-rebuild"));

    COOKIE_LOGSTRING(LogLevel::Warning,
      ("HandleDBClosed(): DBState %p encountered error rebuilding db; move to "
       "'cookies.sqlite.bak-rebuild' gave rv 0x%" PRIx32,
       aDBState, static_cast<uint32_t>(rv)));
    if (os) {
      os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
    }
    break;
  }
  }
}

void
nsCookieService::HandleCorruptDB(DBState* aDBState)
{
  if (mDefaultDBState != aDBState) {
    // We've either closed the state or we've switched profiles. It's getting
    // a bit late to rebuild -- bail instead.
    COOKIE_LOGSTRING(LogLevel::Warning,
      ("HandleCorruptDB(): DBState %p is already closed, aborting", aDBState));
    return;
  }

  COOKIE_LOGSTRING(LogLevel::Debug,
    ("HandleCorruptDB(): DBState %p has corruptFlag %u", aDBState,
      aDBState->corruptFlag));

  // Mark the database corrupt, so the close listener can begin reconstructing
  // it.
  switch (mDefaultDBState->corruptFlag) {
  case DBState::OK: {
    // Move to 'closing' state.
    mDefaultDBState->corruptFlag = DBState::CLOSING_FOR_REBUILD;

    CleanupCachedStatements();
    mDefaultDBState->dbConn->AsyncClose(mDefaultDBState->closeListener);
    CleanupDefaultDBConnection();
    break;
  }
  case DBState::CLOSING_FOR_REBUILD: {
    // We had an error while waiting for close completion. That's OK, just
    // ignore it -- we're rebuilding anyway.
    return;
  }
  case DBState::REBUILDING: {
    // We had an error while rebuilding the DB. Game over. Close the database
    // and let the close handler do nothing; then we'll move it out of the way.
    CleanupCachedStatements();
    if (mDefaultDBState->dbConn) {
      mDefaultDBState->dbConn->AsyncClose(mDefaultDBState->closeListener);
    }
    CleanupDefaultDBConnection();
    break;
  }
  }
}

void
nsCookieService::RebuildCorruptDB(DBState* aDBState)
{
  NS_ASSERTION(!aDBState->dbConn, "shouldn't have an open db connection");
  NS_ASSERTION(aDBState->corruptFlag == DBState::CLOSING_FOR_REBUILD,
    "should be in CLOSING_FOR_REBUILD state");

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();

  aDBState->corruptFlag = DBState::REBUILDING;

  if (mDefaultDBState != aDBState) {
    // We've either closed the state or we've switched profiles. It's getting
    // a bit late to rebuild -- bail instead. In any case, we were waiting
    // on rebuild completion to notify of the db closure, which won't happen --
    // do so now.
    COOKIE_LOGSTRING(LogLevel::Warning,
      ("RebuildCorruptDB(): DBState %p is stale, aborting", aDBState));
    if (os) {
      os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
    }
    return;
  }

  COOKIE_LOGSTRING(LogLevel::Debug,
    ("RebuildCorruptDB(): creating new database"));

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableFunction("RebuildCorruptDB.TryInitDB", [] {
      NS_ENSURE_TRUE_VOID(gCookieService && gCookieService->mDefaultDBState);

      // The database has been closed, and we're ready to rebuild. Open a
      // connection.
      OpenDBResult result = gCookieService->TryInitDB(true);

      nsCOMPtr<nsIRunnable> innerRunnable =
        NS_NewRunnableFunction("RebuildCorruptDB.TryInitDBComplete", [result] {
          NS_ENSURE_TRUE_VOID(gCookieService && gCookieService->mDefaultDBState);

          nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
          if (result != RESULT_OK) {
            // We're done. Reset our DB connection and statements, and notify of
            // closure.
            COOKIE_LOGSTRING(LogLevel::Warning,
              ("RebuildCorruptDB(): TryInitDB() failed with result %u", result));
            gCookieService->CleanupCachedStatements();
            gCookieService->CleanupDefaultDBConnection();
            gCookieService->mDefaultDBState->corruptFlag = DBState::OK;
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
          mozIStorageAsyncStatement* stmt = gCookieService->mDefaultDBState->stmtInsert;
          nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
          stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
          for (auto iter = gCookieService->mDefaultDBState->hostTable.Iter();
               !iter.Done();
               iter.Next()) {
            nsCookieEntry* entry = iter.Get();

            const nsCookieEntry::ArrayType& cookies = entry->GetCookies();
            for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
              nsCookie* cookie = cookies[i];

              if (!cookie->IsSession()) {
                bindCookieParameters(paramsArray, nsCookieKey(entry), cookie);
              }
            }
          }

          // Make sure we've got something to write. If we don't, we're done.
          uint32_t length;
          paramsArray->GetLength(&length);
          if (length == 0) {
            COOKIE_LOGSTRING(LogLevel::Debug,
              ("RebuildCorruptDB(): nothing to write, rebuild complete"));
            gCookieService->mDefaultDBState->corruptFlag = DBState::OK;
            return;
          }

          // Execute the statement. If any errors crop up, we won't try again.
          DebugOnly<nsresult> rv = stmt->BindParameters(paramsArray);
          NS_ASSERT_SUCCESS(rv);
          nsCOMPtr<mozIStoragePendingStatement> handle;
          rv = stmt->ExecuteAsync(gCookieService->mDefaultDBState->insertListener,
                                  getter_AddRefs(handle));
          NS_ASSERT_SUCCESS(rv);
        });
      NS_DispatchToMainThread(innerRunnable);
    });
  mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

nsCookieService::~nsCookieService()
{
  CloseDBStates();

  UnregisterWeakMemoryReporter(this);

  gCookieService = nullptr;
}

NS_IMETHODIMP
nsCookieService::Observe(nsISupports     *aSubject,
                         const char      *aTopic,
                         const char16_t *aData)
{
  // check the topic
  if (!strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.

    // Close the default DB connection and null out our DBStates before
    // changing.
    CloseDBStates();

  } else if (!strcmp(aTopic, "profile-do-change")) {
    NS_ASSERTION(!mDefaultDBState, "shouldn't have a default DBState");
    NS_ASSERTION(!mPrivateDBState, "shouldn't have a private DBState");

    // the profile has already changed; init the db from the new location.
    // if we are in the private browsing state, however, we do not want to read
    // data into it - we should instead put it into the default state, so it's
    // ready for us if and when we switch back to it.
    InitDBStates();

  } else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
    if (prefBranch)
      PrefChanged(prefBranch);

  } else if (!strcmp(aTopic, "last-pb-context-exited")) {
    // Flush all the cookies stored by private browsing contexts
    mozilla::OriginAttributesPattern pattern;
    pattern.mPrivateBrowsingId.Construct(1);
    RemoveCookiesWithOriginAttributes(pattern, EmptyCString());
    mPrivateDBState = new DBState();
  }


  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookieString(nsIURI     *aHostURI,
                                 nsIChannel *aChannel,
                                 char       **aCookie)
{
  return GetCookieStringCommon(aHostURI, aChannel, false, aCookie);
}

NS_IMETHODIMP
nsCookieService::GetCookieStringFromHttp(nsIURI     *aHostURI,
                                         nsIURI     *aFirstURI,
                                         nsIChannel *aChannel,
                                         char       **aCookie)
{
  return GetCookieStringCommon(aHostURI, aChannel, true, aCookie);
}

nsresult
nsCookieService::GetCookieStringCommon(nsIURI *aHostURI,
                                       nsIChannel *aChannel,
                                       bool aHttpBound,
                                       char** aCookie)
{
  NS_ENSURE_ARG(aHostURI);
  NS_ENSURE_ARG(aCookie);

  // Determine whether the request is foreign. Failure is acceptable.
  bool isForeign = true;
  mThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isForeign);

  // Get originAttributes.
  OriginAttributes attrs;
  if (aChannel) {
    NS_GetOriginAttributes(aChannel, attrs);
  }

  bool isSafeTopLevelNav = NS_IsSafeTopLevelNav(aChannel);
  bool isSameSiteForeign = NS_IsSameSiteForeign(aChannel, aHostURI);
  nsAutoCString result;
  GetCookieStringInternal(aHostURI, isForeign, isSafeTopLevelNav, isSameSiteForeign,
                          aHttpBound, attrs, result);
  *aCookie = result.IsEmpty() ? nullptr : ToNewCString(result);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieString(nsIURI     *aHostURI,
                                 nsIPrompt  *aPrompt,
                                 const char *aCookieHeader,
                                 nsIChannel *aChannel)
{
  // The aPrompt argument is deprecated and unused.  Avoid introducing new
  // code that uses this argument by warning if the value is non-null.
  MOZ_ASSERT(!aPrompt);
  if (aPrompt) {
    nsCOMPtr<nsIConsoleService> aConsoleService =
        do_GetService("@mozilla.org/consoleservice;1");
    if (aConsoleService) {
      aConsoleService->LogStringMessage(
        u"Non-null prompt ignored by nsCookieService.");
    }
  }
  return SetCookieStringCommon(aHostURI, aCookieHeader, nullptr, aChannel,
                               false);
}

NS_IMETHODIMP
nsCookieService::SetCookieStringFromHttp(nsIURI     *aHostURI,
                                         nsIURI     *aFirstURI,
                                         nsIPrompt  *aPrompt,
                                         const char *aCookieHeader,
                                         const char *aServerTime,
                                         nsIChannel *aChannel)
{
  // The aPrompt argument is deprecated and unused.  Avoid introducing new
  // code that uses this argument by warning if the value is non-null.
  MOZ_ASSERT(!aPrompt);
  if (aPrompt) {
    nsCOMPtr<nsIConsoleService> aConsoleService =
        do_GetService("@mozilla.org/consoleservice;1");
    if (aConsoleService) {
      aConsoleService->LogStringMessage(
        u"Non-null prompt ignored by nsCookieService.");
    }
  }
  return SetCookieStringCommon(aHostURI, aCookieHeader, aServerTime, aChannel,
                               true);
}

int64_t
nsCookieService::ParseServerTime(const nsCString &aServerTime)
{
  // parse server local time. this is not just done here for efficiency
  // reasons - if there's an error parsing it, and we need to default it
  // to the current time, we must do it here since the current time in
  // SetCookieInternal() will change for each cookie processed (e.g. if the
  // user is prompted).
  PRTime tempServerTime;
  int64_t serverTime;
  PRStatus result = PR_ParseTimeString(aServerTime.get(), true,
                                       &tempServerTime);
  if (result == PR_SUCCESS) {
    serverTime = tempServerTime / int64_t(PR_USEC_PER_SEC);
  } else {
    serverTime = PR_Now() / PR_USEC_PER_SEC;
  }

  return serverTime;
}

nsresult
nsCookieService::SetCookieStringCommon(nsIURI *aHostURI,
                                       const char *aCookieHeader,
                                       const char *aServerTime,
                                       nsIChannel *aChannel,
                                       bool aFromHttp)
{
  NS_ENSURE_ARG(aHostURI);
  NS_ENSURE_ARG(aCookieHeader);

  // Determine whether the request is foreign. Failure is acceptable.
  bool isForeign = true;
  mThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isForeign);

  // Get originAttributes.
  OriginAttributes attrs;
  if (aChannel) {
    NS_GetOriginAttributes(aChannel, attrs);
  }

  nsDependentCString cookieString(aCookieHeader);
  nsDependentCString serverTime(aServerTime ? aServerTime : "");
  SetCookieStringInternal(aHostURI, isForeign, cookieString,
                          serverTime, aFromHttp, attrs, aChannel);
  return NS_OK;
}

void
nsCookieService::SetCookieStringInternal(nsIURI                 *aHostURI,
                                         bool                    aIsForeign,
                                         nsDependentCString     &aCookieHeader,
                                         const nsCString        &aServerTime,
                                         bool                    aFromHttp,
                                         const OriginAttributes &aOriginAttrs,
                                         nsIChannel             *aChannel)
{
  NS_ASSERTION(aHostURI, "null host!");

  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return;
  }

  EnsureReadComplete(true);

  AutoRestore<DBState*> savePrevDBState(mDBState);
  mDBState = (aOriginAttrs.mPrivateBrowsingId > 0) ? mPrivateDBState : mDefaultDBState;

  // get the base domain for the host URI.
  // e.g. for "www.bbc.co.uk", this would be "bbc.co.uk".
  // file:// URI's (i.e. with an empty host) are allowed, but any other
  // scheme must have a non-empty host. A trailing dot in the host
  // is acceptable.
  bool requireHostMatch;
  nsAutoCString baseDomain;
  nsresult rv = GetBaseDomain(mTLDService, aHostURI, baseDomain, requireHostMatch);
  if (NS_FAILED(rv)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
                      "couldn't get base domain from URI");
    return;
  }

  nsCookieKey key(baseDomain, aOriginAttrs);

  // check default prefs
  uint32_t priorCookieCount = 0;
  nsAutoCString hostFromURI;
  aHostURI->GetHost(hostFromURI);
  CountCookiesFromHost(hostFromURI, &priorCookieCount);
  CookieStatus cookieStatus = CheckPrefs(mPermissionService, mCookieBehavior,
                                         mThirdPartySession,
                                         mThirdPartyNonsecureSession, aHostURI,
                                         aIsForeign, aCookieHeader.get(),
                                         priorCookieCount, aOriginAttrs);

  // fire a notification if third party or if cookie was rejected
  // (but not if there was an error)
  switch (cookieStatus) {
  case STATUS_REJECTED:
    NotifyRejected(aHostURI);
    if (aIsForeign) {
      NotifyThirdParty(aHostURI, false, aChannel);
    }
    return; // Stop here
  case STATUS_REJECTED_WITH_ERROR:
    return;
  case STATUS_ACCEPTED: // Fallthrough
  case STATUS_ACCEPT_SESSION:
    if (aIsForeign) {
      NotifyThirdParty(aHostURI, true, aChannel);
    }
    break;
  default:
    break;
  }

  int64_t serverTime = ParseServerTime(aServerTime);

  // process each cookie in the header
  while (SetCookieInternal(aHostURI, key, requireHostMatch, cookieStatus,
                           aCookieHeader, serverTime, aFromHttp, aChannel)) {
    // document.cookie can only set one cookie at a time
    if (!aFromHttp)
      break;
  }
}

// notify observers that a cookie was rejected due to the users' prefs.
void
nsCookieService::NotifyRejected(nsIURI *aHostURI)
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(aHostURI, "cookie-rejected", nullptr);
  }
}

// notify observers that a third-party cookie was accepted/rejected
// if the cookie issuer is unknown, it defaults to "?"
void
nsCookieService::NotifyThirdParty(nsIURI *aHostURI, bool aIsAccepted, nsIChannel *aChannel)
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return;
  }

  const char* topic;

  if (mDBState != mPrivateDBState) {
    // Regular (non-private) browsing
    if (aIsAccepted) {
      topic = "third-party-cookie-accepted";
    } else {
      topic = "third-party-cookie-rejected";
    }
  } else {
    // Private browsing
    if (aIsAccepted) {
      topic = "private-third-party-cookie-accepted";
    } else {
      topic = "private-third-party-cookie-rejected";
    }
  }

  do {
    // Attempt to find the host of aChannel.
    if (!aChannel) {
      break;
    }
    nsCOMPtr<nsIURI> channelURI;
    nsresult rv = aChannel->GetURI(getter_AddRefs(channelURI));
    if (NS_FAILED(rv)) {
      break;
    }

    nsAutoCString referringHost;
    rv = channelURI->GetHost(referringHost);
    if (NS_FAILED(rv)) {
      break;
    }

    nsAutoString referringHostUTF16 = NS_ConvertUTF8toUTF16(referringHost);
    os->NotifyObservers(aHostURI, topic, referringHostUTF16.get());
    return;
  } while (false);

  // This can fail for a number of reasons, in which kind we fallback to "?"
  os->NotifyObservers(aHostURI, topic, u"?");
}

// notify observers that the cookie list changed. there are five possible
// values for aData:
// "deleted" means a cookie was deleted. aSubject is the deleted cookie.
// "added"   means a cookie was added. aSubject is the added cookie.
// "changed" means a cookie was altered. aSubject is the new cookie.
// "cleared" means the entire cookie list was cleared. aSubject is null.
// "batch-deleted" means a set of cookies was purged. aSubject is the list of
// cookies.
void
nsCookieService::NotifyChanged(nsISupports     *aSubject,
                               const char16_t *aData,
                               bool aOldCookieIsSession,
                               bool aFromHttp)
{
  const char* topic = mDBState == mPrivateDBState ?
      "private-cookie-changed" : "cookie-changed";
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return;
  }
  // Notify for topic "private-cookie-changed" or "cookie-changed"
  os->NotifyObservers(aSubject, topic, aData);

  // Notify for topic "session-cookie-changed" to update the copy of session
  // cookies in session restore component.
  // Ignore private session cookies since they will not be restored.
  if (mDBState == mPrivateDBState) {
    return;
  }
  // Filter out notifications for individual non-session cookies.
  if (NS_LITERAL_STRING("changed").Equals(aData) ||
      NS_LITERAL_STRING("deleted").Equals(aData) ||
      NS_LITERAL_STRING("added").Equals(aData)) {
    nsCOMPtr<nsICookie> xpcCookie = do_QueryInterface(aSubject);
    MOZ_ASSERT(xpcCookie);
    auto cookie = static_cast<nsCookie*>(xpcCookie.get());
    if (!cookie->IsSession() && !aOldCookieIsSession) {
      return;
    }
  }
  os->NotifyObservers(aSubject, "session-cookie-changed", aData);
}

already_AddRefed<nsIArray>
nsCookieService::CreatePurgeList(nsICookie2* aCookie)
{
  nsCOMPtr<nsIMutableArray> removedList =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  removedList->AppendElement(aCookie);
  return removedList.forget();
}

/******************************************************************************
 * nsCookieService:
 * public transaction helper impl
 ******************************************************************************/

NS_IMETHODIMP
nsCookieService::RunInTransaction(nsICookieTransactionCallback* aCallback)
{
  NS_ENSURE_ARG(aCallback);
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  if (NS_WARN_IF(!mDefaultDBState->dbConn)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mozStorageTransaction transaction(mDefaultDBState->dbConn, true);

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

void
nsCookieService::PrefChanged(nsIPrefBranch *aPrefBranch)
{
  int32_t val;
  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefCookieBehavior, &val)))
    mCookieBehavior = (uint8_t) LIMIT(val, 0, 3, 0);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefMaxNumberOfCookies, &val)))
    mMaxNumberOfCookies = (uint16_t) LIMIT(val, 1, 0xFFFF, kMaxNumberOfCookies);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefMaxCookiesPerHost, &val)))
    mMaxCookiesPerHost = (uint16_t) LIMIT(val, 1, 0xFFFF, kMaxCookiesPerHost);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefCookiePurgeAge, &val))) {
    mCookiePurgeAge =
      int64_t(LIMIT(val, 0, INT32_MAX, INT32_MAX)) * PR_USEC_PER_SEC;
  }

  bool boolval;
  if (NS_SUCCEEDED(aPrefBranch->GetBoolPref(kPrefThirdPartySession, &boolval)))
    mThirdPartySession = boolval;

  if (NS_SUCCEEDED(aPrefBranch->GetBoolPref(kPrefThirdPartyNonsecureSession, &boolval)))
    mThirdPartyNonsecureSession = boolval;

  if (NS_SUCCEEDED(aPrefBranch->GetBoolPref(kCookieLeaveSecurityAlone, &boolval)))
    mLeaveSecureAlone = boolval;
}

/******************************************************************************
 * nsICookieManager impl:
 * nsICookieManager
 ******************************************************************************/

NS_IMETHODIMP
nsCookieService::RemoveAll()
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  RemoveAllFromMemory();

  // clear the cookie file
  if (mDBState->dbConn) {
    NS_ASSERTION(mDBState == mDefaultDBState, "not in default DB state");

    nsCOMPtr<mozIStorageAsyncStatement> stmt;
    nsresult rv = mDefaultDBState->dbConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_cookies"), getter_AddRefs(stmt));
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(mDefaultDBState->removeListener,
        getter_AddRefs(handle));
      NS_ASSERT_SUCCESS(rv);
    } else {
      // Recreate the database.
      COOKIE_LOGSTRING(LogLevel::Debug,
                       ("RemoveAll(): corruption detected with rv 0x%" PRIx32, static_cast<uint32_t>(rv)));
      HandleCorruptDB(mDefaultDBState);
    }
  }

  NotifyChanged(nullptr, u"cleared");
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetEnumerator(nsISimpleEnumerator **aEnumerator)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  nsCOMArray<nsICookie> cookieList(mDBState->cookieCount);
  for (auto iter = mDBState->hostTable.Iter(); !iter.Done(); iter.Next()) {
    const nsCookieEntry::ArrayType& cookies = iter.Get()->GetCookies();
    for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
      cookieList.AppendObject(cookies[i]);
    }
  }

  return NS_NewArrayEnumerator(aEnumerator, cookieList);
}

NS_IMETHODIMP
nsCookieService::GetSessionEnumerator(nsISimpleEnumerator **aEnumerator)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  nsCOMArray<nsICookie> cookieList(mDBState->cookieCount);
  for (auto iter = mDBState->hostTable.Iter(); !iter.Done(); iter.Next()) {
    const nsCookieEntry::ArrayType& cookies = iter.Get()->GetCookies();
    for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
      nsCookie* cookie = cookies[i];
      // Filter out non-session cookies.
      if (cookie->IsSession()) {
        cookieList.AppendObject(cookie);
      }
    }
  }

  return NS_NewArrayEnumerator(aEnumerator, cookieList);
}

static nsresult
InitializeOriginAttributes(OriginAttributes* aAttrs,
                           JS::HandleValue aOriginAttributes,
                           JSContext* aCx,
                           uint8_t aArgc,
                           const char16_t* aAPI,
                           const char16_t* aInterfaceSuffix)
{
  MOZ_ASSERT(aAttrs);
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aAPI);
  MOZ_ASSERT(aInterfaceSuffix);

  if (aArgc == 0) {
    const char16_t* params[] = {
      aAPI,
      aInterfaceSuffix
    };

    // This is supposed to be temporary and in 1 or 2 releases we want to
    // have originAttributes param as mandatory. But for now, we don't want to
    // break existing addons, so we write a console message to inform the addon
    // developers about it.
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("Cookie Manager"),
                                    nullptr,
                                    nsContentUtils::eNECKO_PROPERTIES,
                                    "nsICookieManagerAPIDeprecated",
                                    params, ArrayLength(params));
  } else if (aArgc == 1) {
    if (!aOriginAttributes.isObject() ||
        !aAttrs->Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::Add(const nsACString &aHost,
                     const nsACString &aPath,
                     const nsACString &aName,
                     const nsACString &aValue,
                     bool              aIsSecure,
                     bool              aIsHttpOnly,
                     bool              aIsSession,
                     int64_t           aExpiry,
                     JS::HandleValue   aOriginAttributes,
                     int32_t           aSameSite,
                     JSContext*        aCx,
                     uint8_t           aArgc)
{
  MOZ_ASSERT(aArgc == 0 || aArgc == 1);

  OriginAttributes attrs;
  nsresult rv = InitializeOriginAttributes(&attrs,
                                           aOriginAttributes,
                                           aCx,
                                           aArgc,
                                           u"nsICookieManager.add()",
                                           u"2");
  NS_ENSURE_SUCCESS(rv, rv);

  return AddNative(aHost, aPath, aName, aValue, aIsSecure, aIsHttpOnly,
                   aIsSession, aExpiry, &attrs, aSameSite);
}

NS_IMETHODIMP_(nsresult)
nsCookieService::AddNative(const nsACString &aHost,
                           const nsACString &aPath,
                           const nsACString &aName,
                           const nsACString &aValue,
                           bool              aIsSecure,
                           bool              aIsHttpOnly,
                           bool              aIsSession,
                           int64_t           aExpiry,
                           OriginAttributes* aOriginAttributes,
                           int32_t           aSameSite)
{
  if (NS_WARN_IF(!aOriginAttributes)) {
    return NS_ERROR_FAILURE;
  }

  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  AutoRestore<DBState*> savePrevDBState(mDBState);
  mDBState = (aOriginAttributes->mPrivateBrowsingId > 0) ? mPrivateDBState : mDefaultDBState;

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
  nsCookieKey key = nsCookieKey(baseDomain, *aOriginAttributes);

  RefPtr<nsCookie> cookie =
    nsCookie::Create(aName, aValue, host, aPath,
                     aExpiry,
                     currentTimeInUsec,
                     nsCookie::GenerateUniqueCreationTime(currentTimeInUsec),
                     aIsSession,
                     aIsSecure,
                     aIsHttpOnly,
                     key.mOriginAttributes,
                     aSameSite);
  if (!cookie) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  AddInternal(key, cookie, currentTimeInUsec, nullptr, nullptr, true);
  return NS_OK;
}


nsresult
nsCookieService::Remove(const nsACString& aHost, const OriginAttributes& aAttrs,
                        const nsACString& aName, const nsACString& aPath,
                        bool aBlocked)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  AutoRestore<DBState*> savePrevDBState(mDBState);
  mDBState = (aAttrs.mPrivateBrowsingId > 0) ? mPrivateDBState : mDefaultDBState;

  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsListIter matchIter;
  RefPtr<nsCookie> cookie;
  if (FindCookie(nsCookieKey(baseDomain, aAttrs),
                 host,
                 PromiseFlatCString(aName),
                 PromiseFlatCString(aPath),
                 matchIter)) {
    cookie = matchIter.Cookie();
    RemoveCookieFromList(matchIter);
  }

  // check if we need to add the host to the permissions blacklist.
  if (aBlocked && mPermissionService) {
    // strip off the domain dot, if necessary
    if (!host.IsEmpty() && host.First() == '.')
      host.Cut(0, 1);

    host.InsertLiteral("http://", 0);

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), host);

    if (uri)
      mPermissionService->SetAccess(uri, nsICookiePermission::ACCESS_DENY);
  }

  if (cookie) {
    // Everything's done. Notify observers.
    NotifyChanged(cookie, u"deleted");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::Remove(const nsACString &aHost,
                        const nsACString &aName,
                        const nsACString &aPath,
                        bool             aBlocked,
                        JS::HandleValue  aOriginAttributes,
                        JSContext*       aCx,
                        uint8_t          aArgc)
{
  MOZ_ASSERT(aArgc == 0 || aArgc == 1);

  OriginAttributes attrs;
  nsresult rv = InitializeOriginAttributes(&attrs,
                                           aOriginAttributes,
                                           aCx,
                                           aArgc,
                                           u"nsICookieManager.remove()",
                                           u"");
  NS_ENSURE_SUCCESS(rv, rv);

  return RemoveNative(aHost, aName, aPath, aBlocked, &attrs);
}

NS_IMETHODIMP_(nsresult)
nsCookieService::RemoveNative(const nsACString &aHost,
                              const nsACString &aName,
                              const nsACString &aPath,
                              bool aBlocked,
                              OriginAttributes* aOriginAttributes)
{
  if (NS_WARN_IF(!aOriginAttributes)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = Remove(aHost, *aOriginAttributes, aName, aPath, aBlocked);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private file I/O functions
 ******************************************************************************/

// Extract data from a single result row and create an nsCookie.
mozilla::UniquePtr<ConstCookie>
nsCookieService::GetCookieFromRow(mozIStorageStatement *aRow,
                                  const OriginAttributes &aOriginAttributes)
{
  // Skip reading 'baseDomain' -- up to the caller.
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

  // Create a new constCookie and assign the data.
  return mozilla::MakeUnique<ConstCookie>(name,
                                          value,
                                          host,
                                          path,
                                          expiry,
                                          lastAccessed,
                                          creationTime,
                                          isSecure,
                                          isHttpOnly,
                                          aOriginAttributes,
                                          sameSite);
}

void
nsCookieService::EnsureReadComplete(bool aInitDBConn)
{
  MOZ_ASSERT(NS_IsMainThread());

  bool isAccumulated = false;

  if (!mInitializedDBStates) {
    TimeStamp startBlockTime = TimeStamp::Now();
    MonitorAutoLock lock(mMonitor);

    while (!mInitializedDBStates) {
      mMonitor.Wait();
    }
    Telemetry::AccumulateTimeDelta(Telemetry::MOZ_SQLITE_COOKIES_BLOCK_MAIN_THREAD_MS_V2,
                                   startBlockTime);
    Telemetry::Accumulate(Telemetry::MOZ_SQLITE_COOKIES_TIME_TO_BLOCK_MAIN_THREAD_MS, 0);
    isAccumulated = true;
  } else if (!mEndInitDBConn.IsNull()) {
    // We didn't block main thread, and here comes the first cookie request.
    // Collect how close we're going to block main thread.
    Telemetry::Accumulate(Telemetry::MOZ_SQLITE_COOKIES_TIME_TO_BLOCK_MAIN_THREAD_MS,
                          (TimeStamp::Now() - mEndInitDBConn).ToMilliseconds());
    // Nullify the timestamp so wo don't accumulate this telemetry probe again.
    mEndInitDBConn = TimeStamp();
    isAccumulated = true;
  } else if (!mInitializedDBConn && aInitDBConn) {
    // A request comes while we finished cookie thread task and InitDBConn is
    // on the way from cookie thread to main thread. We're very close to block
    // main thread.
    Telemetry::Accumulate(Telemetry::MOZ_SQLITE_COOKIES_TIME_TO_BLOCK_MAIN_THREAD_MS, 0);
    isAccumulated = true;
  }

  if (!mInitializedDBConn && aInitDBConn && mDefaultDBState) {
    InitDBConn();
    if (isAccumulated) {
      // Nullify the timestamp so wo don't accumulate this telemetry probe again.
      mEndInitDBConn = TimeStamp();
    }
  }
}

OpenDBResult
nsCookieService::Read()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);

  // Set up a statement to delete any rows with a nullptr 'baseDomain'
  // column. This takes care of any cookies set by browsers that don't
  // understand the 'baseDomain' column, where the database schema version
  // is from one that does. (This would occur when downgrading.)
  nsresult rv = mDefaultDBState->syncConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
                  "DELETE FROM moz_cookies WHERE baseDomain ISNULL"));
  NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  // Read in the data synchronously.
  // see IDX_NAME, etc. for parameter indexes
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mDefaultDBState->syncConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "name, "
      "value, "
      "host, "
      "path, "
      "expiry, "
      "lastAccessed, "
      "creationTime, "
      "isSecure, "
      "isHttpOnly, "
      "baseDomain, "
      "originAttributes, "
      "sameSite "
    "FROM moz_cookies "
    "WHERE baseDomain NOTNULL"), getter_AddRefs(stmt));

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

    if (!hasResult)
      break;

    // Make sure we haven't already read the data.
    stmt->GetUTF8String(IDX_BASE_DOMAIN, baseDomain);

    nsAutoCString suffix;
    OriginAttributes attrs;
    stmt->GetUTF8String(IDX_ORIGIN_ATTRIBUTES, suffix);
    // If PopulateFromSuffix failed we just ignore the OA attributes
    // that we don't support
    Unused << attrs.PopulateFromSuffix(suffix);

    nsCookieKey key(baseDomain, attrs);
    CookieDomainTuple* tuple = mReadArray.AppendElement();
    tuple->key = key;
    tuple->cookie = GetCookieFromRow(stmt, attrs);
  }

  COOKIE_LOGSTRING(LogLevel::Debug, ("Read(): %zu cookies read", mReadArray.Length()));

  return RESULT_OK;
}

NS_IMETHODIMP
nsCookieService::ImportCookies(nsIFile *aCookieFile)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  // Make sure we're in the default DB state. We don't want people importing
  // cookies into a private browsing session!
  if (mDBState != mDefaultDBState) {
    NS_WARNING("Trying to import cookies in a private browsing session!");
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;
  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), aCookieFile);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) return rv;

  static const char kTrue[] = "TRUE";

  nsAutoCString buffer, baseDomain;
  bool isMore = true;
  int32_t hostIndex, isDomainIndex, pathIndex, secureIndex, expiresIndex, nameIndex, cookieIndex;
  nsACString::char_iterator iter;
  int32_t numInts;
  int64_t expires;
  bool isDomain, isHttpOnly = false;
  uint32_t originalCookieCount = mDefaultDBState->cookieCount;

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
  if (originalCookieCount == 0 && mDefaultDBState->dbConn) {
    mDefaultDBState->stmtInsert->NewBindingParamsArray(getter_AddRefs(paramsArray));
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
    if ((isDomainIndex = buffer.FindChar('\t', hostIndex)     + 1) == 0 ||
        (pathIndex     = buffer.FindChar('\t', isDomainIndex) + 1) == 0 ||
        (secureIndex   = buffer.FindChar('\t', pathIndex)     + 1) == 0 ||
        (expiresIndex  = buffer.FindChar('\t', secureIndex)   + 1) == 0 ||
        (nameIndex     = buffer.FindChar('\t', expiresIndex)  + 1) == 0 ||
        (cookieIndex   = buffer.FindChar('\t', nameIndex)     + 1) == 0) {
      continue;
    }

    // check the expirytime first - if it's expired, ignore
    // nullstomp the trailing tab, to avoid copying the string
    buffer.BeginWriting(iter);
    *(iter += nameIndex - 1) = char(0);
    numInts = PR_sscanf(buffer.get() + expiresIndex, "%lld", &expires);
    if (numInts != 1 || expires < currentTime) {
      continue;
    }

    isDomain = Substring(buffer, isDomainIndex, pathIndex - isDomainIndex - 1).EqualsLiteral(kTrue);
    const nsACString& host = Substring(buffer, hostIndex, isDomainIndex - hostIndex - 1);
    // check for bad legacy cookies (domain not starting with a dot, or containing a port),
    // and discard
    if ((isDomain && !host.IsEmpty() && host.First() != '.') ||
        host.Contains(':')) {
      continue;
    }

    // compute the baseDomain from the host
    rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
    if (NS_FAILED(rv))
      continue;

    // pre-existing cookies have inIsolatedMozBrowser=false set by default
    // constructor of OriginAttributes().
    nsCookieKey key = DEFAULT_APP_KEY(baseDomain);

    // Create a new nsCookie and assign the data. We don't know the cookie
    // creation time, so just use the current time to generate a unique one.
    RefPtr<nsCookie> newCookie =
      nsCookie::Create(Substring(buffer, nameIndex, cookieIndex - nameIndex - 1),
                       Substring(buffer, cookieIndex, buffer.Length() - cookieIndex),
                       host,
                       Substring(buffer, pathIndex, secureIndex - pathIndex - 1),
                       expires,
                       lastAccessedCounter,
                       nsCookie::GenerateUniqueCreationTime(currentTimeInUsec),
                       false,
                       Substring(buffer, secureIndex, expiresIndex - secureIndex - 1).EqualsLiteral(kTrue),
                       isHttpOnly,
                       key.mOriginAttributes,
                       nsICookie2::SAMESITE_UNSET);
    if (!newCookie) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // trick: preserve the most-recently-used cookie ordering,
    // by successively decrementing the lastAccessed time
    lastAccessedCounter--;

    if (originalCookieCount == 0) {
      AddCookieToList(key, newCookie, mDefaultDBState, paramsArray);
    }
    else {
      AddInternal(key, newCookie, currentTimeInUsec,
                  nullptr, nullptr, true);
    }
  }

  // If we need to write to disk, do so now.
  if (paramsArray) {
    uint32_t length;
    paramsArray->GetLength(&length);
    if (length) {
      rv = mDefaultDBState->stmtInsert->BindParameters(paramsArray);
      NS_ASSERT_SUCCESS(rv);
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = mDefaultDBState->stmtInsert->ExecuteAsync(
        mDefaultDBState->insertListener, getter_AddRefs(handle));
      NS_ASSERT_SUCCESS(rv);
    }
  }

  if (mDefaultDBState->cookieCount - originalCookieCount > 0) {
    Telemetry::Accumulate(Telemetry::MOZ_SQLITE_COOKIES_OLD_SCHEMA, 0);
  }

  COOKIE_LOGSTRING(LogLevel::Debug, ("ImportCookies(): %" PRIu32 " cookies imported",
    mDefaultDBState->cookieCount));

  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private GetCookie/SetCookie helpers
 ******************************************************************************/

// helper function for GetCookieList
static inline bool ispathdelimiter(char c) { return c == '/' || c == '?' || c == '#' || c == ';'; }

bool
nsCookieService::DomainMatches(nsCookie* aCookie,
                               const nsACString& aHost)
{
  // first, check for an exact host or domain cookie match, e.g. "google.com"
  // or ".google.com"; second a subdomain match, e.g.
  // host = "mail.google.com", cookie domain = ".google.com".
  return aCookie->RawHost() == aHost ||
      (aCookie->IsDomain() && StringEndsWith(aHost, aCookie->Host()));
}

bool
nsCookieService::IsSameSiteEnabled()
{
  static bool prefInitialized = false;
  if (!prefInitialized) {
    Preferences::AddBoolVarCache(&sSameSiteEnabled,
                                 "network.cookie.same-site.enabled", false);
    prefInitialized = true;
  }
  return sSameSiteEnabled;
}

bool
nsCookieService::PathMatches(nsCookie* aCookie,
                             const nsACString& aPath)
{
  // calculate cookie path length, excluding trailing '/'
  uint32_t cookiePathLen = aCookie->Path().Length();
  if (cookiePathLen > 0 && aCookie->Path().Last() == '/')
    --cookiePathLen;

  // if the given path is shorter than the cookie path, it doesn't match
  // if the given path doesn't start with the cookie path, it doesn't match.
  if (!StringBeginsWith(aPath, Substring(aCookie->Path(), 0, cookiePathLen)))
    return false;

  // if the given path is longer than the cookie path, and the first char after
  // the cookie path is not a path delimiter, it doesn't match.
  if (aPath.Length() > cookiePathLen &&
      !ispathdelimiter(aPath.CharAt(cookiePathLen))) {
    /*
     * |ispathdelimiter| tests four cases: '/', '?', '#', and ';'.
     * '/' is the "standard" case; the '?' test allows a site at host/abc?def
     * to receive a cookie that has a path attribute of abc.  this seems
     * strange but at least one major site (citibank, bug 156725) depends
     * on it.  The test for # and ; are put in to proactively avoid problems
     * with other sites - these are the only other chars allowed in the path.
     */
    return false;
  }

  // either the paths match exactly, or the cookie path is a prefix of
  // the given path.
  return true;
}

void
nsCookieService::GetCookiesForURI(nsIURI *aHostURI,
                                  bool aIsForeign,
                                  bool aIsSafeTopLevelNav,
                                  bool aIsSameSiteForeign,
                                  bool aHttpBound,
                                  const OriginAttributes& aOriginAttrs,
                                  nsTArray<nsCookie*>& aCookieList)
{
  NS_ASSERTION(aHostURI, "null host!");

  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return;
  }

  EnsureReadComplete(true);

  AutoRestore<DBState*> savePrevDBState(mDBState);
  mDBState = (aOriginAttrs.mPrivateBrowsingId > 0) ? mPrivateDBState : mDefaultDBState;

  // get the base domain, host, and path from the URI.
  // e.g. for "www.bbc.co.uk", the base domain would be "bbc.co.uk".
  // file:// URI's (i.e. with an empty host) are allowed, but any other
  // scheme must have a non-empty host. A trailing dot in the host
  // is acceptable.
  bool requireHostMatch;
  nsAutoCString baseDomain, hostFromURI, pathFromURI;
  nsresult rv = GetBaseDomain(mTLDService, aHostURI, baseDomain, requireHostMatch);
  if (NS_SUCCEEDED(rv))
    rv = aHostURI->GetAsciiHost(hostFromURI);
  if (NS_SUCCEEDED(rv))
    rv = aHostURI->GetPathQueryRef(pathFromURI);
  if (NS_FAILED(rv)) {
    COOKIE_LOGFAILURE(GET_COOKIE, aHostURI, nullptr, "invalid host/path from URI");
    return;
  }

  // check default prefs
  uint32_t priorCookieCount = 0;
  CountCookiesFromHost(hostFromURI, &priorCookieCount);
  CookieStatus cookieStatus = CheckPrefs(mPermissionService, mCookieBehavior,
                                         mThirdPartySession,
                                         mThirdPartyNonsecureSession, aHostURI,
                                         aIsForeign, nullptr,
                                         priorCookieCount, aOriginAttrs);

  // for GetCookie(), we don't fire rejection notifications.
  switch (cookieStatus) {
  case STATUS_REJECTED:
  case STATUS_REJECTED_WITH_ERROR:
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
  bool isSecure;
  if (NS_FAILED(aHostURI->SchemeIs("https", &isSecure))) {
    isSecure = false;
  }

  nsCookie *cookie;
  int64_t currentTimeInUsec = PR_Now();
  int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;
  bool stale = false;

  nsCookieKey key(baseDomain, aOriginAttrs);

  // perform the hash lookup
  nsCookieEntry *entry = mDBState->hostTable.GetEntry(key);
  if (!entry)
    return;

  // iterate the cookies!
  const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    cookie = cookies[i];

    // check the host, since the base domain lookup is conservative.
    if (!DomainMatches(cookie, hostFromURI))
      continue;

    // if the cookie is secure and the host scheme isn't, we can't send it
    if (cookie->IsSecure() && !isSecure)
      continue;

    int32_t sameSiteAttr = 0;
    cookie->GetSameSite(&sameSiteAttr);
    if (aIsSameSiteForeign && IsSameSiteEnabled()) {
      // it if's a cross origin request and the cookie is same site only (strict)
      // don't send it
      if (sameSiteAttr == nsICookie2::SAMESITE_STRICT) {
        continue;
      }
      // if it's a cross origin request, the cookie is same site lax, but it's not
      // a top-level navigation, don't send it
      if (sameSiteAttr == nsICookie2::SAMESITE_LAX && !aIsSafeTopLevelNav) {
        continue;
      }
    }

    // if the cookie is httpOnly and it's not going directly to the HTTP
    // connection, don't send it
    if (cookie->IsHttpOnly() && !aHttpBound)
      continue;

    // if the nsIURI path doesn't match the cookie path, don't send it back
    if (!PathMatches(cookie, pathFromURI))
      continue;

    // check if the cookie has expired
    if (cookie->Expiry() <= currentTime) {
      continue;
    }

    // all checks passed - add to list and check if lastAccessed stamp needs updating
    aCookieList.AppendElement(cookie);
    if (cookie->IsStale()) {
      stale = true;
    }
  }

  int32_t count = aCookieList.Length();
  if (count == 0)
    return;

  // update lastAccessed timestamps. we only do this if the timestamp is stale
  // by a certain amount, to avoid thrashing the db during pageload.
  if (stale) {
    // Create an array of parameters to bind to our update statement. Batching
    // is OK here since we're updating cookies with no interleaved operations.
    nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
    mozIStorageAsyncStatement* stmt = mDBState->stmtUpdate;
    if (mDBState->dbConn) {
      stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
    }

    for (int32_t i = 0; i < count; ++i) {
      cookie = aCookieList.ElementAt(i);

      if (cookie->IsStale()) {
        UpdateCookieInList(cookie, currentTimeInUsec, paramsArray);
      }
    }
    // Update the database now if necessary.
    if (paramsArray) {
      uint32_t length;
      paramsArray->GetLength(&length);
      if (length) {
        DebugOnly<nsresult> rv = stmt->BindParameters(paramsArray);
        NS_ASSERT_SUCCESS(rv);
        nsCOMPtr<mozIStoragePendingStatement> handle;
        rv = stmt->ExecuteAsync(mDBState->updateListener,
          getter_AddRefs(handle));
        NS_ASSERT_SUCCESS(rv);
      }
    }
  }

  // return cookies in order of path length; longest to shortest.
  // this is required per RFC2109.  if cookies match in length,
  // then sort by creation time (see bug 236772).
  aCookieList.Sort(CompareCookiesForSending());
}

void
nsCookieService::GetCookieStringInternal(nsIURI *aHostURI,
                                         bool aIsForeign,
                                         bool aIsSafeTopLevelNav,
                                         bool aIsSameSiteForeign,
                                         bool aHttpBound,
                                         const OriginAttributes& aOriginAttrs,
                                         nsCString &aCookieString)
{
  AutoTArray<nsCookie*, 8> foundCookieList;
  GetCookiesForURI(aHostURI, aIsForeign, aIsSafeTopLevelNav, aIsSameSiteForeign,
                   aHttpBound, aOriginAttrs, foundCookieList);

  nsCookie* cookie;
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
        aCookieString += cookie->Name() + NS_LITERAL_CSTRING("=") + cookie->Value();
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
bool
nsCookieService::CanSetCookie(nsIURI*             aHostURI,
                              const nsCookieKey&  aKey,
                              nsCookieAttributes& aCookieAttributes,
                              bool                aRequireHostMatch,
                              CookieStatus        aStatus,
                              nsDependentCString& aCookieHeader,
                              int64_t             aServerTime,
                              bool                aFromHttp,
                              nsIChannel*         aChannel,
                              bool                aLeaveSecureAlone,
                              bool&               aSetCookie,
                              mozIThirdPartyUtil* aThirdPartyUtil)
{
  NS_ASSERTION(aHostURI, "null host!");

  aSetCookie = false;

  // init expiryTime such that session cookies won't prematurely expire
  aCookieAttributes.expiryTime = INT64_MAX;

  // aCookieHeader is an in/out param to point to the next cookie, if
  // there is one. Save the present value for logging purposes
  nsDependentCString savedCookieHeader(aCookieHeader);

  // newCookie says whether there are multiple cookies in the header;
  // so we can handle them separately.
  bool newCookie = ParseAttributes(aCookieHeader, aCookieAttributes);

  // Collect telemetry on how often secure cookies are set from non-secure
  // origins, and vice-versa.
  //
  // 0 = nonsecure and "http:"
  // 1 = nonsecure and "https:"
  // 2 = secure and "http:"
  // 3 = secure and "https:"
  bool isHTTPS;
  nsresult rv = aHostURI->SchemeIs("https", &isHTTPS);
  if (NS_SUCCEEDED(rv)) {
    Telemetry::Accumulate(Telemetry::COOKIE_SCHEME_SECURITY,
                          ((aCookieAttributes.isSecure)? 0x02 : 0x00) |
                          ((isHTTPS)? 0x01 : 0x00));

    // Collect telemetry on how often are first- and third-party cookies set
    // from HTTPS origins:
    //
    // 0 (000) = first-party and "http:"
    // 1 (001) = first-party and "http:" with bogus Secure cookie flag?!
    // 2 (010) = first-party and "https:"
    // 3 (011) = first-party and "https:" with Secure cookie flag
    // 4 (100) = third-party and "http:"
    // 5 (101) = third-party and "http:" with bogus Secure cookie flag?!
    // 6 (110) = third-party and "https:"
    // 7 (111) = third-party and "https:" with Secure cookie flag
    if (aThirdPartyUtil) {
      bool isThirdParty = true;
      aThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isThirdParty);
      Telemetry::Accumulate(Telemetry::COOKIE_SCHEME_HTTPS,
                            (isThirdParty ? 0x04 : 0x00) |
                            (isHTTPS ? 0x02 : 0x00) |
                            (aCookieAttributes.isSecure ? 0x01 : 0x00));
    }
  }

  int64_t currentTimeInUsec = PR_Now();

  // calculate expiry time of cookie.
  aCookieAttributes.isSession = GetExpiry(aCookieAttributes, aServerTime,
                                         currentTimeInUsec / PR_USEC_PER_SEC);
  if (aStatus == STATUS_ACCEPT_SESSION) {
    // force lifetime to session. note that the expiration time, if set above,
    // will still apply.
    aCookieAttributes.isSession = true;
  }

  // reject cookie if it's over the size limit, per RFC2109
  if ((aCookieAttributes.name.Length() + aCookieAttributes.value.Length()) > kMaxBytesPerCookie) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "cookie too big (> 4kb)");
    return newCookie;
  }

  const char illegalNameCharacters[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                         0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                         0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,
                                         0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                                         0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
                                         0x1F, 0x00 };
  if (aCookieAttributes.name.FindCharInSet(illegalNameCharacters, 0) != -1) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "invalid name character");
    return newCookie;
  }

  // domain & path checks
  if (!CheckDomain(aCookieAttributes, aHostURI, aKey.mBaseDomain, aRequireHostMatch)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "failed the domain tests");
    return newCookie;
  }
  if (!CheckPath(aCookieAttributes, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "failed the path tests");
    return newCookie;
  }
  // magic prefix checks. MUST be run after CheckDomain() and CheckPath()
  if (!CheckPrefixes(aCookieAttributes, isHTTPS)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "failed the prefix tests");
    return newCookie;
  }

  // reject cookie if value contains an RFC 6265 disallowed character - see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1191423
  // NOTE: this is not the full set of characters disallowed by 6265 - notably
  // 0x09, 0x20, 0x22, 0x2C, 0x5C, and 0x7F are missing from this list. This is
  // for parity with Chrome. This only applies to cookies set via the Set-Cookie
  // header, as document.cookie is defined to be UTF-8. Hooray for
  // symmetry!</sarcasm>
  const char illegalCharacters[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                     0x08, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                     0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
                                     0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
                                     0x1E, 0x1F, 0x3B, 0x00 };
  if (aFromHttp && (aCookieAttributes.value.FindCharInSet(illegalCharacters, 0) != -1)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "invalid value character");
    return newCookie;
  }

  // if the new cookie is httponly, make sure we're not coming from script
  if (!aFromHttp && aCookieAttributes.isHttpOnly) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader,
      "cookie is httponly; coming from script");
    return newCookie;
  }

  bool isSecure = true;
  if (aHostURI) {
    aHostURI->SchemeIs("https", &isSecure);
  }

  // If the new cookie is non-https and wants to set secure flag,
  // browser have to ignore this new cookie.
  // (draft-ietf-httpbis-cookie-alone section 3.1)
  if (aLeaveSecureAlone && aCookieAttributes.isSecure && !isSecure) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
      "non-https cookie can't set secure flag");
    Telemetry::Accumulate(Telemetry::COOKIE_LEAVE_SECURE_ALONE,
                          BLOCKED_SECURE_SET_FROM_HTTP);
    return newCookie;
  }

  // If the new cookie is same-site but in a cross site context,
  // browser must ignore the cookie.
  if ((aCookieAttributes.sameSite != nsICookie2::SAMESITE_UNSET) &&
      aThirdPartyUtil &&
      IsSameSiteEnabled()) {

    // Do not treat loads triggered by web extensions as foreign
    bool addonAllowsLoad = false;
    if (aChannel) {
      nsCOMPtr<nsIURI> channelURI;
      NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
      nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
      addonAllowsLoad = loadInfo &&
        BasePrincipal::Cast(loadInfo->TriggeringPrincipal())->
          AddonAllowsLoad(channelURI);
    }

    if (!addonAllowsLoad) {
      bool isThirdParty = false;
      nsresult rv = aThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isThirdParty);
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
bool
nsCookieService::SetCookieInternal(nsIURI                        *aHostURI,
                                   const mozilla::nsCookieKey    &aKey,
                                   bool                           aRequireHostMatch,
                                   CookieStatus                   aStatus,
                                   nsDependentCString            &aCookieHeader,
                                   int64_t                        aServerTime,
                                   bool                           aFromHttp,
                                   nsIChannel                    *aChannel)
{
  NS_ASSERTION(aHostURI, "null host!");
  bool canSetCookie = false;
  nsDependentCString savedCookieHeader(aCookieHeader);
  nsCookieAttributes cookieAttributes;
  bool newCookie = CanSetCookie(aHostURI, aKey, cookieAttributes, aRequireHostMatch,
                                aStatus, aCookieHeader, aServerTime, aFromHttp,
                                aChannel, mLeaveSecureAlone, canSetCookie,
                                mThirdPartyUtil);

  if (!canSetCookie) {
    return newCookie;
  }

  int64_t currentTimeInUsec = PR_Now();
  // create a new nsCookie and copy attributes
  RefPtr<nsCookie> cookie =
    nsCookie::Create(cookieAttributes.name,
                     cookieAttributes.value,
                     cookieAttributes.host,
                     cookieAttributes.path,
                     cookieAttributes.expiryTime,
                     currentTimeInUsec,
                     nsCookie::GenerateUniqueCreationTime(currentTimeInUsec),
                     cookieAttributes.isSession,
                     cookieAttributes.isSecure,
                     cookieAttributes.isHttpOnly,
                     aKey.mOriginAttributes,
                     cookieAttributes.sameSite);
  if (!cookie)
    return newCookie;

  // check permissions from site permission list, or ask the user,
  // to determine if we can set the cookie
  if (mPermissionService) {
    bool permission;
    mPermissionService->CanSetCookie(aHostURI,
                                     aChannel,
                                     static_cast<nsICookie2*>(static_cast<nsCookie*>(cookie)),
                                     &cookieAttributes.isSession,
                                     &cookieAttributes.expiryTime,
                                     &permission);
    if (!permission) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "cookie rejected by permission manager");
      NotifyRejected(aHostURI);
      return newCookie;
    }

    // update isSession and expiry attributes, in case they changed
    cookie->SetIsSession(cookieAttributes.isSession);
    cookie->SetExpiry(cookieAttributes.expiryTime);
  }

  // add the cookie to the list. AddInternal() takes care of logging.
  // we get the current time again here, since it may have changed during prompting
  AddInternal(aKey, cookie, PR_Now(), aHostURI, savedCookieHeader.get(),
              aFromHttp);
  return newCookie;
}

// this is a backend function for adding a cookie to the list, via SetCookie.
// also used in the cookie manager, for profile migration from IE.
// it either replaces an existing cookie; or adds the cookie to the hashtable,
// and deletes a cookie (if maximum number of cookies has been
// reached). also performs list maintenance by removing expired cookies.
void
nsCookieService::AddInternal(const nsCookieKey &aKey,
                             nsCookie          *aCookie,
                             int64_t            aCurrentTimeInUsec,
                             nsIURI            *aHostURI,
                             const char        *aCookieHeader,
                             bool               aFromHttp)
{
  MOZ_ASSERT(mInitializedDBStates);
  MOZ_ASSERT(mInitializedDBConn);

  int64_t currentTime = aCurrentTimeInUsec / PR_USEC_PER_SEC;

  nsListIter exactIter;
  bool foundCookie = false;
  foundCookie = FindCookie(aKey, aCookie->Host(),
                           aCookie->Name(), aCookie->Path(), exactIter);
  bool foundSecureExact = foundCookie && exactIter.Cookie()->IsSecure();
  bool isSecure = true;
  if (aHostURI && NS_FAILED(aHostURI->SchemeIs("https", &isSecure)))  {
    isSecure = false;
  }
  bool oldCookieIsSession = false;
  if (mLeaveSecureAlone) {
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
    if (!aCookie->IsSecure()
         && (foundSecureExact || FindSecureCookie(aKey, aCookie))) {
      if (!isSecure) {
        COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
          "cookie can't save because older cookie is secure cookie but newer cookie is non-secure cookie");
        if (foundSecureExact) {
          Telemetry::Accumulate(Telemetry::COOKIE_LEAVE_SECURE_ALONE,
                                BLOCKED_DOWNGRADE_SECURE_EXACT);
        } else {
          Telemetry::Accumulate(Telemetry::COOKIE_LEAVE_SECURE_ALONE,
                                BLOCKED_DOWNGRADE_SECURE_INEXACT);
        }
        return;
      }
      // A secure site is allowed to downgrade a secure cookie
      // but we want to measure anyway.
      if (foundSecureExact) {
        Telemetry::Accumulate(Telemetry::COOKIE_LEAVE_SECURE_ALONE,
                              DOWNGRADE_SECURE_FROM_SECURE_EXACT);
      } else {
        Telemetry::Accumulate(Telemetry::COOKIE_LEAVE_SECURE_ALONE,
                              DOWNGRADE_SECURE_FROM_SECURE_INEXACT);
      }
    }
  }

  RefPtr<nsCookie> oldCookie;
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
        COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
          "previously stored cookie is httponly; coming from script");
        return;
      }

      // If the new cookie has the same value, expiry date, and isSecure,
      // isSession, and isHttpOnly flags then we can just keep the old one.
      // Only if any of these differ we would want to override the cookie.
      if (oldCookie->Value().Equals(aCookie->Value()) &&
          oldCookie->Expiry() == aCookie->Expiry() &&
          oldCookie->IsSecure() == aCookie->IsSecure() &&
          oldCookie->IsSession() == aCookie->IsSession() &&
          oldCookie->IsHttpOnly() == aCookie->IsHttpOnly() &&
          // We don't want to perform this optimization if the cookie is
          // considered stale, since in this case we would need to update the
          // database.
          !oldCookie->IsStale()) {
        // Update the last access time on the old cookie.
        oldCookie->SetLastAccessed(aCookie->LastAccessed());
        UpdateCookieOldestTime(mDBState, oldCookie);
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
    nsCookieEntry *entry = mDBState->hostTable.GetEntry(aKey);
    if (entry && entry->GetCookies().Length() >= mMaxCookiesPerHost) {
      nsListIter iter;
      // Prioritize evicting insecure cookies.
      // (draft-ietf-httpbis-cookie-alone section 3.3)
      mozilla::Maybe<bool> optionalSecurity = mLeaveSecureAlone ? Some(false) : Nothing();
      int64_t oldestCookieTime = FindStaleCookie(entry, currentTime, aHostURI, optionalSecurity, iter);
      if (iter.entry == nullptr) {
        if (aCookie->IsSecure()) {
          // It's valid to evict a secure cookie for another secure cookie.
          oldestCookieTime = FindStaleCookie(entry, currentTime, aHostURI, Some(true), iter);
        } else {
          Telemetry::Accumulate(Telemetry::COOKIE_LEAVE_SECURE_ALONE,
                                EVICTING_SECURE_BLOCKED);
          COOKIE_LOGEVICTED(aCookie,
            "Too many cookies for this domain and the new cookie is not a secure cookie");
          return;
        }
      }

      MOZ_ASSERT(iter.entry);

      oldCookie = iter.Cookie();
      if (oldestCookieTime > 0 && mLeaveSecureAlone) {
        TelemetryForEvictingStaleCookie(oldCookie, oldestCookieTime);
      }

      // remove the oldest cookie from the domain
      RemoveCookieFromList(iter);
      COOKIE_LOGEVICTED(oldCookie, "Too many cookies for this domain");
      purgedList = CreatePurgeList(oldCookie);
    } else if (mDBState->cookieCount >= ADD_TEN_PERCENT(mMaxNumberOfCookies)) {
      int64_t maxAge = aCurrentTimeInUsec - mDBState->cookieOldestTime;
      int64_t purgeAge = ADD_TEN_PERCENT(mCookiePurgeAge);
      if (maxAge >= purgeAge) {
        // we're over both size and age limits by 10%; time to purge the table!
        // do this by:
        // 1) removing expired cookies;
        // 2) evicting the balance of old cookies until we reach the size limit.
        // note that the cookieOldestTime indicator can be pessimistic - if it's
        // older than the actual oldest cookie, we'll just purge more eagerly.
        purgedList = PurgeCookies(aCurrentTimeInUsec);
      }
    }
  }

  // Add the cookie to the db. We do not supply a params array for batching
  // because this might result in removals and additions being out of order.
  AddCookieToList(aKey, aCookie, mDBState, nullptr);
  COOKIE_LOGSUCCESS(SET_COOKIE, aHostURI, aCookieHeader, aCookie, foundCookie);

  // Now that list mutations are complete, notify observers. We do it here
  // because observers may themselves attempt to mutate the list.
  if (purgedList) {
    NotifyChanged(purgedList, u"batch-deleted");
  }

  NotifyChanged(aCookie, foundCookie ? u"changed" : u"added", oldCookieIsSession, aFromHttp);
}

/******************************************************************************
 * nsCookieService impl:
 * private cookie header parsing functions
 ******************************************************************************/

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

    5. cookie <NAME> is optional, where spec requires it. This is a fairly
       trivial case, but allows the flexibility of setting only a cookie <VALUE>
       with a blank <NAME> and is required by some sites (see bug 169091).

    6. Attribute "HttpOnly", not covered in the RFCs, is supported
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

// helper functions for GetTokenValue
static inline bool iswhitespace     (char c) { return c == ' '  || c == '\t'; }
static inline bool isterminator     (char c) { return c == '\n' || c == '\r'; }
static inline bool isvalueseparator (char c) { return isterminator(c) || c == ';'; }
static inline bool istokenseparator (char c) { return isvalueseparator(c) || c == '='; }

// Parse a single token/value pair.
// Returns true if a cookie terminator is found, so caller can parse new cookie.
bool
nsCookieService::GetTokenValue(nsACString::const_char_iterator &aIter,
                               nsACString::const_char_iterator &aEndIter,
                               nsDependentCSubstring                         &aTokenString,
                               nsDependentCSubstring                         &aTokenValue,
                               bool                                          &aEqualsFound)
{
  nsACString::const_char_iterator start, lastSpace;
  // initialize value string to clear garbage
  aTokenValue.Rebind(aIter, aIter);

  // find <token>, including any <LWS> between the end-of-token and the
  // token separator. we'll remove trailing <LWS> next
  while (aIter != aEndIter && iswhitespace(*aIter))
    ++aIter;
  start = aIter;
  while (aIter != aEndIter && !istokenseparator(*aIter))
    ++aIter;

  // remove trailing <LWS>; first check we're not at the beginning
  lastSpace = aIter;
  if (lastSpace != start) {
    while (--lastSpace != start && iswhitespace(*lastSpace))
      continue;
    ++lastSpace;
  }
  aTokenString.Rebind(start, lastSpace);

  aEqualsFound = (*aIter == '=');
  if (aEqualsFound) {
    // find <value>
    while (++aIter != aEndIter && iswhitespace(*aIter))
      continue;

    start = aIter;

    // process <token>
    // just look for ';' to terminate ('=' allowed)
    while (aIter != aEndIter && !isvalueseparator(*aIter))
      ++aIter;

    // remove trailing <LWS>; first check we're not at the beginning
    if (aIter != start) {
      lastSpace = aIter;
      while (--lastSpace != start && iswhitespace(*lastSpace))
        continue;
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

// Parses attributes from cookie header. expires/max-age attributes aren't folded into the
// cookie struct here, because we don't know which one to use until we've parsed the header.
bool
nsCookieService::ParseAttributes(nsDependentCString &aCookieHeader,
                                 nsCookieAttributes &aCookieAttributes)
{
  static const char kPath[]    = "path";
  static const char kDomain[]  = "domain";
  static const char kExpires[] = "expires";
  static const char kMaxage[]  = "max-age";
  static const char kSecure[]  = "secure";
  static const char kHttpOnly[]  = "httponly";
  static const char kSameSite[]       = "samesite";
  static const char kSameSiteLax[]    = "lax";
  static const char kSameSiteStrict[]    = "strict";

  nsACString::const_char_iterator tempBegin, tempEnd;
  nsACString::const_char_iterator cookieStart, cookieEnd;
  aCookieHeader.BeginReading(cookieStart);
  aCookieHeader.EndReading(cookieEnd);

  aCookieAttributes.isSecure = false;
  aCookieAttributes.isHttpOnly = false;
  aCookieAttributes.sameSite = nsICookie2::SAMESITE_UNSET;

  nsDependentCSubstring tokenString(cookieStart, cookieStart);
  nsDependentCSubstring tokenValue (cookieStart, cookieStart);
  bool newCookie, equalsFound;

  // extract cookie <NAME> & <VALUE> (first attribute), and copy the strings.
  // if we find multiple cookies, return for processing
  // note: if there's no '=', we assume token is <VALUE>. this is required by
  //       some sites (see bug 169091).
  // XXX fix the parser to parse according to <VALUE> grammar for this case
  newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue, equalsFound);
  if (equalsFound) {
    aCookieAttributes.name = tokenString;
    aCookieAttributes.value = tokenValue;
  } else {
    aCookieAttributes.value = tokenString;
  }

  // extract remaining attributes
  while (cookieStart != cookieEnd && !newCookie) {
    newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue, equalsFound);

    if (!tokenValue.IsEmpty()) {
      tokenValue.BeginReading(tempBegin);
      tokenValue.EndReading(tempEnd);
    }

    // decide which attribute we have, and copy the string
    if (tokenString.LowerCaseEqualsLiteral(kPath))
      aCookieAttributes.path = tokenValue;

    else if (tokenString.LowerCaseEqualsLiteral(kDomain))
      aCookieAttributes.host = tokenValue;

    else if (tokenString.LowerCaseEqualsLiteral(kExpires))
      aCookieAttributes.expires = tokenValue;

    else if (tokenString.LowerCaseEqualsLiteral(kMaxage))
      aCookieAttributes.maxage = tokenValue;

    // ignore any tokenValue for isSecure; just set the boolean
    else if (tokenString.LowerCaseEqualsLiteral(kSecure))
      aCookieAttributes.isSecure = true;

    // ignore any tokenValue for isHttpOnly (see bug 178993);
    // just set the boolean
    else if (tokenString.LowerCaseEqualsLiteral(kHttpOnly))
      aCookieAttributes.isHttpOnly = true;

    else if (tokenString.LowerCaseEqualsLiteral(kSameSite)) {
      if (tokenValue.LowerCaseEqualsLiteral(kSameSiteLax)) {
        aCookieAttributes.sameSite = nsICookie2::SAMESITE_LAX;
      } else if (tokenValue.LowerCaseEqualsLiteral(kSameSiteStrict)) {
        aCookieAttributes.sameSite = nsICookie2::SAMESITE_STRICT;
      }
    }
  }

  // rebind aCookieHeader, in case we need to process another cookie
  aCookieHeader.Rebind(cookieStart, cookieEnd);
  return newCookie;
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
nsresult
nsCookieService::GetBaseDomain(nsIEffectiveTLDService *aTLDService,
                               nsIURI    *aHostURI,
                               nsCString &aBaseDomain,
                               bool      &aRequireHostMatch)
{
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
  if (aBaseDomain.IsEmpty()) {
    bool isFileURI = false;
    aHostURI->SchemeIs("file", &isFileURI);
    if (!isFileURI)
      return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

// Get the base domain for aHost; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". This is done differently than GetBaseDomain(mTLDService, ): it is assumed
// that aHost is already normalized, and it may contain a leading dot
// (indicating that it represents a domain). A trailing dot may be present.
// If aHost is an IP address, an alias such as 'localhost', an eTLD such as
// 'co.uk', or the empty string, aBaseDomain will be the exact host, and a
// leading dot will be treated as an error.
nsresult
nsCookieService::GetBaseDomainFromHost(nsIEffectiveTLDService *aTLDService,
                                       const nsACString &aHost,
                                       nsCString        &aBaseDomain)
{
  // aHost must not be the string '.'.
  if (aHost.Length() == 1 && aHost.Last() == '.')
    return NS_ERROR_INVALID_ARG;

  // aHost may contain a leading dot; if so, strip it now.
  bool domain = !aHost.IsEmpty() && aHost.First() == '.';

  // get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = aTLDService->GetBaseDomainFromHost(Substring(aHost, domain), 0, aBaseDomain);
  if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
    // aHost is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. use the host as a key in such
    // cases; however, we reject any such hosts with a leading dot, since it
    // doesn't make sense for them to be domain cookies.
    if (domain)
      return NS_ERROR_INVALID_ARG;

    aBaseDomain = aHost;
    return NS_OK;
  }
  return rv;
}

// Normalizes the given hostname, component by component. ASCII/ACE
// components are lower-cased, and UTF-8 components are normalized per
// RFC 3454 and converted to ACE.
nsresult
nsCookieService::NormalizeHost(nsCString &aHost)
{
  if (!IsASCII(aHost)) {
    nsAutoCString host;
    nsresult rv = mIDNService->ConvertUTF8toACE(aHost, host);
    if (NS_FAILED(rv))
      return rv;

    aHost = host;
  }

  ToLowerCase(aHost);
  return NS_OK;
}

// returns true if 'a' is equal to or a subdomain of 'b',
// assuming no leading dots are present.
static inline bool IsSubdomainOf(const nsCString &a, const nsCString &b)
{
  if (a == b)
    return true;
  if (a.Length() > b.Length())
    return a[a.Length() - b.Length() - 1] == '.' && StringEndsWith(a, b);
  return false;
}

CookieStatus
nsCookieService::CheckPrefs(nsICookiePermission    *aPermissionService,
                            uint8_t                 aCookieBehavior,
                            bool                    aThirdPartySession,
                            bool                    aThirdPartyNonsecureSession,
                            nsIURI                 *aHostURI,
                            bool                    aIsForeign,
                            const char             *aCookieHeader,
                            const int               aNumOfCookies,
                            const OriginAttributes &aOriginAttrs)
{
  nsresult rv;

  // don't let ftp sites get/set cookies (could be a security issue)
  bool ftp;
  if (NS_SUCCEEDED(aHostURI->SchemeIs("ftp", &ftp)) && ftp) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "ftp sites cannot read cookies");
    return STATUS_REJECTED_WITH_ERROR;
  }

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateCodebasePrincipal(aHostURI, aOriginAttrs);

  if (!principal) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "non-codebase principals cannot get/set cookies");
    return STATUS_REJECTED_WITH_ERROR;
  }

  // check the permission list first; if we find an entry, it overrides
  // default prefs. see bug 184059.
  if (aPermissionService) {
    nsCookieAccess access;
    // Not passing an nsIChannel here is probably OK; our implementation
    // doesn't do anything with it anyway.
    rv = aPermissionService->CanAccess(principal, &access);

    // if we found an entry, use it
    if (NS_SUCCEEDED(rv)) {
      switch (access) {
      case nsICookiePermission::ACCESS_DENY:
        COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI,
                          aCookieHeader, "cookies are blocked for this site");
        return STATUS_REJECTED;

      case nsICookiePermission::ACCESS_ALLOW:
        return STATUS_ACCEPTED;

      case nsICookiePermission::ACCESS_ALLOW_FIRST_PARTY_ONLY:
        if (aIsForeign) {
          COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI,
                            aCookieHeader, "third party cookies are blocked "
                            "for this site");
          return STATUS_REJECTED;

        }
        return STATUS_ACCEPTED;

      case nsICookiePermission::ACCESS_LIMIT_THIRD_PARTY:
        if (!aIsForeign)
          return STATUS_ACCEPTED;
        if (aNumOfCookies == 0) {
          COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI,
                            aCookieHeader, "third party cookies are blocked "
                            "for this site");
          return STATUS_REJECTED;
        }
        return STATUS_ACCEPTED;
      }
    }
  }

  // check default prefs
  if (aCookieBehavior == nsICookieService::BEHAVIOR_REJECT) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "cookies are disabled");
    return STATUS_REJECTED;
  }

  // check if cookie is foreign
  if (aIsForeign) {
    if (aCookieBehavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN) {
      COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "context is third party");
      return STATUS_REJECTED;
    }

    if (aCookieBehavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN) {
      if (aNumOfCookies == 0) {
        COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "context is third party");
        return STATUS_REJECTED;
      }
    }

    MOZ_ASSERT(aCookieBehavior == nsICookieService::BEHAVIOR_ACCEPT ||
               aCookieBehavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN);

    if (aThirdPartySession)
      return STATUS_ACCEPT_SESSION;

    if (aThirdPartyNonsecureSession) {
      bool isHTTPS = false;
      aHostURI->SchemeIs("https", &isHTTPS);
      if (!isHTTPS)
        return STATUS_ACCEPT_SESSION;
    }
  }

  // if nothing has complained, accept cookie
  return STATUS_ACCEPTED;
}

// processes domain attribute, and returns true if host has permission to set for this domain.
bool
nsCookieService::CheckDomain(nsCookieAttributes &aCookieAttributes,
                             nsIURI             *aHostURI,
                             const nsCString    &aBaseDomain,
                             bool                aRequireHostMatch)
{
  // Note: The logic in this function is mirrored in
  // toolkit/components/extensions/ext-cookies.js:checkSetCookiePermissions().
  // If it changes, please update that function, or file a bug for someone
  // else to do so.

  // get host from aHostURI
  nsAutoCString hostFromURI;
  aHostURI->GetAsciiHost(hostFromURI);

  // if a domain is given, check the host has permission
  if (!aCookieAttributes.host.IsEmpty()) {
    // Tolerate leading '.' characters, but not if it's otherwise an empty host.
    if (aCookieAttributes.host.Length() > 1 &&
        aCookieAttributes.host.First() == '.') {
      aCookieAttributes.host.Cut(0, 1);
    }

    // switch to lowercase now, to avoid case-insensitive compares everywhere
    ToLowerCase(aCookieAttributes.host);

    // check whether the host is either an IP address, an alias such as
    // 'localhost', an eTLD such as 'co.uk', or the empty string. in these
    // cases, require an exact string match for the domain, and leave the cookie
    // as a non-domain one. bug 105917 originally noted the requirement to deal
    // with IP addresses.
    if (aRequireHostMatch)
      return hostFromURI.Equals(aCookieAttributes.host);

    // ensure the proposed domain is derived from the base domain; and also
    // that the host domain is derived from the proposed domain (per RFC2109).
    if (IsSubdomainOf(aCookieAttributes.host, aBaseDomain) &&
        IsSubdomainOf(hostFromURI, aCookieAttributes.host)) {
      // prepend a dot to indicate a domain cookie
      aCookieAttributes.host.InsertLiteral(".", 0);
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
  aCookieAttributes.host = hostFromURI;
  return true;
}

nsCString
nsCookieService::GetPathFromURI(nsIURI* aHostURI)
{
  // strip down everything after the last slash to get the path,
  // ignoring slashes in the query string part.
  // if we can QI to nsIURL, that'll take care of the query string portion.
  // otherwise, it's not an nsIURL and can't have a query string, so just find the last slash.
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
  return path;
}

bool
nsCookieService::CheckPath(nsCookieAttributes &aCookieAttributes,
                           nsIURI             *aHostURI)
{
  // if a path is given, check the host has permission
  if (aCookieAttributes.path.IsEmpty() || aCookieAttributes.path.First() != '/') {
    aCookieAttributes.path = GetPathFromURI(aHostURI);

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
        !StringBeginsWith(pathFromURI, aCookieAttributes.path)) {
      return false;
    }
#endif
  }

  if (aCookieAttributes.path.Length() > kMaxBytesPerPath ||
      aCookieAttributes.path.Contains('\t'))
    return false;

  return true;
}

// CheckPrefixes
//
// Reject cookies whose name starts with the magic prefixes from
// https://tools.ietf.org/html/draft-ietf-httpbis-cookie-prefixes-00
// if they do not meet the criteria required by the prefix.
//
// Must not be called until after CheckDomain() and CheckPath() have
// regularized and validated the nsCookieAttributes values!
bool
nsCookieService::CheckPrefixes(nsCookieAttributes &aCookieAttributes,
                               bool aSecureRequest)
{
  static const char kSecure[] = "__Secure-";
  static const char kHost[]   = "__Host-";
  static const int kSecureLen = sizeof( kSecure ) - 1;
  static const int kHostLen   = sizeof( kHost ) - 1;

  bool isSecure = strncmp( aCookieAttributes.name.get(), kSecure, kSecureLen ) == 0;
  bool isHost   = strncmp( aCookieAttributes.name.get(), kHost, kHostLen ) == 0;

  if ( !isSecure && !isHost ) {
    // not one of the magic prefixes: carry on
    return true;
  }

  if ( !aSecureRequest || !aCookieAttributes.isSecure ) {
    // the magic prefixes may only be used from a secure request and
    // the secure attribute must be set on the cookie
    return false;
  }

  if ( isHost ) {
    // The host prefix requires that the path is "/" and that the cookie
    // had no domain attribute. CheckDomain() and CheckPath() MUST be run
    // first to make sure invalid attributes are rejected and to regularlize
    // them. In particular all explicit domain attributes result in a host
    // that starts with a dot, and if the host doesn't start with a dot it
    // correctly matches the true host.
    if ( aCookieAttributes.host[0] == '.' ||
         !aCookieAttributes.path.EqualsLiteral( "/" )) {
      return false;
    }
  }

  return true;
}

bool
nsCookieService::GetExpiry(nsCookieAttributes &aCookieAttributes,
                           int64_t             aServerTime,
                           int64_t             aCurrentTime)
{
  /* Determine when the cookie should expire. This is done by taking the difference between
   * the server time and the time the server wants the cookie to expire, and adding that
   * difference to the client time. This localizes the client time regardless of whether or
   * not the TZ environment variable was set on the client.
   *
   * Note: We need to consider accounting for network lag here, per RFC.
   */
  // check for max-age attribute first; this overrides expires attribute
  if (!aCookieAttributes.maxage.IsEmpty()) {
    // obtain numeric value of maxageAttribute
    int64_t maxage;
    int32_t numInts = PR_sscanf(aCookieAttributes.maxage.get(), "%lld", &maxage);

    // default to session cookie if the conversion failed
    if (numInts != 1) {
      return true;
    }

    // if this addition overflows, expiryTime will be less than currentTime
    // and the cookie will be expired - that's okay.
    aCookieAttributes.expiryTime = aCurrentTime + maxage;

  // check for expires attribute
  } else if (!aCookieAttributes.expires.IsEmpty()) {
    PRTime expires;

    // parse expiry time
    if (PR_ParseTimeString(aCookieAttributes.expires.get(), true, &expires) != PR_SUCCESS) {
      return true;
    }

    // If set-cookie used absolute time to set expiration, and it can't use
    // client time to set expiration.
    // Because if current time be set in the future, but the cookie expire
    // time be set less than current time and more than server time.
    // The cookie item have to be used to the expired cookie.
    aCookieAttributes.expiryTime = expires / int64_t(PR_USEC_PER_SEC);

  // default to session cookie if no attributes found
  } else {
    return true;
  }

  return false;
}

/******************************************************************************
 * nsCookieService impl:
 * private cookielist management functions
 ******************************************************************************/

void
nsCookieService::RemoveAllFromMemory()
{
  // clearing the hashtable will call each nsCookieEntry's dtor,
  // which releases all their respective children.
  mDBState->hostTable.Clear();
  mDBState->cookieCount = 0;
  mDBState->cookieOldestTime = INT64_MAX;
}

// comparator class for lastaccessed times of cookies.
class CompareCookiesByAge {
public:
  bool Equals(const nsListIter &a, const nsListIter &b) const
  {
    return a.Cookie()->LastAccessed() == b.Cookie()->LastAccessed() &&
           a.Cookie()->CreationTime() == b.Cookie()->CreationTime();
  }

  bool LessThan(const nsListIter &a, const nsListIter &b) const
  {
    // compare by lastAccessed time, and tiebreak by creationTime.
    int64_t result = a.Cookie()->LastAccessed() - b.Cookie()->LastAccessed();
    if (result != 0)
      return result < 0;

    return a.Cookie()->CreationTime() < b.Cookie()->CreationTime();
  }
};

// comparator class for sorting cookies by entry and index.
class CompareCookiesByIndex {
public:
  bool Equals(const nsListIter &a, const nsListIter &b) const
  {
    NS_ASSERTION(a.entry != b.entry || a.index != b.index,
      "cookie indexes should never be equal");
    return false;
  }

  bool LessThan(const nsListIter &a, const nsListIter &b) const
  {
    // compare by entryclass pointer, then by index.
    if (a.entry != b.entry)
      return a.entry < b.entry;

    return a.index < b.index;
  }
};

// purges expired and old cookies in a batch operation.
already_AddRefed<nsIArray>
nsCookieService::PurgeCookies(int64_t aCurrentTimeInUsec)
{
  NS_ASSERTION(mDBState->hostTable.Count() > 0, "table is empty");

  uint32_t initialCookieCount = mDBState->cookieCount;
  COOKIE_LOGSTRING(LogLevel::Debug,
    ("PurgeCookies(): beginning purge with %" PRIu32 " cookies and %" PRId64 " oldest age",
     mDBState->cookieCount, aCurrentTimeInUsec - mDBState->cookieOldestTime));

  typedef nsTArray<nsListIter> PurgeList;
  PurgeList purgeList(kMaxNumberOfCookies);

  nsCOMPtr<nsIMutableArray> removedList = do_CreateInstance(NS_ARRAY_CONTRACTID);

  // Create a params array to batch the removals. This is OK here because
  // all the removals are in order, and there are no interleaved additions.
  mozIStorageAsyncStatement *stmt = mDBState->stmtDelete;
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  if (mDBState->dbConn) {
    stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  }

  int64_t currentTime = aCurrentTimeInUsec / PR_USEC_PER_SEC;
  int64_t purgeTime = aCurrentTimeInUsec - mCookiePurgeAge;
  int64_t oldestTime = INT64_MAX;

  for (auto iter = mDBState->hostTable.Iter(); !iter.Done(); iter.Next()) {
    nsCookieEntry* entry = iter.Get();

    const nsCookieEntry::ArrayType& cookies = entry->GetCookies();
    auto length = cookies.Length();
    for (nsCookieEntry::IndexType i = 0; i < length; ) {
      nsListIter iter(entry, i);
      nsCookie* cookie = cookies[i];

      // check if the cookie has expired
      if (cookie->Expiry() <= currentTime) {
        removedList->AppendElement(cookie);
        COOKIE_LOGEVICTED(cookie, "Cookie expired");

        // remove from list; do not increment our iterator, but stop if we're
        // done already.
        gCookieService->RemoveCookieFromList(iter, paramsArray);
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

  uint32_t postExpiryCookieCount = mDBState->cookieCount;

  // now we have a list of iterators for cookies over the age limit.
  // sort them by age, and then we'll see how many to remove...
  purgeList.Sort(CompareCookiesByAge());

  // only remove old cookies until we reach the max cookie limit, no more.
  uint32_t excess = mDBState->cookieCount > mMaxNumberOfCookies ?
    mDBState->cookieCount - mMaxNumberOfCookies : 0;
  if (purgeList.Length() > excess) {
    // We're not purging everything in the list, so update our indicator.
    oldestTime = purgeList[excess].Cookie()->LastAccessed();

    purgeList.SetLength(excess);
  }

  // sort the list again, this time grouping cookies with a common entryclass
  // together, and with ascending index. this allows us to iterate backwards
  // over the list removing cookies, without having to adjust indexes as we go.
  purgeList.Sort(CompareCookiesByIndex());
  for (PurgeList::index_type i = purgeList.Length(); i--; ) {
    nsCookie *cookie = purgeList[i].Cookie();
    removedList->AppendElement(cookie);
    COOKIE_LOGEVICTED(cookie, "Cookie too old");

    RemoveCookieFromList(purgeList[i], paramsArray);
  }

  // Update the database if we have entries to purge.
  if (paramsArray) {
    uint32_t length;
    paramsArray->GetLength(&length);
    if (length) {
      DebugOnly<nsresult> rv = stmt->BindParameters(paramsArray);
      NS_ASSERT_SUCCESS(rv);
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(mDBState->removeListener, getter_AddRefs(handle));
      NS_ASSERT_SUCCESS(rv);
    }
  }

  // reset the oldest time indicator
  mDBState->cookieOldestTime = oldestTime;

  COOKIE_LOGSTRING(LogLevel::Debug,
    ("PurgeCookies(): %" PRIu32 " expired; %" PRIu32 " purged; %" PRIu32
     " remain; %" PRId64 " oldest age",
     initialCookieCount - postExpiryCookieCount,
     postExpiryCookieCount - mDBState->cookieCount,
     mDBState->cookieCount,
     aCurrentTimeInUsec - mDBState->cookieOldestTime));

  return removedList.forget();
}

// find whether a given cookie has been previously set. this is provided by the
// nsICookieManager interface.
NS_IMETHODIMP
nsCookieService::CookieExists(nsICookie2* aCookie,
                              JS::HandleValue aOriginAttributes,
                              JSContext* aCx,
                              uint8_t aArgc,
                              bool* aFoundCookie)
{
  NS_ENSURE_ARG_POINTER(aCookie);
  NS_ENSURE_ARG_POINTER(aCx);
  NS_ENSURE_ARG_POINTER(aFoundCookie);
  MOZ_ASSERT(aArgc == 0 || aArgc == 1);

  OriginAttributes attrs;
  nsresult rv = InitializeOriginAttributes(&attrs,
                                           aOriginAttributes,
                                           aCx,
                                           aArgc,
                                           u"nsICookieManager.cookieExists()",
                                           u"2");
  NS_ENSURE_SUCCESS(rv, rv);

  return CookieExistsNative(aCookie, &attrs, aFoundCookie);
}

NS_IMETHODIMP_(nsresult)
nsCookieService::CookieExistsNative(nsICookie2* aCookie,
                                    OriginAttributes* aOriginAttributes,
                                    bool* aFoundCookie)
{
  NS_ENSURE_ARG_POINTER(aCookie);
  NS_ENSURE_ARG_POINTER(aOriginAttributes);
  NS_ENSURE_ARG_POINTER(aFoundCookie);

  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  AutoRestore<DBState*> savePrevDBState(mDBState);
  mDBState = (aOriginAttributes->mPrivateBrowsingId > 0) ? mPrivateDBState : mDefaultDBState;

  nsAutoCString host, name, path;
  nsresult rv = aCookie->GetHost(host);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCookie->GetName(name);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCookie->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsListIter iter;
  *aFoundCookie = FindCookie(nsCookieKey(baseDomain, *aOriginAttributes),
                             host, name, path, iter);
  return NS_OK;
}

// For a given base domain, find either an expired cookie or the oldest cookie
// by lastAccessed time.
int64_t
nsCookieService::FindStaleCookie(nsCookieEntry *aEntry,
                                 int64_t aCurrentTime,
                                 nsIURI* aSource,
                                 const mozilla::Maybe<bool> &aIsSecure,
                                 nsListIter &aIter)
{
  aIter.entry = nullptr;
  bool requireHostMatch = true;
  nsAutoCString baseDomain, sourceHost, sourcePath;
  if (aSource) {
    GetBaseDomain(mTLDService, aSource, baseDomain, requireHostMatch);
    aSource->GetAsciiHost(sourceHost);
    sourcePath = GetPathFromURI(aSource);
  }

  const nsCookieEntry::ArrayType &cookies = aEntry->GetCookies();

  int64_t oldestNonMatchingCookieTime = 0;
  nsListIter oldestNonMatchingCookie;
  oldestNonMatchingCookie.entry = nullptr;

  int64_t oldestCookieTime = 0;
  nsListIter oldestCookie;
  oldestCookie.entry = nullptr;

  int64_t actualOldestCookieTime = cookies.Length() ? cookies[0]->LastAccessed() : 0;
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    nsCookie *cookie = cookies[i];

    // If we found an expired cookie, we're done.
    if (cookie->Expiry() <= aCurrentTime) {
      aIter.entry = aEntry;
      aIter.index = i;
      return -1;
    }

    int64_t lastAccessed = cookie->LastAccessed();
    // Record the age of the oldest cookie that is stored for this host.
    // oldestCookieTime is the age of the oldest cookie with a matching
    // secure flag, which may be more recent than an older cookie with
    // a non-matching secure flag.
    if (actualOldestCookieTime > lastAccessed) {
      actualOldestCookieTime = lastAccessed;
    }
    if (aIsSecure.isSome() && !aIsSecure.value()) {
      // We want to look for the oldest non-secure cookie first time through,
      // then find the oldest secure cookie the second time we are called.
      if (cookie->IsSecure()) {
        continue;
      }
    }

    // This cookie is a candidate for eviction if we have no information about
    // the source request, or if it is not a path or domain match against the
    // source request.
    bool isPrimaryEvictionCandidate = true;
    if (aSource) {
      isPrimaryEvictionCandidate = !PathMatches(cookie, sourcePath) || !DomainMatches(cookie, sourceHost);
    }

    if (isPrimaryEvictionCandidate &&
        (!oldestNonMatchingCookie.entry ||
         oldestNonMatchingCookieTime > lastAccessed)) {
      oldestNonMatchingCookieTime = lastAccessed;
      oldestNonMatchingCookie.entry = aEntry;
      oldestNonMatchingCookie.index = i;
    }

    // Check if we've found the oldest cookie so far.
    if (!oldestCookie.entry || oldestCookieTime > lastAccessed) {
      oldestCookieTime = lastAccessed;
      oldestCookie.entry = aEntry;
      oldestCookie.index = i;
    }
  }

  // Prefer to evict the oldest cookie with a non-matching path/domain,
  // followed by the oldest matching cookie.
  if (oldestNonMatchingCookie.entry) {
    aIter = oldestNonMatchingCookie;
  } else {
    aIter = oldestCookie;
  }

  return actualOldestCookieTime;
}

void
nsCookieService::TelemetryForEvictingStaleCookie(nsCookie *aEvicted,
                                                 int64_t oldestCookieTime)
{
  // We need to record the evicting cookie to telemetry.
  if (!aEvicted->IsSecure()) {
    if (aEvicted->LastAccessed() > oldestCookieTime) {
      Telemetry::Accumulate(Telemetry::COOKIE_LEAVE_SECURE_ALONE,
                            EVICTED_NEWER_INSECURE);
    } else {
      Telemetry::Accumulate(Telemetry::COOKIE_LEAVE_SECURE_ALONE,
                            EVICTED_OLDEST_COOKIE);
    }
  } else {
    Telemetry::Accumulate(Telemetry::COOKIE_LEAVE_SECURE_ALONE,
                          EVICTED_PREFERRED_COOKIE);
  }
}

// count the number of cookies stored by a particular host. this is provided by the
// nsICookieManager interface.
NS_IMETHODIMP
nsCookieService::CountCookiesFromHost(const nsACString &aHost,
                                      uint32_t         *aCountFromHost)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCookieKey key = DEFAULT_APP_KEY(baseDomain);

  // Return a count of all cookies, including expired.
  nsCookieEntry *entry = mDBState->hostTable.GetEntry(key);
  *aCountFromHost = entry ? entry->GetCookies().Length() : 0;
  return NS_OK;
}

// get an enumerator of cookies stored by a particular host. this is provided by the
// nsICookieManager interface.
NS_IMETHODIMP
nsCookieService::GetCookiesFromHost(const nsACString     &aHost,
                                    JS::HandleValue       aOriginAttributes,
                                    JSContext*            aCx,
                                    uint8_t               aArgc,
                                    nsISimpleEnumerator **aEnumerator)
{
  MOZ_ASSERT(aArgc == 0 || aArgc == 1);

  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(mTLDService, host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes attrs;
  rv = InitializeOriginAttributes(&attrs,
                                  aOriginAttributes,
                                  aCx,
                                  aArgc,
                                  u"nsICookieManager.getCookiesFromHost()",
                                  u"2");
  NS_ENSURE_SUCCESS(rv, rv);

  AutoRestore<DBState*> savePrevDBState(mDBState);
  mDBState = (attrs.mPrivateBrowsingId > 0) ? mPrivateDBState : mDefaultDBState;

  nsCookieKey key = nsCookieKey(baseDomain, attrs);

  nsCookieEntry *entry = mDBState->hostTable.GetEntry(key);
  if (!entry)
    return NS_NewEmptyEnumerator(aEnumerator);

  nsCOMArray<nsICookie> cookieList(mMaxCookiesPerHost);
  const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    cookieList.AppendObject(cookies[i]);
  }

  return NS_NewArrayEnumerator(aEnumerator, cookieList);
}

NS_IMETHODIMP
nsCookieService::GetCookiesWithOriginAttributes(const nsAString&    aPattern,
                                                const nsACString&   aHost,
                                                nsISimpleEnumerator **aEnumerator)
{
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

  return GetCookiesWithOriginAttributes(pattern, baseDomain, aEnumerator);
}

nsresult
nsCookieService::GetCookiesWithOriginAttributes(
    const mozilla::OriginAttributesPattern& aPattern,
    const nsCString& aBaseDomain,
    nsISimpleEnumerator **aEnumerator)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }
  EnsureReadComplete(true);

  AutoRestore<DBState*> savePrevDBState(mDBState);
  mDBState = (aPattern.mPrivateBrowsingId.WasPassed() &&
      aPattern.mPrivateBrowsingId.Value() > 0) ? mPrivateDBState : mDefaultDBState;

  nsCOMArray<nsICookie> cookies;
  for (auto iter = mDBState->hostTable.Iter(); !iter.Done(); iter.Next()) {
    nsCookieEntry* entry = iter.Get();

    if (!aBaseDomain.IsEmpty() && !aBaseDomain.Equals(entry->mBaseDomain)) {
      continue;
    }

    if (!aPattern.Matches(entry->mOriginAttributes)) {
      continue;
    }

    const nsCookieEntry::ArrayType& entryCookies = entry->GetCookies();

    for (nsCookieEntry::IndexType i = 0; i < entryCookies.Length(); ++i) {
      cookies.AppendObject(entryCookies[i]);
    }
  }

  return NS_NewArrayEnumerator(aEnumerator, cookies);
}

NS_IMETHODIMP
nsCookieService::RemoveCookiesWithOriginAttributes(const nsAString& aPattern,
                                                   const nsACString& aHost)
{
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

nsresult
nsCookieService::RemoveCookiesWithOriginAttributes(
    const mozilla::OriginAttributesPattern& aPattern,
    const nsCString& aBaseDomain)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already close?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete(true);

  AutoRestore<DBState*> savePrevDBState(mDBState);
  mDBState = (aPattern.mPrivateBrowsingId.WasPassed() &&
      aPattern.mPrivateBrowsingId.Value() > 0) ? mPrivateDBState : mDefaultDBState;

  mozStorageTransaction transaction(mDBState->dbConn, false);
  // Iterate the hash table of nsCookieEntry.
  for (auto iter = mDBState->hostTable.Iter(); !iter.Done(); iter.Next()) {
    nsCookieEntry* entry = iter.Get();

    if (!aBaseDomain.IsEmpty() && !aBaseDomain.Equals(entry->mBaseDomain)) {
      continue;
    }

    if (!aPattern.Matches(entry->mOriginAttributes)) {
      continue;
    }

    // Pattern matches. Delete all cookies within this nsCookieEntry.
    uint32_t cookiesCount = entry->GetCookies().Length();

    for (nsCookieEntry::IndexType i = 0 ; i < cookiesCount; ++i) {
      // Remove the first cookie from the list.
      nsListIter iter(entry, 0);
      RefPtr<nsCookie> cookie = iter.Cookie();

      // Remove the cookie.
      RemoveCookieFromList(iter);

      if (cookie) {
        NotifyChanged(cookie, u"deleted");
      }
    }
  }
  DebugOnly<nsresult> rv = transaction.Commit();
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  return NS_OK;
}

// find an secure cookie specified by host and name
bool
nsCookieService::FindSecureCookie(const nsCookieKey &aKey,
                                  nsCookie          *aCookie)
{
  nsCookieEntry *entry = mDBState->hostTable.GetEntry(aKey);
  if (!entry)
    return false;

  const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    nsCookie *cookie = cookies[i];
    // isn't a match if insecure or a different name
    if (!cookie->IsSecure() || !aCookie->Name().Equals(cookie->Name()))
      continue;

    // The host must "domain-match" an existing cookie or vice-versa
    if (DomainMatches(cookie, aCookie->Host()) ||
        DomainMatches(aCookie, cookie->Host())) {
      // If the path of new cookie and the path of existing cookie
      // aren't "/", then this situation needs to compare paths to
      // ensure only that a newly-created non-secure cookie does not
      // overlay an existing secure cookie.
      if (PathMatches(cookie, aCookie->Path())) {
        return true;
      }
    }
  }

  return false;
}

// find an exact cookie specified by host, name, and path that hasn't expired.
bool
nsCookieService::FindCookie(const nsCookieKey    &aKey,
                            const nsCString& aHost,
                            const nsCString& aName,
                            const nsCString& aPath,
                            nsListIter           &aIter)
{
  // Should |EnsureReadComplete| before.
  MOZ_ASSERT(mInitializedDBStates);
  MOZ_ASSERT(mInitializedDBConn);

  nsCookieEntry *entry = mDBState->hostTable.GetEntry(aKey);
  if (!entry)
    return false;

  const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    nsCookie *cookie = cookies[i];

    if (aHost.Equals(cookie->Host()) &&
        aPath.Equals(cookie->Path()) &&
        aName.Equals(cookie->Name())) {
      aIter = nsListIter(entry, i);
      return true;
    }
  }

  return false;
}

// remove a cookie from the hashtable, and update the iterator state.
void
nsCookieService::RemoveCookieFromList(const nsListIter              &aIter,
                                      mozIStorageBindingParamsArray *aParamsArray)
{
  // if it's a non-session cookie, remove it from the db
  if (!aIter.Cookie()->IsSession() && mDBState->dbConn) {
    // Use the asynchronous binding methods to ensure that we do not acquire
    // the database lock.
    mozIStorageAsyncStatement *stmt = mDBState->stmtDelete;
    nsCOMPtr<mozIStorageBindingParamsArray> paramsArray(aParamsArray);
    if (!paramsArray) {
      stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
    }

    nsCOMPtr<mozIStorageBindingParams> params;
    paramsArray->NewBindingParams(getter_AddRefs(params));

    DebugOnly<nsresult> rv =
      params->BindUTF8StringByName(NS_LITERAL_CSTRING("name"),
                                   aIter.Cookie()->Name());
    NS_ASSERT_SUCCESS(rv);

    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("host"),
                                      aIter.Cookie()->Host());
    NS_ASSERT_SUCCESS(rv);

    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("path"),
                                      aIter.Cookie()->Path());
    NS_ASSERT_SUCCESS(rv);

    nsAutoCString suffix;
    aIter.Cookie()->OriginAttributesRef().CreateSuffix(suffix);
    rv = params->BindUTF8StringByName(
      NS_LITERAL_CSTRING("originAttributes"), suffix);
    NS_ASSERT_SUCCESS(rv);

    rv = paramsArray->AddParams(params);
    NS_ASSERT_SUCCESS(rv);

    // If we weren't given a params array, we'll need to remove it ourselves.
    if (!aParamsArray) {
      rv = stmt->BindParameters(paramsArray);
      NS_ASSERT_SUCCESS(rv);
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(mDBState->removeListener, getter_AddRefs(handle));
      NS_ASSERT_SUCCESS(rv);
    }
  }

  if (aIter.entry->GetCookies().Length() == 1) {
    // we're removing the last element in the array - so just remove the entry
    // from the hash. note that the entryclass' dtor will take care of
    // releasing this last element for us!
    mDBState->hostTable.RawRemoveEntry(aIter.entry);

  } else {
    // just remove the element from the list
    aIter.entry->GetCookies().RemoveElementAt(aIter.index);
  }

  --mDBState->cookieCount;
}

void
bindCookieParameters(mozIStorageBindingParamsArray *aParamsArray,
                     const nsCookieKey &aKey,
                     const nsCookie *aCookie)
{
  NS_ASSERTION(aParamsArray, "Null params array passed to bindCookieParameters!");
  NS_ASSERTION(aCookie, "Null cookie passed to bindCookieParameters!");

  // Use the asynchronous binding methods to ensure that we do not acquire the
  // database lock.
  nsCOMPtr<mozIStorageBindingParams> params;
  DebugOnly<nsresult> rv =
    aParamsArray->NewBindingParams(getter_AddRefs(params));
  NS_ASSERT_SUCCESS(rv);

  // Bind our values to params
  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("baseDomain"),
                                    aKey.mBaseDomain);
  NS_ASSERT_SUCCESS(rv);

  nsAutoCString suffix;
  aKey.mOriginAttributes.CreateSuffix(suffix);
  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("originAttributes"),
                                    suffix);
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("name"),
                                    aCookie->Name());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("value"),
                                    aCookie->Value());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("host"),
                                    aCookie->Host());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("path"),
                                    aCookie->Path());
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindInt64ByName(NS_LITERAL_CSTRING("expiry"),
                               aCookie->Expiry());
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

  // Bind the params to the array.
  rv = aParamsArray->AddParams(params);
  NS_ASSERT_SUCCESS(rv);
}

void
nsCookieService::UpdateCookieOldestTime(DBState* aDBState,
                                        nsCookie* aCookie)
{
  if (aCookie->LastAccessed() < aDBState->cookieOldestTime) {
    aDBState->cookieOldestTime = aCookie->LastAccessed();
  }
}

void
nsCookieService::AddCookieToList(const nsCookieKey             &aKey,
                                 nsCookie                      *aCookie,
                                 DBState                       *aDBState,
                                 mozIStorageBindingParamsArray *aParamsArray,
                                 bool                           aWriteToDB)
{
  NS_ASSERTION(!(aDBState->dbConn && !aWriteToDB && aParamsArray),
               "Not writing to the DB but have a params array?");
  NS_ASSERTION(!(!aDBState->dbConn && aParamsArray),
               "Do not have a DB connection but have a params array?");

  if (!aCookie) {
    NS_WARNING("Attempting to AddCookieToList with null cookie");
    return;
  }

  nsCookieEntry *entry = aDBState->hostTable.PutEntry(aKey);
  NS_ASSERTION(entry, "can't insert element into a null entry!");

  entry->GetCookies().AppendElement(aCookie);
  ++aDBState->cookieCount;

  // keep track of the oldest cookie, for when it comes time to purge
  UpdateCookieOldestTime(aDBState, aCookie);

  // if it's a non-session cookie and hasn't just been read from the db, write it out.
  if (aWriteToDB && !aCookie->IsSession() && aDBState->dbConn) {
    mozIStorageAsyncStatement *stmt = aDBState->stmtInsert;
    nsCOMPtr<mozIStorageBindingParamsArray> paramsArray(aParamsArray);
    if (!paramsArray) {
      stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
    }
    bindCookieParameters(paramsArray, aKey, aCookie);

    // If we were supplied an array to store parameters, we shouldn't call
    // executeAsync - someone up the stack will do this for us.
    if (!aParamsArray) {
      DebugOnly<nsresult> rv = stmt->BindParameters(paramsArray);
      NS_ASSERT_SUCCESS(rv);
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(mDBState->insertListener, getter_AddRefs(handle));
      NS_ASSERT_SUCCESS(rv);
    }
  }
}

void
nsCookieService::UpdateCookieInList(nsCookie                      *aCookie,
                                    int64_t                        aLastAccessed,
                                    mozIStorageBindingParamsArray *aParamsArray)
{
  NS_ASSERTION(aCookie, "Passing a null cookie to UpdateCookieInList!");

  // udpate the lastAccessed timestamp
  aCookie->SetLastAccessed(aLastAccessed);

  // if it's a non-session cookie, update it in the db too
  if (!aCookie->IsSession() && aParamsArray) {
    // Create our params holder.
    nsCOMPtr<mozIStorageBindingParams> params;
    aParamsArray->NewBindingParams(getter_AddRefs(params));

    // Bind our parameters.
    DebugOnly<nsresult> rv =
      params->BindInt64ByName(NS_LITERAL_CSTRING("lastAccessed"),
                              aLastAccessed);
    NS_ASSERT_SUCCESS(rv);

    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("name"),
                                      aCookie->Name());
    NS_ASSERT_SUCCESS(rv);

    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("host"),
                                      aCookie->Host());
    NS_ASSERT_SUCCESS(rv);

    rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("path"),
                                      aCookie->Path());
    NS_ASSERT_SUCCESS(rv);

    nsAutoCString suffix;
    aCookie->OriginAttributesRef().CreateSuffix(suffix);
    rv = params->BindUTF8StringByName(
      NS_LITERAL_CSTRING("originAttributes"), suffix);
    NS_ASSERT_SUCCESS(rv);

    // Add our bound parameters to the array.
    rv = aParamsArray->AddParams(params);
    NS_ASSERT_SUCCESS(rv);
  }
}

size_t
nsCookieService::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  if (mDefaultDBState) {
    n += mDefaultDBState->SizeOfIncludingThis(aMallocSizeOf);
  }
  if (mPrivateDBState) {
    n += mPrivateDBState->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

MOZ_DEFINE_MALLOC_SIZE_OF(CookieServiceMallocSizeOf)

NS_IMETHODIMP
nsCookieService::CollectReports(nsIHandleReportCallback* aHandleReport,
                                nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/cookie-service", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(CookieServiceMallocSizeOf),
    "Memory used by the cookie service.");

  return NS_OK;
}
