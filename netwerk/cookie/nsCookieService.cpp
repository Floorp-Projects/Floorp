/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifdef MOZ_LOGGING
// this next define has to appear before the include of prlog.h
#define FORCE_PR_LOG // Allow logging in the release build
#endif

#ifdef MOZ_IPC
#include "mozilla/net/CookieServiceChild.h"
#include "mozilla/net/NeckoCommon.h"
#endif

#include "nsCookieService.h"
#include "nsIServiceManager.h"

#include "nsIIOService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
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
#include "prtime.h"
#include "prprf.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIPrivateBrowsingService.h"
#include "nsNetCID.h"
#include "mozilla/storage.h"
#include "mozIStorageCompletionCallback.h"
#include "mozilla/FunctionTimer.h"

using namespace mozilla::net;

/******************************************************************************
 * nsCookieService impl:
 * useful types & constants
 ******************************************************************************/

static nsCookieService *gCookieService;

// XXX_hack. See bug 178993.
// This is a hack to hide HttpOnly cookies from older browsers
static const char kHttpOnlyPrefix[] = "#HttpOnly_";

static const char kCookieFileName[] = "cookies.sqlite";
#define COOKIES_SCHEMA_VERSION 4

static const PRInt64 kCookieStaleThreshold = 60 * PR_USEC_PER_SEC; // 1 minute in microseconds
static const PRInt64 kCookiePurgeAge = 30 * 24 * 60 * 60 * PR_USEC_PER_SEC; // 30 days in microseconds

static const char kOldCookieFileName[] = "cookies.txt";

#undef  LIMIT
#define LIMIT(x, low, high, default) ((x) >= (low) && (x) <= (high) ? (x) : (default))

#undef  ADD_TEN_PERCENT
#define ADD_TEN_PERCENT(i) ((i) + (i)/10)

// default limits for the cookie list. these can be tuned by the
// network.cookie.maxNumber and network.cookie.maxPerHost prefs respectively.
static const PRUint32 kMaxNumberOfCookies = 3000;
static const PRUint32 kMaxCookiesPerHost  = 150;
static const PRUint32 kMaxBytesPerCookie  = 4096;
static const PRUint32 kMaxBytesPerPath    = 1024;

// behavior pref constants
static const PRUint32 BEHAVIOR_ACCEPT        = 0;
static const PRUint32 BEHAVIOR_REJECTFOREIGN = 1;
static const PRUint32 BEHAVIOR_REJECT        = 2;

// pref string constants
static const char kPrefCookieBehavior[]     = "network.cookie.cookieBehavior";
static const char kPrefMaxNumberOfCookies[] = "network.cookie.maxNumber";
static const char kPrefMaxCookiesPerHost[]  = "network.cookie.maxPerHost";
static const char kPrefCookiePurgeAge[]     = "network.cookie.purgeAge";
static const char kPrefThirdPartySession[]  = "network.cookie.thirdparty.sessionOnly";

// struct for temporarily storing cookie attributes during header parsing
struct nsCookieAttributes
{
  nsCAutoString name;
  nsCAutoString value;
  nsCAutoString host;
  nsCAutoString path;
  nsCAutoString expires;
  nsCAutoString maxage;
  PRInt64 expiryTime;
  PRBool isSession;
  PRBool isSecure;
  PRBool isHttpOnly;
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
#include "prlog.h"
#endif

// define logging macros for convenience
#define SET_COOKIE PR_TRUE
#define GET_COOKIE PR_FALSE

#ifdef PR_LOGGING
static PRLogModuleInfo *sCookieLog = PR_NewLogModule("cookie");

#define COOKIE_LOGFAILURE(a, b, c, d)    LogFailure(a, b, c, d)
#define COOKIE_LOGSUCCESS(a, b, c, d, e) LogSuccess(a, b, c, d, e)

#define COOKIE_LOGEVICTED(a, details)          \
  PR_BEGIN_MACRO                               \
    if (PR_LOG_TEST(sCookieLog, PR_LOG_DEBUG)) \
      LogEvicted(a, details);                  \
  PR_END_MACRO

#define COOKIE_LOGSTRING(lvl, fmt)   \
  PR_BEGIN_MACRO                     \
    PR_LOG(sCookieLog, lvl, fmt);    \
    PR_LOG(sCookieLog, lvl, ("\n")); \
  PR_END_MACRO

static void
LogFailure(PRBool aSetCookie, nsIURI *aHostURI, const char *aCookieString, const char *aReason)
{
  // if logging isn't enabled, return now to save cycles
  if (!PR_LOG_TEST(sCookieLog, PR_LOG_WARNING))
    return;

  nsCAutoString spec;
  if (aHostURI)
    aHostURI->GetAsciiSpec(spec);

  PR_LOG(sCookieLog, PR_LOG_WARNING,
    ("===== %s =====\n", aSetCookie ? "COOKIE NOT ACCEPTED" : "COOKIE NOT SENT"));
  PR_LOG(sCookieLog, PR_LOG_WARNING,("request URL: %s\n", spec.get()));
  if (aSetCookie)
    PR_LOG(sCookieLog, PR_LOG_WARNING,("cookie string: %s\n", aCookieString));

  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  PR_LOG(sCookieLog, PR_LOG_WARNING,("current time: %s", timeString));
  PR_LOG(sCookieLog, PR_LOG_WARNING,("rejected because %s\n", aReason));
  PR_LOG(sCookieLog, PR_LOG_WARNING,("\n"));
}

static void
LogCookie(nsCookie *aCookie)
{
  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  PR_LOG(sCookieLog, PR_LOG_DEBUG,("current time: %s", timeString));

  if (aCookie) {
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("----------------\n"));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("name: %s\n", aCookie->Name().get()));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("value: %s\n", aCookie->Value().get()));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("%s: %s\n", aCookie->IsDomain() ? "domain" : "host", aCookie->Host().get()));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("path: %s\n", aCookie->Path().get()));

    PR_ExplodeTime(aCookie->Expiry() * PR_USEC_PER_SEC, PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);
    PR_LOG(sCookieLog, PR_LOG_DEBUG,
      ("expires: %s%s", timeString, aCookie->IsSession() ? " (at end of session)" : ""));

    PR_ExplodeTime(aCookie->CreationTime(), PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("created: %s", timeString));

    PR_LOG(sCookieLog, PR_LOG_DEBUG,("is secure: %s\n", aCookie->IsSecure() ? "true" : "false"));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("is httpOnly: %s\n", aCookie->IsHttpOnly() ? "true" : "false"));
  }
}

static void
LogSuccess(PRBool aSetCookie, nsIURI *aHostURI, const char *aCookieString, nsCookie *aCookie, PRBool aReplacing)
{
  // if logging isn't enabled, return now to save cycles
  if (!PR_LOG_TEST(sCookieLog, PR_LOG_DEBUG)) {
    return;
  }

  nsCAutoString spec;
  if (aHostURI)
    aHostURI->GetAsciiSpec(spec);

  PR_LOG(sCookieLog, PR_LOG_DEBUG,
    ("===== %s =====\n", aSetCookie ? "COOKIE ACCEPTED" : "COOKIE SENT"));
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("request URL: %s\n", spec.get()));
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("cookie string: %s\n", aCookieString));
  if (aSetCookie)
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("replaces existing cookie: %s\n", aReplacing ? "true" : "false"));

  LogCookie(aCookie);

  PR_LOG(sCookieLog, PR_LOG_DEBUG,("\n"));
}

static void
LogEvicted(nsCookie *aCookie, const char* details)
{
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("===== COOKIE EVICTED =====\n"));
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("%s\n", details));

  LogCookie(aCookie);

  PR_LOG(sCookieLog, PR_LOG_DEBUG,("\n"));
}

// inline wrappers to make passing in nsAFlatCStrings easier
static inline void
LogFailure(PRBool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, const char *aReason)
{
  LogFailure(aSetCookie, aHostURI, aCookieString.get(), aReason);
}

static inline void
LogSuccess(PRBool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, nsCookie *aCookie, PRBool aReplacing)
{
  LogSuccess(aSetCookie, aHostURI, aCookieString.get(), aCookie, aReplacing);
}

