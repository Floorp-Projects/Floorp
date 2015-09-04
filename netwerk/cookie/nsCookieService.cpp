/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"

#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/NeckoCommon.h"

#include "nsCookieService.h"
#include "nsIServiceManager.h"

#include "nsIIOService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsICookiePermission.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsILineInputStream.h"
#include "nsIEffectiveTLDService.h"
#include "nsIIDNService.h"
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
#include "mozilla/Telemetry.h"
#include "nsIAppsService.h"
#include "mozIApplication.h"
#include "mozIApplicationClearPrivateDataParams.h"
#include "nsIConsoleService.h"

using namespace mozilla;
using namespace mozilla::net;

// Create key from baseDomain that will access the default cookie namespace.
// TODO: When we figure out what the API will look like for nsICookieManager{2}
// on content processes (see bug 777620), change to use the appropriate app
// namespace.  For now those IDLs aren't supported on child processes.
#define DEFAULT_APP_KEY(baseDomain) \
        nsCookieKey(baseDomain, NECKO_NO_APP_ID, false)

/******************************************************************************
 * nsCookieService impl:
 * useful types & constants
 ******************************************************************************/

static nsCookieService *gCookieService;

// XXX_hack. See bug 178993.
// This is a hack to hide HttpOnly cookies from older browsers
#define HTTP_ONLY_PREFIX "#HttpOnly_"

#define COOKIES_FILE "cookies.sqlite"
#define COOKIES_SCHEMA_VERSION 5

// parameter indexes; see EnsureReadDomain, EnsureReadComplete and
// ReadCookieDBListener::HandleResult
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
#define IDX_APP_ID 10
#define IDX_BROWSER_ELEM 11

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
static const uint32_t kMaxCookiesPerHost  = 150;
static const uint32_t kMaxBytesPerCookie  = 4096;
static const uint32_t kMaxBytesPerPath    = 1024;

// pref string constants
static const char kPrefCookieBehavior[]     = "network.cookie.cookieBehavior";
static const char kPrefMaxNumberOfCookies[] = "network.cookie.maxNumber";
static const char kPrefMaxCookiesPerHost[]  = "network.cookie.maxPerHost";
static const char kPrefCookiePurgeAge[]     = "network.cookie.purgeAge";
static const char kPrefThirdPartySession[]  = "network.cookie.thirdparty.sessionOnly";

static void
bindCookieParameters(mozIStorageBindingParamsArray *aParamsArray,
                     const nsCookieKey &aKey,
                     const nsCookie *aCookie);

// struct for temporarily storing cookie attributes during header parsing
struct nsCookieAttributes
{
  nsAutoCString name;
  nsAutoCString value;
  nsAutoCString host;
  nsAutoCString path;
  nsAutoCString expires;
  nsAutoCString maxage;
  int64_t expiryTime;
  bool isSession;
  bool isSecure;
  bool isHttpOnly;
};

