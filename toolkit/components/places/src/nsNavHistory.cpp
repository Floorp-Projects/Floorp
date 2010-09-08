//* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla History System.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
 *   Dietrich Ayala <dietrich@mozilla.com>
 *   Seth Spitzer <sspitzer@mozilla.com>
 *   Asaf Romano <mano@mozilla.com>
 *   Marco Bonardo <mak77@bonardo.net>
 *   Edward Lee <edward.lee@engineering.uiuc.edu>
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *   Drew Willcoxon <adw@mozilla.com>
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

#include "nsTArray.h"
#include "nsCollationCID.h"
#include "nsILocaleService.h"
#include "nsIPrefBranch2.h"

#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsPromiseFlatString.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "prsystem.h"
#include "prtime.h"
#include "nsEscape.h"
#include "nsIEffectiveTLDService.h"
#include "nsIClassInfoImpl.h"
#include "nsThreadUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsMathUtils.h"
#include "mozIStorageCompletionCallback.h"

#include "nsNavBookmarks.h"
#include "nsAnnotationService.h"
#include "nsILivemarkService.h"
#include "nsFaviconService.h"

#include "nsPlacesTables.h"
#include "nsPlacesIndexes.h"
#include "nsPlacesTriggers.h"
#include "nsPlacesMacros.h"
#include "SQLFunctions.h"
#include "Helpers.h"
#include "History.h"

#include "mozilla/FunctionTimer.h"

#ifdef MOZ_XUL
#include "nsIAutoCompleteInput.h"
#include "nsIAutoCompletePopup.h"
#endif

using namespace mozilla::places;

// The maximum number of things that we will store in the recent events list
// before calling ExpireNonrecentEvents. This number should be big enough so it
// is very difficult to get that many unconsumed events (for example, typed but
// never visited) in the RECENT_EVENT_THRESHOLD. Otherwise, we'll start
// checking each one for every page visit, which will be somewhat slower.
#define RECENT_EVENT_QUEUE_MAX_LENGTH 128

// preference ID strings
#define PREF_PLACES_BRANCH_BASE                 "places."

#define PREF_HISTORY_ENABLED                    "history.enabled"

#define PREF_FRECENCY_NUM_VISITS                "frecency.numVisits"
#define PREF_FRECENCY_FIRST_BUCKET_CUTOFF       "frecency.firstBucketCutoff"
#define PREF_FRECENCY_SECOND_BUCKET_CUTOFF      "frecency.secondBucketCutoff"
#define PREF_FRECENCY_THIRD_BUCKET_CUTOFF       "frecency.thirdBucketCutoff"
#define PREF_FRECENCY_FOURTH_BUCKET_CUTOFF      "frecency.fourthBucketCutoff"
#define PREF_FRECENCY_FIRST_BUCKET_WEIGHT       "frecency.firstBucketWeight"
#define PREF_FRECENCY_SECOND_BUCKET_WEIGHT      "frecency.secondBucketWeight"
#define PREF_FRECENCY_THIRD_BUCKET_WEIGHT       "frecency.thirdBucketWeight"
#define PREF_FRECENCY_FOURTH_BUCKET_WEIGHT      "frecency.fourthBucketWeight"
#define PREF_FRECENCY_DEFAULT_BUCKET_WEIGHT     "frecency.defaultBucketWeight"
#define PREF_FRECENCY_EMBED_VISIT_BONUS         "frecency.embedVisitBonus"
#define PREF_FRECENCY_FRAMED_LINK_VISIT_BONUS   "frecency.framedLinkVisitBonus"
#define PREF_FRECENCY_LINK_VISIT_BONUS          "frecency.linkVisitBonus"
#define PREF_FRECENCY_TYPED_VISIT_BONUS         "frecency.typedVisitBonus"
#define PREF_FRECENCY_BOOKMARK_VISIT_BONUS      "frecency.bookmarkVisitBonus"
#define PREF_FRECENCY_DOWNLOAD_VISIT_BONUS      "frecency.downloadVisitBonus"
#define PREF_FRECENCY_PERM_REDIRECT_VISIT_BONUS "frecency.permRedirectVisitBonus"
#define PREF_FRECENCY_TEMP_REDIRECT_VISIT_BONUS "frecency.tempRedirectVisitBonus"
#define PREF_FRECENCY_DEFAULT_VISIT_BONUS       "frecency.defaultVisitBonus"
#define PREF_FRECENCY_UNVISITED_BOOKMARK_BONUS  "frecency.unvisitedBookmarkBonus"
#define PREF_FRECENCY_UNVISITED_TYPED_BONUS     "frecency.unvisitedTypedBonus"

#define PREF_LAST_VACUUM                        "last_vacuum"

#define PREF_CACHE_TO_MEMORY_PERCENTAGE         "database.cache_to_memory_percentage"

// Default integer value for PREF_CACHE_TO_MEMORY_PERCENTAGE.
// This is 6% of machine memory, giving 15MB for a user with 256MB of memory.
// Out of this cache, SQLite will use at most the size of the database file.
#define DATABASE_DEFAULT_CACHE_TO_MEMORY_PERCENTAGE 6

// This is the schema version, update it at any schema change and add a
// corresponding migrateVxx method below.
#define DATABASE_SCHEMA_VERSION 10

// Filename of the database.
#define DATABASE_FILENAME NS_LITERAL_STRING("places.sqlite")

// Filename used to backup corrupt databases.
#define DATABASE_CORRUPT_FILENAME NS_LITERAL_STRING("places.sqlite.corrupt")

// We use the TRUNCATE journal mode to reduce the number of fsyncs.  Without
// this setting we had a Ts hit on Linux.  See bug 460315 for details.
#define DATABASE_JOURNAL_MODE "TRUNCATE"

// Fraction of free pages in the database to force a vacuum between
// DATABASE_MAX_TIME_BEFORE_VACUUM and DATABASE_MIN_TIME_BEFORE_VACUUM.
#define DATABASE_VACUUM_FREEPAGES_THRESHOLD 0.1
// This is the maximum time (in microseconds) that can pass between 2 VACUUM
// operations.
#define DATABASE_MAX_TIME_BEFORE_VACUUM (PRInt64)60 * 24 * 60 * 60 * 1000 * 1000
// This is the minimum time (in microseconds) that should pass between 2 VACUUM
// operations.
#define DATABASE_MIN_TIME_BEFORE_VACUUM (PRInt64)30 * 24 * 60 * 60 * 1000 * 1000

// In order to avoid calling PR_now() too often we use a cached "now" value
// for repeating stuff.  These are milliseconds between "now" cache refreshes.
#define RENEW_CACHED_NOW_TIMEOUT ((PRInt32)3 * PR_MSEC_PER_SEC)

// USECS_PER_DAY == PR_USEC_PER_SEC * 60 * 60 * 24;
static const PRInt64 USECS_PER_DAY = LL_INIT(20, 500654080);

// character-set annotation
#define CHARSET_ANNO NS_LITERAL_CSTRING("URIProperties/characterSet")

// These macros are used when splitting history by date.
// These are the day containers and catch-all final container.
#define HISTORY_ADDITIONAL_DATE_CONT_NUM 3
// We use a guess of the number of months considering all of them 30 days
// long, but we split only the last 6 months.
#define HISTORY_DATE_CONT_NUM(_daysFromOldestVisit) \
  (HISTORY_ADDITIONAL_DATE_CONT_NUM + \
   NS_MIN(6, (PRInt32)NS_ceilf((float)_daysFromOldestVisit/30)))
// Max number of containers, used to initialize the params hash.
#define HISTORY_DATE_CONT_MAX 10

// Observed topics.
#ifdef MOZ_XUL
#define TOPIC_AUTOCOMPLETE_FEEDBACK_INCOMING "autocomplete-will-enter-text"
#endif
#define TOPIC_IDLE_DAILY "idle-daily"
#define TOPIC_PREF_CHANGED "nsPref:changed"
#define TOPIC_PROFILE_TEARDOWN "profile-change-teardown"
#define TOPIC_PROFILE_CHANGE "profile-before-change"

NS_IMPL_THREADSAFE_ADDREF(nsNavHistory)
NS_IMPL_THREADSAFE_RELEASE(nsNavHistory)

NS_IMPL_CLASSINFO(nsNavHistory, NULL, nsIClassInfo::SINGLETON,
                  NS_NAVHISTORYSERVICE_CID)
NS_INTERFACE_MAP_BEGIN(nsNavHistory)
  NS_INTERFACE_MAP_ENTRY(nsINavHistoryService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIGlobalHistory2, nsIGlobalHistory3)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalHistory3)
  NS_INTERFACE_MAP_ENTRY(nsIDownloadHistory)
  NS_INTERFACE_MAP_ENTRY(nsIBrowserHistory)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsICharsetResolver)
  NS_INTERFACE_MAP_ENTRY(nsPIPlacesDatabase)
  NS_INTERFACE_MAP_ENTRY(nsPIPlacesHistoryListenersNotifier)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsINavHistoryService)
  NS_IMPL_QUERY_CLASSINFO(nsNavHistory)
NS_INTERFACE_MAP_END

// We don't care about flattening everything
NS_IMPL_CI_INTERFACE_GETTER5(
  nsNavHistory
, nsINavHistoryService
, nsIGlobalHistory3
, nsIGlobalHistory2
, nsIDownloadHistory
, nsIBrowserHistory
)

namespace {

static PRInt64 GetSimpleBookmarksQueryFolder(
    const nsCOMArray<nsNavHistoryQuery>& aQueries,
    nsNavHistoryQueryOptions* aOptions);
static void ParseSearchTermsFromQueries(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                                        nsTArray<nsTArray<nsString>*>* aTerms);

class VacuumDBListener : public AsyncStatementCallback
{
public:
  NS_DECL_ISUPPORTS

  VacuumDBListener(nsIPrefBranch* aBranch)
    : mPrefBranch(aBranch)
  {
  }

  NS_IMETHOD HandleResult(mozIStorageResultSet*)
  {
    // 'PRAGMA journal_mode' statements always return a result.  Ignore it.
    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(PRUint16 aReason)
  {
    if (aReason == REASON_FINISHED && mPrefBranch) {
      (void)mPrefBranch->SetIntPref(PREF_LAST_VACUUM,
                                    (PRInt32)(PR_Now() / PR_USEC_PER_SEC));
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIPrefBranch> mPrefBranch;
};

NS_IMPL_ISUPPORTS1(VacuumDBListener, mozIStorageStatementCallback)

} // anonymous namespace

namespace mozilla {
  namespace places {

    bool hasRecentCorruptDB()
    {
      nsCOMPtr<nsIFile> profDir;
      nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                           getter_AddRefs(profDir));
      NS_ENSURE_SUCCESS(rv, false);
      nsCOMPtr<nsISimpleEnumerator> entries;
      rv = profDir->GetDirectoryEntries(getter_AddRefs(entries));
      NS_ENSURE_SUCCESS(rv, false);
      PRBool hasMore;
      while (NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> next;
        rv = entries->GetNext(getter_AddRefs(next));
        NS_ENSURE_SUCCESS(rv, false);
        nsCOMPtr<nsIFile> currFile = do_QueryInterface(next, &rv);
        NS_ENSURE_SUCCESS(rv, false);

        nsAutoString leafName;
        rv = currFile->GetLeafName(leafName);
        NS_ENSURE_SUCCESS(rv, false);
        if (leafName.Length() >= DATABASE_CORRUPT_FILENAME.Length() &&
            leafName.Find(".corrupt", DATABASE_FILENAME.Length()) != -1) {
          PRInt64 lastMod;
          rv = currFile->GetLastModifiedTime(&lastMod);
          NS_ENSURE_SUCCESS(rv, false);
          if (PR_Now() - lastMod > (PRInt64)24 * 60 * 60 * 1000 * 1000)
           return true;
        }
      }
      return false;
    }

    void GetTagsSqlFragment(PRInt64 aTagsFolder,
                            const nsACString& aRelation,
                            PRBool aHasSearchTerms,
                            nsACString& _sqlFragment) {
      if (!aHasSearchTerms)
        _sqlFragment.AssignLiteral("null");
      else {
        _sqlFragment.Assign(NS_LITERAL_CSTRING(
             "(SELECT GROUP_CONCAT(tag_title, ', ') "
              "FROM ( "
                "SELECT t_t.title AS tag_title "
                "FROM moz_bookmarks b_t "
                "JOIN moz_bookmarks t_t ON t_t.id = b_t.parent  "
                "WHERE b_t.fk = ") +
                aRelation + NS_LITERAL_CSTRING(" "
                "AND LENGTH(t_t.title) > 0 "
                "AND t_t.parent = ") +
                nsPrintfCString("%lld", aTagsFolder) + NS_LITERAL_CSTRING(" "
                "ORDER BY t_t.title COLLATE NOCASE ASC "
              ") "
             ")"));
      }

      _sqlFragment.AppendLiteral(" AS tags ");
    }

  } // namespace places
} // namespace mozilla


namespace {

/**
 * This class sets begin/end of batch updates to correspond to C++ scopes so
 * we can be sure end always gets called.
 */
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


class PlacesEvent : public nsRunnable
                  , public mozIStorageCompletionCallback
{
public:
  NS_DECL_ISUPPORTS

  PlacesEvent(const char* aTopic)
    : mTopic(aTopic)
    , mDoubleEnqueue(false)
  {
  }

  PlacesEvent(const char* aTopic,
              bool aDoubleEnqueue)
    : mTopic(aTopic)
    , mDoubleEnqueue(aDoubleEnqueue)
  {
  }

  NS_IMETHODIMP Run()
  {
    Notify();
    return NS_OK;
  }

  NS_IMETHODIMP Complete()
  {
    Notify();
    return NS_OK;
  }

protected:
  void Notify()
  {
    if (mDoubleEnqueue) {
      mDoubleEnqueue = false;
      (void)NS_DispatchToMainThread(this);
    }
    else {
      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      if (obs)
        (void)obs->NotifyObservers(nsnull, mTopic, nsnull);
    }
  }

  const char* mTopic;
  bool mDoubleEnqueue;
};

NS_IMPL_ISUPPORTS2(
  PlacesEvent
, mozIStorageCompletionCallback
, nsIRunnable
)

} // anonymouse namespace


// Queries rows indexes to bind or get values, if adding a new one, be sure to
// update nsNavBookmarks statements and its kGetChildrenIndex_* constants
const PRInt32 nsNavHistory::kGetInfoIndex_PageID = 0;
const PRInt32 nsNavHistory::kGetInfoIndex_URL = 1;
const PRInt32 nsNavHistory::kGetInfoIndex_Title = 2;
const PRInt32 nsNavHistory::kGetInfoIndex_RevHost = 3;
const PRInt32 nsNavHistory::kGetInfoIndex_VisitCount = 4;
const PRInt32 nsNavHistory::kGetInfoIndex_VisitDate = 5;
const PRInt32 nsNavHistory::kGetInfoIndex_FaviconURL = 6;
const PRInt32 nsNavHistory::kGetInfoIndex_SessionId = 7;
const PRInt32 nsNavHistory::kGetInfoIndex_ItemId = 8;
const PRInt32 nsNavHistory::kGetInfoIndex_ItemDateAdded = 9;
const PRInt32 nsNavHistory::kGetInfoIndex_ItemLastModified = 10;
const PRInt32 nsNavHistory::kGetInfoIndex_ItemParentId = 11;
const PRInt32 nsNavHistory::kGetInfoIndex_ItemTags = 12;


PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsNavHistory, gHistoryService)


nsNavHistory::nsNavHistory()
: mBatchLevel(0)
, mBatchDBTransaction(nsnull)
, mCachedNow(0)
, mExpireNowTimer(nsnull)
, mLastSessionID(0)
, mHistoryEnabled(PR_TRUE)
, mNumVisitsForFrecency(10)
, mTagsFolder(-1)
, mInPrivateBrowsing(PRIVATEBROWSING_NOTINITED)
, mDatabaseStatus(DATABASE_STATUS_OK)
, mHasHistoryEntries(-1)
, mCanNotify(true)
, mCacheObservers("history-observers")
{
  NS_ASSERTION(!gHistoryService,
               "Attempting to create two instances of the service!");
  gHistoryService = this;
}


nsNavHistory::~nsNavHistory()
{
  // remove the static reference to the service. Check to make sure its us
  // in case somebody creates an extra instance of the service.
  NS_ASSERTION(gHistoryService == this,
               "Deleting a non-singleton instance of the service");
  if (gHistoryService == this)
    gHistoryService = nsnull;
}


nsresult
nsNavHistory::Init()
{
  NS_TIME_FUNCTION;

  nsCOMPtr<nsIPrefService> prefService =
    do_GetService(NS_PREFSERVICE_CONTRACTID);
  nsCOMPtr<nsIPrefBranch> placesBranch;
  NS_ENSURE_TRUE(prefService, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = prefService->GetBranch(PREF_PLACES_BRANCH_BASE,
                                       getter_AddRefs(placesBranch));
  NS_ENSURE_SUCCESS(rv, rv);
  mPrefBranch = do_QueryInterface(placesBranch);
  LoadPrefs();

  // Init the database file.  If we won't be able to connect to the database it
  // is most likely corrupt, so we will backup it and create a new one.
  rv = InitDBFile(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Init the database schema.  If this will fail there's an high possibility
  // the schema is corrupt or incorrect, so we will force a new database
  // initialization.
  rv = InitDB();
  if (NS_FAILED(rv)) {
    // Forced InitDBFile will backup the old db and create a new one.
    rv = InitDBFile(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
    // Try to initialize the schema again on the new database.
    rv = InitDB();
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize all the items that are not part of the on-disk database, like
  // views, temp tables, functions.  Do not initialize these in InitDBFile, or
  // in case of failure we would mark the database as corrupt and try to
  // replace it, even if it's sane.
  rv = InitAdditionalDBItems();
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify we have finished database initialization.
  // Enqueue the notification, so if we init another service that requires
  // nsNavHistoryService we don't recursive try to get it.
  nsRefPtr<PlacesEvent> completeEvent =
    new PlacesEvent(TOPIC_PLACES_INIT_COMPLETE);
  rv = NS_DispatchToMainThread(completeEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  // recent events hash tables
  NS_ENSURE_TRUE(mRecentTyped.Init(128), NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mRecentLink.Init(128), NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mRecentBookmark.Init(128), NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mRecentRedirects.Init(128), NS_ERROR_OUT_OF_MEMORY);

  /*****************************************************************************
   *** IMPORTANT NOTICE!
   ***
   *** Nothing after these add observer calls should return anything but NS_OK.
   *** If a failure code is returned, this nsNavHistory object will be held onto
   *** by the observer service and the preference service. 
   ****************************************************************************/

  // Observe preferences branch for changes.
  if (mPrefBranch)
    mPrefBranch->AddObserver("", this, PR_FALSE);

  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (obsSvc) {
    (void)obsSvc->AddObserver(this, TOPIC_PROFILE_TEARDOWN, PR_FALSE);
    (void)obsSvc->AddObserver(this, TOPIC_PROFILE_CHANGE, PR_FALSE);
    (void)obsSvc->AddObserver(this, TOPIC_IDLE_DAILY, PR_FALSE);
    (void)obsSvc->AddObserver(this, NS_PRIVATE_BROWSING_SWITCH_TOPIC, PR_FALSE);
#ifdef MOZ_XUL
    (void)obsSvc->AddObserver(this, TOPIC_AUTOCOMPLETE_FEEDBACK_INCOMING, PR_FALSE);
#endif
  }

  // In case we've either imported or done a migration from a pre-frecency
  // build, we will calculate the first cutoff period's frecencies once the rest
  // of the places infrastructure has been initialized.
  if (mDatabaseStatus == DATABASE_STATUS_CREATE ||
      mDatabaseStatus == DATABASE_STATUS_UPGRADED) {
    if (mDatabaseStatus == DATABASE_STATUS_CREATE) {
      // Check if we should import old history from history.dat
      nsCOMPtr<nsIFile> historyFile;
      rv = NS_GetSpecialDirectory(NS_APP_HISTORY_50_FILE,
                                  getter_AddRefs(historyFile));
      if (NS_SUCCEEDED(rv) && historyFile) {
        (void)ImportHistory(historyFile);
      }
    }

    // In case we've either imported or done a migration from a pre-frecency
    // build, we will calculate the first cutoff period's frecencies once the
    // rest of the places infrastructure has been initialized.
    if (obsSvc)
      (void)obsSvc->AddObserver(this, TOPIC_PLACES_INIT_COMPLETE, PR_FALSE);
  }

  // Don't add code that can fail here! Do it up above, before we add our
  // observers.

  return NS_OK;
}


nsresult
nsNavHistory::InitDBFile(PRBool aForceInit)
{
  if (aForceInit) {
    NS_ASSERTION(mDBConn,
                 "When forcing initialization, a database connection must exist!");
    NS_ASSERTION(mDBService,
                 "When forcing initialization, the database service must exist!");
  }

  // Get database file handle.
  nsCOMPtr<nsIFile> profDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profDir));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = profDir->Clone(getter_AddRefs(mDBFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBFile->Append(DATABASE_FILENAME);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aForceInit) {
    // If forcing initialization, backup and remove the old file.  If we have
    // already failed in the last 24 hours avoid to create another corrupt file,
    // since doing so, in some situation, could cause us to create a new corrupt
    // file at every try to access any Places service.  That is bad because it
    // would quickly fill the user's disk space without any notice.
    if (!hasRecentCorruptDB()) {
      // backup the database
      nsCOMPtr<nsIFile> backup;
      rv = mDBService->BackupDatabaseFile(mDBFile, DATABASE_CORRUPT_FILENAME,
                                          profDir, getter_AddRefs(backup));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Close database connection if open.
    // If there's any not finalized statement or this fails for any reason
    // we won't be able to remove the database.
    rv = mDBConn->Close();
    NS_ENSURE_SUCCESS(rv, rv);

    // Remove the broken database.
    rv = mDBFile->Remove(PR_FALSE);
    if (NS_FAILED(rv)) {
      // If the file is still in use this will fail and we won't be able to
      // start with a clean database.  The process of backing up a corrupt
      // database will loop on the same database file at any next service
      // request.
      // We can't do much at this point, so fire a locked event so that user is
      // notified that we can't ensure Places to work.
      nsRefPtr<PlacesEvent> lockedEvent =
        new PlacesEvent(TOPIC_DATABASE_LOCKED);
      (void)NS_DispatchToMainThread(lockedEvent);
    }
    NS_ENSURE_SUCCESS(rv, rv);

    // If aForceInit is true we were unable to initialize or upgrade the current
    // database, so it was corrupt.
    mDatabaseStatus = DATABASE_STATUS_CORRUPT;
  }
  else {
    // Check if database file exists.
    PRBool dbExists = PR_TRUE;
    rv = mDBFile->Exists(&dbExists);
    NS_ENSURE_SUCCESS(rv, rv);
    // If the database didn't previously exist, we create it.
    if (!dbExists)
      mDatabaseStatus = DATABASE_STATUS_CREATE;
  }

  // Open the database file.  If it does not exist a new one will be created.
  mDBService = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  // Open un unshared connection, both for safety and speed.
  rv = mDBService->OpenUnsharedDatabase(mDBFile, getter_AddRefs(mDBConn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // The database is corrupt, try to create a new one.
    mDatabaseStatus = DATABASE_STATUS_CORRUPT;

    // Backup and remove old corrupt database file.
    nsCOMPtr<nsIFile> backup;
    rv = mDBService->BackupDatabaseFile(mDBFile, DATABASE_CORRUPT_FILENAME,
                                        profDir, getter_AddRefs(backup));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Try again to initialize the database file.
    rv = profDir->Clone(getter_AddRefs(mDBFile));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBFile->Append(DATABASE_FILENAME);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBService->OpenUnsharedDatabase(mDBFile, getter_AddRefs(mDBConn));
  }
 
  if (rv != NS_OK && rv != NS_ERROR_FILE_CORRUPTED) {
    // If the database cannot be opened for any reason other than corruption,
    // send out a notification and do not continue initialization.
    // Note: We swallow errors here, since we want service init to fail anyway.
    nsRefPtr<PlacesEvent> lockedEvent =
      new PlacesEvent(TOPIC_DATABASE_LOCKED);
    (void)NS_DispatchToMainThread(lockedEvent);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsNavHistory::InitDB()
{
  // Get the database schema version.
  PRInt32 currentSchemaVersion = 0;
  nsresult rv = mDBConn->GetSchemaVersion(&currentSchemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the page size.  This may be different than the default if the
  // database file already existed with a different page size.
  nsCOMPtr<mozIStorageStatement> statement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("PRAGMA page_size"),
                                getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  mozStorageStatementScoper scoper(statement);
  rv = statement->ExecuteStep(&hasResult);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasResult, NS_ERROR_FAILURE);
  PRInt32 pageSize = statement->AsInt32(0);

  // Ensure that temp tables are held in memory, not on disk.  We use temp
  // tables mainly for fsync and I/O reduction.
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "PRAGMA temp_store = MEMORY"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Set pragma synchronous to FULL to ensure maximum data integrity, even in
  // case of crashes or unclean shutdowns.
  // The suggested setting for SQLite is FULL, but Storage defaults to NORMAL.
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "PRAGMA synchronous = FULL"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Compute the size of the database cache using the device's memory size.
  // We don't use PRAGMA default_cache_size, since the database could be moved
  // among different devices and the value would adapt accordingly.
  PRInt32 cachePercentage;
  if (NS_FAILED(mPrefBranch->GetIntPref(PREF_CACHE_TO_MEMORY_PERCENTAGE,
                                        &cachePercentage)))
    cachePercentage = DATABASE_DEFAULT_CACHE_TO_MEMORY_PERCENTAGE;
  // Sanity checks, we allow values between 0 (disable cache) and 50%.
  if (cachePercentage > 50)
    cachePercentage = 50;
  if (cachePercentage < 0)
    cachePercentage = 0;

  static PRInt64 physMem = PR_GetPhysicalMemorySize();
  PRInt64 cacheSize = physMem * cachePercentage / 100;

  // Compute number of cached pages, this will be our cache size.
  PRInt64 cachePages = cacheSize / pageSize;
  nsCAutoString pageSizePragma("PRAGMA cache_size = ");
  pageSizePragma.AppendInt(cachePages);
  rv = mDBConn->ExecuteSimpleSQL(pageSizePragma);
  NS_ENSURE_SUCCESS(rv, rv);

  // Lock the database file.  This is done partly to avoid third party
  // applications to access it while it's in use, partly for performance.
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "PRAGMA locking_mode = EXCLUSIVE"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "PRAGMA journal_mode = " DATABASE_JOURNAL_MODE));
  NS_ENSURE_SUCCESS(rv, rv);

  // We are going to initialize tables, so everything from now on should be in
  // a transaction for performances.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // Grow places in 10MB increments
  mDBConn->SetGrowthIncrement(10 * 1024 * 1024, EmptyCString());

  // Initialize the other Places services' database tables before creating our
  // statements. Some of our statements depend on these external tables, such as
  // the bookmarks or favicon tables.
  rv = nsNavBookmarks::InitTables(mDBConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = nsFaviconService::InitTables(mDBConn);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = nsAnnotationService::InitTables(mDBConn);
  NS_ENSURE_SUCCESS(rv, rv);

  bool databaseInitialized = (currentSchemaVersion > 0);
  if (!databaseInitialized) {
    // This is the first run, so we set schema version to the latest one, since
    // we don't need to migrate anything.  We will create tables from scratch.
    rv = UpdateSchemaVersion();
    NS_ENSURE_SUCCESS(rv, rv);
    currentSchemaVersion = DATABASE_SCHEMA_VERSION;
  }

  if (DATABASE_SCHEMA_VERSION != currentSchemaVersion) {
    // Migration How-to:
    //
    // 1. increment PLACES_SCHEMA_VERSION.
    // 2. implement a method that performs up/sidegrade to your version
    //    from the current version.
    //
    // NOTE: We don't support downgrading back to History-only Places.
    // If you want to go from newer schema version back to V0, you'll need to
    // blow away your sqlite file. Subsequent up/downgrades have backwards and
    // forward migration code.
    //
    // XXX Backup places.sqlite to places-{version}.sqlite when doing db migration?
    
    if (currentSchemaVersion < DATABASE_SCHEMA_VERSION) {
      // Upgrading.
      mDatabaseStatus = DATABASE_STATUS_UPGRADED;

      // Migrate anno tables up to V3
      if (currentSchemaVersion < 3) {
        rv = MigrateV3Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Migrate bookmarks tables up to V5
      if (currentSchemaVersion < 5) {
        rv = ForceMigrateBookmarksDB(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Migrate anno tables up to V6
      if (currentSchemaVersion < 6) {
        rv = MigrateV6Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Migrate historyvisits and bookmarks up to V7
      if (currentSchemaVersion < 7) {
        rv = MigrateV7Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Migrate historyvisits up to V8
      if (currentSchemaVersion < 8) {
        rv = MigrateV8Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Migrate places up to V9
      if (currentSchemaVersion < 9) {
        rv = MigrateV9Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Migrate places up to V10
      if (currentSchemaVersion < 10) {
        rv = MigrateV10Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Schema Upgrades must add migration code here.

    } else {
      // Downgrading

      // XXX Need to prompt user or otherwise notify of 
      // potential dataloss when downgrading.

      // XXX Downgrades from >V6 must add migration code here.

      // Downgrade v1,2,4,5
      // v3,6 have no backwards incompatible changes.
      if (currentSchemaVersion > 2 && currentSchemaVersion < 6) {
        // perform downgrade to v2
        rv = ForceMigrateBookmarksDB(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    // Update schema version to the current one.
    rv = UpdateSchemaVersion();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!databaseInitialized) {
    // CREATE TABLE moz_places.
    rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_URL);
    NS_ENSURE_SUCCESS(rv, rv);

    // This index is used for favicon expiration, see nsNavHistoryExpire::ExpireItems.
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_FAVICON);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_REVHOST);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_VISITCOUNT);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_FRECENCY);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_LASTVISITDATE);
    NS_ENSURE_SUCCESS(rv, rv);

    // CREATE TABLE moz_historyvisits.
    rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_HISTORYVISITS);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_PLACEDATE);
    NS_ENSURE_SUCCESS(rv, rv);

    // This makes a big difference in startup time for large profiles because of
    // finding bookmark redirects using the referring page. 
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_FROMVISIT);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_VISITDATE);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_inputhistory
    rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_INPUTHISTORY);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // ANY FAILURE IN THIS METHOD WILL CAUSE US TO MARK THE DATABASE AS CORRUPT
  // AND TRY TO REPLACE IT.
  // DO NOT PUT HERE ANYTHING THAT IS NOT RELATED TO INITIALIZATION OR MODIFYING
  // THE DISK DATABASE.

  return NS_OK;
}


nsresult
nsNavHistory::InitAdditionalDBItems()
{
  nsresult rv = InitTempTables();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = InitViews();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = InitFunctions();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = InitStatements();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavHistory::GetDatabaseStatus(PRUint16 *aDatabaseStatus)
{
  NS_ENSURE_ARG_POINTER(aDatabaseStatus);
  *aDatabaseStatus = mDatabaseStatus;
  return NS_OK;
}


/**
 * Called by the individual services' InitTables().
 */
nsresult
nsNavHistory::UpdateSchemaVersion()
{
  return mDBConn->SetSchemaVersion(DATABASE_SCHEMA_VERSION);
}


PRUint32
nsNavHistory::GetRecentFlags(nsIURI *aURI)
{
  PRUint32 result = 0;
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Unable to get aURI's spec");

  if (NS_SUCCEEDED(rv)) {
    if (CheckIsRecentEvent(&mRecentTyped, spec))
      result |= RECENT_TYPED;
    if (CheckIsRecentEvent(&mRecentLink, spec))
      result |= RECENT_ACTIVATED;
    if (CheckIsRecentEvent(&mRecentBookmark, spec))
      result |= RECENT_BOOKMARKED;
  }

  return result;
}


/**
 * Called after InitDB, this creates our own functions
 */
class mozStorageFunctionGetUnreversedHost: public mozIStorageFunction
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS1(mozStorageFunctionGetUnreversedHost, mozIStorageFunction)

NS_IMETHODIMP
mozStorageFunctionGetUnreversedHost::OnFunctionCall(
  mozIStorageValueArray* aFunctionArguments,
  nsIVariant** _retval)
{
  NS_ASSERTION(aFunctionArguments, "Must have non-null function args");
  NS_ASSERTION(_retval, "Must have non-null return pointer");

  nsAutoString src;
  aFunctionArguments->GetString(0, src);

  nsresult rv;
  nsCOMPtr<nsIWritableVariant> result(do_CreateInstance(
      "@mozilla.org/variant;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  if (src.Length()>1) {
    src.Truncate(src.Length() - 1);
    nsAutoString dest;
    ReverseString(src, dest);
    result->SetAsAString(dest);
  } else {
    result->SetAsAString(EmptyString());
  }
  NS_ADDREF(*_retval = result);
  return NS_OK;
}

nsresult
nsNavHistory::InitTempTables()
{
  nsresult rv;

  // moz_places_temp
  rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_TEMP);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_TEMP_URL);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_TEMP_FAVICON);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_TEMP_REVHOST);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_TEMP_VISITCOUNT);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_TEMP_FRECENCY);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_SYNC_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);


  // moz_historyvisits_temp
  rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_HISTORYVISITS_TEMP);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_TEMP_PLACEDATE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_TEMP_FROMVISIT);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_TEMP_VISITDATE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_HISTORYVISITS_SYNC_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  // moz_openpages_temp
  rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_OPENPAGES_TEMP);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_REMOVEOPENPAGE_CLEANUP_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::InitViews()
{
  nsresult rv;

  // moz_places_view
  rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES_VIEW);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_PLACES_VIEW_INSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(CREATE_PLACES_VIEW_DELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(CREATE_PLACES_VIEW_UPDATE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  // moz_historyvisits_view
  rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_HISTORYVISITS_VIEW);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_VIEW_INSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_VIEW_DELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_VIEW_UPDATE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::InitFunctions()
{
  nsCOMPtr<mozIStorageFunction> func =
    new mozStorageFunctionGetUnreversedHost;
  NS_ENSURE_TRUE(func, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = mDBConn->CreateFunction(
    NS_LITERAL_CSTRING("get_unreversed_host"), 1, func
  );
  NS_ENSURE_SUCCESS(rv, rv);

  rv = MatchAutoCompleteFunction::create(mDBConn);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


/**
 * Called after InitDB, this creates our stored statements.
 */
nsresult
nsNavHistory::InitStatements()
{
  // mDBGetURLPageInfo
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // have unique urls.
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, url, title, rev_host, visit_count "
    "FROM moz_places_temp "
    "WHERE url = :page_url "
    "UNION ALL "
    "SELECT id, url, title, rev_host, visit_count "
    "FROM moz_places "
    "WHERE url = :page_url "
    "LIMIT 1"),
    getter_AddRefs(mDBGetURLPageInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetIdPageInfo
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // have unique place ids.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id, url, title, rev_host, visit_count "
      "FROM moz_places_temp "
      "WHERE id = :page_id "
      "UNION ALL "
      "SELECT id, url, title, rev_host, visit_count "
      "FROM moz_places "
      "WHERE id = :page_id "
      "LIMIT 1"),
    getter_AddRefs(mDBGetIdPageInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBRecentVisitOfURL
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // expect visits in temp table being the most recent.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id, session, visit_date "
      "FROM moz_historyvisits_temp "
      "WHERE place_id = IFNULL( "
        "(SELECT id FROM moz_places_temp WHERE url = :page_url), "
        "(SELECT id FROM moz_places WHERE url = :page_url) "
      ") "
      "UNION ALL "
      "SELECT id, session, visit_date "
      "FROM moz_historyvisits "
      "WHERE place_id = IFNULL( "
        "(SELECT id FROM moz_places_temp WHERE url = :page_url), "
        "(SELECT id FROM moz_places WHERE url = :page_url) "
      ") "
      "ORDER BY visit_date DESC "
      "LIMIT 1 "),
    getter_AddRefs(mDBRecentVisitOfURL));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBRecentVisitOfPlace
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // expect visits in temp table being the most recent.  
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id FROM moz_historyvisits_temp "
      "WHERE place_id = :page_id "
        "AND visit_date = :visit_date "
        "AND session = :session "
      "UNION ALL "
      "SELECT id FROM moz_historyvisits "
      "WHERE place_id = :page_id "
        "AND visit_date = :visit_date "
        "AND session = :session "
      "LIMIT 1"),
    getter_AddRefs(mDBRecentVisitOfPlace));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBInsertVisit
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO moz_historyvisits_view "
        "(from_visit, place_id, visit_date, visit_type, session) "
      "VALUES (:from_visit, :page_id, :visit_date, :visit_type, :session)"),
    getter_AddRefs(mDBInsertVisit));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetPageVisitStats (see InternalAdd)
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // have unique place ids.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id, visit_count, typed, hidden "
      "FROM moz_places_temp "
      "WHERE url = :page_url "
      "UNION ALL "
      "SELECT id, visit_count, typed, hidden "
      "FROM moz_places "
      "WHERE url = :page_url "
      "LIMIT 1"),
    getter_AddRefs(mDBGetPageVisitStats));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBIsPageVisited
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // only need to know if a visit exists.
  // Use indexed params here for performance.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id "
      "FROM moz_places_temp h "
      "WHERE url = ?1 " 
        "AND ( "
          "EXISTS(SELECT id FROM moz_historyvisits_temp WHERE place_id = h.id LIMIT 1) "
          "OR EXISTS(SELECT id FROM moz_historyvisits WHERE place_id = h.id LIMIT 1) "
        ") "
      "UNION ALL "
      "SELECT h.id "
      "FROM moz_places h "
      "WHERE url = ?1 "
      "AND ( "
        "EXISTS(SELECT id FROM moz_historyvisits_temp WHERE place_id = h.id LIMIT 1) "
        "OR EXISTS(SELECT id FROM moz_historyvisits WHERE place_id = h.id LIMIT 1) "
      ") "
      "LIMIT 1"), 
    getter_AddRefs(mDBIsPageVisited));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBUpdatePageVisitStats (see InternalAdd)
  // we don't need to update visit_count since it's maintained
  // in sync by triggers, and we must NEVER touch it
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places_view "
      "SET hidden = :hidden, typed = :typed "
      "WHERE id = :page_id"),
    getter_AddRefs(mDBUpdatePageVisitStats));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBAddNewPage (see InternalAddNewPage)
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO moz_places_view "
        "(url, title, rev_host, hidden, typed, frecency) "
      "VALUES (:page_url, :page_title, :rev_host, :hidden, :typed, :frecency)"),
    getter_AddRefs(mDBAddNewPage));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetTags
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "/* do not warn (bug 487594) */ "
      "SELECT GROUP_CONCAT(tag_title, ', ') "
      "FROM ( "
        "SELECT t.title AS tag_title "
        "FROM moz_bookmarks b "
        "JOIN moz_bookmarks t ON t.id = b.parent "
        "WHERE b.fk = IFNULL((SELECT id FROM moz_places_temp WHERE url = :page_url), "
                            "(SELECT id FROM moz_places WHERE url = :page_url)) "
          "AND LENGTH(t.title) > 0 "
          "AND b.type = ") +
            nsPrintfCString("%d", nsINavBookmarksService::TYPE_BOOKMARK) +
          NS_LITERAL_CSTRING(" AND t.parent = :tags_folder "
        "ORDER BY t.title COLLATE NOCASE ASC)"),
    getter_AddRefs(mDBGetTags));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetItemsWithAnno
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT a.item_id, a.content "
      "FROM moz_anno_attributes n "
      "JOIN moz_items_annos a ON n.id = a.anno_attribute_id "
      "WHERE n.name = :anno_name"),
    getter_AddRefs(mDBGetItemsWithAnno));
   NS_ENSURE_SUCCESS(rv, rv);

  // mDBSetPlaceTitle
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places_view "
      "SET title = :page_title "
      "WHERE url = :page_url"),
    getter_AddRefs(mDBSetPlaceTitle));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBRegisterOpenPage
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT OR REPLACE INTO moz_openpages_temp (url, open_count) "
      "VALUES (:page_url, "
        "IFNULL("
          "(SELECT open_count + 1 FROM moz_openpages_temp WHERE url = :page_url), "
          "1"
        ")"
      ")"),
    getter_AddRefs(mDBRegisterOpenPage));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBUnregisterOpenPage
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_openpages_temp "
      "SET open_count = open_count - 1 "
      "WHERE url = :page_url"),
    getter_AddRefs(mDBUnregisterOpenPage));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBVisitsForFrecency
  // NOTE: This is not limited to visits with "visit_type NOT IN (0,4,7,8)"
  // because otherwise mDBVisitsForFrecency would return no visits
  // for places with only embed (or undefined) visits.  That would
  // cause an incorrect frecency, see CalculateFrecencyInternal().
  // In such a case a place with only EMBED visits would result in a non-zero
  // frecency.
  // In case of a temporary or permanent redirect, calculate the frecency as if
  // the original page was visited.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT v.visit_date, COALESCE( "
        "(SELECT r.visit_type FROM moz_historyvisits_temp r "
          "WHERE v.visit_type IN ") +
            nsPrintfCString("(%d,%d) ", TRANSITION_REDIRECT_PERMANENT,
                                        TRANSITION_REDIRECT_TEMPORARY) +
            NS_LITERAL_CSTRING(" AND r.id = v.from_visit), "
        "(SELECT r.visit_type FROM moz_historyvisits r "
          "WHERE v.visit_type IN ") +
            nsPrintfCString("(%d,%d) ", TRANSITION_REDIRECT_PERMANENT,
                                        TRANSITION_REDIRECT_TEMPORARY) +
            NS_LITERAL_CSTRING(" AND r.id = v.from_visit), "
        "visit_type) "
      "FROM moz_historyvisits_temp v "
      "WHERE v.place_id = :page_id "
      "UNION ALL "
      "SELECT v.visit_date, COALESCE( "
        "(SELECT r.visit_type FROM moz_historyvisits_temp r "
          "WHERE v.visit_type IN ") +
            nsPrintfCString("(%d,%d) ", TRANSITION_REDIRECT_PERMANENT,
                                        TRANSITION_REDIRECT_TEMPORARY) +
            NS_LITERAL_CSTRING(" AND r.id = v.from_visit), "
        "(SELECT r.visit_type FROM moz_historyvisits r "
          "WHERE v.visit_type IN ") +
            nsPrintfCString("(%d,%d) ", TRANSITION_REDIRECT_PERMANENT,
                                        TRANSITION_REDIRECT_TEMPORARY) +
            NS_LITERAL_CSTRING(" AND r.id = v.from_visit), "
        "visit_type) "
      "FROM moz_historyvisits v "
      "WHERE v.place_id = :page_id "
        "AND v.id NOT IN (SELECT id FROM moz_historyvisits_temp) "
      "ORDER BY visit_date DESC LIMIT ") +
        nsPrintfCString("%d", mNumVisitsForFrecency),
    getter_AddRefs(mDBVisitsForFrecency));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBUpdateFrecencyAndHidden
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places_view SET frecency = :frecency, hidden = :hidden "
      "WHERE id = :page_id"),
    getter_AddRefs(mDBUpdateFrecencyAndHidden));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetPlaceVisitStats
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // have unique place ids.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT typed, hidden, frecency "
      "FROM moz_places_temp WHERE id = :page_id "
      "UNION ALL "
      "SELECT typed, hidden, frecency "
      "FROM moz_places WHERE id = :page_id "
      "LIMIT 1"),
    getter_AddRefs(mDBGetPlaceVisitStats));
  NS_ENSURE_SUCCESS(rv, rv);

  // when calculating frecency, we want the visit count to be 
  // all the visits.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT "
        "(SELECT COUNT(*) FROM moz_historyvisits WHERE place_id = :page_id) + "
        "(SELECT COUNT(*) FROM moz_historyvisits_temp WHERE place_id = :page_id "
            "AND id NOT IN (SELECT id FROM moz_historyvisits))"),
    getter_AddRefs(mDBFullVisitCount));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