#else
#define COOKIE_LOGFAILURE(a, b, c, d)    PR_BEGIN_MACRO /* nothing */ PR_END_MACRO
#define COOKIE_LOGSUCCESS(a, b, c, d, e) PR_BEGIN_MACRO /* nothing */ PR_END_MACRO
#define COOKIE_LOGEVICTED(a, b)          PR_BEGIN_MACRO /* nothing */ PR_END_MACRO
#define COOKIE_LOGSTRING(a, b)           PR_BEGIN_MACRO /* nothing */ PR_END_MACRO
#endif

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
  virtual const char *GetOpType() = 0;

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD HandleError(mozIStorageError* aError)
  {
    // XXX Ignore corruption handling for now. See bug 547031.
#ifdef PR_LOGGING
    PRInt32 result = -1;
    aError->GetResult(&result);
    nsCAutoString message;
    aError->GetMessage(message);
    COOKIE_LOGSTRING(PR_LOG_WARNING,
                     ("Error %d occurred while performing a %s operation.  Message: `%s`\n",
                      result, GetOpType(), message.get()));
#endif
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(DBListenerErrorHandler, mozIStorageStatementCallback)

/******************************************************************************
 * InsertCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous insertion operations.
 ******************************************************************************/
class InsertCookieDBListener : public DBListenerErrorHandler
{
protected:
  virtual const char *GetOpType() { return "INSERT"; }

public:
  NS_IMETHOD HandleResult(mozIStorageResultSet*)
  {
    NS_NOTREACHED("Unexpected call to InsertCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(PRUint16 aReason)
  {
    return NS_OK;
  }
};

/******************************************************************************
 * UpdateCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous update operations.
 ******************************************************************************/
class UpdateCookieDBListener : public DBListenerErrorHandler
{
protected:
  virtual const char *GetOpType() { return "UPDATE"; }

public:
  NS_IMETHOD HandleResult(mozIStorageResultSet*)
  {
    NS_NOTREACHED("Unexpected call to UpdateCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(PRUint16 aReason)
  {
    return NS_OK;
  }
};

/******************************************************************************
 * RemoveCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous removal operations.
 ******************************************************************************/
class RemoveCookieDBListener :  public DBListenerErrorHandler
{
protected:
  virtual const char *GetOpType() { return "REMOVE"; }

public:
  NS_IMETHOD HandleResult(mozIStorageResultSet*)
  {
    NS_NOTREACHED("Unexpected call to RemoveCookieDBListener::HandleResult");
    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(PRUint16 aReason)
  {
    return NS_OK;
  }
};

/******************************************************************************
 * ReadCookieDBListener impl:
 * mozIStorageStatementCallback used to track asynchronous removal operations.
 ******************************************************************************/
class ReadCookieDBListener :  public DBListenerErrorHandler
{
protected:
  virtual const char *GetOpType() { return "READ"; }
  bool mCanceled;

public:
  ReadCookieDBListener() : mCanceled(false) { }

  void Cancel() { mCanceled = true; }

  NS_IMETHOD HandleResult(mozIStorageResultSet *aResult)
  {
    nsresult rv;
    nsCOMPtr<mozIStorageRow> row;
    nsTArray<CookieDomainTuple> &cookieArray =
      gCookieService->mDefaultDBState.hostArray;

    while (1) {
      rv = aResult->GetNextRow(getter_AddRefs(row));
      NS_ASSERT_SUCCESS(rv);

      if (!row)
        break;

      CookieDomainTuple *tuple = cookieArray.AppendElement();
      row->GetUTF8String(9, tuple->baseDomain);
      tuple->cookie = gCookieService->GetCookieFromRow(row);
    }

    return NS_OK;
  }
  NS_IMETHOD HandleCompletion(PRUint16 aReason)
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
      COOKIE_LOGSTRING(PR_LOG_DEBUG, ("Read canceled"));
      break;
    case mozIStorageStatementCallback::REASON_ERROR:
      // Nothing more to do here. DBListenerErrorHandler::HandleError()
      // can take handle it.
      COOKIE_LOGSTRING(PR_LOG_DEBUG, ("Read error"));
      break;
    default:
      NS_NOTREACHED("invalid reason");
    }
    return NS_OK;
  }
};

/******************************************************************************
 * CloseCookieDBListener imp:
 * Static mozIStorageCompletionCallback used to notify when the database is
 * successfully closed.
 ******************************************************************************/
class CloseCookieDBListener :  public mozIStorageCompletionCallback
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Complete()
  {
    COOKIE_LOGSTRING(PR_LOG_DEBUG, ("Database closed"));

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs)
      obs->NotifyObservers(nsnull, "cookie-db-closed", nsnull);

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(CloseCookieDBListener, mozIStorageCompletionCallback)

/******************************************************************************
 * nsCookieService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/

nsICookieService*
nsCookieService::GetXPCOMSingleton()
{
#ifdef MOZ_IPC
  if (IsNeckoChild())
    return CookieServiceChild::GetSingleton();
#endif

  return GetSingleton();
}

nsCookieService*
nsCookieService::GetSingleton()
{
#ifdef MOZ_IPC
  NS_ASSERTION(!IsNeckoChild(), "not a parent process");
#endif

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

/******************************************************************************
 * nsCookieService impl:
 * public methods
 ******************************************************************************/

NS_IMPL_ISUPPORTS5(nsCookieService,
                   nsICookieService,
                   nsICookieManager,
                   nsICookieManager2,
                   nsIObserver,
                   nsISupportsWeakReference)

nsCookieService::nsCookieService()
 : mDBState(&mDefaultDBState)
 , mCookieBehavior(BEHAVIOR_ACCEPT)
 , mThirdPartySession(PR_FALSE)
 , mMaxNumberOfCookies(kMaxNumberOfCookies)
 , mMaxCookiesPerHost(kMaxCookiesPerHost)
 , mCookiePurgeAge(kCookiePurgeAge)
{
}

nsresult
nsCookieService::Init()
{
  NS_TIME_FUNCTION;

  if (!mDBState->hostTable.Init()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv;
  mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mIDNService = do_GetService(NS_IDNSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // init our pref and observer
  nsCOMPtr<nsIPrefBranch2> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    prefBranch->AddObserver(kPrefCookieBehavior,     this, PR_TRUE);
    prefBranch->AddObserver(kPrefMaxNumberOfCookies, this, PR_TRUE);
    prefBranch->AddObserver(kPrefMaxCookiesPerHost,  this, PR_TRUE);
    prefBranch->AddObserver(kPrefCookiePurgeAge,     this, PR_TRUE);
    prefBranch->AddObserver(kPrefThirdPartySession,  this, PR_TRUE);
    PrefChanged(prefBranch);
  }

  mStorageService = do_GetService("@mozilla.org/storage/service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // failure here is non-fatal (we can run fine without
  // persistent storage - e.g. if there's no profile)
  rv = InitDB();
  if (NS_FAILED(rv))
    COOKIE_LOGSTRING(PR_LOG_WARNING, ("Init(): InitDB() gave error %x", rv));

  mObserverService = mozilla::services::GetObserverService();
  if (mObserverService) {
    mObserverService->AddObserver(this, "profile-before-change", PR_TRUE);
    mObserverService->AddObserver(this, "profile-do-change", PR_TRUE);
    mObserverService->AddObserver(this, NS_PRIVATE_BROWSING_SWITCH_TOPIC, PR_TRUE);

    nsCOMPtr<nsIPrivateBrowsingService> pbs =
      do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
    if (pbs) {
      PRBool inPrivateBrowsing = PR_FALSE;
      pbs->GetPrivateBrowsingEnabled(&inPrivateBrowsing);
      if (inPrivateBrowsing) {
        Observe(nsnull, NS_PRIVATE_BROWSING_SWITCH_TOPIC,
                NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).get());
      }
    }
  }

  mPermissionService = do_GetService(NS_COOKIEPERMISSION_CONTRACTID);
  if (!mPermissionService) {
    NS_WARNING("nsICookiePermission implementation not available - some features won't work!");
    COOKIE_LOGSTRING(PR_LOG_WARNING, ("Init(): nsICookiePermission implementation not available"));
  }

  mInsertListener = new InsertCookieDBListener;
  mUpdateListener = new UpdateCookieDBListener;
  mRemoveListener = new RemoveCookieDBListener;
  mCloseListener = new CloseCookieDBListener;

  return NS_OK;
}

nsresult
nsCookieService::InitDB()
{
  NS_ASSERTION(mDBState == &mDefaultDBState, "not in default DB state");

  // attempt to open and read the database
  nsresult rv = TryInitDB(PR_FALSE);
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // database is corrupt - delete and try again
    COOKIE_LOGSTRING(PR_LOG_WARNING, ("InitDB(): db corrupt, trying again", rv));

    rv = TryInitDB(PR_TRUE);
  }

  if (NS_FAILED(rv)) {
    // reset our DB connection and statements
    CloseDB();
  }
  return rv;
}

nsresult
nsCookieService::TryInitDB(PRBool aDeleteExistingDB)
{
  // null out any existing connection, and clear the cookie table
  CloseDB();
  RemoveAllFromMemory();

  nsCOMPtr<nsIFile> cookieFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(cookieFile));
  if (NS_FAILED(rv)) return rv;

  cookieFile->AppendNative(NS_LITERAL_CSTRING(kCookieFileName));

  // remove an existing db, if we've been told to (i.e. it's corrupt)
  if (aDeleteExistingDB) {
    rv = cookieFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // open a connection to the cookie database, and only cache our connection
  // and statements upon success. The connection is opened unshared to eliminate
  // cache contention between the main and background threads.
  rv = mStorageService->OpenUnsharedDatabase(cookieFile,
    getter_AddRefs(mDBState->dbConn));
  NS_ENSURE_SUCCESS(rv, rv);

  // Grow cookie db in 512KB increments
  mDBState->dbConn->SetGrowthIncrement(512 * 1024, EmptyCString());

  PRBool tableExists = PR_FALSE;
  mDBState->dbConn->TableExists(NS_LITERAL_CSTRING("moz_cookies"), &tableExists);
  if (!tableExists) {
      rv = CreateTable();
      NS_ENSURE_SUCCESS(rv, rv);

  } else {
    // table already exists; check the schema version before reading
    PRInt32 dbSchemaVersion;
    rv = mDBState->dbConn->GetSchemaVersion(&dbSchemaVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    // Start a transaction for the whole migration block.
    mozStorageTransaction transaction(mDBState->dbConn, PR_TRUE);

    switch (dbSchemaVersion) {
    // upgrading.
    // every time you increment the database schema, you need to implement
    // the upgrading code from the previous version to the new one.
    case 1:
      {
        // Add the lastAccessed column to the table.
        rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD lastAccessed INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // Fall through to the next upgrade.

    case 2:
      {
        // Add the baseDomain column and index to the table.
        rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD baseDomain TEXT"));
        NS_ENSURE_SUCCESS(rv, rv);

        // Compute the baseDomains for the table. This must be done eagerly
        // otherwise we won't be able to synchronously read in individual
        // domains on demand.
        nsCOMPtr<mozIStorageStatement> select;
        rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT id, host FROM moz_cookies"), getter_AddRefs(select));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<mozIStorageStatement> update;
        rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
          "UPDATE moz_cookies SET baseDomain = :baseDomain WHERE id = :id"),
          getter_AddRefs(update));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCString baseDomain, host;
        PRBool hasResult;
        while (1) {
          rv = select->ExecuteStep(&hasResult);
          NS_ENSURE_SUCCESS(rv, rv);

          if (!hasResult)
            break;

          PRInt64 id = select->AsInt64(0);
          select->GetUTF8String(1, host);

          rv = GetBaseDomainFromHost(host, baseDomain);
          if (NS_FAILED(rv))
            continue;

          mozStorageStatementScoper scoper(update);

          rv = update->BindUTF8StringByName(NS_LITERAL_CSTRING("baseDomain"),
                                            baseDomain);
          NS_ASSERT_SUCCESS(rv);
          rv = update->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                                       id);
          NS_ASSERT_SUCCESS(rv);

          rv = update->ExecuteStep(&hasResult);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        // Create an index on baseDomain.
        rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain)"));
        NS_ENSURE_SUCCESS(rv, rv);
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
        nsCOMPtr<mozIStorageStatement> select;
        rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT id, name, host, path FROM moz_cookies "
            "ORDER BY name ASC, host ASC, path ASC, expiry ASC"),
          getter_AddRefs(select));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<mozIStorageStatement> deleteExpired;
        rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
          "DELETE FROM moz_cookies WHERE id = :id"),
          getter_AddRefs(deleteExpired));
        NS_ENSURE_SUCCESS(rv, rv);

        // Read the first row.
        PRBool hasResult;
        rv = select->ExecuteStep(&hasResult);
        NS_ENSURE_SUCCESS(rv, rv);

        if (hasResult) {
          nsCString name1, host1, path1;
          PRInt64 id1 = select->AsInt64(0);
          select->GetUTF8String(1, name1);
          select->GetUTF8String(2, host1);
          select->GetUTF8String(3, path1);

          nsCString name2, host2, path2;
          while (1) {
            // Read the second row.
            rv = select->ExecuteStep(&hasResult);
            NS_ENSURE_SUCCESS(rv, rv);

            if (!hasResult)
              break;

            PRInt64 id2 = select->AsInt64(0);
            select->GetUTF8String(1, name2);
            select->GetUTF8String(2, host2);
            select->GetUTF8String(3, path2);

            // If the two rows match in (name, host, path), we know the earlier
            // row has an earlier expiry time. Delete it.
            if (name1 == name2 && host1 == host2 && path1 == path2) {
              mozStorageStatementScoper scoper(deleteExpired);

              rv = deleteExpired->BindInt64ByName(NS_LITERAL_CSTRING("id"), id1);
              NS_ASSERT_SUCCESS(rv);

              rv = deleteExpired->ExecuteStep(&hasResult);
              NS_ENSURE_SUCCESS(rv, rv);
            }

            // Make the second row the first for the next iteration.
            name1 = name2;
            host1 = host2;
            path1 = path2;
            id1 = id2;
          }
        }

        // Add the creationTime column to the table.
        rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD creationTime INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);

        // Copy the id of each row into the new creationTime column.
        rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "UPDATE moz_cookies SET creationTime = "
            "(SELECT id WHERE id = moz_cookies.id)"));
        NS_ENSURE_SUCCESS(rv, rv);

        // Create a unique index on (name, host, path) to allow fast lookup.
        rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "CREATE UNIQUE INDEX moz_uniqueid "
          "ON moz_cookies (name, host, path)"));
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // Fall through to the next upgrade.

      // No more upgrades. Update the schema version.
      rv = mDBState->dbConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
      NS_ENSURE_SUCCESS(rv, rv);

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
        rv = mDBState->dbConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
        NS_ENSURE_SUCCESS(rv, rv);
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
        rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
          "SELECT "
            "id, "
            "baseDomain, "
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
        rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DROP TABLE moz_cookies"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = CreateTable();
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
    }
  }

  // make operations on the table asynchronous, for performance
  mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA synchronous = OFF"));

  // Use write-ahead-logging for performance. We cap the autocheckpoint limit at
  // 16 pages (around 500KB).
  mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA journal_mode = WAL"));
  mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA wal_autocheckpoint = 16"));

  // cache frequently used statements (for insertion, deletion, and updating)
  rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_cookies ("
      "baseDomain, "
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
    getter_AddRefs(mDBState->stmtInsert));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_cookies "
    "WHERE name = :name AND host = :host AND path = :path"),
    getter_AddRefs(mDBState->stmtDelete));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_cookies SET lastAccessed = :lastAccessed "
    "WHERE name = :name AND host = :host AND path = :path"),
    getter_AddRefs(mDBState->stmtUpdate));
  NS_ENSURE_SUCCESS(rv, rv);

  // if we deleted a corrupt db, don't attempt to import - return now
  if (aDeleteExistingDB)
    return NS_OK;

  // check whether to import or just read in the db
  if (tableExists)
    return Read();

  nsCOMPtr<nsIFile> oldCookieFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(oldCookieFile));
  if (NS_FAILED(rv)) return rv;

  oldCookieFile->AppendNative(NS_LITERAL_CSTRING(kOldCookieFileName));
  rv = ImportCookies(oldCookieFile);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_FILE_NOT_FOUND)
      return NS_OK;

    return rv;
  }

  // we're done importing - delete the old cookie file
  oldCookieFile->Remove(PR_FALSE);
  return NS_OK;
}

