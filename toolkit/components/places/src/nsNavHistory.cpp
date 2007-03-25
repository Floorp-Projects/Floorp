//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla History System
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
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

#include <stdio.h>
#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "nsMorkHistoryImporter.h"
#include "nsAnnotationService.h"

#include "nsIArray.h"
#include "nsArrayEnumerator.h"
#include "nsCollationCID.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsDateTimeFormatCID.h"
#include "nsDebug.h"
#include "nsEnumeratorUtils.h"
#include "nsFaviconService.h"
#include "nsIChannelEventSink.h"
#include "nsIComponentManager.h"
#include "nsILocaleService.h"
#include "nsILocalFile.h"
#include "nsIPrefBranch2.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsPrimitives.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsPromiseFlatString.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "prsystem.h"
#include "prtime.h"
#include "prprf.h"

#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageStatement.h"
#include "mozIStorageFunction.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "nsAppDirectoryServiceDefs.h"

// Microsecond timeout for "recent" events such as typed and bookmark following.
// If you typed it more than this time ago, it's not recent.
// This is 15 minutes           m    s/m  us/s
#define RECENT_EVENT_THRESHOLD (15 * 60 * 1000000)

// Microseconds ago to look for redirects when updating bookmarks. Used to
// compute the threshold for nsNavBookmarks::AddBookmarkToHash
#define BOOKMARK_REDIRECT_TIME_THRESHOLD (2 * 60 * 100000)

// The maximum number of things that we will store in the recent events list
// before calling ExpireNonrecentEvents. This number should be big enough so it
// is very difficult to get that many unconsumed events (for example, typed but
// never visited) in the RECENT_EVENT_THRESHOLD. Otherwise, we'll start
// checking each one for every page visit, which will be somewhat slower.
#define RECENT_EVENT_QUEUE_MAX_LENGTH 128

// preference ID strings
#define PREF_BRANCH_BASE                        "browser."
#define PREF_BROWSER_HISTORY_EXPIRE_DAYS        "history_expire_days"
#define PREF_AUTOCOMPLETE_ONLY_TYPED            "urlbar.matchOnlyTyped"
#define PREF_AUTOCOMPLETE_ENABLED               "urlbar.autocomplete.enabled"
#define PREF_DB_CACHE_PERCENTAGE                "history_cache_percentage"

// Default (integer) value of PREF_DB_CACHE_PERCENTAGE from 0-100
// This is 6% of machine memory, giving 15MB for a user with 256MB of memory.
// The most that will be used is the size of the DB file. Normal history sizes
// look like 10MB would be a high average for a typical user, so the maximum
// should not normally be required.
#define DEFAULT_DB_CACHE_PERCENTAGE 6

// The default page size the database should use. This must be a power of 2.
// Larger pages may mean less paging, but when something is changed, the
// entire page is written to the journal and then the main file, meaning a
// lot more data has to be handled.
#define DEFAULT_DB_PAGE_SIZE 1024

// the value of mLastNow expires every 3 seconds
#define HISTORY_EXPIRE_NOW_TIMEOUT (3 * PR_MSEC_PER_SEC)

// see bug #319004 -- clamp title and URL to generously-large but not too large
// length
#define HISTORY_URI_LENGTH_MAX 65536
#define HISTORY_TITLE_LENGTH_MAX 4096

// Lazy adding

#ifdef LAZY_ADD

// time that we'll wait before committing messages
#define LAZY_MESSAGE_TIMEOUT (3 * PR_MSEC_PER_SEC)

// the maximum number of times we'll postpone a lazy timer before committing
// See StartLazyTimer()
#define MAX_LAZY_TIMER_DEFERMENTS 2

#endif // LAZY_ADD

NS_IMPL_ADDREF(nsNavHistory)
NS_IMPL_RELEASE(nsNavHistory)

NS_INTERFACE_MAP_BEGIN(nsNavHistory)
  NS_INTERFACE_MAP_ENTRY(nsINavHistoryService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIGlobalHistory2, nsIGlobalHistory3)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalHistory3)
  NS_INTERFACE_MAP_ENTRY(nsIBrowserHistory)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteSearch)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsINavHistoryService)
NS_INTERFACE_MAP_END

static nsresult GetReversedHostname(nsIURI* aURI, nsAString& host);
static void GetReversedHostname(const nsString& aForward, nsAString& aReversed);
static void GetSubstringFromNthDot(const nsCString& aInput, PRInt32 aStartingSpot,
                                   PRInt32 aN, PRBool aIncludeDot,
                                   nsACString& aSubstr);
static nsresult GenerateTitleFromURI(nsIURI* aURI, nsAString& aTitle);
static PRInt32 GetTLDCharCount(const nsCString& aHost);
static PRInt32 GetTLDType(const nsCString& aHostTail);
static PRBool IsNumericHostName(const nsCString& aHost);
static PRInt64 GetSimpleBookmarksQueryFolder(
    const nsCOMArray<nsNavHistoryQuery>& aQueries);
static void ParseSearchQuery(const nsString& aQuery, nsStringArray* aTerms);

inline void ReverseString(const nsString& aInput, nsAString& aReversed)
{
  aReversed.Truncate(0);
  for (PRInt32 i = aInput.Length() - 1; i >= 0; i --)
    aReversed.Append(aInput[i]);
}
inline void parameterString(PRInt32 paramIndex, nsACString& aParamString)
{
  aParamString = nsPrintfCString("?%d", paramIndex + 1);
}


// UpdateBatchScoper
//
//    This just sets begin/end of batch updates to correspond to C++ scopes so
//    we can be sure end always gets called.

class UpdateBatchScoper
{
public:
  UpdateBatchScoper(nsNavHistory& aNavHistory) : mNavHistory(aNavHistory)
  {
    mNavHistory.BeginUpdateBatch();
  }
  ~UpdateBatchScoper()
  {
    mNavHistory.EndUpdateBatch();
  }
protected:
  nsNavHistory& mNavHistory;
};

// if adding a new one, be sure to update nsNavBookmarks statements and
// its kGetChildrenIndex_* constants
const PRInt32 nsNavHistory::kGetInfoIndex_PageID = 0;
const PRInt32 nsNavHistory::kGetInfoIndex_URL = 1;
const PRInt32 nsNavHistory::kGetInfoIndex_Title = 2;
const PRInt32 nsNavHistory::kGetInfoIndex_UserTitle = 3;
const PRInt32 nsNavHistory::kGetInfoIndex_RevHost = 4;
const PRInt32 nsNavHistory::kGetInfoIndex_VisitCount = 5;
const PRInt32 nsNavHistory::kGetInfoIndex_VisitDate = 6;
const PRInt32 nsNavHistory::kGetInfoIndex_FaviconURL = 7;
const PRInt32 nsNavHistory::kGetInfoIndex_SessionId = 8;

const PRInt32 nsNavHistory::kAutoCompleteIndex_URL = 0;
const PRInt32 nsNavHistory::kAutoCompleteIndex_Title = 1;
const PRInt32 nsNavHistory::kAutoCompleteIndex_VisitCount = 2;
const PRInt32 nsNavHistory::kAutoCompleteIndex_Typed = 3;

static nsDataHashtable<nsCStringHashKey, int>* gTldTypes;
static const char* gQuitApplicationMessage = "quit-application";
static const char* gXpcomShutdown = "xpcom-shutdown";

// annotation names
const char nsNavHistory::kAnnotationPreviousEncoding[] = "history/encoding";


nsNavHistory* nsNavHistory::gHistoryService;

// nsNavHistory::nsNavHistory

nsNavHistory::nsNavHistory() : mNowValid(PR_FALSE),
                               mExpireNowTimer(nsnull),
                               mExpire(this),
                               mBatchesInProgress(0),
                               mAutoCompleteOnlyTyped(PR_FALSE),
                               mExpireDays(0)
{
#ifdef LAZY_ADD
  mLazyTimerSet = PR_TRUE;
  mLazyTimerDeferments = 0;
#endif
  NS_ASSERTION(! gHistoryService, "YOU ARE CREATING 2 COPIES OF THE HISTORY SERVICE. Everything will break.");
  gHistoryService = this;
}


// nsNavHistory::~nsNavHistory

nsNavHistory::~nsNavHistory()
{
  // remove the static reference to the service. Check to make sure its us
  // in case somebody creates an extra instance of the service.
  NS_ASSERTION(gHistoryService == this, "YOU CREATED 2 COPIES OF THE HISTORY SERVICE.");
  gHistoryService = nsnull;
}


// nsNavHistory::Init

nsresult
nsNavHistory::Init()
{
  nsresult rv;

  gExpandedItems.Init(128);

  // prefs (must be before DB init, which uses the pref service)
  nsCOMPtr<nsIPrefService> prefService =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = prefService->GetBranch(PREF_BRANCH_BASE, getter_AddRefs(mPrefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool doImport;
  rv = InitDB(&doImport);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = InitStatements();
  NS_ENSURE_SUCCESS(rv, rv);
#ifdef IN_MEMORY_LINKS
  rv = InitMemDB();
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  rv = InitAutoComplete();
  NS_ENSURE_SUCCESS(rv, rv);

  // extract the last session ID so we know where to pick up. There is no index
  // over sessions so the naive statement "SELECT MAX(session) FROM
  // moz_historyvisits" won't have good performance. Instead we select the
  // session of the last visited page because we do have indices over dates.
  // We still do MAX(session) in case there are duplicate sessions for the same
  // date, but there will generally be very few (1) of these.
  {
    nsCOMPtr<mozIStorageStatement> selectSession;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT MAX(session) FROM "
        "(SELECT MAX(visit_date) AS visit_date FROM moz_historyvisits) maxvd "
        "JOIN moz_historyvisits v ON maxvd.visit_date = v.visit_date"),
      getter_AddRefs(selectSession));
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool hasSession;
    if (NS_SUCCEEDED(selectSession->ExecuteStep(&hasSession)) && hasSession)
      mLastSessionID = selectSession->AsInt64(0);
    else
      mLastSessionID = 1;
  }

  // string bundle for localization
  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = bundleService->CreateBundle(
      "chrome://browser/locale/places/places.properties",
      getter_AddRefs(mBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  // locale
  nsCOMPtr<nsILocaleService> ls = do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ls->GetApplicationLocale(getter_AddRefs(mLocale));
  NS_ENSURE_SUCCESS(rv, rv);

  // collation
  nsCOMPtr<nsICollationFactory> cfact = do_CreateInstance(
     NS_COLLATIONFACTORY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = cfact->CreateCollation(mLocale, getter_AddRefs(mCollation));
  NS_ENSURE_SUCCESS(rv, rv);

  // date formatter
  mDateFormatter = do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // annotation service - just ignore errors
  //mAnnotationService = do_GetService("@mozilla.org/annotation-service;1", &rv);

  // prefs
  LoadPrefs();

  // recent events hash tables
  NS_ENSURE_TRUE(mRecentTyped.Init(128), NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mRecentBookmark.Init(128), NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mRecentRedirects.Init(128), NS_ERROR_OUT_OF_MEMORY);

  rv = CreateLookupIndexes();
  if (NS_FAILED(rv))
    return rv;

  // The AddObserver calls must be the last lines in this function, because
  // this function may fail, and thus, this object would be not completely
  // initialized), but the observerservice would still keep a reference to us
  // and notify us about shutdown, which may cause crashes.

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrefBranch2> pbi = do_QueryInterface(mPrefBranch);
  if (pbi) {
    pbi->AddObserver(PREF_AUTOCOMPLETE_ONLY_TYPED, this, PR_FALSE);
    pbi->AddObserver(PREF_BROWSER_HISTORY_EXPIRE_DAYS, this, PR_FALSE);
  }

  observerService->AddObserver(this, gQuitApplicationMessage, PR_FALSE);
  observerService->AddObserver(this, gXpcomShutdown, PR_FALSE);

  if (doImport) {
    nsCOMPtr<nsIMorkHistoryImporter> importer = new nsMorkHistoryImporter();
    NS_ENSURE_TRUE(importer, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIFile> historyFile;
    rv = NS_GetSpecialDirectory(NS_APP_HISTORY_50_FILE,
                                getter_AddRefs(historyFile));
    if (NS_SUCCEEDED(rv) && historyFile) {
      importer->ImportHistory(historyFile, this);
    }
  }

  // Don't add code that can fail here! Do it up above, before we add our
  // observers.

  return NS_OK;
}


// nsNavHistory::InitDB
//

nsresult
nsNavHistory::InitDB(PRBool *aDoImport)
{
  nsresult rv;
  PRBool tableExists;
  *aDoImport = PR_FALSE;

  // init DB
  nsCOMPtr<nsIFile> dbFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbFile->Append(NS_LITERAL_STRING("places.sqlite"));
  NS_ENSURE_SUCCESS(rv, rv);

  mDBService = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBService->OpenDatabase(dbFile, getter_AddRefs(mDBConn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // delete the db and try opening again
    rv = dbFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBService->OpenDatabase(dbFile, getter_AddRefs(mDBConn));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the database page size. This will only have any effect on empty files,
  // so must be done before anything else. If the file already exists, we'll
  // get that file's page size and this will have no effect.
  nsCAutoString pageSizePragma("PRAGMA page_size=");
  pageSizePragma.AppendInt(DEFAULT_DB_PAGE_SIZE);
  rv = mDBConn->ExecuteSimpleSQL(pageSizePragma);
  NS_ENSURE_SUCCESS(rv, rv);

  // dummy database connection
  rv = mDBService->OpenDatabase(dbFile, getter_AddRefs(mDummyDBConn));
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // Initialize the other places services' database tables. We do this before:
  //
  // - Starting the dummy statement, because once the dummy statement has
  //   started we can't modify the schema. Stopping and re-starting the
  //   dummy statement is pretty heavyweight
  //
  // - Creating our statements. Some of our statements depend on these external
  //   tables, such as the bookmarks or favicon tables.
  rv = nsNavBookmarks::InitTables(mDBConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = nsFaviconService::InitTables(mDBConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = nsAnnotationService::InitTables(mDBConn);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the page size. This may be different than was set above if the database
  // file already existed and has a different page size.
  PRInt32 pageSize;
  {
    nsCOMPtr<mozIStorageStatement> statement;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("PRAGMA page_size"),
                                  getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = statement->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(hasResult, NS_ERROR_FAILURE);
    pageSize = statement->AsInt32(0);
  }

  // compute the size of the database cache
  PRInt32 cachePercentage;
  if (NS_FAILED(mPrefBranch->GetIntPref(PREF_DB_CACHE_PERCENTAGE,
                                        &cachePercentage)))
    cachePercentage = DEFAULT_DB_CACHE_PERCENTAGE;
  if (cachePercentage > 50)
    cachePercentage = 50; // sanity check, don't take too much
  if (cachePercentage < 0)
    cachePercentage = 0;
  PRInt64 cacheSize = PR_GetPhysicalMemorySize() * cachePercentage / 100;
  PRInt64 cachePages = cacheSize / pageSize;

  // set the cache size
  nsCAutoString cacheSizePragma("PRAGMA cache_size=");
  cacheSizePragma.AppendInt(cachePages);
  rv = mDBConn->ExecuteSimpleSQL(cacheSizePragma);
  NS_ENSURE_SUCCESS(rv, rv);

  // moz_places
  rv = mDBConn->TableExists(NS_LITERAL_CSTRING("moz_places"), &tableExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! tableExists) {
    *aDoImport = PR_TRUE;
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_places ("
        "id INTEGER PRIMARY KEY, "
        "url LONGVARCHAR, "
        "title LONGVARCHAR, "
        "user_title LONGVARCHAR, "
        "rev_host LONGVARCHAR, "
        "visit_count INTEGER DEFAULT 0, "
        "hidden INTEGER DEFAULT 0 NOT NULL, "
        "typed INTEGER DEFAULT 0 NOT NULL, "
        "favicon_id INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("CREATE INDEX moz_places_urlindex ON moz_places (url)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // FIXME: this should be moved inside the moz_places table creation block.
  // It is left outside and the return value is ignored because alpha 1 did not
  // have this index. When it is likely that all alpha users have run a more
  // recent build, we can move this to only happen on init so that startup time
  // is faster. This index is used for favicon expiration, see
  // nsNavHistoryExpire::ExpireItems
  mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX moz_places_faviconindex ON moz_places (favicon_id)"));

  // moz_historyvisits
  rv = mDBConn->TableExists(NS_LITERAL_CSTRING("moz_historyvisits"), &tableExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! tableExists) {
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_historyvisits ("
        "id INTEGER PRIMARY KEY, "
        "from_visit INTEGER, "
        "place_id INTEGER, "
        "visit_date INTEGER, "
        "visit_type INTEGER, "
        "session INTEGER)"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("CREATE INDEX moz_historyvisits_pageindex ON moz_historyvisits (place_id)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // This must be outside of the visit creation above because the alpha1 shipped
  // without this index. This makes a big difference in startup time for
  // large profiles because of finding bookmark redirects using the referring
  // page. For final release, if we think everybody running alpha1 has run
  // alpha2 or later, we can put it in the if statement above for faster
  // startup time (same as above for the moz_places_faviconindex)
  mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX moz_historyvisits_fromindex ON moz_historyvisits (from_visit)"));

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // --- PUT SCHEMA-MODIFYING THINGS (like create table) ABOVE THIS LINE ---

  // now that the schema has been finalized, we can initialize the dummy stmt.
  rv = StartDummyStatement();
  NS_ENSURE_SUCCESS(rv, rv);

  // This causes the database data to be preloaded up to the maximum cache size
  // set above. This dramatically speeds up some later operations. Failures
  // here are not fatal since we can run fine without this. It must happen
  // after the dummy statement is running for the cache to be initialized.
  if (cachePages > 0) {
    rv = mDBConn->Preload();
    if (NS_FAILED(rv))
      NS_WARNING("Preload of database failed");
  }

  // DO NOT PUT ANY SCHEMA-MODIFYING THINGS HERE
  return NS_OK;
}


// nsNavHistory::InitStatements
//
//    Called after InitDB, this creates our stored statements

nsresult
nsNavHistory::InitStatements()
{
  nsresult rv;

  // mDBGetURLPageInfo
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, (SELECT title FROM moz_bookmarks WHERE "
      "item_child = h.id), h.rev_host, h.visit_count "
      "FROM moz_places h "
      "WHERE h.url = ?1"),
    getter_AddRefs(mDBGetURLPageInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetURLPageInfoFull
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, (SELECT title FROM moz_bookmarks WHERE "
      "item_child = h.id), h.rev_host, h.visit_count, "
        "(SELECT MAX(visit_date) FROM moz_historyvisits WHERE place_id = h.id), "
        "f.url "
      "FROM moz_places h "
      "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
      "WHERE h.url = ?1 "),
    getter_AddRefs(mDBGetURLPageInfoFull));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetIdPageInfo
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, (SELECT title FROM moz_bookmarks WHERE "
      "item_child = h.id), h.rev_host, h.visit_count "
      "FROM moz_places h WHERE h.id = ?1"),
                                getter_AddRefs(mDBGetIdPageInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetIdPageInfoFull
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, (SELECT title FROM moz_bookmarks WHERE "
        "item_child = h.id), h.rev_host, h.visit_count, "
        "(SELECT MAX(visit_date) FROM moz_historyvisits WHERE place_id = h.id), "
        "f.url "
      "FROM moz_places h "
      "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
      "WHERE h.id = ?1"),
    getter_AddRefs(mDBGetIdPageInfoFull));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBFullAutoComplete
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.url, h.title, h.visit_count, h.typed "
      "FROM moz_places h "
      "JOIN moz_historyvisits v ON h.id = v.place_id "
      "WHERE h.hidden <> 1 "
      "GROUP BY h.id "
      "ORDER BY h.visit_count "
      "LIMIT ?1"),
    getter_AddRefs(mDBFullAutoComplete));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBRecentVisitOfURL
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT v.id, v.session "
      "FROM moz_places h JOIN moz_historyvisits v ON h.id = v.place_id "
      "WHERE h.url = ?1 "
      "ORDER BY v.visit_date DESC "
      "LIMIT 1"),
    getter_AddRefs(mDBRecentVisitOfURL));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBInsertVisit
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO moz_historyvisits "
      "(from_visit, place_id, visit_date, visit_type, session) "
      "VALUES (?1, ?2, ?3, ?4, ?5)"),
    getter_AddRefs(mDBInsertVisit));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetPageVisitStats (see InternalAdd)
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id, visit_count, typed, hidden "
      "FROM moz_places "
      "WHERE url = ?1"),
    getter_AddRefs(mDBGetPageVisitStats));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBUpdatePageVisitStats (see InternalAdd)
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places "
      "SET visit_count = ?2, hidden = ?3, typed = ?4 "
      "WHERE id = ?1"),
    getter_AddRefs(mDBUpdatePageVisitStats));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBAddNewPage (see InternalAddNewPage)
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO moz_places "
      "(url, title, rev_host, hidden, typed, visit_count) "
      "VALUES (?1, ?2, ?3, ?4, ?5, ?6)"),
    getter_AddRefs(mDBAddNewPage));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBVisitToURLResult, should match kGetInfoIndex_* (see GetQueryResults)
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, (SELECT title FROM moz_bookmarks WHERE "
        "item_child = h.id), h.rev_host, h.visit_count, "
        "(SELECT MAX(visit_date) FROM moz_historyvisits WHERE place_id = h.id), "
        "f.url, null "
      "FROM moz_places h "
      "JOIN moz_historyvisits v ON h.id = v.place_id "
      "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
      "WHERE v.id = ?1"),
    getter_AddRefs(mDBVisitToURLResult));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBVisitToVisitResult, should match kGetInfoIndex_* (see GetQueryResults)
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, (SELECT title FROM moz_bookmarks WHERE "
             "item_child = h.id), h.rev_host, h.visit_count, "
             "v.visit_date, f.url, v.session "
      "FROM moz_places h "
      "JOIN moz_historyvisits v ON h.id = v.place_id "
      "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
      "WHERE v.id = ?1"),
    getter_AddRefs(mDBVisitToVisitResult));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBUrlToURLResult, should match kGetInfoIndex_*
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, (SELECT title FROM moz_bookmarks WHERE "
        "item_child = h.id), h.rev_host, h.visit_count, "
        "(SELECT MAX(visit_date) FROM moz_historyvisits WHERE place_id = h.id), "
        "f.url, null "
      "FROM moz_places h "
      "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
      "WHERE h.url = ?1"),
    getter_AddRefs(mDBUrlToUrlResult));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