/**
 * This dumps all bookmarks-related tables, and recreates them,
 * forcing a re-import of bookmarks.html.
 *
 * @note This may cause data-loss if downgrading!
 *       Only use this for migration if you're sure that bookmarks.html
 *       and the target version support all bookmarks fields.
 */
nsresult
nsNavHistory::ForceMigrateBookmarksDB(mozIStorageConnection* aDBConn) 
{
  // drop bookmarks tables
  nsresult rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP TABLE IF EXISTS moz_bookmarks"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP TABLE IF EXISTS moz_bookmarks_folders"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP TABLE IF EXISTS moz_bookmarks_roots"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP TABLE IF EXISTS moz_keywords"));
  NS_ENSURE_SUCCESS(rv, rv);

  // initialize bookmarks tables
  rv = nsNavBookmarks::InitTables(aDBConn);
  NS_ENSURE_SUCCESS(rv, rv);

  // We have done a new database init, so we mark this as if the database has
  // been created now, so the frontend can distinguish this status and import
  // if needed.
  mDatabaseStatus = DATABASE_STATUS_CREATE;

  return NS_OK;
}


nsresult
nsNavHistory::MigrateV3Up(mozIStorageConnection* aDBConn) 
{
  // if type col is already there, then a partial update occurred.
  // return, making no changes, and allowing db version to be updated.
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT type from moz_annos"),
    getter_AddRefs(statement));
  if (NS_SUCCEEDED(rv))
    return NS_OK;

  // add type column to moz_annos
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_annos ADD type INTEGER DEFAULT 0"));
  if (NS_FAILED(rv)) {
    // if the alteration failed, force-migrate
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP TABLE IF EXISTS moz_annos"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = nsAnnotationService::InitTables(mDBConn);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


nsresult
nsNavHistory::MigrateV6Up(mozIStorageConnection* aDBConn) 
{
  mozStorageTransaction transaction(aDBConn, PR_FALSE);

  // if dateAdded & lastModified cols are already there, then a partial update occurred,
  // and so we should not attempt to add these cols.
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT a.dateAdded, a.lastModified FROM moz_annos a"), 
    getter_AddRefs(statement));
  if (NS_FAILED(rv)) {
    // add dateAdded and lastModified columns to moz_annos
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_annos ADD dateAdded INTEGER DEFAULT 0"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_annos ADD lastModified INTEGER DEFAULT 0"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // if dateAdded & lastModified cols are already there, then a partial update occurred,
  // and so we should not attempt to add these cols.  see bug #408443 for details.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT b.dateAdded, b.lastModified FROM moz_items_annos b"), 
    getter_AddRefs(statement));
  if (NS_FAILED(rv)) {
    // add dateAdded and lastModified columns to moz_items_annos
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_items_annos ADD dateAdded INTEGER DEFAULT 0"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_items_annos ADD lastModified INTEGER DEFAULT 0"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // we used to create an indexes on moz_favicons.url and
  // moz_anno_attributes.name, but those indexes are not needed
  // because those columns are UNIQUE, so remove them.
  // see bug #386303 for more details
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP INDEX IF EXISTS moz_favicons_url"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP INDEX IF EXISTS moz_anno_attributes_nameindex"));
  NS_ENSURE_SUCCESS(rv, rv);


  // bug #371800 - remove moz_places.user_title
  // test for moz_places.user_title
  nsCOMPtr<mozIStorageStatement> statement2;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT user_title FROM moz_places"),
    getter_AddRefs(statement2));
  if (NS_SUCCEEDED(rv)) {
    // 1. Indexes are moved along with the renamed table. Since we're dropping
    // that table, we're also dropping its indexes, and later re-creating them
    // for the new table.
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP INDEX IF EXISTS moz_places_urlindex"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP INDEX IF EXISTS moz_places_titleindex"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP INDEX IF EXISTS moz_places_faviconindex"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP INDEX IF EXISTS moz_places_hostindex"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP INDEX IF EXISTS moz_places_visitcount"));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP INDEX IF EXISTS moz_places_frecencyindex"));
    NS_ENSURE_SUCCESS(rv, rv);

    // 2. remove any duplicate URIs
    rv = RemoveDuplicateURIs();
    NS_ENSURE_SUCCESS(rv, rv);

    // 3. rename moz_places to moz_places_backup
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_places RENAME TO moz_places_backup"));
    NS_ENSURE_SUCCESS(rv, rv);

    // 4. create moz_places w/o user_title
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE TABLE moz_places ("
          "id INTEGER PRIMARY KEY, "
          "url LONGVARCHAR, "
          "title LONGVARCHAR, "
          "rev_host LONGVARCHAR, "
          "visit_count INTEGER DEFAULT 0, "
          "hidden INTEGER DEFAULT 0 NOT NULL, "
          "typed INTEGER DEFAULT 0 NOT NULL, "
          "favicon_id INTEGER, "
          "frecency INTEGER DEFAULT -1 NOT NULL)"));
    NS_ENSURE_SUCCESS(rv, rv);

    // 5. recreate the indexes
    // NOTE: tests showed that it's faster to create the indexes prior to filling
    // the table than it is to add them afterwards.
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_URL);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_FAVICON);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_REVHOST);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_VISITCOUNT);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_FRECENCY);
    NS_ENSURE_SUCCESS(rv, rv);

    // 6. copy all data into moz_places
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "INSERT INTO moz_places (" MOZ_PLACES_COLUMNS ")"
        "SELECT " MOZ_PLACES_COLUMNS " FROM moz_places_backup"));
    NS_ENSURE_SUCCESS(rv, rv);

    // 7. drop moz_places_backup
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP TABLE moz_places_backup"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return transaction.Commit();
}


nsresult
nsNavHistory::MigrateV7Up(mozIStorageConnection* aDBConn) 
{
  mozStorageTransaction transaction(aDBConn, PR_FALSE);

  // We need an index on lastModified to catch quickly last modified bookmark
  // title for tag container's children. This will be useful for sync too.
  PRBool lastModIndexExists = PR_FALSE;
  nsresult rv = aDBConn->IndexExists(
    NS_LITERAL_CSTRING("moz_bookmarks_itemlastmodifiedindex"),
    &lastModIndexExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!lastModIndexExists) {
    rv = aDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_PLACELASTMODIFIED);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We need to do a one-time change of the moz_historyvisits.pageindex
  // to speed up finding last visit date when joinin with moz_places.
  // See bug 392399 for more details.
  PRBool pageIndexExists = PR_FALSE;
  rv = aDBConn->IndexExists(
    NS_LITERAL_CSTRING("moz_historyvisits_pageindex"), &pageIndexExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (pageIndexExists) {
    // drop old index
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP INDEX IF EXISTS moz_historyvisits_pageindex"));
    NS_ENSURE_SUCCESS(rv, rv);

    // create the new multi-column index
    rv = aDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_PLACEDATE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // for existing profiles, we may not have a frecency column
  nsCOMPtr<mozIStorageStatement> hasFrecencyStatement;
  rv = aDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT frecency FROM moz_places"),
    getter_AddRefs(hasFrecencyStatement));

  if (NS_FAILED(rv)) {
    // Add frecency column to moz_places, default to -1 so that all the
    // frecencies are invalid
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_places ADD frecency INTEGER DEFAULT -1 NOT NULL"));
    NS_ENSURE_SUCCESS(rv, rv);

    // create index for the frecency column
    // XXX multi column index with typed, and visit_count?
    rv = aDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_FRECENCY);
    NS_ENSURE_SUCCESS(rv, rv);

    // for place: items and unvisited livemark items, we need to set
    // the frecency to 0 so that they don't show up in url bar autocomplete
    rv = FixInvalidFrecenciesForExcludedPlaces();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Temporary migration code for bug 396300
  nsCOMPtr<mozIStorageStatement> moveUnfiledBookmarks;
  rv = aDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_bookmarks "
      "SET parent = ("
        "SELECT folder_id "
        "FROM moz_bookmarks_roots "
        "WHERE root_name = :root_name "
      ") "
      "WHERE type = :item_type "
      "AND parent = ("
        "SELECT folder_id "
        "FROM moz_bookmarks_roots "
        "WHERE root_name = :parent_name "
      ")"),
    getter_AddRefs(moveUnfiledBookmarks));
  rv = moveUnfiledBookmarks->BindUTF8StringByName(
    NS_LITERAL_CSTRING("root_name"), NS_LITERAL_CSTRING("unfiled")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moveUnfiledBookmarks->BindInt32ByName(
    NS_LITERAL_CSTRING("item_type"), nsINavBookmarksService::TYPE_BOOKMARK
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moveUnfiledBookmarks->BindUTF8StringByName(
    NS_LITERAL_CSTRING("parent_name"), NS_LITERAL_CSTRING("places")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moveUnfiledBookmarks->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a statement to test for trigger creation
  nsCOMPtr<mozIStorageStatement> triggerDetection;
  rv = aDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT name "
      "FROM sqlite_master "
      "WHERE type = 'trigger' "
      "AND name = :trigger_name"),
    getter_AddRefs(triggerDetection));
  NS_ENSURE_SUCCESS(rv, rv);

  // Check for existence
  PRBool triggerExists;
  rv = triggerDetection->BindUTF8StringByName(
    NS_LITERAL_CSTRING("trigger_name"),
    NS_LITERAL_CSTRING("moz_historyvisits_afterinsert_v1_trigger")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = triggerDetection->ExecuteStep(&triggerExists);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = triggerDetection->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to create two triggers on moz_historyvists to maintain the
  // accuracy of moz_places.visit_count.  For this to work, we must ensure that
  // all moz_places.visit_count values are correct.
  // See bug 416313 for details.
  if (!triggerExists) {
    // First, we do a one-time reset of all the moz_places.visit_count values.
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "UPDATE moz_places SET visit_count = "
          "(SELECT count(*) FROM moz_historyvisits "
           "WHERE place_id = moz_places.id "
            "AND visit_type NOT IN ") +
              nsPrintfCString("(0,%d,%d,%d) ",
                              nsINavHistoryService::TRANSITION_EMBED,
                              nsINavHistoryService::TRANSITION_FRAMED_LINK,
                              nsINavHistoryService::TRANSITION_DOWNLOAD) +
          NS_LITERAL_CSTRING(")"));
    NS_ENSURE_SUCCESS(rv, rv);

    // We used to create two triggers here, but we no longer need that with
    // schema version eight and greater.  We've removed their creation here as
    // a result.
  }

  // Check for existence
  rv = triggerDetection->BindUTF8StringByName(
    NS_LITERAL_CSTRING("trigger_name"),
    NS_LITERAL_CSTRING("moz_bookmarks_beforedelete_v1_trigger")
  );
  NS_ENSURE_SUCCESS(rv, rv);
  rv = triggerDetection->ExecuteStep(&triggerExists);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = triggerDetection->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to create one trigger on moz_bookmarks to remove unused keywords.
  // See bug 421180 for details.
  if (!triggerExists) {
    // First, remove any existing dangling keywords
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DELETE FROM moz_keywords "
        "WHERE id IN ("
          "SELECT k.id "
          "FROM moz_keywords k "
          "LEFT OUTER JOIN moz_bookmarks b "
          "ON b.keyword_id = k.id "
          "WHERE b.id IS NULL"
        ")"));
    NS_ENSURE_SUCCESS(rv, rv);

    // Now we create our trigger
    rv = aDBConn->ExecuteSimpleSQL(CREATE_KEYWORD_VALIDITY_TRIGGER);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return transaction.Commit();
}


nsresult
nsNavHistory::MigrateV8Up(mozIStorageConnection *aDBConn)
{
  mozStorageTransaction transaction(aDBConn, PR_FALSE);

  nsresult rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP TRIGGER IF EXISTS moz_historyvisits_afterinsert_v1_trigger"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP TRIGGER IF EXISTS moz_historyvisits_afterdelete_v1_trigger"));
  NS_ENSURE_SUCCESS(rv, rv);


  // bug #381795 - remove unused indexes
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP INDEX IF EXISTS moz_places_titleindex"));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DROP INDEX IF EXISTS moz_annos_item_idindex"));
  NS_ENSURE_SUCCESS(rv, rv);


  // Do a one-time re-creation of the moz_annos indexes (bug 415201)
  PRBool oldIndexExists = PR_FALSE;
  rv = mDBConn->IndexExists(NS_LITERAL_CSTRING("moz_annos_attributesindex"), &oldIndexExists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (oldIndexExists) {
    // drop old uri annos index
    rv = mDBConn->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("DROP INDEX moz_annos_attributesindex"));
    NS_ENSURE_SUCCESS(rv, rv);

    // create new uri annos index
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_ANNOS_PLACEATTRIBUTE);
    NS_ENSURE_SUCCESS(rv, rv);

    // drop old item annos index
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP INDEX IF EXISTS moz_items_annos_attributesindex"));
    NS_ENSURE_SUCCESS(rv, rv);

    // create new item annos index
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_ITEMSANNOS_PLACEATTRIBUTE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return transaction.Commit();
}


nsresult
nsNavHistory::MigrateV9Up(mozIStorageConnection *aDBConn)
{
  mozStorageTransaction transaction(aDBConn, PR_FALSE);
  // Added in Bug 488966.  The last_visit_date column caches the last
  // visit date, this enhances SELECT performances when we
  // need to sort visits by visit date.
  // The cached value is synced by INSERT and DELETE triggers on
  // moz_historyvisits_view, on every added or removed visit.
  // See nsPlacesTriggers.h for details on the triggers.
  PRBool oldIndexExists = PR_FALSE;
  nsresult rv = mDBConn->IndexExists(
    NS_LITERAL_CSTRING("moz_places_lastvisitdateindex"), &oldIndexExists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!oldIndexExists) {
    // Add last_visit_date column to moz_places.
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "ALTER TABLE moz_places ADD last_visit_date INTEGER"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_LASTVISITDATE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now let's sync the column contents with real visit dates.
    // This query can be really slow due to disk access, since it will basically
    // dupe the table contents in the journal file, and then write them down
    // in the database.
    // We will temporary use a memory journal file, this has the advantage of
    // reducing write times by a half, but will temporary consume more memory
    // and increase risks of corruption if we should crash in the middle of this
    // update.
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "PRAGMA journal_mode = MEMORY"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "UPDATE moz_places SET last_visit_date = "
          "(SELECT MAX(visit_date) "
           "FROM moz_historyvisits "
           "WHERE place_id = moz_places.id)"));
    NS_ENSURE_SUCCESS(rv, rv);

    // Restore the default journal mode.
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "PRAGMA journal_mode = " DATABASE_JOURNAL_MODE));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return transaction.Commit();
}


nsresult
nsNavHistory::MigrateV10Up(mozIStorageConnection *aDBConn)
{
  // LastModified is set to the same value as dateAdded on item creation.
  // This way we can use lastModified index to sort.
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "UPDATE moz_bookmarks SET lastModified = dateAdded "
      "WHERE lastModified IS NULL"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
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

  mozStorageStatementScoper statementResetter(mDBGetURLPageInfo);
  nsresult rv = URIBinder::Bind(mDBGetURLPageInfo, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasEntry = PR_FALSE;
  rv = mDBGetURLPageInfo->ExecuteStep(&hasEntry);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasEntry)
    return mDBGetURLPageInfo->GetInt64(kGetInfoIndex_PageID, aEntryID);

  if (aAutoCreate) {
    // create a new hidden, untyped, unvisited entry
    mDBGetURLPageInfo->Reset();
    statementResetter.Abandon();
    nsString voidString;
    voidString.SetIsVoid(PR_TRUE);
    return InternalAddNewPage(aURI, voidString, PR_TRUE, PR_FALSE, 0, PR_TRUE, aEntryID);
  }

  // Doesn't exist: don't do anything, entry ID was already set to 0 above
  return NS_OK;
}


// nsNavHistory::InternalAddNewPage
//
//    Adds a new page to the DB.
//    THIS SHOULD BE THE ONLY PLACE NEW moz_places ROWS ARE
//    CREATED. This allows us to maintain better consistency.
//
//    XXX this functionality is being moved to History.cpp, so
//    in fact there *are* two places where new pages are added.
//
//    If non-null, the new page ID will be placed into aPageID.