// Sets the schema version and creates the moz_cookies table.
nsresult
nsCookieService::CreateTable()
{
  // Set the schema version, before creating the table.
  nsresult rv = mDBState->dbConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
  if (NS_FAILED(rv)) return rv;

  // Create the table.
  rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE moz_cookies ("
      "id INTEGER PRIMARY KEY, "
      "baseDomain TEXT, "
      "name TEXT, "
      "value TEXT, "
      "host TEXT, "
      "path TEXT, "
      "expiry INTEGER, "
      "lastAccessed INTEGER, "
      "creationTime INTEGER, "
      "isSecure INTEGER, "
      "isHttpOnly INTEGER, "
      "CONSTRAINT moz_uniqueid UNIQUE (name, host, path)"
    ")"));
  if (NS_FAILED(rv)) return rv;

  // Create an index on baseDomain.
  return mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain)"));
}

void
nsCookieService::CloseDB()
{
  NS_ASSERTION(!mPrivateDBState.dbConn, "private DB connection should always be null");

  // finalize our statements and close the db connection for the default state.
  // since we own these objects, nulling the pointers is sufficient here.
  mDefaultDBState.stmtInsert = nsnull;
  mDefaultDBState.stmtDelete = nsnull;
  mDefaultDBState.stmtUpdate = nsnull;
  if (mDefaultDBState.dbConn) {
    // Cancel any pending read. No further results will be received by our
    // read listener.
    if (mDefaultDBState.pendingRead) {
      CancelAsyncRead(PR_TRUE);
      mDefaultDBState.syncConn = nsnull;
    }

    mDefaultDBState.dbConn->AsyncClose(mCloseListener);
    mDefaultDBState.dbConn = nsnull;
  }
}

nsCookieService::~nsCookieService()
{
  CloseDB();

  gCookieService = nsnull;
}

NS_IMETHODIMP
nsCookieService::Observe(nsISupports     *aSubject,
                         const char      *aTopic,
                         const PRUnichar *aData)
{
  // check the topic
  if (!strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.
    RemoveAllFromMemory();

    if (mDBState->dbConn) {
      if (!nsCRT::strcmp(aData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
        // clear the cookie file
        RemoveAll();
      }

      // Close the DB connection before changing
      CloseDB();
    }

  } else if (!strcmp(aTopic, "profile-do-change")) {
    // the profile has already changed; init the db from the new location.
    // if we are in the private browsing state, however, we do not want to read
    // data into it - we should instead put it into the default state, so it's
    // ready for us if and when we switch back to it.
    if (mDBState == &mPrivateDBState) {
      mDBState = &mDefaultDBState;
      InitDB();
      mDBState = &mPrivateDBState;
    } else {
      InitDB();
    }

  } else if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
    if (prefBranch)
      PrefChanged(prefBranch);

  } else if (!strcmp(aTopic, NS_PRIVATE_BROWSING_SWITCH_TOPIC)) {
    if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).Equals(aData)) {
      if (!mPrivateDBState.hostTable.IsInitialized() &&
          !mPrivateDBState.hostTable.Init())
        return NS_ERROR_OUT_OF_MEMORY;

      NS_ASSERTION(mDBState == &mDefaultDBState, "already in private state");
      NS_ASSERTION(mPrivateDBState.cookieCount == 0, "private count not 0");
      NS_ASSERTION(mPrivateDBState.cookieOldestTime == LL_MAXINT, "private time not reset");
      NS_ASSERTION(mPrivateDBState.hostTable.Count() == 0, "private table not empty");
      NS_ASSERTION(mPrivateDBState.dbConn == NULL, "private DB connection not null");

      // swap the private and default states
      mDBState = &mPrivateDBState;

      NotifyChanged(nsnull, NS_LITERAL_STRING("reload").get());

    } else if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).Equals(aData)) {
      // restore the default state, and clear the private one
      mDBState = &mDefaultDBState;

      NS_ASSERTION(!mPrivateDBState.dbConn, "private DB connection not null");

      mPrivateDBState.cookieCount = 0;
      mPrivateDBState.cookieOldestTime = LL_MAXINT;
      if (mPrivateDBState.hostTable.IsInitialized())
        mPrivateDBState.hostTable.Clear();

      NotifyChanged(nsnull, NS_LITERAL_STRING("reload").get());
    }
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
  PRBool isForeign = true;
  if (RequireThirdPartyCheck())
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isForeign);

  nsCAutoString result;
  GetCookieStringInternal(aHostURI, isForeign, aHttpBound, result);
  *aCookie = result.IsEmpty() ? nsnull : ToNewCString(result);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieString(nsIURI     *aHostURI,
                                 nsIPrompt  *aPrompt,
                                 const char *aCookieHeader,
                                 nsIChannel *aChannel)
{
  return SetCookieStringCommon(aHostURI, aCookieHeader, NULL, aChannel, false);
}

NS_IMETHODIMP
nsCookieService::SetCookieStringFromHttp(nsIURI     *aHostURI,
                                         nsIURI     *aFirstURI,
                                         nsIPrompt  *aPrompt,
                                         const char *aCookieHeader,
                                         const char *aServerTime,
                                         nsIChannel *aChannel) 
{
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
  PRBool isForeign = true;
  if (RequireThirdPartyCheck())
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, aHostURI, &isForeign);

  nsDependentCString cookieString(aCookieHeader);
  nsDependentCString serverTime(aServerTime ? aServerTime : "");
  SetCookieStringInternal(aHostURI, isForeign, cookieString,
                          serverTime, aFromHttp);
  return NS_OK;
}

void
nsCookieService::SetCookieStringInternal(nsIURI          *aHostURI,
                                         bool             aIsForeign,
                                         const nsCString &aCookieHeader,
                                         const nsCString &aServerTime,
                                         PRBool           aFromHttp) 
{
  NS_ASSERTION(aHostURI, "null host!");

  // get the base domain for the host URI.
  // e.g. for "www.bbc.co.uk", this would be "bbc.co.uk".
  // file:// URI's (i.e. with an empty host) are allowed, but any other
  // scheme must have a non-empty host. A trailing dot in the host
  // is acceptable, and will be stripped.
  PRBool requireHostMatch;
  nsCAutoString baseDomain;
  nsresult rv = GetBaseDomain(aHostURI, baseDomain, requireHostMatch);
  if (NS_FAILED(rv)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, 
                      "couldn't get base domain from URI");
    return;
  }

  // check default prefs
  CookieStatus cookieStatus = CheckPrefs(aHostURI, aIsForeign, baseDomain,
                                         requireHostMatch, aCookieHeader.get());
  // fire a notification if cookie was rejected (but not if there was an error)
  switch (cookieStatus) {
  case STATUS_REJECTED:
    NotifyRejected(aHostURI);
  case STATUS_REJECTED_WITH_ERROR:
    return;
  default:
    break;
  }

  // parse server local time. this is not just done here for efficiency
  // reasons - if there's an error parsing it, and we need to default it
  // to the current time, we must do it here since the current time in
  // SetCookieInternal() will change for each cookie processed (e.g. if the
  // user is prompted).
  PRTime tempServerTime;
  PRInt64 serverTime;
  PRStatus result = PR_ParseTimeString(aServerTime.get(), PR_TRUE,
                                       &tempServerTime);
  if (result == PR_SUCCESS) {
    serverTime = tempServerTime / PR_USEC_PER_SEC;
  } else {
    serverTime = PR_Now() / PR_USEC_PER_SEC;
  }

  // process each cookie in the header
  nsDependentCString cookieHeader(aCookieHeader);
  while (SetCookieInternal(aHostURI, baseDomain, requireHostMatch,
                           cookieStatus, cookieHeader, serverTime, aFromHttp));
}

// notify observers that a cookie was rejected due to the users' prefs.
void
nsCookieService::NotifyRejected(nsIURI *aHostURI)
{
  if (mObserverService)
    mObserverService->NotifyObservers(aHostURI, "cookie-rejected", nsnull);
}

// notify observers that the cookie list changed. there are five possible
// values for aData:
// "deleted" means a cookie was deleted. aSubject is the deleted cookie.
// "added"   means a cookie was added. aSubject is the added cookie.
// "changed" means a cookie was altered. aSubject is the new cookie.
// "cleared" means the entire cookie list was cleared. aSubject is null.
// "batch-deleted" means multiple cookies were deleted. aSubject is the list of
// cookies.
void
nsCookieService::NotifyChanged(nsISupports     *aSubject,
                               const PRUnichar *aData)
{
  if (mObserverService)
    mObserverService->NotifyObservers(aSubject, "cookie-changed", aData);
}

/******************************************************************************
 * nsCookieService:
 * pref observer impl
 ******************************************************************************/