// stores the nsCookieEntry entryclass and an index into the cookie array
// within that entryclass, for purposes of storing an iteration state that
// points to a certain cookie.
struct nsListIter
{
  // default (non-initializing) constructor.
  nsListIter()
  {
  }

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
//    set NSPR_LOG_MODULES=cookie:3 -- shows rejected cookies
//    set NSPR_LOG_MODULES=cookie:4 -- shows accepted and rejected cookies
//    set NSPR_LOG_FILE=cookie.log
//
#include "mozilla/Logging.h"
#endif

// define logging macros for convenience
#define SET_COOKIE true
#define GET_COOKIE false

static PRLogModuleInfo *
GetCookieLog()
{
  static PRLogModuleInfo *sCookieLog;
  if (!sCookieLog)
    sCookieLog = PR_NewLogModule("cookie");
  return sCookieLog;
}

#define COOKIE_LOGFAILURE(a, b, c, d)    LogFailure(a, b, c, d)
#define COOKIE_LOGSUCCESS(a, b, c, d, e) LogSuccess(a, b, c, d, e)

#define COOKIE_LOGEVICTED(a, details)          \
  PR_BEGIN_MACRO                               \
  if (MOZ_LOG_TEST(GetCookieLog(), LogLevel::Debug))  \
      LogEvicted(a, details);                  \
  PR_END_MACRO

#define COOKIE_LOGSTRING(lvl, fmt)   \
  PR_BEGIN_MACRO                     \
    MOZ_LOG(GetCookieLog(), lvl, fmt);  \
    MOZ_LOG(GetCookieLog(), lvl, ("\n")); \
  PR_END_MACRO

static void
LogFailure(bool aSetCookie, nsIURI *aHostURI, const char *aCookieString, const char *aReason)
{
  // if logging isn't enabled, return now to save cycles
  if (!MOZ_LOG_TEST(GetCookieLog(), LogLevel::Warning))
    return;

  nsAutoCString spec;
  if (aHostURI)
    aHostURI->GetAsciiSpec(spec);

  MOZ_LOG(GetCookieLog(), LogLevel::Warning,
    ("===== %s =====\n", aSetCookie ? "COOKIE NOT ACCEPTED" : "COOKIE NOT SENT"));
  MOZ_LOG(GetCookieLog(), LogLevel::Warning,("request URL: %s\n", spec.get()));
  if (aSetCookie)
    MOZ_LOG(GetCookieLog(), LogLevel::Warning,("cookie string: %s\n", aCookieString));

  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  MOZ_LOG(GetCookieLog(), LogLevel::Warning,("current time: %s", timeString));
  MOZ_LOG(GetCookieLog(), LogLevel::Warning,("rejected because %s\n", aReason));
  MOZ_LOG(GetCookieLog(), LogLevel::Warning,("\n"));
}

static void
LogCookie(nsCookie *aCookie)
{
  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  MOZ_LOG(GetCookieLog(), LogLevel::Debug,("current time: %s", timeString));

  if (aCookie) {
    MOZ_LOG(GetCookieLog(), LogLevel::Debug,("----------------\n"));
    MOZ_LOG(GetCookieLog(), LogLevel::Debug,("name: %s\n", aCookie->Name().get()));
    MOZ_LOG(GetCookieLog(), LogLevel::Debug,("value: %s\n", aCookie->Value().get()));
    MOZ_LOG(GetCookieLog(), LogLevel::Debug,("%s: %s\n", aCookie->IsDomain() ? "domain" : "host", aCookie->Host().get()));
    MOZ_LOG(GetCookieLog(), LogLevel::Debug,("path: %s\n", aCookie->Path().get()));

    PR_ExplodeTime(aCookie->Expiry() * int64_t(PR_USEC_PER_SEC),
                   PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);
    MOZ_LOG(GetCookieLog(), LogLevel::Debug,
      ("expires: %s%s", timeString, aCookie->IsSession() ? " (at end of session)" : ""));

    PR_ExplodeTime(aCookie->CreationTime(), PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);
    MOZ_LOG(GetCookieLog(), LogLevel::Debug,("created: %s", timeString));

    MOZ_LOG(GetCookieLog(), LogLevel::Debug,("is secure: %s\n", aCookie->IsSecure() ? "true" : "false"));
    MOZ_LOG(GetCookieLog(), LogLevel::Debug,("is httpOnly: %s\n", aCookie->IsHttpOnly() ? "true" : "false"));
  }
}

static void
LogSuccess(bool aSetCookie, nsIURI *aHostURI, const char *aCookieString, nsCookie *aCookie, bool aReplacing)
{
  // if logging isn't enabled, return now to save cycles
  if (!MOZ_LOG_TEST(GetCookieLog(), LogLevel::Debug)) {
    return;
  }

  nsAutoCString spec;
  if (aHostURI)
    aHostURI->GetAsciiSpec(spec);

  MOZ_LOG(GetCookieLog(), LogLevel::Debug,
    ("===== %s =====\n", aSetCookie ? "COOKIE ACCEPTED" : "COOKIE SENT"));
  MOZ_LOG(GetCookieLog(), LogLevel::Debug,("request URL: %s\n", spec.get()));
  MOZ_LOG(GetCookieLog(), LogLevel::Debug,("cookie string: %s\n", aCookieString));
  if (aSetCookie)
    MOZ_LOG(GetCookieLog(), LogLevel::Debug,("replaces existing cookie: %s\n", aReplacing ? "true" : "false"));

  LogCookie(aCookie);

  MOZ_LOG(GetCookieLog(), LogLevel::Debug,("\n"));
}

static void
LogEvicted(nsCookie *aCookie, const char* details)
{
  MOZ_LOG(GetCookieLog(), LogLevel::Debug,("===== COOKIE EVICTED =====\n"));
  MOZ_LOG(GetCookieLog(), LogLevel::Debug,("%s\n", details));

  LogCookie(aCookie);

  MOZ_LOG(GetCookieLog(), LogLevel::Debug,("\n"));
}

// inline wrappers to make passing in nsAFlatCStrings easier
static inline void
LogFailure(bool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, const char *aReason)
{
  LogFailure(aSetCookie, aHostURI, aCookieString.get(), aReason);
}

static inline void
LogSuccess(bool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, nsCookie *aCookie, bool aReplacing)
{
  LogSuccess(aSetCookie, aHostURI, aCookieString.get(), aCookie, aReplacing);
}

#ifdef DEBUG
#define NS_ASSERT_SUCCESS(res)                                               \
  PR_BEGIN_MACRO                                                             \
  nsresult __rv = res; /* Do not evaluate |res| more than once! */           \
  if (NS_FAILED(__rv)) {                                                     \
    char *msg = PR_smprintf("NS_ASSERT_SUCCESS(%s) failed with result 0x%X", \
                            #res, __rv);                                     \
    NS_ASSERTION(NS_SUCCEEDED(__rv), msg);                                   \
    PR_smprintf_free(msg);                                                   \
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
  nsRefPtr<DBState> mDBState;
  virtual const char *GetOpType() = 0;

public:
  NS_IMETHOD HandleError(mozIStorageError* aError) override
  {
    if (MOZ_LOG_TEST(GetCookieLog(), LogLevel::Warning)) {
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
  virtual const char *GetOpType() override { return "INSERT"; }

  ~InsertCookieDBListener() {}

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
  virtual const char *GetOpType() override { return "UPDATE"; }

  ~UpdateCookieDBListener() {}

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
  virtual const char *GetOpType() override { return "REMOVE"; }

  ~RemoveCookieDBListener() {}

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
 * ReadCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous removal operations.
 ******************************************************************************/
class ReadCookieDBListener final : public DBListenerErrorHandler
{
private:
  virtual const char *GetOpType() override { return "READ"; }
  bool mCanceled;

  ~ReadCookieDBListener() {}

public:
  NS_DECL_ISUPPORTS

  explicit ReadCookieDBListener(DBState* dbState)
    : DBListenerErrorHandler(dbState)
    , mCanceled(false)
  {
  }

  void Cancel() { mCanceled = true; }

  NS_IMETHOD HandleResult(mozIStorageResultSet *aResult) override
  {
    nsCOMPtr<mozIStorageRow> row;

    while (1) {
      DebugOnly<nsresult> rv = aResult->GetNextRow(getter_AddRefs(row));
      NS_ASSERT_SUCCESS(rv);

      if (!row)
        break;

      CookieDomainTuple *tuple = mDBState->hostArray.AppendElement();
      row->GetUTF8String(IDX_BASE_DOMAIN, tuple->key.mBaseDomain);
      tuple->key.mAppId = static_cast<uint32_t>(row->AsInt32(IDX_APP_ID));
      tuple->key.mInBrowserElement = static_cast<bool>(row->AsInt32(IDX_BROWSER_ELEM));
      tuple->cookie = gCookieService->GetCookieFromRow(row);
    }

    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(uint16_t aReason) override
  {
    // Process the completion of the read operation. If we have been canceled,
    // we cannot assume that the cookieservice still has an open connection
    // or that it even refers to the same database, so we must return early.
    // Conversely, the cookieservice guarantees that if we have not been
    // canceled, the database connection is still alive and we can safely
    // operate on it.

    if (mCanceled) {
      // We may receive a REASON_FINISHED after being canceled;
      // tweak the reason accordingly.
      aReason = mozIStorageStatementCallback::REASON_CANCELED;
    }

    switch (aReason) {
    case mozIStorageStatementCallback::REASON_FINISHED:
      gCookieService->AsyncReadComplete();
      break;
    case mozIStorageStatementCallback::REASON_CANCELED:
      // Nothing more to do here. The partially read data has already been
      // thrown away.
      COOKIE_LOGSTRING(LogLevel::Debug, ("Read canceled"));
      break;
    case mozIStorageStatementCallback::REASON_ERROR:
      // Nothing more to do here. DBListenerErrorHandler::HandleError()
      // can handle it.
      COOKIE_LOGSTRING(LogLevel::Debug, ("Read error"));
      break;
    default:
      NS_NOTREACHED("invalid reason");
    }
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(ReadCookieDBListener, mozIStorageStatementCallback)

/******************************************************************************
 * CloseCookieDBListener imp:
 * Static mozIStorageCompletionCallback used to notify when the database is
 * successfully closed.
 ******************************************************************************/
class CloseCookieDBListener final :  public mozIStorageCompletionCallback
{
  ~CloseCookieDBListener() {}

public:
  explicit CloseCookieDBListener(DBState* dbState) : mDBState(dbState) { }
  nsRefPtr<DBState> mDBState;
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

  ~AppClearDataObserver() {}

public:
  NS_DECL_ISUPPORTS

  // nsIObserver implementation.
  NS_IMETHODIMP
  Observe(nsISupports *aSubject, const char *aTopic, const char16_t *data) override
  {
    MOZ_ASSERT(!nsCRT::strcmp(aTopic, TOPIC_WEB_APP_CLEAR_DATA));

    uint32_t appId = NECKO_UNKNOWN_APP_ID;
    bool browserOnly = false;
    nsresult rv = NS_GetAppInfoFromClearDataNotification(aSubject, &appId,
                                                         &browserOnly);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsICookieManager2> cookieManager
      = do_GetService(NS_COOKIEMANAGER_CONTRACTID);
    MOZ_ASSERT(cookieManager);
    return cookieManager->RemoveCookiesForApp(appId, browserOnly);
  }
};

NS_IMPL_ISUPPORTS(AppClearDataObserver, nsIObserver)

} // namespace

size_t
nsCookieKey::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return mBaseDomain.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

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
CookieDomainTuple::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = 0;

  amount += key.SizeOfExcludingThis(aMallocSizeOf);
  amount += cookie->SizeOfIncludingThis(aMallocSizeOf);

  return amount;
}

size_t
DBState::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = 0;

  amount += aMallocSizeOf(this);
  amount += hostTable.SizeOfExcludingThis(aMallocSizeOf);
  amount += hostArray.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < hostArray.Length(); ++i) {
    amount += hostArray[i].SizeOfExcludingThis(aMallocSizeOf);
  }
  amount += readSet.SizeOfExcludingThis(aMallocSizeOf);

  return amount;
}

/******************************************************************************
 * nsCookieService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/

nsICookieService*
nsCookieService::GetXPCOMSingleton()
{
  if (IsNeckoChild())
    return CookieServiceChild::GetSingleton();

  return GetSingleton();
}

nsCookieService*
nsCookieService::GetSingleton()
{
  NS_ASSERTION(!IsNeckoChild(), "not a parent process");

  if (gCookieService) {
    NS_ADDREF(gCookieService);
    return gCookieService;
  }

  // Create a new singleton nsCookieService.
  // We AddRef only once since XPCOM has rules about the ordering of module
  // teardowns - by the time our module destructor is called, it's too late to
  // Release our members (e.g. nsIObserverService and nsIPrefBranch), since GC
  // cycles have already been completed and would result in serious leaks.
  // See bug 209571.
  gCookieService = new nsCookieService();
  if (gCookieService) {
    NS_ADDREF(gCookieService);
    if (NS_FAILED(gCookieService->Init())) {
      NS_RELEASE(gCookieService);
    }
  }

  return gCookieService;
}

/* static */ void
nsCookieService::AppClearDataObserverInit()
{
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  nsCOMPtr<nsIObserver> obs = new AppClearDataObserver();
  observerService->AddObserver(obs, TOPIC_WEB_APP_CLEAR_DATA,
                               /* holdsWeak= */ false);
}

/******************************************************************************
 * nsCookieService impl:
 * public methods
 ******************************************************************************/

NS_IMPL_ISUPPORTS(nsCookieService,
                  nsICookieService,
                  nsICookieManager,
                  nsICookieManager2,
                  nsIObserver,
                  nsISupportsWeakReference,
                  nsIMemoryReporter)

nsCookieService::nsCookieService()
 : mDBState(nullptr)
 , mCookieBehavior(nsICookieService::BEHAVIOR_ACCEPT)
 , mThirdPartySession(false)
 , mMaxNumberOfCookies(kMaxNumberOfCookies)
 , mMaxCookiesPerHost(kMaxCookiesPerHost)
 , mCookiePurgeAge(kCookiePurgeAge)
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
    prefBranch->AddObserver(kPrefCookieBehavior,     this, true);
    prefBranch->AddObserver(kPrefMaxNumberOfCookies, this, true);
    prefBranch->AddObserver(kPrefMaxCookiesPerHost,  this, true);
    prefBranch->AddObserver(kPrefCookiePurgeAge,     this, true);
    prefBranch->AddObserver(kPrefThirdPartySession,  this, true);
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
    return;
  }
  mDefaultDBState->cookieFile->AppendNative(NS_LITERAL_CSTRING(COOKIES_FILE));

  // Attempt to open and read the database. If TryInitDB() returns RESULT_RETRY,
  // do so.
  OpenDBResult result = TryInitDB(false);
  if (result == RESULT_RETRY) {
    // Database may be corrupt. Synchronously close the connection, clean up the
    // default DBState, and try again.
    COOKIE_LOGSTRING(LogLevel::Warning, ("InitDBStates(): retrying TryInitDB()"));
    CleanupCachedStatements();
    CleanupDefaultDBConnection();
    result = TryInitDB(true);
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
    CleanupCachedStatements();
    CleanupDefaultDBConnection();
  }
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
 * cleanup of the default DBState.
 */
OpenDBResult
nsCookieService::TryInitDB(bool aRecreateDB)
{
  NS_ASSERTION(!mDefaultDBState->dbConn, "nonnull dbConn");
  NS_ASSERTION(!mDefaultDBState->stmtInsert, "nonnull stmtInsert");
  NS_ASSERTION(!mDefaultDBState->insertListener, "nonnull insertListener");
  NS_ASSERTION(!mDefaultDBState->syncConn, "nonnull syncConn");

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
      getter_AddRefs(mDefaultDBState->dbConn));
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
  }

  // Set up our listeners.
  mDefaultDBState->insertListener = new InsertCookieDBListener(mDefaultDBState);
  mDefaultDBState->updateListener = new UpdateCookieDBListener(mDefaultDBState);
  mDefaultDBState->removeListener = new RemoveCookieDBListener(mDefaultDBState);
  mDefaultDBState->closeListener = new CloseCookieDBListener(mDefaultDBState);

  // Grow cookie db in 512KB increments
  mDefaultDBState->dbConn->SetGrowthIncrement(512 * 1024, EmptyCString());

  bool tableExists = false;
  mDefaultDBState->dbConn->TableExists(NS_LITERAL_CSTRING("moz_cookies"),
    &tableExists);
  if (!tableExists) {
    rv = CreateTable();
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  } else {
    // table already exists; check the schema version before reading
    int32_t dbSchemaVersion;
    rv = mDefaultDBState->dbConn->GetSchemaVersion(&dbSchemaVersion);
    NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

    // Start a transaction for the whole migration block.
    mozStorageTransaction transaction(mDefaultDBState->dbConn, true);

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
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD lastAccessed INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
      // Fall through to the next upgrade.

    case 2:
      {
        // Add the baseDomain column and index to the table.
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD baseDomain TEXT"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Compute the baseDomains for the table. This must be done eagerly
        // otherwise we won't be able to synchronously read in individual
        // domains on demand.
        const int64_t SCHEMA2_IDX_ID  =  0;
        const int64_t SCHEMA2_IDX_HOST = 1;
        nsCOMPtr<mozIStorageStatement> select;
        rv = mDefaultDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT id, host FROM moz_cookies"), getter_AddRefs(select));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageStatement> update;
        rv = mDefaultDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
          "UPDATE moz_cookies SET baseDomain = :baseDomain WHERE id = :id"),
          getter_AddRefs(update));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCString baseDomain, host;
        bool hasResult;
        while (1) {
          rv = select->ExecuteStep(&hasResult);
          NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

          if (!hasResult)
            break;

          int64_t id = select->AsInt64(SCHEMA2_IDX_ID);
          select->GetUTF8String(SCHEMA2_IDX_HOST, host);

          rv = GetBaseDomainFromHost(host, baseDomain);
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
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
      // Fall through to the next upgrade.

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
        rv = mDefaultDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT id, name, host, path FROM moz_cookies "
            "ORDER BY name ASC, host ASC, path ASC, expiry ASC"),
          getter_AddRefs(select));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        nsCOMPtr<mozIStorageStatement> deleteExpired;
        rv = mDefaultDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
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
          while (1) {
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
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD creationTime INTEGER"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy the id of each row into the new creationTime column.
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "UPDATE moz_cookies SET creationTime = "
            "(SELECT id WHERE id = moz_cookies.id)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create a unique index on (name, host, path) to allow fast lookup.
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "CREATE UNIQUE INDEX moz_uniqueid "
          "ON moz_cookies (name, host, path)"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
      // Fall through to the next upgrade.

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
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies RENAME TO moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop existing index (CreateTable will create new one for new table)
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "DROP INDEX moz_basedomain"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Create new table (with new fields and new unique constraint)
        rv = CreateTable();
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Copy data from old table, using appId/inBrowser=0 for existing rows
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "INSERT INTO moz_cookies "
          "(baseDomain, appId, inBrowserElement, name, value, host, path, expiry,"
          " lastAccessed, creationTime, isSecure, isHttpOnly) "
          "SELECT baseDomain, 0, 0, name, value, host, path, expiry,"
          " lastAccessed, creationTime, isSecure, isHttpOnly "
          "FROM moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        // Drop old table
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "DROP TABLE moz_cookies_old"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        COOKIE_LOGSTRING(LogLevel::Debug, 
          ("Upgraded database to schema version 5"));
      }