nsresult
nsNavHistory::InternalAddNewPage(nsIURI* aURI,
                                 const nsAString& aTitle,
                                 PRBool aHidden,
                                 PRBool aTyped,
                                 PRInt32 aVisitCount,
                                 PRBool aCalculateFrecency,
                                 PRInt64* aPageID)
{
  mozStorageStatementScoper scoper(mDBAddNewPage);
  nsresult rv = URIBinder::Bind(mDBAddNewPage, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  if (aTitle.IsVoid()) {
    rv = mDBAddNewPage->BindNullByName(NS_LITERAL_CSTRING("page_title"));
  }
  else {
    rv = mDBAddNewPage->BindStringByName(
      NS_LITERAL_CSTRING("page_title"), StringHead(aTitle, TITLE_LENGTH_MAX)
    );
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // host (reversed with trailing period)
  nsAutoString revHost;
  rv = GetReversedHostname(aURI, revHost);
  // Not all URI types have hostnames, so this is optional.
  if (NS_SUCCEEDED(rv)) {
    rv = mDBAddNewPage->BindStringByName(NS_LITERAL_CSTRING("rev_host"), revHost);
  } else {
    rv = mDBAddNewPage->BindNullByName(NS_LITERAL_CSTRING("rev_host"));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // hidden
  rv = mDBAddNewPage->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), aHidden);
  NS_ENSURE_SUCCESS(rv, rv);

  // typed
  rv = mDBAddNewPage->BindInt32ByName(NS_LITERAL_CSTRING("typed"), aTyped);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString url;
  rv = aURI->GetSpec(url);
  NS_ENSURE_SUCCESS(rv, rv);

  // frecency
  PRInt32 frecency = -1;
  if (aCalculateFrecency) {
    rv = CalculateFrecency(-1 /* no page id, since this page doesn't exist */,
                           aTyped, aVisitCount, url, &frecency);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mDBAddNewPage->BindInt32ByName(NS_LITERAL_CSTRING("frecency"), frecency);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBAddNewPage->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // If the caller wants the page ID, go get it
  if (aPageID) {
    mozStorageStatementScoper scoper(mDBGetURLPageInfo);

    rv = URIBinder::Bind(mDBGetURLPageInfo, NS_LITERAL_CSTRING("page_url"), aURI);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult = PR_FALSE;
    rv = mDBGetURLPageInfo->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(hasResult, "hasResult is false but the call succeeded?");

    *aPageID = mDBGetURLPageInfo->AsInt64(0);
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

  {
    mozStorageStatementScoper scoper(mDBInsertVisit);
  
    rv = mDBInsertVisit->BindInt64ByName(NS_LITERAL_CSTRING("from_visit"), aReferringVisit);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBInsertVisit->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPageID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBInsertVisit->BindInt64ByName(NS_LITERAL_CSTRING("visit_date"), aTime);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBInsertVisit->BindInt32ByName(NS_LITERAL_CSTRING("visit_type"), aTransitionType);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBInsertVisit->BindInt64ByName(NS_LITERAL_CSTRING("session"), aSessionID);
    NS_ENSURE_SUCCESS(rv, rv);
  
    rv = mDBInsertVisit->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  {
    mozStorageStatementScoper scoper(mDBRecentVisitOfPlace);

    rv = mDBRecentVisitOfPlace->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPageID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBRecentVisitOfPlace->BindInt64ByName(NS_LITERAL_CSTRING("visit_date"), aTime);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBRecentVisitOfPlace->BindInt64ByName(NS_LITERAL_CSTRING("session"), aSessionID);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = mDBRecentVisitOfPlace->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(hasResult, "hasResult is false but the call succeeded?");

    rv = mDBRecentVisitOfPlace->GetInt64(0, visitID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Invalidate the cached value for whether there's history or not.
  mHasHistoryEntries = -1;

  return NS_OK;
}


// nsNavHistory::FindLastVisit
//
//    This finds the most recent visit to the given URL. If found, it will put
//    that visit's ID and session into the respective out parameters and return
//    true. Returns false if no visit is found.
//
//    This is used to compute the referring visit.

PRBool
nsNavHistory::FindLastVisit(nsIURI* aURI,
                            PRInt64* aVisitID,
                            PRTime* aTime,
                            PRInt64* aSessionID)
{
  mozStorageStatementScoper scoper(mDBRecentVisitOfURL);
  nsresult rv = URIBinder::Bind(mDBRecentVisitOfURL,
                                NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  PRBool hasMore;
  rv = mDBRecentVisitOfURL->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  if (hasMore) {
    rv = mDBRecentVisitOfURL->GetInt64(0, aVisitID);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    rv = mDBRecentVisitOfURL->GetInt64(1, aSessionID);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    rv = mDBRecentVisitOfURL->GetInt64(2, aTime);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
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
  mozStorageStatementScoper scoper(mDBIsPageVisited);
  nsresult rv = URIBinder::Bind(mDBIsPageVisited, 0, aURIString);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  PRBool hasMore = PR_FALSE;
  rv = mDBIsPageVisited->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  return hasMore;
}


void
nsNavHistory::LoadPrefs()
{
  if (!mPrefBranch)
    return;

  // History preferences.
  // Check the old preference and migrate disabled state.
  nsCOMPtr<nsIPrefBranch> prefSvc = do_GetService(NS_PREFSERVICE_CONTRACTID);
  PRInt32 oldDaysPref = 0;
  if (prefSvc &&
      NS_SUCCEEDED(prefSvc->GetIntPref("browser.history_expire_days",
                                       &oldDaysPref))) {
    if (!oldDaysPref) {
      // Preserve history disabled state, for privacy reasons.
      mPrefBranch->SetBoolPref(PREF_HISTORY_ENABLED, PR_FALSE);
      mHistoryEnabled = PR_FALSE;
    }
    // Clear the old pref, otherwise we will keep using it.
    prefSvc->ClearUserPref("browser.history_expire_days");
  }
  else
    mPrefBranch->GetBoolPref(PREF_HISTORY_ENABLED, &mHistoryEnabled);

  // Frecency preferences.
  mPrefBranch->GetIntPref(PREF_FRECENCY_NUM_VISITS,
    &mNumVisitsForFrecency);
  mPrefBranch->GetIntPref(PREF_FRECENCY_FIRST_BUCKET_CUTOFF,
    &mFirstBucketCutoffInDays);
  mPrefBranch->GetIntPref(PREF_FRECENCY_SECOND_BUCKET_CUTOFF,
    &mSecondBucketCutoffInDays);
  mPrefBranch->GetIntPref(PREF_FRECENCY_THIRD_BUCKET_CUTOFF,
    &mThirdBucketCutoffInDays);
  mPrefBranch->GetIntPref(PREF_FRECENCY_FOURTH_BUCKET_CUTOFF,
    &mFourthBucketCutoffInDays);
  mPrefBranch->GetIntPref(PREF_FRECENCY_EMBED_VISIT_BONUS,
    &mEmbedVisitBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_FRAMED_LINK_VISIT_BONUS,
    &mFramedLinkVisitBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_LINK_VISIT_BONUS,
    &mLinkVisitBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_TYPED_VISIT_BONUS,
    &mTypedVisitBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_BOOKMARK_VISIT_BONUS,
    &mBookmarkVisitBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_DOWNLOAD_VISIT_BONUS,
    &mDownloadVisitBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_PERM_REDIRECT_VISIT_BONUS,
    &mPermRedirectVisitBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_TEMP_REDIRECT_VISIT_BONUS,
    &mTempRedirectVisitBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_DEFAULT_VISIT_BONUS,
    &mDefaultVisitBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_UNVISITED_BOOKMARK_BONUS,
    &mUnvisitedBookmarkBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_UNVISITED_TYPED_BONUS,
    &mUnvisitedTypedBonus);
  mPrefBranch->GetIntPref(PREF_FRECENCY_FIRST_BUCKET_WEIGHT,
    &mFirstBucketWeight);
  mPrefBranch->GetIntPref(PREF_FRECENCY_SECOND_BUCKET_WEIGHT,
    &mSecondBucketWeight);
  mPrefBranch->GetIntPref(PREF_FRECENCY_THIRD_BUCKET_WEIGHT,
    &mThirdBucketWeight);
  mPrefBranch->GetIntPref(PREF_FRECENCY_FOURTH_BUCKET_WEIGHT,
    &mFourthBucketWeight);
  mPrefBranch->GetIntPref(PREF_FRECENCY_DEFAULT_BUCKET_WEIGHT,
    &mDefaultWeight);
}


PRInt64
nsNavHistory::GetNewSessionID()
{
  // Use cached value if already initialized.
  if (mLastSessionID)
    return ++mLastSessionID;

  // Extract the last session ID, so we know where to pick up. There is no
  // index over sessions so we use the visit_date index.
  // This happens on the first visit, so we don't care about temp tables.
  nsCOMPtr<mozIStorageStatement> selectSession;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT session FROM moz_historyvisits "
      "ORDER BY visit_date DESC LIMIT 1"),
    getter_AddRefs(selectSession));
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool hasSession;
  if (NS_SUCCEEDED(selectSession->ExecuteStep(&hasSession)) && hasSession)
    mLastSessionID = selectSession->AsInt64(0) + 1;
  else
    mLastSessionID = 1;

  return mLastSessionID;
}


void
nsNavHistory::NotifyOnVisit(nsIURI* aURI,
                          PRInt64 aVisitID,
                          PRTime aTime,
                          PRInt64 aSessionID,
                          PRInt64 referringVisitID,
                          PRInt32 aTransitionType)
{
  PRUint32 added = 0;
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver,
                   OnVisit(aURI, aVisitID, aTime, aSessionID,
                           referringVisitID, aTransitionType, &added));
}

void
nsNavHistory::NotifyTitleChange(nsIURI* aURI, const nsString& aTitle)
{
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver, OnTitleChanged(aURI, aTitle));
}

PRInt32
nsNavHistory::GetDaysOfHistory() {
  PRInt32 daysOfHistory = 0;
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT ROUND(( "
        "strftime('%s','now','localtime','utc') - "
        "( "
          "SELECT visit_date FROM moz_historyvisits "
          "UNION ALL "
          "SELECT visit_date FROM moz_historyvisits_temp "
          "ORDER BY visit_date ASC LIMIT 1 "
        ")/1000000 "
      ")/86400) AS daysOfHistory "),
    getter_AddRefs(statement));
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Unable to create statement.");
  NS_ENSURE_SUCCESS(rv, 0);
  PRBool hasResult;
  if (NS_SUCCEEDED(statement->ExecuteStep(&hasResult)) && hasResult)
    statement->GetInt32(0, &daysOfHistory);

  return daysOfHistory;
}


PRTime
nsNavHistory::GetNow()
{
  if (!mCachedNow) {
    mCachedNow = PR_Now();
    if (!mExpireNowTimer)
      mExpireNowTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mExpireNowTimer)
      mExpireNowTimer->InitWithFuncCallback(expireNowTimerCallback, this,
                                            RENEW_CACHED_NOW_TIMEOUT,
                                            nsITimer::TYPE_ONE_SHOT);
  }
  return mCachedNow;
}


void nsNavHistory::expireNowTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsNavHistory *history = static_cast<nsNavHistory *>(aClosure);
  if (history) {
    history->mCachedNow = 0;
    history->mExpireNowTimer = 0;
  }
}


/**
 * Code borrowed from mozilla/xpfe/components/history/src/nsGlobalHistory.cpp
 * Pass in a pre-normalized now and a date, and we'll find the difference since
 * midnight on each of the days.
 */
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

// nsNavHistory::GetUpdateRequirements
//
//    Returns conditions for query update.
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

  PRBool nonTimeBasedItems = PR_FALSE;
  PRBool domainBasedItems = PR_FALSE;
  PRBool queryContainsTransitions = PR_FALSE;

  for (i = 0; i < aQueries.Count(); i ++) {
    nsNavHistoryQuery* query = aQueries[i];

    if (query->Folders().Length() > 0 ||
        query->OnlyBookmarked() ||
        query->Tags().Length() > 0) {
      return QUERYUPDATE_COMPLEX_WITH_BOOKMARKS;
    }

    if (query->Transitions().Length() > 0)
      queryContainsTransitions = PR_TRUE;

    // Note: we don't currently have any complex non-bookmarked items, but these
    // are expected to be added. Put detection of these items here.
    if (! query->SearchTerms().IsEmpty() ||
        ! query->Domain().IsVoid() ||
        query->Uri() != nsnull)
      nonTimeBasedItems = PR_TRUE;

    if (! query->Domain().IsVoid())
      domainBasedItems = PR_TRUE;
  }

  if (aOptions->ResultType() ==
      nsINavHistoryQueryOptions::RESULTS_AS_TAG_QUERY)
    return QUERYUPDATE_COMPLEX_WITH_BOOKMARKS;

  if (queryContainsTransitions)
    return QUERYUPDATE_COMPLEX;

  // Whenever there is a maximum number of results, 
  // and we are not a bookmark query we must requery. This
  // is because we can't generally know if any given addition/change causes
  // the item to be in the top N items in the database.
  if (aOptions->MaxResults() > 0)
    return QUERYUPDATE_COMPLEX;

  if (aQueries.Count() == 1 && domainBasedItems)
    return QUERYUPDATE_HOST;
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
      nsCOMArray<nsNavHistoryQuery> queries;
      queries.AppendObject(query);
      nsCOMArray<nsNavHistoryResultNode> filteredSet;
      nsresult rv = FilterResultSet(nsnull, inputSet, &filteredSet, queries, aOptions);
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
      nsCAutoString asciiRequest;
      if (NS_FAILED(AsciiHostNameFromHostString(query->Domain(), asciiRequest)))
        continue;

      if (query->DomainIsHost()) {
        nsCAutoString host;
        if (NS_FAILED(nodeUri->GetAsciiHost(host)))
          continue;

        if (! asciiRequest.Equals(host))
          continue; // host names don't match
      }
      // check domain names
      nsCAutoString domain;
      DomainNameFromURI(nodeUri, domain);
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
        NS_ENSURE_SUCCESS(rv, PR_FALSE);
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


// nsNavHistory::DomainNameFromURI
//
//    This does the www.mozilla.org -> mozilla.org and
//    foo.theregister.co.uk -> theregister.co.uk conversion

void
nsNavHistory::DomainNameFromURI(nsIURI *aURI,
                                nsACString& aDomainName)
{
  // lazily get the effective tld service
  if (!mTLDService)
    mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);

  if (mTLDService) {
    // get the base domain for a given hostname.
    // e.g. for "images.bbc.co.uk", this would be "bbc.co.uk".
    nsresult rv = mTLDService->GetBaseDomain(aURI, 0, aDomainName);
    if (NS_SUCCEEDED(rv))
      return;
  }

  // just return the original hostname
  // (it's also possible the host is an IP address)
  aURI->GetAsciiHost(aDomainName);
}


// Nav history *****************************************************************


// nsNavHistory::GetHasHistoryEntries

NS_IMETHODIMP
nsNavHistory::GetHasHistoryEntries(PRBool* aHasEntries)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG_POINTER(aHasEntries);

  // Use cached value if it's been set
  if (mHasHistoryEntries != -1) {
    *aHasEntries = (mHasHistoryEntries == 1);
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT 1 "
      "WHERE EXISTS (SELECT id FROM moz_historyvisits_temp LIMIT 1) "
        "OR EXISTS (SELECT id FROM moz_historyvisits LIMIT 1)"),
    getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = dbSelectStatement->ExecuteStep(aHasEntries);
  NS_ENSURE_SUCCESS(rv, rv);

  mHasHistoryEntries = *aHasEntries ? 1 : 0;
  return NS_OK;
}

nsresult
nsNavHistory::FixInvalidFrecenciesForExcludedPlaces()
{
  // for every moz_place that has an invalid frecency (< 0) and
  // is an unvisited child of a livemark feed, or begins with "place:",
  // set frecency to 0 so that it is excluded from url bar autocomplete.
  nsCOMPtr<mozIStorageStatement> dbUpdateStatement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places_view "
      "SET frecency = 0 WHERE id IN ("
        "SELECT h.id FROM moz_places h "
        "WHERE h.url >= 'place:' AND h.url < 'place;' "
        "UNION "
        "SELECT h.id FROM moz_places_temp h "
        "WHERE  h.url >= 'place:' AND h.url < 'place;' "
        "UNION "
        // Unvisited child of a livemark
        "SELECT b.fk FROM moz_bookmarks b "
        "JOIN moz_bookmarks bp ON bp.id = b.parent "
        "JOIN moz_items_annos a ON a.item_id = bp.id "
        "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id "
        "WHERE n.name = :anno_name "
        "AND b.fk IN( "
          "SELECT id FROM moz_places WHERE visit_count = 0 AND frecency < 0 "
          "UNION ALL "
          "SELECT id FROM moz_places_temp WHERE visit_count = 0 AND frecency < 0 "
        ") "
      ")"),
    getter_AddRefs(dbUpdateStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbUpdateStatement->BindUTF8StringByName(
    NS_LITERAL_CSTRING("anno_name"), NS_LITERAL_CSTRING(LMANNO_FEEDURI)
  );
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dbUpdateStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::CalculateFullVisitCount(PRInt64 aPlaceId, PRInt32 *aVisitCount)
{
  mozStorageStatementScoper scope(mDBFullVisitCount);

  nsresult rv = mDBFullVisitCount->BindInt64ByName(
    NS_LITERAL_CSTRING("page_id"), aPlaceId
  );
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasVisits = PR_TRUE;
  rv = mDBFullVisitCount->ExecuteStep(&hasVisits);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasVisits) {
    rv = mDBFullVisitCount->GetInt32(0, aVisitCount);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
    *aVisitCount = 0;
  
  return NS_OK;
}


// Call this method before visiting a URL in order to help determine the
// transition type of the visit.
// Later, in AddVisitChain() the next visit to this page will be associated to
// TRANSITION_BOOKMARK.
//
// @see MarkPageAsTyped

NS_IMETHODIMP
nsNavHistory::MarkPageAsFollowedBookmark(nsIURI* aURI)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // don't add when history is disabled
  if (IsHistoryDisabled())
    return NS_OK;

  nsCAutoString uriString;
  nsresult rv = aURI->GetSpec(uriString);
  NS_ENSURE_SUCCESS(rv, rv);

  // if URL is already in the bookmark queue, then we need to remove the old one
  PRInt64 unusedEventTime;
  if (mRecentBookmark.Get(uriString, &unusedEventTime))
    mRecentBookmark.Remove(uriString);

  if (mRecentBookmark.Count() > RECENT_EVENT_QUEUE_MAX_LENGTH)
    ExpireNonrecentEvents(&mRecentBookmark);

  mRecentBookmark.Put(uriString, GetNow());
  return NS_OK;
}


// nsNavHistory::CanAddURI
//
//    Filter out unwanted URIs such as "chrome:", "mailbox:", etc.
//
//    The model is if we don't know differently then add which basically means
//    we are suppose to try all the things we know not to allow in and then if
//    we don't bail go on and allow it in.

NS_IMETHODIMP
nsNavHistory::CanAddURI(nsIURI* aURI, PRBool* canAdd)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(canAdd);

  // If history is disabled (included privatebrowsing), don't add any entry.
  if (IsHistoryDisabled()) {
    *canAdd = PR_FALSE;
    return NS_OK;
  }

  nsCAutoString scheme;
  nsresult rv = aURI->GetScheme(scheme);
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
      scheme.EqualsLiteral("data") ||
      scheme.EqualsLiteral("wyciwyg")) {
    *canAdd = PR_FALSE;
    return NS_OK;
  }
  *canAdd = PR_TRUE;
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
nsNavHistory::AddVisit(nsIURI* aURI, PRTime aTime, nsIURI* aReferringURI,
                       PRInt32 aTransitionType, PRBool aIsRedirect,
                       PRInt64 aSessionID, PRInt64* aVisitID)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(aVisitID);

  // Filter out unwanted URIs, silently failing
  PRBool canAdd = PR_FALSE;
  nsresult rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    *aVisitID = 0;
    return NS_OK;
  }

  // This will prevent corruption since we have to do a two-phase add.
  // Generally this won't do anything because AddURI has its own transaction.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // see if this is an update (revisit) or a new page
  mozStorageStatementScoper scoper(mDBGetPageVisitStats);
  rv = URIBinder::Bind(mDBGetPageVisitStats, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool alreadyVisited = PR_FALSE;
  rv = mDBGetPageVisitStats->ExecuteStep(&alreadyVisited);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 pageID = 0;
  PRInt32 hidden;
  PRInt32 typed;
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

    PRBool oldHiddenState = 0;
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
    // Rather, we want to hide pages that redirect themselves somewhere
    // else, which is what the redirect flag means.
    //
    // note, we want to unhide any hidden pages that the user explicitly types
    // (aTransitionType == TRANSITION_TYPED) so that they will appear in
    // the history UI (sidebar, history menu, url bar autocomplete, etc)
    hidden = oldHiddenState;
    if (hidden == 1 &&
        (!aIsRedirect || aTransitionType == TRANSITION_TYPED) &&
        aTransitionType != TRANSITION_EMBED &&
        aTransitionType != TRANSITION_FRAMED_LINK)
      hidden = 0; // unhide

    typed = (PRInt32)(oldTypedState == 1 || (aTransitionType == TRANSITION_TYPED));

    // some items may have a visit count of 0 which will not count for link
    // visiting, so be sure to note this transition
    if (oldVisitCount == 0)
      newItem = PR_TRUE;

    // update with new stats
    mozStorageStatementScoper updateScoper(mDBUpdatePageVisitStats);
    rv = mDBUpdatePageVisitStats->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), pageID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBUpdatePageVisitStats->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), hidden);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBUpdatePageVisitStats->BindInt32ByName(NS_LITERAL_CSTRING("typed"), typed);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBUpdatePageVisitStats->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // New page
    newItem = PR_TRUE;

    // free the previous statement before we make a new one
    mDBGetPageVisitStats->Reset();
    scoper.Abandon();

    // Hide only embedded links and redirects
    // See the hidden computation code above for a little more explanation.
    hidden = (PRInt32)(aTransitionType == TRANSITION_EMBED ||
                       aTransitionType == TRANSITION_FRAMED_LINK ||
                       aIsRedirect);

    typed = (PRInt32)(aTransitionType == TRANSITION_TYPED);

    // set as visited once, no title
    nsString voidString;
    voidString.SetIsVoid(PR_TRUE);
    rv = InternalAddNewPage(aURI, voidString, hidden == 1, typed == 1, 1,
                            PR_TRUE, &pageID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Get the visit id for the referrer, if it exists.
  PRInt64 referringVisitID = 0;
  PRInt64 referringSessionID;
  PRTime referringTime;
  PRBool referrerIsSame;
  if (aReferringURI &&
      NS_SUCCEEDED(aReferringURI->Equals(aURI, &referrerIsSame)) &&
      !referrerIsSame &&
      !FindLastVisit(aReferringURI, &referringVisitID, &referringTime, &referringSessionID)) {
    // The referrer is not in the database and is not the same as aURI, so it
    // must be added.
    rv = AddVisit(aReferringURI, aTime - 1, nsnull, TRANSITION_LINK, PR_FALSE,
                  aSessionID, &referringVisitID);
    if (NS_FAILED(rv))
      referringVisitID = 0;
  }

  rv = InternalAddVisit(pageID, referringVisitID, aSessionID, aTime,
                        aTransitionType, aVisitID);
  transaction.Commit();

  // Update frecency (*after* the visit info is in the db)
  // Swallow errors here, since if we've gotten this far, it's more
  // important to notify the observers below.
  nsNavBookmarks *bs = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bs, NS_ERROR_OUT_OF_MEMORY);
  (void)UpdateFrecency(pageID, bs->IsRealBookmark(pageID));

  // Notify observers: The hidden detection code must match that in
  // GetQueryResults to maintain consistency.
  // FIXME bug 325241: make a way to observe hidden URLs
  if (!hidden) {
    NotifyOnVisit(aURI, *aVisitID, aTime, aSessionID, referringVisitID,
                  aTransitionType);
  }

  // Normally docshell sends the link visited observer notification for us (this
  // will tell all the documents to update their visited link coloring).
  // However, for redirects (since we implement nsIGlobalHistory3) and downloads
  // (since we implement nsIDownloadHistory) this will not happen and we need to
  // send it ourselves.
  if (newItem && (aIsRedirect || aTransitionType == TRANSITION_DOWNLOAD)) {
    nsCOMPtr<nsIObserverService> obsService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    if (obsService)
      obsService->NotifyObservers(aURI, NS_LINK_VISITED_EVENT_TOPIC, nsnull);
  }

  // Because we implement IHistory, we always have to notify about the visit.
  History::GetService()->NotifyVisited(aURI);

  return NS_OK;
}


// nsNavHistory::GetNewQuery

NS_IMETHODIMP
nsNavHistory::GetNewQuery(nsINavHistoryQuery **_retval)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = new nsNavHistoryQuery();
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval);
  return NS_OK;
}

// nsNavHistory::GetNewQueryOptions