void
nsCookieService::PrefChanged(nsIPrefBranch *aPrefBranch)
{
  PRInt32 val;
  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefCookieBehavior, &val)))
    mCookieBehavior = (PRUint8) LIMIT(val, 0, 2, 0);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefMaxNumberOfCookies, &val)))
    mMaxNumberOfCookies = (PRUint16) LIMIT(val, 1, 0xFFFF, kMaxNumberOfCookies);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefMaxCookiesPerHost, &val)))
    mMaxCookiesPerHost = (PRUint16) LIMIT(val, 1, 0xFFFF, kMaxCookiesPerHost);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefCookiePurgeAge, &val)))
    mCookiePurgeAge = LIMIT(val, 0, PR_INT32_MAX, PR_INT32_MAX) * PR_USEC_PER_SEC;

  PRBool boolval;
  if (NS_SUCCEEDED(aPrefBranch->GetBoolPref(kPrefThirdPartySession, &boolval)))
    mThirdPartySession = boolval;

  // Lazily instantiate the third party service if necessary.
  if (!mThirdPartyUtil && RequireThirdPartyCheck()) {
    mThirdPartyUtil = do_GetService(THIRDPARTYUTIL_CONTRACTID);
    NS_ABORT_IF_FALSE(mThirdPartyUtil, "require ThirdPartyUtil service");
  }
}

/******************************************************************************
 * nsICookieManager impl:
 * nsICookieManager
 ******************************************************************************/

NS_IMETHODIMP
nsCookieService::RemoveAll()
{
  RemoveAllFromMemory();

  // clear the cookie file
  if (mDBState->dbConn) {
    NS_ASSERTION(mDBState == &mDefaultDBState, "not in default DB state");

    // Cancel any pending read. No further results will be received by our
    // read listener.
    if (mDefaultDBState.pendingRead) {
      CancelAsyncRead(PR_TRUE);
      mDefaultDBState.syncConn = nsnull;
    }

    // XXX Ignore corruption for now. See bug 547031.
    nsCOMPtr<mozIStorageStatement> stmt;
    nsresult rv = mDefaultDBState.dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_cookies"), getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStoragePendingStatement> handle;
    rv = stmt->ExecuteAsync(mRemoveListener, getter_AddRefs(handle));
    NS_ASSERT_SUCCESS(rv);
  }

  NotifyChanged(nsnull, NS_LITERAL_STRING("cleared").get());
  return NS_OK;
}

static PLDHashOperator
COMArrayCallback(nsCookieEntry *aEntry,
                 void          *aArg)
{
  nsCOMArray<nsICookie> *data = static_cast<nsCOMArray<nsICookie> *>(aArg);

  const nsCookieEntry::ArrayType &cookies = aEntry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    data->AppendObject(cookies[i]);
  }

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsCookieService::GetEnumerator(nsISimpleEnumerator **aEnumerator)
{
  EnsureReadComplete();

  nsCOMArray<nsICookie> cookieList(mDBState->cookieCount);
  mDBState->hostTable.EnumerateEntries(COMArrayCallback, &cookieList);

  return NS_NewArrayEnumerator(aEnumerator, cookieList);
}