      // No more upgrades. Update the schema version.
      rv = mDefaultDBState->dbConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
      NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

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
        rv = mDefaultDBState->dbConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
      // fall through to downgrade check

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
        rv = mDefaultDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT "
            "id, "
            "baseDomain, "
            "appId, "
            "inBrowserElement, "
            "name, "
            "value, "
            "host, "
            "path, "
            "expiry, "
            "lastAccessed, "
            "creationTime, "
            "isSecure, "
            "isHttpOnly "
          "FROM moz_cookies"), getter_AddRefs(stmt));
        if (NS_SUCCEEDED(rv))
          break;

        // our columns aren't there - drop the table!
        rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "DROP TABLE moz_cookies"));
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

        rv = CreateTable();
        NS_ENSURE_SUCCESS(rv, RESULT_RETRY);
      }
      break;
    }
  }

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
      "appId, "
      "inBrowserElement, "
      "name, "
      "value, "
      "host, "
      "path, "
      "expiry, "
      "lastAccessed, "
      "creationTime, "
      "isSecure, "
      "isHttpOnly"
    ") VALUES ("
      ":baseDomain, "
      ":appId, "
      ":inBrowserElement, "
      ":name, "
      ":value, "
      ":host, "
      ":path, "
      ":expiry, "
      ":lastAccessed, "
      ":creationTime, "
      ":isSecure, "
      ":isHttpOnly"
    ")"),
    getter_AddRefs(mDefaultDBState->stmtInsert));
  NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  rv = mDefaultDBState->dbConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_cookies "
    "WHERE name = :name AND host = :host AND path = :path"),
    getter_AddRefs(mDefaultDBState->stmtDelete));
  NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  rv = mDefaultDBState->dbConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_cookies SET lastAccessed = :lastAccessed "
    "WHERE name = :name AND host = :host AND path = :path"),
    getter_AddRefs(mDefaultDBState->stmtUpdate));
  NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  // if we deleted a corrupt db, don't attempt to import - return now
  if (aRecreateDB)
    return RESULT_OK;

  // check whether to import or just read in the db
  if (tableExists)
    return Read();

  nsCOMPtr<nsIFile> oldCookieFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
    getter_AddRefs(oldCookieFile));
  if (NS_FAILED(rv)) return RESULT_OK;

  // Import cookies, and clean up the old file regardless of success or failure.
  // Note that we have to switch out our DBState temporarily, in case we're in
  // private browsing mode; otherwise ImportCookies() won't be happy.
  DBState* initialState = mDBState;
  mDBState = mDefaultDBState;
  oldCookieFile->AppendNative(NS_LITERAL_CSTRING(OLD_COOKIE_FILE_NAME));
  ImportCookies(oldCookieFile);
  oldCookieFile->Remove(false);
  mDBState = initialState;

  return RESULT_OK;
}

// Sets the schema version and creates the moz_cookies table.
nsresult
nsCookieService::CreateTable()
{
  // Set the schema version, before creating the table.
  nsresult rv = mDefaultDBState->dbConn->SetSchemaVersion(
    COOKIES_SCHEMA_VERSION);
  if (NS_FAILED(rv)) return rv;

  // Create the table.  We default appId/inBrowserElement to 0: this is so if
  // users revert to an older Firefox version that doesn't know about these
  // fields, any cookies set will still work once they upgrade back.
  rv = mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
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
  return mDefaultDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, "
                                                "appId, "
                                                "inBrowserElement)"));
}

void
nsCookieService::CloseDBStates()
{
  // Null out our private and pointer DBStates regardless.
  mPrivateDBState = nullptr;
  mDBState = nullptr;

  // If we don't have a default DBState, we're done.
  if (!mDefaultDBState)
    return;

  // Cleanup cached statements before we can close anything.
  CleanupCachedStatements();

  if (mDefaultDBState->dbConn) {
    // Cancel any pending read. No further results will be received by our
    // read listener.
    if (mDefaultDBState->pendingRead) {
      CancelAsyncRead(true);
    }

    // Asynchronously close the connection. We will null it below.
    mDefaultDBState->dbConn->AsyncClose(mDefaultDBState->closeListener);
  }

  CleanupDefaultDBConnection();

  mDefaultDBState = nullptr;
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
  mDefaultDBState->syncConn = nullptr;

  // Manually null out our listeners. This is necessary because they hold a
  // strong ref to the DBState itself. They'll stay alive until whatever
  // statements are still executing complete.
  mDefaultDBState->readListener = nullptr;
  mDefaultDBState->insertListener = nullptr;
  mDefaultDBState->updateListener = nullptr;
  mDefaultDBState->removeListener = nullptr;
  mDefaultDBState->closeListener = nullptr;
}