NS_IMETHODIMP
nsNavHistory::GetNewQueryOptions(nsINavHistoryQueryOptions **_retval)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG_POINTER(_retval);

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
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aQuery);
  NS_ENSURE_ARG(aOptions);
  NS_ENSURE_ARG_POINTER(_retval);

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
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aQueries);
  NS_ENSURE_ARG(aOptions);
  NS_ENSURE_ARG(aQueryCount);
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv;
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
  PRInt64 folderId = GetSimpleBookmarksQueryFolder(queries, options);
  if (folderId) {
    // In the simple case where we're just querying children of a single bookmark
    // folder, we can more efficiently generate results.
    nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
    nsRefPtr<nsNavHistoryResultNode> tempRootNode;
    rv = bookmarks->ResultNodeForContainer(folderId, options,
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

// determine from our nsNavHistoryQuery array and nsNavHistoryQueryOptions
// if this is the place query from the history menu.
// from browser-menubar.inc, our history menu query is:
// place:redirectsMode=2&sort=4&maxResults=10
// note, any maxResult > 0 will still be considered a history menu query
// or if this is the place query from the "Most Visited" item in the "Smart Bookmarks" folder:
// place:redirectsMode=2&sort=8&maxResults=10
// note, any maxResult > 0 will still be considered a Most Visited menu query
static
PRBool IsOptimizableHistoryQuery(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                                 nsNavHistoryQueryOptions *aOptions,
                                 PRUint16 aSortMode)
{
  if (aQueries.Count() != 1)
    return PR_FALSE;

  nsNavHistoryQuery *aQuery = aQueries[0];
 
  if (aOptions->QueryType() != nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY)
    return PR_FALSE;

  if (aOptions->ResultType() != nsINavHistoryQueryOptions::RESULTS_AS_URI)
    return PR_FALSE;

  if (aOptions->SortingMode() != aSortMode)
    return PR_FALSE;

  if (aOptions->MaxResults() <= 0)
    return PR_FALSE;

  if (aOptions->ExcludeItems())
    return PR_FALSE;

  if (aOptions->IncludeHidden())
    return PR_FALSE;

  if (aQuery->MinVisits() != -1 || aQuery->MaxVisits() != -1)
    return PR_FALSE;

  if (aQuery->BeginTime() || aQuery->BeginTimeReference()) 
    return PR_FALSE;

  if (aQuery->EndTime() || aQuery->EndTimeReference()) 
    return PR_FALSE;

  if (!aQuery->SearchTerms().IsEmpty()) 
    return PR_FALSE;

  if (aQuery->OnlyBookmarked()) 
    return PR_FALSE;

  if (aQuery->DomainIsHost() || !aQuery->Domain().IsEmpty())
    return PR_FALSE;

  if (aQuery->AnnotationIsNot() || !aQuery->Annotation().IsEmpty()) 
    return PR_FALSE;

  if (aQuery->UriIsPrefix() || aQuery->Uri()) 
    return PR_FALSE;

  if (aQuery->Folders().Length() > 0)
    return PR_FALSE;

  if (aQuery->Tags().Length() > 0)
    return PR_FALSE;

  if (aQuery->Transitions().Length() > 0)
    return PR_FALSE;

  return PR_TRUE;
}

static
PRBool NeedToFilterResultSet(const nsCOMArray<nsNavHistoryQuery>& aQueries, 
                             nsNavHistoryQueryOptions *aOptions)
{
  // Never filter queries returning queries
  PRUint16 resultType = aOptions->ResultType();
  if (resultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY ||
      resultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_SITE_QUERY ||
      resultType == nsINavHistoryQueryOptions::RESULTS_AS_TAG_QUERY ||
      resultType == nsINavHistoryQueryOptions::RESULTS_AS_SITE_QUERY)
    return PR_FALSE;

  // Always filter bookmarks queries to avoid the inclusion of query nodes,
  // but RESULTS AS TAG QUERY never needs to be filtered.
  if (aOptions->QueryType() == nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS)
    return PR_TRUE;

  nsCString parentAnnotationToExclude;
  nsresult rv = aOptions->GetExcludeItemIfParentHasAnnotation(parentAnnotationToExclude);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);
  if (!parentAnnotationToExclude.IsEmpty())
    return PR_TRUE;

  PRInt32 i;
  for (i = 0; i < aQueries.Count(); i ++) {
    if (aQueries[i]->Folders().Length() != 0) {
      return PR_TRUE;
    } else {
      PRBool hasSearchTerms;
      nsresult rv = aQueries[i]->GetHasSearchTerms(&hasSearchTerms);
      if (NS_FAILED(rv) || hasSearchTerms)
        return PR_TRUE;
    }
  }
  return PR_FALSE;
}

// ** Helper class for ConstructQueryString **/

class PlacesSQLQueryBuilder
{
public:
  PlacesSQLQueryBuilder(const nsCString& aConditions,
                        nsNavHistoryQueryOptions* aOptions,
                        PRBool aUseLimit,
                        nsNavHistory::StringHash& aAddParams,
                        PRBool aHasSearchTerms);

  nsresult GetQueryString(nsCString& aQueryString);

private:
  nsresult Select();

  nsresult SelectAsURI();
  nsresult SelectAsVisit();
  nsresult SelectAsDay();
  nsresult SelectAsSite();
  nsresult SelectAsTag();

  nsresult Where();
  nsresult GroupBy();
  nsresult OrderBy();
  nsresult Limit();

  void OrderByColumnIndexAsc(PRInt32 aIndex);
  void OrderByColumnIndexDesc(PRInt32 aIndex);
  // Use these if you want a case insensitive sorting.
  void OrderByTextColumnIndexAsc(PRInt32 aIndex);
  void OrderByTextColumnIndexDesc(PRInt32 aIndex);

  const nsCString& mConditions;
  PRBool mUseLimit;
  PRBool mHasSearchTerms;

  PRUint16 mResultType;
  PRUint16 mQueryType;
  PRBool mIncludeHidden;
  PRUint16 mRedirectsMode;
  PRUint16 mSortingMode;
  PRUint32 mMaxResults;

  nsCString mQueryString;
  nsCString mGroupBy;
  PRBool mHasDateColumns;
  PRBool mSkipOrderBy;
  nsNavHistory::StringHash& mAddParams;
};

PlacesSQLQueryBuilder::PlacesSQLQueryBuilder(
    const nsCString& aConditions, 
    nsNavHistoryQueryOptions* aOptions, 
    PRBool aUseLimit,
    nsNavHistory::StringHash& aAddParams,
    PRBool aHasSearchTerms)
: mConditions(aConditions)
, mUseLimit(aUseLimit)
, mHasSearchTerms(aHasSearchTerms)
, mResultType(aOptions->ResultType())
, mQueryType(aOptions->QueryType())
, mIncludeHidden(aOptions->IncludeHidden())
, mRedirectsMode(aOptions->RedirectsMode())
, mSortingMode(aOptions->SortingMode())
, mMaxResults(aOptions->MaxResults())
, mSkipOrderBy(PR_FALSE)
, mAddParams(aAddParams)
{
  mHasDateColumns = (mQueryType == nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS);
}

nsresult
PlacesSQLQueryBuilder::GetQueryString(nsCString& aQueryString)
{
  nsresult rv = Select();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = Where();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = GroupBy();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = OrderBy();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = Limit();
  NS_ENSURE_SUCCESS(rv, rv);

  aQueryString = mQueryString;
  return NS_OK;
}

nsresult
PlacesSQLQueryBuilder::Select()
{
  nsresult rv;

  switch (mResultType)
  {
    case nsINavHistoryQueryOptions::RESULTS_AS_URI:
    case nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS:
      rv = SelectAsURI();
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    case nsINavHistoryQueryOptions::RESULTS_AS_VISIT:
    case nsINavHistoryQueryOptions::RESULTS_AS_FULL_VISIT:
      rv = SelectAsVisit();
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    case nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY:
    case nsINavHistoryQueryOptions::RESULTS_AS_DATE_SITE_QUERY:
      rv = SelectAsDay();
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    case nsINavHistoryQueryOptions::RESULTS_AS_SITE_QUERY:
      rv = SelectAsSite();
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    case nsINavHistoryQueryOptions::RESULTS_AS_TAG_QUERY:
      rv = SelectAsTag();
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    default:
      NS_NOTREACHED("Invalid result type");
  }
  return NS_OK;
}

nsresult
PlacesSQLQueryBuilder::SelectAsURI()
{
  nsNavHistory *history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  nsCAutoString tagsSqlFragment;

  switch (mQueryType) {
    case nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY:
      GetTagsSqlFragment(history->GetTagsFolder(),
                         NS_LITERAL_CSTRING("h.id"),
                         mHasSearchTerms,
                         tagsSqlFragment);

      mQueryString = NS_LITERAL_CSTRING(
        "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
        "h.last_visit_date, f.url, v.session, null, null, null, null, ") +
        tagsSqlFragment + NS_LITERAL_CSTRING(
        "FROM moz_places_temp h "
        "JOIN moz_historyvisits_temp v ON h.id = v.place_id "
        "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
        // WHERE 1 is a no-op since additonal conditions will start with AND.
        "WHERE 1 "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "
        "GROUP BY h.id "
        "UNION ALL "
        "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
        "h.last_visit_date, f.url, v.session, null, null, null, null, ") +
        tagsSqlFragment + NS_LITERAL_CSTRING(
        "FROM moz_places_temp h "
        "JOIN moz_historyvisits v ON h.id = v.place_id "
        "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
        "WHERE h.id NOT IN (SELECT place_id FROM moz_historyvisits_temp) "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "
        "GROUP BY h.id "
        "UNION ALL "
        "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
        "h.last_visit_date, f.url, v.session, null, null, null, null, ") +
        tagsSqlFragment + NS_LITERAL_CSTRING(
        "FROM moz_places h "
        "JOIN moz_historyvisits_temp v ON h.id = v.place_id "
        "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
        "WHERE h.id NOT IN (SELECT id FROM moz_places_temp) "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "
        "GROUP BY h.id "
        "UNION ALL "
        "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
        "h.last_visit_date, f.url, v.session, null, null, null, null, ") +
        tagsSqlFragment + NS_LITERAL_CSTRING(
        "FROM moz_places h "
        "JOIN moz_historyvisits v ON h.id = v.place_id "
        "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
        "WHERE h.id NOT IN (SELECT id FROM moz_places_temp) "
          "AND h.id NOT IN (SELECT place_id FROM moz_historyvisits_temp) "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "
        "GROUP BY h.id ");
      break;

    case nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS:
      if (mResultType == nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS) {
        // Order-by clause is hardcoded because we need to discard duplicates
        // in FilterResultSet. We will retain only the last modified item,
        // so we are ordering by place id and last modified to do a faster
        // filtering.
        mSkipOrderBy = PR_TRUE;

        GetTagsSqlFragment(history->GetTagsFolder(),
                           NS_LITERAL_CSTRING("b2.fk"),
                           mHasSearchTerms,
                           tagsSqlFragment);

        mQueryString = NS_LITERAL_CSTRING(
          "SELECT b2.fk, h.url, COALESCE(b2.title, h.title), h.rev_host, "
            "h.visit_count, h.last_visit_date, f.url, null, b2.id, "
            "b2.dateAdded, b2.lastModified, b2.parent, ") +
            tagsSqlFragment + NS_LITERAL_CSTRING(
          "FROM moz_bookmarks b2 "
          "JOIN (SELECT b.fk "
                "FROM moz_bookmarks b "
                // ADDITIONAL_CONDITIONS will filter on parent.
                "WHERE b.type = 1 {ADDITIONAL_CONDITIONS} "
                ") AS seed ON b2.fk = seed.fk "
          "JOIN moz_places_temp h ON h.id = b2.fk "
          "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
          "WHERE NOT EXISTS ( "
            "SELECT id FROM moz_bookmarks WHERE id = b2.parent AND parent = ") +
                nsPrintfCString("%lld", history->GetTagsFolder()) +
          NS_LITERAL_CSTRING(") "
          "UNION ALL "
          "SELECT b2.fk, h.url, COALESCE(b2.title, h.title), h.rev_host, "
            "h.visit_count, h.last_visit_date, f.url, null, b2.id, "
            "b2.dateAdded, b2.lastModified, b2.parent, ") +
            tagsSqlFragment + NS_LITERAL_CSTRING(
          "FROM moz_bookmarks b2 "
          "JOIN (SELECT b.fk "
                "FROM moz_bookmarks b "
                // ADDITIONAL_CONDITIONS will filter on parent.
                "WHERE b.type = 1 {ADDITIONAL_CONDITIONS} "
                ") AS seed ON b2.fk = seed.fk "
          "JOIN moz_places h ON h.id = b2.fk "
          "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
          "WHERE NOT EXISTS ( "
            "SELECT id FROM moz_bookmarks WHERE id = b2.parent AND parent = ") +
                nsPrintfCString("%lld", history->GetTagsFolder()) +
          NS_LITERAL_CSTRING(") "
            "AND h.id NOT IN (SELECT id FROM moz_places_temp) "
          "ORDER BY b2.fk DESC, b2.lastModified DESC");
      }
      else {
        GetTagsSqlFragment(history->GetTagsFolder(),
                           NS_LITERAL_CSTRING("b.fk"),
                           mHasSearchTerms,
                           tagsSqlFragment);
        mQueryString = NS_LITERAL_CSTRING(
          "SELECT b.fk, h.url, COALESCE(b.title, h.title), h.rev_host, "
            "h.visit_count, h.last_visit_date, f.url, null, b.id, "
            "b.dateAdded, b.lastModified, b.parent, ") +
            tagsSqlFragment + NS_LITERAL_CSTRING(
          "FROM moz_bookmarks b "
          "JOIN moz_places_temp h ON b.fk = h.id AND b.type = 1 "
          "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
          "WHERE NOT EXISTS "
            "(SELECT id FROM moz_bookmarks "
              "WHERE id = b.parent AND parent = ") +
                nsPrintfCString("%lld", history->GetTagsFolder()) +
            NS_LITERAL_CSTRING(") "
            "{ADDITIONAL_CONDITIONS}"
          "UNION ALL "
          "SELECT b.fk, h.url, COALESCE(b.title, h.title), h.rev_host, "
            "h.visit_count, h.last_visit_date, f.url, null, b.id, "
            "b.dateAdded, b.lastModified, b.parent, ") +
            tagsSqlFragment + NS_LITERAL_CSTRING(
          "FROM moz_bookmarks b "
          "JOIN moz_places h ON b.fk = h.id AND b.type = 1 "
          "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
          "WHERE h.id NOT IN (SELECT id FROM moz_places_temp) "
            "AND NOT EXISTS "
              "(SELECT id FROM moz_bookmarks "
                "WHERE id = b.parent AND parent = ") +
                  nsPrintfCString("%lld", history->GetTagsFolder()) +
              NS_LITERAL_CSTRING(") "
            "{ADDITIONAL_CONDITIONS}");
      }
      break;

    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
  return NS_OK;
}

nsresult
PlacesSQLQueryBuilder::SelectAsVisit()
{
  nsNavHistory *history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  nsCAutoString tagsSqlFragment;
  GetTagsSqlFragment(history->GetTagsFolder(),
                     NS_LITERAL_CSTRING("h.id"),
                     mHasSearchTerms,
                     tagsSqlFragment);
  mQueryString = NS_LITERAL_CSTRING(
    "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
      "v.visit_date, f.url, v.session, null, null, null, null, ") +
      tagsSqlFragment + NS_LITERAL_CSTRING(
    "FROM moz_places_temp h "
    "JOIN moz_historyvisits_temp v ON h.id = v.place_id "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    // WHERE 1 is a no-op since additonal conditions will start with AND.
    "WHERE 1 "
      "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
      "{ADDITIONAL_CONDITIONS} "
    "UNION ALL "
    "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
      "v.visit_date, f.url, v.session, null, null, null, null, ") +
      tagsSqlFragment + NS_LITERAL_CSTRING(
    "FROM moz_places_temp h "
    "JOIN moz_historyvisits v ON h.id = v.place_id "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    // WHERE 1 is a no-op since additonal conditions will start with AND.
    "WHERE 1 "
      "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
      "{ADDITIONAL_CONDITIONS} "
    "UNION ALL "
    "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
      "v.visit_date, f.url, v.session, null, null, null, null, ") +
      tagsSqlFragment + NS_LITERAL_CSTRING(
    "FROM moz_places h "
    "JOIN moz_historyvisits_temp v ON h.id = v.place_id "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.id NOT IN (SELECT id FROM moz_places_temp) "
      "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
      "{ADDITIONAL_CONDITIONS} "
    "UNION ALL "
    "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
      "v.visit_date, f.url, v.session, null, null, null, null, ") +
      tagsSqlFragment + NS_LITERAL_CSTRING(
    "FROM moz_places h "
    "JOIN moz_historyvisits v ON h.id = v.place_id "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.id NOT IN (SELECT id FROM moz_places_temp) "
      "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
      "{ADDITIONAL_CONDITIONS} ");

  return NS_OK;
}

nsresult
PlacesSQLQueryBuilder::SelectAsDay()
{
  mSkipOrderBy = PR_TRUE;

  // Sort child queries based on sorting mode if it's provided, otherwise
  // fallback to default sort by title ascending.
  PRUint16 sortingMode = nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING;
  if (mSortingMode != nsINavHistoryQueryOptions::SORT_BY_NONE &&
      mResultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY)
    sortingMode = mSortingMode;

  PRUint16 resultType =
    mResultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY ?
      (PRUint16)nsINavHistoryQueryOptions::RESULTS_AS_URI :
      (PRUint16)nsINavHistoryQueryOptions::RESULTS_AS_SITE_QUERY;

  // beginTime will become the node's time property, we don't use endTime
  // because it could overlap, and we use time to sort containers and find
  // insert position in a result.
  mQueryString = nsPrintfCString(1024,
     "SELECT null, "
       "'place:type=%ld&sort=%ld&beginTime='||beginTime||'&endTime='||endTime, "
      "dayTitle, null, null, beginTime, null, null, null, null, null, null "
     "FROM (", // TOUTER BEGIN
     resultType,
     sortingMode);
 
   nsNavHistory *history = nsNavHistory::GetHistoryService();
   NS_ENSURE_STATE(history);

  PRInt32 daysOfHistory = history->GetDaysOfHistory();
  for (PRInt32 i = 0; i <= HISTORY_DATE_CONT_NUM(daysOfHistory); i++) {
    nsCAutoString dateName;
    // Timeframes are calculated as BeginTime <= container < EndTime.
    // Notice times can't be relative to now, since to recognize a query we
    // must ensure it won't change based on the time it is built.
    // So, to select till now, we really select till start of tomorrow, that is
    // a fixed timestamp.
    // These are used as limits for the inside containers.
    nsCAutoString sqlFragmentContainerBeginTime, sqlFragmentContainerEndTime;
    // These are used to query if the container should be visible.
    nsCAutoString sqlFragmentSearchBeginTime, sqlFragmentSearchEndTime;
    switch(i) {
       case 0:
        // Today
         history->GetStringFromName(
          NS_LITERAL_STRING("finduri-AgeInDays-is-0").get(), dateName);
        // From start of today
        sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of day','utc')*1000000)");
        // To now (tomorrow)
        sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of day','+1 day','utc')*1000000)");
        // Search for the same timeframe.
        sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
        sqlFragmentSearchEndTime = sqlFragmentContainerEndTime;
         break;
       case 1:
        // Yesterday
         history->GetStringFromName(
          NS_LITERAL_STRING("finduri-AgeInDays-is-1").get(), dateName);
        // From start of yesterday
        sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of day','-1 day','utc')*1000000)");
        // To start of today
        sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of day','utc')*1000000)");
        // Search for the same timeframe.
        sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
        sqlFragmentSearchEndTime = sqlFragmentContainerEndTime;
        break;
      case 2:
        // Last 7 days
        history->GetAgeInDaysString(7,
          NS_LITERAL_STRING("finduri-AgeInDays-last-is").get(), dateName);
        // From start of 7 days ago
        sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of day','-7 days','utc')*1000000)");
        // To now (tomorrow)
        sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of day','+1 day','utc')*1000000)");
        // This is an overlapped container, but we show it only if there are
        // visits older than yesterday.
        sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
        sqlFragmentSearchEndTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of day','-2 days','utc')*1000000)");
        break;
      case 3:
        // This month
        history->GetStringFromName(
          NS_LITERAL_STRING("finduri-AgeInMonths-is-0").get(), dateName);
        // From start of this month
        sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of month','utc')*1000000)");
        // To now (tomorrow)
        sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of day','+1 day','utc')*1000000)");
        // This is an overlapped container, but we show it only if there are
        // visits older than 7 days ago.
        sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
        sqlFragmentSearchEndTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of day','-7 days','utc')*1000000)");
         break;
       default:
        if (i == HISTORY_ADDITIONAL_DATE_CONT_NUM + 6) {
          // Older than 6 months
          history->GetAgeInDaysString(6,
            NS_LITERAL_STRING("finduri-AgeInMonths-isgreater").get(), dateName);
          // From start of epoch
          sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
            "(datetime(0, 'unixepoch')*1000000)");
          // To start of 6 months ago ( 5 months + this month).
          sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of month','-5 months','utc')*1000000)");
          // Search for the same timeframe.
          sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
          sqlFragmentSearchEndTime = sqlFragmentContainerEndTime;
          break;
        }
        PRInt32 MonthIndex = i - HISTORY_ADDITIONAL_DATE_CONT_NUM;
        // Previous months' titles are month's name if inside this year,
        // month's name and year for previous years.
        PRExplodedTime tm;
        PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &tm);
        PRUint16 currentYear = tm.tm_year;
        // Set day before month, setting month without day could cause issues.
        // For example setting month to February when today is 30, since
        // February has not 30 days, will return March instead.
        tm.tm_mday = 1;
        tm.tm_month -= MonthIndex;
        // Notice we use GMTParameters because we just want to get the first
        // day of each month.  Using LocalTimeParameters would instead force us
        // to apply a DST correction that we don't really need here.
        PR_NormalizeTime(&tm, PR_GMTParameters);
        // tm_month starts from 0 while GetMonthName expects a 1-based index.
        history->GetMonthName(tm.tm_month+1, dateName);

        // If the container is for a past year, add the year as suffix.
        if (tm.tm_year < currentYear)
          dateName.Append(nsPrintfCString(" %d", tm.tm_year));

        // From start of MonthIndex + 1 months ago
        sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of month','-");
        sqlFragmentContainerBeginTime.AppendInt(MonthIndex);
        sqlFragmentContainerBeginTime.Append(NS_LITERAL_CSTRING(
            " months','utc')*1000000)"));
        // To start of MonthIndex months ago
        sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
          "(strftime('%s','now','localtime','start of month','-");
        sqlFragmentContainerEndTime.AppendInt(MonthIndex - 1);
        sqlFragmentContainerEndTime.Append(NS_LITERAL_CSTRING(
            " months','utc')*1000000)"));
        // Search for the same timeframe.
        sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
        sqlFragmentSearchEndTime = sqlFragmentContainerEndTime;
        break;
    }

    nsPrintfCString dateParam("dayTitle%d", i);
    mAddParams.Put(dateParam, dateName);

     nsPrintfCString dayRange(1024,
        "SELECT :%s AS dayTitle, "
               "%s AS beginTime, "
               "%s AS endTime "
         "WHERE EXISTS ( "
           "SELECT id FROM moz_historyvisits_temp "
          "WHERE visit_date >= %s "
            "AND visit_date < %s "
            "AND visit_type NOT IN (0,%d,%d) "
            "{QUERY_OPTIONS_VISITS} "
          "UNION ALL "
          "SELECT id FROM moz_historyvisits "
          "WHERE visit_date >= %s "
            "AND visit_date < %s "
             "AND visit_type NOT IN (0,%d,%d) "
             "{QUERY_OPTIONS_VISITS} "
           "LIMIT 1 "
        ") ",
      dateParam.get(),
      sqlFragmentContainerBeginTime.get(),
      sqlFragmentContainerEndTime.get(),
      sqlFragmentSearchBeginTime.get(),
      sqlFragmentSearchEndTime.get(),
       nsINavHistoryService::TRANSITION_EMBED,
       nsINavHistoryService::TRANSITION_FRAMED_LINK,
      sqlFragmentSearchBeginTime.get(),
      sqlFragmentSearchEndTime.get(),
      nsINavHistoryService::TRANSITION_EMBED,
      nsINavHistoryService::TRANSITION_FRAMED_LINK);

    mQueryString.Append(dayRange);

    if (i < HISTORY_DATE_CONT_NUM(daysOfHistory))
      mQueryString.Append(NS_LITERAL_CSTRING(" UNION ALL "));
  }

  mQueryString.Append(NS_LITERAL_CSTRING(") ")); // TOUTER END

  return NS_OK;
}

nsresult
PlacesSQLQueryBuilder::SelectAsSite()
{
  nsCAutoString localFiles;

  nsNavHistory *history = nsNavHistory::GetHistoryService();
  NS_ENSURE_STATE(history);

  history->GetStringFromName(NS_LITERAL_STRING("localhost").get(), localFiles);
  mAddParams.Put(NS_LITERAL_CSTRING("localhost"), localFiles);

  // We want just sites, but from whole database.
  if (mConditions.IsEmpty()) {
    mQueryString = nsPrintfCString(2048,
      "SELECT DISTINCT null, "
             "'place:type=%ld&sort=%ld&domain=&domainIsHost=true', "
             ":localhost, :localhost, null, null, null, null, null, null, null "
      "WHERE EXISTS ( "
        "SELECT id FROM moz_places_temp "
        "WHERE hidden <> 1 "
          "AND rev_host = '.' "
          "AND visit_count > 0 "
          "AND url BETWEEN 'file://' AND 'file:/~' "
        "UNION ALL "
        "SELECT id FROM moz_places "
        "WHERE id NOT IN (SELECT id FROM moz_places_temp) "
          "AND hidden <> 1 "
          "AND rev_host = '.' "
          "AND visit_count > 0 "
          "AND url BETWEEN 'file://' AND 'file:/~' "
      ") "
      "UNION ALL "
      "SELECT DISTINCT null, "
             "'place:type=%ld&sort=%ld&domain='||host||'&domainIsHost=true', "
             "host, host, null, null, null, null, null, null, null "
      "FROM ( "
        "SELECT get_unreversed_host(rev_host) host "
        "FROM ( "
          "SELECT DISTINCT rev_host FROM moz_places_temp "
          "WHERE hidden <> 1 "
            "AND rev_host <> '.' "
            "AND visit_count > 0 "
          "UNION ALL "
          "SELECT DISTINCT rev_host FROM moz_places "
          "WHERE id NOT IN (SELECT id FROM moz_places_temp) "
            "AND hidden <> 1 "
            "AND rev_host <> '.' "
            "AND visit_count > 0 "
        ") "
      "ORDER BY 1 ASC) ",
      nsINavHistoryQueryOptions::RESULTS_AS_URI,
      mSortingMode,
      nsINavHistoryQueryOptions::RESULTS_AS_URI,
      mSortingMode);
  // Now we need to use the filters - we need them all
  } else {

    mQueryString = nsPrintfCString(4096,
      "SELECT DISTINCT null, "
             "'place:type=%ld&sort=%ld&domain=&domainIsHost=true"
               "&beginTime='||:begin_time||'&endTime='||:end_time, "
             ":localhost, :localhost, null, null, null, null, null, null, null "
      "WHERE EXISTS( "
        "SELECT h.id "
        "FROM moz_places h "
        "JOIN moz_historyvisits v ON v.place_id = h.id "
        "WHERE h.hidden <> 1 AND h.rev_host = '.' "
          "AND h.visit_count > 0 "
          "AND h.url BETWEEN 'file://' AND 'file:/~' "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "
        "UNION "
        "SELECT h.id "
        "FROM moz_places_temp h "
        "JOIN moz_historyvisits v ON v.place_id = h.id "
        "WHERE h.hidden <> 1 AND h.rev_host = '.' "
          "AND h.visit_count > 0 "
          "AND h.url BETWEEN 'file://' AND 'file:/~' "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "
        "UNION "
        "SELECT h.id "
        "FROM moz_places h "
        "JOIN moz_historyvisits_temp v ON v.place_id = h.id "
        "WHERE h.hidden <> 1 AND h.rev_host = '.' "
          "AND h.visit_count > 0 "
          "AND h.url BETWEEN 'file://' AND 'file:/~' "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "
        "UNION "
        "SELECT h.id "
        "FROM moz_places_temp h "
        "JOIN moz_historyvisits_temp v ON v.place_id = h.id "
        "WHERE h.hidden <> 1 AND h.rev_host = '.' "
          "AND h.visit_count > 0 "
          "AND h.url BETWEEN 'file://' AND 'file:/~' "
          "{QUERY_OPTIONS_VISITS}  {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "        
      ") "
      "UNION ALL "
      "SELECT DISTINCT null, "
             "'place:type=%ld&sort=%ld&domain='||host||'&domainIsHost=true"
               "&beginTime='||:begin_time||'&endTime='||:end_time, "
             "host, host, null, null, null, null, null, null, null "
      "FROM ( "
        "SELECT DISTINCT get_unreversed_host(rev_host) AS host "
        "FROM moz_places h "
        "JOIN moz_historyvisits v ON v.place_id = h.id "
        "WHERE h.rev_host <> '.' "
          "AND h.visit_count > 0 "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "
        "UNION "
        "SELECT DISTINCT get_unreversed_host(rev_host) AS host "
        "FROM moz_places_temp h "
        "JOIN moz_historyvisits v ON v.place_id = h.id "
        "WHERE h.rev_host <> '.' "
          "AND h.visit_count > 0 "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "
        "UNION "
        "SELECT DISTINCT get_unreversed_host(rev_host) AS host "
        "FROM moz_places h "
        "JOIN moz_historyvisits_temp v ON v.place_id = h.id "
        "WHERE h.rev_host <> '.' "
          "AND h.visit_count > 0 "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "
        "UNION "
        "SELECT DISTINCT get_unreversed_host(rev_host) AS host "
        "FROM moz_places_temp h "
        "JOIN moz_historyvisits_temp v ON v.place_id = h.id "        
        "WHERE h.rev_host <> '.' "
          "AND h.visit_count > 0 "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} "        
        "ORDER BY 1 ASC "
      ") ",
      nsINavHistoryQueryOptions::RESULTS_AS_URI,
      mSortingMode,
      nsINavHistoryQueryOptions::RESULTS_AS_URI,
      mSortingMode);
  }

  return NS_OK;
}

nsresult
PlacesSQLQueryBuilder::SelectAsTag()
{
  nsNavHistory *history = nsNavHistory::GetHistoryService();
  NS_ENSURE_STATE(history);

  // This allows sorting by date fields what is not possible with
  // other history queries.
  mHasDateColumns = PR_TRUE; 

  mQueryString = nsPrintfCString(2048,
    "SELECT null, 'place:folder=' || id || '&queryType=%d&type=%ld', "
      "title, null, null, null, null, null, null, dateAdded, lastModified, "
      "null, null "
    "FROM   moz_bookmarks "
    "WHERE  parent = %lld",
    nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS,
    nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS,
    history->GetTagsFolder());

  return NS_OK;
}

nsresult
PlacesSQLQueryBuilder::Where()
{

  // Set query options
  nsCAutoString additionalVisitsConditions;
  nsCAutoString additionalPlacesConditions;

  if (mRedirectsMode == nsINavHistoryQueryOptions::REDIRECTS_MODE_SOURCE) {
    additionalVisitsConditions += NS_LITERAL_CSTRING(
      "AND visit_type NOT IN ") +
      nsPrintfCString("(%d,%d) ", nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
                                  nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY);
  }
  else if (mRedirectsMode == nsINavHistoryQueryOptions::REDIRECTS_MODE_TARGET) {
    additionalVisitsConditions += NS_LITERAL_CSTRING(
      "AND NOT EXISTS ( "
        "SELECT id FROM moz_historyvisits_temp WHERE from_visit = v.id "
        "AND visit_type IN ") +
        nsPrintfCString("(%d,%d) ", nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
                                    nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY) +
      NS_LITERAL_CSTRING(") AND NOT EXISTS ( "
        "SELECT id FROM moz_historyvisits WHERE from_visit = v.id "
        "AND visit_type IN ") +
        nsPrintfCString("(%d,%d) ", nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
                                    nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY) +
      NS_LITERAL_CSTRING(") ");
  }

  if (!mIncludeHidden) {
    additionalVisitsConditions += NS_LITERAL_CSTRING(
      "AND visit_type NOT IN ") +
      nsPrintfCString("(0,%d,%d) ",
                      nsINavHistoryService::TRANSITION_EMBED,
                      nsINavHistoryService::TRANSITION_FRAMED_LINK);
    additionalPlacesConditions += NS_LITERAL_CSTRING(
      "AND hidden <> 1 ");
  }

  mQueryString.ReplaceSubstring("{QUERY_OPTIONS_VISITS}",
                                additionalVisitsConditions.get());
  mQueryString.ReplaceSubstring("{QUERY_OPTIONS_PLACES}",
                                additionalPlacesConditions.get());

  // If we used WHERE already, we inject the conditions 
  // in place of {ADDITIONAL_CONDITIONS}
  PRInt32 useInnerCondition;
  useInnerCondition = mQueryString.Find("{ADDITIONAL_CONDITIONS}", 0);
  if (useInnerCondition != kNotFound) {

    nsCAutoString innerCondition;
    // If we have condition AND it
    if (!mConditions.IsEmpty()) {
      innerCondition = " AND (";
      innerCondition += mConditions;
      innerCondition += ")";
    }
    mQueryString.ReplaceSubstring("{ADDITIONAL_CONDITIONS}",
                                  innerCondition.get());

  } else if (!mConditions.IsEmpty()) {

    mQueryString += "WHERE ";
    mQueryString += mConditions;

  }
  return NS_OK;
}

nsresult
PlacesSQLQueryBuilder::GroupBy()
{
  mQueryString += mGroupBy;
  return NS_OK;
}

nsresult
PlacesSQLQueryBuilder::OrderBy()
{
  if (mSkipOrderBy)
    return NS_OK;

  // Sort clause: we will sort later, but if it comes out of the DB sorted,
  // our later sort will be basically free. The DB can sort these for free
  // most of the time anyway, because it has indices over these items.
  switch(mSortingMode)
  {
    case nsINavHistoryQueryOptions::SORT_BY_NONE:
      // If this is a URI query the sorting could change based on the
      // sync status of disk and temp tables, we must ensure sorting does not
      // change between queries.
      if (mResultType == nsINavHistoryQueryOptions::RESULTS_AS_URI) {
        if (mQueryType == nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS)
          mQueryString += NS_LITERAL_CSTRING(" ORDER BY b.id ASC ");
        else if (mQueryType == nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY)
          mQueryString += NS_LITERAL_CSTRING(" ORDER BY h.id ASC ");
      }
      break;
    case nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING:
    case nsINavHistoryQueryOptions::SORT_BY_TITLE_DESCENDING:
      // If the user wants few results, we limit them by date, necessitating
      // a sort by date here (see the IDL definition for maxResults).
      // Otherwise we will do actual sorting by title, but since we could need
      // to special sort for some locale we will repeat a second sorting at the
      // end in nsNavHistoryResult, that should be faster since the list will be
      // almost ordered.
      if (mMaxResults > 0)
        OrderByColumnIndexDesc(nsNavHistory::kGetInfoIndex_VisitDate);
      else if (mSortingMode == nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING)
        OrderByTextColumnIndexAsc(nsNavHistory::kGetInfoIndex_Title);
      else
        OrderByTextColumnIndexDesc(nsNavHistory::kGetInfoIndex_Title);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING:
      OrderByColumnIndexAsc(nsNavHistory::kGetInfoIndex_VisitDate);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING:
      OrderByColumnIndexDesc(nsNavHistory::kGetInfoIndex_VisitDate);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_URI_ASCENDING:
      OrderByColumnIndexAsc(nsNavHistory::kGetInfoIndex_URL);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_URI_DESCENDING:
      OrderByColumnIndexDesc(nsNavHistory::kGetInfoIndex_URL);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING:
      OrderByColumnIndexAsc(nsNavHistory::kGetInfoIndex_VisitCount);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING:
      OrderByColumnIndexDesc(nsNavHistory::kGetInfoIndex_VisitCount);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_DATEADDED_ASCENDING:
      if (mHasDateColumns)
        OrderByColumnIndexAsc(nsNavHistory::kGetInfoIndex_ItemDateAdded);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_DATEADDED_DESCENDING:
      if (mHasDateColumns)
        OrderByColumnIndexDesc(nsNavHistory::kGetInfoIndex_ItemDateAdded);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_LASTMODIFIED_ASCENDING:
      if (mHasDateColumns)
        OrderByColumnIndexAsc(nsNavHistory::kGetInfoIndex_ItemLastModified);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_LASTMODIFIED_DESCENDING:
      if (mHasDateColumns)
        OrderByColumnIndexDesc(nsNavHistory::kGetInfoIndex_ItemLastModified);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_TAGS_ASCENDING:
    case nsINavHistoryQueryOptions::SORT_BY_TAGS_DESCENDING:
    case nsINavHistoryQueryOptions::SORT_BY_ANNOTATION_ASCENDING:
    case nsINavHistoryQueryOptions::SORT_BY_ANNOTATION_DESCENDING:
      break; // Sort later in nsNavHistoryQueryResultNode::FillChildren()
    default:
      NS_NOTREACHED("Invalid sorting mode");
  }
  return NS_OK;
}

void PlacesSQLQueryBuilder::OrderByColumnIndexAsc(PRInt32 aIndex)
{
  mQueryString += nsPrintfCString(128, " ORDER BY %d ASC", aIndex+1);
}

void PlacesSQLQueryBuilder::OrderByColumnIndexDesc(PRInt32 aIndex)
{
  mQueryString += nsPrintfCString(128, " ORDER BY %d DESC", aIndex+1);
}

void PlacesSQLQueryBuilder::OrderByTextColumnIndexAsc(PRInt32 aIndex)
{
  mQueryString += nsPrintfCString(128, " ORDER BY %d COLLATE NOCASE ASC",
                                  aIndex+1);
}

