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
#include "mozIStorageService.h"
#include "mozStorageHelper.h"
#include "nsIPrivateBrowsingService.h"
#include "nsNetCID.h"

/******************************************************************************
 * nsCookieService impl:
 * useful types & constants
 ******************************************************************************/

// XXX_hack. See bug 178993.
// This is a hack to hide HttpOnly cookies from older browsers
static const char kHttpOnlyPrefix[] = "#HttpOnly_";

static const char kCookieFileName[] = "cookies.sqlite";
#define COOKIES_SCHEMA_VERSION 2

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
static const PRUint32 kMaxCookiesPerHost  = 50;
static const PRUint32 kMaxBytesPerCookie  = 4096;
static const PRUint32 kMaxBytesPerPath    = 1024;

// these constants represent a decision about a cookie based on user prefs.
static const PRUint32 STATUS_ACCEPTED            = 0;
static const PRUint32 STATUS_REJECTED            = 1;
// STATUS_REJECTED_WITH_ERROR indicates the cookie should be rejected because
// of an error (rather than something the user can control). this is used for
// notification purposes, since we only want to notify of rejections where
// the user can do something about it (e.g. whitelist the site).
static const PRUint32 STATUS_REJECTED_WITH_ERROR = 2;

// behavior pref constants 
static const PRUint32 BEHAVIOR_ACCEPT        = 0;
static const PRUint32 BEHAVIOR_REJECTFOREIGN = 1;
static const PRUint32 BEHAVIOR_REJECT        = 2;

// pref string constants
static const char kPrefCookiesPermissions[] = "network.cookie.cookieBehavior";
static const char kPrefMaxNumberOfCookies[] = "network.cookie.maxNumber";
static const char kPrefMaxCookiesPerHost[]  = "network.cookie.maxPerHost";
static const char kPrefCookiePurgeAge[]     = "network.cookie.purgeAge";

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

// stores temporary data for enumerating over the hash entries,
// since enumeration is done using callback functions.
struct nsEnumerationData
{
  nsEnumerationData(PRInt64 aCurrentTime, PRInt64 aOldestTime)
   : currentTime(aCurrentTime)
   , oldestTime(aOldestTime)
  {
  }

  // the current time, in seconds
  PRInt64 currentTime;

  // oldest lastAccessed time in the cookie list. use aOldestTime = LL_MAXINT
  // to enable this search, LL_MININT to disable it.
  PRInt64 oldestTime;

  // an iterator object that points to the desired cookie
  nsListIter iter;
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
// this next define has to appear before the include of prlog.h
#define FORCE_PR_LOG // Allow logging in the release build
#include "prlog.h"
#endif

// define logging macros for convenience
#define SET_COOKIE PR_TRUE
#define GET_COOKIE PR_FALSE

#ifdef PR_LOGGING
static PRLogModuleInfo *sCookieLog = PR_NewLogModule("cookie");

#define COOKIE_LOGFAILURE(a, b, c, d)    LogFailure(a, b, c, d)
#define COOKIE_LOGSUCCESS(a, b, c, d, e) LogSuccess(a, b, c, d, e)

#define COOKIE_LOGEVICTED(a)                   \
  PR_BEGIN_MACRO                               \
    if (PR_LOG_TEST(sCookieLog, PR_LOG_DEBUG)) \
      LogEvicted(a);                           \
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