NS_IMETHODIMP
nsCookieService::Add(const nsACString &aHost,
                     const nsACString &aPath,
                     const nsACString &aName,
                     const nsACString &aValue,
                     PRBool            aIsSecure,
                     PRBool            aIsHttpOnly,
                     PRBool            aIsSession,
                     PRInt64           aExpiry)
{
  // first, normalize the hostname, and fail if it contains illegal characters.
  nsCAutoString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the base domain for the host URI.
  // e.g. for "www.bbc.co.uk", this would be "bbc.co.uk".
  nsCAutoString baseDomain;
  rv = GetBaseDomainFromHost(host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 currentTimeInUsec = PR_Now();

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

  AddInternal(baseDomain, cookie, currentTimeInUsec, nsnull, nsnull, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::Remove(const nsACString &aHost,
                        const nsACString &aName,
                        const nsACString &aPath,
                        PRBool           aBlocked)
{
  // first, normalize the hostname, and fail if it contains illegal characters.
  nsCAutoString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString baseDomain;
  rv = GetBaseDomainFromHost(host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsListIter matchIter;
  if (FindCookie(baseDomain,
                 host,
                 PromiseFlatCString(aName),
                 PromiseFlatCString(aPath),
                 matchIter)) {
    nsRefPtr<nsCookie> cookie = matchIter.Cookie();
    RemoveCookieFromList(matchIter);
    NotifyChanged(cookie, NS_LITERAL_STRING("deleted").get());
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

  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private file I/O functions
 ******************************************************************************/

nsresult
nsCookieService::Read()
{
  // Let the reading begin! Note that our query specifies that 'baseDomain'
  // not be NULL -- see below for why.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDefaultDBState.dbConn->CreateStatement(NS_LITERAL_CSTRING(
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
      "baseDomain "
    "FROM moz_cookies "
    "WHERE baseDomain NOTNULL"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<ReadCookieDBListener> readListener = new ReadCookieDBListener;
  rv = stmt->ExecuteAsync(readListener,
                          getter_AddRefs(mDefaultDBState.pendingRead));
  NS_ASSERT_SUCCESS(rv);

  mDefaultDBState.readListener = readListener;
  if (!mDefaultDBState.readSet.IsInitialized())
    mDefaultDBState.readSet.Init();

  // Queue up an operation to delete any rows with a NULL 'baseDomain'
  // column. This takes care of any cookies set by browsers that don't
  // understand the 'baseDomain' column, where the database schema version
  // is from one that does. (This would occur when downgrading.)
  rv = mDefaultDBState.dbConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_cookies WHERE baseDomain ISNULL"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStoragePendingStatement> handle;
  rv = stmt->ExecuteAsync(mRemoveListener, getter_AddRefs(handle));
  NS_ASSERT_SUCCESS(rv);

  return NS_OK;
}

// Extract data from a single result row and create an nsCookie.
// This is templated since 'T' is different for sync vs async results.
template<class T> nsCookie*
nsCookieService::GetCookieFromRow(T &aRow)
{
  // Skip reading 'baseDomain' -- up to the caller.
  nsCString name, value, host, path;
  nsresult rv = aRow->GetUTF8String(0, name);
  NS_ASSERT_SUCCESS(rv);
  rv = aRow->GetUTF8String(1, value);
  NS_ASSERT_SUCCESS(rv);
  rv = aRow->GetUTF8String(2, host);
  NS_ASSERT_SUCCESS(rv);
  rv = aRow->GetUTF8String(3, path);
  NS_ASSERT_SUCCESS(rv);

  PRInt64 expiry = aRow->AsInt64(4);
  PRInt64 lastAccessed = aRow->AsInt64(5);
  PRInt64 creationTime = aRow->AsInt64(6);
  PRBool isSecure = 0 != aRow->AsInt32(7);
  PRBool isHttpOnly = 0 != aRow->AsInt32(8);

  // Create a new nsCookie and assign the data.
  return nsCookie::Create(name, value, host, path,
                          expiry,
                          lastAccessed,
                          creationTime,
                          PR_FALSE,
                          isSecure,
                          isHttpOnly);
}

void
nsCookieService::AsyncReadComplete()
{
  // We may be in the private browsing DB state, with a pending read on the
  // default DB state. (This would occur if we started up in private browsing
  // mode.) As long as we do all our operations on the default state, we're OK.
  NS_ASSERTION(mDefaultDBState.pendingRead, "no pending read");
  NS_ASSERTION(mDefaultDBState.readListener, "no read listener");

  // Merge the data read on the background thread with the data synchronously
  // read on the main thread. Note that transactions on the cookie table may
  // have occurred on the main thread since, making the background data stale.
  for (PRUint32 i = 0; i < mDefaultDBState.hostArray.Length(); ++i) {
    const CookieDomainTuple &tuple = mDefaultDBState.hostArray[i];

    // Tiebreak: if the given base domain has already been read in, ignore
    // the background data. Note that readSet may contain domains that were
    // queried but found not to be in the db -- that's harmless.
    if (mDefaultDBState.readSet.GetEntry(tuple.baseDomain))
      continue;

    AddCookieToList(tuple.baseDomain, tuple.cookie, &mDefaultDBState, NULL,
      PR_FALSE);
  }

  mDefaultDBState.stmtReadDomain = nsnull;
  mDefaultDBState.pendingRead = nsnull;
  mDefaultDBState.readListener = nsnull;
  mDefaultDBState.syncConn = nsnull;
  mDefaultDBState.hostArray.Clear();
  mDefaultDBState.readSet.Clear();

  mObserverService->NotifyObservers(nsnull, "cookie-db-read", nsnull);

  COOKIE_LOGSTRING(PR_LOG_DEBUG, ("Read(): %ld cookies read",
                                  mDefaultDBState.cookieCount));
}

void
nsCookieService::CancelAsyncRead(PRBool aPurgeReadSet)
{
  // We may be in the private browsing DB state, with a pending read on the
  // default DB state. (This would occur if we started up in private browsing
  // mode.) As long as we do all our operations on the default state, we're OK.
  NS_ASSERTION(mDefaultDBState.pendingRead, "no pending read");
  NS_ASSERTION(mDefaultDBState.readListener, "no read listener");

  // Cancel the pending read, kill the read listener, and empty the array
  // of data already read in on the background thread.
  mDefaultDBState.readListener->Cancel();
  nsresult rv = mDefaultDBState.pendingRead->Cancel();
  NS_ASSERT_SUCCESS(rv);

  mDefaultDBState.stmtReadDomain = nsnull;
  mDefaultDBState.pendingRead = nsnull;
  mDefaultDBState.readListener = nsnull;
  mDefaultDBState.hostArray.Clear();

  // Only clear the 'readSet' table if we no longer need to know what set of
  // data is already accounted for.
  if (aPurgeReadSet)
    mDefaultDBState.readSet.Clear();
}

mozIStorageConnection*
nsCookieService::GetSyncDBConn()
{
  NS_ASSERTION(!mDefaultDBState.syncConn, "already have sync db connection");

  // Start a new connection for sync reads to reduce contention with the
  // background thread.
  nsCOMPtr<nsIFile> cookieFile;
  mDefaultDBState.dbConn->GetDatabaseFile(getter_AddRefs(cookieFile));
  NS_ASSERTION(cookieFile, "no cookie file on connection");

  mStorageService->OpenUnsharedDatabase(cookieFile,
    getter_AddRefs(mDefaultDBState.syncConn));
  if (!mDefaultDBState.syncConn) {
    NS_ERROR("can't open sync db connection");
    COOKIE_LOGSTRING(PR_LOG_DEBUG,
      ("GetSyncDBConn(): can't open sync db connection"));
  }

  return mDefaultDBState.syncConn;
}

void
nsCookieService::EnsureReadDomain(const nsCString &aBaseDomain)
{
  NS_ASSERTION(!mDBState->dbConn || mDBState == &mDefaultDBState,
    "not in default db state");

  // Fast path 1: nothing to read, or we've already finished reading.
  if (NS_LIKELY(!mDBState->dbConn || !mDefaultDBState.pendingRead))
    return;

  // Fast path 2: already read in this particular domain.
  if (NS_LIKELY(mDefaultDBState.readSet.GetEntry(aBaseDomain)))
    return;

  // Read in the data synchronously.
  nsresult rv;
  if (!mDefaultDBState.stmtReadDomain) {
    if (!GetSyncDBConn())
      return;

    // Cache the statement, since it's likely to be used again.
    rv = mDefaultDBState.syncConn->CreateStatement(NS_LITERAL_CSTRING(
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
      "WHERE baseDomain = ?1"),
      getter_AddRefs(mDefaultDBState.stmtReadDomain));

    // XXX Ignore corruption for now. See bug 547031.
    if (NS_FAILED(rv)) return;
  }

  NS_ASSERTION(mDefaultDBState.syncConn, "should have a sync db connection");

  mozStorageStatementScoper scoper(mDefaultDBState.stmtReadDomain);

  rv = mDefaultDBState.stmtReadDomain->BindUTF8StringByIndex(0, aBaseDomain);
  NS_ASSERT_SUCCESS(rv);

  PRBool hasResult;
  PRUint32 readCount = 0;
  nsCString name, value, host, path;
  while (1) {
    rv = mDefaultDBState.stmtReadDomain->ExecuteStep(&hasResult);
    // XXX Ignore corruption for now. See bug 547031.
    if (NS_FAILED(rv)) return;

    if (!hasResult)
      break;

    nsCookie* newCookie = GetCookieFromRow(mDefaultDBState.stmtReadDomain);
    AddCookieToList(aBaseDomain, newCookie, &mDefaultDBState, NULL, PR_FALSE);
    ++readCount;
  }

  // Add it to the hashset of read entries, so we don't read it again.
  mDefaultDBState.readSet.PutEntry(aBaseDomain);

  COOKIE_LOGSTRING(PR_LOG_DEBUG,
    ("EnsureReadDomain(): %ld cookies read for base domain %s",
     readCount, aBaseDomain.get()));
}

void
nsCookieService::EnsureReadComplete()
{
  NS_ASSERTION(!mDBState->dbConn || mDBState == &mDefaultDBState,
    "not in default db state");

  // Fast path 1: nothing to read, or we've already finished reading.
  if (NS_LIKELY(!mDBState->dbConn || !mDefaultDBState.pendingRead))
    return;

  // Cancel the pending read, so we don't get any more results.
  CancelAsyncRead(PR_FALSE);

  if (!mDefaultDBState.syncConn && !GetSyncDBConn())
    return;

  // Read in the data synchronously.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mDefaultDBState.syncConn->CreateStatement(NS_LITERAL_CSTRING(
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
      "baseDomain "
    "FROM moz_cookies"), getter_AddRefs(stmt));

  // XXX Ignore corruption for now. See bug 547031.
  if (NS_FAILED(rv)) return;

  nsCString baseDomain, name, value, host, path;
  PRBool hasResult;
  PRUint32 readCount = 0;
  while (1) {
    rv = stmt->ExecuteStep(&hasResult);
    // XXX Ignore corruption for now. See bug 547031.
    if (NS_FAILED(rv)) return;

    if (!hasResult)
      break;

    // Make sure we haven't already read the data.
    stmt->GetUTF8String(9, baseDomain);
    if (mDefaultDBState.readSet.GetEntry(baseDomain))
      continue;

    nsCookie* newCookie = GetCookieFromRow(stmt);
    AddCookieToList(baseDomain, newCookie, &mDefaultDBState, NULL, PR_FALSE);
    ++readCount;
  }

  mDefaultDBState.syncConn = nsnull;
  mDefaultDBState.readSet.Clear();

  mObserverService->NotifyObservers(nsnull, "cookie-db-read", nsnull);

  COOKIE_LOGSTRING(PR_LOG_DEBUG,
    ("EnsureReadComplete(): %ld cookies read", readCount));
}

NS_IMETHODIMP
nsCookieService::ImportCookies(nsIFile *aCookieFile)
{
  // Make sure we're in the default DB state. We don't want people importing
  // cookies into a private browsing session!
  if (mDBState != &mDefaultDBState) {
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

  nsCAutoString buffer, baseDomain;
  PRBool isMore = PR_TRUE;
  PRInt32 hostIndex, isDomainIndex, pathIndex, secureIndex, expiresIndex, nameIndex, cookieIndex;
  nsASingleFragmentCString::char_iterator iter;
  PRInt32 numInts;
  PRInt64 expires;
  PRBool isDomain, isHttpOnly = PR_FALSE;
  PRUint32 originalCookieCount = mDefaultDBState.cookieCount;

  PRInt64 currentTimeInUsec = PR_Now();
  PRInt64 currentTime = currentTimeInUsec / PR_USEC_PER_SEC;
  // we use lastAccessedCounter to keep cookies in recently-used order,
  // so we start by initializing to currentTime (somewhat arbitrary)
  PRInt64 lastAccessedCounter = currentTimeInUsec;

  /* file format is:
   *
   * host \t isDomain \t path \t secure \t expires \t name \t cookie
   *
   * if this format isn't respected we move onto the next line in the file.
   * isDomain is "TRUE" or "FALSE" (default to "FALSE")
   * isSecure is "TRUE" or "FALSE" (default to "TRUE")
   * expires is a PRInt64 integer
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

  // First, ensure we've read in everything from the database, if we have one.
  EnsureReadComplete();

  // We will likely be adding a bunch of cookies to the DB, so we use async
  // batching with storage to make this super fast.
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  if (originalCookieCount == 0 && mDefaultDBState.dbConn) {
    mDefaultDBState.stmtInsert->NewBindingParamsArray(getter_AddRefs(paramsArray));
  }

  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(buffer, &isMore))) {
    if (StringBeginsWith(buffer, NS_LITERAL_CSTRING(kHttpOnlyPrefix))) {
      isHttpOnly = PR_TRUE;
      hostIndex = sizeof(kHttpOnlyPrefix) - 1;
    } else if (buffer.IsEmpty() || buffer.First() == '#') {
      continue;
    } else {
      isHttpOnly = PR_FALSE;
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
        host.FindChar(':') != kNotFound) {
      continue;
    }

    // compute the baseDomain from the host
    rv = GetBaseDomainFromHost(host, baseDomain);
    if (NS_FAILED(rv))
      continue;

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
                       PR_FALSE,
                       Substring(buffer, secureIndex, expiresIndex - secureIndex - 1).EqualsLiteral(kTrue),
                       isHttpOnly);
    if (!newCookie) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    // trick: preserve the most-recently-used cookie ordering,
    // by successively decrementing the lastAccessed time
    lastAccessedCounter--;

    if (originalCookieCount == 0) {
      AddCookieToList(baseDomain, newCookie, &mDefaultDBState, paramsArray);
    }
    else {
      AddInternal(baseDomain, newCookie, currentTimeInUsec, NULL, NULL, PR_TRUE);
    }
  }

  // If we need to write to disk, do so now.
  if (paramsArray) {
    PRUint32 length;
    paramsArray->GetLength(&length);
    if (length) {
      rv = mDefaultDBState.stmtInsert->BindParameters(paramsArray);
      NS_ASSERT_SUCCESS(rv);
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = mDefaultDBState.stmtInsert->ExecuteAsync(mInsertListener,
                                              getter_AddRefs(handle));
      NS_ASSERT_SUCCESS(rv);
    }
  }


  COOKIE_LOGSTRING(PR_LOG_DEBUG, ("ImportCookies(): %ld cookies imported",
    mDefaultDBState.cookieCount));

  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private GetCookie/SetCookie helpers
 ******************************************************************************/

// helper function for GetCookieList
static inline PRBool ispathdelimiter(char c) { return c == '/' || c == '?' || c == '#' || c == ';'; }

// Comparator class for sorting cookies before sending to a server.
class CompareCookiesForSending
{
public:
  PRBool Equals(const nsCookie* aCookie1, const nsCookie* aCookie2) const
  {
    return aCookie1->CreationTime() == aCookie2->CreationTime() &&
           aCookie2->Path().Length() == aCookie1->Path().Length();
  }

  PRBool LessThan(const nsCookie* aCookie1, const nsCookie* aCookie2) const
  {
    // compare by cookie path length in accordance with RFC2109
    PRInt32 result = aCookie2->Path().Length() - aCookie1->Path().Length();
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
                                         PRBool aHttpBound,
                                         nsCString &aCookieString)
{
  NS_ASSERTION(aHostURI, "null host!");

  // get the base domain, host, and path from the URI.
  // e.g. for "www.bbc.co.uk", the base domain would be "bbc.co.uk".
  // file:// URI's (i.e. with an empty host) are allowed, but any other
  // scheme must have a non-empty host. A trailing dot in the host
  // is acceptable, and will be stripped.
  PRBool requireHostMatch;
  nsCAutoString baseDomain, hostFromURI, pathFromURI;
  nsresult rv = GetBaseDomain(aHostURI, baseDomain, requireHostMatch);
  if (NS_SUCCEEDED(rv))
    rv = aHostURI->GetAsciiHost(hostFromURI);
  if (NS_SUCCEEDED(rv))
    rv = aHostURI->GetPath(pathFromURI);
  // trim any trailing dot
  if (!hostFromURI.IsEmpty() && hostFromURI.Last() == '.')
    hostFromURI.Truncate(hostFromURI.Length() - 1);
  if (NS_FAILED(rv)) {
    COOKIE_LOGFAILURE(GET_COOKIE, aHostURI, nsnull, "invalid host/path from URI");
    return;
  }

  // check default prefs
  CookieStatus cookieStatus = CheckPrefs(aHostURI, aIsForeign, baseDomain,
                                         requireHostMatch, nsnull);
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
  PRBool isSecure;
  if (NS_FAILED(aHostURI->SchemeIs("https", &isSecure))) {
    isSecure = PR_FALSE;
  }

  nsCookie *cookie;
  nsAutoTArray<nsCookie*, 8> foundCookieList;
  PRInt64 currentTimeInUsec = PR_Now();
  PRInt64 currentTime = currentTimeInUsec / PR_USEC_PER_SEC;
  PRBool stale = PR_FALSE;

  EnsureReadDomain(baseDomain);

  // perform the hash lookup
  nsCookieEntry *entry = mDBState->hostTable.GetEntry(baseDomain);
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
    PRUint32 cookiePathLen = cookie->Path().Length();
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
    if (currentTimeInUsec - cookie->LastAccessed() > kCookieStaleThreshold)
      stale = PR_TRUE;
  }

  PRInt32 count = foundCookieList.Length();
  if (count == 0)
    return;

  // update lastAccessed timestamps. we only do this if the timestamp is stale
  // by a certain amount, to avoid thrashing the db during pageload.
  if (stale) {
    // Create an array of parameters to bind to our update statement. Batching
    // is OK here since we're updating cookies with no interleaved operations.
    nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
    mozIStorageStatement* stmt = mDBState->stmtUpdate;
    if (mDBState->dbConn) {
      stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
    }

    for (PRInt32 i = 0; i < count; ++i) {
      cookie = foundCookieList.ElementAt(i);

      if (currentTimeInUsec - cookie->LastAccessed() > kCookieStaleThreshold)
        UpdateCookieInList(cookie, currentTimeInUsec, paramsArray);
    }
    // Update the database now if necessary.
    if (paramsArray) {
      PRUint32 length;
      paramsArray->GetLength(&length);
      if (length) {
        nsresult rv = stmt->BindParameters(paramsArray);
        NS_ASSERT_SUCCESS(rv);
        nsCOMPtr<mozIStoragePendingStatement> handle;
        rv = stmt->ExecuteAsync(mUpdateListener, getter_AddRefs(handle));
        NS_ASSERT_SUCCESS(rv);
      }
    }
  }

  // return cookies in order of path length; longest to shortest.
  // this is required per RFC2109.  if cookies match in length,
  // then sort by creation time (see bug 236772).
  foundCookieList.Sort(CompareCookiesForSending());

  for (PRInt32 i = 0; i < count; ++i) {
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
    COOKIE_LOGSUCCESS(GET_COOKIE, aHostURI, aCookieString, nsnull, nsnull);
}

// processes a single cookie, and returns PR_TRUE if there are more cookies
// to be processed
PRBool
nsCookieService::SetCookieInternal(nsIURI                        *aHostURI,
                                   const nsCString               &aBaseDomain,
                                   PRBool                         aRequireHostMatch,
                                   CookieStatus                   aStatus,
                                   nsDependentCString            &aCookieHeader,
                                   PRInt64                        aServerTime,
                                   PRBool                         aFromHttp)
{
  NS_ASSERTION(aHostURI, "null host!");

  // create a stack-based nsCookieAttributes, to store all the
  // attributes parsed from the cookie
  nsCookieAttributes cookieAttributes;

  // init expiryTime such that session cookies won't prematurely expire
  cookieAttributes.expiryTime = LL_MAXINT;

  // aCookieHeader is an in/out param to point to the next cookie, if
  // there is one. Save the present value for logging purposes
  nsDependentCString savedCookieHeader(aCookieHeader);

  // newCookie says whether there are multiple cookies in the header;
  // so we can handle them separately.
  PRBool newCookie = ParseAttributes(aCookieHeader, cookieAttributes);

  PRInt64 currentTimeInUsec = PR_Now();

  // calculate expiry time of cookie.
  cookieAttributes.isSession = GetExpiry(cookieAttributes, aServerTime,
                                         currentTimeInUsec / PR_USEC_PER_SEC);
  if (aStatus == STATUS_ACCEPT_SESSION) {
    // force lifetime to session. note that the expiration time, if set above,
    // will still apply.
    cookieAttributes.isSession = PR_TRUE;
  }

  // reject cookie if it's over the size limit, per RFC2109
  if ((cookieAttributes.name.Length() + cookieAttributes.value.Length()) > kMaxBytesPerCookie) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "cookie too big (> 4kb)");
    return newCookie;
  }

  if (cookieAttributes.name.FindChar('\t') != kNotFound) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "invalid name character");
    return newCookie;
  }

  // domain & path checks
  if (!CheckDomain(cookieAttributes, aHostURI, aBaseDomain, aRequireHostMatch)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "failed the domain tests");
    return newCookie;
  }
  if (!CheckPath(cookieAttributes, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, savedCookieHeader, "failed the path tests");
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
    PRBool permission;
    // Not passing an nsIChannel here means CanSetCookie will use the currently
    // active window to display the prompt. This isn't exactly ideal, but this
    // code is going away. See bug 546746.
    mPermissionService->CanSetCookie(aHostURI,
                                     nsnull,
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
  AddInternal(aBaseDomain, cookie, PR_Now(), aHostURI, savedCookieHeader.get(),
              aFromHttp);
  return newCookie;
}

// this is a backend function for adding a cookie to the list, via SetCookie.
// also used in the cookie manager, for profile migration from IE.
// it either replaces an existing cookie; or adds the cookie to the hashtable,
// and deletes a cookie (if maximum number of cookies has been
// reached). also performs list maintenance by removing expired cookies.
void
nsCookieService::AddInternal(const nsCString               &aBaseDomain,
                             nsCookie                      *aCookie,
                             PRInt64                        aCurrentTimeInUsec,
                             nsIURI                        *aHostURI,
                             const char                    *aCookieHeader,
                             PRBool                         aFromHttp)
{
  PRInt64 currentTime = aCurrentTimeInUsec / PR_USEC_PER_SEC;

  // if the new cookie is httponly, make sure we're not coming from script
  if (!aFromHttp && aCookie->IsHttpOnly()) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
      "cookie is httponly; coming from script");
    return;
  }

  nsListIter matchIter;
  PRBool foundCookie = FindCookie(aBaseDomain, aCookie->Host(),
    aCookie->Name(), aCookie->Path(), matchIter);

  nsRefPtr<nsCookie> oldCookie;
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

      // Remove the stale cookie and notify.
      RemoveCookieFromList(matchIter);

      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
        "stale cookie was deleted");
      NotifyChanged(oldCookie, NS_LITERAL_STRING("deleted").get());

      // We've done all we need to wrt removing and notifying the stale cookie.
      // From here on out, we pretend pretend it didn't exist, so that we
      // preserve expected notification semantics when adding the new cookie.
      foundCookie = PR_FALSE;

    } else {
      // If the old cookie is httponly, make sure we're not coming from script.
      if (!aFromHttp && oldCookie->IsHttpOnly()) {
        COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
          "previously stored cookie is httponly; coming from script");
        return;
      }

      // Remove the old cookie and notify.
      RemoveCookieFromList(matchIter);

      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader,
        "previously stored cookie was deleted");
      NotifyChanged(oldCookie, NS_LITERAL_STRING("deleted").get());

      // If the new cookie has expired -- i.e. the intent was simply to delete
      // the old cookie -- then we're done.
      if (aCookie->Expiry() <= currentTime) {
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
    nsCookieEntry *entry = mDBState->hostTable.GetEntry(aBaseDomain);
    if (entry && entry->GetCookies().Length() >= mMaxCookiesPerHost) {
      nsListIter iter;
      FindStaleCookie(entry, currentTime, iter);

      // remove the oldest cookie from the domain
      COOKIE_LOGEVICTED(iter.Cookie(), "Too many cookies for this domain");
      RemoveCookieFromList(iter);

      NotifyChanged(iter.Cookie(), NS_LITERAL_STRING("deleted").get());

    } else if (mDBState->cookieCount >= ADD_TEN_PERCENT(mMaxNumberOfCookies)) {
      PRInt64 maxAge = aCurrentTimeInUsec - mDBState->cookieOldestTime;
      PRInt64 purgeAge = ADD_TEN_PERCENT(mCookiePurgeAge);
      if (maxAge >= purgeAge) {
        // we're over both size and age limits by 10%; time to purge the table!
        // do this by:
        // 1) removing expired cookies;
        // 2) evicting the balance of old cookies until we reach the size limit.
        // note that the cookieOldestTime indicator can be pessimistic - if it's
        // older than the actual oldest cookie, we'll just purge more eagerly.
        PurgeCookies(aCurrentTimeInUsec);
      }
    }
  }

  // Add the cookie to the db. We do not supply a params array for batching
  // because this might result in removals and additions being out of order.
  AddCookieToList(aBaseDomain, aCookie, mDBState, NULL);
  NotifyChanged(aCookie, foundCookie ? NS_LITERAL_STRING("changed").get()
                                     : NS_LITERAL_STRING("added").get());

  COOKIE_LOGSUCCESS(SET_COOKIE, aHostURI, aCookieHeader, aCookie, foundCookie);
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
static inline PRBool iswhitespace     (char c) { return c == ' '  || c == '\t'; }
static inline PRBool isterminator     (char c) { return c == '\n' || c == '\r'; }
static inline PRBool isvalueseparator (char c) { return isterminator(c) || c == ';'; }
static inline PRBool istokenseparator (char c) { return isvalueseparator(c) || c == '='; }