#ifdef IN_MEMORY_LINKS
// nsNavHistory::InitMemDB
//
//    Should be called after InitDB

nsresult
nsNavHistory::InitMemDB()
{
  nsresult rv = mDBService->OpenSpecialDatabase("memory", getter_AddRefs(mMemDBConn));
  NS_ENSURE_SUCCESS(rv, rv);

  // create our table and index
  rv = mMemDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE moz_memhistory (url LONGVARCHAR UNIQUE)"));
  NS_ENSURE_SUCCESS(rv, rv);

  // prepackaged statements
  rv = mMemDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT url FROM moz_memhistory WHERE url = ?1"),
      getter_AddRefs(mMemDBGetPage));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mMemDBConn->CreateStatement(
      NS_LITERAL_CSTRING("INSERT OR IGNORE INTO moz_memhistory VALUES (?1)"),
      getter_AddRefs(mMemDBAddPage));
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the URLs over: sort by URL because the original table already has
  // and index. We can therefor not spend time sorting the whole thing another
  // time by inserting in order.
  nsCOMPtr<mozIStorageStatement> selectStatement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("SELECT url FROM moz_places WHERE visit_count > 0 ORDER BY url"),
                                getter_AddRefs(selectStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  //rv = mMemDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("BEGIN TRANSACTION"));
  mozStorageTransaction transaction(mMemDBConn, PR_FALSE);
  nsCString url;
  while(NS_SUCCEEDED(rv = selectStatement->ExecuteStep(&hasMore)) && hasMore) {
    rv = selectStatement->GetUTF8String(0, url);
    if (NS_SUCCEEDED(rv) && ! url.IsEmpty()) {
      rv = mMemDBAddPage->BindUTF8StringParameter(0, url);
      if (NS_SUCCEEDED(rv))
        mMemDBAddPage->Execute();
    }
  }
  transaction.Commit();

  return NS_OK;
}
#endif


// nsNavHistory::StartDummyStatement
//
//    sqlite page caches are discarded when a statement is complete. This sucks
//    for things like history queries where we do many small reads. This means
//    that for every small transaction, we have to re-read from disk (or the OS
//    cache) all pages associated with that transaction.
//
//    To get around this, we keep a different connection. This dummy connection
//    has a statement that stays open and thus keeps its pager cache in memory.
//    When the shared pager cache is enabled before either connection has been
//    opened (this is done by the storage service on DB init), our main
//    connection will get the same pager cache, which will be persisted.
//
//    HOWEVER, when a statement is open on a database, it is disallowed to
//    change the schema of the database (add or modify tables or indices).
//    We deal with this in two ways. First, for initialization, all the
//    services that depend on the places connection are told to create their
//    tables using static functions. This happens before StartDummyStatement
//    is called in InitDB so that this problem doesn't happen.
//
//    If some service needs to change the schema for some reason after this,
//    they can call StopDummyStatement which will terminate the statement and
//    clear the cache. This will allow the database schema to be modified, but
//    will have bad performance implications (because the cache will need to
//    be re-loaded). It is also possible for some buggy function to leave a
//    statement open that will prevent modifictation of the DB.