void
nsCookieService::HandleDBClosed(DBState* aDBState)
{
  COOKIE_LOGSTRING(LogLevel::Debug,
    ("HandleDBClosed(): DBState %x closed", aDBState));

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
      ("HandleDBClosed(): DBState %x encountered error rebuilding db; move to "
       "'cookies.sqlite.bak-rebuild' gave rv 0x%x", aDBState, rv));
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
      ("HandleCorruptDB(): DBState %x is already closed, aborting", aDBState));
    return;
  }

  COOKIE_LOGSTRING(LogLevel::Debug,
    ("HandleCorruptDB(): DBState %x has corruptFlag %u", aDBState,
      aDBState->corruptFlag));

  // Mark the database corrupt, so the close listener can begin reconstructing
  // it.
  switch (mDefaultDBState->corruptFlag) {
  case DBState::OK: {
    // Move to 'closing' state.
    mDefaultDBState->corruptFlag = DBState::CLOSING_FOR_REBUILD;

    // Cancel any pending read and close the database. If we do have an
    // in-flight read we want to throw away all the results so far -- we have no
    // idea how consistent the database is. Note that we may have already
    // canceled the read but not emptied our readSet; do so now.
    mDefaultDBState->readSet.Clear();
    if (mDefaultDBState->pendingRead) {
      CancelAsyncRead(true);
      mDefaultDBState->syncConn = nullptr;
    }

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
      ("RebuildCorruptDB(): DBState %x is stale, aborting", aDBState));
    if (os) {
      os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
    }
    return;
  }

  COOKIE_LOGSTRING(LogLevel::Debug,
    ("RebuildCorruptDB(): creating new database"));

  // The database has been closed, and we're ready to rebuild. Open a
  // connection.
  OpenDBResult result = TryInitDB(true);
  if (result != RESULT_OK) {
    // We're done. Reset our DB connection and statements, and notify of
    // closure.
    COOKIE_LOGSTRING(LogLevel::Warning,
      ("RebuildCorruptDB(): TryInitDB() failed with result %u", result));
    CleanupCachedStatements();
    CleanupDefaultDBConnection();
    mDefaultDBState->corruptFlag = DBState::OK;
    if (os) {
      os->NotifyObservers(nullptr, "cookie-db-closed", nullptr);
    }
    return;
  }

  // Notify observers that we're beginning the rebuild.
  if (os) {
    os->NotifyObservers(nullptr, "cookie-db-rebuilding", nullptr);
  }

  // Enumerate the hash, and add cookies to the params array.
  mozIStorageAsyncStatement* stmt = aDBState->stmtInsert;
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  for (auto iter = aDBState->hostTable.Iter(); !iter.Done(); iter.Next()) {
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
    mDefaultDBState->corruptFlag = DBState::OK;
    return;
  }

  // Execute the statement. If any errors crop up, we won't try again.
  DebugOnly<nsresult> rv = stmt->BindParameters(paramsArray);
  NS_ASSERT_SUCCESS(rv);
  nsCOMPtr<mozIStoragePendingStatement> handle;
  rv = stmt->ExecuteAsync(aDBState->insertListener, getter_AddRefs(handle));
  NS_ASSERT_SUCCESS(rv);
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

  // Get app info, if channel is present.  Else assume default namespace.
  uint32_t appId = NECKO_NO_APP_ID;
  bool inBrowserElement = false;
  if (aChannel) {
    NS_GetAppInfo(aChannel, &appId, &inBrowserElement);
  }

  bool isPrivate = aChannel && NS_UsePrivateBrowsing(aChannel);

  nsAutoCString result;
  GetCookieStringInternal(aHostURI, isForeign, aHttpBound, appId,
                          inBrowserElement, isPrivate, result);
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
        MOZ_UTF16("Non-null prompt ignored by nsCookieService."));
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
        MOZ_UTF16("Non-null prompt ignored by nsCookieService."));
    }
  }
  return SetCookieStringCommon(aHostURI, aCookieHeader, aServerTime, aChannel,
                               true);
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

  // Get app info, if channel is present.  Else assume default namespace.
  uint32_t appId = NECKO_NO_APP_ID;
  bool inBrowserElement = false;
  if (aChannel) {
    NS_GetAppInfo(aChannel, &appId, &inBrowserElement);
  }

  bool isPrivate = aChannel && NS_UsePrivateBrowsing(aChannel);

  nsDependentCString cookieString(aCookieHeader);
  nsDependentCString serverTime(aServerTime ? aServerTime : "");
  SetCookieStringInternal(aHostURI, isForeign, cookieString,
                          serverTime, aFromHttp, appId, inBrowserElement,
                          isPrivate, aChannel);
  return NS_OK;
}

void
nsCookieService::SetCookieStringInternal(nsIURI             *aHostURI,
                                         bool                aIsForeign,
                                         nsDependentCString &aCookieHeader,
                                         const nsCString    &aServerTime,
                                         bool                aFromHttp,
                                         uint32_t            aAppId,
                                         bool                aInBrowserElement,
                                         bool                aIsPrivate,
                                         nsIChannel         *aChannel)
{
  NS_ASSERTION(aHostURI, "null host!");

  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return;
  }

  AutoRestore<DBState*> savePrevDBState(mDBState);
  mDBState = aIsPrivate ? mPrivateDBState : mDefaultDBState;

  // get the base domain for the host URI.
  // e.g. for "www.bbc.co.uk", this would be "bbc.co.uk".
  // file:// URI's (i.e. with an empty host) are allowed, but any other
  // scheme must have a non-empty host. A trailing dot in the host
  // is acceptable.
  bool requireHostMatch;
  nsAutoCString baseDomain;
  nsresult rv = GetBaseDomain(aHostURI, baseDomain, requireHostMatch);
  if (NS_FAILED(rv)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, 
                      "couldn't get base domain from URI");
    return;
  }

  nsCookieKey key(baseDomain, aAppId, aInBrowserElement);

  // check default prefs
  CookieStatus cookieStatus = CheckPrefs(aHostURI, aIsForeign, requireHostMatch,
                                         aCookieHeader.get());
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
  } while (0);

  // This can fail for a number of reasons, in which kind we fallback to "?"
  os->NotifyObservers(aHostURI, topic, MOZ_UTF16("?"));
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
                               const char16_t *aData)
{
  const char* topic = mDBState == mPrivateDBState ?
      "private-cookie-changed" : "cookie-changed";
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(aSubject, topic, aData);
  }
}

already_AddRefed<nsIArray>
nsCookieService::CreatePurgeList(nsICookie2* aCookie)
{
  nsCOMPtr<nsIMutableArray> removedList =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  removedList->AppendElement(aCookie, false);
  return removedList.forget();
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

  RemoveAllFromMemory();

  // clear the cookie file
  if (mDBState->dbConn) {
    NS_ASSERTION(mDBState == mDefaultDBState, "not in default DB state");

    // Cancel any pending read. No further results will be received by our
    // read listener.
    if (mDefaultDBState->pendingRead) {
      CancelAsyncRead(true);
    }

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
        ("RemoveAll(): corruption detected with rv 0x%x", rv));
      HandleCorruptDB(mDefaultDBState);
    }
  }

  NotifyChanged(nullptr, MOZ_UTF16("cleared"));
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetEnumerator(nsISimpleEnumerator **aEnumerator)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureReadComplete();

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
nsCookieService::Add(const nsACString &aHost,
                     const nsACString &aPath,
                     const nsACString &aName,
                     const nsACString &aValue,
                     bool              aIsSecure,
                     bool              aIsHttpOnly,
                     bool              aIsSession,
                     int64_t           aExpiry)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the base domain for the host URI.
  // e.g. for "www.bbc.co.uk", this would be "bbc.co.uk".
  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t currentTimeInUsec = PR_Now();

  nsRefPtr<nsCookie> cookie =
    nsCookie::Create(aName, aValue, host, aPath,
                     aExpiry,
                     currentTimeInUsec,
                     nsCookie::GenerateUniqueCreationTime(currentTimeInUsec),
                     aIsSession,
                     aIsSecure,
                     aIsHttpOnly);
  if (!cookie) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  AddInternal(DEFAULT_APP_KEY(baseDomain), cookie, currentTimeInUsec, nullptr, nullptr, true);
  return NS_OK;
}