// Parse a single token/value pair.
// Returns PR_TRUE if a cookie terminator is found, so caller can parse new cookie.
PRBool
nsCookieService::GetTokenValue(nsASingleFragmentCString::const_char_iterator &aIter,
                               nsASingleFragmentCString::const_char_iterator &aEndIter,
                               nsDependentCSubstring                         &aTokenString,
                               nsDependentCSubstring                         &aTokenValue,
                               PRBool                                        &aEqualsFound)
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
    while (--lastSpace != start && iswhitespace(*lastSpace));
    ++lastSpace;
  }
  aTokenString.Rebind(start, lastSpace);

  aEqualsFound = (*aIter == '=');
  if (aEqualsFound) {
    // find <value>
    while (++aIter != aEndIter && iswhitespace(*aIter));

    start = aIter;

    // process <token>
    // just look for ';' to terminate ('=' allowed)
    while (aIter != aEndIter && !isvalueseparator(*aIter))
      ++aIter;

    // remove trailing <LWS>; first check we're not at the beginning
    if (aIter != start) {
      lastSpace = aIter;
      while (--lastSpace != start && iswhitespace(*lastSpace));
      aTokenValue.Rebind(start, ++lastSpace);
    }
  }

  // aIter is on ';', or terminator, or EOS
  if (aIter != aEndIter) {
    // if on terminator, increment past & return PR_TRUE to process new cookie
    if (isterminator(*aIter)) {
      ++aIter;
      return PR_TRUE;
    }
    // fall-through: aIter is on ';', increment and return PR_FALSE
    ++aIter;
  }
  return PR_FALSE;
}

// Parses attributes from cookie header. expires/max-age attributes aren't folded into the
// cookie struct here, because we don't know which one to use until we've parsed the header.
PRBool
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

  aCookieAttributes.isSecure = PR_FALSE;
  aCookieAttributes.isHttpOnly = PR_FALSE;
  
  nsDependentCSubstring tokenString(cookieStart, cookieStart);
  nsDependentCSubstring tokenValue (cookieStart, cookieStart);
  PRBool newCookie, equalsFound;

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
      aCookieAttributes.isSecure = PR_TRUE;
      
    // ignore any tokenValue for isHttpOnly (see bug 178993);
    // just set the boolean
    else if (tokenString.LowerCaseEqualsLiteral(kHttpOnly))
      aCookieAttributes.isHttpOnly = PR_TRUE;
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
// dot may be present (and will be stripped). If aHostURI is an IP address,
// an alias such as 'localhost', an eTLD such as 'co.uk', or the empty string,
// aBaseDomain will be the exact host, and aRequireHostMatch will be true to
// indicate that substring matches should not be performed.
nsresult
nsCookieService::GetBaseDomain(nsIURI    *aHostURI,
                               nsCString &aBaseDomain,
                               PRBool    &aRequireHostMatch)
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

  // aHost (and thus aBaseDomain) may contain a trailing dot; if so, trim it.
  if (!aBaseDomain.IsEmpty() && aBaseDomain.Last() == '.')
    aBaseDomain.Truncate(aBaseDomain.Length() - 1);

  // block any URIs without a host that aren't file:// URIs.
  if (aBaseDomain.IsEmpty()) {
    PRBool isFileURI = PR_FALSE;
    aHostURI->SchemeIs("file", &isFileURI);
    if (!isFileURI)
      return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

// Get the base domain for aHost; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". This is done differently than GetBaseDomain(): it is assumed
// that aHost is already normalized, and it may contain a leading dot
// (indicating that it represents a domain). A trailing dot must not be present.
// If aHost is an IP address, an alias such as 'localhost', an eTLD such as
// 'co.uk', or the empty string, aBaseDomain will be the exact host, and a
// leading dot will be treated as an error.
nsresult
nsCookieService::GetBaseDomainFromHost(const nsACString &aHost,
                                       nsCString        &aBaseDomain)
{
  // aHost must not contain a trailing dot, or be the string '.'.
  if (!aHost.IsEmpty() && aHost.Last() == '.')
    return NS_ERROR_INVALID_ARG;

  // aHost may contain a leading dot; if so, strip it now.
  nsDependentCString host(aHost);
  PRBool domain = !host.IsEmpty() && host.First() == '.';
  if (domain)
    host.Rebind(host.BeginReading() + 1, host.EndReading());

  // get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = mTLDService->GetBaseDomainFromHost(host, 0, aBaseDomain);
  if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
    // aHost is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. use the host as a key in such
    // cases; however, we reject any such hosts with a leading dot, since it
    // doesn't make sense for them to be domain cookies.
    if (domain)
      return NS_ERROR_INVALID_ARG;

    aBaseDomain = host;
    return NS_OK;
  }
  return rv;
}