    PR_ExplodeTime(aCookie->CreationID(), PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);
    PR_LOG(sCookieLog, PR_LOG_DEBUG,
      ("created: %s (id %lld)", timeString, aCookie->CreationID()));

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
LogEvicted(nsCookie *aCookie)
{
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("===== COOKIE EVICTED =====\n"));

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
#define COOKIE_LOGEVICTED(a)             PR_BEGIN_MACRO /* nothing */ PR_END_MACRO
#define COOKIE_LOGSTRING(a, b)           PR_BEGIN_MACRO /* nothing */ PR_END_MACRO
#endif

/******************************************************************************
 * nsCookieService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/

nsCookieService *nsCookieService::gCookieService = nsnull;

nsCookieService*
nsCookieService::GetSingleton()
{
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
 , mCookiesPermissions(BEHAVIOR_ACCEPT)
 , mMaxNumberOfCookies(kMaxNumberOfCookies)
 , mMaxCookiesPerHost(kMaxCookiesPerHost)
 , mCookiePurgeAge(kCookiePurgeAge)
{
}

nsresult
nsCookieService::Init()
{
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
    prefBranch->AddObserver(kPrefCookiesPermissions, this, PR_TRUE);
    prefBranch->AddObserver(kPrefMaxNumberOfCookies, this, PR_TRUE);
    prefBranch->AddObserver(kPrefMaxCookiesPerHost,  this, PR_TRUE);
    prefBranch->AddObserver(kPrefCookiePurgeAge,     this, PR_TRUE);
    PrefChanged(prefBranch);
  }

  // failure here is non-fatal (we can run fine without
  // persistent storage - e.g. if there's no profile)
  rv = InitDB();
  if (NS_FAILED(rv))
    COOKIE_LOGSTRING(PR_LOG_WARNING, ("Init(): InitDB() gave error %x", rv));

  mObserverService = do_GetService("@mozilla.org/observer-service;1");
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

  nsCOMPtr<mozIStorageService> storage = do_GetService("@mozilla.org/storage/service;1");
  if (!storage)
    return NS_ERROR_UNEXPECTED;

  // open a connection to the cookie database, and only cache our connection
  // and statements upon success.
  rv = storage->OpenUnsharedDatabase(cookieFile, getter_AddRefs(mDBState->dbConn));
  NS_ENSURE_SUCCESS(rv, rv);

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

    switch (dbSchemaVersion) {
    // upgrading.
    // every time you increment the database schema, you need to implement
    // the upgrading code from the previous version to the new one.
    case 1:
      {
        // add the lastAccessed column to the table
        rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE moz_cookies ADD lastAccessed INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);

        // update the schema version
        rv = mDBState->dbConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // fall through to the next upgrade

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
            "name, "
            "value, "
            "host, "
            "path, "
            "expiry, "
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

  // open in exclusive mode for performance
  mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("PRAGMA locking_mode = EXCLUSIVE"));

  // cache frequently used statements (for insertion, deletion, and updating)
  rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO moz_cookies ("
      "id, "
      "name, "
      "value, "
      "host, "
      "path, "
      "expiry, "
      "lastAccessed, "
      "isSecure, "
      "isHttpOnly"
    ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)"),
    getter_AddRefs(mDBState->stmtInsert));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_cookies WHERE id = ?1"),
    getter_AddRefs(mDBState->stmtDelete));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_cookies SET lastAccessed = ?1 WHERE id = ?2"),
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

// sets the schema version and creates the moz_cookies table.
nsresult
nsCookieService::CreateTable()
{
  // set the schema version, before creating the table
  nsresult rv = mDBState->dbConn->SetSchemaVersion(COOKIES_SCHEMA_VERSION);
  if (NS_FAILED(rv)) return rv;

  // create the table
  return mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE moz_cookies ("
      "id INTEGER PRIMARY KEY, "
      "name TEXT, "
      "value TEXT, "
      "host TEXT, "
      "path TEXT, "
      "expiry INTEGER, "
      "lastAccessed INTEGER, "
      "isSecure INTEGER, "
      "isHttpOnly INTEGER"
    ")"));
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
  mDefaultDBState.dbConn = nsnull;
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
        nsresult rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DELETE FROM moz_cookies"));
        if (NS_FAILED(rv))
          NS_WARNING("db delete failed");
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
  GetCookieInternal(aHostURI, aChannel, PR_FALSE, aCookie);
  
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookieStringFromHttp(nsIURI     *aHostURI,
                                         nsIURI     *aFirstURI,
                                         nsIChannel *aChannel,
                                         char       **aCookie)
{
  GetCookieInternal(aHostURI, aChannel, PR_TRUE, aCookie);

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieString(nsIURI     *aHostURI,
                                 nsIPrompt  *aPrompt,
                                 const char *aCookieHeader,
                                 nsIChannel *aChannel)
{
  return SetCookieStringInternal(aHostURI, aPrompt, aCookieHeader, nsnull, aChannel, PR_FALSE);
}

NS_IMETHODIMP
nsCookieService::SetCookieStringFromHttp(nsIURI     *aHostURI,
                                         nsIURI     *aFirstURI,
                                         nsIPrompt  *aPrompt,
                                         const char *aCookieHeader,
                                         const char *aServerTime,
                                         nsIChannel *aChannel) 
{
  return SetCookieStringInternal(aHostURI, aPrompt, aCookieHeader, aServerTime, aChannel, PR_TRUE);
}

nsresult
nsCookieService::SetCookieStringInternal(nsIURI     *aHostURI,
                                         nsIPrompt  *aPrompt,
                                         const char *aCookieHeader,
                                         const char *aServerTime,
                                         nsIChannel *aChannel,
                                         PRBool      aFromHttp) 
{
  if (!aHostURI) {
    COOKIE_LOGFAILURE(SET_COOKIE, nsnull, aCookieHeader, "host URI is null");
    return NS_OK;
  }

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
    return NS_OK;
  }

  // check default prefs
  PRUint32 cookieStatus = CheckPrefs(aHostURI, aChannel, baseDomain,
                                     requireHostMatch, aCookieHeader);
  // fire a notification if cookie was rejected (but not if there was an error)
  switch (cookieStatus) {
  case STATUS_REJECTED:
    NotifyRejected(aHostURI);
  case STATUS_REJECTED_WITH_ERROR:
    return NS_OK;
  }

  // parse server local time. this is not just done here for efficiency
  // reasons - if there's an error parsing it, and we need to default it
  // to the current time, we must do it here since the current time in
  // SetCookieInternal() will change for each cookie processed (e.g. if the
  // user is prompted).
  PRTime tempServerTime;
  PRInt64 serverTime;
  if (aServerTime &&
      PR_ParseTimeString(aServerTime, PR_TRUE, &tempServerTime) == PR_SUCCESS) {
    serverTime = tempServerTime / PR_USEC_PER_SEC;
  } else {
    serverTime = PR_Now() / PR_USEC_PER_SEC;
  }

  // start a transaction on the storage db, to optimize insertions.
  // transaction will automically commit on completion
  mozStorageTransaction transaction(mDBState->dbConn, PR_TRUE);
 
  // switch to a nice string type now, and process each cookie in the header
  nsDependentCString cookieHeader(aCookieHeader);
  while (SetCookieInternal(aHostURI, aChannel, baseDomain, requireHostMatch,
                           cookieHeader, serverTime, aFromHttp));

  return NS_OK;
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
  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefCookiesPermissions, &val)))
    mCookiesPermissions = (PRUint8) LIMIT(val, 0, 2, 0);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefMaxNumberOfCookies, &val)))
    mMaxNumberOfCookies = (PRUint16) LIMIT(val, 1, 0xFFFF, kMaxNumberOfCookies);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefMaxCookiesPerHost, &val)))
    mMaxCookiesPerHost = (PRUint16) LIMIT(val, 1, 0xFFFF, kMaxCookiesPerHost);

  if (NS_SUCCEEDED(aPrefBranch->GetIntPref(kPrefCookiePurgeAge, &val)))
    mCookiePurgeAge = LIMIT(val, 0, PR_INT32_MAX, PR_INT32_MAX) * PR_USEC_PER_SEC;
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

    nsresult rv = mDBState->dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DELETE FROM moz_cookies"));
    if (NS_FAILED(rv)) {
      // Database must be corrupted, so remove it completely.
      nsCOMPtr<nsIFile> dbFile;
      mDBState->dbConn->GetDatabaseFile(getter_AddRefs(dbFile));
      CloseDB();
      dbFile->Remove(PR_FALSE);

      InitDB();
    }
  }

  NotifyChanged(nsnull, NS_LITERAL_STRING("cleared").get());
  return NS_OK;
}

// helper struct for passing arguments into hash enumeration callback.
struct nsGetEnumeratorData
{
  nsGetEnumeratorData(nsCOMArray<nsICookie> *aArray, PRInt64 aTime)
   : array(aArray)
   , currentTime(aTime) {}

  nsCOMArray<nsICookie> *array;
  PRInt64 currentTime;
};

static PLDHashOperator
COMArrayCallback(nsCookieEntry *aEntry,
                 void          *aArg)
{
  nsGetEnumeratorData *data = static_cast<nsGetEnumeratorData *>(aArg);

  const nsCookieEntry::ArrayType &cookies = aEntry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    nsCookie *cookie = cookies[i];

    // only append non-expired cookies
    if (cookie->Expiry() > data->currentTime)
      data->array->AppendObject(cookie);
  }
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsCookieService::GetEnumerator(nsISimpleEnumerator **aEnumerator)
{
  nsCOMArray<nsICookie> cookieList(mDBState->cookieCount);
  nsGetEnumeratorData data(&cookieList, PR_Now() / PR_USEC_PER_SEC);

  mDBState->hostTable.EnumerateEntries(COMArrayCallback, &data);

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
                     currentTimeInUsec,
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
                 matchIter,
                 PR_Now() / PR_USEC_PER_SEC)) {
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
  nsresult rv;

  // delete expired cookies, before we read in the db
  {
    // scope the deletion, so the write lock is released when finished
    nsCOMPtr<mozIStorageStatement> stmtDeleteExpired;
    rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_cookies WHERE expiry <= ?1"),
      getter_AddRefs(stmtDeleteExpired));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmtDeleteExpired->BindInt64Parameter(0, PR_Now() / PR_USEC_PER_SEC);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = stmtDeleteExpired->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // let the reading begin!
  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mDBState->dbConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "id, "
      "name, "
      "value, "
      "host, "
      "path, "
      "expiry, "
      "lastAccessed, "
      "isSecure, "
      "isHttpOnly "
    "FROM moz_cookies"), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString baseDomain, name, value, host, path;
  PRBool hasResult;
  while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&hasResult)) && hasResult) {
    PRInt64 creationID = stmt->AsInt64(0);
    
    stmt->GetUTF8String(1, name);
    stmt->GetUTF8String(2, value);
    stmt->GetUTF8String(3, host);
    stmt->GetUTF8String(4, path);

    PRInt64 expiry = stmt->AsInt64(5);
    PRInt64 lastAccessed = stmt->AsInt64(6);
    PRBool isSecure = 0 != stmt->AsInt32(7);
    PRBool isHttpOnly = 0 != stmt->AsInt32(8);

    // compute the baseDomain from the host
    rv = GetBaseDomainFromHost(host, baseDomain);
    if (NS_FAILED(rv))
      continue;

    // create a new nsCookie and assign the data.
    nsCookie* newCookie =
      nsCookie::Create(name, value, host, path,
                       expiry,
                       lastAccessed,
                       creationID,
                       PR_FALSE,
                       isSecure,
                       isHttpOnly);
    if (!newCookie)
      return NS_ERROR_OUT_OF_MEMORY;

    if (!AddCookieToList(baseDomain, newCookie, PR_FALSE))
      // It is purpose that created us; purpose that connects us;
      // purpose that pulls us; that guides us; that drives us.
      // It is purpose that defines us; purpose that binds us.
      // When a cookie no longer has purpose, it has a choice:
      // it can return to the source to be deleted, or it can go
      // into exile, and stay hidden inside the Matrix.
      // Let's choose deletion.
      delete newCookie;
  }

  COOKIE_LOGSTRING(PR_LOG_DEBUG, ("Read(): %ld cookies read", mDBState->cookieCount));

  return rv;
}

NS_IMETHODIMP
nsCookieService::ImportCookies(nsIFile *aCookieFile)
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), aCookieFile);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) return rv;

  // start a transaction on the storage db, to optimize insertions.
  // transaction will automically commit on completion
  mozStorageTransaction transaction(mDBState->dbConn, PR_TRUE);

  static const char kTrue[] = "TRUE";

  nsCAutoString buffer, baseDomain;
  PRBool isMore = PR_TRUE;
  PRInt32 hostIndex, isDomainIndex, pathIndex, secureIndex, expiresIndex, nameIndex, cookieIndex;
  nsASingleFragmentCString::char_iterator iter;
  PRInt32 numInts;
  PRInt64 expires;
  PRBool isDomain, isHttpOnly = PR_FALSE;
  PRUint32 originalCookieCount = mDBState->cookieCount;

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

    // create a new nsCookie and assign the data.
    // we don't know the cookie creation time, so just use the current time;
    // this is okay, since nsCookie::Create() will make sure the creation id
    // ends up monotonically increasing.
    nsRefPtr<nsCookie> newCookie =
      nsCookie::Create(Substring(buffer, nameIndex, cookieIndex - nameIndex - 1),
                       Substring(buffer, cookieIndex, buffer.Length() - cookieIndex),
                       host,
                       Substring(buffer, pathIndex, secureIndex - pathIndex - 1),
                       expires,
                       lastAccessedCounter,
                       currentTimeInUsec,
                       PR_FALSE,
                       Substring(buffer, secureIndex, expiresIndex - secureIndex - 1).EqualsLiteral(kTrue),
                       isHttpOnly);
    if (!newCookie) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    // trick: preserve the most-recently-used cookie ordering,
    // by successively decrementing the lastAccessed time
    lastAccessedCounter--;

    if (originalCookieCount == 0)
      AddCookieToList(baseDomain, newCookie);
    else
      AddInternal(baseDomain, newCookie, currentTimeInUsec, nsnull, nsnull, PR_TRUE);
  }

  COOKIE_LOGSTRING(PR_LOG_DEBUG, ("ImportCookies(): %ld cookies imported", mDBState->cookieCount));

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
    // CreationID is unique, so two id's can never be equal.
    return PR_FALSE;
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
    // note: CreationID is unique, so two id's can never be equal.
    return aCookie1->CreationID() < aCookie2->CreationID();
  }
};

void
nsCookieService::GetCookieInternal(nsIURI      *aHostURI,
                                   nsIChannel  *aChannel,
                                   PRBool       aHttpBound,
                                   char       **aCookie)
{
  *aCookie = nsnull;

  if (!aHostURI) {
    COOKIE_LOGFAILURE(GET_COOKIE, nsnull, nsnull, "host URI is null");
    return;
  }

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
  PRUint32 cookieStatus = CheckPrefs(aHostURI, aChannel, baseDomain,
                                     requireHostMatch, nsnull);
  // for GetCookie(), we don't fire rejection notifications.
  switch (cookieStatus) {
  case STATUS_REJECTED:
  case STATUS_REJECTED_WITH_ERROR:
    return;
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
    // start a transaction on the storage db, to optimize updates.
    // transaction will automically commit on completion.
    mozStorageTransaction transaction(mDBState->dbConn, PR_TRUE);

    for (PRInt32 i = 0; i < count; ++i) {
      cookie = foundCookieList.ElementAt(i);

      if (currentTimeInUsec - cookie->LastAccessed() > kCookieStaleThreshold)
        UpdateCookieInList(cookie, currentTimeInUsec);
    }
  }

  // return cookies in order of path length; longest to shortest.
  // this is required per RFC2109.  if cookies match in length,
  // then sort by creation time (see bug 236772).
  foundCookieList.Sort(CompareCookiesForSending());

  nsCAutoString cookieData;
  for (PRInt32 i = 0; i < count; ++i) {
    cookie = foundCookieList.ElementAt(i);

    // check if we have anything to write
    if (!cookie->Name().IsEmpty() || !cookie->Value().IsEmpty()) {
      // if we've already added a cookie to the return list, append a "; " so
      // that subsequent cookies are delimited in the final list.
      if (!cookieData.IsEmpty()) {
        cookieData.AppendLiteral("; ");
      }

      if (!cookie->Name().IsEmpty()) {
        // we have a name and value - write both
        cookieData += cookie->Name() + NS_LITERAL_CSTRING("=") + cookie->Value();
      } else {
        // just write value
        cookieData += cookie->Value();
      }
    }
  }

  // it's wasteful to alloc a new string; but we have no other choice, until we
  // fix the callers to use nsACStrings.
  if (!cookieData.IsEmpty()) {
    COOKIE_LOGSUCCESS(GET_COOKIE, aHostURI, cookieData, nsnull, nsnull);
    *aCookie = ToNewCString(cookieData);
  }
}

// processes a single cookie, and returns PR_TRUE if there are more cookies
// to be processed
PRBool
nsCookieService::SetCookieInternal(nsIURI             *aHostURI,
                                   nsIChannel         *aChannel,
                                   const nsCString    &aBaseDomain,
                                   PRBool              aRequireHostMatch,
                                   nsDependentCString &aCookieHeader,
                                   PRInt64             aServerTime,
                                   PRBool              aFromHttp)
{
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
                     currentTimeInUsec,
                     cookieAttributes.isSession,
                     cookieAttributes.isSecure,
                     cookieAttributes.isHttpOnly);
  if (!cookie)
    return newCookie;

  // check permissions from site permission list, or ask the user,
  // to determine if we can set the cookie
  if (mPermissionService) {
    PRBool permission;
    // we need to think about prompters/parent windows here - TestPermission
    // needs one to prompt, so right now it has to fend for itself to get one
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
nsCookieService::AddInternal(const nsCString &aBaseDomain,
                             nsCookie        *aCookie,
                             PRInt64          aCurrentTimeInUsec,
                             nsIURI          *aHostURI,
                             const char      *aCookieHeader,
                             PRBool           aFromHttp)
{
  PRInt64 currentTime = aCurrentTimeInUsec / PR_USEC_PER_SEC;

  // if the new cookie is httponly, make sure we're not coming from script
  if (!aFromHttp && aCookie->IsHttpOnly()) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "cookie is httponly; coming from script");
    return;
  }

  // start a transaction on the storage db, to optimize deletions/insertions.
  // transaction will automically commit on completion. if we already have a
  // transaction (e.g. from SetCookie*()), this will have no effect. 
  mozStorageTransaction transaction(mDBState->dbConn, PR_TRUE);

  nsListIter matchIter;
  PRBool foundCookie = FindCookie(aBaseDomain, aCookie->Host(),
    aCookie->Name(), aCookie->Path(), matchIter, currentTime);

  nsRefPtr<nsCookie> oldCookie;
  if (foundCookie) {
    oldCookie = matchIter.Cookie();

    // if the old cookie is httponly, make sure we're not coming from script
    if (!aFromHttp && oldCookie->IsHttpOnly()) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "previously stored cookie is httponly; coming from script");
      return;
    }

    RemoveCookieFromList(matchIter);

    // check if the cookie has expired
    if (aCookie->Expiry() <= currentTime) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "previously stored cookie was deleted");
      NotifyChanged(oldCookie, NS_LITERAL_STRING("deleted").get());
      return;
    }

    // preserve creation time of cookie
    if (oldCookie)
      aCookie->SetCreationID(oldCookie->CreationID());

  } else {
    // check if cookie has already expired
    if (aCookie->Expiry() <= currentTime) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "cookie has already expired");
      return;
    }

    // check if we have to delete an old cookie.
    nsEnumerationData data(currentTime, LL_MAXINT);
    if (CountCookiesFromHostInternal(aBaseDomain, data) >= mMaxCookiesPerHost) {
      // remove the oldest cookie from host
      oldCookie = data.iter.Cookie();
      COOKIE_LOGEVICTED(oldCookie);
      RemoveCookieFromList(data.iter);

      NotifyChanged(oldCookie, NS_LITERAL_STRING("deleted").get());

    } else if (mDBState->cookieCount >= ADD_TEN_PERCENT(mMaxNumberOfCookies)) {
      PRInt64 maxAge = aCurrentTimeInUsec - mDBState->cookieOldestTime;
      PRInt64 purgeAge = ADD_TEN_PERCENT(mCookiePurgeAge);
      if (maxAge >= purgeAge) {
        // we're over both size and age limits by 10%; time to purge the table!
        // do this by:
        // 1) removing expired cookies;
        // 2) evicting the balance of old cookies, until we reach the size limit.
        // note that the cookieOldestTime indicator can be pessimistic - if it's
        // older than the actual oldest cookie, we'll just purge more eagerly.
        PurgeCookies(aCurrentTimeInUsec);
      }
    }
  }

  // add the cookie to head of list
  AddCookieToList(aBaseDomain, aCookie);
  NotifyChanged(aCookie, foundCookie ? NS_LITERAL_STRING("changed").get()
                                     : NS_LITERAL_STRING("added").get());

  COOKIE_LOGSUCCESS(SET_COOKIE, aHostURI, aCookieHeader, aCookie, foundCookie != nsnull);
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

PRBool
nsCookieService::IsForeign(const nsCString &aBaseDomain,
                           PRBool           aRequireHostMatch,
                           nsIURI          *aFirstURI)
{
  nsCAutoString firstHost;
  if (NS_FAILED(aFirstURI->GetAsciiHost(firstHost))) {
    // assume foreign
    return PR_TRUE;
  }

  // trim any trailing dot
  if (!firstHost.IsEmpty() && firstHost.Last() == '.')
    firstHost.Truncate(firstHost.Length() - 1);

  // check whether the host is either an IP address, an alias such as
  // 'localhost', an eTLD such as 'co.uk', or the empty string. in these
  // cases, require an exact string match for the domain. note that the base
  // domain parameter will be equivalent to the host in this case.
  if (aRequireHostMatch)
    return !firstHost.Equals(aBaseDomain);

  // ensure the originating domain is also derived from the host's base domain.
  return !IsSubdomainOf(firstHost, aBaseDomain);
}

PRUint32
nsCookieService::CheckPrefs(nsIURI          *aHostURI,
                            nsIChannel      *aChannel,
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
    rv = mPermissionService->CanAccess(aHostURI, aChannel, &access);

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
  if (mCookiesPermissions == BEHAVIOR_REJECT) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "cookies are disabled");
    return STATUS_REJECTED;

  } else if (mCookiesPermissions == BEHAVIOR_REJECTFOREIGN) {
    // check if cookie is foreign
    if (!mPermissionService) {
      NS_WARNING("Foreign cookie blocking enabled, but nsICookiePermission unavailable! Rejecting cookie");
      COOKIE_LOGSTRING(PR_LOG_WARNING, ("CheckPrefs(): foreign blocking enabled, but nsICookiePermission unavailable! Rejecting cookie"));
      return STATUS_REJECTED;
    }

    nsCOMPtr<nsIURI> firstURI;
    rv = mPermissionService->GetOriginatingURI(aChannel, getter_AddRefs(firstURI));

    if (NS_FAILED(rv) || IsForeign(aBaseDomain, aRequireHostMatch, firstURI)) {
      COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "originating server test failed");
      return STATUS_REJECTED;
    }
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
              nsIMutableArray *aRemovedList)
   : currentTime(aCurrentTime)
   , purgeTime(aPurgeTime)
   , oldestTime(LL_MAXINT)
   , purgeList(aPurgeList)
   , removedList(aRemovedList) {}

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
};

// comparator class for lastaccessed times of cookies.
class CompareCookiesByAge {
public:
  PRBool Equals(const nsListIter &a, const nsListIter &b) const
  {
    // CreationID is unique, so two id's can never be equal.
    return PR_FALSE;
  }

  PRBool LessThan(const nsListIter &a, const nsListIter &b) const
  {
    // compare by LastAccessed time, and tiebreak by CreationID.
    PRInt64 result = a.Cookie()->LastAccessed() - b.Cookie()->LastAccessed();
    if (result != 0)
      return result < 0;

    return a.Cookie()->CreationID() < b.Cookie()->CreationID();
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
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ) {
    nsListIter iter(aEntry, i);
    nsCookie *cookie = cookies[i];

    // check if the cookie has expired
    if (cookie->Expiry() <= data.currentTime) {
      data.removedList->AppendElement(cookie, PR_FALSE);
      COOKIE_LOGEVICTED(cookie);

      // remove from list; do not increment our iterator
      nsCookieService::gCookieService->RemoveCookieFromList(iter);

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
    ("PurgeCookies(): beginning purge with %ld cookies and %lld age",
     mDBState->cookieCount, aCurrentTimeInUsec - mDBState->cookieOldestTime));
#endif

  nsAutoTArray<nsListIter, kMaxNumberOfCookies> purgeList;

  nsCOMPtr<nsIMutableArray> removedList = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!removedList)
    return;

  nsPurgeData data(aCurrentTimeInUsec / PR_USEC_PER_SEC,
    aCurrentTimeInUsec - mCookiePurgeAge, purgeList, removedList);
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
    COOKIE_LOGEVICTED(cookie);

    RemoveCookieFromList(purgeList[i]);
  }

  // take all the cookies in the removed list, and notify about them in one batch
  NotifyChanged(removedList, NS_LITERAL_STRING("batch-deleted").get());

  // reset the oldest time indicator
  mDBState->cookieOldestTime = data.oldestTime;

  COOKIE_LOGSTRING(PR_LOG_DEBUG,
    ("PurgeCookies(): %ld expired; %ld purged; %ld remain; %lld oldest age",
     initialCookieCount - postExpiryCookieCount,
     mDBState->cookieCount - postExpiryCookieCount,
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
  *aFoundCookie = FindCookie(baseDomain, host, name, path, iter,
                             PR_Now() / PR_USEC_PER_SEC);
  return NS_OK;
}

// count the number of cookies in a base domain, and simultaneously find the
// oldest cookie in that domain.
PRUint32
nsCookieService::CountCookiesFromHostInternal(const nsCString   &aBaseDomain,
                                              nsEnumerationData &aData)
{
  nsCookieEntry *entry = mDBState->hostTable.GetEntry(aBaseDomain);
  if (!entry)
    return 0;

  PRUint32 countFromHost = 0;
  const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    nsCookie *cookie = cookies[i];

    // only count non-expired cookies
    if (cookie->Expiry() > aData.currentTime) {
      ++countFromHost;

      // check if we've found the oldest cookie so far
      if (aData.oldestTime > cookie->LastAccessed()) {
        aData.oldestTime = cookie->LastAccessed();
        aData.iter = nsListIter(entry, i);
      }
    }
  }

  return countFromHost;
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

  // we don't care about finding the oldest cookie here, so disable the search
  nsEnumerationData data(PR_Now() / PR_USEC_PER_SEC, LL_MININT);
  *aCountFromHost = CountCookiesFromHostInternal(baseDomain, data);
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

  nsCOMArray<nsICookie> cookieList(mMaxCookiesPerHost);
  PRInt64 currentTime = PR_Now() / PR_USEC_PER_SEC;

  nsCookieEntry *entry = mDBState->hostTable.GetEntry(baseDomain);
  if (!entry)
    return NS_NewEmptyEnumerator(aEnumerator);

  const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    nsCookie *cookie = cookies[i];

    // only append non-expired cookies
    if (cookie->Expiry() > currentTime)
      cookieList.AppendObject(cookie);
  }

  return NS_NewArrayEnumerator(aEnumerator, cookieList);
}

// find an exact cookie specified by host, name, and path that hasn't expired.
PRBool
nsCookieService::FindCookie(const nsCString      &aBaseDomain,
                            const nsAFlatCString &aHost,
                            const nsAFlatCString &aName,
                            const nsAFlatCString &aPath,
                            nsListIter           &aIter,
                            PRInt64               aCurrentTime)
{
  nsCookieEntry *entry = mDBState->hostTable.GetEntry(aBaseDomain);
  if (!entry)
    return PR_FALSE;

  const nsCookieEntry::ArrayType &cookies = entry->GetCookies();
  for (nsCookieEntry::IndexType i = 0; i < cookies.Length(); ++i) {
    nsCookie *cookie = cookies[i];

    if (cookie->Expiry() > aCurrentTime &&
        aHost.Equals(cookie->Host()) &&
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
nsCookieService::RemoveCookieFromList(const nsListIter &aIter)
{
  // if it's a non-session cookie, remove it from the db
  if (!aIter.Cookie()->IsSession() && mDBState->dbConn) {
    // use our cached sqlite "delete" statement
    mozStorageStatementScoper scoper(mDBState->stmtDelete);

    PRInt64 creationID = aIter.Cookie()->CreationID();
    nsresult rv = mDBState->stmtDelete->BindInt64Parameter(0, creationID);
    if (NS_SUCCEEDED(rv)) {
      PRBool hasResult;
      rv = mDBState->stmtDelete->ExecuteStep(&hasResult);
    }

    if (NS_FAILED(rv)) {
      NS_WARNING("db remove failed!");
      COOKIE_LOGSTRING(PR_LOG_WARNING, ("RemoveCookieFromList(): removing from db gave error %x", rv));
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

static nsresult
bindCookieParameters(mozIStorageStatement *aStmt, const nsCookie *aCookie)
{
  nsresult rv;

  rv = aStmt->BindInt64Parameter(0, aCookie->CreationID());
  if (NS_FAILED(rv)) return rv;

  rv = aStmt->BindUTF8StringParameter(1, aCookie->Name());
  if (NS_FAILED(rv)) return rv;
  
  rv = aStmt->BindUTF8StringParameter(2, aCookie->Value());
  if (NS_FAILED(rv)) return rv;
  
  rv = aStmt->BindUTF8StringParameter(3, aCookie->Host());
  if (NS_FAILED(rv)) return rv;
  
  rv = aStmt->BindUTF8StringParameter(4, aCookie->Path());
  if (NS_FAILED(rv)) return rv;
  
  rv = aStmt->BindInt64Parameter(5, aCookie->Expiry());
  if (NS_FAILED(rv)) return rv;
  
  rv = aStmt->BindInt64Parameter(6, aCookie->LastAccessed());
  if (NS_FAILED(rv)) return rv;
  
  rv = aStmt->BindInt32Parameter(7, aCookie->IsSecure());
  if (NS_FAILED(rv)) return rv;
  
  rv = aStmt->BindInt32Parameter(8, aCookie->IsHttpOnly());
  return rv;
}

PRBool
nsCookieService::AddCookieToList(const nsCString &aBaseDomain,
                                 nsCookie        *aCookie,
                                 PRBool           aWriteToDB)
{
  nsCookieEntry *entry = mDBState->hostTable.PutEntry(aBaseDomain);
  if (!entry) {
    NS_ERROR("can't insert element into a null entry!");
    return PR_FALSE;
  }

  entry->GetCookies().AppendElement(aCookie);
  ++mDBState->cookieCount;

  // keep track of the oldest cookie, for when it comes time to purge
  if (aCookie->LastAccessed() < mDBState->cookieOldestTime)
    mDBState->cookieOldestTime = aCookie->LastAccessed();

  // if it's a non-session cookie and hasn't just been read from the db, write it out.
  if (aWriteToDB && !aCookie->IsSession() && mDBState->dbConn) {
    // use our cached sqlite "insert" statement
    mozStorageStatementScoper scoper(mDBState->stmtInsert);

    nsresult rv = bindCookieParameters(mDBState->stmtInsert, aCookie);
    if (NS_SUCCEEDED(rv)) {
      PRBool hasResult;
      rv = mDBState->stmtInsert->ExecuteStep(&hasResult);
    }

    if (NS_FAILED(rv)) {
      NS_WARNING("db insert failed!");
      COOKIE_LOGSTRING(PR_LOG_WARNING, ("AddCookieToList(): adding to db gave error %x", rv));
    }
  }

  return PR_TRUE;
}

void
nsCookieService::UpdateCookieInList(nsCookie *aCookie, PRInt64 aLastAccessed)
{
  // update the lastAccessed timestamp
  aCookie->SetLastAccessed(aLastAccessed);

  // if it's a non-session cookie, update it in the db too
  if (!aCookie->IsSession() && mDBState->dbConn) {
    // use our cached sqlite "update" statement
    mozStorageStatementScoper scoper(mDBState->stmtUpdate);

    nsresult rv = mDBState->stmtUpdate->BindInt64Parameter(0, aLastAccessed);
    if (NS_SUCCEEDED(rv)) {
      rv = mDBState->stmtUpdate->BindInt64Parameter(1, aCookie->CreationID());
      if (NS_SUCCEEDED(rv)) {
        PRBool hasResult;
        rv = mDBState->stmtUpdate->ExecuteStep(&hasResult);
      }
    }

    if (NS_FAILED(rv)) {
      NS_WARNING("db update failed!");
      COOKIE_LOGSTRING(PR_LOG_WARNING, ("UpdateCookieInList(): updating db gave error %x", rv));
    }
  }
}