nsresult
nsCookieService::Remove(const nsACString& aHost, uint32_t aAppId,
                        bool aInBrowserElement, const nsACString& aName,
                        const nsACString& aPath, bool aBlocked)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsListIter matchIter;
  nsRefPtr<nsCookie> cookie;
  if (FindCookie(nsCookieKey(baseDomain, aAppId, aInBrowserElement),
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

    host.Insert(NS_LITERAL_CSTRING("http://"), 0);

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), host);

    if (uri)
      mPermissionService->SetAccess(uri, nsICookiePermission::ACCESS_DENY);
  }

  if (cookie) {
    // Everything's done. Notify observers.
    NotifyChanged(cookie, MOZ_UTF16("deleted"));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::Remove(const nsACString &aHost,
                        const nsACString &aName,
                        const nsACString &aPath,
                        bool             aBlocked)
{
  return Remove(aHost, NECKO_NO_APP_ID, false, aName, aPath, aBlocked);
}

/******************************************************************************
 * nsCookieService impl:
 * private file I/O functions
 ******************************************************************************/

// Begin an asynchronous read from the database.
OpenDBResult
nsCookieService::Read()
{
  // Set up a statement for the read. Note that our query specifies that
  // 'baseDomain' not be nullptr -- see below for why.
  nsCOMPtr<mozIStorageAsyncStatement> stmtRead;
  nsresult rv = mDefaultDBState->dbConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
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
      "appId,  "
      "inBrowserElement "
    "FROM moz_cookies "
    "WHERE baseDomain NOTNULL"), getter_AddRefs(stmtRead));
  NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  // Set up a statement to delete any rows with a nullptr 'baseDomain'
  // column. This takes care of any cookies set by browsers that don't
  // understand the 'baseDomain' column, where the database schema version
  // is from one that does. (This would occur when downgrading.)
  nsCOMPtr<mozIStorageAsyncStatement> stmtDeleteNull;
  rv = mDefaultDBState->dbConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_cookies WHERE baseDomain ISNULL"),
    getter_AddRefs(stmtDeleteNull));
  NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  // Start a new connection for sync reads, to reduce contention with the
  // background thread. We need to do this before we kick off write statements,
  // since they can lock the database and prevent connections from being opened.
  rv = mStorageService->OpenUnsharedDatabase(mDefaultDBState->cookieFile,
    getter_AddRefs(mDefaultDBState->syncConn));
  NS_ENSURE_SUCCESS(rv, RESULT_RETRY);

  // Init our readSet hash and execute the statements. Note that, after this
  // point, we cannot fail without altering the cleanup code in InitDBStates()
  // to handle closing of the now-asynchronous connection.
  mDefaultDBState->hostArray.SetCapacity(kMaxNumberOfCookies);

  mDefaultDBState->readListener = new ReadCookieDBListener(mDefaultDBState);
  rv = stmtRead->ExecuteAsync(mDefaultDBState->readListener,
    getter_AddRefs(mDefaultDBState->pendingRead));
  NS_ASSERT_SUCCESS(rv);

  nsCOMPtr<mozIStoragePendingStatement> handle;
  rv = stmtDeleteNull->ExecuteAsync(mDefaultDBState->removeListener,
    getter_AddRefs(handle));
  NS_ASSERT_SUCCESS(rv);

  return RESULT_OK;
}

// Extract data from a single result row and create an nsCookie.
// This is templated since 'T' is different for sync vs async results.
template<class T> nsCookie*
nsCookieService::GetCookieFromRow(T &aRow)
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

  // Create a new nsCookie and assign the data.
  return nsCookie::Create(name, value, host, path,
                          expiry,
                          lastAccessed,
                          creationTime,
                          false,
                          isSecure,
                          isHttpOnly);
}

void
nsCookieService::AsyncReadComplete()
{
  // We may be in the private browsing DB state, with a pending read on the
  // default DB state. (This would occur if we started up in private browsing
  // mode.) As long as we do all our operations on the default state, we're OK.
  NS_ASSERTION(mDefaultDBState, "no default DBState");
  NS_ASSERTION(mDefaultDBState->pendingRead, "no pending read");
  NS_ASSERTION(mDefaultDBState->readListener, "no read listener");

  // Merge the data read on the background thread with the data synchronously
  // read on the main thread. Note that transactions on the cookie table may
  // have occurred on the main thread since, making the background data stale.
  for (uint32_t i = 0; i < mDefaultDBState->hostArray.Length(); ++i) {
    const CookieDomainTuple &tuple = mDefaultDBState->hostArray[i];

    // Tiebreak: if the given base domain has already been read in, ignore
    // the background data. Note that readSet may contain domains that were
    // queried but found not to be in the db -- that's harmless.
    if (mDefaultDBState->readSet.GetEntry(tuple.key))
      continue;

    AddCookieToList(tuple.key, tuple.cookie, mDefaultDBState, nullptr, false);
  }

  mDefaultDBState->stmtReadDomain = nullptr;
  mDefaultDBState->pendingRead = nullptr;
  mDefaultDBState->readListener = nullptr;
  mDefaultDBState->syncConn = nullptr;
  mDefaultDBState->hostArray.Clear();
  mDefaultDBState->readSet.Clear();

  COOKIE_LOGSTRING(LogLevel::Debug, ("Read(): %ld cookies read",
                                  mDefaultDBState->cookieCount));

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, "cookie-db-read", nullptr);
  }
}

void
nsCookieService::CancelAsyncRead(bool aPurgeReadSet)
{
  // We may be in the private browsing DB state, with a pending read on the
  // default DB state. (This would occur if we started up in private browsing
  // mode.) As long as we do all our operations on the default state, we're OK.
  NS_ASSERTION(mDefaultDBState, "no default DBState");
  NS_ASSERTION(mDefaultDBState->pendingRead, "no pending read");
  NS_ASSERTION(mDefaultDBState->readListener, "no read listener");

  // Cancel the pending read, kill the read listener, and empty the array
  // of data already read in on the background thread.
  mDefaultDBState->readListener->Cancel();
  DebugOnly<nsresult> rv = mDefaultDBState->pendingRead->Cancel();
  NS_ASSERT_SUCCESS(rv);

  mDefaultDBState->stmtReadDomain = nullptr;
  mDefaultDBState->pendingRead = nullptr;
  mDefaultDBState->readListener = nullptr;
  mDefaultDBState->hostArray.Clear();

  // Only clear the 'readSet' table if we no longer need to know what set of
  // data is already accounted for.
  if (aPurgeReadSet)
    mDefaultDBState->readSet.Clear();
}

void
nsCookieService::EnsureReadDomain(const nsCookieKey &aKey)
{
  NS_ASSERTION(!mDBState->dbConn || mDBState == mDefaultDBState,
    "not in default db state");

  // Fast path 1: nothing to read, or we've already finished reading.
  if (MOZ_LIKELY(!mDBState->dbConn || !mDefaultDBState->pendingRead))
    return;

  // Fast path 2: already read in this particular domain.
  if (MOZ_LIKELY(mDefaultDBState->readSet.GetEntry(aKey)))
    return;

  // Read in the data synchronously.
  // see IDX_NAME, etc. for parameter indexes
  nsresult rv;
  if (!mDefaultDBState->stmtReadDomain) {
    // Cache the statement, since it's likely to be used again.
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
        "isHttpOnly "
      "FROM moz_cookies "
      "WHERE baseDomain = :baseDomain "
      "  AND appId = :appId "
      "  AND inBrowserElement = :inBrowserElement"),
      getter_AddRefs(mDefaultDBState->stmtReadDomain));

    if (NS_FAILED(rv)) {
      // Recreate the database.
      COOKIE_LOGSTRING(LogLevel::Debug,
        ("EnsureReadDomain(): corruption detected when creating statement "
         "with rv 0x%x", rv));
      HandleCorruptDB(mDefaultDBState);
      return;
    }
  }

  NS_ASSERTION(mDefaultDBState->syncConn, "should have a sync db connection");

  mozStorageStatementScoper scoper(mDefaultDBState->stmtReadDomain);

  rv = mDefaultDBState->stmtReadDomain->BindUTF8StringByName(
    NS_LITERAL_CSTRING("baseDomain"), aKey.mBaseDomain);
  NS_ASSERT_SUCCESS(rv);
  rv = mDefaultDBState->stmtReadDomain->BindInt32ByName(
    NS_LITERAL_CSTRING("appId"), aKey.mAppId);
  NS_ASSERT_SUCCESS(rv);
  rv = mDefaultDBState->stmtReadDomain->BindInt32ByName(
    NS_LITERAL_CSTRING("inBrowserElement"), aKey.mInBrowserElement ? 1 : 0);
  NS_ASSERT_SUCCESS(rv);


  bool hasResult;
  nsCString name, value, host, path;
  nsAutoTArray<nsRefPtr<nsCookie>, kMaxCookiesPerHost> array;
  while (1) {
    rv = mDefaultDBState->stmtReadDomain->ExecuteStep(&hasResult);
    if (NS_FAILED(rv)) {
      // Recreate the database.
      COOKIE_LOGSTRING(LogLevel::Debug,
        ("EnsureReadDomain(): corruption detected when reading result "
         "with rv 0x%x", rv));
      HandleCorruptDB(mDefaultDBState);
      return;
    }

    if (!hasResult)
      break;

    array.AppendElement(GetCookieFromRow(mDefaultDBState->stmtReadDomain));
  }

  // Add the cookies to the table in a single operation. This makes sure that
  // either all the cookies get added, or in the case of corruption, none.
  for (uint32_t i = 0; i < array.Length(); ++i) {
    AddCookieToList(aKey, array[i], mDefaultDBState, nullptr, false);
  }

  // Add it to the hashset of read entries, so we don't read it again.
  mDefaultDBState->readSet.PutEntry(aKey);

  COOKIE_LOGSTRING(LogLevel::Debug,
    ("EnsureReadDomain(): %ld cookies read for base domain %s, "
     " appId=%u, inBrowser=%d", array.Length(), aKey.mBaseDomain.get(),
     (unsigned)aKey.mAppId, (int)aKey.mInBrowserElement));
}