// Normalizes the given hostname, component by component. ASCII/ACE
// components are lower-cased, and UTF-8 components are normalized per
// RFC 3454 and converted to ACE. Any trailing dot is stripped.
nsresult
nsCookieService::NormalizeHost(nsCString &aHost)
{
  if (!IsASCII(aHost)) {
    nsCAutoString host;
    nsresult rv = mIDNService->ConvertUTF8toACE(aHost, host);
    if (NS_FAILED(rv))
      return rv;

    aHost = host;
  }

  // Only strip the trailing dot if it wouldn't result in the empty string;
  // in that case, treat it like a leading dot.
  if (aHost.Length() > 1 && aHost.Last() == '.')
    aHost.Truncate(aHost.Length() - 1);

  ToLowerCase(aHost);
  return NS_OK;
}

// returns PR_TRUE if 'a' is equal to or a subdomain of 'b',
// assuming no leading or trailing dots are present.
static inline PRBool IsSubdomainOf(const nsCString &a, const nsCString &b)
{
  if (a == b)
    return PR_TRUE;
  if (a.Length() > b.Length())
    return a[a.Length() - b.Length() - 1] == '.' && StringEndsWith(a, b);
  return PR_FALSE;
}

bool
nsCookieService::RequireThirdPartyCheck()
{
  // 'true' iff we need to perform a third party test.
  return mCookieBehavior == BEHAVIOR_REJECTFOREIGN || mThirdPartySession;
}

CookieStatus
nsCookieService::CheckPrefs(nsIURI          *aHostURI,
                            bool             aIsForeign,
                            const nsCString &aBaseDomain,
                            PRBool           aRequireHostMatch,
                            const char      *aCookieHeader)
{
  nsresult rv;

  // don't let ftp sites get/set cookies (could be a security issue)
  PRBool ftp;
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
    rv = mPermissionService->CanAccess(aHostURI, nsnull, &access);

    // if we found an entry, use it
    if (NS_SUCCEEDED(rv)) {
      switch (access) {
      case nsICookiePermission::ACCESS_DENY:
        COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "cookies are blocked for this site");
        return STATUS_REJECTED;

      case nsICookiePermission::ACCESS_ALLOW:
        return STATUS_ACCEPTED;
      }
    }
  }

  // check default prefs
  if (mCookieBehavior == BEHAVIOR_REJECT) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "cookies are disabled");
    return STATUS_REJECTED;
  }

  if (RequireThirdPartyCheck() && aIsForeign) {
    // check if cookie is foreign
    if (mCookieBehavior == BEHAVIOR_ACCEPT && mThirdPartySession)
      return STATUS_ACCEPT_SESSION;

    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "context is third party");
    return STATUS_REJECTED;
  }

  // if nothing has complained, accept cookie
  return STATUS_ACCEPTED;
}

// processes domain attribute, and returns PR_TRUE if host has permission to set for this domain.
PRBool
nsCookieService::CheckDomain(nsCookieAttributes &aCookieAttributes,
                             nsIURI             *aHostURI,
                             const nsCString    &aBaseDomain,
                             PRBool              aRequireHostMatch)
{
  // get host from aHostURI
  nsCAutoString hostFromURI;
  aHostURI->GetAsciiHost(hostFromURI);

  // trim any trailing dot
  if (!hostFromURI.IsEmpty() && hostFromURI.Last() == '.')
    hostFromURI.Truncate(hostFromURI.Length() - 1);

  // if a domain is given, check the host has permission
  if (!aCookieAttributes.host.IsEmpty()) {
    // Tolerate leading '.' characters.
    if (aCookieAttributes.host.First() == '.')
      aCookieAttributes.host.Cut(0, 1);

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
      return PR_TRUE;
    }

    /*
     * note: RFC2109 section 4.3.2 requires that we check the following:
     * that the portion of host not in domain does not contain a dot.
     * this prevents hosts of the form x.y.co.nz from setting cookies in the
     * entire .co.nz domain. however, it's only a only a partial solution and
     * it breaks sites (IE doesn't enforce it), so we don't perform this check.
     */
    return PR_FALSE;
  }

  // no domain specified, use hostFromURI
  aCookieAttributes.host = hostFromURI;
  return PR_TRUE;
}

PRBool
nsCookieService::CheckPath(nsCookieAttributes &aCookieAttributes,
                           nsIURI             *aHostURI)
{
  // if a path is given, check the host has permission
  if (aCookieAttributes.path.IsEmpty()) {
    // strip down everything after the last slash to get the path,
    // ignoring slashes in the query string part.
    // if we can QI to nsIURL, that'll take care of the query string portion.
    // otherwise, it's not an nsIURL and can't have a query string, so just find the last slash.
    nsCOMPtr<nsIURL> hostURL = do_QueryInterface(aHostURI);
    if (hostURL) {
      hostURL->GetDirectory(aCookieAttributes.path);
    } else {
      aHostURI->GetPath(aCookieAttributes.path);
      PRInt32 slash = aCookieAttributes.path.RFindChar('/');
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
    nsCAutoString pathFromURI;
    if (NS_FAILED(aHostURI->GetPath(pathFromURI)) ||
        !StringBeginsWith(pathFromURI, aCookieAttributes.path)) {
      return PR_FALSE;
    }
#endif
  }

  if (aCookieAttributes.path.Length() > kMaxBytesPerPath ||
      aCookieAttributes.path.FindChar('\t') != kNotFound )
    return PR_FALSE;

  return PR_TRUE;
}

PRBool
nsCookieService::GetExpiry(nsCookieAttributes &aCookieAttributes,
                           PRInt64             aServerTime,
                           PRInt64             aCurrentTime)
{
  /* Determine when the cookie should expire. This is done by taking the difference between 
   * the server time and the time the server wants the cookie to expire, and adding that 
   * difference to the client time. This localizes the client time regardless of whether or
   * not the TZ environment variable was set on the client.
   *
   * Note: We need to consider accounting for network lag here, per RFC.
   */
  PRInt64 delta;

  // check for max-age attribute first; this overrides expires attribute
  if (!aCookieAttributes.maxage.IsEmpty()) {
    // obtain numeric value of maxageAttribute
    PRInt64 maxage;
    PRInt32 numInts = PR_sscanf(aCookieAttributes.maxage.get(), "%lld", &maxage);

    // default to session cookie if the conversion failed
    if (numInts != 1) {
      return PR_TRUE;
    }

    delta = maxage;

  // check for expires attribute
  } else if (!aCookieAttributes.expires.IsEmpty()) {
    PRTime expires;

    // parse expiry time
    if (PR_ParseTimeString(aCookieAttributes.expires.get(), PR_TRUE, &expires) != PR_SUCCESS) {
      return PR_TRUE;
    }

    delta = expires / PR_USEC_PER_SEC - aServerTime;

  // default to session cookie if no attributes found
  } else {
    return PR_TRUE;
  }

  // if this addition overflows, expiryTime will be less than currentTime
  // and the cookie will be expired - that's okay.
  aCookieAttributes.expiryTime = aCurrentTime + delta;

  return PR_FALSE;
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
  mDBState->cookieOldestTime = LL_MAXINT;
}

// stores temporary data for enumerating over the hash entries,
// since enumeration is done using callback functions
struct nsPurgeData
{
  typedef nsTArray<nsListIter> ArrayType;

  nsPurgeData(PRInt64 aCurrentTime,
              PRInt64 aPurgeTime,
              ArrayType &aPurgeList,
              nsIMutableArray *aRemovedList,
              mozIStorageBindingParamsArray *aParamsArray)
   : currentTime(aCurrentTime)
   , purgeTime(aPurgeTime)
   , oldestTime(LL_MAXINT)
   , purgeList(aPurgeList)
   , removedList(aRemovedList)
   , paramsArray(aParamsArray)
  {
  }

  // the current time, in seconds
  PRInt64 currentTime;

  // lastAccessed time older than which cookies are eligible for purge
  PRInt64 purgeTime;

  // lastAccessed time of the oldest cookie found during purge, to update our indicator
  PRInt64 oldestTime;

  // list of cookies over the age limit, for purging
  ArrayType &purgeList;

  // list of all cookies we've removed, for notification
  nsIMutableArray *removedList;

  // The array of parameters to be bound to the statement for deletion later.
  mozIStorageBindingParamsArray *paramsArray;
};

// comparator class for lastaccessed times of cookies.
class CompareCookiesByAge {
public:
  PRBool Equals(const nsListIter &a, const nsListIter &b) const
  {
    return a.Cookie()->LastAccessed() == b.Cookie()->LastAccessed() &&
           a.Cookie()->CreationTime() == b.Cookie()->CreationTime();
  }

  PRBool LessThan(const nsListIter &a, const nsListIter &b) const
  {
    // compare by lastAccessed time, and tiebreak by creationTime.
    PRInt64 result = a.Cookie()->LastAccessed() - b.Cookie()->LastAccessed();
    if (result != 0)
      return result < 0;

    return a.Cookie()->CreationTime() < b.Cookie()->CreationTime();
  }
};

// comparator class for sorting cookies by entry and index.
class CompareCookiesByIndex {
public:
  PRBool Equals(const nsListIter &a, const nsListIter &b) const
  {
    return PR_FALSE;
  }

  PRBool LessThan(const nsListIter &a, const nsListIter &b) const
  {
    // compare by entryclass pointer, then by index.
    if (a.entry != b.entry)
      return a.entry < b.entry;

    return a.index < b.index;
  }
};

PLDHashOperator
purgeCookiesCallback(nsCookieEntry *aEntry,
                     void          *aArg)
{
  nsPurgeData &data = *static_cast<nsPurgeData*>(aArg);

  const nsCookieEntry::ArrayType &cookies = aEntry->GetCookies();
  mozIStorageBindingParamsArray *array = data.paramsArray;
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ) {
    nsListIter iter(aEntry, i);
    nsCookie *cookie = cookies[i];

    // check if the cookie has expired
    if (cookie->Expiry() <= data.currentTime) {
      data.removedList->AppendElement(cookie, PR_FALSE);
      COOKIE_LOGEVICTED(cookie, "Cookie expired");

      // remove from list; do not increment our iterator
      gCookieService->RemoveCookieFromList(iter, array);

    } else {
      // check if the cookie is over the age limit
      if (cookie->LastAccessed() <= data.purgeTime) {
        data.purgeList.AppendElement(iter);

      } else if (cookie->LastAccessed() < data.oldestTime) {
        // reset our indicator
        data.oldestTime = cookie->LastAccessed();
      }

      ++i;
    }
  }
  return PL_DHASH_NEXT;
}