void PlacesSQLQueryBuilder::OrderByTextColumnIndexDesc(PRInt32 aIndex)
{
  mQueryString += nsPrintfCString(128, " ORDER BY %d COLLATE NOCASE DESC",
                                  aIndex+1);
}

nsresult
PlacesSQLQueryBuilder::Limit()
{
  if (mUseLimit && mMaxResults > 0) {
    mQueryString += NS_LITERAL_CSTRING(" LIMIT ");
    mQueryString.AppendInt(mMaxResults);
    mQueryString.AppendLiteral(" ");
  }
  return NS_OK;
}

nsresult
nsNavHistory::ConstructQueryString(
    const nsCOMArray<nsNavHistoryQuery>& aQueries,
    nsNavHistoryQueryOptions* aOptions, 
    nsCString& queryString, 
    PRBool& aParamsPresent,
    nsNavHistory::StringHash& aAddParams)
{
  // For information about visit_type see nsINavHistoryService.idl.
  // visitType == 0 is undefined (see bug #375777 for details).
  // Some sites, especially Javascript-heavy ones, load things in frames to 
  // display them, resulting in a lot of these entries. This is the reason 
  // why such visits are filtered out.
  nsresult rv;
  aParamsPresent = PR_FALSE;

  PRInt32 sortingMode = aOptions->SortingMode();
  NS_ASSERTION(sortingMode >= nsINavHistoryQueryOptions::SORT_BY_NONE &&
               sortingMode <= nsINavHistoryQueryOptions::SORT_BY_ANNOTATION_DESCENDING,
               "Invalid sortingMode found while building query!");

  PRBool hasSearchTerms = PR_FALSE;
  for (PRInt32 i = 0; i < aQueries.Count() && !hasSearchTerms; i++) {
    aQueries[i]->GetHasSearchTerms(&hasSearchTerms);
  }

  nsCAutoString tagsSqlFragment;
  GetTagsSqlFragment(GetTagsFolder(),
                     NS_LITERAL_CSTRING("h.id"),
                     hasSearchTerms,
                     tagsSqlFragment);

  if (IsOptimizableHistoryQuery(aQueries, aOptions,
        nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING) ||
      IsOptimizableHistoryQuery(aQueries, aOptions,
        nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING)) {
    // Generate an optimized query for the history menu and most visited
    // smart bookmark.
    queryString = NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, h.last_visit_date, "
          "f.url, null, null, null, null, null, ") +
          tagsSqlFragment + NS_LITERAL_CSTRING(
        "FROM moz_places_temp h "
        "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
        "WHERE h.hidden <> 1 "
          "AND EXISTS (SELECT id FROM moz_historyvisits_temp WHERE place_id = h.id "
                       "AND visit_type NOT IN ") +
                       nsPrintfCString("(0,%d,%d) ",
                                       nsINavHistoryService::TRANSITION_EMBED,
                                       nsINavHistoryService::TRANSITION_FRAMED_LINK) +
                       NS_LITERAL_CSTRING("UNION ALL "
                       "SELECT id FROM moz_historyvisits WHERE place_id = h.id "
                       "AND visit_type NOT IN ") +
                       nsPrintfCString("(0,%d,%d) ",
                                       nsINavHistoryService::TRANSITION_EMBED,
                                       nsINavHistoryService::TRANSITION_FRAMED_LINK) +
                       NS_LITERAL_CSTRING("LIMIT 1) "
          "{QUERY_OPTIONS} "
      "UNION ALL "
      "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, h.last_visit_date, "
          "f.url, null, null, null, null, null, ") +
          tagsSqlFragment + NS_LITERAL_CSTRING(
        "FROM moz_places h "
        "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
        "WHERE h.hidden <> 1 "
          "AND h.id NOT IN (SELECT id FROM moz_places_temp) "
          "AND EXISTS (SELECT id FROM moz_historyvisits_temp WHERE place_id = h.id "
                       "AND visit_type NOT IN ") +
                       nsPrintfCString("(0,%d,%d) ",
                                       nsINavHistoryService::TRANSITION_EMBED,
                                       nsINavHistoryService::TRANSITION_FRAMED_LINK) +
                       NS_LITERAL_CSTRING("UNION ALL "
                       "SELECT id FROM moz_historyvisits WHERE place_id = h.id "
                       "AND visit_type NOT IN ") +
                       nsPrintfCString("(0,%d,%d) ",
                                       nsINavHistoryService::TRANSITION_EMBED,
                                       nsINavHistoryService::TRANSITION_FRAMED_LINK) +
                       NS_LITERAL_CSTRING("LIMIT 1) "
          "{QUERY_OPTIONS} "
        );

    queryString.Append(NS_LITERAL_CSTRING("ORDER BY "));
    if (sortingMode == nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING)
      queryString.Append(NS_LITERAL_CSTRING("last_visit_date DESC "));
    else
      queryString.Append(NS_LITERAL_CSTRING("visit_count DESC "));

    queryString.Append(NS_LITERAL_CSTRING("LIMIT "));
    queryString.AppendInt(aOptions->MaxResults());

    nsCAutoString additionalQueryOptions;
    if (aOptions->RedirectsMode() ==
          nsINavHistoryQueryOptions::REDIRECTS_MODE_SOURCE) {
      additionalQueryOptions +=  nsPrintfCString(256,
        "AND NOT EXISTS ( "
          "SELECT id FROM moz_historyvisits_temp WHERE place_id = h.id "
                                             "AND visit_type IN (%d,%d)"
        ") "
        "AND NOT EXISTS ( "
          "SELECT id FROM moz_historyvisits WHERE place_id = h.id "
                                             "AND visit_type IN (%d,%d)"
        ") ",
        TRANSITION_REDIRECT_PERMANENT,
        TRANSITION_REDIRECT_TEMPORARY,
        TRANSITION_REDIRECT_PERMANENT,
        TRANSITION_REDIRECT_TEMPORARY);
    }
    else if (aOptions->RedirectsMode() ==
              nsINavHistoryQueryOptions::REDIRECTS_MODE_TARGET) {
      additionalQueryOptions += nsPrintfCString(1024,
        "AND NOT EXISTS ( "
          "SELECT id "
          "FROM moz_historyvisits_temp v "
          "WHERE place_id = h.id "
            "AND EXISTS(SELECT id FROM moz_historyvisits_temp "
                           "WHERE from_visit = v.id AND visit_type IN (%d,%d) "
                        "UNION ALL "
                        "SELECT id FROM moz_historyvisits "
                           "WHERE from_visit = v.id AND visit_type IN (%d,%d)) "
          "UNION ALL "
          "SELECT id "
          "FROM moz_historyvisits v "
          "WHERE place_id = h.id "
            "AND EXISTS(SELECT id FROM moz_historyvisits_temp "
                           "WHERE from_visit = v.id AND visit_type IN (%d,%d) "
                        "UNION ALL "
                        "SELECT id FROM moz_historyvisits "
                           "WHERE from_visit = v.id AND visit_type IN (%d,%d)) "
        ") ",
        TRANSITION_REDIRECT_PERMANENT,
        TRANSITION_REDIRECT_TEMPORARY,
        TRANSITION_REDIRECT_PERMANENT,
        TRANSITION_REDIRECT_TEMPORARY,
        TRANSITION_REDIRECT_PERMANENT,
        TRANSITION_REDIRECT_TEMPORARY,
        TRANSITION_REDIRECT_PERMANENT,
        TRANSITION_REDIRECT_TEMPORARY);
    }
    queryString.ReplaceSubstring("{QUERY_OPTIONS}",
                                  additionalQueryOptions.get());
    return NS_OK;
  }

  nsCAutoString conditions;
  for (PRInt32 i = 0; i < aQueries.Count(); i++) {
    nsCString queryClause;
    rv = QueryToSelectClause(aQueries[i], aOptions, i, &queryClause);
    NS_ENSURE_SUCCESS(rv, rv);
    if (! queryClause.IsEmpty()) {
      aParamsPresent = PR_TRUE;
      if (! conditions.IsEmpty()) // exists previous clause: multiple ones are ORed
        conditions += NS_LITERAL_CSTRING(" OR ");
      conditions += NS_LITERAL_CSTRING("(") + queryClause +
        NS_LITERAL_CSTRING(")");
    }
  }

  // Determine whether we can push maxResults constraints into the queries
  // as LIMIT, or if we need to do result count clamping later
  // using FilterResultSet()
  PRBool useLimitClause = !NeedToFilterResultSet(aQueries, aOptions);

  PlacesSQLQueryBuilder queryStringBuilder(conditions, aOptions,
                                           useLimitClause, aAddParams,
                                           hasSearchTerms);
  rv = queryStringBuilder.GetQueryString(queryString);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

PLDHashOperator BindAdditionalParameter(nsNavHistory::StringHash::KeyType aParamName, 
                                        nsCString aParamValue,
                                        void* aStatement)
{
  mozIStorageStatement* stmt = static_cast<mozIStorageStatement*>(aStatement);

  nsresult rv = stmt->BindUTF8StringByName(aParamName, aParamValue);
  if (NS_FAILED(rv))
    return PL_DHASH_STOP;

  return PL_DHASH_NEXT;
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
nsNavHistory::GetQueryResults(nsNavHistoryQueryResultNode *aResultNode,
                              const nsCOMArray<nsNavHistoryQuery>& aQueries,
                              nsNavHistoryQueryOptions *aOptions,
                              nsCOMArray<nsNavHistoryResultNode>* aResults)
{
  NS_ENSURE_ARG_POINTER(aOptions);
  NS_ASSERTION(aResults->Count() == 0, "Initial result array must be empty");
  if (! aQueries.Count())
    return NS_ERROR_INVALID_ARG;

  nsCString queryString;
  PRBool paramsPresent = PR_FALSE;
  nsNavHistory::StringHash addParams;
  addParams.Init(HISTORY_DATE_CONT_MAX);
  nsresult rv = ConstructQueryString(aQueries, aOptions, queryString, 
                                     paramsPresent, addParams);
  NS_ENSURE_SUCCESS(rv,rv);

  // create statement
  nsCOMPtr<mozIStorageStatement> statement;
  rv = mDBConn->CreateStatement(queryString, getter_AddRefs(statement));
#ifdef DEBUG
  if (NS_FAILED(rv)) {
    nsCAutoString lastErrorString;
    (void)mDBConn->GetLastErrorString(lastErrorString);
    PRInt32 lastError = 0;
    (void)mDBConn->GetLastError(&lastError);
    printf("Places failed to create a statement from this query:\n%s\nStorage error (%d): %s\n",
           PromiseFlatCString(queryString).get(),
           lastError,
           PromiseFlatCString(lastErrorString).get());
  }
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  if (paramsPresent) {
    // bind parameters
    PRInt32 i;
    for (i = 0; i < aQueries.Count(); i++) {
      rv = BindQueryClauseParameters(statement, i, aQueries[i], aOptions);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  addParams.EnumerateRead(BindAdditionalParameter, statement.get());

  // Optimize the case where there is no need for any post-query filtering.
  if (NeedToFilterResultSet(aQueries, aOptions)) {
    // Generate the top-level results.
    nsCOMArray<nsNavHistoryResultNode> toplevel;
    rv = ResultsAsList(statement, aOptions, &toplevel);
    NS_ENSURE_SUCCESS(rv, rv);

    FilterResultSet(aResultNode, toplevel, aResults, aQueries, aOptions);
  } else {
    rv = ResultsAsList(statement, aOptions, aResults);
    NS_ENSURE_SUCCESS(rv, rv);
  } 

  return NS_OK;
}

// nsNavHistory::AddObserver

NS_IMETHODIMP
nsNavHistory::AddObserver(nsINavHistoryObserver* aObserver, PRBool aOwnsWeak)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aObserver);

  return mObservers.AppendWeakElement(aObserver, aOwnsWeak);
}


// nsNavHistory::RemoveObserver

NS_IMETHODIMP
nsNavHistory::RemoveObserver(nsINavHistoryObserver* aObserver)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aObserver);

  return mObservers.RemoveWeakElement(aObserver);
}

// nsNavHistory::BeginUpdateBatch
// See RunInBatchMode
nsresult
nsNavHistory::BeginUpdateBatch()
{
  if (mBatchLevel++ == 0) {
    mBatchDBTransaction = new mozStorageTransaction(mDBConn, PR_FALSE);

    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavHistoryObserver, OnBeginUpdateBatch());
  }
  return NS_OK;
}

// nsNavHistory::EndUpdateBatch
nsresult
nsNavHistory::EndUpdateBatch()
{
  if (--mBatchLevel == 0) {
    if (mBatchDBTransaction) {
      nsresult rv = mBatchDBTransaction->Commit();
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Batch failed to commit transaction");
      delete mBatchDBTransaction;
      mBatchDBTransaction = nsnull;
    }

    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavHistoryObserver, OnEndUpdateBatch());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::RunInBatchMode(nsINavHistoryBatchCallback* aCallback,
                             nsISupports* aUserData)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aCallback);

  UpdateBatchScoper batch(*this);
  return aCallback->RunBatched(aUserData);
}

NS_IMETHODIMP
nsNavHistory::GetHistoryDisabled(PRBool *_retval)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
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

NS_IMETHODIMP
nsNavHistory::AddPageWithDetails(nsIURI *aURI, const PRUnichar *aTitle,
                                 PRInt64 aLastVisited)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // Don't update the page title inside the private browsing mode.
  if (InPrivateBrowsingMode())
    return NS_OK;

  PRInt64 visitID;
  nsresult rv = AddVisit(aURI, aLastVisited, 0, TRANSITION_LINK, PR_FALSE,
                         0, &visitID);
  NS_ENSURE_SUCCESS(rv, rv);

  return SetPageTitleInternal(aURI, nsString(aTitle));
}


// nsNavHistory::GetLastPageVisited
//
//    This was once used when the new window is set to "previous page." It
//    doesn't seem to be used anymore, so we don't spend any time precompiling
//    the statement.

NS_IMETHODIMP
nsNavHistory::GetLastPageVisited(nsACString & aLastPageVisited)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  nsCOMPtr<mozIStorageStatement> statement;
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // expect newest visits being in temp table.
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT url, visit_date FROM moz_historyvisits_temp v "
      "JOIN moz_places_temp h ON v.place_id = h.id "
      "WHERE h.hidden <> 1 "
      "UNION ALL "
      "SELECT url, visit_date FROM moz_historyvisits_temp v "
      "JOIN moz_places h ON v.place_id = h.id "
      "WHERE h.hidden <> 1 "
      "UNION ALL "
      "SELECT url, visit_date FROM moz_historyvisits v "
      "JOIN moz_places_temp h ON v.place_id = h.id "
      "WHERE h.hidden <> 1 "
      "UNION ALL "
      "SELECT url, visit_date FROM moz_historyvisits v "
      "JOIN moz_places h ON v.place_id = h.id "
      "WHERE h.hidden <> 1 "
      "ORDER BY visit_date DESC "
      "LIMIT 1 "),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMatch = PR_FALSE;
  if (NS_SUCCEEDED(statement->ExecuteStep(&hasMatch)) && hasMatch)
    return statement->GetUTF8String(0, aLastPageVisited);

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
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG_POINTER(aCount);

  PRBool hasEntries = PR_FALSE;
  nsresult rv = GetHasHistoryEntries(&hasEntries);
  if (hasEntries)
    *aCount = 1;
  else
    *aCount = 0;
  return rv;
}


// nsNavHistory::RemovePagesInternal
//
//    Deletes a list of placeIds from history.
//    This is an internal method used by RemovePages, RemovePagesFromHost and
//    RemovePagesByTimeframe.
//    Takes a comma separated list of place ids.
//    This method does not do any observer notification.

nsresult
nsNavHistory::RemovePagesInternal(const nsCString& aPlaceIdsQueryString)
{
  // Return early if there is nothing to delete.
  if (aPlaceIdsQueryString.IsEmpty())
    return NS_OK;

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  nsresult rv = PreparePlacesForVisitsDelete(aPlaceIdsQueryString);
  NS_ENSURE_SUCCESS(rv, rv);

  // Delete all visits for the specified place ids.
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_historyvisits_view WHERE place_id IN (") +
        aPlaceIdsQueryString +
        NS_LITERAL_CSTRING(")"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CleanupPlacesOnVisitsDelete(aPlaceIdsQueryString);
  NS_ENSURE_SUCCESS(rv, rv);

  // Invalidate the cached value for whether there's history or not.
  mHasHistoryEntries = -1;

  return transaction.Commit();
}


/**
 * Prepares for deletion places that are about to have all their visits removed.
 * This is an internal method used by RemovePagesInternal and
 * RemoveVisitsByTimeframe.  This method does not execute in a transaction, so
 * callers should make sure they begin one if needed.
 *
 * @param aPlaceIdsQueryString
 *        A comma-separated list of place IDs, each of which is about to have
 *        all its visits removed
 */
nsresult
nsNavHistory::PreparePlacesForVisitsDelete(const nsCString& aPlaceIdsQueryString)
{
  // Return early if there is nothing to delete.
  if (aPlaceIdsQueryString.IsEmpty())
    return NS_OK;

  // if a moz_place is annotated or was a bookmark,
  // we won't delete it, but we will delete the moz_visits
  // so we need to reset the frecency.  Note, we set frecency to
  // -visit_count, as we use that value in our "on idle" query
  // to figure out which places to recalculate frecency first.
  // Pay attention to not set frecency = 0 if visit_count = 0
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "UPDATE moz_places_view "
      "SET frecency = -MAX(visit_count, 1) "
      "WHERE id IN ( "
        "SELECT h.id " 
        "FROM moz_places_temp h "
        "WHERE h.id IN ( ") + aPlaceIdsQueryString + NS_LITERAL_CSTRING(") "
          "AND ( "
            "EXISTS (SELECT b.id FROM moz_bookmarks b WHERE b.fk =h.id) "
            "OR EXISTS (SELECT a.id FROM moz_annos a WHERE a.place_id = h.id) "
          ") "
        "UNION ALL "
        "SELECT h.id " 
        "FROM moz_places h "
        "WHERE h.id IN ( ") + aPlaceIdsQueryString + NS_LITERAL_CSTRING(") "
          "AND h.id NOT IN (SELECT id FROM moz_places_temp) "
          "AND ( "
            "EXISTS (SELECT b.id FROM moz_bookmarks b WHERE b.fk =h.id) "
            "OR EXISTS (SELECT a.id FROM moz_annos a WHERE a.place_id = h.id) "
          ") "        
      ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


/**
 * Performs cleanup on places that just had all their visits removed, including
 * deletion of those places.  This is an internal method used by
 * RemovePagesInternal and RemoveVisitsByTimeframe.  This method does not
 * execute in a transaction, so callers should make sure they begin one if
 * needed.
 *
 * @param aPlaceIdsQueryString
 *        A comma-separated list of place IDs, each of which just had all its
 *        visits removed
 */
nsresult
nsNavHistory::CleanupPlacesOnVisitsDelete(const nsCString& aPlaceIdsQueryString)
{
  // Return early if there is nothing to delete.
  if (aPlaceIdsQueryString.IsEmpty())
    return NS_OK;

  // if the entry is not bookmarked and is not a place: uri
  // then we can remove it from moz_places.
  // Note that we do NOT delete favicons. Any unreferenced favicons will be
  // deleted next time the browser is shut down.
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_places_view WHERE id IN ("
        "SELECT h.id FROM moz_places_temp h "
        "WHERE h.id IN ( ") + aPlaceIdsQueryString + NS_LITERAL_CSTRING(") "
          "AND SUBSTR(h.url, 1, 6) <> 'place:' "
          "AND NOT EXISTS "
            "(SELECT b.id FROM moz_bookmarks b WHERE b.fk = h.id LIMIT 1) "
        "UNION ALL "
        "SELECT h.id FROM moz_places h "
        "WHERE h.id NOT IN (SELECT id FROM moz_places_temp) "
          "AND h.id IN ( ") + aPlaceIdsQueryString + NS_LITERAL_CSTRING(") "
          "AND SUBSTR(h.url, 1, 6) <> 'place:' "
          "AND NOT EXISTS "
            "(SELECT b.id FROM moz_bookmarks b WHERE b.fk = h.id LIMIT 1) "
    ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  // If we have removed all visits to a livemark's child, we need to fix its
  // frecency, or it would appear in the url bar autocomplete.
  // XXX this might be dog slow, further degrading delete perf.
  rv = FixInvalidFrecenciesForExcludedPlaces();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavHistory::RemovePages
//
//    Removes a bunch of uris from history.
//    Has better performance than RemovePage when deleting a lot of history.
//    Notice that this function does not call the onDeleteURI observers,
//    instead, if aDoBatchNotify is true, we call OnBegin/EndUpdateBatch.
//    We don't do duplicates removal, URIs array should be cleaned-up before.

NS_IMETHODIMP
nsNavHistory::RemovePages(nsIURI **aURIs, PRUint32 aLength, PRBool aDoBatchNotify)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURIs);

  nsresult rv;
  // build a list of place ids to delete
  nsCString deletePlaceIdsQueryString;
  for (PRUint32 i = 0; i < aLength; i++) {
    PRInt64 placeId;
    rv = GetUrlIdFor(aURIs[i], &placeId, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    if (placeId != 0) {
      if (!deletePlaceIdsQueryString.IsEmpty())
        deletePlaceIdsQueryString.AppendLiteral(",");
      deletePlaceIdsQueryString.AppendInt(placeId);
    }
  }

  rv = RemovePagesInternal(deletePlaceIdsQueryString);
  NS_ENSURE_SUCCESS(rv, rv);

  // force a full refresh calling onEndUpdateBatch (will call Refresh())
  if (aDoBatchNotify)
    UpdateBatchScoper batch(*this); // sends Begin/EndUpdateBatch to observers

  return NS_OK;
}


// nsNavHistory::RemovePage
//
//    Removes all visits and the main history entry for the given URI.
//    Silently fails if we have no knowledge of the page.

NS_IMETHODIMP
nsNavHistory::RemovePage(nsIURI *aURI)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // Before we remove, we have to notify our observers!
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver, OnBeforeDeleteURI(aURI));

  nsIURI** URIs = &aURI;
  nsresult rv = RemovePages(URIs, 1, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify our observers that the URI has been removed.
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver, OnDeleteURI(aURI));
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
//    Silently fails if we have no knowledge of the host.
//
//    This sends onBeginUpdateBatch/onEndUpdateBatch to observers

NS_IMETHODIMP
nsNavHistory::RemovePagesFromHost(const nsACString& aHost, PRBool aEntireDomain)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  nsresult rv;
  // Local files don't have any host name. We don't want to delete all files in
  // history when we get passed an empty string, so force to exact match
  if (aHost.IsEmpty())
    aEntireDomain = PR_FALSE;

  // translate "(local files)" to an empty host name
  // be sure to use the TitleForDomain to get the localized name
  nsCString localFiles;
  TitleForDomain(EmptyCString(), localFiles);
  nsAutoString host16;
  if (!aHost.Equals(localFiles))
    CopyUTF8toUTF16(aHost, host16);

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

  // build condition string based on host selection type
  nsCAutoString conditionString;
  if (aEntireDomain)
    conditionString.AssignLiteral("rev_host >= ?1 AND rev_host < ?2 ");
  else
    conditionString.AssignLiteral("rev_host = ?1 ");

  nsCOMPtr<mozIStorageStatement> statement;

  // create statement depending on delete type
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id FROM moz_places_temp "
      "WHERE ") + conditionString + NS_LITERAL_CSTRING(
      "UNION ALL "
      "SELECT id FROM moz_places "
      "WHERE id NOT IN (SELECT id FROM moz_places_temp) "
        "AND ") + conditionString,
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindStringByIndex(0, revHostDot);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aEntireDomain) {
    rv = statement->BindStringByIndex(1, revHostSlash);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCString hostPlaceIds;
  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
    if (!hostPlaceIds.IsEmpty())
      hostPlaceIds.AppendLiteral(",");
    PRInt64 placeId;
    rv = statement->GetInt64(0, &placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    hostPlaceIds.AppendInt(placeId);
  }

  // force a full refresh calling onEndUpdateBatch (will call Refresh())
  UpdateBatchScoper batch(*this); // sends Begin/EndUpdateBatch to observers

  rv = RemovePagesInternal(hostPlaceIds);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavHistory::RemovePagesByTimeframe
//
//    This function will delete all history information about
//    pages for a given timeframe.
//    Limits are included: aBeginTime <= timeframe <= aEndTime
//
//    This method sends onBeginUpdateBatch/onEndUpdateBatch to observers

NS_IMETHODIMP
nsNavHistory::RemovePagesByTimeframe(PRTime aBeginTime, PRTime aEndTime)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  nsresult rv;
  // build a list of place ids to delete
  nsCString deletePlaceIdsQueryString;

  // we only need to know if a place has a visit into the given timeframe
  // this query is faster than actually selecting in moz_historyvisits
  nsCOMPtr<mozIStorageStatement> selectByTime;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id FROM moz_places_temp h WHERE "
        "EXISTS "
          "(SELECT id FROM moz_historyvisits v WHERE v.place_id = h.id "
            "AND v.visit_date >= :from_date AND v.visit_date <= :to_date LIMIT 1)"
        "OR EXISTS "
          "(SELECT id FROM moz_historyvisits_temp v WHERE v.place_id = h.id "
            "AND v.visit_date >= :from_date AND v.visit_date <= :to_date LIMIT 1) "
      "UNION "
      "SELECT h.id FROM moz_places h WHERE "
        "EXISTS "
          "(SELECT id FROM moz_historyvisits v WHERE v.place_id = h.id "
            "AND v.visit_date >= :from_date AND v.visit_date <= :to_date LIMIT 1)"
        "OR EXISTS "
          "(SELECT id FROM moz_historyvisits_temp v WHERE v.place_id = h.id "
            "AND v.visit_date >= :from_date AND v.visit_date <= :to_date LIMIT 1)"),
    getter_AddRefs(selectByTime));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = selectByTime->BindInt64ByName(NS_LITERAL_CSTRING("from_date"), aBeginTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = selectByTime->BindInt64ByName(NS_LITERAL_CSTRING("to_date"), aEndTime);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(selectByTime->ExecuteStep(&hasMore)) && hasMore) {
    PRInt64 placeId;
    rv = selectByTime->GetInt64(0, &placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    if (placeId != 0) {
      if (!deletePlaceIdsQueryString.IsEmpty())
        deletePlaceIdsQueryString.AppendLiteral(",");
      deletePlaceIdsQueryString.AppendInt(placeId);
    }
  }

  rv = RemovePagesInternal(deletePlaceIdsQueryString);
  NS_ENSURE_SUCCESS(rv, rv);

  // force a full refresh calling onEndUpdateBatch (will call Refresh())
  UpdateBatchScoper batch(*this); // sends Begin/EndUpdateBatch to observers

  return NS_OK;
}


/**
 * Removes all visits in a given timeframe.  Limits are included:
 * aBeginTime <= timeframe <= aEndTime.  Any place that becomes unvisited
 * as a result will also be deleted.
 *
 * Note that removal is performed in batch, so observers will not be
 * notified of individual places that are deleted.  Instead they will be
 * notified onBeginUpdateBatch and onEndUpdateBatch.
 *
 * @param aBeginTime
 *        The start of the timeframe, inclusive
 * @param aEndTime
 *        The end of the timeframe, inclusive
 */
NS_IMETHODIMP
nsNavHistory::RemoveVisitsByTimeframe(PRTime aBeginTime, PRTime aEndTime)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  nsresult rv;

  // Build a list of place IDs whose visits fall entirely within the timespan.
  // These places will be deleted by the call to CleanupPlacesOnVisitsDelete
  // below.
  nsCString deletePlaceIdsQueryString;
  {
    nsCOMPtr<mozIStorageStatement> selectByTime;
    mozStorageStatementScoper scope(selectByTime);
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT place_id "
        "FROM moz_historyvisits_temp "
        "WHERE :from_date <= visit_date AND visit_date <= :to_date "
        "UNION "
        "SELECT place_id "
        "FROM moz_historyvisits "
        "WHERE :from_date <= visit_date AND visit_date <= :to_date "
        "EXCEPT "
        "SELECT place_id "
        "FROM moz_historyvisits_temp "
        "WHERE visit_date < :from_date OR :to_date < visit_date "
        "EXCEPT "
        "SELECT place_id "
        "FROM moz_historyvisits "
        "WHERE visit_date < :from_date OR :to_date < visit_date"),
      getter_AddRefs(selectByTime));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = selectByTime->BindInt64ByName(NS_LITERAL_CSTRING("from_date"), aBeginTime);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = selectByTime->BindInt64ByName(NS_LITERAL_CSTRING("to_date"), aEndTime);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore = PR_FALSE;
    while (NS_SUCCEEDED(selectByTime->ExecuteStep(&hasMore)) && hasMore) {
      PRInt64 placeId;
      rv = selectByTime->GetInt64(0, &placeId);
      NS_ENSURE_SUCCESS(rv, rv);
      // placeId should not be <= 0, but be defensive.
      if (placeId > 0) {
        if (!deletePlaceIdsQueryString.IsEmpty())
          deletePlaceIdsQueryString.AppendLiteral(",");
        deletePlaceIdsQueryString.AppendInt(placeId);
      }
    }
  }

  // force a full refresh calling onEndUpdateBatch (will call Refresh())
  UpdateBatchScoper batch(*this); // sends Begin/EndUpdateBatch to observers

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  rv = PreparePlacesForVisitsDelete(deletePlaceIdsQueryString);
  NS_ENSURE_SUCCESS(rv, rv);

  // Delete all visits within the timeframe.
  nsCOMPtr<mozIStorageStatement> deleteVisitsStmt;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_historyvisits_view "
      "WHERE :from_date <= visit_date AND visit_date <= :to_date"),
    getter_AddRefs(deleteVisitsStmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteVisitsStmt->BindInt64ByName(NS_LITERAL_CSTRING("from_date"), aBeginTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteVisitsStmt->BindInt64ByName(NS_LITERAL_CSTRING("to_date"), aEndTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = deleteVisitsStmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CleanupPlacesOnVisitsDelete(deletePlaceIdsQueryString);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // Invalidate the cached value for whether there's history or not.
  mHasHistoryEntries = -1;

  return NS_OK;
}


// nsNavHistory::RemoveAllPages
//
//    This function is used to clear history.

NS_IMETHODIMP
nsNavHistory::RemoveAllPages()
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // reset frecency for all items that will _not_ be deleted
  // Note, we set frecency to -visit_count since we use that value in our
  // idle query to figure out which places to recalcuate frecency first.
  // We must do this before deleting visits.
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_places_view SET frecency = -MAX(visit_count, 1) "
    "WHERE id IN("
      "SELECT h.id FROM moz_places_temp h "
      "WHERE EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) "
      "UNION ALL "
      "SELECT h.id FROM moz_places h "
      "WHERE EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) "
    ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Expire visits, then let the paranoid functions do the cleanup for us.
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_historyvisits_view"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Some of the remaining places could be place: urls or
  // unvisited livemark items, so setting the frecency to -1
  // will cause them to show up in the url bar autocomplete
  // call FixInvalidFrecenciesForExcludedPlaces to handle that scenario.
  rv = FixInvalidFrecenciesForExcludedPlaces();
  if (NS_FAILED(rv))
    NS_WARNING("failed to fix invalid frecencies");

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  // Invalidate the cached value for whether there's history or not.
  mHasHistoryEntries = -1;

  // Expiration will take care of orphans.
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver, OnClearHistory());

  // privacy cleanup, if there's an old history.dat around, just delete it
  nsCOMPtr<nsIFile> oldHistoryFile;
  rv = NS_GetSpecialDirectory(NS_APP_HISTORY_50_FILE,
                              getter_AddRefs(oldHistoryFile));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool fileExists;
  if (NS_SUCCEEDED(oldHistoryFile->Exists(&fileExists)) && fileExists) {
    rv = oldHistoryFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsNavHistory::HidePage
//
//    Sets the 'hidden' column to true. If we've not heard of the page, we
//    succeed and do nothing.

NS_IMETHODIMP
nsNavHistory::HidePage(nsIURI *aURI)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  return NS_ERROR_NOT_IMPLEMENTED;
}


// Call this method before visiting a URL in order to help determine the
// transition type of the visit.
// Later, in AddVisitChain() the next visit to this page will be associated to
// TRANSITION_TYPED.
//
// @see MarkPageAsFollowedBookmark

NS_IMETHODIMP
nsNavHistory::MarkPageAsTyped(nsIURI *aURI)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // don't add when history is disabled
  if (IsHistoryDisabled())
    return NS_OK;

  nsCAutoString uriString;
  nsresult rv = aURI->GetSpec(uriString);
  NS_ENSURE_SUCCESS(rv, rv);

  // if URL is already in the typed queue, then we need to remove the old one
  PRInt64 unusedEventTime;
  if (mRecentTyped.Get(uriString, &unusedEventTime))
    mRecentTyped.Remove(uriString);

  if (mRecentTyped.Count() > RECENT_EVENT_QUEUE_MAX_LENGTH)
    ExpireNonrecentEvents(&mRecentTyped);

  mRecentTyped.Put(uriString, GetNow());
  return NS_OK;
}