void
nsCookieService::EnsureReadComplete()
{
  NS_ASSERTION(!mDBState->dbConn || mDBState == mDefaultDBState,
    "not in default db state");

  // Fast path 1: nothing to read, or we've already finished reading.
  if (MOZ_LIKELY(!mDBState->dbConn || !mDefaultDBState->pendingRead))
    return;

  // Cancel the pending read, so we don't get any more results.
  CancelAsyncRead(false);

  // Read in the data synchronously.
  // see IDX_NAME, etc. for parameter indexes
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDefaultDBState->syncConn->CreateStatement(NS_LITERAL_CSTRING(
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
      "appId,  "
      "inBrowserElement "
    "FROM moz_cookies "
    "WHERE baseDomain NOTNULL"), getter_AddRefs(stmt));

  if (NS_FAILED(rv)) {
    // Recreate the database.
    COOKIE_LOGSTRING(LogLevel::Debug,
      ("EnsureReadComplete(): corruption detected when creating statement "
       "with rv 0x%x", rv));
    HandleCorruptDB(mDefaultDBState);
    return;
  }

  nsCString baseDomain, name, value, host, path;
  uint32_t appId;
  bool inBrowserElement, hasResult;
  nsAutoTArray<CookieDomainTuple, kMaxNumberOfCookies> array;
  while (1) {
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_FAILED(rv)) {
      // Recreate the database.
      COOKIE_LOGSTRING(LogLevel::Debug,
        ("EnsureReadComplete(): corruption detected when reading result "
         "with rv 0x%x", rv));
      HandleCorruptDB(mDefaultDBState);
      return;
    }

    if (!hasResult)
      break;

    // Make sure we haven't already read the data.
    stmt->GetUTF8String(IDX_BASE_DOMAIN, baseDomain);
    appId = static_cast<uint32_t>(stmt->AsInt32(IDX_APP_ID));
    inBrowserElement = static_cast<bool>(stmt->AsInt32(IDX_BROWSER_ELEM));
    nsCookieKey key(baseDomain, appId, inBrowserElement);
    if (mDefaultDBState->readSet.GetEntry(key))
      continue;

    CookieDomainTuple* tuple = array.AppendElement();
    tuple->key = key;
    tuple->cookie = GetCookieFromRow(stmt);
  }

  // Add the cookies to the table in a single operation. This makes sure that
  // either all the cookies get added, or in the case of corruption, none.
  for (uint32_t i = 0; i < array.Length(); ++i) {
    CookieDomainTuple& tuple = array[i];
    AddCookieToList(tuple.key, tuple.cookie, mDefaultDBState, nullptr,
      false);
  }

  mDefaultDBState->syncConn = nullptr;
  mDefaultDBState->readSet.Clear();

  COOKIE_LOGSTRING(LogLevel::Debug,
    ("EnsureReadComplete(): %ld cookies read", array.Length()));
}

NS_IMETHODIMP
nsCookieService::ImportCookies(nsIFile *aCookieFile)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

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

  // First, ensure we've read in everything from the database, if we have one.
  EnsureReadComplete();

  static const char kTrue[] = "TRUE";

  nsAutoCString buffer, baseDomain;
  bool isMore = true;
  int32_t hostIndex, isDomainIndex, pathIndex, secureIndex, expiresIndex, nameIndex, cookieIndex;
  nsASingleFragmentCString::char_iterator iter;
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
    const nsASingleFragmentCString &host = Substring(buffer, hostIndex, isDomainIndex - hostIndex - 1);
    // check for bad legacy cookies (domain not starting with a dot, or containing a port),
    // and discard
    if ((isDomain && !host.IsEmpty() && host.First() != '.') ||
        host.Contains(':')) {
      continue;
    }

    // compute the baseDomain from the host
    rv = GetBaseDomainFromHost(host, baseDomain);
    if (NS_FAILED(rv))
      continue;

    // pre-existing cookies have appId=0, inBrowser=false
    nsCookieKey key = DEFAULT_APP_KEY(baseDomain);

    // Create a new nsCookie and assign the data. We don't know the cookie
    // creation time, so just use the current time to generate a unique one.
    nsRefPtr<nsCookie> newCookie =
      nsCookie::Create(Substring(buffer, nameIndex, cookieIndex - nameIndex - 1),
                       Substring(buffer, cookieIndex, buffer.Length() - cookieIndex),
                       host,
                       Substring(buffer, pathIndex, secureIndex - pathIndex - 1),
                       expires,
                       lastAccessedCounter,
                       nsCookie::GenerateUniqueCreationTime(currentTimeInUsec),
                       false,
                       Substring(buffer, secureIndex, expiresIndex - secureIndex - 1).EqualsLiteral(kTrue),
                       isHttpOnly);
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


  COOKIE_LOGSTRING(LogLevel::Debug, ("ImportCookies(): %ld cookies imported",
    mDefaultDBState->cookieCount));

  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private GetCookie/SetCookie helpers
 ******************************************************************************/

// helper function for GetCookieList
static inline bool ispathdelimiter(char c) { return c == '/' || c == '?' || c == '#' || c == ';'; }

// Comparator class for sorting cookies before sending to a server.
class CompareCookiesForSending
{
public:
  bool Equals(const nsCookie* aCookie1, const nsCookie* aCookie2) const
  {
    return aCookie1->CreationTime() == aCookie2->CreationTime() &&
           aCookie2->Path().Length() == aCookie1->Path().Length();
  }

  bool LessThan(const nsCookie* aCookie1, const nsCookie* aCookie2) const
  {
    // compare by cookie path length in accordance with RFC2109
    int32_t result = aCookie2->Path().Length() - aCookie1->Path().Length();
    if (result != 0)
      return result < 0;

    // when path lengths match, older cookies should be listed first.  this is
    // required for backwards compatibility since some websites erroneously
    // depend on receiving cookies in the order in which they were sent to the
    // browser!  see bug 236772.
    return aCookie1->CreationTime() < aCookie2->CreationTime();
  }
};