// purges expired and old cookies in a batch operation.
void
nsCookieService::PurgeCookies(PRInt64 aCurrentTimeInUsec)
{
  NS_ASSERTION(mDBState->hostTable.Count() > 0, "table is empty");
#ifdef PR_LOGGING
  PRUint32 initialCookieCount = mDBState->cookieCount;
  COOKIE_LOGSTRING(PR_LOG_DEBUG,
    ("PurgeCookies(): beginning purge with %ld cookies and %lld oldest age",
     mDBState->cookieCount, aCurrentTimeInUsec - mDBState->cookieOldestTime));
#endif

  nsAutoTArray<nsListIter, kMaxNumberOfCookies> purgeList;

  nsCOMPtr<nsIMutableArray> removedList = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!removedList)
    return;

  // Create a params array to batch the removals. This is OK here because
  // all the removals are in order, and there are no interleaved additions.
  mozIStorageStatement *stmt = mDBState->stmtDelete;
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  if (mDBState->dbConn) {
    stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  }

  EnsureReadComplete();

  nsPurgeData data(aCurrentTimeInUsec / PR_USEC_PER_SEC,
    aCurrentTimeInUsec - mCookiePurgeAge, purgeList, removedList, paramsArray);
  mDBState->hostTable.EnumerateEntries(purgeCookiesCallback, &data);

#ifdef PR_LOGGING
  PRUint32 postExpiryCookieCount = mDBState->cookieCount;
#endif

  // now we have a list of iterators for cookies over the age limit.
  // sort them by age, and then we'll see how many to remove...
  purgeList.Sort(CompareCookiesByAge());

  // only remove old cookies until we reach the max cookie limit, no more.
  PRUint32 excess = mDBState->cookieCount - mMaxNumberOfCookies;
  if (purgeList.Length() > excess) {
    // we're not purging everything in the list, so update our indicator
    data.oldestTime = purgeList[excess].Cookie()->LastAccessed();

    purgeList.SetLength(excess);
  }

  // sort the list again, this time grouping cookies with a common entryclass
  // together, and with ascending index. this allows us to iterate backwards
  // over the list removing cookies, without having to adjust indexes as we go.
  purgeList.Sort(CompareCookiesByIndex());
  for (nsPurgeData::ArrayType::index_type i = purgeList.Length(); i--; ) {
    nsCookie *cookie = purgeList[i].Cookie();
    removedList->AppendElement(cookie, PR_FALSE);
    COOKIE_LOGEVICTED(cookie, "Cookie too old");

    RemoveCookieFromList(purgeList[i], paramsArray);
  }

  // Update the database if we have entries to purge.
  if (paramsArray) {
    PRUint32 length;
    paramsArray->GetLength(&length);
    if (length) {
      nsresult rv = stmt->BindParameters(paramsArray);
      NS_ASSERT_SUCCESS(rv);
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(mRemoveListener, getter_AddRefs(handle));
      NS_ASSERT_SUCCESS(rv);
    }
  }

  // take all the cookies in the removed list, and notify about them in one batch
  NotifyChanged(removedList, NS_LITERAL_STRING("batch-deleted").get());

  // reset the oldest time indicator
  mDBState->cookieOldestTime = data.oldestTime;

  COOKIE_LOGSTRING(PR_LOG_DEBUG,
    ("PurgeCookies(): %ld expired; %ld purged; %ld remain; %lld oldest age",
     initialCookieCount - postExpiryCookieCount,
     postExpiryCookieCount - mDBState->cookieCount,
     mDBState->cookieCount,
     aCurrentTimeInUsec - mDBState->cookieOldestTime));
}

// find whether a given cookie has been previously set. this is provided by the
// nsICookieManager2 interface.
NS_IMETHODIMP
nsCookieService::CookieExists(nsICookie2 *aCookie,
                              PRBool     *aFoundCookie)
{
  NS_ENSURE_ARG_POINTER(aCookie);

  nsCAutoString host, name, path;
  nsresult rv = aCookie->GetHost(host);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCookie->GetName(name);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aCookie->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString baseDomain;
  rv = GetBaseDomainFromHost(host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsListIter iter;
  *aFoundCookie = FindCookie(baseDomain, host, name, path, iter);
  return NS_OK;
}

// For a given base domain, find either an expired cookie or the oldest cookie
// by lastAccessed time.
void
nsCookieService::FindStaleCookie(nsCookieEntry *aEntry,
                                 PRInt64 aCurrentTime,
                                 nsListIter &aIter)
{
  aIter.entry = NULL;

  PRInt64 oldestTime;
  const nsCookieEntry::ArrayType &cookies = aEntry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    nsCookie *cookie = cookies[i];

    // If we found an expired cookie, we're done.
    if (cookie->Expiry() > aCurrentTime) {
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
                                      PRUint32         *aCountFromHost)
{
  // first, normalize the hostname, and fail if it contains illegal characters.
  nsCAutoString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString baseDomain;
  rv = GetBaseDomainFromHost(host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  EnsureReadDomain(baseDomain);

  // Return a count of all cookies, including expired.
  nsCookieEntry *entry = mDBState->hostTable.GetEntry(baseDomain);
  *aCountFromHost = entry ? entry->GetCookies().Length() : 0;
  return NS_OK;
}

// get an enumerator of cookies stored by a particular host. this is provided by the
// nsICookieManager2 interface.
NS_IMETHODIMP
nsCookieService::GetCookiesFromHost(const nsACString     &aHost,
                                    nsISimpleEnumerator **aEnumerator)
{
  // first, normalize the hostname, and fail if it contains illegal characters.
  nsCAutoString host(aHost);
  nsresult rv = NormalizeHost(host);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString baseDomain;
  rv = GetBaseDomainFromHost(host, baseDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  EnsureReadDomain(baseDomain);

  nsCookieEntry *entry = mDBState->hostTable.GetEntry(baseDomain);
  if (!entry)
    return NS_NewEmptyEnumerator(aEnumerator);

  nsCOMArray<nsICookie> cookieList(mMaxCookiesPerHost);
  const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    cookieList.AppendObject(cookies[i]);
  }

  return NS_NewArrayEnumerator(aEnumerator, cookieList);
}

// find an exact cookie specified by host, name, and path that hasn't expired.
PRBool
nsCookieService::FindCookie(const nsCString      &aBaseDomain,
                            const nsAFlatCString &aHost,
                            const nsAFlatCString &aName,
                            const nsAFlatCString &aPath,
                            nsListIter           &aIter)
{
  EnsureReadDomain(aBaseDomain);

  nsCookieEntry *entry = mDBState->hostTable.GetEntry(aBaseDomain);
  if (!entry)
    return PR_FALSE;

  const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    nsCookie *cookie = cookies[i];

    if (aHost.Equals(cookie->Host()) &&
        aPath.Equals(cookie->Path()) &&
        aName.Equals(cookie->Name())) {
      aIter = nsListIter(entry, i);
      return PR_TRUE;
    }
  }

  return PR_FALSE;
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
    mozIStorageStatement *stmt = mDBState->stmtDelete;
    nsCOMPtr<mozIStorageBindingParamsArray> paramsArray(aParamsArray);
    if (!paramsArray) {
      stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
    }

    nsCOMPtr<mozIStorageBindingParams> params;
    paramsArray->NewBindingParams(getter_AddRefs(params));

    nsresult rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("name"),
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
      rv = stmt->ExecuteAsync(mRemoveListener, getter_AddRefs(handle));
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

static void
bindCookieParameters(mozIStorageBindingParamsArray *aParamsArray,
                     const nsCString &aBaseDomain,
                     const nsCookie *aCookie)
{
  NS_ASSERTION(aParamsArray, "Null params array passed to bindCookieParameters!");
  NS_ASSERTION(aCookie, "Null cookie passed to bindCookieParameters!");
  nsresult rv;

  // Use the asynchronous binding methods to ensure that we do not acquire the
  // database lock.
  nsCOMPtr<mozIStorageBindingParams> params;
  rv = aParamsArray->NewBindingParams(getter_AddRefs(params));
  NS_ASSERT_SUCCESS(rv);

  // Bind our values to params
  rv = params->BindUTF8StringByName(NS_LITERAL_CSTRING("baseDomain"),
                                    aBaseDomain);
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
nsCookieService::AddCookieToList(const nsCString               &aBaseDomain,
                                 nsCookie                      *aCookie,
                                 DBState                       *aDBState,
                                 mozIStorageBindingParamsArray *aParamsArray,
                                 PRBool                         aWriteToDB)
{
  NS_ASSERTION(!(aDBState->dbConn && !aWriteToDB && aParamsArray),
               "Not writing to the DB but have a params array?");
  NS_ASSERTION(!(!aDBState->dbConn && aParamsArray),
               "Do not have a DB connection but have a params array?");

  nsCookieEntry *entry = aDBState->hostTable.PutEntry(aBaseDomain);
  NS_ASSERTION(entry, "can't insert element into a null entry!");

  entry->GetCookies().AppendElement(aCookie);
  ++aDBState->cookieCount;

  // keep track of the oldest cookie, for when it comes time to purge
  if (aCookie->LastAccessed() < aDBState->cookieOldestTime)
    aDBState->cookieOldestTime = aCookie->LastAccessed();

  // if it's a non-session cookie and hasn't just been read from the db, write it out.
  if (aWriteToDB && !aCookie->IsSession() && aDBState->dbConn) {
    mozIStorageStatement *stmt = aDBState->stmtInsert;
    nsCOMPtr<mozIStorageBindingParamsArray> paramsArray(aParamsArray);
    if (!paramsArray) {
      stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
    }
    bindCookieParameters(paramsArray, aBaseDomain, aCookie);

    // If we were supplied an array to store parameters, we shouldn't call
    // executeAsync - someone up the stack will do this for us.
    if (!aParamsArray) {
      nsresult rv = stmt->BindParameters(paramsArray);
      NS_ASSERT_SUCCESS(rv);
      nsCOMPtr<mozIStoragePendingStatement> handle;
      rv = stmt->ExecuteAsync(mInsertListener, getter_AddRefs(handle));
      NS_ASSERT_SUCCESS(rv);
    }
  }
}

void
nsCookieService::UpdateCookieInList(nsCookie                      *aCookie,
                                    PRInt64                        aLastAccessed,
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
    nsresult rv = params->BindInt64ByName(NS_LITERAL_CSTRING("lastAccessed"),
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