nsresult
nsNavHistory::StartDummyStatement()
{
  nsresult rv;
  NS_ASSERTION(mDummyDBConn, "The dummy connection should have been set up by Init");

  // do nothing if the dummy statement is already running
  if (mDBDummyStatement)
    return NS_OK;

  // Make sure the dummy table exists
  PRBool tableExists;
  rv = mDBConn->TableExists(NS_LITERAL_CSTRING("moz_dummy_table"), &tableExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! tableExists) {
    rv = mDBConn->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("CREATE TABLE moz_dummy_table (id INTEGER PRIMARY KEY)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // This table is guaranteed to have something in it and will keep the dummy
  // statement open. If the table is empty, it won't hold the statement open.
  // the PRIMARY KEY value on ID means that it is unique. The OR IGNORE means
  // that if there is already a value of 1 there, this insert will be ignored,
  // which is what we want so as to avoid growing the table infinitely.
  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("INSERT OR IGNORE INTO moz_dummy_table VALUES (1)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDummyDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id FROM moz_dummy_table LIMIT 1"),
    getter_AddRefs(mDBDummyStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // we have to step the dummy statement so that it will hold a lock on the DB
  PRBool dummyHasResults;
  rv = mDBDummyStatement->ExecuteStep(&dummyHasResults);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavHistory::StopDummyStatement
//
//    @see StartDummyStatement for how this works.
//
//    It is very important that if the dummy statement is ever stopped, that
//    it is restarted as soon as possible, or else the whole browser will run
//    without DB cache, which will slow everything down.

nsresult
nsNavHistory::StopDummyStatement()
{
  // do nothing if the dummy statement isn't running
  if (! mDBDummyStatement)
    return NS_OK;

  nsresult rv = mDBDummyStatement->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  mDBDummyStatement = nsnull;
  return NS_OK;
}


// nsNavHistory::SaveExpandItem
//
//    This adds an item to the persistent list of items that should be
//    expanded.

void
nsNavHistory::SaveExpandItem(const nsAString& aTitle)
{
  gExpandedItems.Put(aTitle, 1);
}


// nsNavHistory::GetUrlIdFor
//
//    Called by the bookmarks and annotation services, this function returns the
//    ID of the row for the given URL, optionally creating one if it doesn't
//    exist. A newly created entry will have no visits.
//
//    If aAutoCreate is false and the item doesn't exist, the entry ID will be
//    zero.
//
//    This DOES NOT check for bad URLs other than that they're nonempty.

nsresult
nsNavHistory::GetUrlIdFor(nsIURI* aURI, PRInt64* aEntryID,
                          PRBool aAutoCreate)
{
  *aEntryID = 0;

  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  mozStorageStatementScoper statementResetter(mDBGetURLPageInfo);
  nsresult rv = BindStatementURI(mDBGetURLPageInfo, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasEntry = PR_FALSE;
  rv = mDBGetURLPageInfo->ExecuteStep(&hasEntry);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasEntry) {
    return mDBGetURLPageInfo->GetInt64(kGetInfoIndex_PageID, aEntryID);
  } else if (aAutoCreate) {
    // create a new hidden, untyped, unvisited entry
    mDBGetURLPageInfo->Reset();
    statementResetter.Abandon();
    nsString voidString;
    voidString.SetIsVoid(PR_TRUE);
    rv = InternalAddNewPage(aURI, voidString, PR_TRUE, PR_FALSE, 0, aEntryID);
    if (NS_SUCCEEDED(rv))
      transaction.Commit();
    return rv;
  } else {
    // Doesn't exist: don't do anything, entry ID was already set to 0 above
    return NS_OK;
  }
}


// nsNavHistory::SaveCollapseItem
//
//    For now, just remove things that should be collapsed. Maybe in the
//    future we'll want to save things that have been explicitly collapsed
//    versus things that were just never expanded in the first place. This could
//    be done by doing a put with a value of 0.

void
nsNavHistory::SaveCollapseItem(const nsAString& aTitle)
{
  gExpandedItems.Remove(aTitle);
}


// nsNavHistory::InternalAddNewPage
//
//    Adds a new page to the DB. THIS SHOULD BE THE ONLY PLACE NEW ROWS ARE
//    CREATED. This allows us to maintain better consistency.
//
//    If non-null, the new page ID will be placed into aPageID.

nsresult
nsNavHistory::InternalAddNewPage(nsIURI* aURI, const nsAString& aTitle,
                                 PRBool aHidden, PRBool aTyped,
                                 PRInt32 aVisitCount, PRInt64* aPageID)
{
  mozStorageStatementScoper scoper(mDBAddNewPage);
  nsresult rv = BindStatementURI(mDBAddNewPage, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  if (aTitle.IsVoid()) {
    // if no title is specified, make up a title based on the filename
    nsAutoString title;
    GenerateTitleFromURI(aURI, title);
    rv = mDBAddNewPage->BindStringParameter(1,
        StringHead(title, HISTORY_TITLE_LENGTH_MAX));
  } else {
    rv = mDBAddNewPage->BindStringParameter(1,
        StringHead(aTitle, HISTORY_TITLE_LENGTH_MAX));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // host (reversed with trailing period)
  nsAutoString revHost;
  rv = GetReversedHostname(aURI, revHost);
  // Not all URI types have hostnames, so this is optional.
  if (NS_SUCCEEDED(rv)) {
    rv = mDBAddNewPage->BindStringParameter(2, revHost);
  } else {
    rv = mDBAddNewPage->BindNullParameter(2);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // hidden
  rv = mDBAddNewPage->BindInt32Parameter(3, aHidden);
  NS_ENSURE_SUCCESS(rv, rv);

  // typed
  rv = mDBAddNewPage->BindInt32Parameter(4, aTyped);
  NS_ENSURE_SUCCESS(rv, rv);

  // visit count
  rv = mDBAddNewPage->BindInt32Parameter(5, aVisitCount);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBAddNewPage->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // If the caller wants the page ID, go get it
  if (aPageID) {
    rv = mDBConn->GetLastInsertRowID(aPageID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsNavHistory::InternalAddVisit
//
//    Just a wrapper for inserting a new visit in the DB.

nsresult
nsNavHistory::InternalAddVisit(PRInt64 aPageID, PRInt64 aReferringVisit,
                               PRInt64 aSessionID, PRTime aTime,
                               PRInt32 aTransitionType, PRInt64* visitID)
{
  nsresult rv;
  mozStorageStatementScoper scoper(mDBInsertVisit);

  rv = mDBInsertVisit->BindInt64Parameter(0, aReferringVisit);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBInsertVisit->BindInt64Parameter(1, aPageID);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBInsertVisit->BindInt64Parameter(2, aTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBInsertVisit->BindInt32Parameter(3, aTransitionType);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBInsertVisit->BindInt64Parameter(4, aSessionID);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBInsertVisit->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return mDBConn->GetLastInsertRowID(visitID);
}


// nsNavHistory::FindLastVisit
//
//    This finds the most recent visit to the given URL. If found, it will put
//    that visit's ID and session into the respective out parameters and return
//    true. Returns false if no visit is found.
//
//    This is used to compute the referring visit.

PRBool
nsNavHistory::FindLastVisit(nsIURI* aURI, PRInt64* aVisitID,
                            PRInt64* aSessionID)
{
  mozStorageStatementScoper scoper(mDBRecentVisitOfURL);
  nsresult rv = BindStatementURI(mDBRecentVisitOfURL, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore;
  rv = mDBRecentVisitOfURL->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  if (hasMore) {
    *aVisitID = mDBRecentVisitOfURL->AsInt64(0);
    *aSessionID = mDBRecentVisitOfURL->AsInt64(1);
    return PR_TRUE;
  }
  return PR_FALSE;
}


// nsNavHistory::IsURIStringVisited
//
//    Takes a URL as a string and returns true if we've visited it.
//
//    Be careful to always reset the statement since it will be reused.

PRBool nsNavHistory::IsURIStringVisited(const nsACString& aURIString)
{
#ifdef IN_MEMORY_LINKS
  // check the memory DB
  nsresult rv = mMemDBGetPage->BindUTF8StringParameter(0, aURIString);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  PRBool hasPage = PR_FALSE;
  mMemDBGetPage->ExecuteStep(&hasPage);
  mMemDBGetPage->Reset();
  return hasPage;
#else

#ifdef LAZY_ADD
  // check the lazy list to see if this has recently been added
  for (PRUint32 i = 0; i < mLazyMessages.Length(); i ++) {
    if (mLazyMessages[i].type == LazyMessage::Type_AddURI) {
      if (aURIString.Equals(mLazyMessages[i].uriSpec))
        return PR_TRUE;
    }
  }
#endif

  // check the main DB
  nsresult rv;
  mozStorageStatementScoper statementResetter(mDBGetURLPageInfo);
  rv = mDBGetURLPageInfo->BindUTF8StringParameter(0, aURIString);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  PRBool hasMore = PR_FALSE;
  rv = mDBGetURLPageInfo->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  if (! hasMore)
    return PR_FALSE;

  // Actually get the result to make sure the visit count > 0.  there are
  // several ways that we can get pages with visit counts of 0, and those
  // should not count.
  PRInt32 visitCount;
  rv = mDBGetURLPageInfo->GetInt32(kGetInfoIndex_VisitCount, &visitCount);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  return visitCount > 0;
#endif
}


// nsNavHistory::LoadPrefs

nsresult
nsNavHistory::LoadPrefs()
{
  if (! mPrefBranch)
    return NS_OK;

  mPrefBranch->GetIntPref(PREF_BROWSER_HISTORY_EXPIRE_DAYS, &mExpireDays);
  PRBool oldCompleteOnlyTyped = mAutoCompleteOnlyTyped;
  mPrefBranch->GetBoolPref(PREF_AUTOCOMPLETE_ONLY_TYPED,
                           &mAutoCompleteOnlyTyped);
  if (oldCompleteOnlyTyped != mAutoCompleteOnlyTyped) {
    // update the autocomplete statement if the option has changed.
    nsresult rv = CreateAutoCompleteQuery();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}


// nsNavHistory::GetNow
//
//    This is a hack to avoid calling PR_Now() too often, as is the case when
//    we're asked the ageindays of many history entries in a row. A timer is
//    set which will clear our valid flag after a short timeout.

PRTime
nsNavHistory::GetNow()
{
  if (!mNowValid) {
    mLastNow = PR_Now();
    mNowValid = PR_TRUE;
    if (!mExpireNowTimer)
      mExpireNowTimer = do_CreateInstance("@mozilla.org/timer;1");

    if (mExpireNowTimer)
      mExpireNowTimer->InitWithFuncCallback(expireNowTimerCallback, this,
                                            HISTORY_EXPIRE_NOW_TIMEOUT,
                                            nsITimer::TYPE_ONE_SHOT);
  }

  return mLastNow;
}


// nsNavHistory::expireNowTimerCallback

void nsNavHistory::expireNowTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsNavHistory* history = NS_STATIC_CAST(nsNavHistory*, aClosure);
  history->mNowValid = PR_FALSE;
  history->mExpireNowTimer = nsnull;
}

static PRTime
NormalizeTimeRelativeToday(PRTime aTime)
{
  // round to midnight this morning
  PRExplodedTime explodedTime;
  PR_ExplodeTime(aTime, PR_LocalTimeParameters, &explodedTime);

  // set to midnight (0:00)
  explodedTime.tm_min =
    explodedTime.tm_hour =
    explodedTime.tm_sec =
    explodedTime.tm_usec = 0;

  return PR_ImplodeTime(&explodedTime);
}

// nsNavHistory::NormalizeTime
//
//    Converts a nsINavHistoryQuery reference+offset time into a PRTime
//    relative to the epoch.
//
//    It is important that this function NOT use the current time optimization.
//    It is called to update queries, and we really need to know what right
//    now is because those incoming values will also have current times that
//    we will have to compare against.

PRTime // static
nsNavHistory::NormalizeTime(PRUint32 aRelative, PRTime aOffset)
{
  PRTime ref;
  switch (aRelative)
  {
    case nsINavHistoryQuery::TIME_RELATIVE_EPOCH:
      return aOffset;
    case nsINavHistoryQuery::TIME_RELATIVE_TODAY:
      ref = NormalizeTimeRelativeToday(PR_Now());
      break;
    case nsINavHistoryQuery::TIME_RELATIVE_NOW:
      ref = PR_Now();
      break;
    default:
      NS_NOTREACHED("Invalid relative time");
      return 0;
  }
  return ref + aOffset;
}

// nsNavHistory::CanLiveUpdateQuery
//
//    Returns true if this set of queries/options can be live-updated. That is,
//    we can look at a node and compare its attributes to the query and easily
//    tell whether it belongs in the result set or not.
//
//    QUERYUPDATE_TIME:
//      This query is only limited by an inclusive time range on the first
//      query object. The caller can quickly evaluate the time itself if it
//      chooses. This is even simpler than "simple" below.
//    QUERYUPDATE_SIMPLE:
//      This query is evaluatable using EvaluateQueryForNode to do live
//      updating.
//    QUERYUPDATE_COMPLEX:
//      This query is not evaluatable using EvaluateQueryForNode. When something
//      happens that this query updates, you will need to re-run the query.
//    QUERYUPDATE_COMPLEX_WITH_BOOKMARKS:
//      A complex query that additionally has dependence on bookmarks. All
//      bookmark-dependent queries fall under this category.
//
//    aHasSearchTerms will be set to true if the query has any dependence on
//    keywords. When there is no dependence on keywords, we can handle title
//    change operations as simple instead of complex.

PRUint32
nsNavHistory::GetUpdateRequirements(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                                    nsNavHistoryQueryOptions* aOptions,
                                    PRBool* aHasSearchTerms)
{
  NS_ASSERTION(aQueries.Count() > 0, "Must have at least one query");

  // first check if there are search terms
  *aHasSearchTerms = PR_FALSE;
  PRInt32 i;
  for (i = 0; i < aQueries.Count(); i ++) {
    aQueries[i]->GetHasSearchTerms(aHasSearchTerms);
    if (*aHasSearchTerms)
      break;
  }

  // Whenever there is a maximum number of results, we must requery. This
  // is because we can't generally know if any given addition/change causes
  // the item to be in the top N items in the database.
  if (aOptions->MaxResults() > 0)
    return QUERYUPDATE_COMPLEX;

  PRBool nonTimeBasedItems = PR_FALSE;
  for (i = 0; i < aQueries.Count(); i ++) {
    nsNavHistoryQuery* query = aQueries[i];

    if (query->Folders().Length() > 0 || query->OnlyBookmarked()) {
      return QUERYUPDATE_COMPLEX_WITH_BOOKMARKS;
    }
    // Note: we don't currently have any complex non-bookmarked items, but these
    // are expected to be added. Put detection of these items here.
    if (! query->SearchTerms().IsEmpty() ||
        ! query->Domain().IsVoid() ||
        query->Uri() != nsnull)
      nonTimeBasedItems = PR_TRUE;
  }

  if (aQueries.Count() == 1 && ! nonTimeBasedItems)
    return QUERYUPDATE_TIME;
  return QUERYUPDATE_SIMPLE;
}


// nsNavHistory::EvaluateQueryForNode
//
//    This runs the node through the given queries to see if satisfies the
//    query conditions. Not every query parameters are handled by this code,
//    but we handle the most common ones so that performance is better.
//
//    We assume that the time on the node is the time that we want to compare.
//    This is not necessarily true because URL nodes have the last access time,
//    which is not necessarily the same. However, since this is being called
//    to update the list, we assume that the last access time is the current
//    access time that we are being asked to compare so it works out.
//
//    Returns true if node matches the query, false if not.

PRBool
nsNavHistory::EvaluateQueryForNode(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                                   nsNavHistoryQueryOptions* aOptions,
                                   nsNavHistoryResultNode* aNode)
{
  // lazily created from the node's string when we need to match URIs
  nsCOMPtr<nsIURI> nodeUri;

  for (PRInt32 i = 0; i < aQueries.Count(); i ++) {
    PRBool hasIt;
    nsCOMPtr<nsNavHistoryQuery> query = aQueries[i];

    // --- begin time ---
    query->GetHasBeginTime(&hasIt);
    if (hasIt) {
      PRTime beginTime = NormalizeTime(query->BeginTimeReference(),
                                       query->BeginTime());
      if (aNode->mTime < beginTime)
        continue; // before our time range
    }

    // --- end time ---
    query->GetHasEndTime(&hasIt);
    if (hasIt) {
      PRTime endTime = NormalizeTime(query->EndTimeReference(),
                                     query->EndTime());
      if (aNode->mTime > endTime)
        continue; // after our time range
    }

    // --- search terms ---
    if (! query->SearchTerms().IsEmpty()) {
      // we can use the existing filtering code, just give it our one object in
      // an array.
      nsCOMArray<nsNavHistoryResultNode> inputSet;
      inputSet.AppendObject(aNode);
      nsCOMArray<nsNavHistoryResultNode> filteredSet;
      nsresult rv = FilterResultSet(inputSet, &filteredSet, query->SearchTerms());
      if (NS_FAILED(rv))
        continue;
      if (! filteredSet.Count())
        continue; // did not make it through the filter, doesn't match
    }

    // --- domain/host matching ---
    query->GetHasDomain(&hasIt);
    if (hasIt) {
      if (! nodeUri) {
        // lazy creation of nodeUri, which might be checked for multiple queries
        if (NS_FAILED(NS_NewURI(getter_AddRefs(nodeUri), aNode->mURI)))
          continue;
      }
      nsCAutoString host;
      if (NS_FAILED(nodeUri->GetAsciiHost(host)))
        continue;
      nsCAutoString asciiRequest;
      if (NS_FAILED(AsciiHostNameFromHostString(query->Domain(), asciiRequest)))
        continue;

      if (query->DomainIsHost()) {
        if (! asciiRequest.Equals(host))
          continue; // host names don't match
      }
      // check domain names
      nsCAutoString domain;
      DomainNameFromHostName(host, domain);
      if (! asciiRequest.Equals(domain))
        continue; // domain names don't match
    }

    // --- URI matching ---
    if (query->Uri()) {
      if (! nodeUri) { // lazy creation of nodeUri
        if (NS_FAILED(NS_NewURI(getter_AddRefs(nodeUri), aNode->mURI)))
          continue;
      }
      if (! query->UriIsPrefix()) {
        // easy case: the URI is an exact match
        PRBool equals;
        nsresult rv = query->Uri()->Equals(nodeUri, &equals);
        NS_ENSURE_SUCCESS(rv, rv);
        if (! equals)
          continue;
      } else {
        // harder case: match prefix, note that we need to get the ASCII string
        // from the node's parsed URI instead of using the node's mUrl string,
        // because that might not be normalized
        nsCAutoString nodeUriString;
        nodeUri->GetAsciiSpec(nodeUriString);
        nsCAutoString queryUriString;
        query->Uri()->GetAsciiSpec(queryUriString);
        if (queryUriString.Length() > nodeUriString.Length())
          continue; // not long enough to match as prefix
        nodeUriString.SetLength(queryUriString.Length());
        if (! nodeUriString.Equals(queryUriString))
          continue; // prefixes don't match
      }
    }

    // If we ever make it to the bottom of this loop, that means it passed all
    // tests for the given query. Since queries are ORed together, that means
    // it passed everything and we are done.
    return PR_TRUE;
  }

  // didn't match any query
  return PR_FALSE;
}


// nsNavHistory::AsciiHostNameFromHostString
//
//    We might have interesting encodings and different case in the host name.
//    This will convert that host name into an ASCII host name by sending it
//    through the URI canonicalization. The result can be used for comparison
//    with other ASCII host name strings.

nsresult // static
nsNavHistory::AsciiHostNameFromHostString(const nsACString& aHostName,
                                          nsACString& aAscii)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHostName);
  NS_ENSURE_SUCCESS(rv, rv);
  return uri->GetAsciiHost(aAscii);
}


// nsNavHistory::DomainNameFromHostName
//
//    This does the www.mozilla.org -> mozilla.org and
//    foo.theregister.co.uk -> theregister.co.uk conversion

void // static
nsNavHistory::DomainNameFromHostName(const nsCString& aHostName,
                                     nsACString& aDomainName)
{
  if (IsNumericHostName(aHostName)) {
    // easy case
    aDomainName = aHostName;
  } else {
    // compute the toplevel domain name
    PRInt32 tldLength = GetTLDCharCount(aHostName);
    if (tldLength < PRInt32(aHostName.Length())) {
      // bugzilla.mozilla.org : tldLength = 3, topDomain = mozilla.org
      GetSubstringFromNthDot(aHostName,
                             aHostName.Length() - tldLength - 2,
                             1, PR_FALSE, aDomainName);
    }
  }
}


// Nav history *****************************************************************


// nsNavHistory::GetHasHistoryEntries

NS_IMETHODIMP
nsNavHistory::GetHasHistoryEntries(PRBool* aHasEntries)
{
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  nsresult rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT id FROM moz_historyvisits LIMIT 1"),
      getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  return dbSelectStatement->ExecuteStep(aHasEntries);
}


// nsNavHistory::SetPageUserTitle

NS_IMETHODIMP
nsNavHistory::SetPageUserTitle(nsIURI* aURI, const nsAString& aUserTitle)
{
  return SetPageTitleInternal(aURI, PR_TRUE, aUserTitle);
}


// nsNavHistory::MarkPageAsFollowedBookmark
//
//    @see MarkPageAsTyped

NS_IMETHODIMP
nsNavHistory::MarkPageAsFollowedBookmark(nsIURI* aURI)
{
  // don't add when history is disabled
  if (IsHistoryDisabled())
    return NS_OK;

  nsCAutoString uriString;
  aURI->GetSpec(uriString);

  // if URL is already in the queue, then we need to remove the old one
  PRInt64 unusedEventTime;
  if (mRecentBookmark.Get(uriString, &unusedEventTime))
    mRecentBookmark.Remove(uriString);

  if (mRecentBookmark.Count() > RECENT_EVENT_QUEUE_MAX_LENGTH)
    ExpireNonrecentEvents(&mRecentBookmark);

  mRecentTyped.Put(uriString, GetNow());
  return NS_OK;
}


// nsNavHistory::CanAddURI
//
//    Filter out unwanted URIs such as "chrome:", "mailbox:", etc.
//
//    The model is if we don't know differently then add which basically means
//    we are suppose to try all the things we know not to allow in and then if
//    we don't bail go on and allow it in.

nsresult
nsNavHistory::CanAddURI(nsIURI* aURI, PRBool* canAdd)
{
  nsresult rv;

  nsCString scheme;
  rv = aURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  // first check the most common cases (HTTP, HTTPS) to allow in to avoid most
  // of the work
  if (scheme.EqualsLiteral("http")) {
    *canAdd = PR_TRUE;
    return NS_OK;
  }
  if (scheme.EqualsLiteral("https")) {
    *canAdd = PR_TRUE;
    return NS_OK;
  }

  // now check for all bad things
  if (scheme.EqualsLiteral("about") ||
      scheme.EqualsLiteral("imap") ||
      scheme.EqualsLiteral("news") ||
      scheme.EqualsLiteral("mailbox") ||
      scheme.EqualsLiteral("moz-anno") ||
      scheme.EqualsLiteral("view-source") ||
      scheme.EqualsLiteral("chrome") ||
      scheme.EqualsLiteral("data")) {
    *canAdd = PR_FALSE;
    return NS_OK;
  }
  *canAdd = PR_TRUE;
  return NS_OK;
}


// nsNavHistory::SetPageDetails

NS_IMETHODIMP
nsNavHistory::SetPageDetails(nsIURI* aURI, const nsAString& aTitle,
                             const nsAString& aUserTitle, PRUint32 aVisitCount,
                             PRBool aHidden, PRBool aTyped)
{
  // look up the page ID, creating a new one if necessary
  PRInt64 pageID;
  nsresult rv = GetUrlIdFor(aURI, &pageID, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> statement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places "
      "SET title = ?2, "
          "user_title = ?3, "
          "visit_count = ?4, "
          "hidden = ?5, "
          "typed = ?6 "
       "WHERE id = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(0, pageID);
  NS_ENSURE_SUCCESS(rv, rv);

  // for the titles, be careful to interpret isVoid as NULL SQL command so that
  // we can tell the difference between "set to empty" and "unset"
  if (aTitle.IsVoid())
    statement->BindNullParameter(1);
  else
    statement->BindStringParameter(1, StringHead(aTitle, HISTORY_TITLE_LENGTH_MAX));
  NS_ENSURE_SUCCESS(rv, rv);
  if (aUserTitle.IsVoid())
    statement->BindNullParameter(2);
  else
    statement->BindStringParameter(2, StringHead(aUserTitle, HISTORY_TITLE_LENGTH_MAX));
  NS_ENSURE_SUCCESS(rv, rv);

  statement->BindInt32Parameter(3, aVisitCount);
  NS_ENSURE_SUCCESS(rv, rv);
  statement->BindInt32Parameter(4, aHidden ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, rv);
  statement->BindInt32Parameter(5, aTyped ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavHistory::AddVisit
//
//    Adds or updates a page with the given URI. The ID of the new visit will
//    be put into aVisitID.
//
//    THE RETURNED NEW VISIT ID MAY BE 0 indicating that this page should not be
//    added to the history.

NS_IMETHODIMP
nsNavHistory::AddVisit(nsIURI* aURI, PRTime aTime, PRInt64 aReferringVisit,
                       PRInt32 aTransitionType, PRBool aIsRedirect,
                       PRInt64 aSessionID, PRInt64* aVisitID)
{
  nsresult rv;

  // Filter out unwanted URIs, silently failing
  PRBool canAdd = PR_FALSE;
  rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    *aVisitID = 0;
    return NS_OK;
  }

  // in-memory version
#ifdef IN_MEMORY_LINKS
  rv = BindStatementURI(mMemDBAddPage, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  mMemDBAddPage->Execute();
#endif

  // This will prevent corruption since we have to do a two-phase add.
  // Generally this won't do anything because AddURI has its own transaction.
  mozStorageTransaction transaction(mDBConn, PR_FALSE,
                                  mozIStorageConnection::TRANSACTION_EXCLUSIVE);

  // see if this is an update (revisit) or a new page
  mozStorageStatementScoper scoper(mDBGetPageVisitStats);
  rv = BindStatementURI(mDBGetPageVisitStats, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool alreadyVisited = PR_TRUE;
  rv = mDBGetPageVisitStats->ExecuteStep(&alreadyVisited);

  PRInt64 pageID = 0;
  PRBool hidden;
  PRBool typed;
  PRBool newItem = PR_FALSE; // used to send out notifications at the end
  if (alreadyVisited) {
    // Update the existing entry...

    rv = mDBGetPageVisitStats->GetInt64(0, &pageID);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 oldVisitCount = 0;
    rv = mDBGetPageVisitStats->GetInt32(1, &oldVisitCount);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 oldTypedState = 0;
    rv = mDBGetPageVisitStats->GetInt32(2, &oldTypedState);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 oldHiddenState = 0;
    rv = mDBGetPageVisitStats->GetInt32(3, &oldHiddenState);
    NS_ENSURE_SUCCESS(rv, rv);

    // free the previous statement before we make a new one
    mDBGetPageVisitStats->Reset();
    scoper.Abandon();

    // embedded links and redirects will be hidden, but don't hide pages that
    // are already unhidden.
    //
    // Note that we test the redirect flag and not for the redirect transition
    // type. The transition type refers to how we got here, and whether a page
    // is shown does not depend on whether you got to it through a redirect.
    // Rather, we want to hide pages that do not themselves redirect somewhere
    // else, which is what the redirect flag means.
    hidden = oldHiddenState;
    if (hidden && ! aIsRedirect &&
        aTransitionType != nsINavHistoryService::TRANSITION_EMBED)
      hidden = PR_FALSE; // unhide

    typed = oldTypedState || (aTransitionType == TRANSITION_TYPED);

    // some items may have a visit count of 0 which will not count for link
    // visiting, so be sure to note this transition
    if (oldVisitCount == 0)
      newItem = PR_TRUE;

    // update with new stats
    mozStorageStatementScoper updateScoper(mDBUpdatePageVisitStats);
    rv = mDBUpdatePageVisitStats->BindInt64Parameter(0, pageID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBUpdatePageVisitStats->BindInt32Parameter(1, oldVisitCount + 1);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBUpdatePageVisitStats->BindInt32Parameter(2, hidden);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBUpdatePageVisitStats->BindInt32Parameter(3, typed);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBUpdatePageVisitStats->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // New page
    newItem = PR_TRUE;

    // free the previous statement before we make a new one
    mDBGetPageVisitStats->Reset();
    scoper.Abandon();

    // hide embedded links and redirects, everything else is visible,
    // See the hidden computation code above for a little more explanation.
    hidden = (aTransitionType == TRANSITION_EMBED || aIsRedirect);

    typed = (aTransitionType == TRANSITION_TYPED);

    // set as visited once, no title
    nsString voidString;
    voidString.SetIsVoid(PR_TRUE);
    rv = InternalAddNewPage(aURI, voidString, hidden, typed, 1, &pageID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = InternalAddVisit(pageID, aReferringVisit, aSessionID, aTime,
                        aTransitionType, aVisitID);

  // Notify observers: The hidden detection code must match that in
  // GetQueryResults to maintain consistency.
  // FIXME bug 325241: make a way to observe hidden URLs
  transaction.Commit();
  if (! hidden && aTransitionType != TRANSITION_EMBED) {
    ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryObserver,
                        OnVisit(aURI, *aVisitID, aTime, aSessionID,
                                aReferringVisit, aTransitionType));
  }

  // Normally docshell send the link visited observer notification for us (this
  // will tell all the documents to update their visited link coloring).
  // However, for redirects (since we implement nsIGlobalHistory3) this will
  // not happen and we need to send it ourselves.
  if (newItem && aIsRedirect) {
    nsCOMPtr<nsIObserverService> obsService =
      do_GetService("@mozilla.org/observer-service;1");
    if (obsService)
      obsService->NotifyObservers(aURI, NS_LINK_VISITED_EVENT_TOPIC, nsnull);
  }

  return NS_OK;
}


// nsNavHistory::GetNewQuery

NS_IMETHODIMP nsNavHistory::GetNewQuery(nsINavHistoryQuery **_retval)
{
  *_retval = new nsNavHistoryQuery();
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval);
  return NS_OK;
}

// nsNavHistory::GetNewQueryOptions

NS_IMETHODIMP nsNavHistory::GetNewQueryOptions(nsINavHistoryQueryOptions **_retval)
{
  *_retval = new nsNavHistoryQueryOptions();
  NS_ENSURE_TRUE(*_retval, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*_retval);
  return NS_OK;
}

// nsNavHistory::ExecuteQuery
//

NS_IMETHODIMP
nsNavHistory::ExecuteQuery(nsINavHistoryQuery *aQuery, nsINavHistoryQueryOptions *aOptions,
                           nsINavHistoryResult** _retval)
{
  return ExecuteQueries(&aQuery, 1, aOptions, _retval);
}


// nsNavHistory::ExecuteQueries
//
//    This function is actually very simple, we just create the proper root node (either
//    a bookmark folder or a complex query node) and assign it to the result. The node
//    will then populate itself accordingly.
//
//    Quick overview of query operation: When you call this function, we will construct
//    the correct container node and set the options you give it. This node will then
//    fill itself. Folder nodes will call nsNavBookmarks::QueryFolderChildren, and
//    all other queries will call GetQueryResults. If these results contain other
//    queries, those will be populated when the container is opened.

NS_IMETHODIMP
nsNavHistory::ExecuteQueries(nsINavHistoryQuery** aQueries, PRUint32 aQueryCount,
                             nsINavHistoryQueryOptions *aOptions,
                             nsINavHistoryResult** _retval)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aQueries);
  NS_ENSURE_ARG_POINTER(aOptions);
  if (! aQueryCount)
    return NS_ERROR_INVALID_ARG;

  // concrete options
  nsCOMPtr<nsNavHistoryQueryOptions> options = do_QueryInterface(aOptions);
  NS_ENSURE_TRUE(options, NS_ERROR_INVALID_ARG);

  // concrete queries array
  nsCOMArray<nsNavHistoryQuery> queries;
  for (PRUint32 i = 0; i < aQueryCount; i ++) {
    nsCOMPtr<nsNavHistoryQuery> query = do_QueryInterface(aQueries[i], &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    queries.AppendObject(query);
  }

  // root node
  nsRefPtr<nsNavHistoryContainerResultNode> rootNode;
  PRInt64 folderId = GetSimpleBookmarksQueryFolder(queries);
  if (folderId) {
    // In the simple case where we're just querying children of a single bookmark
    // folder, we can more efficiently generate results.
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
    nsCOMPtr<nsNavHistoryResultNode> tempRootNode;
    rv = bookmarks->ResultNodeForFolder(folderId, options,
                                        getter_AddRefs(tempRootNode));
    NS_ENSURE_SUCCESS(rv, rv);
    rootNode = tempRootNode->GetAsContainer();
  } else {
    // complex query
    rootNode = new nsNavHistoryQueryResultNode(EmptyCString(), EmptyCString(),
                                               queries, options);
    NS_ENSURE_TRUE(rootNode, NS_ERROR_OUT_OF_MEMORY);
  }

  // result object
  nsRefPtr<nsNavHistoryResult> result;
  rv = nsNavHistoryResult::NewHistoryResult(aQueries, aQueryCount, options, rootNode,
                                            getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = result);
  return NS_OK;
}


// nsNavHistory::GetQueryResults
//
//    Call this to get the results from a complex query. This is used by
//    nsNavHistoryQueryResultNode to populate its children. For simple bookmark
//    queries, use nsNavBookmarks::QueryFolderChildren.
//
//    THIS DOES NOT DO SORTING. You will need to sort the container yourself
//    when you get the results. This is because sorting depends on tree
//    statistics that will be built from the perspective of the tree. See
//    nsNavHistoryQueryResultNode::FillChildren
//
//    FIXME: This only does keyword searching for the first query, and does
//    it ANDed with the all the rest of the queries.

nsresult
nsNavHistory::GetQueryResults(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                              nsNavHistoryQueryOptions *aOptions,
                              nsCOMArray<nsNavHistoryResultNode>* aResults)
{
  NS_ENSURE_ARG_POINTER(aOptions);
  NS_ASSERTION(aResults->Count() == 0, "Initial result array must be empty");
  if (! aQueries.Count())
    return NS_ERROR_INVALID_ARG;

  nsresult rv;

  PRInt32 sortingMode = aOptions->SortingMode();
  if (sortingMode < 0 ||
      sortingMode > nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING) {
    return NS_ERROR_INVALID_ARG;
  }

  PRBool asVisits =
    (aOptions->ResultType() == nsINavHistoryQueryOptions::RESULTS_AS_VISIT ||
     aOptions->ResultType() == nsINavHistoryQueryOptions::RESULTS_AS_FULL_VISIT);

  nsCAutoString commonConditionsForHistory;

  if (! aOptions->IncludeHidden()) {
    // The hiding code here must match the notification behavior in AddVisit
    commonConditionsForHistory.AssignLiteral("hidden <> 1 ");

    // Some items are unhidden but are subframe navigations that we shouldn't
    // show. This happens especially on imported profiles because the previous
    // history system didn't hide as many things as we do now. Some sites,
    // especially Javascript-heavy ones, load things in frames to display them,
    // resulting in a lot of these entries. This filters those visits out.
    if (asVisits)
      commonConditionsForHistory.AppendLiteral("AND v.visit_type <> 4 "); // not TRANSITION_EMBED
  }

  // Query string: Output parameters should be in order of kGetInfoIndex_*
  // WATCH OUT: nsNavBookmarks::Init also creates some statements that share
  // these same indices for passing to RowToResult. If you add something to
  // this, you also need to update the bookmark statements to keep them in
  // sync!
  nsCAutoString queryString;
  nsCAutoString groupBy;
  if (asVisits) {
    // if we want visits, this is easy, just combine all possible matches
    // between the history and visits table and do our query.
    // FIXME(brettw) Add full visit info
    queryString = NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, (SELECT title from moz_bookmarks WHERE "
             "item_child = h.id), h.rev_host, h.visit_count, "
             "v.visit_date, f.url, v.session "
      "FROM moz_places h "
      "JOIN moz_historyvisits v ON h.id = v.place_id "
      "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
      "WHERE ");
  } else {
    // For URLs, it is more complicated, because we want each URL once. The
    // GROUP BY clause gives us this. To get the max visit time, we populate
    // one column by using a nested SELECT on the visit table. Also, ignore
    // session information.
    // FIXME(brettw) add nulls for full visit info
    queryString = NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, (SELECT title FROM moz_bookmarks WHERE "
        "item_child = h.id), h.rev_host, h.visit_count, "
        "(SELECT MAX(visit_date) FROM moz_historyvisits WHERE place_id = h.id), "
        "f.url, null "
      "FROM moz_places h "
      "LEFT OUTER JOIN moz_historyvisits v ON h.id = v.place_id "
      "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
      "WHERE ");
    groupBy = NS_LITERAL_CSTRING(" GROUP BY h.id");
  }

  PRInt32 numParameters = 0;
  nsCAutoString conditions;
  PRInt32 i;
  for (i = 0; i < aQueries.Count(); i ++) {
    nsCString queryClause;
    PRInt32 clauseParameters = 0;
    rv = QueryToSelectClause(aQueries[i], numParameters,
                             &queryClause, &clauseParameters,
                             aQueries[i]->OnlyBookmarked() ? 
                               EmptyCString() : commonConditionsForHistory);
    NS_ENSURE_SUCCESS(rv, rv);
    if (! queryClause.IsEmpty()) {
      if (! conditions.IsEmpty()) // exists previous clause: multiple ones are ORed
        conditions += NS_LITERAL_CSTRING(" OR ");
      conditions += NS_LITERAL_CSTRING("(") + queryClause +
        NS_LITERAL_CSTRING(")");
      numParameters += clauseParameters;
    }
  }

  // in cases where there were no queries, we need to use the common conditions
  // (normally these are appended to each clause that are not annotation-based)
  if (! conditions.IsEmpty()) {
    queryString += conditions;
  } else {
    queryString += commonConditionsForHistory;
  }
  queryString += groupBy;

  // Sort clause: we will sort later, but if it comes out of the DB sorted,
  // our later sort will be basically free. The DB can sort these for free
  // most of the time anyway, because it has indices over these items.
  //
  // FIXME: do some performance tests, I'm not sure that the indices are getting
  // used, in which case we should just remove this except when there are max
  // results.
  switch(sortingMode) {
    case nsINavHistoryQueryOptions::SORT_BY_NONE:
      break;
    case nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING:
    case nsINavHistoryQueryOptions::SORT_BY_TITLE_DESCENDING:
      // the DB doesn't have indices on titles, and we need to do special
      // sorting for locales. This type of sorting is done only at the end.
      //
      // If the user wants few results, we limit them by date, necessitating
      // a sort by date here (see the IDL definition for maxResults). We'll
      // still do the official sort by title later.
      if (aOptions->MaxResults() > 0)
        queryString += NS_LITERAL_CSTRING(" ORDER BY v.visit_date DESC");
      break;
    case nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY v.visit_date ASC");
      break;
    case nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY v.visit_date DESC");
      break;
    case nsINavHistoryQueryOptions::SORT_BY_URI_ASCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY h.url ASC");
      break;
    case nsINavHistoryQueryOptions::SORT_BY_URI_DESCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY h.url DESC");
      break;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY h.visit_count ASC");
      break;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING:
      queryString += NS_LITERAL_CSTRING(" ORDER BY h.visit_count DESC");
      break;
    default:
      NS_NOTREACHED("Invalid sorting mode");
  }

  // limit clause if there are 'maxResults'
  if (aOptions->MaxResults() > 0) {
    queryString += NS_LITERAL_CSTRING(" LIMIT ");
    queryString.AppendInt(aOptions->MaxResults());
    queryString.AppendLiteral(" ");
  }

#ifdef DEBUG_thunder
  printf("Constructed the query: %s\n", PromiseFlatCString(queryString).get());
#endif

  // Put this in a transaction. Even though we are only reading, this will
  // speed up the grouped queries to the annotation service for titles and
  // full text searching.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // create statement
  nsCOMPtr<mozIStorageStatement> statement;
  rv = mDBConn->CreateStatement(queryString, getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  // bind parameters
  numParameters = 0;
  for (i = 0; i < aQueries.Count(); i ++) {
    PRInt32 clauseParameters = 0;
    rv = BindQueryClauseParameters(statement, numParameters,
                                   aQueries[i], &clauseParameters);
    NS_ENSURE_SUCCESS(rv, rv);
    numParameters += clauseParameters;
  }

  PRBool hasSearchTerms;
  rv = aQueries[0]->GetHasSearchTerms(&hasSearchTerms);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 groupCount;
  const PRUint32 *groupings = aOptions->GroupingMode(&groupCount);

  if (groupCount == 0 && ! hasSearchTerms) {
    // optimize the case where we just want a list with no grouping: this
    // directly fills in the results and we avoid a copy of the whole list
    rv = ResultsAsList(statement, aOptions, aResults);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // generate the toplevel results
    nsCOMArray<nsNavHistoryResultNode> toplevel;
    rv = ResultsAsList(statement, aOptions, &toplevel);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasSearchTerms) {
      // keyword search
      if (groupCount == 0) {
        // keyword search with no grouping: can filter directly into the result
        FilterResultSet(toplevel, aResults, aQueries[0]->SearchTerms());
      } else {
        // keyword searching with grouping: need intermediate filtered results
        nsCOMArray<nsNavHistoryResultNode> filteredResults;
        FilterResultSet(toplevel, &filteredResults, aQueries[0]->SearchTerms());
        rv = RecursiveGroup(filteredResults, groupings, groupCount,
                            aResults);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    } else {
      // group unfiltered results
      rv = RecursiveGroup(toplevel, groupings, groupCount, aResults);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // automatically expand things that were expanded before
  // FIXME(brettw)
  //if (gExpandedItems.Count() > 0)
  //  result->ApplyTreeState(gExpandedItems);

  return NS_OK;
}


// nsNavHistory::AddObserver

NS_IMETHODIMP
nsNavHistory::AddObserver(nsINavHistoryObserver* aObserver, PRBool aOwnsWeak)
{
  return mObservers.AppendWeakElement(aObserver, aOwnsWeak);
}


// nsNavHistory::RemoveObserver

NS_IMETHODIMP
nsNavHistory::RemoveObserver(nsINavHistoryObserver* aObserver)
{
  return mObservers.RemoveWeakElement(aObserver);
}


// nsNavHistory::BeginUpdateBatch

NS_IMETHODIMP
nsNavHistory::BeginUpdateBatch()
{
  mBatchesInProgress ++;
  if (mBatchesInProgress == 1) {
    ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryObserver,
                        OnBeginUpdateBatch())
  }
  return NS_OK;
}


// nsNavHistory::EndUpdateBatch

NS_IMETHODIMP
nsNavHistory::EndUpdateBatch()
{
  if (mBatchesInProgress == 0)
    return NS_ERROR_FAILURE;
  if (--mBatchesInProgress == 0) {
    ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryObserver, OnEndUpdateBatch())
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::GetHistoryDisabled(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = IsHistoryDisabled();
  return NS_OK;
}

// Browser history *************************************************************


// nsNavHistory::AddPageWithDetails
//
//    This function is used by the migration components to import history.
//
//    Note that this always adds the page with one visit and no parent, which
//    is appropriate for imported URIs.
//
//    UNTESTED

NS_IMETHODIMP
nsNavHistory::AddPageWithDetails(nsIURI *aURI, const PRUnichar *aTitle,
                                 PRInt64 aLastVisited)
{
  PRInt64 visitID;
  nsresult rv = AddVisit(aURI, aLastVisited, 0, TRANSITION_LINK, PR_FALSE,
                         0, &visitID);
  NS_ENSURE_SUCCESS(rv, rv);

  return SetPageTitleInternal(aURI, PR_FALSE, nsString(aTitle));
}


// nsNavHistory::GetLastPageVisited
//
//    This was once used when the new window is set to "previous page." It
//    doesn't seem to be used anymore, so we don't spend any time precompiling
//    the statement.

NS_IMETHODIMP
nsNavHistory::GetLastPageVisited(nsACString & aLastPageVisited)
{
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.url "
      "FROM moz_places h LEFT OUTER JOIN moz_historyvisits v ON h.id = v.place_id "
      "WHERE v.visit_date IN "
      "(SELECT MAX(visit_date) "
       "FROM moz_historyvisits v2 LEFT JOIN moz_places h2 ON v2.place_id = h2.id "
        "WHERE h2.hidden != 1)"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMatch = PR_FALSE;
  if (NS_SUCCEEDED(statement->ExecuteStep(&hasMatch)) && hasMatch) {
    return statement->GetUTF8String(0, aLastPageVisited);
  }
  aLastPageVisited.Truncate(0);
  return NS_OK;
}


// nsNavHistory::GetCount
//
//    This function is used in legacy code to see if there is any history to
//    clear. Counting the actual number of history entries is very slow, so
//    we just see if there are any and return 0 or 1, which is enough to make
//    all the code that uses this function happy.

NS_IMETHODIMP
nsNavHistory::GetCount(PRUint32 *aCount)
{
  PRBool hasEntries = PR_FALSE;
  nsresult rv = GetHasHistoryEntries(&hasEntries);
  if (hasEntries)
    *aCount = 1;
  else
    *aCount = 0;
  return rv;
}


// nsNavHistory::RemovePage
//
//    Removes all visits and the main history entry for the given URI.
//    Silently fails if we have no knowledge of the page.

NS_IMETHODIMP
nsNavHistory::RemovePage(nsIURI *aURI)
{
  nsresult rv;
  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  nsCOMPtr<mozIStorageStatement> statement;

  // delete all visits for this page
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_historyvisits WHERE place_id IN (SELECT id FROM moz_places WHERE url = ?1)"),
      getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = BindStatementURI(statement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  nsNavBookmarks* bookmarksService = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarksService, NS_ERROR_FAILURE);
  PRBool bookmarked = PR_FALSE;
  rv = bookmarksService->IsBookmarked(aURI, &bookmarked);
  NS_ENSURE_SUCCESS(rv, rv);

  if (! bookmarked) {
    // Only delete everything else if the page is not bookmarked.  Note that we
    // do NOT delete favicons. Any unreferenced favicons will be deleted next
    // time the browser is shut down.

    // FIXME: delete annotations

    // delete main history entries
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM moz_places WHERE url = ?1"),
        getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = BindStatementURI(statement, 0, aURI);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Observers: Be sure to finish transaction before calling observers. Note also
  // that we always call the observers even though we aren't sure something
  // actually got deleted.
  transaction.Commit();
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryObserver, OnDeleteURI(aURI))
  return NS_OK;
}


// nsNavHistory::RemovePagesFromHost
//
//    This function will delete all history information about pages from a
//    given host. If aEntireDomain is set, we will also delete pages from
//    sub hosts (so if we are passed in "microsoft.com" we delete
//    "www.microsoft.com", "msdn.microsoft.com", etc.). An empty host name
//    means local files and anything else with no host name. You can also pass
//    in the localized "(local files)" title given to you from a history query.
//
//    Silently fails if we have no knowledge of the host
//
//    This function is actually pretty simple, it just boils down to a DELETE
//    statement, but is out of control due to observers and the two types of
//    similar delete operations that we need to supports.

NS_IMETHODIMP
nsNavHistory::RemovePagesFromHost(const nsACString& aHost, PRBool aEntireDomain)
{
  nsresult rv;
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // Local files don't have any host name. We don't want to delete all files in
  // history when we get passed an empty string, so force to exact match
  if (aHost.IsEmpty())
    aEntireDomain = PR_FALSE;

  // translate "(local files)" to an empty host name
  // be sure to use the TitleForDomain to get the localized name
  nsCString localFiles;
  TitleForDomain(EmptyCString(), localFiles);
  nsAutoString host16;
  if (! aHost.Equals(localFiles))
    host16 = NS_ConvertUTF8toUTF16(aHost);

  // nsISupports version of the host string for passing to observers
  nsCOMPtr<nsISupportsString> hostSupports(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = hostSupports->SetData(host16);
  NS_ENSURE_SUCCESS(rv, rv);

  // see BindQueryClauseParameters for how this host selection works
  nsAutoString revHostDot;
  GetReversedHostname(host16, revHostDot);
  NS_ASSERTION(revHostDot[revHostDot.Length() - 1] == '.', "Invalid rev. host");
  nsAutoString revHostSlash(revHostDot);
  revHostSlash.Truncate(revHostSlash.Length() - 1);
  revHostSlash.Append(NS_LITERAL_STRING("/"));

  // how we are selecting host names
  nsCAutoString conditionString;
  if (aEntireDomain)
    conditionString.AssignLiteral("WHERE h.rev_host >= ?1 AND h.rev_host < ?2 ");
  else
    conditionString.AssignLiteral("WHERE h.rev_host = ?1");

  // Tell the observers about the non-hidden items we are about to delete.
  // Since this is a two-step process, if we get an error, we may tell them we
  // will delete something but then not actually delete it. Too bad.
  //
  // Note also that we *include* bookmarked items here. We generally want to
  // send out delete notifications for bookmarked items since in general,
  // deleting the visits (like we always do) will cause the item to disappear
  // from history views. This will also cause all visit dates to be deleted,
  // which affects many bookmark views
  nsCStringArray deletedURIs;
  nsCOMPtr<mozIStorageStatement> statement;

  // create statement depending on delete type
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT url FROM moz_places h ") + conditionString,
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(0, revHostDot);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aEntireDomain) {
    rv = statement->BindStringParameter(1, revHostSlash);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRBool hasMore = PR_FALSE;
  while ((statement->ExecuteStep(&hasMore) == NS_OK) && hasMore) {
    nsCAutoString thisURIString;
    if (NS_FAILED(statement->GetUTF8String(0, thisURIString)) || 
        thisURIString.IsEmpty())
      continue; // no URI
    if (! deletedURIs.AppendCString(thisURIString))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // first, delete all the visits
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_historyvisits WHERE place_id IN (SELECT id FROM moz_places h ") + conditionString + NS_LITERAL_CSTRING(")"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(0, revHostDot);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aEntireDomain) {
    rv = statement->BindStringParameter(1, revHostSlash);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // FIXME: delete annotations

  // now, delete the actual history entries that are not bookmarked
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_places WHERE id IN "
      "(SELECT id from moz_places h "
      " LEFT OUTER JOIN moz_bookmarks b ON h.id = b.item_child ")
      + conditionString +
      NS_LITERAL_CSTRING("AND b.item_child IS NULL)"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringParameter(0, revHostDot);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aEntireDomain) {
    rv = statement->BindStringParameter(1, revHostSlash);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  transaction.Commit();

  // notify observers
  UpdateBatchScoper batch(*this); // sends Begin/EndUpdateBatch to obsrvrs.
  if (deletedURIs.Count()) {
    nsCOMPtr<nsIURI> thisURI;
    for (PRUint32 observerIndex = 0; observerIndex < mObservers.Length();
         observerIndex ++) {
      const nsCOMPtr<nsINavHistoryObserver> &obs = mObservers.ElementAt(observerIndex);
      if (! obs)
        continue;

      // send it all the URIs
      for (PRInt32 i = 0; i < deletedURIs.Count(); i ++) {
        if (NS_FAILED(NS_NewURI(getter_AddRefs(thisURI), *deletedURIs[i],
                                nsnull, nsnull)))
          continue; // bad URI
        obs->OnDeleteURI(thisURI);
      }
    }
  }
  return NS_OK;
}


// nsNavHistory::RemoveAllPages
//
//    This function is used to clear history.

NS_IMETHODIMP
nsNavHistory::RemoveAllPages()
{
  // expire everything
  mExpire.ClearHistory();

  // Compress DB. Currently commented out because compression is very slow.
  // Deleted data will be overwritten with 0s by sqlite. Note that we have to
  // stop the dummy statement before doing this.
#if 0
  StopDummyStatement();
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM"));
  StartDummyStatement();
  NS_ENSURE_SUCCESS(rv, rv);
#endif
  return NS_OK;
}


// nsNavHistory::HidePage
//
//    Sets the 'hidden' column to true. If we've not heard of the page, we
//    succeed and do nothing.

NS_IMETHODIMP
nsNavHistory::HidePage(nsIURI *aURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
  /*
  // for speed to save disk accesses
  mozStorageTransaction transaction(mDBConn, PR_FALSE,
                                  mozIStorageConnection::TRANSACTION_EXCLUSIVE);

  // We need to do a query anyway to see if this URL is already in the DB.
  // Might as well ask for the hidden column to save updates in some cases.
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  nsresult rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT id,hidden FROM moz_places WHERE url = ?1"),
      getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = BindStatementURI(dbSelectStatement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool alreadyVisited = PR_TRUE;
  rv = dbSelectStatement->ExecuteStep(&alreadyVisited);
  NS_ENSURE_SUCCESS(rv, rv);

  // don't need to do anything if we've never heard of this page
  if (!alreadyVisited)
    return NS_OK;
 
  // modify the existing page if necessary

  PRInt32 oldHiddenState = 0;
  rv = dbSelectStatement->GetInt32(1, &oldHiddenState);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!oldHiddenState)
    return NS_OK; // already marked as hidden, we're done

  // find the old ID, which can be found faster than long URLs
  PRInt32 entryid = 0;
  rv = dbSelectStatement->GetInt32(0, &entryid);
  NS_ENSURE_SUCCESS(rv, rv);

  // need to clear the old statement before we create a new one
  dbSelectStatement = nsnull;

  nsCOMPtr<mozIStorageStatement> dbModStatement;
  rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("UPDATE moz_places SET hidden = 1 WHERE id = ?1"),
      getter_AddRefs(dbModStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  dbModStatement->BindInt32Parameter(0, entryid);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbModStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // notify observers, finish transaction first
  transaction.Commit();
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryObserver,
                      OnPageChanged(aURI,
                                    nsINavHistoryObserver::ATTRIBUTE_HIDDEN,
                                    EmptyString()))

  return NS_OK;
  */
}


// nsNavHistory::MarkPageAsTyped
//
//    Just sets the typed column to true, which will make this page more likely
//    to float to the top of autocomplete suggestions.
//
//    We can get this notification for pages that have not yet been added to the
//    DB. This happens when you type a new URL. The AddURI is called only when
//    the page is successfully found. If we don't have an entry yet, we add
//    one for this page, marking it as typed but hidden, with a 0 visit count.
//    This will get updated when AddURI is called, and it will clear the hidden
//    flag for typed URLs.
//
//    @see MarkPageAsFollowedBookmark

NS_IMETHODIMP
nsNavHistory::MarkPageAsTyped(nsIURI *aURI)
{
  // don't add when history is disabled
  if (IsHistoryDisabled())
    return NS_OK;

  nsCAutoString uriString;
  aURI->GetSpec(uriString);

  // if URL is already in the typed queue, then we need to remove the old one
  PRInt64 unusedEventTime;
  if (mRecentTyped.Get(uriString, &unusedEventTime))
    mRecentTyped.Remove(uriString);

  if (mRecentTyped.Count() > RECENT_EVENT_QUEUE_MAX_LENGTH)
    ExpireNonrecentEvents(&mRecentTyped);

  mRecentTyped.Put(uriString, GetNow());
  return NS_OK;
}

// nsGlobalHistory2 ************************************************************


// nsNavHistory::AddURI
//
//    This is the main method of adding history entries.

NS_IMETHODIMP
nsNavHistory::AddURI(nsIURI *aURI, PRBool aRedirect,
                     PRBool aToplevel, nsIURI *aReferrer)
{
  // don't add when history is disabled
  if (IsHistoryDisabled())
    return NS_OK;

  PRTime now = PR_Now();

  nsresult rv;
#ifdef LAZY_ADD
  LazyMessage message;
  rv = message.Init(LazyMessage::Type_AddURI, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  message.isRedirect = aRedirect;
  message.isToplevel = aToplevel;
  if (aReferrer) {
    rv = aReferrer->Clone(getter_AddRefs(message.referrer));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  message.time = now;
  rv = AddLazyMessage(message);
  NS_ENSURE_SUCCESS(rv, rv);
#else
  rv = AddURIInternal(aURI, now, aRedirect, aToplevel, aReferrer);
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  mExpire.OnAddURI(now);

  return NS_OK;
}


// nsNavHistory::AddURIInternal
//
//    This does the work of AddURI so it can be done lazily.

nsresult
nsNavHistory::AddURIInternal(nsIURI* aURI, PRTime aTime, PRBool aRedirect,
                             PRBool aToplevel, nsIURI* aReferrer)
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  PRInt64 redirectBookmark = 0;
  PRInt64 visitID, sessionID;
  nsresult rv = AddVisitChain(aURI, aTime, aToplevel, aRedirect, aReferrer,
                              &visitID, &sessionID, &redirectBookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  // The bookmark cache of redirects may be out-of-date with this addition, so
  // we need to update it. The issue here is if they bookmark "mozilla.org" by
  // typing it in without ever having visited "www.mozilla.org". They will then
  // get redirected to the latter, and we need to add mozilla.org ->
  // www.mozilla.org to the bookmark hashtable.
  //
  // AddVisitChain will put the spec of a bookmarked URI if it encounters one
  // into bookmarkURI. If this is non-empty, we know that something has happened
  // with a bookmark and we should probably go update it.
  if (redirectBookmark) {
    nsNavBookmarks* bookmarkService = nsNavBookmarks::GetBookmarksService();
    if (bookmarkService) {
      PRTime now = GetNow();
      bookmarkService->AddBookmarkToHash(redirectBookmark,
                                         now - BOOKMARK_REDIRECT_TIME_THRESHOLD);
    }
  }

  return transaction.Commit();
}


// nsNavHistory::AddVisitChain
//
//    This function is sits between AddURI (which is called when a page is
//    visited) and AddVisit (which creates the DB entries) to figure out what
//    we should add and what are the detailed parameters that should be used
//    (like referring visit ID and typed/bookmarked state).
//
//    This function walks up the referring chain and recursively calls itself,
//    each time calling InternalAdd to create a new history entry. (When we
//    get notified of redirects, we don't actually add any history entries, just
//    save them in mRecentRedirects. This function will add all of them for a
//    given destination page when that page is actually visited.)
//    See GetRedirectFor for more information about how redirects work.
//
//    aRedirectBookmark should be empty when this function is first called. If
//    there are any redirects that are bookmarks the specs will be placed in
//    this buffer. The caller can then determine if any bookmarked items were
//    visited so it knows whether to update the bookmark service's redirect
//    hashtable.

nsresult
nsNavHistory::AddVisitChain(nsIURI* aURI, PRTime aTime,
                            PRBool aToplevel, PRBool aIsRedirect,
                            nsIURI* aReferrer, PRInt64* aVisitID,
                            PRInt64* aSessionID, PRInt64* aRedirectBookmark)
{
  PRUint32 transitionType = 0;
  PRInt64 referringVisit = 0;
  PRTime visitTime = 0;

  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString redirectSource;
  if (GetRedirectFor(spec, redirectSource, &visitTime, &transitionType)) {
    // this was a redirect: See GetRedirectFor for info on how this works
    nsCOMPtr<nsIURI> redirectURI;
    rv = NS_NewURI(getter_AddRefs(redirectURI), redirectSource);
    NS_ENSURE_SUCCESS(rv, rv);

    // remember if any redirect sources were bookmarked
    nsNavBookmarks* bookmarkService = nsNavBookmarks::GetBookmarksService();
    PRBool isBookmarked;
    if (bookmarkService &&
        NS_SUCCEEDED(bookmarkService->IsBookmarked(redirectURI, &isBookmarked))
        && isBookmarked) {
      GetUrlIdFor(redirectURI, aRedirectBookmark, PR_FALSE);
    }

    // Find the visit for the source. Note that we decrease the time counter,
    // which will ensure that the referrer and this page will appear in history
    // in the correct order. Since the times are in microseconds, it should not
    // normally be possible to get two pages within one microsecond of each
    // other so the referrer won't appear before a previous page viewed.
    rv = AddVisitChain(redirectURI, aTime - 1, aToplevel, PR_TRUE, aReferrer,
                       &referringVisit, aSessionID, aRedirectBookmark);
    NS_ENSURE_SUCCESS(rv, rv);

  } else if (aReferrer) {
    // If there is a referrer, we know you came from somewhere, either manually
    // or automatically. For toplevel windows, assume its manual and you want
    // to see this in history. For other things, it's some kind of embedded
    // navigation. This is true of images and other content the user doesn't
    // want to see in their history, but also of embedded frames that the user
    // navigated manually and probably DOES want to see in history.
    // Unfortunately, there isn't any easy way to distinguish these.
    //
    // Generally, it boils down to the problem of detecting whether a frame
    // content change is the result of a user action, which isn't well defined
    // since script could change a frame's source as a result of user request,
    // or just because it feels like loading a new ad. The "back" button will
    // undo either of these actions.
    if (aToplevel)
      transitionType = nsINavHistoryService::TRANSITION_LINK;
    else
      transitionType = nsINavHistoryService::TRANSITION_EMBED;

    // Note that here we should NOT use the GetNow function. That function
    // caches the value of "now" until next time the event loop runs. This
    // gives better performance, but here we may get many notifications without
    // running the event loop. We must preserve these events' ordering. This
    // most commonly happens on redirects.
    visitTime = PR_Now();

    // try to turn the referrer into a visit
    if (! FindLastVisit(aReferrer, &referringVisit, aSessionID)) {
      // we couldn't find a visit for the referrer, don't set it
      *aSessionID = GetNewSessionID();
    }
  } else {
    // When there is no referrer, we know the user must have gotten the link
    // from somewhere, so check our sources to see if it was recently typed or
    // has a bookmark selected. We don't handle drag-and-drop operations.
    if (CheckIsRecentEvent(&mRecentTyped, spec))
      transitionType = nsINavHistoryService::TRANSITION_TYPED;
    else if (CheckIsRecentEvent(&mRecentBookmark, spec))
      transitionType = nsINavHistoryService::TRANSITION_BOOKMARK;

    visitTime = PR_Now();
    *aSessionID = GetNewSessionID();
  }

  // this call will create the visit and create/update the page entry
  return AddVisit(aURI, visitTime, referringVisit, transitionType,
                  aIsRedirect, *aSessionID, aVisitID);
}


// nsNavHistory::IsVisited
//
//    Note that this ignores the "hidden" flag. This function just checks if the
//    given page is in the DB for link coloring. The "hidden" flag affects
//    the history list view and autocomplete.

NS_IMETHODIMP
nsNavHistory::IsVisited(nsIURI *aURI, PRBool *_retval)
{
  // if history is disabled, we can optimize
  if (IsHistoryDisabled()) {
    *_retval = PR_FALSE;
    return NS_OK;
  }

  nsCAutoString utf8URISpec;
  nsresult rv = aURI->GetSpec(utf8URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = IsURIStringVisited(utf8URISpec);
  return NS_OK;
}


// nsNavHistory::SetPageTitle
//
//    This sets the page "real" title. Use nsINavHistory::SetPageUserTitle to
//    set any user-defined title.
//
//    Note that we do not allow empty real titles and will silently ignore such
//    requests. When a URL is added we give it a default title based on the
//    URL. Most pages provide a title and it gets replaced to something better.
//    Some pages don't: some say <title></title>, and some don't have any title
//    element. In BOTH cases, we get SetPageTitle(URI, ""), but in both cases,
//    our default title is more useful to the user than "(no title)".
//
//    User titles will accept empty strings so the user can still manually
//    override it.

NS_IMETHODIMP
nsNavHistory::SetPageTitle(nsIURI *aURI,
                           const nsAString & aTitle)
{
  if (aTitle.IsEmpty())
    return NS_OK;

#ifdef LAZY_ADD
  LazyMessage message;
  nsresult rv = message.Init(LazyMessage::Type_Title, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  message.title = aTitle;
  return AddLazyMessage(message);
#else
  return SetPageTitleInternal(aURI, PR_FALSE, aTitle);
#endif
}

NS_IMETHODIMP
nsNavHistory::GetPageTitle(nsIURI *aURI, nsAString &aTitle)
{
  aTitle.Truncate(0);

  mozIStorageStatement *statement = DBGetURLPageInfo();
  mozStorageStatementScoper scope(statement);
  nsresult rv = BindStatementURI(statement, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);


  PRBool results;
  rv = statement->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!results) {
    aTitle.SetIsVoid(PR_TRUE);
    return NS_OK; // not found: return void string
  }

  return statement->GetString(nsNavHistory::kGetInfoIndex_Title, aTitle);
}


#ifndef MOZILLA_1_8_BRANCH
// nsNavHistory::GetURIGeckoFlags
//
//    FIXME: should we try to use annotations for this stuff?

NS_IMETHODIMP
nsNavHistory::GetURIGeckoFlags(nsIURI* aURI, PRUint32* aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistory::SetURIGeckoFlags
//
//    FIXME: should we try to use annotations for this stuff?

NS_IMETHODIMP
nsNavHistory::SetURIGeckoFlags(nsIURI* aURI, PRUint32 aFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

// nsIGlobalHistory3 ***********************************************************

// nsNavHistory::AddDocumentRedirect
//
//    This adds a redirect mapping from the destination of the redirect to the
//    source, time, and type. This mapping is used by GetRedirectFor when we
//    get a page added to reconstruct the redirects that happened when a page
//    is visited. See GetRedirectFor for more information

// this is the expiration callback function that deletes stale entries
PLDHashOperator PR_CALLBACK nsNavHistory::ExpireNonrecentRedirects(
    nsCStringHashKey::KeyType aKey, RedirectInfo& aData, void* aUserArg)
{
  PRInt64* threshold = NS_REINTERPRET_CAST(PRInt64*, aUserArg);
  if (aData.mTimeCreated < *threshold)
    return PL_DHASH_REMOVE;
  return PL_DHASH_NEXT;
}
NS_IMETHODIMP
nsNavHistory::AddDocumentRedirect(nsIChannel *aOldChannel,
                                  nsIChannel *aNewChannel,
                                  PRInt32 aFlags,
                                  PRBool aTopLevel)
{
  nsresult rv;
  nsCOMPtr<nsIURI> oldURI, newURI;
  rv = aOldChannel->GetURI(getter_AddRefs(oldURI));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aNewChannel->GetURI(getter_AddRefs(newURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString oldSpec, newSpec;
  rv = oldURI->GetSpec(oldSpec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = newURI->GetSpec(newSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mRecentRedirects.Count() > RECENT_EVENT_QUEUE_MAX_LENGTH) {
    // expire out-of-date ones
    PRInt64 threshold = PR_Now() - RECENT_EVENT_THRESHOLD;
    mRecentRedirects.Enumerate(ExpireNonrecentRedirects,
                               NS_REINTERPRET_CAST(void*, &threshold));
  }

  RedirectInfo info;

  // remove any old entries for this redirect destination
  if (mRecentRedirects.Get(newSpec, &info))
    mRecentRedirects.Remove(newSpec);

  // save the new redirect info
  info.mSourceURI = oldSpec;
  info.mTimeCreated = PR_Now();
  if (aFlags & nsIChannelEventSink::REDIRECT_TEMPORARY)
    info.mType = TRANSITION_REDIRECT_TEMPORARY;
  else
    info.mType = TRANSITION_REDIRECT_PERMANENT;
  mRecentRedirects.Put(newSpec, info);

  return NS_OK;
}


// nsIObserver *****************************************************************

NS_IMETHODIMP
nsNavHistory::Observe(nsISupports *aSubject, const char *aTopic,
                    const PRUnichar *aData)
{
  if (nsCRT::strcmp(aTopic, gQuitApplicationMessage) == 0) {
    if (gTldTypes) {
      delete gTldTypes;
      gTldTypes = nsnull;
    }
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefService =
      do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
      prefService->SavePrefFile(nsnull);

    // notify expiring system that we're quitting, it may want to do stuff
    mExpire.OnQuit();
    
    // notify the bookmarks service we're quitting
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
    (void)bookmarks->OnQuit();
  } else if (nsCRT::strcmp(aTopic, gXpcomShutdown) == 0) {
    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    observerService->RemoveObserver(this, gXpcomShutdown);
    observerService->RemoveObserver(this, gQuitApplicationMessage);
  } else if (nsCRT::strcmp(aTopic, "nsPref:changed") == 0) {
    PRInt32 oldDays = mExpireDays;
    LoadPrefs();
    if (oldDays != mExpireDays)
      mExpire.OnExpirationChanged();
  }

  return NS_OK;
}


// Lazy stuff ******************************************************************

#ifdef LAZY_ADD

// nsNavHistory::AddLazyLoadFaviconMessage

nsresult
nsNavHistory::AddLazyLoadFaviconMessage(nsIURI* aPage, nsIURI* aFavicon,
                                        PRBool aForceReload)
{
  LazyMessage message;
  nsresult rv = message.Init(LazyMessage::Type_Favicon, aPage);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aFavicon->Clone(getter_AddRefs(message.favicon));
  NS_ENSURE_SUCCESS(rv, rv);
  message.alwaysLoadFavicon = aForceReload;
  return AddLazyMessage(message);
}


// nsNavHistory::StartLazyTimer
//
//    This schedules flushing of the lazy message queue for the future.
//
//    If we already have timer set, we canel it and schedule a new timer in
//    the future. This saves you from having to wait if you open a bunch of
//    pages in a row. However, we don't want to defer too long, so we'll only
//    push it back MAX_LAZY_TIMER_DEFERMENTS times. After that we always
//    let the timer go the next time.

nsresult
nsNavHistory::StartLazyTimer()
{
  if (! mLazyTimer) {
    mLazyTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (! mLazyTimer)
      return NS_ERROR_OUT_OF_MEMORY;
  } else {
    if (mLazyTimerSet) {
      if (mLazyTimerDeferments >= MAX_LAZY_TIMER_DEFERMENTS) {
        // already set and we don't want to push it back any later, use that one
        return NS_OK;
      } else {
        // push back the active timer
        mLazyTimer->Cancel();
        mLazyTimerDeferments ++;
      }
    }
  }
  nsresult rv = mLazyTimer->InitWithFuncCallback(LazyTimerCallback, this,
                                                 LAZY_MESSAGE_TIMEOUT,
                                                 nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);
  mLazyTimerSet = PR_TRUE;
  return NS_OK;
}


// nsNavHistory::AddLazyMessage

nsresult
nsNavHistory::AddLazyMessage(const LazyMessage& aMessage)
{
  if (! mLazyMessages.AppendElement(aMessage))
    return NS_ERROR_OUT_OF_MEMORY;
  return StartLazyTimer();
}


// nsNavHistory::LazyTimerCallback

void // static
nsNavHistory::LazyTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsNavHistory* that = NS_STATIC_CAST(nsNavHistory*, aClosure);
  that->mLazyTimerSet = PR_FALSE;
  that->mLazyTimerDeferments = 0;
  that->CommitLazyMessages();
}


// nsNavHistory::CommitLazyMessages

void
nsNavHistory::CommitLazyMessages()
{
  mozStorageTransaction transaction(mDBConn, PR_TRUE);
  for (PRUint32 i = 0; i < mLazyMessages.Length(); i ++) {
    LazyMessage& message = mLazyMessages[i];
    switch (message.type) {
      case LazyMessage::Type_AddURI:
        AddURIInternal(message.uri, message.time, message.isRedirect,
                       message.isToplevel, message.referrer);
        break;
      case LazyMessage::Type_Title:
        SetPageTitleInternal(message.uri, PR_FALSE, message.title);
        break;
      case LazyMessage::Type_Favicon: {
        nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
        if (faviconService) {
          nsCString spec;
          message.uri->GetSpec(spec);
          faviconService->DoSetAndLoadFaviconForPage(message.uri,
                                                     message.favicon,
                                                     message.alwaysLoadFavicon);
        }
        break;
      }
      default:
        NS_NOTREACHED("Invalid lazy message type");
    }
  }
  mLazyMessages.Clear();
}
#endif // LAZY_ADD


// Query stuff *****************************************************************


// nsNavHistory::QueryToSelectClause
//
//    THE ORDER AND BEHAVIOR SHOULD BE IN SYNC WITH BindQueryClauseParameters
//
//    I don't check return values from the query object getters because there's
//    no way for those to fail.

nsresult
nsNavHistory::QueryToSelectClause(nsNavHistoryQuery* aQuery, // const
                                  PRInt32 aStartParameter,
                                  nsCString* aClause,
                                  PRInt32* aParamCount,
                                  const nsACString& aCommonConditions)
{
  PRBool hasIt;

  aClause->Truncate();
  *aParamCount = 0;
  nsCAutoString paramString;

  // begin time
  if (NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt) {
    parameterString(aStartParameter + *aParamCount, paramString);
    *aClause += NS_LITERAL_CSTRING("v.visit_date >= ") + paramString;
    (*aParamCount) ++;
  }

  // end time
  if (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt) {
    if (! aClause->IsEmpty())
      *aClause += NS_LITERAL_CSTRING(" AND ");
    parameterString(aStartParameter + *aParamCount, paramString);
    *aClause += NS_LITERAL_CSTRING("v.visit_date <= ") + paramString;
    (*aParamCount) ++;
  }

  // search terms FIXME

  // min and max visit count
  if (aQuery->MinVisits() >= 0) {
    if (! aClause->IsEmpty())
      *aClause += NS_LITERAL_CSTRING(" AND ");
    parameterString(aStartParameter + *aParamCount, paramString);
    *aClause += NS_LITERAL_CSTRING("h.visit_count >= ") + paramString;
    (*aParamCount) ++;
  }

  if (aQuery->MaxVisits() >= 0) {
    if (! aClause->IsEmpty())
      *aClause += NS_LITERAL_CSTRING(" AND ");
    parameterString(aStartParameter + *aParamCount, paramString);
    *aClause += NS_LITERAL_CSTRING("h.visit_count <= ") + paramString;
    (*aParamCount) ++;
  }

  // only bookmarked
  if (aQuery->OnlyBookmarked()) {
    if (! aClause->IsEmpty())
      *aClause += NS_LITERAL_CSTRING(" AND ");

    *aClause += NS_LITERAL_CSTRING("EXISTS (SELECT b.item_child FROM moz_bookmarks b WHERE b.item_child = h.id)");
  }

  // domain
  if (NS_SUCCEEDED(aQuery->GetHasDomain(&hasIt)) && hasIt) {
    if (! aClause->IsEmpty())
      *aClause += NS_LITERAL_CSTRING(" AND ");

    PRBool domainIsHost = PR_FALSE;
    aQuery->GetDomainIsHost(&domainIsHost);
    if (domainIsHost) {
      parameterString(aStartParameter + *aParamCount, paramString);
      *aClause += NS_LITERAL_CSTRING("h.rev_host = ") + paramString;
      aClause->Append(' ');
      (*aParamCount) ++;
    } else {
      // see domain setting in BindQueryClauseParameters for why we do this
      parameterString(aStartParameter + *aParamCount, paramString);
      *aClause += NS_LITERAL_CSTRING("h.rev_host >= ") + paramString;
      (*aParamCount) ++;

      parameterString(aStartParameter + *aParamCount, paramString);
      *aClause += NS_LITERAL_CSTRING(" AND h.rev_host < ") + paramString;
      aClause->Append(' ');
      (*aParamCount) ++;
    }
  }

  // URI
  //
  // Performance improvement: Selecting URI by prefixes this way is slow because
  // sqlite will not use indices when you use substring. Currently, there is
  // not really any use for URI queries, so this isn't worth optimizing a lot.
  // In the future, we could do a >=,<= thing like we do for domain names to
  // make it use the index.
  if (NS_SUCCEEDED(aQuery->GetHasUri(&hasIt)) && hasIt) {
    if (! aClause->IsEmpty())
      *aClause += NS_LITERAL_CSTRING(" AND ");

    nsCAutoString paramString;
    parameterString(aStartParameter + *aParamCount, paramString);
    (*aParamCount) ++;

    nsCAutoString match;
    if (aQuery->UriIsPrefix()) {
      // Prefix: want something of the form SUBSTR(h.url, 0, length(?1)) = ?1
      *aClause += NS_LITERAL_CSTRING("SUBSTR(h.url, 0, LENGTH(") +
        paramString + NS_LITERAL_CSTRING(")) = ") + paramString;
    } else {
      *aClause += NS_LITERAL_CSTRING("h.url = ") + paramString;
    }
    aClause->Append(' ');
  }

  // annotation
  aQuery->GetHasAnnotation(&hasIt);
  if (hasIt) {
    if (! aClause->IsEmpty())
      *aClause += NS_LITERAL_CSTRING(" AND ");

    nsCAutoString paramString;
    parameterString(aStartParameter + *aParamCount, paramString);
    (*aParamCount) ++;

    if (aQuery->AnnotationIsNot())
      aClause->AppendLiteral("NOT ");
    aClause->AppendLiteral("EXISTS (SELECT id FROM moz_annos anno JOIN moz_anno_attributes annoname ON anno.anno_attribute_id = annoname.id WHERE anno.place_id = h.id AND annoname.name = ");
    aClause->Append(paramString);
    aClause->AppendLiteral(") ");
    // annotation-based queries don't get the common conditions, so you get
    // all URLs with that annotation
  } else {
    if (!(aClause->IsEmpty() || aCommonConditions.IsEmpty()))
      *aClause += NS_LITERAL_CSTRING(" AND ");
    aClause->Append(aCommonConditions);
  }

  return NS_OK;
}


// nsNavHistory::BindQueryClauseParameters
//
//    THE ORDER AND BEHAVIOR SHOULD BE IN SYNC WITH QueryToSelectClause

nsresult
nsNavHistory::BindQueryClauseParameters(mozIStorageStatement* statement,
                                        PRInt32 aStartParameter,
                                        nsNavHistoryQuery* aQuery, // const
                                        PRInt32* aParamCount)
{
  nsresult rv;
  (*aParamCount) = 0;

  PRBool hasIt;

  // begin time
  if (NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt) {
    PRTime time = NormalizeTime(aQuery->BeginTimeReference(),
                                aQuery->BeginTime());
    rv = statement->BindInt64Parameter(aStartParameter + *aParamCount, time);
    NS_ENSURE_SUCCESS(rv, rv);
    (*aParamCount) ++;
  }

  // end time
  if (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt) {
    PRTime time = NormalizeTime(aQuery->EndTimeReference(),
                                aQuery->EndTime());
    rv = statement->BindInt64Parameter(aStartParameter + *aParamCount, time);
    NS_ENSURE_SUCCESS(rv, rv);
    (*aParamCount) ++;
  }

  // search terms FIXME

  // min and max visit count
  PRInt32 visits = aQuery->MinVisits();
  if (visits >= 0) {
    rv = statement->BindInt32Parameter(aStartParameter + *aParamCount, visits);
    NS_ENSURE_SUCCESS(rv, rv);
    (*aParamCount) ++;
  }

  visits = aQuery->MaxVisits();
  if (visits >= 0) {
    rv = statement->BindInt32Parameter(aStartParameter + *aParamCount, visits);
    NS_ENSURE_SUCCESS(rv, rv);
    (*aParamCount) ++;
  }

  // onlyBookmarked: nothing to bind

  // domain (see GetReversedHostname for more info on reversed host names)
  if (NS_SUCCEEDED(aQuery->GetHasDomain(&hasIt)) && hasIt) {
    nsString revDomain;
    GetReversedHostname(NS_ConvertUTF8toUTF16(aQuery->Domain()), revDomain);

    if (aQuery->DomainIsHost()) {
      rv = statement->BindStringParameter(aStartParameter + *aParamCount, revDomain);
      NS_ENSURE_SUCCESS(rv, rv);
      (*aParamCount) ++;
    } else {
      // for "mozilla.org" do query >= "gro.allizom." AND < "gro.allizom/"
      // which will get everything starting with "gro.allizom." while using the
      // index (using SUBSTRING() causes indexes to be discarded).
      NS_ASSERTION(revDomain[revDomain.Length() - 1] == '.', "Invalid rev. host");
      rv = statement->BindStringParameter(aStartParameter + *aParamCount, revDomain);
      NS_ENSURE_SUCCESS(rv, rv);
      (*aParamCount) ++;
      revDomain.Truncate(revDomain.Length() - 1);
      revDomain.Append(PRUnichar('/'));
      rv = statement->BindStringParameter(aStartParameter + *aParamCount, revDomain);
      NS_ENSURE_SUCCESS(rv, rv);
      (*aParamCount) ++;
    }
  }

  // URI
  if (NS_SUCCEEDED(aQuery->GetHasUri(&hasIt)) && hasIt) {
    BindStatementURI(statement, aStartParameter + *aParamCount, aQuery->Uri());
    (*aParamCount) ++;
  }

  // annotation
  aQuery->GetHasAnnotation(&hasIt);
  if (hasIt) {
    rv = statement->BindUTF8StringParameter(aStartParameter + *aParamCount,
                                            aQuery->Annotation());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsNavHistory::ResultsAsList
//

nsresult
nsNavHistory::ResultsAsList(mozIStorageStatement* statement,
                            nsNavHistoryQueryOptions* aOptions,
                            nsCOMArray<nsNavHistoryResultNode>* aResults)
{
  nsresult rv;
  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(statement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
    nsCOMPtr<nsNavHistoryResultNode> result;
    rv = RowToResult(row, aOptions, getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);
    aResults->AppendObject(result);
  }
  return NS_OK;
}


// nsNavHistory::RecursiveGroup
//
//    aSource and aDest must be different!
//
//    This just calls the correct grouping subroutine. These will generate the
//    grouping and return to us a list of nodes that are the groups.
//
//    If we need to do another level of grouping, we go in and replace those
//    container's children with another level of grouping. This is less
//    efficient because we need to copy the lists around. However, multilevel
//    grouping will be very uncommon so we are more interested in an optimized
//    single level of grouping.

nsresult
nsNavHistory::RecursiveGroup(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                             const PRUint32* aGroupingMode, PRUint32 aGroupCount,
                             nsCOMArray<nsNavHistoryResultNode>* aDest)
{
  NS_ASSERTION(aGroupCount > 0, "Invalid group count");
  NS_ASSERTION(aDest->Count() == 0, "Destination array is not empty");
  NS_ASSERTION(&aSource != aDest, "Source and dest must be different for grouping");

  nsresult rv;
  switch (aGroupingMode[0]) {
    case nsINavHistoryQueryOptions::GROUP_BY_DAY:
      rv = GroupByDay(aSource, aDest);
      break;
    case nsINavHistoryQueryOptions::GROUP_BY_HOST:
      rv = GroupByHost(aSource, aDest, PR_FALSE);
      break;
    case nsINavHistoryQueryOptions::GROUP_BY_DOMAIN:
      rv = GroupByHost(aSource, aDest, PR_TRUE);
      break;
    default:
      // unknown grouping mode
      return NS_ERROR_INVALID_ARG;
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (aGroupCount > 1) {
    // Sort another level: We need to copy the array since we want the output
    // to be our level's destionation arrays.
    for (PRInt32 i = 0; i < aDest->Count(); i ++) {
      nsNavHistoryResultNode* curNode = (*aDest)[i];
      if (curNode->IsContainer()) {
        nsNavHistoryContainerResultNode* container = curNode->GetAsContainer();
        nsCOMArray<nsNavHistoryResultNode> temp(container->mChildren);
        container->mChildren.Clear();
        rv = RecursiveGroup(temp, &aGroupingMode[1], aGroupCount - 1,
                            &container->mChildren);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  return NS_OK;
}

// code borrowed from mozilla/xpfe/components/history/src/nsGlobalHistory.cpp
// pass in a pre-normalized now and a date, and we'll find
// the difference since midnight on each of the days.
//
// USECS_PER_DAY == PR_USEC_PER_SEC * 60 * 60 * 24;
static const PRInt64 USECS_PER_DAY = LL_INIT(20, 500654080);
static PRInt64
GetAgeInDays(PRTime aNormalizedNow, PRTime aDate)
{
  PRTime dateMidnight = NormalizeTimeRelativeToday(aDate);
  return ((aNormalizedNow - dateMidnight) / USECS_PER_DAY);
}

// XXX todo
// we should make "group by date" more flexible and extensible, in order
// to allow extension developers to write better history sidebars and viewers
// see bug #359346
// nsNavHistory::GroupByDay
nsresult
nsNavHistory::GroupByDay(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                         nsCOMArray<nsNavHistoryResultNode>* aDest)
{
  // 8 == today, yesterday, 2 ago, 3 ago, 4 ago, 5 ago, 6 ago, older than 6
  const PRInt32 numDays = 8;

  nsNavHistoryContainerResultNode *dates[numDays];
  for (PRInt32 i = 0; i < numDays; i++)
    dates[i] = nsnull;

  nsCAutoString dateNames[numDays];
  // special case: Today
  GetStringFromName(NS_LITERAL_STRING("finduri-AgeInDays-is-0").get(),  
                    dateNames[0]);
  // special case: Yesterday 
  GetStringFromName(NS_LITERAL_STRING("finduri-AgeInDays-is-1").get(),  
                    dateNames[1]);
  for (PRInt32 curDay = 2; curDay <= numDays-2; curDay++) {
    // common case:  "<curDay> days ago"
    GetAgeInDaysString(curDay, NS_LITERAL_STRING("finduri-AgeInDays-is").get(),
                       dateNames[curDay]);
  }
  // special case:  "Older than <numDays-2> days"
  GetAgeInDaysString(numDays-2, 
                     NS_LITERAL_STRING("finduri-AgeInDays-isgreater").get(),
                     dateNames[numDays-1]);
    
  PRTime normalizedNow = NormalizeTimeRelativeToday(PR_Now());

  for (PRInt32 i = 0; i < aSource.Count(); i ++) {
    if (!aSource[i]->IsURI()) {
      // what do we do with non-URLs? I'll just dump them into the top level
      aDest->AppendObject(aSource[i]);
      continue;
    }

    // get the date from aSource[i]
    nsCAutoString curDateName;
    PRInt64 ageInDays = GetAgeInDays(normalizedNow, aSource[i]->mTime);
    if (ageInDays > (numDays - 1))
      ageInDays = numDays - 1;
    curDateName = dateNames[ageInDays];

    if (!dates[ageInDays]) {
      // need to create an entry for this date
      dates[ageInDays] = new nsNavHistoryContainerResultNode(EmptyCString(), 
          curDateName,
          EmptyCString(),
          nsNavHistoryResultNode::RESULT_TYPE_DAY,
          PR_TRUE,
          EmptyCString());

      if (!dates[ageInDays])
        return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!dates[ageInDays]->mChildren.AppendObject(aSource[i]))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  for (PRInt32 i = 0; i < numDays; i++) {
    if (dates[i]) {
      nsresult rv = aDest->AppendObject(dates[i]);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

// nsNavHistory::GroupByHost
//
//    OPTIMIZATION: This parses the URI of each node that is coming in. This
//    makes it kind of slow. A previous version of this code used a host on
//    each result node. This host name is populated from the database, so it
//    doesn't have to be populated at query time. This is faster, but takes up
//    a lot of space in result nodes that isn't very helpful.
//
//    One option would be to store the host names in the nodes only if we will
//    be grouping by host later. Once we group by host, we could set it to the
//    empty string to free that heap data. Then this code would be fast but
//    we would only be charged the overhead of an empty string on each node.

nsresult
nsNavHistory::GroupByHost(const nsCOMArray<nsNavHistoryResultNode>& aSource,
                          nsCOMArray<nsNavHistoryResultNode>* aDest,
                          PRBool aIsDomain)
{
  nsDataHashtable<nsCStringHashKey, nsNavHistoryContainerResultNode*> hosts;
  if (! hosts.Init(512))
    return NS_ERROR_OUT_OF_MEMORY;

  for (PRInt32 i = 0; i < aSource.Count(); i ++) {
    if (! aSource[i]->IsURI()) {
      // what do we do with non-URLs? I'll just dump them into the top level
      aDest->AppendObject(aSource[i]);
      continue;
    }

    // get the host name
    nsCOMPtr<nsIURI> uri;
    nsCAutoString fullHostName;
    if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), aSource[i]->mURI)) ||
        NS_FAILED(uri->GetHost(fullHostName))) {
      // invalid host name, just drop it in the top level
      aDest->AppendObject(aSource[i]);
      continue;
    }

    nsCAutoString curHostName;
    if (aIsDomain) {
      DomainNameFromHostName(fullHostName, curHostName);
    } else {
      // just use the full host name
      curHostName = fullHostName;
    }

    nsNavHistoryContainerResultNode* curTopGroup = nsnull;
    if (! hosts.Get(curHostName, &curTopGroup)) {
      // need to create an entry for this host
      nsCAutoString title;
      TitleForDomain(curHostName, title);

      curTopGroup = new nsNavHistoryContainerResultNode(EmptyCString(), title,
          EmptyCString(), nsNavHistoryResultNode::RESULT_TYPE_HOST, PR_TRUE,
          EmptyCString());
      if (! curTopGroup)
        return NS_ERROR_OUT_OF_MEMORY;

      if (! hosts.Put(curHostName, curTopGroup))
        return NS_ERROR_OUT_OF_MEMORY;
      nsresult rv = aDest->AppendObject(curTopGroup);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (! curTopGroup->mChildren.AppendObject(aSource[i]))
      return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}


// nsNavHistory::FilterResultSet
//
//    Currently, this just does title/url filtering. This should be expanded in
//    the future.

nsresult
nsNavHistory::FilterResultSet(const nsCOMArray<nsNavHistoryResultNode>& aSet,
                              nsCOMArray<nsNavHistoryResultNode>* aFiltered,
                              const nsString& aSearch)
{
  nsStringArray terms;
  ParseSearchQuery(aSearch, &terms);

  // if there are no search terms, just return everything (i.e. do nothing)
  if (terms.Count() == 0) {
    aFiltered->AppendObjects(aSet);
    return NS_OK;
  }

  nsCStringArray searchAnnotations;
  /*
  if (mAnnotationService) {
    searchAnnotations.AppendCString(NS_LITERAL_CSTRING("qwer"));
    searchAnnotations.AppendCString(NS_LITERAL_CSTRING("asdf"));
    searchAnnotations.AppendCString(NS_LITERAL_CSTRING("zxcv"));
    //mAnnotationService->GetSearchableAnnotations();
  }
  */

  for (PRInt32 nodeIndex = 0; nodeIndex < aSet.Count(); nodeIndex ++) {
    PRBool allTermsFound = PR_TRUE;

    nsStringArray curAnnotations;
    /*
    if (searchAnnotations.Count()) {
      // come up with a list of all annotation *values* we need to search
      for (PRInt32 annotIndex = 0; annotIndex < searchAnnotations.Count(); annotIndex ++) {
        nsString annot;
        if (NS_SUCCEEDED(mAnnotationService->GetAnnotationString(
                                         aSet[nodeIndex]->mURI,
                                         *searchAnnotations[annotIndex],
                                         annot)))
          curAnnotations.AppendString(annot);
      }
    }
    */

    for (PRInt32 termIndex = 0; termIndex < terms.Count(); termIndex ++) {
      PRBool termFound = PR_FALSE;
      // title and URL
      if (CaseInsensitiveFindInReadable(*terms[termIndex],
                                        NS_ConvertUTF8toUTF16(aSet[nodeIndex]->mTitle)) ||
          (aSet[nodeIndex]->IsURI() &&
           CaseInsensitiveFindInReadable(*terms[termIndex],
                                  NS_ConvertUTF8toUTF16(aSet[nodeIndex]->mURI))))
        termFound = PR_TRUE;
      // searchable annotations
      /*if (! termFound) {
        for (PRInt32 annotIndex = 0; annotIndex < curAnnotations.Count(); annotIndex ++) {
          if (CaseInsensitiveFindInReadable(*terms[termIndex],
                                            *curAnnotations[annotIndex]))
            termFound = PR_TRUE;
        }
      }*/
      if (! termFound) {
        allTermsFound = PR_FALSE;
        break;
      }
    }
    if (allTermsFound)
      aFiltered->AppendObject(aSet[nodeIndex]);
  }
  return NS_OK;
}


// nsNavHistory::CheckIsRecentEvent
//
//    Sees if this URL happened "recently."
//
//    It is always removed from our recent list no matter what. It only counts
//    as "recent" if the event happend more recently than our event
//    threshold ago.

PRBool
nsNavHistory::CheckIsRecentEvent(RecentEventHash* hashTable,
                                 const nsACString& url)
{
  PRTime eventTime;
  if (hashTable->Get(url, &eventTime)) {
    hashTable->Remove(url);
    if (eventTime > GetNow() - RECENT_EVENT_THRESHOLD)
      return PR_TRUE;
    return PR_FALSE;
  }
  return PR_FALSE;
}


// nsNavHistory::ExpireNonrecentEvents
//
//    This goes through our

PR_STATIC_CALLBACK(PLDHashOperator)
ExpireNonrecentEventsCallback(nsCStringHashKey::KeyType aKey,
                              PRInt64& aData,
                              void* userArg)
{
  PRInt64* threshold = NS_REINTERPRET_CAST(PRInt64*, userArg);
  if (aData < *threshold)
    return PL_DHASH_REMOVE;
  return PL_DHASH_NEXT;
}
void
nsNavHistory::ExpireNonrecentEvents(RecentEventHash* hashTable)
{
  PRInt64 threshold = GetNow() - RECENT_EVENT_THRESHOLD;
  hashTable->Enumerate(ExpireNonrecentEventsCallback,
                       NS_REINTERPRET_CAST(void*, &threshold));
}


// nsNavHistory::GetRedirectFor
//
//    Given a destination URI, this finds a recent redirect that resulted in
//    this URI. If it finds one, it will put the redirect source info into
//    the out params and return true. If there is no matching redirect, it will
//    return false.
//
//    @param aDestination The destination URI spec of the redirect to look for.
//    @param aSource      Will be filled with the redirect source URI when a
//                        redirect is found.
//    @param aTime        Will be filled with the time the redirect happened
//                         when a redirect is found.
//    @param aRedirectType Will be filled with the redirect type when a redirect
//                         is found. Will be either
//                         TRANSITION_REDIRECT_PERMANENT or
//                         TRANSITION_REDIRECT_TEMPORARY
//    @returns True if the redirect is found.
//
//    HOW REDIRECT TRACKING WORKS
//    ---------------------------
//    When we get an AddDocumentRedirect message, we store the redirect in
//    our mRecentRedirects which maps the destination URI to a source,time pair.
//    When we get a new URI, we see if there were any redirects to this page
//    in the hash table. If found, we know that the page came through the given
//    redirect and add it.
//
//    Example: Page S redirects throught R1, then R2, to give page D. Page S
//    will have been already added to history.
//    - AddDocumentRedirect(R1, R2)
//    - AddDocumentRedirect(R2, D)
//    - AddURI(uri=D, referrer=S)
//
//    When we get the AddURI(D), we see the hash table has a value for D from R2.
//    We have to recursively check that source since there could be more than
//    one redirect, as in this case. Here we see there was a redirect to R2 from
//    R1. The referrer for D is S, so we know S->R1->R2->D.
//
//    Alternatively, the user could have typed or followed a bookmark from S.
//    In this case, with two redirects we'll get:
//    - MarkPageAsTyped(S)
//    - AddDocumentRedirect(S, R)
//    - AddDocumentRedirect(R, D)
//    - AddURI(uri=D, referrer=null)
//    We need to be careful to add a visit to S in this case with an incoming
//    transition of typed and an outgoing transition of redirect.
//
//    Note that this can get confused in some cases where you have a page
//    open in more than one window loading at the same time. This should be rare,
//    however, and should not affect much.

PRBool
nsNavHistory::GetRedirectFor(const nsACString& aDestination,
                             nsACString& aSource, PRTime* aTime,
                             PRUint32* aRedirectType)
{
  RedirectInfo info;
  if (mRecentRedirects.Get(aDestination, &info)) {
    mRecentRedirects.Remove(aDestination);
    if (info.mTimeCreated < GetNow() - RECENT_EVENT_THRESHOLD)
      return PR_FALSE; // too long ago, probably invalid
    aSource = info.mSourceURI;
    *aTime = info.mTimeCreated;
    *aRedirectType = info.mType;
    return PR_TRUE;
  }
  return PR_FALSE;
}


// nsNavHistory::RowToResult
//
//    Here, we just have a generic row. It could be a query, URL, visit,
//    or full visit.

nsresult
nsNavHistory::RowToResult(mozIStorageValueArray* aRow,
                          nsNavHistoryQueryOptions* aOptions,
                          nsNavHistoryResultNode** aResult)
{
  *aResult = nsnull;
  NS_ASSERTION(aRow && aOptions && aResult, "Null pointer in RowToResult");

  // URL
  nsCAutoString url;
  nsresult rv = aRow->GetUTF8String(kGetInfoIndex_URL, url);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  nsCAutoString title;
  title.SetIsVoid(PR_TRUE);
  if (! aOptions->ForceOriginalTitle()) {
    rv = aRow->GetUTF8String(kGetInfoIndex_UserTitle, title);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (title.IsVoid()) {
    rv = aRow->GetUTF8String(kGetInfoIndex_Title, title);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRUint32 accessCount = aRow->AsInt32(kGetInfoIndex_VisitCount);
  PRTime time = aRow->AsInt64(kGetInfoIndex_VisitDate);

  // favicon
  nsCAutoString favicon;
  rv = aRow->GetUTF8String(kGetInfoIndex_FaviconURL, favicon);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsQueryURI(url)) {
    // special case "place:" URIs: turn them into containers
    return QueryRowToResult(url, title, accessCount, time, favicon, aResult);
  } else if (aOptions->ResultType() == nsNavHistoryQueryOptions::RESULTS_AS_URI) {
    *aResult = new nsNavHistoryResultNode(url, title, accessCount, time,
                                          favicon);
    if (! *aResult)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aResult);
    return NS_OK;
  }
  // now we know the result type is some kind of visit (regular or full)

  // session
  PRInt64 session = aRow->AsInt64(kGetInfoIndex_SessionId);

  if (aOptions->ResultType() == nsNavHistoryQueryOptions::RESULTS_AS_VISIT) {
    *aResult = new nsNavHistoryVisitResultNode(url, title, accessCount, time,
                                               favicon, session);
    if (! *aResult)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aResult);
    return NS_OK;
  }

  // now it had better be a full visit
  NS_ASSERTION(aOptions->ResultType() == nsNavHistoryQueryOptions::RESULTS_AS_FULL_VISIT,
               "Invalid result type in RowToResult");

  NS_NOTREACHED("Full visits not supported yet.");
  // visit ID
  /*PRUint32 accessCount;
  rv = aRow->GetInt32(kGetInfoIndex_VisitCount, &accessCount);
  NS_ENSURE_SUCCESS(rv, rv);*/

  // referring visit ID
  /*PRUint32 accessCount;
  rv = aRow->GetInt32(kGetInfoIndex_VisitCount, &accessCount);
  NS_ENSURE_SUCCESS(rv, rv);*/

  // transition
  /*PRUint32 transition;
  rv = aRow->GetInt32(kGetInfoIndex_VisitCount, &transition);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = new nsNavHistoryFullVisitResultNode(title, accessCount, time,
                                                 favicon, url, session, visitId,
                                                 referring, transition);
  if (! *aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);*/
  return NS_OK;
}


// nsNavHistory::QueryRowToResult
//
//    Called by RowToResult when the URI is a place: URI to generate the proper
//    folder or query node.

nsresult
nsNavHistory::QueryRowToResult(const nsACString& aURI, const nsACString& aTitle,
                               PRUint32 aAccessCount, PRTime aTime,
                               const nsACString& aFavicon,
                               nsNavHistoryResultNode** aNode)
{
  nsCOMArray<nsNavHistoryQuery> queries;
  nsCOMPtr<nsNavHistoryQueryOptions> options;
  nsresult rv = QueryStringToQueryArray(aURI, &queries,
                                        getter_AddRefs(options));
  if (NS_FAILED(rv)) {
    // This was a query that did not parse, what do we do? We don't want to
    // return failure since that will kill the whole query process. Instead
    // make a query node with the query as a string. This way we have a valid
    // node for the user to manipulate that will look like a query, but it will
    // never populate since the query string is invalid.
    *aNode = new nsNavHistoryQueryResultNode(aURI, aTitle, aFavicon);
    if (! *aNode)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aNode);
  } else {
    PRInt64 folderId = GetSimpleBookmarksQueryFolder(queries);
    if (folderId) {
      // simple bookmarks folder, magically generate a bookmarks folder node
      nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
      NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

      // this addrefs for us
      rv = bookmarks->ResultNodeForFolder(folderId, options, aNode);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // regular query
      *aNode = new nsNavHistoryQueryResultNode(aTitle, EmptyCString(),
                                               queries, options);
      if (! *aNode)
        return NS_ERROR_OUT_OF_MEMORY;
      NS_ADDREF(*aNode);
    }
  }
  return NS_OK;
}


// nsNavHistory::VisitIdToResultNode
//
//    Used by the query results to create new nodes on the fly when
//    notifications come in. This just creates a node for the given visit ID.

nsresult
nsNavHistory::VisitIdToResultNode(PRInt64 visitId,
                                  nsNavHistoryQueryOptions* aOptions,
                                  nsNavHistoryResultNode** aResult)
{
  mozIStorageStatement* statement; // non-owning!
  if (aOptions->ResultType() == nsNavHistoryQueryOptions::RESULTS_AS_VISIT ||
      aOptions->ResultType() == nsNavHistoryQueryOptions::RESULTS_AS_FULL_VISIT) {
    // visit query - want exact visit time
    statement = mDBVisitToVisitResult;
  } else {
    // URL results - want last visit time
    statement = mDBVisitToURLResult;
  }

  mozStorageStatementScoper scoper(statement);
  nsresult rv = statement->BindInt64Parameter(0, visitId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = statement->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! hasMore) {
    NS_NOTREACHED("Trying to get a result node for an invalid visit");
    return NS_ERROR_INVALID_ARG;
  }

  return RowToResult(statement, aOptions, aResult);
}


// nsNavHistory::UriToResultNode
//
//    Used by the query results to create new nodes on the fly when
//    notifications come in. This creates a URL node for the given URL.

nsresult
nsNavHistory::UriToResultNode(nsIURI* aUri, nsNavHistoryQueryOptions* aOptions,
                              nsNavHistoryResultNode** aResult)
{
  // this query must be asking for URL results, because we don't have enough
  // information to construct a visit result node here
  NS_ASSERTION(aOptions->ResultType() == nsNavHistoryQueryOptions::RESULTS_AS_URI,
               "Can't make visits from URIs");
  mozStorageStatementScoper scoper(mDBUrlToUrlResult);
  nsresult rv = BindStatementURI(mDBUrlToUrlResult, 0, aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = mDBUrlToUrlResult->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! hasMore) {
    NS_NOTREACHED("Trying to get a result node for an invalid URL");
    return NS_ERROR_INVALID_ARG;
  }

  return RowToResult(mDBUrlToUrlResult, aOptions, aResult);
}


// nsNavHistory::TitleForDomain
//
//    This computes the title for a given domain. Normally, this is just the
//    domain name, but we specially handle empty cases to give you a nice
//    localized string.

void
nsNavHistory::TitleForDomain(const nsCString& domain, nsACString& aTitle)
{
  if (! domain.IsEmpty()) {
    aTitle = domain;
    return;
  }

  // use the localized one instead
  GetStringFromName(NS_LITERAL_STRING("localhost").get(), aTitle);
}

void
nsNavHistory::GetAgeInDaysString(PRInt32 aInt, const PRUnichar *aName, nsACString& aResult)
{
  nsAutoString intString;
  intString.AppendInt(aInt);
  const PRUnichar* strings[1] = { intString.get() };
  nsXPIDLString value;
  nsresult rv = mBundle->FormatStringFromName(aName, strings, 
                                              1, getter_Copies(value));
  if (NS_SUCCEEDED(rv))
    CopyUTF16toUTF8(value, aResult);
  else
    aResult.Truncate(0);
}

void
nsNavHistory::GetStringFromName(const PRUnichar *aName, nsACString& aResult)
{
  nsXPIDLString value;
  nsresult rv = mBundle->GetStringFromName(aName, getter_Copies(value));
  if (NS_SUCCEEDED(rv))
    CopyUTF16toUTF8(value, aResult);
  else
    aResult.Truncate(0);
}

// nsNavHistory::SetPageTitleInternal
//
//    Called to set either the user-defined title (aIsUserTitle=true) or the
//    "real" page title (aIsUserTitle=false) for the given URI. Used as a
//    backend for SetPageUserTitle and SetTitle
//
//    Will fail for pages that are not in the DB. To clear the corresponding
//    title, use aTitle.SetIsVoid(). Sending an empty string will save an
//    empty string instead of clearing it.

nsresult
nsNavHistory::SetPageTitleInternal(nsIURI* aURI, PRBool aIsUserTitle,
                                   const nsAString& aTitle)
{
  nsresult rv;

  mozStorageTransaction transaction(mDBConn, PR_TRUE);

  // first, make sure the page exists, and fetch the old title (we need the one
  // that isn't changing to send notifications)
  nsAutoString title;
  nsAutoString userTitle;
  { // scope for statement
    mozStorageStatementScoper infoScoper(mDBGetURLPageInfo);
    rv = BindStatementURI(mDBGetURLPageInfo, 0, aURI);
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool hasURL = PR_FALSE;
    rv = mDBGetURLPageInfo->ExecuteStep(&hasURL);
    NS_ENSURE_SUCCESS(rv, rv);
    if (! hasURL) {
      // we don't have the URL, give up
      return NS_ERROR_NOT_AVAILABLE;
    }

    // page title
    rv = mDBGetURLPageInfo->GetString(kGetInfoIndex_Title, title);
    NS_ENSURE_SUCCESS(rv, rv);

    // user title
    rv = mDBGetURLPageInfo->GetString(kGetInfoIndex_UserTitle, userTitle);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // It is actually common to set the title to be the same thing it used to
  // be. For example, going to any web page will always cause a title to be set,
  // even though it will often be unchanged since the last visit. In these
  // cases, we can avoid DB writing and (most significantly) observer overhead.
  if (aIsUserTitle && aTitle.IsVoid() == userTitle.IsVoid() &&
      aTitle == userTitle)
    return NS_OK;
  if (! aIsUserTitle && aTitle.IsVoid() == title.IsVoid() &&
      aTitle == title)
    return NS_OK;

  nsCOMPtr<mozIStorageStatement> dbModStatement;
  if (aIsUserTitle) {
    userTitle = aTitle;
    rv = mDBConn->CreateStatement(
        NS_LITERAL_CSTRING("UPDATE moz_places SET user_title = ?1 WHERE url = ?2"),
        getter_AddRefs(dbModStatement));
  } else {
    title = aTitle;
    rv = mDBConn->CreateStatement(
        NS_LITERAL_CSTRING("UPDATE moz_places SET title = ?1 WHERE url = ?2"),
        getter_AddRefs(dbModStatement));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  if (aTitle.IsVoid())
    dbModStatement->BindNullParameter(0);
  else
    dbModStatement->BindStringParameter(0, StringHead(aTitle, HISTORY_TITLE_LENGTH_MAX));
  NS_ENSURE_SUCCESS(rv, rv);

  // url
  rv = BindStatementURI(dbModStatement, 1, aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbModStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  transaction.Commit();

  // observers (have to check first if it's bookmarked)
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryObserver,
                      OnTitleChanged(aURI, title, userTitle, aIsUserTitle))

  return NS_OK;

}


// nsNavHistory::CreateLookupIndexes
//
// This creates some indexes on the history tables which are expensive to
// update when we're doing many insertions, as with history import.  Instead,
// we defer creation of the index until import is finished.
//
//    FIXME: We should check if the index exists (bug 327317) and then not
//    try to create it. That way we can check for errors. Currently we ignore
//    errors since the indeices may already exist.

nsresult
nsNavHistory::CreateLookupIndexes()
{
  nsresult rv;

  // History table indexes
  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_places_hostindex ON moz_places (rev_host)"));
  //NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_places_visitcount ON moz_places (visit_count)"));
  //NS_ENSURE_SUCCESS(rv, rv);

  // Visit table indexes
  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_historyvisits_fromindex ON moz_historyvisits (from_visit)"));
  //NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_historyvisits_dateindex ON moz_historyvisits (visit_date)"));
  //NS_ENSURE_SUCCESS(rv, rv);

#ifdef IN_MEMORY_LINKS
  // In-memory links indexes
  rv = mMemDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE INDEX moz_memhistory_index ON moz_memhistory (url)"));
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  return NS_OK;
}

nsresult
nsNavHistory::AddPageWithVisit(nsIURI *aURI,
                               const nsString &aTitle,
                               const nsString &aUserTitle,
                               PRBool aHidden, PRBool aTyped,
                               PRInt32 aVisitCount,
                               PRInt32 aLastVisitTransition,
                               PRTime aLastVisitDate)
{
  PRBool canAdd = PR_FALSE;
  nsresult rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  PRInt64 pageID;
  rv = InternalAddNewPage(aURI, aTitle, aHidden, aTyped, aVisitCount, &pageID);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aLastVisitDate != -1) {
    PRInt64 visitID;
    rv = InternalAddVisit(pageID, 0, 0,
                          aLastVisitDate, aLastVisitTransition, &visitID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;  
}

nsresult
nsNavHistory::RemoveDuplicateURIs()
{
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT id, url FROM moz_places ORDER BY url"),
      getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<PRInt64> duplicates;
  nsCAutoString lastURI;
  PRBool hasMore;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
    nsCAutoString uri;
    statement->GetUTF8String(1, uri);
    if (uri.Equals(lastURI)) {
      duplicates.AppendElement(statement->AsInt64(0));
    } else {
      lastURI = uri;
    }
  }

  // Now remove all of the duplicates from the history and visit tables.
  rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("DELETE FROM moz_places WHERE id = ?1"),
      getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> visitDelete;
  rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("DELETE FROM moz_historyvisits WHERE place_id = ?1"),
      getter_AddRefs(visitDelete));
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < duplicates.Length(); ++i) {
    PRInt64 id = duplicates[i];
    {
      mozStorageStatementScoper scope(statement);
      rv = statement->BindInt64Parameter(0, id);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = statement->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    {
      mozStorageStatementScoper scope(visitDelete);
      rv = visitDelete->BindInt64Parameter(0, id);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = visitDelete->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

// Local function **************************************************************


// GetReversedHostname
//
//    This extracts the hostname from the URI and reverses it in the
//    form that we use (always ending with a "."). So
//    "http://microsoft.com/" becomes "moc.tfosorcim."
//
//    The idea behind this is that we can create an index over the items in
//    the reversed host name column, and then query for as much or as little
//    of the host name as we feel like.
//
//    For example, the query "host >= 'gro.allizom.' AND host < 'gro.allizom/'
//    Matches all host names ending in '.mozilla.org', including
//    'developer.mozilla.org' and just 'mozilla.org' (since we define all
//    reversed host names to end in a period, even 'mozilla.org' matches).
//    The important thing is that this operation uses the index. Any substring
//    calls in a select statement (even if it's for the beginning of a string)
//    will bypass any indices and will be slow).

nsresult
GetReversedHostname(nsIURI* aURI, nsAString& aRevHost)
{
  nsCString forward8;
  nsresult rv = aURI->GetHost(forward8);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // can't do reversing in UTF8, better use 16-bit chars
  nsAutoString forward = NS_ConvertUTF8toUTF16(forward8);
  GetReversedHostname(forward, aRevHost);
  return NS_OK;
}


// GetReversedHostname
//
//    Same as previous but for strings

void
GetReversedHostname(const nsString& aForward, nsAString& aRevHost)
{
  ReverseString(aForward, aRevHost);
  aRevHost.Append(PRUnichar('.'));
}


// IsNumericHostName
//
//    For host-based groupings, numeric IPs should not be collapsed by the
//    last part of the domain name, but should stand alone. This code determines
//    if this is the case so we don't collapse "10.1.2.3" to "3". It would be
//    nice to use the URL parsing code, but it doesn't give us this information,
//    this is usually done by the OS in response to DNS lookups.
//
//    This implementation is not perfect, we just check for all numbers and
//    digits, and three periods. You could come up with crazy internal host
//    names that would fool this logic, but I bet there are no real examples.

PRBool IsNumericHostName(const nsCString& aHost)
{
  PRInt32 periodCount = 0;
  for (PRUint32 i = 0; i < aHost.Length(); i ++) {
    PRUnichar cur = aHost[i];
    if (cur == '.')
      periodCount ++;
    else if (cur < '0' || cur > '9')
      return PR_FALSE;
  }
  return (periodCount == 3);
}


// GetSimpleBookmarksQueryFolder
//
//    Determines if this set of queries is a simple bookmarks query for a
//    folder with no other constraints. In these common cases, we can more
//    efficiently compute the results.
//
//    Returns the folder ID if it is a simple folder query, 0 if not.

static PRInt64
GetSimpleBookmarksQueryFolder(const nsCOMArray<nsNavHistoryQuery>& aQueries)
{
  if (aQueries.Count() != 1)
    return 0;

  nsNavHistoryQuery* query = aQueries[0];
  if (query->Folders().Length() != 1)
    return 0;

  PRBool hasIt;
  query->GetHasBeginTime(&hasIt);
  if (hasIt)
    return 0;
  query->GetHasEndTime(&hasIt);
  if (hasIt)
    return 0;
  query->GetHasDomain(&hasIt);
  if (hasIt)
    return 0;
  query->GetHasUri(&hasIt);
  if (hasIt)
    return 0;

  // Note that we don't care about the onlyBookmarked flag, if you specify a bookmark
  // folder, onlyBookmarked is inferred.

  return query->Folders()[0];
}


// ParseSearchQuery
//
//    This just breaks the query up into words. We don't do anything fancy,
//    not even quoting. We do, however, strip quotes, because people might
//    try to input quotes expecting them to do something and get no results
//    back.

inline PRBool isQueryWhitespace(PRUnichar ch)
{
  return ch == ' ';
}

void ParseSearchQuery(const nsString& aQuery, nsStringArray* aTerms)
{
  PRInt32 lastBegin = -1;
  for (PRUint32 i = 0; i < aQuery.Length(); i ++) {
    if (isQueryWhitespace(aQuery[i]) || aQuery[i] == '"') {
      if (lastBegin >= 0) {
        // found the end of a word
        aTerms->AppendString(Substring(aQuery, lastBegin, i - lastBegin));
        lastBegin = -1;
      }
    } else {
      if (lastBegin < 0) {
        // found the beginning of a word
        lastBegin = i;
      }
    }
  }
  // last word
  if (lastBegin >= 0)
    aTerms->AppendString(Substring(aQuery, lastBegin));
}


// GenerateTitleFromURI
//
//    Given a URL, we try to get a reasonable title for this page. We try
//    to use a filename out of the URI, then fall back on the path, then fall
//    back on the whole hostname.

nsresult // static
GenerateTitleFromURI(nsIURI* aURI, nsAString& aTitle)
{
  nsCAutoString name;
  nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
  if (url)
    url->GetFileName(name);
  if (name.IsEmpty()) {
    // path
    nsresult rv = aURI->GetPath(name);
    if (NS_FAILED(rv) || (name.Length() == 1 && name[0] == '/')) {
      // empty path name, use hostname
      rv = aURI->GetHost(name);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  aTitle = NS_ConvertUTF8toUTF16(name);
  return NS_OK;
}


// GetTLDCharCount
//
//    Given a normal, forward host name ("bugzilla.mozilla.org")
//    returns the number of 8-bit characters that the TLD occupies, NOT
//    including the trailing dot: bugzilla.mozilla.org -> 3
//                  theregister.co.uk -> 5
//                  mysite.us -> 2

PRInt32
GetTLDCharCount(const nsCString& aHost)
{
  nsCAutoString trailing;
  GetSubstringFromNthDot(aHost, aHost.Length() - 1, 1,
                         PR_FALSE, trailing);

  switch (GetTLDType(trailing)) {
    case 0:
      // not a known TLD
      return 0;
    case 1:
      // first-level TLD
      return trailing.Length();
    case 2: {
      // need to check second level and trim it too (if valid)
      nsCAutoString trailingMore;
      GetSubstringFromNthDot(aHost, aHost.Length() - 1,
                             2, PR_FALSE, trailingMore);
      if (GetTLDType(trailingMore))
        return trailingMore.Length();
      else
        return trailing.Length();
    }
    default:
      NS_NOTREACHED("Invalid TLD type");
      return 0;
  }
}


// GetTLDType
//
//    Given the last part of a host name, tells you whether this is a known TLD.
//      0 -> not known
//      1 -> known 1st or second level TLD ("com", "co.uk")
//      2 -> end of a two-part TLD ("uk")
//
//    If this returns 2, you should probably re-call the function including
//    the next level of name. For example ("uk" -> 2, then you call with
//    "co.uk" and know that the last two pars of this domain name are
//    "toplevel".
//
//    This should be moved somewhere else (like cookies) and made easier to
//    update.

PRInt32
GetTLDType(const nsCString& aHostTail)
{
  //static nsDataHashtable<nsStringHashKey, int> tldTypes;
  if (! gTldTypes) {
    // need to populate table
    gTldTypes = new nsDataHashtable<nsCStringHashKey, int>();
    if (! gTldTypes)
      return 0;

    gTldTypes->Init(256);

    gTldTypes->Put(NS_LITERAL_CSTRING("com"), 1);
    gTldTypes->Put(NS_LITERAL_CSTRING("org"), 1);
    gTldTypes->Put(NS_LITERAL_CSTRING("net"), 1);
    gTldTypes->Put(NS_LITERAL_CSTRING("edu"), 1);
    gTldTypes->Put(NS_LITERAL_CSTRING("gov"), 1);
    gTldTypes->Put(NS_LITERAL_CSTRING("mil"), 1);
    gTldTypes->Put(NS_LITERAL_CSTRING("uk"), 2);
    gTldTypes->Put(NS_LITERAL_CSTRING("co.uk"), 1);
    gTldTypes->Put(NS_LITERAL_CSTRING("kr"), 2);
    gTldTypes->Put(NS_LITERAL_CSTRING("co.kr"), 1);
    gTldTypes->Put(NS_LITERAL_CSTRING("hu"), 1);
    gTldTypes->Put(NS_LITERAL_CSTRING("us"), 1);

    // FIXME: add the rest
  }

  PRInt32 type = 0;
  if (gTldTypes->Get(aHostTail, &type))
    return type;
  else
    return 0;
}


// GetSubstringFromNthDot
//
//    Similar to GetSubstringToNthDot except searches backward
//      GetSubstringFromNthDot("foo.bar", length, 1, PR_FALSE) -> "bar"
//
//    It is legal to pass in a starting position < 0 so you can just
//    use Length()-1 as the starting position even if the length is 0.

void GetSubstringFromNthDot(const nsCString& aInput, PRInt32 aStartingSpot,
                            PRInt32 aN, PRBool aIncludeDot, nsACString& aSubstr)
{
  PRInt32 dotsFound = 0;
  for (PRInt32 i = aStartingSpot; i >= 0; i --) {
    if (aInput[i] == '.') {
      dotsFound ++;
      if (dotsFound == aN) {
        if (aIncludeDot)
          aSubstr = Substring(aInput, i, aInput.Length() - i);
        else
          aSubstr = Substring(aInput, i + 1, aInput.Length() - i - 1);
        return;
      }
    }
  }
  aSubstr = aInput; // no dot found
}


// BindStatementURI
//
//    Binds the specified URI as the parameter 'index' for the statment.
//    URIs are always bound as UTF8

nsresult BindStatementURI(mozIStorageStatement* statement, PRInt32 index,
                          nsIURI* aURI)
{
  nsCAutoString utf8URISpec;
  nsresult rv = aURI->GetSpec(utf8URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindUTF8StringParameter(index,
      StringHead(utf8URISpec, HISTORY_URI_LENGTH_MAX));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}