// Call this method before visiting a URL in order to help determine the
// transition type of the visit.
// Later, in AddVisitChain() the next visit to this page will be associated to
// TRANSITION_FRAMED_LINK or TRANSITION_LINK.
//
// @see MarkPageAsTyped

NS_IMETHODIMP
nsNavHistory::MarkPageAsFollowedLink(nsIURI *aURI)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // don't add when history is disabled
  if (IsHistoryDisabled())
    return NS_OK;

  nsCAutoString uriString;
  nsresult rv = aURI->GetSpec(uriString);
  NS_ENSURE_SUCCESS(rv, rv);

  // if URL is already in the links queue, then we need to remove the old one
  PRInt64 unusedEventTime;
  if (mRecentLink.Get(uriString, &unusedEventTime))
    mRecentLink.Remove(uriString);

  if (mRecentLink.Count() > RECENT_EVENT_QUEUE_MAX_LENGTH)
    ExpireNonrecentEvents(&mRecentLink);

  mRecentLink.Put(uriString, GetNow());
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistory::RegisterOpenPage(nsIURI* aURI)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // Don't add any pages while in Private Browsing mode, so as to avoid leaking
  // information about other windows that might otherwise stay hidden
  // and private.
  if (InPrivateBrowsingMode())
    return NS_OK;

  mozStorageStatementScoper scoper(mDBRegisterOpenPage);
  nsresult rv = URIBinder::Bind(mDBRegisterOpenPage,
                                NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBRegisterOpenPage->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavHistory::UnregisterOpenPage(nsIURI* aURI)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // Entering Private Browsing mode will unregister all open pages, therefore
  // there shouldn't be anything in the moz_openpages_temp table. So we can stop
  // now without doing any unnecessary work.
  if (InPrivateBrowsingMode())
    return NS_OK;

  mozStorageStatementScoper scoper(mDBUnregisterOpenPage);
  nsresult rv = URIBinder::Bind(mDBUnregisterOpenPage,
                                NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBUnregisterOpenPage->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavHistory::SetCharsetForURI
//
// Sets the character-set for a URI.
// If aCharset is empty remove character-set annotation for aURI.

NS_IMETHODIMP
nsNavHistory::SetCharsetForURI(nsIURI* aURI,
                               const nsAString& aCharset)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);

  if (aCharset.IsEmpty()) {
    // remove the current page character-set annotation
    nsresult rv = annosvc->RemovePageAnnotation(aURI, CHARSET_ANNO);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Set page character-set annotation, silently overwrite if already exists
    nsresult rv = annosvc->SetPageAnnotationString(aURI, CHARSET_ANNO,
                                                   aCharset, 0,
                                                   nsAnnotationService::EXPIRE_NEVER);
    if (rv == NS_ERROR_INVALID_ARG) {
      // We don't have this page.  Silently fail.
      return NS_OK;
    }
    else if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}


// nsNavHistory::GetCharsetForURI
//
// Get the last saved character-set for a URI.

NS_IMETHODIMP
nsNavHistory::GetCharsetForURI(nsIURI* aURI, 
                               nsAString& aCharset)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);

  nsAutoString charset;
  nsresult rv = annosvc->GetPageAnnotationString(aURI, CHARSET_ANNO, aCharset);
  if (NS_FAILED(rv)) {
    // be sure to return an empty string if character-set is not found
    aCharset.Truncate();
  }
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
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // filter out any unwanted URIs
  PRBool canAdd = PR_FALSE;
  nsresult rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd)
    return NS_OK;

  PRTime now = PR_Now();

  rv = AddURIInternal(aURI, now, aRedirect, aToplevel, aReferrer);
  NS_ENSURE_SUCCESS(rv, rv);

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

  PRInt64 visitID = 0;
  PRInt64 sessionID = 0;
  nsresult rv = AddVisitChain(aURI, aTime, aToplevel, aRedirect, aReferrer,
                              &visitID, &sessionID);
  NS_ENSURE_SUCCESS(rv, rv);

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

nsresult
nsNavHistory::AddVisitChain(nsIURI* aURI,
                            PRTime aTime,
                            PRBool aToplevel,
                            PRBool aIsRedirect,
                            nsIURI* aReferrerURI,
                            PRInt64* aVisitID,
                            PRInt64* aSessionID)
{
  // This is the address that will be saved to from_visit column, will be
  // overwritten later if needed.
  nsCOMPtr<nsIURI> fromVisitURI = aReferrerURI;

  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // A visit is considered EMBED if it's in a frame and the page visit does not
  // come from a user's action (like clicking a link), otherwise is FRAMED_LINK.
  // An embed visit should not appear in history views.
  // See bug 381453 for details.
  PRBool isEmbedVisit = !aToplevel &&
                        !CheckIsRecentEvent(&mRecentLink, spec);

  // Check if this visit came from a redirect.
  PRUint32 transitionType = 0;
  PRTime redirectTime = 0;
  nsCAutoString redirectSourceUrl;
  if (GetRedirectFor(spec, redirectSourceUrl, &redirectTime, &transitionType)) {
    // redirectSourceUrl redirected to aURL, at redirectTime, with
    // a transitionType redirect.
    nsCOMPtr<nsIURI> redirectSourceURI;
    rv = NS_NewURI(getter_AddRefs(redirectSourceURI), redirectSourceUrl);
    NS_ENSURE_SUCCESS(rv, rv);

    // Don't add a new visit if a page redirects to itself.
    PRBool redirectIsSame;
    if (NS_SUCCEEDED(aURI->Equals(redirectSourceURI, &redirectIsSame)) &&
        redirectIsSame)
      return NS_OK;

    // Recusively call addVisitChain to walk up the chain till the first
    // not-redirected URI.
    // Ensure that the sources have a visit time smaller than aTime, otherwise
    // visits would end up incorrectly ordered.
    PRTime sourceTime = NS_MIN(redirectTime, aTime - 1);
    PRInt64 sourceVisitId = 0;
    rv = AddVisitChain(redirectSourceURI, sourceTime, aToplevel,
                       PR_TRUE, // Is a redirect.
                       aReferrerURI, // This one is the originating source.
                       &sourceVisitId, // Get back the visit id of the source.
                       aSessionID);
    NS_ENSURE_SUCCESS(rv, rv);

    // All the visits for preceding pages in the redirects chain have been
    // added, now add the visit to aURI.
    if (isEmbedVisit)
      transitionType = nsINavHistoryService::TRANSITION_EMBED;
    else if (!aToplevel)
      transitionType = nsINavHistoryService::TRANSITION_FRAMED_LINK;

    // This page is result of a redirect, save the source page in from_visit,
    // to be able to walk up the chain.
    // See bug 411966 and bug 428690 for details.
    // TODO: Add a closure table with a chain id to easily reconstruct chains
    // without having to recurse through the table.  See bug 468710.
    fromVisitURI = redirectSourceURI;
  }
  else if (aReferrerURI) {
    // This page does not come from a redirect and had a referrer.

    // Check if the referrer has a previous visit.
    PRTime lastVisitTime;
    PRInt64 referringVisitId;
    PRBool referrerHasPreviousVisit =
      FindLastVisit(aReferrerURI, &referringVisitId, &lastVisitTime, aSessionID);

    // Don't add a new visit if the referring site is the same as
    // the new site.  This happens when a page refreshes itself.
    // Otherwise, if the page has never been added, the visit should be
    // registered regardless.
    PRBool referrerIsSame;
    if (NS_SUCCEEDED(aURI->Equals(aReferrerURI, &referrerIsSame)) &&
        referrerIsSame && referrerHasPreviousVisit) {
      // Ensure a valid session id to the chain.
      if (aIsRedirect)
        *aSessionID = GetNewSessionID();
      return NS_OK;
    }

    if (!referrerHasPreviousVisit ||
        aTime - lastVisitTime > RECENT_EVENT_THRESHOLD) {
      // Either the referrer has no visits or the last visit is too
      // old to be part of this session.  Thus start a new session.
      *aSessionID = GetNewSessionID();
    }

    // Since referrer is set, this visit comes from an originating page.
    // For top-level windows, visit is considered user-initiated and it should
    // appear in history views.
    // Visits to pages in frames are distinguished between user-initiated ones
    // and automatic ones.
    if (isEmbedVisit)
      transitionType = nsINavHistoryService::TRANSITION_EMBED;
    else if (!aToplevel)
      transitionType = nsINavHistoryService::TRANSITION_FRAMED_LINK;
    else
      transitionType = nsINavHistoryService::TRANSITION_LINK;
  }
  else {
    // When there is no referrer:
    // - Check recent events for a typed-in uri.
    // - Check recent events for a bookmark selection.
    // - Otherwise mark as TRANSITION_LINK or TRANSITION_EMBED depending on
    //   whether it happens in a frame (see above for reasoning about this).
    // Drag and drop operations are not handled, so they will most likely
    // be marked as links.
    if (CheckIsRecentEvent(&mRecentTyped, spec))
      transitionType = nsINavHistoryService::TRANSITION_TYPED;
    else if (CheckIsRecentEvent(&mRecentBookmark, spec))
      transitionType = nsINavHistoryService::TRANSITION_BOOKMARK;
    else if (isEmbedVisit)
      transitionType = nsINavHistoryService::TRANSITION_EMBED;
    else if (!aToplevel)
      transitionType = nsINavHistoryService::TRANSITION_FRAMED_LINK;
    else
      transitionType = nsINavHistoryService::TRANSITION_LINK;

    // Since there is no referrer, there is no way to continue am existing
    // session.
    *aSessionID = GetNewSessionID();
  }

  NS_WARN_IF_FALSE(transitionType > 0, "Visit must have a transition type");

  // Create the visit and update the page entry.
  return AddVisit(aURI, aTime, fromVisitURI, transitionType,
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
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(_retval);

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
//    This sets the page title.
//
//    Note that we do not allow empty real titles and will silently ignore such
//    requests. When a URL is added we give it a default title based on the
//    URL. Most pages provide a title and it gets replaced to something better.
//    Some pages don't: some say <title></title>, and some don't have any title
//    element. In BOTH cases, we get SetPageTitle(URI, ""), but in both cases,
//    our default title is more useful to the user than "(no title)".

NS_IMETHODIMP
nsNavHistory::SetPageTitle(nsIURI* aURI,
                           const nsAString& aTitle)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // Don't update the page title inside the private browsing mode.
  if (InPrivateBrowsingMode())
    return NS_OK;

  // if aTitle is empty we want to clear the previous title.
  // We don't want to set it to an empty string, but to a NULL value,
  // so we use SetIsVoid and SetPageTitleInternal will take care of that

  nsresult rv;
  if (aTitle.IsEmpty()) {
    // Using a void string to bind a NULL in the database.
    nsString voidString;
    voidString.SetIsVoid(PR_TRUE);
    rv = SetPageTitleInternal(aURI, voidString);
  }
  else {
    rv = SetPageTitleInternal(aURI, aTitle);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::GetPageTitle(nsIURI* aURI, nsAString& aTitle)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  aTitle.Truncate(0);

  mozStorageStatementScoper scope(mDBGetURLPageInfo);
  nsresult rv = URIBinder::Bind(mDBGetURLPageInfo, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResults = PR_FALSE;
  rv = mDBGetURLPageInfo->ExecuteStep(&hasResults);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasResults) {
    aTitle.SetIsVoid(PR_TRUE);
    return NS_OK; // Not found, return a void string.
  }

  rv = mDBGetURLPageInfo->GetString(nsNavHistory::kGetInfoIndex_Title, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// nsNavHistory::GetURIGeckoFlags
//
//    FIXME: should we try to use annotations for this stuff?

NS_IMETHODIMP
nsNavHistory::GetURIGeckoFlags(nsIURI* aURI, PRUint32* aResult)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(aResult);

  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistory::SetURIGeckoFlags
//
//    FIXME: should we try to use annotations for this stuff?

NS_IMETHODIMP
nsNavHistory::SetURIGeckoFlags(nsIURI* aURI, PRUint32 aFlags)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIGlobalHistory3 ***********************************************************

// nsNavHistory::AddDocumentRedirect
//
//    This adds a redirect mapping from the destination of the redirect to the
//    source, time, and type. This mapping is used by GetRedirectFor when we
//    get a page added to reconstruct the redirects that happened when a page
//    is visited. See GetRedirectFor for more information

// this is the expiration callback function that deletes stale entries
PLDHashOperator nsNavHistory::ExpireNonrecentRedirects(
    nsCStringHashKey::KeyType aKey, RedirectInfo& aData, void* aUserArg)
{
  PRInt64* threshold = reinterpret_cast<PRInt64*>(aUserArg);
  if (aData.mTimeCreated < *threshold)
    return PL_DHASH_REMOVE;
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsNavHistory::AddDocumentRedirect(nsIChannel *aOldChannel,
                                  nsIChannel *aNewChannel,
                                  PRInt32 aFlags,
                                  PRBool aToplevel)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aOldChannel);
  NS_ENSURE_ARG(aNewChannel);

  // Ignore internal redirects.
  // These redirects are not initiated by the remote server, but specific to the
  // channel implementation, so they are ignored.
  if (aFlags & nsIChannelEventSink::REDIRECT_INTERNAL)
    return NS_OK;

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
    // Expire outdated cached redirects.
    PRInt64 threshold = PR_Now() - RECENT_EVENT_THRESHOLD;
    mRecentRedirects.Enumerate(ExpireNonrecentRedirects,
                               reinterpret_cast<void*>(&threshold));
  }

  RedirectInfo info;

  // Remove any old entries for this redirect destination, since they are going
  // to be replaced.
  if (mRecentRedirects.Get(newSpec, &info))
    mRecentRedirects.Remove(newSpec);
  // Save the new redirect info.
  info.mSourceURI = oldSpec;
  info.mTimeCreated = PR_Now();
  if (aFlags & nsIChannelEventSink::REDIRECT_TEMPORARY)
    info.mType = TRANSITION_REDIRECT_TEMPORARY;
  else
    info.mType = TRANSITION_REDIRECT_PERMANENT;
  mRecentRedirects.Put(newSpec, info);

  return NS_OK;
}

// nsIDownloadHistory **********************************************************

NS_IMETHODIMP
nsNavHistory::AddDownload(nsIURI* aSource, nsIURI* aReferrer,
                          PRTime aStartTime)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aSource);

  // don't add when history is disabled and silently fail
  if (IsHistoryDisabled())
    return NS_OK;

  PRInt64 visitID;
  return AddVisit(aSource, aStartTime, aReferrer, TRANSITION_DOWNLOAD, PR_FALSE,
                  0, &visitID);
}

// nsPIPlacesDatabase **********************************************************

NS_IMETHODIMP
nsNavHistory::GetDBConnection(mozIStorageConnection **_DBConnection)
{
  NS_ENSURE_ARG_POINTER(_DBConnection);
  NS_ADDREF(*_DBConnection = mDBConn);
  return NS_OK;
}


nsresult
nsNavHistory::FinalizeInternalStatements()
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  // nsNavHistory
  nsresult rv = FinalizeStatements();
  NS_ENSURE_SUCCESS(rv, rv);

  // nsNavBookmarks
  nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
  rv = bookmarks->FinalizeStatements();
  NS_ENSURE_SUCCESS(rv, rv);

  // nsAnnotationService
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);
  rv = annosvc->FinalizeStatements();
  NS_ENSURE_SUCCESS(rv, rv);

  // nsFaviconService
  nsFaviconService* iconsvc = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(iconsvc, NS_ERROR_OUT_OF_MEMORY);
  rv = iconsvc->FinalizeStatements();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// nsPIPlacesHistoryListenersNotifier ******************************************

NS_IMETHODIMP
nsNavHistory::NotifyOnPageExpired(nsIURI *aURI, PRTime aVisitTime,
                                  PRBool aWholeEntry)
{
  // Invalidate the cached value for whether there's history or not.
  mHasHistoryEntries = -1;

  if (aWholeEntry) {
    // Notify our observers that the page has been removed.
    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavHistoryObserver, OnDeleteURI(aURI));
  }
  else {
    // Notify our observers that some visits for the page have been removed.
    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavHistoryObserver, OnDeleteVisits(aURI, aVisitTime));
  }

  return NS_OK;
}

// nsIObserver *****************************************************************

NS_IMETHODIMP
nsNavHistory::Observe(nsISupports *aSubject, const char *aTopic,
                    const PRUnichar *aData)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  if (strcmp(aTopic, TOPIC_PROFILE_TEARDOWN) == 0) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (!os) {
      NS_WARNING("Unable to shutdown Places: Observer Service unavailable.");
      return NS_OK;
    }

    (void)os->RemoveObserver(this, TOPIC_PROFILE_TEARDOWN);
    (void)os->RemoveObserver(this, NS_PRIVATE_BROWSING_SWITCH_TOPIC);
    (void)os->RemoveObserver(this, TOPIC_IDLE_DAILY);
#ifdef MOZ_XUL
    (void)os->RemoveObserver(this, TOPIC_AUTOCOMPLETE_FEEDBACK_INCOMING);
#endif

    // If shutdown happens in the same scope as the service init, we should
    // immediately notify the places-init topic.  Otherwise, since it's an
    // enqueued notification and the event loop won't spin, it could be notified
    // after xpcom-shutdown, when the connection does not exist anymore.
    nsCOMPtr<nsISimpleEnumerator> e;
    nsresult rv = os->EnumerateObservers(TOPIC_PLACES_INIT_COMPLETE,
                                         getter_AddRefs(e));
    if (NS_SUCCEEDED(rv) && e) {
      nsCOMPtr<nsIObserver> observer;
      PRBool loop = PR_TRUE;
      while(NS_SUCCEEDED(e->HasMoreElements(&loop)) && loop) {
        e->GetNext(getter_AddRefs(observer));
        (void)observer->Observe(observer, TOPIC_PLACES_INIT_COMPLETE, nsnull);
      }
    }

    // Notify all Places users that we are about to shutdown.
    (void)os->NotifyObservers(nsnull, TOPIC_PLACES_SHUTDOWN, nsnull);
  }

  else if (strcmp(aTopic, TOPIC_PROFILE_CHANGE) == 0) {
    // Fire internal shutdown notifications.
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      (void)os->RemoveObserver(this, TOPIC_PROFILE_CHANGE);
      // Double notification allows to correctly enqueue tasks without the need
      // to enqueue notification events to the main-thread.  There is no
      // guarantee that the event loop will spin before xpcom-shutdown indeed,
      // see bug 580892.
      (void)os->NotifyObservers(nsnull, TOPIC_PLACES_WILL_CLOSE_CONNECTION, nsnull);
      (void)os->NotifyObservers(nsnull, TOPIC_PLACES_CONNECTION_CLOSING, nsnull);
    }

    // Operations that are unlikely to create issues to implementers should go
    // in profile teardown.  Any other thing that must run really late should be
    // here instead.

    // Don't even try to notify observers from this point on, the category
    // cache would init services that could try to use our APIs.
    mCanNotify = false;

    // Stop observing preferences changes.
    if (mPrefBranch)
      mPrefBranch->RemoveObserver("", this);

    // Finalize all statements.
    nsresult rv = FinalizeInternalStatements();
    NS_ENSURE_SUCCESS(rv, rv);

    // Finally, close the connection.
    nsRefPtr<PlacesEvent> closeListener =
      new PlacesEvent(TOPIC_PLACES_CONNECTION_CLOSED);
    (void)mDBConn->AsyncClose(closeListener);
  }

#ifdef MOZ_XUL
  else if (strcmp(aTopic, TOPIC_AUTOCOMPLETE_FEEDBACK_INCOMING) == 0) {
    nsCOMPtr<nsIAutoCompleteInput> input = do_QueryInterface(aSubject);
    if (!input)
      return NS_OK;

    nsCOMPtr<nsIAutoCompletePopup> popup;
    input->GetPopup(getter_AddRefs(popup));
    if (!popup)
      return NS_OK;

    nsCOMPtr<nsIAutoCompleteController> controller;
    input->GetController(getter_AddRefs(controller));
    if (!controller)
      return NS_OK;

    // Don't bother if the popup is closed
    PRBool open;
    nsresult rv = popup->GetPopupOpen(&open);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!open)
      return NS_OK;

    // Ignore if nothing selected from the popup
    PRInt32 selectedIndex;
    rv = popup->GetSelectedIndex(&selectedIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    if (selectedIndex == -1)
      return NS_OK;

    rv = AutoCompleteFeedback(selectedIndex, controller);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#endif
  else if (strcmp(aTopic, TOPIC_PREF_CHANGED) == 0) {
    LoadPrefs();
  }

  else if (strcmp(aTopic, TOPIC_IDLE_DAILY) == 0) {
    // Ensure our connection is still alive.  The idle-daily observer is removed
    // on shutdown, but we could have closed the connection earlier due
    // to errors or during normal shutdown process.
    NS_ENSURE_TRUE(mDBConn, NS_OK);

    (void)DecayFrecency();
    (void)VacuumDatabase();
  }

  else if (strcmp(aTopic, NS_PRIVATE_BROWSING_SWITCH_TOPIC) == 0) {
    if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).Equals(aData)) {
      mInPrivateBrowsing = PR_TRUE;
    }
    else if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).Equals(aData)) {
      mInPrivateBrowsing = PR_FALSE;
    }
  }

  else if (strcmp(aTopic, TOPIC_PLACES_INIT_COMPLETE) == 0) {
    nsCOMPtr<nsIObserverService> os =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(os, NS_ERROR_FAILURE);
    (void)os->RemoveObserver(this, TOPIC_PLACES_INIT_COMPLETE);

    // This code is only called if we've either imported or done a migration
    // from a pre-frecency build, so we will calculate all their frecencies.
    (void)FixInvalidFrecencies();
  }

  return NS_OK;
}


nsresult
nsNavHistory::VacuumDatabase()
{
  // SQLite cannot give us a real value for fragmentation percentage,
  // we could analyze the database file page by page, and count fragmented
  // space, but that would be slow and not maintainable across different SQLite
  // versions.
  // For this reason we just take a guess using the freelist count.
  // This way we know how much pages are unused, but we don't know anything
  // about fragmentation.
  // This ratio is used in conjunction with a time pref to avoid vacuuming too
  // often or too rarely.

  PRInt32 lastVacuumPref;
  PRInt64 lastVacuumTime = 0;
  if (mPrefBranch &&
      NS_SUCCEEDED(mPrefBranch->GetIntPref(PREF_LAST_VACUUM, &lastVacuumPref))) {
    // Value are seconds till epoch, convert it to microseconds.
    lastVacuumTime = (PRInt64)lastVacuumPref * PR_USEC_PER_SEC;
  }

  nsresult rv;
  float freePagesRatio = 0;
  if (!lastVacuumTime ||
      (lastVacuumTime < (PR_Now() - DATABASE_MIN_TIME_BEFORE_VACUUM) &&
       lastVacuumTime > (PR_Now() - DATABASE_MAX_TIME_BEFORE_VACUUM))) {
    // This is the first vacuum, or we are in the timeframe where vacuum could
    // happen.  Calculate the vacuum ratio and vacuum if it is less then
    // threshold.
    nsCOMPtr<mozIStorageStatement> statement;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("PRAGMA page_count"),
                                  getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool hasResult = PR_FALSE;
    rv = statement->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(hasResult, NS_ERROR_FAILURE);
    PRInt32 pageCount = statement->AsInt32(0);

    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("PRAGMA freelist_count"),
                                  getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);
    hasResult = PR_FALSE;
    rv = statement->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(hasResult, NS_ERROR_FAILURE);
    PRInt32 freelistCount = statement->AsInt32(0);

    freePagesRatio = (float)(freelistCount / pageCount);
  }
  
  if (freePagesRatio > DATABASE_VACUUM_FREEPAGES_THRESHOLD ||
      lastVacuumTime < (PR_Now() - DATABASE_MAX_TIME_BEFORE_VACUUM)) {
    // We vacuum in 2 cases:
    //  - We are in the valid vacuum timeframe and vacuum ratio is high.
    //  - Last vacuum has been executed a lot of time ago.

    // Notify we are about to vacuum.  This is mostly for testability.
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    if (observerService) {
      (void)observerService->NotifyObservers(nsnull,
                                             TOPIC_DATABASE_VACUUM_STARTING,
                                             nsnull);
    }

    // Actually vacuuming a database is a slow operation, since it could take
    // seconds.  Part of the time is spent in updating the journal file on disk
    // and this is particularly bad on devices with slow I/O.  Temporary
    // moving the journal to memory could increase a bit the possibility of
    // corruption if we crash during this time, but makes the process really
    // faster.
    nsCOMPtr<mozIStorageStatement> journalToMemory;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "PRAGMA journal_mode = MEMORY"),
      getter_AddRefs(journalToMemory));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageStatement> vacuum;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("VACUUM"),
                                  getter_AddRefs(vacuum));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageStatement> journalToDefault;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "PRAGMA journal_mode = " DATABASE_JOURNAL_MODE),
      getter_AddRefs(journalToDefault));
    NS_ENSURE_SUCCESS(rv, rv);

    mozIStorageBaseStatement *stmts[] = {
      journalToMemory,
      vacuum,
      journalToDefault
    };
    nsCOMPtr<mozIStoragePendingStatement> ps;
    nsRefPtr<VacuumDBListener> vacuumDBListener =
      new VacuumDBListener(mPrefBranch);
    rv = mDBConn->ExecuteAsync(stmts, NS_ARRAY_LENGTH(stmts),
                               vacuumDBListener, getter_AddRefs(ps));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