void
nsCookieService::GetCookieStringInternal(nsIURI *aHostURI,
                                         bool aIsForeign,
                                         bool aHttpBound,
                                         uint32_t aAppId,
                                         bool aInBrowserElement,
                                         bool aIsPrivate,
                                         nsCString &aCookieString)
{
  NS_ASSERTION(aHostURI, "null host!");

  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return;
  }

  AutoRestore<DBState*> savePrevDBState(mDBState);
  mDBState = aIsPrivate ? mPrivateDBState : mDefaultDBState;

  // get the base domain, host, and path from the URI.
  // e.g. for "www.bbc.co.uk", the base domain would be "bbc.co.uk".
  // file:// URI's (i.e. with an empty host) are allowed, but any other
  // scheme must have a non-empty host. A trailing dot in the host
  // is acceptable.
  bool requireHostMatch;
  nsAutoCString baseDomain, hostFromURI, pathFromURI;
  nsresult rv = GetBaseDomain(aHostURI, baseDomain, requireHostMatch);
  if (NS_SUCCEEDED(rv))
    rv = aHostURI->GetAsciiHost(hostFromURI);
  if (NS_SUCCEEDED(rv))
    rv = aHostURI->GetPath(pathFromURI);
  if (NS_FAILED(rv)) {
    COOKIE_LOGFAILURE(GET_COOKIE, aHostURI, nullptr, "invalid host/path from URI");
    return;
  }

  // check default prefs
  CookieStatus cookieStatus = CheckPrefs(aHostURI, aIsForeign, requireHostMatch,
                                         nullptr);
  // for GetCookie(), we don't fire rejection notifications.
  switch (cookieStatus) {
  case STATUS_REJECTED:
  case STATUS_REJECTED_WITH_ERROR:
    return;
  default:
    break;
  }

  // check if aHostURI is using an https secure protocol.
  // if it isn't, then we can't send a secure cookie over the connection.
  // if SchemeIs fails, assume an insecure connection, to be on the safe side
  bool isSecure;
  if (NS_FAILED(aHostURI->SchemeIs("https", &isSecure))) {
    isSecure = false;
  }

  nsCookie *cookie;
  nsAutoTArray<nsCookie*, 8> foundCookieList;
  int64_t currentTimeInUsec = PR_Now();
  int64_t currentTime = currentTimeInUsec / PR_USEC_PER_SEC;
  bool stale = false;

  nsCookieKey key(baseDomain, aAppId, aInBrowserElement);
  EnsureReadDomain(key);

  // perform the hash lookup
  nsCookieEntry *entry = mDBState->hostTable.GetEntry(key);
  if (!entry)
    return;

  // iterate the cookies!
  const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    cookie = cookies[i];

    // check the host, since the base domain lookup is conservative.
    // first, check for an exact host or domain cookie match, e.g. "google.com"
    // or ".google.com"; second a subdomain match, e.g.
    // host = "mail.google.com", cookie domain = ".google.com".
    if (cookie->RawHost() != hostFromURI &&
        !(cookie->IsDomain() && StringEndsWith(hostFromURI, cookie->Host())))
      continue;

    // if the cookie is secure and the host scheme isn't, we can't send it
    if (cookie->IsSecure() && !isSecure)
      continue;

    // if the cookie is httpOnly and it's not going directly to the HTTP
    // connection, don't send it
    if (cookie->IsHttpOnly() && !aHttpBound)
      continue;

    // calculate cookie path length, excluding trailing '/'
    uint32_t cookiePathLen = cookie->Path().Length();
    if (cookiePathLen > 0 && cookie->Path().Last() == '/')
      --cookiePathLen;

    // if the nsIURI path is shorter than the cookie path, don't send it back
    if (!StringBeginsWith(pathFromURI, Substring(cookie->Path(), 0, cookiePathLen)))
      continue;

    if (pathFromURI.Length() > cookiePathLen &&
        !ispathdelimiter(pathFromURI.CharAt(cookiePathLen))) {
      /*
       * |ispathdelimiter| tests four cases: '/', '?', '#', and ';'.
       * '/' is the "standard" case; the '?' test allows a site at host/abc?def
       * to receive a cookie that has a path attribute of abc.  this seems
       * strange but at least one major site (citibank, bug 156725) depends
       * on it.  The test for # and ; are put in to proactively avoid problems
       * with other sites - these are the only other chars allowed in the path.
       */
      continue;
    }

    // check if the cookie has expired
    if (cookie->Expiry() <= currentTime) {
      continue;
    }

    // all checks passed - add to list and check if lastAccessed stamp needs updating
    foundCookieList.AppendElement(cookie);
    if (cookie->IsStale()) {
      stale = true;
    }
  }

  int32_t count = foundCookieList.Length();
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
      cookie = foundCookieList.ElementAt(i);

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
  foundCookieList.Sort(CompareCookiesForSending());

  for (int32_t i = 0; i < count; ++i) {
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
nsCookieService::SetCookieInternal(nsIURI                        *aHostURI,
                                   const nsCookieKey             &aKey,
                                   bool                           aRequireHostMatch,
                                   CookieStatus                   aStatus,
                                   nsDependentCString            &aCookieHeader,
                                   int64_t                        aServerTime,
                                   bool                           aFromHttp,
                                   nsIChannel                    *aChannel)
{
  NS_ASSERTION(aHostURI, "null host!");

  // create a stack-based nsCookieAttributes, to store all the
  // attributes parsed from the cookie
  nsCookieAttributes cookieAttributes;

  // init expiryTime such that session cookies won't prematurely expire
  cookieAttributes.expiryTime = INT64_MAX;

  // aCookieHeader is an in/out param to point to the next cookie, if
  // there is one. Save the present value for logging purposes
  nsDependentCString savedCookieHeader(aCookieHeader);

  // newCookie says whether there are multiple cookies in the header;
  // so we can handle them separately.
  bool newCookie = ParseAttributes(aCookieHeader, cookieAttributes);

  int64_t currentTimeInUsec = PR_Now();

  // calculate expiry time of cookie.
  cookieAttributes.isSession = GetExpiry(cookieAttributes, aServerTime,
                                         currentTimeInUsec / PR_USEC_PER_SEC);
  if (aStatus == STATUS_ACCEPT_SESSION) {
    // force lifetime to session. note that the expiration time, if set above,
    // will still apply.
    cookieAttributes.isSession = true;
  }

  // reject cookie if it's over the size limit, per RFC2109
  if ((cookieAttributes.name.Length() + cookieAttributes.value.Length()) > kMaxBytesPerCookie) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "cookie too big (> 4kb)");
    return newCookie;
  }

  if (cookieAttributes.name.Contains('\t')) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "invalid name character");
    return newCookie;
  }

  // domain & path checks
  if (!CheckDomain(cookieAttributes, aHostURI, aKey.mBaseDomain, aRequireHostMatch)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "failed the domain tests");
    return newCookie;
  }
  if (!CheckPath(cookieAttributes, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "failed the path tests");
    return newCookie;
  }

  // reject cookie if value contains an RFC 6265 disallowed character - see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1191423
  // NOTE: this is not the full set of characters disallowed by 6265 - notably
  // 0x09, 0x20, 0x22, 0x2C, 0x5C, and 0x7F are missing from this list. This is
  // for parity with Chrome.
  const char illegalCharacters[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                     0x08, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                     0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
                                     0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
                                     0x1E, 0x1F, 0x3B };
  if (cookieAttributes.value.FindCharInSet(illegalCharacters, 0) != -1) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "invalid value character");
    return newCookie;
  }

  // create a new nsCookie and copy attributes
  nsRefPtr<nsCookie> cookie =
    nsCookie::Create(cookieAttributes.name,
                     cookieAttributes.value,
                     cookieAttributes.host,
                     cookieAttributes.path,
                     cookieAttributes.expiryTime,
                     currentTimeInUsec,
                     nsCookie::GenerateUniqueCreationTime(currentTimeInUsec),
                     cookieAttributes.isSession,
                     cookieAttributes.isSecure,
                     cookieAttributes.isHttpOnly);
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
nsCookieService::AddInternal(const nsCookieKey             &aKey,
                             nsCookie                      *aCookie,
                             int64_t                        aCurrentTimeInUsec,
                             nsIURI                        *aHostURI,
                             const char                    *aCookieHeader,
                             bool                           aFromHttp)
{
  int64_t currentTime = aCurrentTimeInUsec / PR_USEC_PER_SEC;

  // if the new cookie is httponly, make sure we're not coming from script
  if (!aFromHttp && aCookie->IsHttpOnly()) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
      "cookie is httponly; coming from script");
    return;
  }

  nsListIter matchIter;
  bool foundCookie = FindCookie(aKey, aCookie->Host(),
    aCookie->Name(), aCookie->Path(), matchIter);

  nsRefPtr<nsCookie> oldCookie;
  nsCOMPtr<nsIArray> purgedList;
  if (foundCookie) {
    oldCookie = matchIter.Cookie();

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
      RemoveCookieFromList(matchIter);
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
      RemoveCookieFromList(matchIter);

      // If the new cookie has expired -- i.e. the intent was simply to delete
      // the old cookie -- then we're done.
      if (aCookie->Expiry() <= currentTime) {
        COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
          "previously stored cookie was deleted");
        NotifyChanged(oldCookie, MOZ_UTF16("deleted"));
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
      FindStaleCookie(entry, currentTime, iter);
      oldCookie = iter.Cookie();

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
    NotifyChanged(purgedList, MOZ_UTF16("batch-deleted"));
  }

  NotifyChanged(aCookie, foundCookie ? MOZ_UTF16("changed")
                                     : MOZ_UTF16("added"));
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
nsCookieService::GetTokenValue(nsASingleFragmentCString::const_char_iterator &aIter,
                               nsASingleFragmentCString::const_char_iterator &aEndIter,
                               nsDependentCSubstring                         &aTokenString,
                               nsDependentCSubstring                         &aTokenValue,
                               bool                                          &aEqualsFound)
{
  nsASingleFragmentCString::const_char_iterator start, lastSpace;
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

  nsASingleFragmentCString::const_char_iterator tempBegin, tempEnd;
  nsASingleFragmentCString::const_char_iterator cookieStart, cookieEnd;
  aCookieHeader.BeginReading(cookieStart);
  aCookieHeader.EndReading(cookieEnd);

  aCookieAttributes.isSecure = false;
  aCookieAttributes.isHttpOnly = false;
  
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
nsCookieService::GetBaseDomain(nsIURI    *aHostURI,
                               nsCString &aBaseDomain,
                               bool      &aRequireHostMatch)
{
  // get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = mTLDService->GetBaseDomain(aHostURI, 0, aBaseDomain);
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
// "bbc.co.uk". This is done differently than GetBaseDomain(): it is assumed
// that aHost is already normalized, and it may contain a leading dot
// (indicating that it represents a domain). A trailing dot may be present.
// If aHost is an IP address, an alias such as 'localhost', an eTLD such as
// 'co.uk', or the empty string, aBaseDomain will be the exact host, and a
// leading dot will be treated as an error.
nsresult
nsCookieService::GetBaseDomainFromHost(const nsACString &aHost,
                                       nsCString        &aBaseDomain)
{
  // aHost must not be the string '.'.
  if (aHost.Length() == 1 && aHost.Last() == '.')
    return NS_ERROR_INVALID_ARG;

  // aHost may contain a leading dot; if so, strip it now.
  bool domain = !aHost.IsEmpty() && aHost.First() == '.';

  // get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = mTLDService->GetBaseDomainFromHost(Substring(aHost, domain), 0, aBaseDomain);
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
nsCookieService::CheckPrefs(nsIURI          *aHostURI,
                            bool             aIsForeign,
                            bool             aRequireHostMatch,
                            const char      *aCookieHeader)
{
  nsresult rv;

  // don't let ftp sites get/set cookies (could be a security issue)
  bool ftp;
  if (NS_SUCCEEDED(aHostURI->SchemeIs("ftp", &ftp)) && ftp) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "ftp sites cannot read cookies");
    return STATUS_REJECTED_WITH_ERROR;
  }

  // check the permission list first; if we find an entry, it overrides
  // default prefs. see bug 184059.
  if (mPermissionService) {
    nsCookieAccess access;
    // Not passing an nsIChannel here is probably OK; our implementation
    // doesn't do anything with it anyway.
    rv = mPermissionService->CanAccess(aHostURI, nullptr, &access);

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
        uint32_t priorCookieCount = 0;
        nsAutoCString hostFromURI;
        aHostURI->GetHost(hostFromURI);
        CountCookiesFromHost(hostFromURI, &priorCookieCount);
        if (priorCookieCount == 0) {
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
  if (mCookieBehavior == nsICookieService::BEHAVIOR_REJECT) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "cookies are disabled");
    return STATUS_REJECTED;
  }

  // check if cookie is foreign
  if (aIsForeign) {
    if (mCookieBehavior == nsICookieService::BEHAVIOR_ACCEPT && mThirdPartySession)
      return STATUS_ACCEPT_SESSION;

    if (mCookieBehavior == nsICookieService::BEHAVIOR_REJECT_FOREIGN) {
      COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "context is third party");
      return STATUS_REJECTED;
    }

    if (mCookieBehavior == nsICookieService::BEHAVIOR_LIMIT_FOREIGN) {
      uint32_t priorCookieCount = 0;
      nsAutoCString hostFromURI;
      aHostURI->GetHost(hostFromURI);
      CountCookiesFromHost(hostFromURI, &priorCookieCount);
      if (priorCookieCount == 0) {
        COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "context is third party");
        return STATUS_REJECTED;
      }
      if (mThirdPartySession)
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
      aCookieAttributes.host.Insert(NS_LITERAL_CSTRING("."), 0);
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

bool
nsCookieService::CheckPath(nsCookieAttributes &aCookieAttributes,
                           nsIURI             *aHostURI)
{
  // if a path is given, check the host has permission
  if (aCookieAttributes.path.IsEmpty() || aCookieAttributes.path.First() != '/') {
    // strip down everything after the last slash to get the path,
    // ignoring slashes in the query string part.
    // if we can QI to nsIURL, that'll take care of the query string portion.
    // otherwise, it's not an nsIURL and can't have a query string, so just find the last slash.
    nsCOMPtr<nsIURL> hostURL = do_QueryInterface(aHostURI);
    if (hostURL) {
      hostURL->GetDirectory(aCookieAttributes.path);
    } else {
      aHostURI->GetPath(aCookieAttributes.path);
      int32_t slash = aCookieAttributes.path.RFindChar('/');
      if (slash != kNotFound) {
        aCookieAttributes.path.Truncate(slash + 1);
      }
    }

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
    if (NS_FAILED(aHostURI->GetPath(pathFromURI)) ||
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
  int64_t delta;

  // check for max-age attribute first; this overrides expires attribute
  if (!aCookieAttributes.maxage.IsEmpty()) {
    // obtain numeric value of maxageAttribute
    int64_t maxage;
    int32_t numInts = PR_sscanf(aCookieAttributes.maxage.get(), "%lld", &maxage);

    // default to session cookie if the conversion failed
    if (numInts != 1) {
      return true;
    }

    delta = maxage;

  // check for expires attribute
  } else if (!aCookieAttributes.expires.IsEmpty()) {
    PRTime expires;

    // parse expiry time
    if (PR_ParseTimeString(aCookieAttributes.expires.get(), true, &expires) != PR_SUCCESS) {
      return true;
    }

    delta = expires / int64_t(PR_USEC_PER_SEC) - aServerTime;

  // default to session cookie if no attributes found
  } else {
    return true;
  }

  // if this addition overflows, expiryTime will be less than currentTime
  // and the cookie will be expired - that's okay.
  aCookieAttributes.expiryTime = aCurrentTime + delta;

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
  EnsureReadComplete();

  uint32_t initialCookieCount = mDBState->cookieCount;
  COOKIE_LOGSTRING(LogLevel::Debug,
    ("PurgeCookies(): beginning purge with %ld cookies and %lld oldest age",
     mDBState->cookieCount, aCurrentTimeInUsec - mDBState->cookieOldestTime));

  typedef nsAutoTArray<nsListIter, kMaxNumberOfCookies> PurgeList;
  PurgeList purgeList;

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

    const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
    for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ) {
      nsListIter iter(entry, i);
      nsCookie* cookie = cookies[i];

      // check if the cookie has expired
      if (cookie->Expiry() <= currentTime) {
        removedList->AppendElement(cookie, false);
        COOKIE_LOGEVICTED(cookie, "Cookie expired");

        // remove from list; do not increment our iterator
        gCookieService->RemoveCookieFromList(iter, paramsArray);

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
    removedList->AppendElement(cookie, false);
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
    ("PurgeCookies(): %ld expired; %ld purged; %ld remain; %lld oldest age",
     initialCookieCount - postExpiryCookieCount,
     postExpiryCookieCount - mDBState->cookieCount,
     mDBState->cookieCount,
     aCurrentTimeInUsec - mDBState->cookieOldestTime));

  return removedList.forget();
}

// find whether a given cookie has been previously set. this is provided by the
// nsICookieManager2 interface.
NS_IMETHODIMP
nsCookieService::CookieExists(nsICookie2 *aCookie,
                              bool       *aFoundCookie)
{
  NS_ENSURE_ARG_POINTER(aCookie);

  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString host, name, path;
  nsresult rv = aCookie->GetHost(host);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCookie->GetName(name);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCookie->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsListIter iter;
  *aFoundCookie = FindCookie(DEFAULT_APP_KEY(baseDomain), host, name, path, iter);
  return NS_OK;
}

// For a given base domain, find either an expired cookie or the oldest cookie
// by lastAccessed time.
void
nsCookieService::FindStaleCookie(nsCookieEntry *aEntry,
                                 int64_t aCurrentTime,
                                 nsListIter &aIter)
{
  aIter.entry = nullptr;

  int64_t oldestTime = 0;
  const nsCookieEntry::ArrayType &cookies = aEntry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    nsCookie *cookie = cookies[i];

    // If we found an expired cookie, we're done.
    if (cookie->Expiry() <= aCurrentTime) {
      aIter.entry = aEntry;
      aIter.index = i;
      return;
    }

    // Check if we've found the oldest cookie so far.
    if (!aIter.entry || oldestTime > cookie->LastAccessed()) {
      oldestTime = cookie->LastAccessed();
      aIter.entry = aEntry;
      aIter.index = i;
    }
  }
}

// count the number of cookies stored by a particular host. this is provided by the
// nsICookieManager2 interface.
NS_IMETHODIMP
nsCookieService::CountCookiesFromHost(const nsACString &aHost,
                                      uint32_t         *aCountFromHost)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCookieKey key = DEFAULT_APP_KEY(baseDomain);
  EnsureReadDomain(key);

  // Return a count of all cookies, including expired.
  nsCookieEntry *entry = mDBState->hostTable.GetEntry(key);
  *aCountFromHost = entry ? entry->GetCookies().Length() : 0;
  return NS_OK;
}

// get an enumerator of cookies stored by a particular host. this is provided by the
// nsICookieManager2 interface.
NS_IMETHODIMP
nsCookieService::GetCookiesFromHost(const nsACString     &aHost,
                                    nsISimpleEnumerator **aEnumerator)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  // first, normalize the hostname, and fail if it contains illegal characters.
  nsAutoCString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString baseDomain;
  rv = GetBaseDomainFromHost(host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCookieKey key = DEFAULT_APP_KEY(baseDomain);
  EnsureReadDomain(key);

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
nsCookieService::GetCookiesForApp(uint32_t aAppId, bool aOnlyBrowserElement,
                                  nsISimpleEnumerator** aEnumerator)
{
  if (!mDBState) {
    NS_WARNING("No DBState! Profile already closed?");
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ENSURE_TRUE(aAppId != NECKO_UNKNOWN_APP_ID, NS_ERROR_INVALID_ARG);

  nsCOMArray<nsICookie> cookies;
  for (auto iter = mDBState->hostTable.Iter(); !iter.Done(); iter.Next()) {
    nsCookieEntry* entry = iter.Get();

    if (entry->mAppId != aAppId ||
        (aOnlyBrowserElement && !entry->mInBrowserElement)) {
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
nsCookieService::RemoveCookiesForApp(uint32_t aAppId, bool aOnlyBrowserElement)
{
  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult rv = GetCookiesForApp(aAppId, aOnlyBrowserElement,
                                 getter_AddRefs(enumerator));

  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    nsCOMPtr<nsICookie> cookie;
    rv = enumerator->GetNext(getter_AddRefs(supports));
    NS_ENSURE_SUCCESS(rv, rv);

    cookie = do_QueryInterface(supports);

    nsAutoCString host;
    cookie->GetHost(host);

    nsAutoCString name;
    cookie->GetName(name);

    nsAutoCString path;
    cookie->GetPath(path);

    // nsICookie do not carry the appId/inBrowserElement information.
    // That means we have to guess. This is easy for appId but not for
    // inBrowserElement flag.
    // A simple solution is to always ask to remove the cookie with
    // inBrowserElement = true and only ask for the other one to be removed if
    // we happen to be in the case of !aOnlyBrowserElement.
    // Anyway, with this solution, we will likely be looking for unexistant
    // cookies.
    //
    // NOTE: we could make this better by getting nsCookieEntry objects instead
    // of plain nsICookie.
    Remove(host, aAppId, true, name, path, false);
    if (!aOnlyBrowserElement) {
      Remove(host, aAppId, false, name, path, false);
    }
  }

  return NS_OK;
}

// find an exact cookie specified by host, name, and path that hasn't expired.
bool
nsCookieService::FindCookie(const nsCookieKey    &aKey,
                            const nsAFlatCString &aHost,
                            const nsAFlatCString &aName,
                            const nsAFlatCString &aPath,
                            nsListIter           &aIter)
{
  EnsureReadDomain(aKey);

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

  rv = params->BindInt32ByName(NS_LITERAL_CSTRING("appId"),
                               aKey.mAppId);
  NS_ASSERT_SUCCESS(rv);

  rv = params->BindInt32ByName(NS_LITERAL_CSTRING("inBrowserElement"),
                               aKey.mInBrowserElement ? 1 : 0);
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
  return MOZ_COLLECT_REPORT(
    "explicit/cookie-service", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(CookieServiceMallocSizeOf),
    "Memory used by the cookie service.");
}