nsresult
nsNavHistory::DecayFrecency()
{
  // Update frecency values.
  nsresult rv = FixInvalidFrecencies();
  NS_ENSURE_SUCCESS(rv, rv);

  // Globally decay places frecency rankings to estimate reduced frecency
  // values of pages that haven't been visited for a while, i.e., they do
  // not get an updated frecency. We directly modify moz_places to avoid
  // bringing the whole database into places_temp through places_view. A
  // scaling factor of .975 results in .5 the original value after 28 days.
  nsCOMPtr<mozIStorageStatement> decayFrecency;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places SET frecency = ROUND(frecency * .975) "
      "WHERE frecency > 0"),
    getter_AddRefs(decayFrecency));
  NS_ENSURE_SUCCESS(rv, rv);

  // Decay potentially unused adaptive entries (e.g. those that are at 1)
  // to allow better chances for new entries that will start at 1.
  nsCOMPtr<mozIStorageStatement> decayAdaptive;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_inputhistory SET use_count = use_count * .975"),
    getter_AddRefs(decayAdaptive));
  NS_ENSURE_SUCCESS(rv, rv);

  // Delete any adaptive entries that won't help in ordering anymore.
  nsCOMPtr<mozIStorageStatement> deleteAdaptive;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_inputhistory WHERE use_count < .01"),
    getter_AddRefs(deleteAdaptive));
  NS_ENSURE_SUCCESS(rv, rv);

  mozIStorageBaseStatement *stmts[] = {
    decayFrecency,
    decayAdaptive,
    deleteAdaptive
  };
  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = mDBConn->ExecuteAsync(stmts, NS_ARRAY_LENGTH(stmts), nsnull,
                             getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// Query stuff *****************************************************************

// Helper class for QueryToSelectClause
//
// This class helps to build part of the WHERE clause. It supports 
// multiple queries by appending the query index to the parameter name. 
// For the query with index 0 the parameter name is not altered what
// allows using this parameter in other situations (see SelectAsSite). 

class ConditionBuilder
{
public:

  ConditionBuilder(PRInt32 aQueryIndex): mQueryIndex(aQueryIndex)
  { }

  ConditionBuilder& Condition(const char* aStr)
  {
    if (!mClause.IsEmpty())
      mClause.AppendLiteral(" AND ");
    Str(aStr);
    return *this;
  }

  ConditionBuilder& Str(const char* aStr)
  {
    mClause.Append(' ');
    mClause.Append(aStr);
    mClause.Append(' ');
    return *this;
  }

  ConditionBuilder& Param(const char* aParam)
  {
    mClause.Append(' ');
    if (!mQueryIndex)
      mClause.Append(aParam);
    else
      mClause += nsPrintfCString("%s%d", aParam, mQueryIndex);

    mClause.Append(' ');
    return *this;
  }

  void GetClauseString(nsCString& aResult) 
  {
    aResult = mClause;
  }

private:

  PRInt32 mQueryIndex;
  nsCString mClause;
};


// nsNavHistory::QueryToSelectClause
//
//    THE BEHAVIOR SHOULD BE IN SYNC WITH BindQueryClauseParameters
//
//    I don't check return values from the query object getters because there's
//    no way for those to fail.

nsresult
nsNavHistory::QueryToSelectClause(nsNavHistoryQuery* aQuery, // const
                                  nsNavHistoryQueryOptions* aOptions,
                                  PRInt32 aQueryIndex,
                                  nsCString* aClause)
{
  PRBool hasIt;

  ConditionBuilder clause(aQueryIndex);

  // begin time
  if (NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt) 
    clause.Condition("v.visit_date >=").Param(":begin_time");

  // end time
  if (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt)
    clause.Condition("v.visit_date <=").Param(":end_time");

  // search terms FIXME

  // min and max visit count
  if (aQuery->MinVisits() >= 0)
    clause.Condition("h.visit_count >=").Param(":min_visits");

  if (aQuery->MaxVisits() >= 0)
    clause.Condition("h.visit_count <=").Param(":max_visits");
  
  // only bookmarked, has no affect on bookmarks-only queries
  if (aOptions->QueryType() != nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS &&
      aQuery->OnlyBookmarked())
    clause.Condition("EXISTS (SELECT b.fk FROM moz_bookmarks b WHERE b.type = ")
          .Str(nsPrintfCString("%d", nsNavBookmarks::TYPE_BOOKMARK).get())
          .Str("AND b.fk = h.id)");

  // domain
  if (NS_SUCCEEDED(aQuery->GetHasDomain(&hasIt)) && hasIt) {
    PRBool domainIsHost = PR_FALSE;
    aQuery->GetDomainIsHost(&domainIsHost);
    if (domainIsHost)
      clause.Condition("h.rev_host =").Param(":domain_lower");
    else
      // see domain setting in BindQueryClauseParameters for why we do this
      clause.Condition("h.rev_host >=").Param(":domain_lower")
            .Condition("h.rev_host <").Param(":domain_upper");
  }

  // URI
  if (NS_SUCCEEDED(aQuery->GetHasUri(&hasIt)) && hasIt) {
    if (aQuery->UriIsPrefix()) {
      clause.Condition("h.url >= ").Param(":uri")
            .Condition("h.url <= ").Param(":uri_upper");
    }
    else
      clause.Condition("h.url =").Param(":uri");
  }

  // annotation
  aQuery->GetHasAnnotation(&hasIt);
  if (hasIt) {
    clause.Condition("");
    if (aQuery->AnnotationIsNot())
      clause.Str("NOT");
    clause.Str(
      "EXISTS "
        "(SELECT h.id "
         "FROM moz_annos anno "
         "JOIN moz_anno_attributes annoname "
           "ON anno.anno_attribute_id = annoname.id "
         "WHERE anno.place_id = h.id "
           "AND annoname.name = ").Param(":anno").Str(")");
    // annotation-based queries don't get the common conditions, so you get
    // all URLs with that annotation
  }

  // tags
  const nsTArray<nsString> &tags = aQuery->Tags();
  if (tags.Length() > 0) {
    clause.Condition("h.id");
    if (aQuery->TagsAreNot())
      clause.Str("NOT");
    clause.Str(
      "IN "
        "(SELECT bms.fk "
         "FROM moz_bookmarks bms "
         "JOIN moz_bookmarks tags ON bms.parent = tags.id "
         "WHERE tags.parent =").
           Param(":tags_folder").
           Str("AND tags.title IN (");
    for (PRUint32 i = 0; i < tags.Length(); ++i) {
      nsPrintfCString param(":tag%d_", i);
      clause.Param(param.get());
      if (i < tags.Length() - 1)
        clause.Str(",");
    }
    clause.Str(")");
    if (!aQuery->TagsAreNot())
      clause.Str("GROUP BY bms.fk HAVING count(*) >=").Param(":tag_count");
    clause.Str(")");
  }

  // transitions
  const nsTArray<PRUint32>& transitions = aQuery->Transitions();
  // Optimize single transition query, since this is the most common use case.
  if (transitions.Length() == 1) {
    clause.Condition("v.visit_type =").Param(":transition0_");
  }
  else if (transitions.Length() > 1) {
    for (PRUint32 i = 0; i < transitions.Length(); ++i) {
      nsPrintfCString param(":transition%d_", i);
      clause.Str("EXISTS (SELECT 1 FROM moz_historyvisits "
                         "WHERE place_id = h.id AND visit_type = "
                ).Param(param.get()).Str(" UNION ALL "
                         "SELECT 1 FROM moz_historyvisits_temp "
                         "WHERE place_id = h.id AND visit_type = "
                ).Param(param.get()).Str(" LIMIT 1)");
      if (i < transitions.Length() - 1)
        clause.Str("AND");
    }
  }

  // parent parameter is used in tag contents queries.
  // Only one folder should be defined for them.
  if (aOptions->ResultType() == nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS &&
      aQuery->Folders().Length() == 1) {
    clause.Condition("b.parent =").Param(":parent");
  }

  clause.GetClauseString(*aClause);
  return NS_OK;
}


// nsNavHistory::BindQueryClauseParameters
//
//    THE BEHAVIOR SHOULD BE IN SYNC WITH QueryToSelectClause

nsresult
nsNavHistory::BindQueryClauseParameters(mozIStorageStatement* statement,
                                        PRInt32 aQueryIndex,
                                        nsNavHistoryQuery* aQuery, // const
                                        nsNavHistoryQueryOptions* aOptions)
{
  nsresult rv;

  PRBool hasIt;
  // Append numbered index to param names, to replace them correctly in
  // case of multiple queries.  If we have just one query we don't change the
  // param name though.
  nsCAutoString qIndex;
  if (aQueryIndex > 0)
    qIndex.AppendInt(aQueryIndex);

  // begin time
  if (NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt) {
    PRTime time = NormalizeTime(aQuery->BeginTimeReference(),
                                aQuery->BeginTime());
    rv = statement->BindInt64ByName(
      NS_LITERAL_CSTRING("begin_time") + qIndex, time);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // end time
  if (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt) {
    PRTime time = NormalizeTime(aQuery->EndTimeReference(),
                                aQuery->EndTime());
    rv = statement->BindInt64ByName(
      NS_LITERAL_CSTRING("end_time") + qIndex, time
    );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // search terms FIXME

  // min and max visit count
  PRInt32 visits = aQuery->MinVisits();
  if (visits >= 0) {
    rv = statement->BindInt32ByName(
      NS_LITERAL_CSTRING("min_visits") + qIndex, visits
    );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  visits = aQuery->MaxVisits();
  if (visits >= 0) {
    rv = statement->BindInt32ByName(
      NS_LITERAL_CSTRING("max_visits") + qIndex, visits
    );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // domain (see GetReversedHostname for more info on reversed host names)
  if (NS_SUCCEEDED(aQuery->GetHasDomain(&hasIt)) && hasIt) {
    nsString revDomain;
    GetReversedHostname(NS_ConvertUTF8toUTF16(aQuery->Domain()), revDomain);

    if (aQuery->DomainIsHost()) {
      rv = statement->BindStringByName(
        NS_LITERAL_CSTRING("domain_lower") + qIndex, revDomain
      );
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // for "mozilla.org" do query >= "gro.allizom." AND < "gro.allizom/"
      // which will get everything starting with "gro.allizom." while using the
      // index (using SUBSTRING() causes indexes to be discarded).
      NS_ASSERTION(revDomain[revDomain.Length() - 1] == '.', "Invalid rev. host");
      rv = statement->BindStringByName(
        NS_LITERAL_CSTRING("domain_lower") + qIndex, revDomain
      );
      NS_ENSURE_SUCCESS(rv, rv);
      revDomain.Truncate(revDomain.Length() - 1);
      revDomain.Append(PRUnichar('/'));
      rv = statement->BindStringByName(
        NS_LITERAL_CSTRING("domain_upper") + qIndex, revDomain
      );
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // URI
  if (aQuery->Uri()) {
    rv = URIBinder::Bind(
      statement, NS_LITERAL_CSTRING("uri") + qIndex, aQuery->Uri()
    );
    NS_ENSURE_SUCCESS(rv, rv);
    if (aQuery->UriIsPrefix()) {
      nsCAutoString uriString;
      aQuery->Uri()->GetSpec(uriString);
      uriString.Append(char(0x7F)); // MAX_UTF8
      rv = URIBinder::Bind(
        statement, NS_LITERAL_CSTRING("uri_upper") + qIndex, uriString
      );
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // annotation
  if (!aQuery->Annotation().IsEmpty()) {
    rv = statement->BindUTF8StringByName(
      NS_LITERAL_CSTRING("anno") + qIndex, aQuery->Annotation()
    );
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // tags
  const nsTArray<nsString> &tags = aQuery->Tags();
  if (tags.Length() > 0) {
    for (PRUint32 i = 0; i < tags.Length(); ++i) {
      nsPrintfCString paramName("tag%d_", i);
      NS_ConvertUTF16toUTF8 tag(tags[i]);
      rv = statement->BindUTF8StringByName(paramName + qIndex, tag);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    PRInt64 tagsFolder = GetTagsFolder();
    rv = statement->BindInt64ByName(
      NS_LITERAL_CSTRING("tags_folder") + qIndex, tagsFolder
    );
    NS_ENSURE_SUCCESS(rv, rv);
    if (!aQuery->TagsAreNot()) {
      rv = statement->BindInt32ByName(
        NS_LITERAL_CSTRING("tag_count") + qIndex, tags.Length()
      );
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // transitions
  const nsTArray<PRUint32>& transitions = aQuery->Transitions();
  if (transitions.Length() > 0) {
    for (PRUint32 i = 0; i < transitions.Length(); ++i) {
      nsPrintfCString paramName("transition%d_", i);
      rv = statement->BindInt64ByName(paramName + qIndex, transitions[i]);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // parent parameter
  if (aOptions->ResultType() == nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS &&
      aQuery->Folders().Length() == 1) {
    rv = statement->BindInt64ByName(
      NS_LITERAL_CSTRING("parent") + qIndex, aQuery->Folders()[0]
    );
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
    nsRefPtr<nsNavHistoryResultNode> result;
    rv = RowToResult(row, aOptions, getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);
    aResults->AppendObject(result);
  }
  return NS_OK;
}

static PRInt64
GetAgeInDays(PRTime aNormalizedNow, PRTime aDate)
{
  PRTime dateMidnight = NormalizeTimeRelativeToday(aDate);
  // if the visit time is in the future
  // treat as "today" see bug #385867
  if (dateMidnight > aNormalizedNow)
    return 0;
  else
    return ((aNormalizedNow - dateMidnight) / USECS_PER_DAY);
}

const PRInt64 UNDEFINED_URN_VALUE = -1;

// Create a urn (like
// urn:places-persist:place:group=0&group=1&sort=1&type=1,,%28local%20files%29)
// to be used to persist the open state of this container in localstore.rdf
nsresult
CreatePlacesPersistURN(nsNavHistoryQueryResultNode *aResultNode, 
                      PRInt64 aValue, const nsCString& aTitle, nsCString& aURN)
{
  nsCAutoString uri;
  nsresult rv = aResultNode->GetUri(uri);
  NS_ENSURE_SUCCESS(rv, rv);

  aURN.Assign(NS_LITERAL_CSTRING("urn:places-persist:"));
  aURN.Append(uri);

  aURN.Append(NS_LITERAL_CSTRING(","));
  if (aValue != UNDEFINED_URN_VALUE)
    aURN.AppendInt(aValue);

  aURN.Append(NS_LITERAL_CSTRING(","));
  if (!aTitle.IsEmpty()) {
    nsCAutoString escapedTitle;
    PRBool success = NS_Escape(aTitle, escapedTitle, url_XAlphas);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
    aURN.Append(escapedTitle);
  }

  return NS_OK;
}

PRInt64
nsNavHistory::GetTagsFolder()
{
  // cache our tags folder
  // note, we can't do this in nsNavHistory::Init(), 
  // as getting the bookmarks service would initialize it.
  if (mTagsFolder == -1) {
    nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, -1);
    
    nsresult rv = bookmarks->GetTagsFolder(&mTagsFolder);
    NS_ENSURE_SUCCESS(rv, -1);
  }
  return mTagsFolder;
}

// nsNavHistory::FilterResultSet
//
// This does some post-query-execution filtering:
//   - searching on title & url
//   - parent folder (recursively)
//   - excludeQueries
//   - tags
//   - limit count
//   - excludingLivemarkItems
//
// Note:  changes to filtering in FilterResultSet() 
// may require changes to NeedToFilterResultSet()

nsresult
nsNavHistory::FilterResultSet(nsNavHistoryQueryResultNode* aQueryNode,
                              const nsCOMArray<nsNavHistoryResultNode>& aSet,
                              nsCOMArray<nsNavHistoryResultNode>* aFiltered,
                              const nsCOMArray<nsNavHistoryQuery>& aQueries,
                              nsNavHistoryQueryOptions *aOptions)
{
  nsresult rv;

  // get the bookmarks service
  nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

  // parse the search terms
  nsTArray<nsTArray<nsString>*> terms;
  ParseSearchTermsFromQueries(aQueries, &terms);

  // The includeFolders array for each query is initialized with its
  // query's folders array. We add sub-folders as we check items.
  nsTArray< nsTArray<PRInt64>* > includeFolders;
  nsTArray< nsTArray<PRInt64>* > excludeFolders;
  for (PRInt32 queryIndex = 0;
       queryIndex < aQueries.Count(); queryIndex++) {
    includeFolders.AppendElement(new nsTArray<PRInt64>(aQueries[queryIndex]->Folders()));
    excludeFolders.AppendElement(new nsTArray<PRInt64>());
  }

  // Filter against query options.
  // XXX Only excludeQueries and excludeItemIfParentHasAnnotation are supported
  // at the moment.
  PRBool excludeQueries = PR_FALSE;
  if (aQueryNode) {
    rv = aQueryNode->mOptions->GetExcludeQueries(&excludeQueries);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCString parentAnnotationToExclude;
  nsTArray<PRInt64> parentFoldersToExclude;
  if (aQueryNode) {
    rv = aQueryNode->mOptions->GetExcludeItemIfParentHasAnnotation(parentAnnotationToExclude);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!parentAnnotationToExclude.IsEmpty()) {
    // Find all the folders with the annotation we are excluding and save their
    // item ids.  When doing filtering, if item id of a result's parent
    // matches one of the saved item ids, the result will be excluded.
    mozStorageStatementScoper scope(mDBGetItemsWithAnno);

    rv = mDBGetItemsWithAnno->BindUTF8StringByName(
      NS_LITERAL_CSTRING("anno_name"), parentAnnotationToExclude
    );
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore = PR_FALSE;
    while (NS_SUCCEEDED(mDBGetItemsWithAnno->ExecuteStep(&hasMore)) && hasMore) {
      PRInt64 folderId = 0;
      rv = mDBGetItemsWithAnno->GetInt64(0, &folderId);
      NS_ENSURE_SUCCESS(rv, rv);
      parentFoldersToExclude.AppendElement(folderId);
    }
  }

  PRUint16 resultType = aOptions->ResultType();
  for (PRInt32 nodeIndex = 0; nodeIndex < aSet.Count(); nodeIndex++) {
    // exclude-queries is implicit when searching, we're only looking at
    // plan URI nodes
    if (!aSet[nodeIndex]->IsURI())
      continue;

    // RESULTS_AS_TAG_CONTENTS returns a set ordered by place_id and
    // lastModified. So, to remove duplicates, we can retain the first result
    // for each uri.
    if (resultType == nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS &&
        nodeIndex > 0 && aSet[nodeIndex]->mURI == aSet[nodeIndex-1]->mURI)
      continue;

    PRInt64 parentId = -1;
    if (aSet[nodeIndex]->mItemId != -1) {
      if (aQueryNode && aQueryNode->mItemId == aSet[nodeIndex]->mItemId)
        continue;
      parentId = aSet[nodeIndex]->mFolderId;
    }

    // if we are excluding items by parent annotation, 
    // exclude items who's parent is a folder with that annotation
    if (!parentAnnotationToExclude.IsEmpty() &&
        parentFoldersToExclude.Contains(parentId))
      continue;

    // Append the node only if it matches one of the queries.
    PRBool appendNode = PR_FALSE;
    for (PRInt32 queryIndex = 0;
         queryIndex < aQueries.Count() && !appendNode; queryIndex++) {

      if (terms[queryIndex]->Length()) {
        // Filter based on search terms.
        // Convert title and url for the current node to UTF16 strings.
        NS_ConvertUTF8toUTF16 nodeTitle(aSet[nodeIndex]->mTitle);
        // Unescape the URL for search terms matching.
        nsCAutoString cNodeURL(aSet[nodeIndex]->mURI);
        NS_ConvertUTF8toUTF16 nodeURL(NS_UnescapeURL(cNodeURL));

        // Determine if every search term matches anywhere in the title, url or
        // tag.
        PRBool matchAll = PR_TRUE;
        for (PRInt32 termIndex = terms[queryIndex]->Length() - 1;
             termIndex >= 0 && matchAll;
             termIndex--) {
          nsString& term = terms[queryIndex]->ElementAt(termIndex);

          // True if any of them match; false makes us quit the loop
          matchAll = CaseInsensitiveFindInReadable(term, nodeTitle) ||
                     CaseInsensitiveFindInReadable(term, nodeURL) ||
                     CaseInsensitiveFindInReadable(term, aSet[nodeIndex]->mTags);
        }

        // Skip the node if we don't match all terms in the title, url or tag
        if (!matchAll)
          continue;
      }

      // Filter bookmarks on parent folder.
      // RESULTS_AS_TAG_CONTENTS changes bookmarks' parents, so we cannot filter
      // this kind of result based on the parent.
      if (includeFolders[queryIndex]->Length() != 0 &&
          resultType != nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS) {
        // Filter out the node if its parent is in the excludeFolders
        // cache.
        if (excludeFolders[queryIndex]->Contains(parentId))
          continue;

        if (!includeFolders[queryIndex]->Contains(parentId)) {
          // If parent is not found in current includeFolders cache, we check
          // its ancestors.
          PRInt64 ancestor = parentId;
          PRBool belongs = PR_FALSE;
          nsTArray<PRInt64> ancestorFolders;

          while (!belongs) {
            // Avoid using |ancestor| itself if GetFolderIdForItem failed.
            ancestorFolders.AppendElement(ancestor);

            // GetFolderIdForItems throws when called for the places-root
            if (NS_FAILED(bookmarks->GetFolderIdForItem(ancestor, &ancestor))) {
              break;
            } else if (excludeFolders[queryIndex]->Contains(ancestor)) {
              break;
            } else if (includeFolders[queryIndex]->Contains(ancestor)) {
              belongs = PR_TRUE;
            }
          }
          // if the parentId or any of its ancestors "belong",
          // include all of them.  otherwise, exclude all of them.
          if (belongs) {
            includeFolders[queryIndex]->AppendElements(ancestorFolders);
          } else {
            excludeFolders[queryIndex]->AppendElements(ancestorFolders);
            continue;
          }
        }
      }

      // We passed all filters, so we can append the node to filtered results.
      appendNode = PR_TRUE;
    }

    if (appendNode)
      aFiltered->AppendObject(aSet[nodeIndex]);
      
    // Stop once we have reached max results.
    if (aOptions->MaxResults() > 0 &&
        (PRUint32)aFiltered->Count() >= aOptions->MaxResults())
      break;
  }

  // De-allocate the temporary matrixes.
  for (PRInt32 i = 0; i < aQueries.Count(); i++) {
    delete terms[i];
    delete includeFolders[i];
    delete excludeFolders[i];
  }

  return NS_OK;
}


// nsNavHistory::CheckIsRecentEvent
//
//    Sees if this URL happened "recently."
//
//    It is always removed from our recent list no matter what. It only counts
//    as "recent" if the event happened more recently than our event
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

static PLDHashOperator
ExpireNonrecentEventsCallback(nsCStringHashKey::KeyType aKey,
                              PRInt64& aData,
                              void* userArg)
{
  PRInt64* threshold = reinterpret_cast<PRInt64*>(userArg);
  if (aData < *threshold)
    return PL_DHASH_REMOVE;
  return PL_DHASH_NEXT;
}
void
nsNavHistory::ExpireNonrecentEvents(RecentEventHash* hashTable)
{
  PRInt64 threshold = GetNow() - RECENT_EVENT_THRESHOLD;
  hashTable->Enumerate(ExpireNonrecentEventsCallback,
                       reinterpret_cast<void*>(&threshold));
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
                             nsACString& aSource,
                             PRTime* aTime,
                             PRUint32* aRedirectType)
{
  RedirectInfo info;
  if (mRecentRedirects.Get(aDestination, &info)) {
    // Consume the redirect entry, it's no longer useful.
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
  NS_ASSERTION(aRow && aOptions && aResult, "Null pointer in RowToResult");
  *aResult = nsnull;

  // URL
  nsCAutoString url;
  nsresult rv = aRow->GetUTF8String(kGetInfoIndex_URL, url);
  NS_ENSURE_SUCCESS(rv, rv);

  // title
  nsCAutoString title;
  rv = aRow->GetUTF8String(kGetInfoIndex_Title, title);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 accessCount = aRow->AsInt32(kGetInfoIndex_VisitCount);
  PRTime time = aRow->AsInt64(kGetInfoIndex_VisitDate);

  // favicon
  nsCAutoString favicon;
  rv = aRow->GetUTF8String(kGetInfoIndex_FaviconURL, favicon);
  NS_ENSURE_SUCCESS(rv, rv);

  // itemId
  PRInt64 itemId = aRow->AsInt64(kGetInfoIndex_ItemId);
  PRInt64 parentId = -1;
  if (itemId == 0) {
    // This is not a bookmark.  For non-bookmarks we use a -1 itemId value.
    // Notice ids in sqlite tables start from 1, so itemId cannot ever be 0.
    itemId = -1;
  }
  else {
    // This is a bookmark, so it has a parent.
    PRInt64 itemParentId = aRow->AsInt64(kGetInfoIndex_ItemParentId);
    if (itemParentId > 0) {
      // The Places root has parent == 0, but that item id does not really
      // exist. We want to set the parent only if it's a real one.
      parentId = itemParentId;
    }
  }

  if (IsQueryURI(url)) {
    // special case "place:" URIs: turn them into containers
      
    // We should never expose the history title for query nodes if the
    // bookmark-item's title is set to null (the history title may be the
    // query string without the place: prefix). Thus we call getItemTitle
    // explicitly. Doing this in the SQL query would be less performant since
    // it should be done for all results rather than only for queries.
    if (itemId != -1) {
      nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
      NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

      rv = bookmarks->GetItemTitle(itemId, title);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = QueryRowToResult(itemId, url, title, accessCount, time, favicon, aResult);
    NS_ENSURE_STATE(*aResult);
    if (aOptions->ResultType() == nsNavHistoryQueryOptions::RESULTS_AS_TAG_QUERY) {
      // RESULTS_AS_TAG_QUERY has date columns
      (*aResult)->mDateAdded = aRow->AsInt64(kGetInfoIndex_ItemDateAdded);
      (*aResult)->mLastModified = aRow->AsInt64(kGetInfoIndex_ItemLastModified);
    }
    else if ((*aResult)->IsFolder()) {
      // If it's a simple folder node (i.e. a shortcut to another folder), apply
      // our options for it. However, if the parent type was tag query, we do not
      // apply them, because it would not yield any results.
      (*aResult)->GetAsContainer()->mOptions = aOptions;
    }

    return rv;
  } else if (aOptions->ResultType() == nsNavHistoryQueryOptions::RESULTS_AS_URI ||
             aOptions->ResultType() == nsNavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS) {
    *aResult = new nsNavHistoryResultNode(url, title, accessCount, time,
                                          favicon);
    if (!*aResult)
      return NS_ERROR_OUT_OF_MEMORY;

    if (itemId != -1) {
      (*aResult)->mItemId = itemId;
      (*aResult)->mFolderId = parentId;
      (*aResult)->mDateAdded = aRow->AsInt64(kGetInfoIndex_ItemDateAdded);
      (*aResult)->mLastModified = aRow->AsInt64(kGetInfoIndex_ItemLastModified);
    }

    nsAutoString tags;
    rv = aRow->GetString(kGetInfoIndex_ItemTags, tags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!tags.IsVoid())
      (*aResult)->mTags.Assign(tags);

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

    nsAutoString tags;
    rv = aRow->GetString(kGetInfoIndex_ItemTags, tags);
    if (!tags.IsVoid())
      (*aResult)->mTags.Assign(tags);

    NS_ADDREF(*aResult);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


// nsNavHistory::QueryRowToResult
//
//    Called by RowToResult when the URI is a place: URI to generate the proper
//    folder or query node.

nsresult
nsNavHistory::QueryRowToResult(PRInt64 itemId, const nsACString& aURI,
                               const nsACString& aTitle,
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
    PRInt64 folderId = GetSimpleBookmarksQueryFolder(queries, options);
    if (folderId) {
      // simple bookmarks folder, magically generate a bookmarks folder node
      nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
      NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

      // this addrefs for us
      rv = bookmarks->ResultNodeForContainer(folderId, options, aNode);
      NS_ENSURE_SUCCESS(rv, rv);

      // this is the query item-Id, and is what is exposed by node.itemId
      (*aNode)->GetAsFolder()->mQueryItemId = itemId;

      // Use the query item title, unless it's void (in that case,
      // we keep the concrete folder title set)
      if (!aTitle.IsVoid())
        (*aNode)->mTitle = aTitle;
    } else {
      // regular query
      *aNode = new nsNavHistoryQueryResultNode(aTitle, EmptyCString(), aTime,
                                               queries, options);
      if (! *aNode)
        return NS_ERROR_OUT_OF_MEMORY;
      (*aNode)->mItemId = itemId;
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

  switch (aOptions->ResultType())
  {
    case nsNavHistoryQueryOptions::RESULTS_AS_VISIT:
    case nsNavHistoryQueryOptions::RESULTS_AS_FULL_VISIT:
      // visit query - want exact visit time
      statement = GetDBVisitToVisitResult();
      break;

    case nsNavHistoryQueryOptions::RESULTS_AS_URI:
      // URL results - want last visit time
      statement = GetDBVisitToURLResult();
      break;

    default:
      // Query base types like RESULTS_AS_*_QUERY handle additions
      // by registering their own observers when they are expanded.
      return NS_OK;
  }
  NS_ENSURE_STATE(statement);

  mozStorageStatementScoper scoper(statement);
  nsresult rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("visit_id"),
                                           visitId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = statement->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! hasMore) {
    NS_NOTREACHED("Trying to get a result node for an invalid visit");
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(statement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return RowToResult(row, aOptions, aResult);
}

nsresult
nsNavHistory::BookmarkIdToResultNode(PRInt64 aBookmarkId, nsNavHistoryQueryOptions* aOptions,
                                     nsNavHistoryResultNode** aResult)
{
  mozIStorageStatement *stmt = GetDBBookmarkToUrlResult();
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"),
                                      aBookmarkId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = stmt->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasMore) {
    NS_NOTREACHED("Trying to get a result node for an invalid bookmark identifier");
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(stmt, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return RowToResult(row, aOptions, aResult);
}

void
nsNavHistory::SendPageChangedNotification(nsIURI* aURI, PRUint32 aWhat,
                                          const nsAString& aValue)
{
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver, OnPageChanged(aURI, aWhat, aValue));
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
nsNavHistory::GetAgeInDaysString(PRInt32 aInt, const PRUnichar *aName,
                                 nsACString& aResult)
{
  nsIStringBundle *bundle = GetBundle();
  if (!bundle)
    aResult.Truncate(0);
  else {
    nsAutoString intString;
    intString.AppendInt(aInt);
    const PRUnichar* strings[1] = { intString.get() };
    nsXPIDLString value;
    nsresult rv = bundle->FormatStringFromName(aName, strings,
                                               1, getter_Copies(value));
    if (NS_SUCCEEDED(rv))
      CopyUTF16toUTF8(value, aResult);
    else
      aResult.Truncate(0);
  }
}

void
nsNavHistory::GetStringFromName(const PRUnichar *aName, nsACString& aResult)
{
  nsIStringBundle *bundle = GetBundle();
  if (!bundle)
    aResult.Truncate(0);

  nsXPIDLString value;
  nsresult rv = bundle->GetStringFromName(aName, getter_Copies(value));
  if (NS_SUCCEEDED(rv))
    CopyUTF16toUTF8(value, aResult);
  else
    aResult.Truncate(0);
}

void
nsNavHistory::GetMonthName(PRInt32 aIndex, nsACString& aResult)
{
  nsIStringBundle *bundle = GetDateFormatBundle();
  if (!bundle)
    aResult.Truncate(0);
  else {
    nsCString name = nsPrintfCString("month.%d.name", aIndex);
    nsXPIDLString value;
    nsresult rv = bundle->GetStringFromName(NS_ConvertUTF8toUTF16(name).get(),
                                            getter_Copies(value));
    if (NS_SUCCEEDED(rv))
      CopyUTF16toUTF8(value, aResult);
    else
      aResult.Truncate(0);
  }
}

// nsNavHistory::SetPageTitleInternal
//
//    Called to set the title for the given URI. Used as a
//    backend for SetTitle.
//
//    Will fail for pages that are not in the DB. To clear the corresponding
//    title, use aTitle.SetIsVoid(). Sending an empty string will save an
//    empty string instead of clearing it.

nsresult
nsNavHistory::SetPageTitleInternal(nsIURI* aURI, const nsAString& aTitle)
{
  nsresult rv;

  // first, make sure the page exists, and fetch the old title (we need the one
  // that isn't changing to send notifications)
  nsAutoString title;
  { // scope for statement
    mozStorageStatementScoper infoScoper(mDBGetURLPageInfo);
    rv = URIBinder::Bind(mDBGetURLPageInfo, NS_LITERAL_CSTRING("page_url"), aURI);
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
  }

  // It is actually common to set the title to be the same thing it used to
  // be. For example, going to any web page will always cause a title to be set,
  // even though it will often be unchanged since the last visit. In these
  // cases, we can avoid DB writing and (most significantly) observer overhead.
  if ((aTitle.IsVoid() && title.IsVoid()) || aTitle == title)
    return NS_OK;

  mozStorageStatementScoper scoper(mDBSetPlaceTitle);
  // title
  if (aTitle.IsVoid())
    rv = mDBSetPlaceTitle->BindNullByName(NS_LITERAL_CSTRING("page_title"));
  else {
    rv = mDBSetPlaceTitle->BindStringByName(
      NS_LITERAL_CSTRING("page_title"), StringHead(aTitle, TITLE_LENGTH_MAX)
    );
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // url
  rv = URIBinder::Bind(mDBSetPlaceTitle, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBSetPlaceTitle->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver, OnTitleChanged(aURI, aTitle));

  return NS_OK;
}

nsresult
nsNavHistory::AddPageWithVisits(nsIURI *aURI,
                                const nsString &aTitle,
                                PRInt32 aVisitCount,
                                PRInt32 aTransitionType,
                                PRTime aFirstVisitDate,
                                PRTime aLastVisitDate)
{
  PRBool canAdd = PR_FALSE;
  nsresult rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  // see if this is an update (revisit) or a new page
  mozStorageStatementScoper scoper(mDBGetPageVisitStats);
  rv = URIBinder::Bind(mDBGetPageVisitStats, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool alreadyVisited = PR_FALSE;
  rv = mDBGetPageVisitStats->ExecuteStep(&alreadyVisited);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 placeId = 0;
  PRInt32 typed = 0;
  PRInt32 hidden = 0;

  if (alreadyVisited) {
    // Update the existing entry
    rv = mDBGetPageVisitStats->GetInt64(0, &placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    // We don't mind visit_count
    rv = mDBGetPageVisitStats->GetInt32(2, &typed);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBGetPageVisitStats->GetInt32(3, &hidden);
    NS_ENSURE_SUCCESS(rv, rv);

    if (typed == 0 && aTransitionType == TRANSITION_TYPED) {
      typed = 1;
      // Update with new stats
      mozStorageStatementScoper updateScoper(mDBUpdatePageVisitStats);
      rv = mDBUpdatePageVisitStats->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), placeId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mDBUpdatePageVisitStats->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), hidden);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mDBUpdatePageVisitStats->BindInt32ByName(NS_LITERAL_CSTRING("typed"), typed);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mDBUpdatePageVisitStats->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else {
    // Insert the new place entry
    rv = InternalAddNewPage(aURI, aTitle, hidden == 1,
                            aTransitionType == TRANSITION_TYPED, 0,
                            PR_FALSE, &placeId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ASSERTION(placeId != 0, "Cannot add a visit to a nonexistent page");

  if (aFirstVisitDate != -1) {
    // Add the first visit
    PRInt64 visitId;
    rv = InternalAddVisit(placeId, 0, 0,
                          aFirstVisitDate, aTransitionType, &visitId);
    aVisitCount--;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aLastVisitDate != -1) {
   // Add remaining visits starting from the last one
   for (PRInt64 i = 0; i < aVisitCount; i++) {
      PRInt64 visitId;
      rv = InternalAddVisit(placeId, 0, 0,
                            aLastVisitDate - i, aTransitionType, &visitId);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
nsNavHistory::RemoveDuplicateURIs()
{
  // this must be in a transaction because we do related queries
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // this query chooses an id for every duplicate uris
  // this id will be retained while duplicates will be discarded
  // total_visit_count is the sum of all duplicate uris visit_count
  nsCOMPtr<mozIStorageStatement> selectStatement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT "
        "(SELECT h.id FROM moz_places h WHERE h.url = url "
         "ORDER BY h.visit_count DESC LIMIT 1), "
        "url, SUM(visit_count) "
      "FROM moz_places "
      "GROUP BY url HAVING( COUNT(url) > 1)"),
    getter_AddRefs(selectStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // this query remaps history visits to the retained place_id
  nsCOMPtr<mozIStorageStatement> updateStatement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_historyvisits "
      "SET place_id = ?1 "
      "WHERE place_id IN "
        "(SELECT id FROM moz_places WHERE id <> ?1 AND url = ?2)"),
    getter_AddRefs(updateStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // this query remaps bookmarks to the retained place_id
  nsCOMPtr<mozIStorageStatement> bookmarkStatement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_bookmarks "
      "SET fk = ?1 "
      "WHERE fk IN "
        "(SELECT id FROM moz_places WHERE id <> ?1 AND url = ?2)"),
    getter_AddRefs(bookmarkStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // this query remaps annotations to the retained place_id
  nsCOMPtr<mozIStorageStatement> annoStatement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_annos "
      "SET place_id = ?1 "
      "WHERE place_id IN "
        "(SELECT id FROM moz_places WHERE id <> ?1 AND url = ?2)"),
    getter_AddRefs(annoStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // this query deletes all duplicate uris except the chosen id
  nsCOMPtr<mozIStorageStatement> deleteStatement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_places WHERE url = ?1 AND id <> ?2"),
    getter_AddRefs(deleteStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // this query updates visit_count to the sum of all visits
  nsCOMPtr<mozIStorageStatement> countStatement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places SET visit_count = ?1 WHERE id = ?2"),
    getter_AddRefs(countStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // for each duplicate uri we update historyvisit and visit_count
  PRBool hasMore;
  while (NS_SUCCEEDED(selectStatement->ExecuteStep(&hasMore)) && hasMore) {
    PRUint64 id = selectStatement->AsInt64(0);
    nsCAutoString url;
    rv = selectStatement->GetUTF8String(1, url);
    NS_ENSURE_SUCCESS(rv, rv);
    PRUint64 visit_count = selectStatement->AsInt64(2);

    // update historyvisits so they are remapped to the retained uri
    rv = updateStatement->BindInt64ByIndex(0, id);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = URIBinder::Bind(updateStatement, 1, url);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = updateStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    // remap bookmarks to the retained id
    rv = bookmarkStatement->BindInt64ByIndex(0, id);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = URIBinder::Bind(bookmarkStatement, 1, url);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = bookmarkStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    // remap annotations to the retained id
    rv = annoStatement->BindInt64ByIndex(0, id);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = URIBinder::Bind(annoStatement, 1, url);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = annoStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
    
    // remove duplicate uris from moz_places
    rv = URIBinder::Bind(deleteStatement, 0, url);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = deleteStatement->BindInt64ByIndex(1, id);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = deleteStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    // update visit_count to the sum of all visit_count
    rv = countStatement->BindInt64ByIndex(0, visit_count);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = countStatement->BindInt64ByIndex(1, id);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = countStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


namespace {

// GetSimpleBookmarksQueryFolder
//
//    Determines if this set of queries is a simple bookmarks query for a
//    folder with no other constraints. In these common cases, we can more
//    efficiently compute the results.
//
//    A simple bookmarks query will result in a hierarchical tree of
//    bookmark items, folders and separators.
//
//    Returns the folder ID if it is a simple folder query, 0 if not.
static PRInt64
GetSimpleBookmarksQueryFolder(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                              nsNavHistoryQueryOptions* aOptions)
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
  (void)query->GetHasSearchTerms(&hasIt);
  if (hasIt)
    return 0;
  if (query->Tags().Length() > 0)
    return 0;
  if (aOptions->MaxResults() > 0)
    return 0;

  // RESULTS_AS_TAG_CONTENTS is quite similar to a folder shortcut, but it must
  // not be treated like that, since it needs all query options.
  if(aOptions->ResultType() == nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS)
    return 0;

  // Don't care about onlyBookmarked flag, since specifying a bookmark
  // folder is inferring onlyBookmarked.
  NS_ASSERTION(query->Folders()[0] > 0, "bad folder id");
  return query->Folders()[0];
}


// ParseSearchTermsFromQueries
//
//    Construct a matrix of search terms from the given queries array.
//    All of the query objects are ORed together. Within a query, all the terms
//    are ANDed together. See nsINavHistoryService.idl.
//
//    This just breaks the query up into words. We don't do anything fancy,
//    not even quoting. We do, however, strip quotes, because people might
//    try to input quotes expecting them to do something and get no results
//    back.

inline PRBool isQueryWhitespace(PRUnichar ch)
{
  return ch == ' ';
}

void ParseSearchTermsFromQueries(const nsCOMArray<nsNavHistoryQuery>& aQueries,
                                 nsTArray<nsTArray<nsString>*>* aTerms)
{
  PRInt32 lastBegin = -1;
  for (PRInt32 i = 0; i < aQueries.Count(); i++) {
    nsTArray<nsString> *queryTerms = new nsTArray<nsString>();
    PRBool hasSearchTerms;
    if (NS_SUCCEEDED(aQueries[i]->GetHasSearchTerms(&hasSearchTerms)) &&
        hasSearchTerms) {
      const nsString& searchTerms = aQueries[i]->SearchTerms();
      for (PRUint32 j = 0; j < searchTerms.Length(); j++) {
        if (isQueryWhitespace(searchTerms[j]) ||
            searchTerms[j] == '"') {
          if (lastBegin >= 0) {
            // found the end of a word
            queryTerms->AppendElement(Substring(searchTerms, lastBegin,
                                               j - lastBegin));
            lastBegin = -1;
          }
        } else {
          if (lastBegin < 0) {
            // found the beginning of a word
            lastBegin = j;
          }
        }
      }
      // last word
      if (lastBegin >= 0)
        queryTerms->AppendElement(Substring(searchTerms, lastBegin));
    }
    aTerms->AppendElement(queryTerms);
  }
}

} // anonymous namespace


nsresult
nsNavHistory::UpdateFrecency(PRInt64 aPlaceId, PRBool aIsBookmarked)
{
  mozStorageStatementScoper statsScoper(mDBGetPlaceVisitStats);
  nsresult rv = mDBGetPlaceVisitStats->BindInt64ByName(
    NS_LITERAL_CSTRING("page_id"), aPlaceId
  );
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResults = PR_FALSE;
  rv = mDBGetPlaceVisitStats->ExecuteStep(&hasResults);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasResults) {
    NS_WARNING("attempting to update frecency for a bogus place");
    // before I added the check for itemType == TYPE_BOOKMARK
    // I hit this with aPlaceId of 0 (on import)
    return NS_OK;
  }

  PRInt32 typed = 0;
  rv = mDBGetPlaceVisitStats->GetInt32(0, &typed);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 hidden = 0;
  rv = mDBGetPlaceVisitStats->GetInt32(1, &hidden);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 oldFrecency = 0;
  rv = mDBGetPlaceVisitStats->GetInt32(2, &oldFrecency);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = UpdateFrecencyInternal(aPlaceId, typed, hidden, oldFrecency,
                              aIsBookmarked);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::UpdateFrecencyInternal(PRInt64 aPlaceId, PRInt32 aTyped,
  PRInt32 aHidden, PRInt32 aOldFrecency, PRBool aIsBookmarked)
{
  PRInt32 visitCountForFrecency = 0;
  // Since visit_count excludes visit with visit_type NOT IN(0,4,7,8), it can't
  // be uses for calculating frecency.  Instead it must must be recalculated.
  nsresult rv = CalculateFullVisitCount(aPlaceId, &visitCountForFrecency);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 newFrecency = 0;
  rv = CalculateFrecencyInternal(aPlaceId, aTyped, visitCountForFrecency,
                                 aIsBookmarked, &newFrecency);
  NS_ENSURE_SUCCESS(rv, rv);

  // save ourselves the UPDATE if the frecency hasn't changed
  // One way this can happen is with livemarks.
  // when we added the livemark, the frecency was 0.  
  // On refresh, when we remove and then add the livemark items,
  // the frecency (for a given moz_places) will not have changed
  // (if we've never visited that place).
  // Additionally, don't bother overwriting a valid frecency with an invalid one
  if ((newFrecency == aOldFrecency) || (aOldFrecency && newFrecency < 0))
    return NS_OK;

  mozStorageStatementScoper updateScoper(mDBUpdateFrecencyAndHidden);
  rv = mDBUpdateFrecencyAndHidden->BindInt64ByName(
    NS_LITERAL_CSTRING("page_id"), aPlaceId
  );
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBUpdateFrecencyAndHidden->BindInt32ByName(
    NS_LITERAL_CSTRING("frecency"), newFrecency
  );
  NS_ENSURE_SUCCESS(rv, rv);

  // if we calculated a non-zero frecency we should unhide this place
  // so that previously hidden (non-livebookmark item) bookmarks 
  // will now appear in autocomplete
  // if we calculated a zero frecency, we re-use the old hidden value.
  rv = mDBUpdateFrecencyAndHidden->BindInt32ByName(
    NS_LITERAL_CSTRING("hidden"),  newFrecency ? 0 /* not hidden */ : aHidden
  );
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBUpdateFrecencyAndHidden->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::CalculateFrecencyInternal(PRInt64 aPlaceId,
                                        PRInt32 aTyped,
                                        PRInt32 aVisitCount,
                                        PRBool aIsBookmarked,
                                        PRInt32 *aFrecency)
{
  PRTime normalizedNow = NormalizeTimeRelativeToday(GetNow());

  float pointsForSampledVisits = 0.0;

  if (aPlaceId != -1) {
    PRInt32 numSampledVisits = 0;

    mozStorageStatementScoper scoper(mDBVisitsForFrecency);
    nsresult rv = mDBVisitsForFrecency->BindInt64ByName(
      NS_LITERAL_CSTRING("page_id"), aPlaceId
    );
    NS_ENSURE_SUCCESS(rv, rv);

    // mDBVisitsForFrecency is limited by the browser.frecency.numVisits pref
    PRBool hasMore = PR_FALSE;
    while (NS_SUCCEEDED(mDBVisitsForFrecency->ExecuteStep(&hasMore)) 
           && hasMore) {
      numSampledVisits++;

      PRInt32 visitType = mDBVisitsForFrecency->AsInt32(1);

      PRInt32 bonus = 0;

      switch (visitType) {
        case nsINavHistoryService::TRANSITION_EMBED:
          bonus = mEmbedVisitBonus;
          break;
        case nsINavHistoryService::TRANSITION_FRAMED_LINK:
          bonus = mFramedLinkVisitBonus;
          break;
        case nsINavHistoryService::TRANSITION_LINK:
          bonus = mLinkVisitBonus;
          break;
        case nsINavHistoryService::TRANSITION_TYPED:
          bonus = mTypedVisitBonus;
          break;
        case nsINavHistoryService::TRANSITION_BOOKMARK:
          bonus = mBookmarkVisitBonus;
          break;
        case nsINavHistoryService::TRANSITION_DOWNLOAD:
          bonus = mDownloadVisitBonus;
          break;
        case nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT:
          bonus = mPermRedirectVisitBonus;
          break;
        case nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY:
          bonus = mTempRedirectVisitBonus;
          break;
        default:
          // 0 == undefined (see bug #375777 for details)
          NS_WARN_IF_FALSE(!visitType, "new transition but no weight for frecency");
          bonus = mDefaultVisitBonus;
          break;
      }

      // Always add the bookmark visit bonus.
      if (aIsBookmarked)
        bonus += mBookmarkVisitBonus;

#ifdef DEBUG_FRECENCY
      printf("CalculateFrecency() for place %lld has a bonus of %d\n", aPlaceId, bonus);
#endif

      // if bonus was zero, we can skip the work to determine the weight
      if (bonus) {
        PRTime visitDate = mDBVisitsForFrecency->AsInt64(0);
        PRInt64 ageInDays = GetAgeInDays(normalizedNow, visitDate);

        PRInt32 weight = 0;

        if (ageInDays <= mFirstBucketCutoffInDays)
          weight = mFirstBucketWeight;
        else if (ageInDays <= mSecondBucketCutoffInDays)
          weight = mSecondBucketWeight;
        else if (ageInDays <= mThirdBucketCutoffInDays)
          weight = mThirdBucketWeight;
        else if (ageInDays <= mFourthBucketCutoffInDays) 
          weight = mFourthBucketWeight;
        else
          weight = mDefaultWeight;

        pointsForSampledVisits += (float)(weight * (bonus / 100.0));
      }
    }

    if (numSampledVisits) {
      // fix for bug #412219
      if (!pointsForSampledVisits) {
        // For URIs with zero points in the sampled recent visits
        // but "browsing" type visits outside the sampling range, set
        // frecency to -visit_count, so they're still shown in autocomplete.
        PRInt32 visitCount = 0;
        mozStorageStatementScoper scoper(mDBGetIdPageInfo);
        rv = mDBGetIdPageInfo->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlaceId);
        NS_ENSURE_SUCCESS(rv, rv);

        PRBool hasVisits = PR_TRUE;
        if (NS_SUCCEEDED(mDBGetIdPageInfo->ExecuteStep(&hasVisits)) && hasVisits) {
          rv = mDBGetIdPageInfo->GetInt32(nsNavHistory::kGetInfoIndex_VisitCount,
                                          &visitCount);
          NS_ENSURE_SUCCESS(rv, rv);
        }
        // If we don't have visits set to 0
        *aFrecency = -visitCount;
      }
      else {
        // Estimate frecency using the last few visits.
        // Use NS_ceilf() so that we don't round down to 0, which
        // would cause us to completely ignore the place during autocomplete.
        *aFrecency = (PRInt32) NS_ceilf(aVisitCount * NS_ceilf(pointsForSampledVisits) / numSampledVisits);
      }

#ifdef DEBUG_FRECENCY
      printf("CalculateFrecency() for place %lld: %d = %d * %f / %d\n", aPlaceId, *aFrecency, aVisitCount, pointsForSampledVisits, numSampledVisits);
#endif

      return NS_OK;
    }
  }
 
  // XXX the code below works well for guessing the frecency on import, and we'll correct later once we have
  // visits.
  // what if we don't have visits and we never visit?  we could end up with a really high value
  // that keeps coming up in ac results?  only do this on import?  something to figure out.
  PRInt32 bonus = 0;

  // not the same logic above, as a single visit could not be both
  // a bookmark visit and a typed visit.  but when estimating a frecency
  // for a place that doesn't have any visits, this will make it so
  // something bookmarked and typed will have a higher frecency than
  // something just typed or just bookmarked.
  if (aIsBookmarked)
    bonus += mUnvisitedBookmarkBonus;
  if (aTyped)
    bonus += mUnvisitedTypedBonus;

  // assume "now" as our ageInDays, so use the first bucket.
  pointsForSampledVisits = mFirstBucketWeight * (bonus / (float)100.0); 
   
  // for a unvisited bookmark, produce a non-zero frecency
  // so that unvisited bookmarks show up in URL bar autocomplete
  if (!aVisitCount && aIsBookmarked)
    aVisitCount = 1;

  // use NS_ceilf() so that we don't round down to 0, which
  // would cause us to completely ignore the place during autocomplete
  *aFrecency = (PRInt32) NS_ceilf(aVisitCount * NS_ceilf(pointsForSampledVisits));
#ifdef DEBUG_FRECENCY
  printf("CalculateFrecency() for unvisited: frecency %d = %f points (b: %d, t: %d) * visit count %d\n", *aFrecency, pointsForSampledVisits, aIsBookmarked, aTyped, aVisitCount);
#endif
  return NS_OK;
}

nsresult
nsNavHistory::CalculateFrecency(PRInt64 aPlaceId,
                                PRInt32 aTyped,
                                PRInt32 aVisitCount,
                                nsCAutoString &aURL,
                                PRInt32 *aFrecency)
{
  *aFrecency = 0;

  PRBool isBookmark = PR_FALSE;

  // determine if the place is a (non-livemark item) bookmark and prevent
  // place: queries from showing up in the URL bar autocomplete results
  if (!IsQueryURI(aURL) && aPlaceId != -1) {
    nsNavBookmarks *bs = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bs, NS_ERROR_OUT_OF_MEMORY);
    isBookmark = bs->IsRealBookmark(aPlaceId);
  }

  nsresult rv = CalculateFrecencyInternal(aPlaceId, aTyped, aVisitCount,
                                          isBookmark, aFrecency);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
nsNavHistory::FixInvalidFrecencies()
{
  mozStorageTransaction transaction(mDBConn, PR_TRUE);

  // Find all places with invalid frecencies (frecency < 0) that occur when:
  // 1) we've done "clear private data"
  // 2) we've expired or deleted visits
  // 3) we've migrated from an older version, before global frecency
  //
  // From older versions, unmigrated bookmarks might be hidden, so we can't
  // exclude hidden places (by doing "WHERE hidden <> 1") from our query, as we
  // want to calculate the frecency for those places and unhide them (if they
  // are not livemark items and not place: queries.)
  //
  // Note, we are not limiting ourselves to places with visits because we may
  // not have any if the place is a bookmark and we expired or deleted all the
  // visits.
  nsCOMPtr<mozIStorageStatement> invalidFrecencies;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id, typed, hidden, frecency, url "
      "FROM moz_places_view "
      "WHERE frecency < 0"),
    getter_AddRefs(invalidFrecencies));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(invalidFrecencies->ExecuteStep(&hasMore)) && hasMore) {
    PRInt64 placeId = invalidFrecencies->AsInt64(0);
    PRInt32 typed = invalidFrecencies->AsInt32(1);
    PRInt32 hidden = invalidFrecencies->AsInt32(2);
    PRInt32 oldFrecency = invalidFrecencies->AsInt32(3);
    nsCAutoString url;
    invalidFrecencies->GetUTF8String(4, url);

    PRBool isBook = PR_FALSE;
    if (!IsQueryURI(url)) {
      nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
      NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
      isBook = bookmarks->IsRealBookmark(placeId);
    }

    rv = UpdateFrecencyInternal(placeId, typed, hidden, oldFrecency, isBook);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


#ifdef MOZ_XUL

namespace {

// Used to notify a topic to system observers on async execute completion.
class AutoCompleteStatementCallbackNotifier : public AsyncStatementCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_ASYNCSTATEMENTCALLBACK
};

NS_IMPL_ISUPPORTS1(AutoCompleteStatementCallbackNotifier,
                   mozIStorageStatementCallback)

NS_IMETHODIMP
AutoCompleteStatementCallbackNotifier::HandleCompletion(PRUint16 aReason)
{
  if (aReason != mozIStorageStatementCallback::REASON_FINISHED)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIObserverService> observerService =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (observerService) {
    (void)observerService->NotifyObservers(nsnull,
                                           TOPIC_AUTOCOMPLETE_FEEDBACK_UPDATED,
                                           nsnull);
  }

  return NS_OK;
}

NS_IMETHODIMP
AutoCompleteStatementCallbackNotifier::HandleResult(mozIStorageResultSet *aResultSet)
{
  NS_ASSERTION(PR_FALSE, "You cannot use AutoCompleteStatementCallbackNotifier to get async statements resultset");
  return NS_OK;
}

} // anonymous namespace

nsresult
nsNavHistory::AutoCompleteFeedback(PRInt32 aIndex,
                                   nsIAutoCompleteController *aController)
{
  // We do not track user choices in the location bar in private browsing mode.
  if (InPrivateBrowsingMode())
    return NS_OK;

  mozIStorageStatement *stmt = GetDBFeedbackIncrease();
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scope(stmt);

  nsAutoString input;
  nsresult rv = aController->GetSearchString(input);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("input_text"), input);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString url;
  rv = aController->GetValueAt(aIndex, url);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"),
                       NS_ConvertUTF16toUTF8(url));
  NS_ENSURE_SUCCESS(rv, rv);

  // We do the update asynchronously and we do not care about failures.
  nsCOMPtr<AutoCompleteStatementCallbackNotifier> callback =
    new AutoCompleteStatementCallbackNotifier();
  nsCOMPtr<mozIStoragePendingStatement> canceler;
  rv = stmt->ExecuteAsync(callback, getter_AddRefs(canceler));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

mozIStorageStatement *
nsNavHistory::GetDBFeedbackIncrease()
{
  if (mDBFeedbackIncrease)
    return mDBFeedbackIncrease;

  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    // Leverage the PRIMARY KEY (place_id, input) to insert/update entries.
    "INSERT OR REPLACE INTO moz_inputhistory "
      // use_count will asymptotically approach the max of 10.
      "SELECT h.id, IFNULL(i.input, :input_text), IFNULL(i.use_count, 0) * .9 + 1 "
      "FROM moz_places_temp h "
      "LEFT JOIN moz_inputhistory i ON i.place_id = h.id AND i.input = :input_text "
      "WHERE url = :page_url "
      "UNION ALL "
      "SELECT h.id, IFNULL(i.input, :input_text), IFNULL(i.use_count, 0) * .9 + 1 "
      "FROM moz_places h "
      "LEFT JOIN moz_inputhistory i ON i.place_id = h.id AND i.input = :input_text "
      "WHERE url = :page_url "
        "AND h.id NOT IN (SELECT id FROM moz_places_temp)"),
    getter_AddRefs(mDBFeedbackIncrease));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return mDBFeedbackIncrease;
}
#endif


nsICollation *
nsNavHistory::GetCollation()
{
  if (mCollation)
    return mCollation;

  // locale
  nsCOMPtr<nsILocale> locale;
  nsCOMPtr<nsILocaleService> ls(do_GetService(NS_LOCALESERVICE_CONTRACTID));
  NS_ENSURE_TRUE(ls, nsnull);
  nsresult rv = ls->GetApplicationLocale(getter_AddRefs(locale));
  NS_ENSURE_SUCCESS(rv, nsnull);

  // collation
  nsCOMPtr<nsICollationFactory> cfact =
    do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID);
  NS_ENSURE_TRUE(cfact, nsnull);
  rv = cfact->CreateCollation(locale, getter_AddRefs(mCollation));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return mCollation;
}

nsIStringBundle *
nsNavHistory::GetBundle()
{
  if (!mBundle) {
    nsCOMPtr<nsIStringBundleService> bundleService =
      mozilla::services::GetStringBundleService();
    NS_ENSURE_TRUE(bundleService, nsnull);
    nsresult rv = bundleService->CreateBundle(
        "chrome://places/locale/places.properties",
        getter_AddRefs(mBundle));
    NS_ENSURE_SUCCESS(rv, nsnull);
  }
  return mBundle;
}

nsIStringBundle *
nsNavHistory::GetDateFormatBundle()
{
  if (!mDateFormatBundle) {
    nsCOMPtr<nsIStringBundleService> bundleService =
      mozilla::services::GetStringBundleService();
    NS_ENSURE_TRUE(bundleService, nsnull);
    nsresult rv = bundleService->CreateBundle(
        "chrome://global/locale/dateFormat.properties",
        getter_AddRefs(mDateFormatBundle));
    NS_ENSURE_SUCCESS(rv, nsnull);
  }
  return mDateFormatBundle;
}

mozIStorageStatement *
nsNavHistory::GetDBVisitToVisitResult()
{
  if (mDBVisitToVisitResult)
    return mDBVisitToVisitResult;

  // mDBVisitToVisitResult, should match kGetInfoIndex_* (see GetQueryResults)
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // have unique visit ids.
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
          "v.visit_date, f.url, v.session, null, null, null, null "
        "FROM moz_places_temp h "
        "LEFT JOIN moz_historyvisits_temp v_t ON h.id = v_t.place_id "
        "LEFT JOIN moz_historyvisits v ON h.id = v.place_id "
        "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
        "WHERE v.id = :visit_id OR v_t.id = :visit_id "
      "UNION ALL "
      "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
          "v.visit_date, f.url, v.session, null, null, null, null "
        "FROM moz_places h "
        "LEFT JOIN moz_historyvisits_temp v_t ON h.id = v_t.place_id "
        "LEFT JOIN moz_historyvisits v ON h.id = v.place_id "
        "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
        "WHERE v.id = :visit_id OR v_t.id = :visit_id "
      "LIMIT 1"),
    getter_AddRefs(mDBVisitToVisitResult));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return mDBVisitToVisitResult;
}

mozIStorageStatement *
nsNavHistory::GetDBVisitToURLResult()
{
  if (mDBVisitToURLResult)
    return mDBVisitToURLResult;

  // mDBVisitToURLResult, should match kGetInfoIndex_* (see GetQueryResults)
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // have unique visit ids.
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
             "h.last_visit_date, f.url, null, null, null, null, null, null "
        "FROM moz_places_temp h "
        "LEFT JOIN moz_historyvisits_temp v_t ON h.id = v_t.place_id "
        "LEFT JOIN moz_historyvisits v ON h.id = v.place_id "
        "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
        "WHERE v.id = :visit_id OR v_t.id = :visit_id "
      "UNION ALL "
      "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
             "h.last_visit_date, f.url, null, null, null, null, null, null "
        "FROM moz_places h "
        "LEFT JOIN moz_historyvisits_temp v_t ON h.id = v_t.place_id "
        "LEFT JOIN moz_historyvisits v ON h.id = v.place_id "
        "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
        "WHERE v.id = :visit_id OR v_t.id = :visit_id "
      "LIMIT 1"),
    getter_AddRefs(mDBVisitToURLResult));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return mDBVisitToURLResult;
}

mozIStorageStatement *
nsNavHistory::GetDBBookmarkToUrlResult()
{
  if (mDBBookmarkToUrlResult)
    return mDBBookmarkToUrlResult;

  // mDBBookmarkToUrlResult, should match kGetInfoIndex_*
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // have unique place ids.
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT b.fk, h.url, COALESCE(b.title, h.title), "
        "h.rev_host, h.visit_count, h.last_visit_date, f.url, null, b.id, "
        "b.dateAdded, b.lastModified, b.parent, null "
      "FROM moz_bookmarks b "
      "JOIN moz_places_temp h ON b.fk = h.id "
      "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
      "WHERE b.id = :item_id "
      "UNION ALL "
      "SELECT b.fk, h.url, COALESCE(b.title, h.title), "
        "h.rev_host, h.visit_count, h.last_visit_date, f.url, null, b.id, "
        "b.dateAdded, b.lastModified, b.parent, null "
      "FROM moz_bookmarks b "
      "JOIN moz_places h ON b.fk = h.id "
      "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
      "WHERE b.id = :item_id "
      "LIMIT 1"),
    getter_AddRefs(mDBBookmarkToUrlResult));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return mDBBookmarkToUrlResult;
}

nsresult
nsNavHistory::FinalizeStatements() {
  mozIStorageStatement* stmts[] = {
#ifdef MOZ_XUL
    mDBFeedbackIncrease,
#endif
    mDBGetURLPageInfo,
    mDBGetIdPageInfo,
    mDBRecentVisitOfURL,
    mDBRecentVisitOfPlace,
    mDBInsertVisit,
    mDBGetPageVisitStats,
    mDBIsPageVisited,
    mDBUpdatePageVisitStats,
    mDBAddNewPage,
    mDBGetTags,
    mDBGetItemsWithAnno,
    mDBSetPlaceTitle,
    mDBVisitToURLResult,
    mDBVisitToVisitResult,
    mDBBookmarkToUrlResult,
    mDBVisitsForFrecency,
    mDBUpdateFrecencyAndHidden,
    mDBGetPlaceVisitStats,
    mDBFullVisitCount,
    mDBRegisterOpenPage,
    mDBUnregisterOpenPage,
  };

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(stmts); i++) {
    nsresult rv = nsNavHistory::FinalizeStatement(stmts[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// nsICharsetResolver **********************************************************

NS_IMETHODIMP
nsNavHistory::RequestCharset(nsIWebNavigation* aWebNavigation,
                             nsIChannel* aChannel,
                             PRBool* aWantCharset,
                             nsISupports** aClosure,
                             nsACString& aResult)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aChannel);
  NS_ENSURE_ARG_POINTER(aWantCharset);
  NS_ENSURE_ARG_POINTER(aClosure);

  *aWantCharset = PR_FALSE;
  *aClosure = nsnull;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv))
    return NS_OK;

  nsAutoString charset;
  rv = GetCharsetForURI(uri, charset);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUTF16toUTF8(charset, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::NotifyResolvedCharset(const nsACString& aCharset,
                                    nsISupports* aClosure)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  NS_ERROR("Unexpected call to NotifyResolvedCharset -- we never set aWantCharset to true!");
  return NS_ERROR_NOT_IMPLEMENTED;
}
