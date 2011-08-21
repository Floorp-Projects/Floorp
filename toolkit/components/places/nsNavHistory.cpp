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
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *   Paolo Amadini <http://www.amadzone.org/>
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
#include "mozIStorageAsyncStatement.h"
#include "mozIPlacesAutoComplete.h"

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
#include "mozilla/Util.h"

#ifdef MOZ_XUL
#include "nsIAutoCompleteInput.h"
#include "nsIAutoCompletePopup.h"
#endif

using namespace mozilla;
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

#define PREF_CACHE_TO_MEMORY_PERCENTAGE         "database.cache_to_memory_percentage"

#define PREF_FORCE_DATABASE_REPLACEMENT         "database.replaceOnStartup"

// Default integer value for PREF_CACHE_TO_MEMORY_PERCENTAGE.
// This is 6% of machine memory, giving 15MB for a user with 256MB of memory.
// Out of this cache, SQLite will use at most the size of the database file.
#define DATABASE_DEFAULT_CACHE_TO_MEMORY_PERCENTAGE 6

// If the physical memory size is not available, use MEMSIZE_FALLBACK_BYTES
// instead.  Must stay in sync with the code in nsPlacesExpiration.js.
#define MEMSIZE_FALLBACK_BYTES 268435456 // 256 M

// Maximum size for the WAL file.  It should be small enough since in case of
// crashes we could lose all the transactions in the file.  But a too small
// file could hurt performance.
#define DATABASE_MAX_WAL_SIZE_IN_KIBIBYTES 512

#define BYTES_PER_MEBIBYTE 1048576

// This is the schema version, update it at any schema change and add a
// corresponding migrateVxx method below.
#define DATABASE_SCHEMA_VERSION 11

// Filename of the database.
#define DATABASE_FILENAME NS_LITERAL_STRING("places.sqlite")

// Filename used to backup corrupt databases.
#define DATABASE_CORRUPT_FILENAME NS_LITERAL_STRING("places.sqlite.corrupt")

// In order to avoid calling PR_now() too often we use a cached "now" value
// for repeating stuff.  These are milliseconds between "now" cache refreshes.
#define RENEW_CACHED_NOW_TIMEOUT ((PRInt32)3 * PR_MSEC_PER_SEC)

// USECS_PER_DAY == PR_USEC_PER_SEC * 60 * 60 * 24;
static const PRInt64 USECS_PER_DAY = LL_INIT(20, 500654080);

// character-set annotation
#define CHARSET_ANNO NS_LITERAL_CSTRING("URIProperties/characterSet")

// Sync guid annotation
#define SYNCGUID_ANNO NS_LITERAL_CSTRING("sync/guid")

// Download destination file URI annotation
#define DESTINATIONFILEURI_ANNO \
  NS_LITERAL_CSTRING("downloads/destinationFileURI")

// Download destination file name annotation
#define DESTINATIONFILENAME_ANNO \
  NS_LITERAL_CSTRING("downloads/destinationFileName")

// These macros are used when splitting history by date.
// These are the day containers and catch-all final container.
#define HISTORY_ADDITIONAL_DATE_CONT_NUM 3
// We use a guess of the number of months considering all of them 30 days
// long, but we split only the last 6 months.
#define HISTORY_DATE_CONT_NUM(_daysFromOldestVisit) \
  (HISTORY_ADDITIONAL_DATE_CONT_NUM + \
   NS_MIN(6, (PRInt32)ceilf((float)_daysFromOldestVisit/30)))
// Max number of containers, used to initialize the params hash.
#define HISTORY_DATE_CONT_MAX 10

// Initial size of the embed visits cache.
#define EMBED_VISITS_INITIAL_CACHE_SIZE 128

// Initial size of the recent events caches.
#define RECENT_EVENTS_INITIAL_CACHE_SIZE 128

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
  NS_INTERFACE_MAP_ENTRY(nsIGlobalHistory2)
  NS_INTERFACE_MAP_ENTRY(nsIDownloadHistory)
  NS_INTERFACE_MAP_ENTRY(nsIBrowserHistory)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsICharsetResolver)
  NS_INTERFACE_MAP_ENTRY(nsPIPlacesDatabase)
  NS_INTERFACE_MAP_ENTRY(nsPIPlacesHistoryListenersNotifier)
  NS_INTERFACE_MAP_ENTRY(mozIStorageVacuumParticipant)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsINavHistoryService)
  NS_IMPL_QUERY_CLASSINFO(nsNavHistory)
NS_INTERFACE_MAP_END

// We don't care about flattening everything
NS_IMPL_CI_INTERFACE_GETTER4(
  nsNavHistory
, nsINavHistoryService
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
        // This subquery DOES NOT order tags for performance reasons.
        _sqlFragment.Assign(NS_LITERAL_CSTRING(
             "(SELECT GROUP_CONCAT(t_t.title, ',') "
               "FROM moz_bookmarks b_t "
               "JOIN moz_bookmarks t_t ON t_t.id = b_t.parent  "
               "WHERE b_t.fk = ") + aRelation + NS_LITERAL_CSTRING(" "
               "AND t_t.parent = ") +
               nsPrintfCString("%lld", aTagsFolder) + NS_LITERAL_CSTRING(" "
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


// Used to notify a topic to system observers on async execute completion.
class AsyncStatementCallbackNotifier : public AsyncStatementCallback
{
public:
  AsyncStatementCallbackNotifier(const char* aTopic)
    : mTopic(aTopic)
  {
  }

  NS_IMETHOD HandleCompletion(PRUint16 aReason);

private:
  const char* mTopic;
};

NS_IMETHODIMP
AsyncStatementCallbackNotifier::HandleCompletion(PRUint16 aReason)
{
  if (aReason != mozIStorageStatementCallback::REASON_FINISHED)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    (void)obs->NotifyObservers(nsnull, mTopic, nsnull);
  }

  return NS_OK;
}

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
const PRInt32 nsNavHistory::kGetInfoIndex_Frecency = 13;

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsNavHistory, gHistoryService)


nsNavHistory::nsNavHistory()
: mBatchLevel(0)
, mBatchDBTransaction(nsnull)
, mAsyncThreadStatements(mDBConn)
, mStatements(mDBConn)
, mDBPageSize(0)
, mCurrentJournalMode(JOURNAL_DELETE)
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
  NS_ENSURE_TRUE(mRecentTyped.Init(RECENT_EVENTS_INITIAL_CACHE_SIZE),
                 NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mRecentLink.Init(RECENT_EVENTS_INITIAL_CACHE_SIZE),
                 NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mRecentBookmark.Init(RECENT_EVENTS_INITIAL_CACHE_SIZE),
                 NS_ERROR_OUT_OF_MEMORY);

  // Embed visits hash table.
  NS_ENSURE_TRUE(mEmbedVisits.Init(EMBED_VISITS_INITIAL_CACHE_SIZE),
                 NS_ERROR_OUT_OF_MEMORY);

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

  // Don't add code that can fail here! Do it up above, before we add our
  // observers.

  return NS_OK;
}


nsresult
nsNavHistory::InitDBFile(PRBool aForceInit)
{
  if (!mDBService) {
    mDBService = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
    NS_ENSURE_STATE(mDBService);
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
    if (mDBConn) {
      // If there's any not finalized statement or this fails for any reason
      // we won't be able to remove the database.
      rv = mDBConn->Close();
      NS_ENSURE_SUCCESS(rv, rv);
    }

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
    if (!dbExists) {
      mDatabaseStatus = DATABASE_STATUS_CREATE;
    }
    else {
      // Check if maintenance required a database replacement.
      PRBool forceDatabaseReplacement;
      if (NS_SUCCEEDED(mPrefBranch->GetBoolPref(PREF_FORCE_DATABASE_REPLACEMENT,
                                                &forceDatabaseReplacement)) &&
          forceDatabaseReplacement) {
        // Be sure to clear the pref to avoid handling it more than once.
        rv = mPrefBranch->ClearUserPref(PREF_FORCE_DATABASE_REPLACEMENT);
        NS_ENSURE_SUCCESS(rv, rv);
        // Re-enter this same method, forcing the replacement.
        rv = InitDBFile(PR_TRUE);
        NS_ENSURE_SUCCESS(rv, rv);
        return NS_OK;
      }
    }
  }

  // Open the database file.  If it does not exist a new one will be created.
  // Use a unshared connection, both for safety and performance.
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
nsNavHistory::SetJournalMode(enum JournalMode aJournalMode)
{
  nsCAutoString journalMode;
  switch (aJournalMode) {
    default:
      NS_NOTREACHED("Trying to set an unknown journal mode.");
      // Fall through to the default mode of DELETE.
    case JOURNAL_DELETE:
      journalMode.AssignLiteral("delete");
      break;
    case JOURNAL_TRUNCATE:
      journalMode.AssignLiteral("truncate");
      break;
    case JOURNAL_MEMORY:
      journalMode.AssignLiteral("memory");
      break;
    case JOURNAL_WAL:
      journalMode.AssignLiteral("wal");
      break;
  }

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "PRAGMA journal_mode = ") + journalMode,
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageStatementScoper scoper(statement);
  PRBool hasResult;
  rv = statement->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(hasResult, NS_ERROR_FAILURE);

  nsCAutoString currentJournalMode;
  rv = statement->GetUTF8String(0, currentJournalMode);
  NS_ENSURE_SUCCESS(rv, rv);
  bool succeeded = currentJournalMode.Equals(journalMode);
  if (succeeded) {
    mCurrentJournalMode = aJournalMode;
  }
  else {
    NS_WARNING(nsPrintfCString(128, "Setting journal mode failed: %s",
                               PromiseFlatCString(journalMode).get()).get());
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


nsresult
nsNavHistory::InitDB()
{
  {
    // Get the page size.  This may be different than the default if the
    // database file already existed with a different page size.
    nsCOMPtr<mozIStorageStatement> statement;
    nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING("PRAGMA page_size"),
                                  getter_AddRefs(statement));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    mozStorageStatementScoper scoper(statement);
    rv = statement->ExecuteStep(&hasResult);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasResult, NS_ERROR_FAILURE);
    rv = statement->GetInt32(0, &mDBPageSize);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(mDBPageSize > 0, NS_ERROR_UNEXPECTED);
  }

  // Ensure that temp tables are held in memory, not on disk.
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "PRAGMA temp_store = MEMORY"));
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

  static PRUint64 physMem = PR_GetPhysicalMemorySize();
  if (physMem == 0)
    physMem = MEMSIZE_FALLBACK_BYTES;

  PRUint64 cacheSize = physMem * cachePercentage / 100;

  // Compute number of cached pages, this will be our cache size.
  PRUint64 cachePages = cacheSize / mDBPageSize;
  nsCAutoString cacheSizePragma("PRAGMA cache_size = ");
  cacheSizePragma.AppendInt(cachePages);
  rv = mDBConn->ExecuteSimpleSQL(cacheSizePragma);
  NS_ENSURE_SUCCESS(rv, rv);

  // Be sure to set journal mode after page_size.  WAL would prevent the change
  // otherwise.
  if (NS_SUCCEEDED(SetJournalMode(JOURNAL_WAL))) {
    // Set the WAL journal size limit.  We want it to be small, since in
    // synchronous = NORMAL mode a crash could cause loss of all the
    // transactions in the journal.  For added safety we will also force
    // checkpointing at strategic moments.
    PRInt32 checkpointPages =
      static_cast<PRInt32>(DATABASE_MAX_WAL_SIZE_IN_KIBIBYTES * 1024 / mDBPageSize);
    nsCAutoString checkpointPragma("PRAGMA wal_autocheckpoint = ");
    checkpointPragma.AppendInt(checkpointPages);
    rv = mDBConn->ExecuteSimpleSQL(checkpointPragma);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Ignore errors, if we fail here the database could be considered corrupt
    // and we won't be able to go on, even if it's just matter of a bogus file
    // system.  The default mode (DELETE) will be fine in such a case.
    (void)SetJournalMode(JOURNAL_TRUNCATE);

    // Set synchronous to FULL to ensure maximum data integrity, even in
    // case of crashes or unclean shutdowns.
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "PRAGMA synchronous = FULL"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Grow places in 10MiB increments
  (void)mDBConn->SetGrowthIncrement(10 * BYTES_PER_MEBIBYTE, EmptyCString());

  // We use our functions during migration, so initialize them now.
  rv = InitFunctions();
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the database schema version.
  PRInt32 currentSchemaVersion;
  rv = mDBConn->GetSchemaVersion(&currentSchemaVersion);
  NS_ENSURE_SUCCESS(rv, rv);
  bool databaseInitialized = currentSchemaVersion > 0;

  if (databaseInitialized && currentSchemaVersion == DATABASE_SCHEMA_VERSION) {
    // The database is up to date and ready to go.
    return NS_OK;
  }

  // We are going to update the database, so everything from now on should be in
  // a transaction for performances.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  if (databaseInitialized) {
    // Migration How-to:
    //
    // 1. increment PLACES_SCHEMA_VERSION.
    // 2. implement a method that performs upgrade to your version from the
    //    previous one.
    //
    // NOTE: The downgrade process is pretty much complicated by the fact old
    //       versions cannot know what a new version is going to implement.
    //       The only thing we will do for downgrades is setting back the schema
    //       version, so that next upgrades will run again the migration step.

    if (currentSchemaVersion < DATABASE_SCHEMA_VERSION) {
      mDatabaseStatus = DATABASE_STATUS_UPGRADED;

      // Firefox 3.0 uses schema version 6.

      if (currentSchemaVersion < 7) {
        rv = MigrateV7Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 8) {
        rv = MigrateV8Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 3.5 uses schema version 8.

      if (currentSchemaVersion < 9) {
        rv = MigrateV9Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      if (currentSchemaVersion < 10) {
        rv = MigrateV10Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 3.6 uses schema version 10.

      if (currentSchemaVersion < 11) {
        rv = MigrateV11Up(mDBConn);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Firefox 4.0 uses schema version 11.

      // Schema Upgrades must add migration code here.
    }
  }
  else {
    // This is a new database, so we have to create all the tables and indices.
    rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_PLACES);
    NS_ENSURE_SUCCESS(rv, rv);
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
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_LASTVISITDATE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_GUID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_HISTORYVISITS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_PLACEDATE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_FROMVISIT);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_HISTORYVISITS_VISITDATE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mDBConn->ExecuteSimpleSQL(CREATE_MOZ_INPUTHISTORY);
    NS_ENSURE_SUCCESS(rv, rv);

    // Initialize the other Places services' database tables, since some of our
    // statements depend on them.
    rv = nsNavBookmarks::InitTables(mDBConn);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = nsFaviconService::InitTables(mDBConn);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = nsAnnotationService::InitTables(mDBConn);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set the schema version to the current one.
  rv = UpdateSchemaVersion();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ForceWALCheckpoint(mDBConn);

  // ANY FAILURE IN THIS METHOD WILL CAUSE US TO MARK THE DATABASE AS CORRUPT
  // AND TRY TO REPLACE IT.
  // DO NOT PUT HERE ANYTHING THAT IS NOT RELATED TO INITIALIZATION OR MODIFYING
  // THE DISK DATABASE.

  return NS_OK;
}


nsresult
nsNavHistory::InitAdditionalDBItems()
{
  nsresult rv = InitTriggers();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsNavHistory::CheckAndUpdateGUIDs()
{
  // First, import any bookmark guids already set by Sync.
  nsCOMPtr<mozIStorageStatement> updateStmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks "
    "SET guid = :guid "
    "WHERE id = :item_id "
  ), getter_AddRefs(updateStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT item_id, content "
    "FROM moz_items_annos "
    "JOIN moz_anno_attributes "
    "WHERE name = :anno_name "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"),
                                  SYNCGUID_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    PRInt64 itemId;
    rv = stmt->GetInt64(0, &itemId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString guid;
    rv = stmt->GetUTF8String(1, guid);
    NS_ENSURE_SUCCESS(rv, rv);

    // If we have an invalid guid, we don't need to do any more work.
    if (!IsValidGUID(guid)) {
      continue;
    }

    mozStorageStatementScoper scoper(updateStmt);
    rv = updateStmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), itemId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = updateStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), guid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = updateStmt->Execute();
    if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
      // We just tried to insert a duplicate guid.  Ignore this error, and we
      // will generate a new one next.
      continue;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Now, remove all the bookmark guid annotations that we just imported.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_items_annos "
    "WHERE anno_attribute_id = ( "
      "SELECT id "
      "FROM moz_anno_attributes "
      "WHERE name = :anno_name "
    ") "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"),
                                  SYNCGUID_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Next, generate guids for any bookmark that does not already have one.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_bookmarks "
    "SET guid = GENERATE_GUID() "
    "WHERE guid IS NULL "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Now, import any history guids already set by Sync.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_places "
    "SET guid = :guid "
    "WHERE id = :place_id "
  ), getter_AddRefs(updateStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT place_id, content "
    "FROM moz_annos "
    "JOIN moz_anno_attributes "
    "WHERE name = :anno_name "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"),
                                  SYNCGUID_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);

  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    PRInt64 placeId;
    rv = stmt->GetInt64(0, &placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString guid;
    rv = stmt->GetUTF8String(1, guid);
    NS_ENSURE_SUCCESS(rv, rv);

    // If we have an invalid guid, we don't need to do any more work.
    if (!IsValidGUID(guid)) {
      continue;
    }

    mozStorageStatementScoper scoper(updateStmt);
    rv = updateStmt->BindInt64ByName(NS_LITERAL_CSTRING("place_id"), placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = updateStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), guid);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = updateStmt->Execute();
    if (rv == NS_ERROR_STORAGE_CONSTRAINT) {
      // We just tried to insert a duplicate guid.  Ignore this error, and we
      // will generate a new one next.
      continue;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Now, remove all the place guid annotations that we just imported.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM moz_annos "
    "WHERE anno_attribute_id = ( "
      "SELECT id "
      "FROM moz_anno_attributes "
      "WHERE name = :anno_name "
    ") "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("anno_name"),
                                  SYNCGUID_ANNO);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, we need to generate guids for any places that do not already have
  // one.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE moz_places "
    "SET guid = GENERATE_GUID() "
    "WHERE guid IS NULL "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
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

  rv = CalculateFrecencyFunction::create(mDBConn);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GenerateGUIDFunction::create(mDBConn);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::InitTriggers()
{
  nsresult rv = mDBConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_AFTERINSERT_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBConn->ExecuteSimpleSQL(CREATE_HISTORYVISITS_AFTERDELETE_TRIGGER);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

mozIStorageStatement*
nsNavHistory::GetStatement(const nsCOMPtr<mozIStorageStatement>& aStmt)
{
  // mCanNotify is set to false on shutdown.
  if (!mCanNotify)
    return nsnull;

  // Note that this query violates the kGetInfoIndex_* convention in
  // the last column.
  RETURN_IF_STMT(mDBGetURLPageInfo, NS_LITERAL_CSTRING(
    "SELECT id, url, title, rev_host, visit_count, guid "
    "FROM moz_places "
    "WHERE url = :page_url "
  ));

  RETURN_IF_STMT(mDBGetIdPageInfo, NS_LITERAL_CSTRING(
    "SELECT id, url, title, rev_host, visit_count "
    "FROM moz_places "
    "WHERE id = :page_id "
  ));

  RETURN_IF_STMT(mDBRecentVisitOfURL, NS_LITERAL_CSTRING(
    "SELECT id, session, visit_date "
    "FROM moz_historyvisits "
    "WHERE place_id = (SELECT id FROM moz_places WHERE url = :page_url) "
    "ORDER BY visit_date DESC "
  ));

  RETURN_IF_STMT(mDBRecentVisitOfPlace, NS_LITERAL_CSTRING(
    "SELECT id FROM moz_historyvisits "
    "WHERE place_id = :page_id "
      "AND visit_date = :visit_date "
      "AND session = :session "
  ));

  RETURN_IF_STMT(mDBInsertVisit, NS_LITERAL_CSTRING(
    "INSERT INTO moz_historyvisits "
      "(from_visit, place_id, visit_date, visit_type, session) "
    "VALUES (:from_visit, :page_id, :visit_date, :visit_type, :session) "
  ));

  RETURN_IF_STMT(mDBGetPageVisitStats, NS_LITERAL_CSTRING(
    "SELECT id, visit_count, typed, hidden, guid "
    "FROM moz_places "
    "WHERE url = :page_url "
  ));

  RETURN_IF_STMT(mDBIsPageVisited, NS_LITERAL_CSTRING(
    "SELECT h.id "
    "FROM moz_places h "
    "WHERE url = ?1 "
      "AND EXISTS(SELECT id FROM moz_historyvisits WHERE place_id = h.id LIMIT 1) "
  ));

  RETURN_IF_STMT(mDBUpdatePageVisitStats, NS_LITERAL_CSTRING(
    "UPDATE moz_places "
    "SET hidden = :hidden, typed = :typed "
    "WHERE id = :page_id "
  ));

  // We have both sync and async users, so it could happen that an async
  // statement tries to insert a page when a sync statement just added it.
  // We should ignore the insertion in such a case, the async implementer
  // will fetch the id of the existing entry.
  RETURN_IF_STMT(mDBAddNewPage, NS_LITERAL_CSTRING(
    "INSERT OR IGNORE INTO moz_places "
      "(url, title, rev_host, hidden, typed, frecency, guid) "
    "VALUES (:page_url, :page_title, :rev_host, :hidden, :typed, :frecency, "
             "GENERATE_GUID()) "
  ));

  RETURN_IF_STMT(mDBGetTags, NS_LITERAL_CSTRING(
    "/* do not warn (bug 487594) */ "
    "SELECT GROUP_CONCAT(tag_title, ', ') "
    "FROM ( "
      "SELECT t.title AS tag_title "
      "FROM moz_bookmarks b "
      "JOIN moz_bookmarks t ON t.id = b.parent "
      "WHERE b.fk = (SELECT id FROM moz_places WHERE url = :page_url) "
        "AND t.parent = :tags_folder "
      "ORDER BY t.title COLLATE NOCASE ASC "
    ") "
  ));

  RETURN_IF_STMT(mDBGetItemsWithAnno, NS_LITERAL_CSTRING(
    "SELECT a.item_id, a.content "
    "FROM moz_anno_attributes n "
    "JOIN moz_items_annos a ON n.id = a.anno_attribute_id "
    "WHERE n.name = :anno_name "
  ));

  RETURN_IF_STMT(mDBSetPlaceTitle, NS_LITERAL_CSTRING(
    "UPDATE moz_places "
    "SET title = :page_title "
    "WHERE url = :page_url "
  ));

  RETURN_IF_STMT(mDBUpdateFrecency, NS_LITERAL_CSTRING(
      "UPDATE moz_places "
      "SET frecency = CALCULATE_FRECENCY(:page_id) "
      "WHERE id = :page_id"
  ));

  RETURN_IF_STMT(mDBUpdateHiddenOnFrecency, NS_LITERAL_CSTRING(
      "UPDATE moz_places "
      "SET hidden = 0 "
      "WHERE id = :page_id AND frecency <> 0"
  ));

#ifdef MOZ_XUL
  RETURN_IF_STMT(mDBFeedbackIncrease, NS_LITERAL_CSTRING(
    // Leverage the PRIMARY KEY (place_id, input) to insert/update entries.
    "INSERT OR REPLACE INTO moz_inputhistory "
      // use_count will asymptotically approach the max of 10.
      "SELECT h.id, IFNULL(i.input, :input_text), IFNULL(i.use_count, 0) * .9 + 1 "
      "FROM moz_places h "
      "LEFT JOIN moz_inputhistory i ON i.place_id = h.id AND i.input = :input_text "
      "WHERE url = :page_url "));
#endif

  nsCAutoString tagsFragment;
  GetTagsSqlFragment(GetTagsFolder(), NS_LITERAL_CSTRING("h.id"),
                     PR_TRUE, tagsFragment);

  // Should match kGetInfoIndex_* (see GetQueryResults)
  RETURN_IF_STMT(mDBVisitToVisitResult, NS_LITERAL_CSTRING(
    "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
           "v.visit_date, f.url, v.session, null, null, null, null, "
           ) + tagsFragment + NS_LITERAL_CSTRING(", h.frecency "
    "FROM moz_places h "
    "JOIN moz_historyvisits v ON h.id = v.place_id "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE v.id = :visit_id "
  ));

  // Should match kGetInfoIndex_* (see GetQueryResults)
  RETURN_IF_STMT(mDBVisitToURLResult, NS_LITERAL_CSTRING(
    "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
           "h.last_visit_date, f.url, null, null, null, null, null, "
           ) + tagsFragment + NS_LITERAL_CSTRING(", h.frecency "
    "FROM moz_places h "
    "JOIN moz_historyvisits v ON h.id = v.place_id "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE v.id = :visit_id "
  ));

  // mDBBookmarkToUrlResult, should match kGetInfoIndex_*
  RETURN_IF_STMT(mDBBookmarkToUrlResult, NS_LITERAL_CSTRING(
    "SELECT b.fk, h.url, COALESCE(b.title, h.title), "
           "h.rev_host, h.visit_count, h.last_visit_date, f.url, null, b.id, "
           "b.dateAdded, b.lastModified, b.parent, "
           ) + tagsFragment + NS_LITERAL_CSTRING(", h.frecency "
    "FROM moz_bookmarks b "
    "JOIN moz_places h ON b.fk = h.id "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE b.id = :item_id "
  ));

  // mDBUrlToUrlResult, should match kGetInfoIndex_*
  RETURN_IF_STMT(mDBUrlToUrlResult, NS_LITERAL_CSTRING(
    "SELECT h.id, :page_url, h.title, h.rev_host, h.visit_count, "
           "h.last_visit_date, f.url, null, null, null, null, null, "
           ) + tagsFragment + NS_LITERAL_CSTRING(", h.frecency "
    "FROM moz_places h "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.url = :page_url "
  ));

  return nsnull;
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
  // The cached value is synced by triggers on every added or removed visit.
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
    rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "UPDATE moz_places SET last_visit_date = "
          "(SELECT MAX(visit_date) "
           "FROM moz_historyvisits "
           "WHERE place_id = moz_places.id)"));
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


nsresult
nsNavHistory::MigrateV11Up(mozIStorageConnection *aDBConn)
{
  // Temp tables are going away.
  // For triggers correctness, every time we pass through this migration
  // step, we must ensure correctness of visit_count values.
  nsresult rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_places SET visit_count = "
      "(SELECT count(*) FROM moz_historyvisits "
       "WHERE place_id = moz_places.id "
        "AND visit_type NOT IN ") +
          nsPrintfCString("(0,%d,%d,%d) ",
                          nsINavHistoryService::TRANSITION_EMBED,
                          nsINavHistoryService::TRANSITION_FRAMED_LINK,
                          nsINavHistoryService::TRANSITION_DOWNLOAD) +
      NS_LITERAL_CSTRING(")")
  );
  NS_ENSURE_SUCCESS(rv, rv);

  // For existing profiles, we may not have a moz_bookmarks.guid column
  nsCOMPtr<mozIStorageStatement> hasGuidStatement;
  rv = aDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT guid FROM moz_bookmarks"),
    getter_AddRefs(hasGuidStatement));

  if (NS_FAILED(rv)) {
    // moz_bookmarks grew a guid column.  Add the column, but do not populate it
    // with anything just yet.  We will do that soon.
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_bookmarks "
      "ADD COLUMN guid TEXT"
    ));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_BOOKMARKS_GUID);
    NS_ENSURE_SUCCESS(rv, rv);

    // moz_placess grew a guid column.  Add the column, but do not populate it
    // with anything just yet.  We will do that soon.
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE moz_places "
      "ADD COLUMN guid TEXT"
    ));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aDBConn->ExecuteSimpleSQL(CREATE_IDX_MOZ_PLACES_GUID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We need to update our guids before we do any real database work.
  rv = CheckAndUpdateGUIDs();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::GetIdForPage(nsIURI* aURI,
                           PRInt64* _pageId,
                           nsCString& _GUID)
{
  *_pageId = 0;

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetURLPageInfo);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasEntry = PR_FALSE;
  rv = stmt->ExecuteStep(&hasEntry);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasEntry) {
    rv = stmt->GetInt64(kGetInfoIndex_PageID, _pageId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetUTF8String(5, _GUID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
  
nsresult
nsNavHistory::GetOrCreateIdForPage(nsIURI* aURI,
                                   PRInt64* _pageId,
                                   nsCString& _GUID)
{
  nsresult rv = GetIdForPage(aURI, _pageId, _GUID);
  NS_ENSURE_SUCCESS(rv, rv);

  if (*_pageId == 0) {
    // Create a new hidden, untyped and unvisited entry.
    nsAutoString voidString;
    voidString.SetIsVoid(PR_TRUE);
    rv = InternalAddNewPage(aURI, voidString, PR_TRUE, PR_FALSE, 0, PR_TRUE,
                            _pageId, _GUID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
                                 PRInt64* aPageID,
                                 nsACString& guid)
{
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBAddNewPage);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aTitle.IsVoid()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("page_title"));
  }
  else {
    rv = stmt->BindStringByName(
      NS_LITERAL_CSTRING("page_title"), StringHead(aTitle, TITLE_LENGTH_MAX)
    );
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // host (reversed with trailing period)
  nsAutoString revHost;
  rv = GetReversedHostname(aURI, revHost);
  // Not all URI types have hostnames, so this is optional.
  if (NS_SUCCEEDED(rv)) {
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("rev_host"), revHost);
  } else {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("rev_host"));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), aHidden);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("typed"), aTyped);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("frecency"),
                             IsQueryURI(spec) ? 0 : -1);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt64 pageId = 0;
  {
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(getIdStmt, mDBGetURLPageInfo);
    rv = URIBinder::Bind(getIdStmt, NS_LITERAL_CSTRING("page_url"), aURI);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult = PR_FALSE;
    rv = getIdStmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(hasResult, "hasResult is false but the call succeeded?");
    pageId = getIdStmt->AsInt64(0);
    rv = getIdStmt->GetUTF8String(5, guid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aCalculateFrecency) {
    rv = UpdateFrecency(pageId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If the caller wants the page ID, return it.
  if (aPageID) {
    *aPageID = pageId;
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
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBInsertVisit);
  
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("from_visit"), aReferringVisit);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPageID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("visit_date"), aTime);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("visit_type"), aTransitionType);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("session"), aSessionID);
    NS_ENSURE_SUCCESS(rv, rv);
  
    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  {
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBRecentVisitOfPlace);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPageID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("visit_date"), aTime);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("session"), aSessionID);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(hasResult, "hasResult is false but the call succeeded?");

    rv = stmt->GetInt64(0, visitID);
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
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT_RET(stmt, mDBRecentVisitOfURL, PR_FALSE);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  PRBool hasMore;
  rv = stmt->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  if (hasMore) {
    rv = stmt->GetInt64(0, aVisitID);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    rv = stmt->GetInt64(1, aSessionID);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    rv = stmt->GetInt64(2, aTime);
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
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT_RET(stmt, mDBIsPageVisited, PR_FALSE);
  nsresult rv = URIBinder::Bind(stmt, 0, aURIString);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  PRBool hasMore = PR_FALSE;
  rv = stmt->ExecuteStep(&hasMore);
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
  nsCOMPtr<mozIStorageStatement> selectSession;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT session FROM moz_historyvisits "
    "ORDER BY visit_date DESC "
  ), getter_AddRefs(selectSession));
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
                          PRInt32 aTransitionType,
                          const nsACString& aGUID)
{
  PRUint32 added = 0;
  MOZ_ASSERT(!aGUID.IsEmpty());
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver,
                   OnVisit(aURI, aVisitID, aTime, aSessionID,
                           referringVisitID, aTransitionType, aGUID, &added));
}

void
nsNavHistory::NotifyTitleChange(nsIURI* aURI,
                                const nsString& aTitle,
                                const nsACString& aGUID)
{
  MOZ_ASSERT(!aGUID.IsEmpty());
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver, OnTitleChanged(aURI, aTitle, aGUID));
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
    if (!query->SearchTerms().IsEmpty() ||
        !query->Domain().IsVoid() ||
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
  if (aQueries.Count() == 1 && !nonTimeBasedItems)
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
  // To properly generate a uri we must provide a protocol.
  nsCAutoString fakeURL("http://");
  fakeURL.Append(aHostName);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), fakeURL);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = uri->GetAsciiHost(aAscii);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
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
      "SELECT 1 FROM moz_historyvisits "),
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
      "UPDATE moz_places "
      "SET frecency = 0 WHERE id IN ("
        "SELECT h.id FROM moz_places h "
        "WHERE h.url >= 'place:' AND h.url < 'place;' "
        "UNION ALL "
        // Unvisited child of a livemark
        "SELECT b.fk FROM moz_bookmarks b "
        "JOIN moz_places h ON b.fk = h.id AND visit_count = 0 AND frecency < 0 "
        "JOIN moz_bookmarks bp ON bp.id = b.parent "
        "JOIN moz_items_annos a ON a.item_id = bp.id "
        "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id "
        "WHERE n.name = :anno_name "
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
      scheme.EqualsLiteral("resource") ||
      scheme.EqualsLiteral("data") ||
      scheme.EqualsLiteral("wyciwyg") ||
      scheme.EqualsLiteral("javascript")) {
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

  // Embed visits are not added to database, but registered in a  session cache.
  // For the above reason they don't have a visit id.
  if (aTransitionType == TRANSITION_EMBED) {
    registerEmbedVisit(aURI, GetNow());
    *aVisitID = 0;
    return NS_OK;
  }

  // This will prevent corruption since we have to do a two-phase add.
  // Generally this won't do anything because AddURI has its own transaction.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // see if this is an update (revisit) or a new page
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetPageVisitStats);
  rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool alreadyVisited = PR_FALSE;
  rv = stmt->ExecuteStep(&alreadyVisited);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString guid;
  PRInt64 pageID = 0;
  PRInt32 hidden;
  PRInt32 typed;
  PRBool newItem = PR_FALSE; // used to send out notifications at the end
  if (alreadyVisited) {
    // Update the existing entry...
    rv = stmt->GetInt64(0, &pageID);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 oldVisitCount = 0;
    rv = stmt->GetInt32(1, &oldVisitCount);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 oldTypedState = 0;
    rv = stmt->GetInt32(2, &oldTypedState);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool oldHiddenState = 0;
    rv = stmt->GetInt32(3, &oldHiddenState);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->GetUTF8String(4, guid);
    NS_ENSURE_SUCCESS(rv, rv);

    // free the previous statement before we make a new one
    stmt->Reset();
    scoper.Abandon();

    // Note that we want to unhide any hidden pages that the user explicitly
    // types (aTransitionType == TRANSITION_TYPED) so that they will appear in
    // the history UI (sidebar, history menu, url bar autocomplete, etc).
    // Additionally, we don't want to hide any pages that are already unhidden.
    hidden = oldHiddenState;
    if (hidden == 1 &&
        (!GetHiddenState(aIsRedirect, aTransitionType) ||
         aTransitionType == TRANSITION_TYPED)) {
      hidden = 0; // unhide
    }

    typed = (PRInt32)(oldTypedState == 1 || (aTransitionType == TRANSITION_TYPED));

    // some items may have a visit count of 0 which will not count for link
    // visiting, so be sure to note this transition
    if (oldVisitCount == 0)
      newItem = PR_TRUE;

    // update with new stats
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(updateStmt, mDBUpdatePageVisitStats);
    rv = updateStmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), pageID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = updateStmt->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), hidden);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = updateStmt->BindInt32ByName(NS_LITERAL_CSTRING("typed"), typed);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = updateStmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // New page
    newItem = PR_TRUE;

    // free the previous statement before we make a new one
    stmt->Reset();
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
                            PR_TRUE, &pageID, guid);
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
  (void)UpdateFrecency(pageID);

  // Notify observers: The hidden detection code must match that in
  // GetQueryResults to maintain consistency.
  // FIXME bug 325241: make a way to observe hidden URLs
  if (!hidden) {
    NotifyOnVisit(aURI, *aVisitID, aTime, aSessionID, referringVisitID,
                  aTransitionType, guid);
  }

  // Normally docshell sends the link visited observer notification for us (this
  // will tell all the documents to update their visited link coloring).
  // However, for redirects and downloads (since we implement nsIDownloadHistory)
  // this will not happen and we need to send it ourselves.
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

  // Create the root node.
  nsRefPtr<nsNavHistoryContainerResultNode> rootNode;
  PRInt64 folderId = GetSimpleBookmarksQueryFolder(queries, options);
  if (folderId) {
    // In the simple case where we're just querying children of a single
    // bookmark folder, we can more efficiently generate results.
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
    nsRefPtr<nsNavHistoryResultNode> tempRootNode;
    rv = bookmarks->ResultNodeForContainer(folderId, options,
                                           getter_AddRefs(tempRootNode));
    if (NS_SUCCEEDED(rv)) {
      rootNode = tempRootNode->GetAsContainer();
    }
    else {
      NS_WARNING("Generating a generic empty node for a broken query!");
      // This is a perf hack to generate an empty query that skips filtering.
      options->SetExcludeItems(PR_TRUE);
    }
  }

  if (!rootNode) {
    // Either this is not a folder shortcut, or is a broken one.  In both cases
    // just generate a query node.
    rootNode = new nsNavHistoryQueryResultNode(EmptyCString(), EmptyCString(),
                                               queries, options);
  }

  // Create the result that will hold nodes.  Inject batching status into it.
  nsRefPtr<nsNavHistoryResult> result;
  rv = nsNavHistoryResult::NewHistoryResult(aQueries, aQueryCount, options,
                                            rootNode, isBatching(),
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

  // Need to filter on parent if any folder is set.
  for (PRInt32 i = 0; i < aQueries.Count(); ++i) {
    if (aQueries[i]->Folders().Length() != 0) {
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
        "SELECT h.id, h.url, h.title AS page_title, h.rev_host, h.visit_count, "
        "h.last_visit_date, f.url, null, null, null, null, null, ") +
        tagsSqlFragment + NS_LITERAL_CSTRING(", h.frecency "
        "FROM moz_places h "
        "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
        // WHERE 1 is a no-op since additonal conditions will start with AND.
        "WHERE 1 "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} ");
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
          "SELECT b2.fk, h.url, COALESCE(b2.title, h.title) AS page_title, "
            "h.rev_host, h.visit_count, h.last_visit_date, f.url, null, b2.id, "
            "b2.dateAdded, b2.lastModified, b2.parent, ") +
            tagsSqlFragment + NS_LITERAL_CSTRING(", h.frecency "
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
          "ORDER BY b2.fk DESC, b2.lastModified DESC");
      }
      else {
        GetTagsSqlFragment(history->GetTagsFolder(),
                           NS_LITERAL_CSTRING("b.fk"),
                           mHasSearchTerms,
                           tagsSqlFragment);
        mQueryString = NS_LITERAL_CSTRING(
          "SELECT b.fk, h.url, COALESCE(b.title, h.title) AS page_title, "
            "h.rev_host, h.visit_count, h.last_visit_date, f.url, null, b.id, "
            "b.dateAdded, b.lastModified, b.parent, ") +
            tagsSqlFragment + NS_LITERAL_CSTRING(", h.frecency "
          "FROM moz_bookmarks b "
          "JOIN moz_places h ON b.fk = h.id "
          "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
          "WHERE NOT EXISTS "
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
    "SELECT h.id, h.url, h.title AS page_title, h.rev_host, h.visit_count, "
      "v.visit_date, f.url, v.session, null, null, null, null, ") +
      tagsSqlFragment + NS_LITERAL_CSTRING(", h.frecency "
    "FROM moz_places h "
    "JOIN moz_historyvisits v ON h.id = v.place_id "
    "LEFT JOIN moz_favicons f ON h.favicon_id = f.id "
    // WHERE 1 is a no-op since additonal conditions will start with AND.
    "WHERE 1 "
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
        // Also, we use day 2 instead of day 1, so that the GMT month is always
        // the same as the local month. (Bug 603002)
        tm.tm_mday = 2;
        tm.tm_month -= MonthIndex;
        // Notice we use GMTParameters because we just want to get the first
        // day of each month.  Using LocalTimeParameters would instead force us
        // to apply a DST correction that we don't really need here.
        PR_NormalizeTime(&tm, PR_GMTParameters);
        // If the container is for a past year, add the year to its title,
        // otherwise just show the month name.
        // Note that tm_month starts from 0, while we need a 1-based index.
        if (tm.tm_year < currentYear) {
          history->GetMonthYear(tm.tm_month + 1, tm.tm_year, dateName);
        }
        else {
          history->GetMonthName(tm.tm_month + 1, dateName);
        }

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
      nsINavHistoryService::TRANSITION_FRAMED_LINK
    );

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

  // If there are additional conditions the query has to join on visits too.
  nsCAutoString visitsJoin;
  nsCAutoString additionalConditions;
  nsCAutoString timeConstraints;
  if (!mConditions.IsEmpty()) {
    visitsJoin.AssignLiteral("JOIN moz_historyvisits v ON v.place_id = h.id ");
    additionalConditions.AssignLiteral("{QUERY_OPTIONS_VISITS} "
                                       "{QUERY_OPTIONS_PLACES} "
                                       "{ADDITIONAL_CONDITIONS} ");
    timeConstraints.AssignLiteral("||'&beginTime='||:begin_time||"
                                    "'&endTime='||:end_time");
  }

  mQueryString = nsPrintfCString(2048,
    "SELECT null, 'place:type=%ld&sort=%ld&domain=&domainIsHost=true'%s, "
           ":localhost, :localhost, null, null, null, null, null, null, null "
    "WHERE EXISTS ( "
      "SELECT h.id FROM moz_places h "
      "%s "
      "WHERE h.hidden = 0 "
        "AND h.visit_count > 0 "
        "AND h.url BETWEEN 'file://' AND 'file:/~' "
      "%s "
      "LIMIT 1 "
    ") "
    "UNION ALL "
    "SELECT null, "
           "'place:type=%ld&sort=%ld&domain='||host||'&domainIsHost=true'%s, "
           "host, host, null, null, null, null, null, null, null "
    "FROM ( "
      "SELECT get_unreversed_host(h.rev_host) AS host "
      "FROM moz_places h "
      "%s "
      "WHERE h.hidden = 0 "
        "AND h.rev_host <> '.' "
        "AND h.visit_count > 0 "
        "%s "
      "GROUP BY h.rev_host "
      "ORDER BY host ASC "
    ") ",
    nsINavHistoryQueryOptions::RESULTS_AS_URI,
    mSortingMode,
    PromiseFlatCString(timeConstraints).get(),
    PromiseFlatCString(visitsJoin).get(),
    PromiseFlatCString(additionalConditions).get(),
    nsINavHistoryQueryOptions::RESULTS_AS_URI,
    mSortingMode,
    PromiseFlatCString(timeConstraints).get(),
    PromiseFlatCString(visitsJoin).get(),
    PromiseFlatCString(additionalConditions).get()
  );

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
           "title, null, null, null, null, null, null, dateAdded, "
           "lastModified, null, null "
    "FROM moz_bookmarks "
    "WHERE parent = %lld",
    nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS,
    nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS,
    history->GetTagsFolder()
  );

  return NS_OK;
}

nsresult
PlacesSQLQueryBuilder::Where()
{

  // Set query options
  nsCAutoString additionalVisitsConditions;
  nsCAutoString additionalPlacesConditions;

  if (mRedirectsMode == nsINavHistoryQueryOptions::REDIRECTS_MODE_SOURCE) {
    // At least one visit that is not a redirect target should exist.
    additionalVisitsConditions += NS_LITERAL_CSTRING(
      "AND visit_type NOT IN ") +
      nsPrintfCString("(%d,%d) ", nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
                                  nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY);
  }
  else if (mRedirectsMode == nsINavHistoryQueryOptions::REDIRECTS_MODE_TARGET) {
    // At least one visit that is not a redirect source should exist.
    additionalPlacesConditions += nsPrintfCString(1024,
      "AND EXISTS ( "
        "SELECT id "
        "FROM moz_historyvisits v "
        "WHERE place_id = h.id "
          "AND NOT EXISTS(SELECT id FROM moz_historyvisits "
                         "WHERE from_visit = v.id AND visit_type IN (%d,%d)) "
      ") ",
      nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT,
      nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY);
  }

  if (!mIncludeHidden) {
    additionalPlacesConditions += NS_LITERAL_CSTRING("AND hidden = 0 ");
  }

  if (mQueryType == nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY) {
    // last_visit_date is updated for any kind of visit, so it's a good
    // indicator whether the page has visits.
    additionalPlacesConditions += NS_LITERAL_CSTRING(
      "AND last_visit_date NOTNULL "
    );
  }

  if (mResultType == nsINavHistoryQueryOptions::RESULTS_AS_URI &&
      !additionalVisitsConditions.IsEmpty()) {
    // URI results don't join on visits.
    nsCAutoString tmp = additionalVisitsConditions;
    additionalVisitsConditions = "AND EXISTS (SELECT 1 FROM moz_historyvisits WHERE place_id = h.id ";
    additionalVisitsConditions.Append(tmp);
    additionalVisitsConditions.Append("LIMIT 1)");
  }

  mQueryString.ReplaceSubstring("{QUERY_OPTIONS_VISITS}",
                                additionalVisitsConditions.get());
  mQueryString.ReplaceSubstring("{QUERY_OPTIONS_PLACES}",
                                additionalPlacesConditions.get());

  // If we used WHERE already, we inject the conditions 
  // in place of {ADDITIONAL_CONDITIONS}
  if (mQueryString.Find("{ADDITIONAL_CONDITIONS}", 0) != kNotFound) {
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
      // Ensure sorting does not change based on tables status.
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
    case nsINavHistoryQueryOptions::SORT_BY_FRECENCY_ASCENDING:
        OrderByColumnIndexAsc(nsNavHistory::kGetInfoIndex_Frecency);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_FRECENCY_DESCENDING:
        OrderByColumnIndexDesc(nsNavHistory::kGetInfoIndex_Frecency);
      break;
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
               sortingMode <= nsINavHistoryQueryOptions::SORT_BY_FRECENCY_DESCENDING,
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
      "SELECT h.id, h.url, h.title AS page_title, h.rev_host, h.visit_count, h.last_visit_date, "
          "f.url, null, null, null, null, null, ") +
          tagsSqlFragment + NS_LITERAL_CSTRING(", h.frecency "
        "FROM moz_places h "
        "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
        "WHERE h.hidden = 0 "
          "AND EXISTS (SELECT id FROM moz_historyvisits WHERE place_id = h.id "
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
      // At least one visit that is not a redirect target should exist.
      additionalQueryOptions +=  nsPrintfCString(256,
        "AND EXISTS ( "
          "SELECT id "
          "FROM moz_historyvisits "
          "WHERE place_id = h.id "
            "AND visit_type NOT IN (%d,%d)"
        ") ",
        TRANSITION_REDIRECT_PERMANENT,
        TRANSITION_REDIRECT_TEMPORARY);
    }
    else if (aOptions->RedirectsMode() ==
              nsINavHistoryQueryOptions::REDIRECTS_MODE_TARGET) {
      // At least one visit that is not a redirect source should exist.
      additionalQueryOptions += nsPrintfCString(1024,
        "AND EXISTS ( "
          "SELECT id "
          "FROM moz_historyvisits v "
          "WHERE place_id = h.id "
            "AND NOT EXISTS(SELECT id FROM moz_historyvisits "
                           "WHERE from_visit = v.id AND visit_type IN (%d,%d)) "
        ") ",
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
      DebugOnly<nsresult> rv = mBatchDBTransaction->Commit();
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


/**
 * This was once used when the new window was set to "previous page".
 * Currently it is still referenced by browser.startup.page == 2, but that value
 * is not selectable from the UI.
 * TODO: Should be deprecated? There is no fast alternative to get this info.
 */
NS_IMETHODIMP
nsNavHistory::GetLastPageVisited(nsACString & aLastPageVisited)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT url FROM moz_places "
      "WHERE hidden = 0 "
        "AND last_visit_date NOTNULL "
      "ORDER BY last_visit_date DESC "),
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
      "DELETE FROM moz_historyvisits WHERE place_id IN (") +
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
      "UPDATE moz_places "
      "SET frecency = -MAX(visit_count, 1) "
      "WHERE id IN ( "
        "SELECT h.id " 
        "FROM moz_places h "
        "WHERE h.id IN ( ") + aPlaceIdsQueryString + NS_LITERAL_CSTRING(") "
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

  // Collect about-to-be-deleted URIs to notify onDeleteURI.
  nsCOMPtr<mozIStorageStatement> stmt;
  mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT h.id, h.url, h.guid, "
           "(SUBSTR(h.url, 1, 6) <> 'place:' "
           " AND NOT EXISTS (SELECT b.id FROM moz_bookmarks b "
                            "WHERE b.fk = h.id LIMIT 1)) as whole_entry "
    "FROM moz_places h "
    "WHERE h.id IN ( ") + aPlaceIdsQueryString + NS_LITERAL_CSTRING(") "
  ), getter_AddRefs(stmt));
  NS_ENSURE_STATE(stmt);
  nsCString filteredPlaceIds;
  nsCOMArray<nsIURI> URIs;
  nsTArray<nsCString> GUIDs;
  PRBool hasMore;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
    PRInt64 placeId;
    nsresult rv = stmt->GetInt64(0, &placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString URLString;
    rv = stmt->GetUTF8String(1, URLString);
    nsCString guid;
    rv = stmt->GetUTF8String(2, guid);
    PRInt32 wholeEntry;
    rv = stmt->GetInt32(3, &wholeEntry);
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), URLString);
    NS_ENSURE_SUCCESS(rv, rv);
    if (wholeEntry) {
      if (!filteredPlaceIds.IsEmpty()) {
        filteredPlaceIds.AppendLiteral(",");
      }
      filteredPlaceIds.AppendInt(placeId);
      URIs.AppendObject(uri);
      GUIDs.AppendElement(guid);
      // Notify we are about to remove this uri.
      NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                       nsINavHistoryObserver,
                       OnBeforeDeleteURI(uri, guid, nsINavHistoryObserver::REASON_DELETED));
    }
    else {
      // Notify that we will delete all visits for this page, but not the page
      // itself, since it's bookmarked or a place: query.
      NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                       nsINavHistoryObserver,
                       OnDeleteVisits(uri, 0, guid, nsINavHistoryObserver::REASON_DELETED));
    }
  }

  // if the entry is not bookmarked and is not a place: uri
  // then we can remove it from moz_places.
  // Note that we do NOT delete favicons. Any unreferenced favicons will be
  // deleted next time the browser is shut down.
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_places WHERE id IN ( "
        ) + filteredPlaceIds + NS_LITERAL_CSTRING(
      ") "));
  NS_ENSURE_SUCCESS(rv, rv);

  // If we have removed all visits to a livemark's child, we need to fix its
  // frecency, or it would appear in the url bar autocomplete.
  // XXX this might be dog slow, further degrading delete perf.
  rv = FixInvalidFrecenciesForExcludedPlaces();
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally notify about the removed URIs.
  for (PRInt32 i = 0; i < URIs.Count(); ++i) {
    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavHistoryObserver,
                     OnDeleteURI(URIs[i], GUIDs[i], nsINavHistoryObserver::REASON_DELETED));
  }

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
    nsCAutoString guid;
    rv = GetIdForPage(aURIs[i], &placeId, guid);
    NS_ENSURE_SUCCESS(rv, rv);
    if (placeId != 0) {
      if (!deletePlaceIdsQueryString.IsEmpty())
        deletePlaceIdsQueryString.AppendLiteral(",");
      deletePlaceIdsQueryString.AppendInt(placeId);
    }
  }

  // force a full refresh calling onEndUpdateBatch (will call Refresh())
  if (aDoBatchNotify)
    UpdateBatchScoper batch(*this); // sends Begin/EndUpdateBatch to observers

  rv = RemovePagesInternal(deletePlaceIdsQueryString);
  NS_ENSURE_SUCCESS(rv, rv);

  // Clear the registered embed visits.
  clearEmbedVisits();

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

  nsIURI** URIs = &aURI;
  nsresult rv = RemovePages(URIs, 1, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

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
      "SELECT id FROM moz_places "
      "WHERE ") + conditionString,
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

  // Clear the registered embed visits.
  clearEmbedVisits();

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
      "SELECT h.id FROM moz_places h WHERE "
        "EXISTS "
          "(SELECT id FROM moz_historyvisits v WHERE v.place_id = h.id "
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

  // force a full refresh calling onEndUpdateBatch (will call Refresh())
  UpdateBatchScoper batch(*this); // sends Begin/EndUpdateBatch to observers

  rv = RemovePagesInternal(deletePlaceIdsQueryString);
  NS_ENSURE_SUCCESS(rv, rv);

  // Clear the registered embed visits.
  clearEmbedVisits();

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
        "FROM moz_historyvisits "
        "WHERE :from_date <= visit_date AND visit_date <= :to_date "
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
      "DELETE FROM moz_historyvisits "
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

  // Clear the registered embed visits.
  clearEmbedVisits();

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
    "UPDATE moz_places SET frecency = -MAX(visit_count, 1) "
    "WHERE id IN(SELECT b.fk FROM moz_bookmarks b WHERE b.fk NOTNULL)"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Expire visits, then let the paranoid functions do the cleanup for us.
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_historyvisits"));
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

  // Clear the registered embed visits.
  clearEmbedVisits();

  // Invalidate the cached value for whether there's history or not.
  mHasHistoryEntries = -1;

  // Expiration will take care of orphans.
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver, OnClearHistory());

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

  nsCOMPtr<mozIPlacesAutoComplete> ac =
    do_GetService("@mozilla.org/autocomplete/search;1?name=history");
  NS_ENSURE_STATE(ac);

  nsresult rv = ac->RegisterOpenPage(aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsNavHistory::UnregisterOpenPage(nsIURI* aURI)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  nsCOMPtr<mozIPlacesAutoComplete> ac =
    do_GetService("@mozilla.org/autocomplete/search;1?name=history");
  NS_ENSURE_STATE(ac);

  nsresult rv = ac->UnregisterOpenPage(aURI);
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
//    each time calling InternalAdd to create a new history entry.

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

  PRUint32 transitionType = 0;

  if (aReferrerURI) {
  // This page had a referrer.

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

  *_retval = hasEmbedVisit(aURI) ? PR_TRUE : IsURIStringVisited(utf8URISpec);
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

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetURLPageInfo);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResults = PR_FALSE;
  rv = stmt->ExecuteStep(&hasResults);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasResults) {
    aTitle.SetIsVoid(PR_TRUE);
    return NS_OK; // Not found, return a void string.
  }

  rv = stmt->GetString(nsNavHistory::kGetInfoIndex_Title, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
//// mozIStorageVacuumParticipant

NS_IMETHODIMP
nsNavHistory::GetDatabaseConnection(mozIStorageConnection** _DBConnection)
{
  return GetDBConnection(_DBConnection);
}


NS_IMETHODIMP
nsNavHistory::GetExpectedDatabasePageSize(PRInt32* _expectedPageSize)
{
  *_expectedPageSize = mozIStorageConnection::DEFAULT_PAGE_SIZE;
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistory::OnBeginVacuum(PRBool* _vacuumGranted)
{
  // TODO: Check if we have to deny the vacuum in some heavy-load case.
  // We could maybe want to do that during batches?
  *_vacuumGranted = PR_TRUE;
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistory::OnEndVacuum(PRBool aSucceeded)
{
  NS_WARN_IF_FALSE(aSucceeded, "Places.sqlite vacuum failed.");
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
//// nsIDownloadHistory

NS_IMETHODIMP
nsNavHistory::AddDownload(nsIURI* aSource, nsIURI* aReferrer,
                          PRTime aStartTime, nsIURI* aDestination)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aSource);

  // don't add when history is disabled and silently fail
  if (IsHistoryDisabled())
    return NS_OK;

  PRInt64 visitID;
  nsresult rv = AddVisit(aSource, aStartTime, aReferrer, TRANSITION_DOWNLOAD,
                         PR_FALSE, 0, &visitID);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aDestination) {
    return NS_OK;
  }

  // Exit silently if the download destination is not a local file.
  nsCOMPtr<nsIFileURL> destinationFileURL = do_QueryInterface(aDestination);
  if (!destinationFileURL) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> destinationFile;
  rv = destinationFileURL->GetFile(getter_AddRefs(destinationFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString destinationFileName;
  rv = destinationFile->GetLeafName(destinationFileName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString destinationURISpec;
  rv = destinationFileURL->GetSpec(destinationURISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  // Use annotations for storing the additional download metadata.
  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);

  (void)annosvc->SetPageAnnotationString(
    aSource,
    DESTINATIONFILEURI_ANNO,
    NS_ConvertUTF8toUTF16(destinationURISpec),
    0,
    nsIAnnotationService::EXPIRE_WITH_HISTORY
  );

  (void)annosvc->SetPageAnnotationString(
    aSource,
    DESTINATIONFILENAME_ANNO,
    destinationFileName,
    0,
    nsIAnnotationService::EXPIRE_WITH_HISTORY
  );

  // In case we are downloading a file that does not correspond to a web
  // page for which the title is present, we populate the otherwise empty
  // history title with the name of the destination file, to allow it to be
  // visible and searchable in history results.
  nsAutoString title;
  if (NS_SUCCEEDED(GetPageTitle(aSource, title)) && title.IsEmpty()) {
    (void)SetPageTitle(aSource, destinationFileName);
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
//// nsPIPlacesDatabase

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


NS_IMETHODIMP
nsNavHistory::AsyncExecuteLegacyQueries(nsINavHistoryQuery** aQueries,
                                        PRUint32 aQueryCount,
                                        nsINavHistoryQueryOptions* aOptions,
                                        mozIStorageStatementCallback* aCallback,
                                        mozIStoragePendingStatement** _stmt)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aQueries);
  NS_ENSURE_ARG(aOptions);
  NS_ENSURE_ARG(aCallback);
  NS_ENSURE_ARG_POINTER(_stmt);

  nsCOMArray<nsNavHistoryQuery> queries;
  for (PRUint32 i = 0; i < aQueryCount; i ++) {
    nsCOMPtr<nsNavHistoryQuery> query = do_QueryInterface(aQueries[i]);
    NS_ENSURE_STATE(query);
    queries.AppendObject(query);
  }
  NS_ENSURE_ARG_MIN(queries.Count(), 1);

  nsCOMPtr<nsNavHistoryQueryOptions> options = do_QueryInterface(aOptions);
  NS_ENSURE_ARG(options);

  nsCString queryString;
  PRBool paramsPresent = PR_FALSE;
  nsNavHistory::StringHash addParams;
  addParams.Init(HISTORY_DATE_CONT_MAX);
  nsresult rv = ConstructQueryString(queries, options, queryString,
                                     paramsPresent, addParams);
  NS_ENSURE_SUCCESS(rv,rv);

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
    for (i = 0; i < queries.Count(); i++) {
      rv = BindQueryClauseParameters(statement, i, queries[i], options);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  addParams.EnumerateRead(BindAdditionalParameter, statement.get());

  rv = statement->ExecuteAsync(aCallback, _stmt);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsPIPlacesHistoryListenersNotifier ******************************************

NS_IMETHODIMP
nsNavHistory::NotifyOnPageExpired(nsIURI *aURI, PRTime aVisitTime,
                                  PRBool aWholeEntry, const nsACString& aGUID,
                                  PRUint16 aReason)
{
  // Invalidate the cached value for whether there's history or not.
  mHasHistoryEntries = -1;

  MOZ_ASSERT(!aGUID.IsEmpty());
  if (aWholeEntry) {
    // Notify our observers that the page has been removed.
    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavHistoryObserver, OnDeleteURI(aURI, aGUID, aReason));
  }
  else {
    // Notify our observers that some visits for the page have been removed.
    NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                     nsINavHistoryObserver,
                     OnDeleteVisits(aURI, aVisitTime, aGUID, aReason));
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
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
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
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
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

#ifdef DEBUG
    { // Sanity check that all places have guids.
      nsCOMPtr<mozIStorageStatement> stmt;
      nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT * "
        "FROM moz_places "
        "WHERE guid IS NULL "
      ), getter_AddRefs(stmt));
      NS_ENSURE_SUCCESS(rv, rv);

      PRBool haveNullGuids;
      rv = stmt->ExecuteStep(&haveNullGuids);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ASSERTION(!haveNullGuids,
                   "Someone added a place without adding a GUID!");
    }
#endif

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
  }

  else if (strcmp(aTopic, NS_PRIVATE_BROWSING_SWITCH_TOPIC) == 0) {
    if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).Equals(aData)) {
      mInPrivateBrowsing = PR_TRUE;
    }
    else if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).Equals(aData)) {
      mInPrivateBrowsing = PR_FALSE;
    }
  }

  return NS_OK;
}


nsresult
nsNavHistory::DecayFrecency()
{
  nsresult rv = FixInvalidFrecencies();
  NS_ENSURE_SUCCESS(rv, rv);

  // Globally decay places frecency rankings to estimate reduced frecency
  // values of pages that haven't been visited for a while, i.e., they do
  // not get an updated frecency.  A scaling factor of .975 results in .5 the
  // original value after 28 days.
  nsCOMPtr<mozIStorageAsyncStatement> decayFrecency;
  rv = mDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places SET frecency = ROUND(frecency * .975) "
      "WHERE frecency > 0"),
    getter_AddRefs(decayFrecency));
  NS_ENSURE_SUCCESS(rv, rv);

  // Decay potentially unused adaptive entries (e.g. those that are at 1)
  // to allow better chances for new entries that will start at 1.
  nsCOMPtr<mozIStorageAsyncStatement> decayAdaptive;
  rv = mDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_inputhistory SET use_count = use_count * .975"),
    getter_AddRefs(decayAdaptive));
  NS_ENSURE_SUCCESS(rv, rv);

  // Delete any adaptive entries that won't help in ordering anymore.
  nsCOMPtr<mozIStorageAsyncStatement> deleteAdaptive;
  rv = mDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
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

  if ((NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt) ||
    (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt)) {
    clause.Condition("EXISTS (SELECT 1 FROM moz_historyvisits "
                              "WHERE place_id = h.id");
    // begin time
    if (NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt) 
      clause.Condition("visit_date >=").Param(":begin_time");
    // end time
    if (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt)
      clause.Condition("visit_date <=").Param(":end_time");
    clause.Str(" LIMIT 1)");
  }

  // search terms
  PRBool hasSearchTerms;
  if (NS_SUCCEEDED(aQuery->GetHasSearchTerms(&hasSearchTerms)) && hasSearchTerms) {
    // Re-use the autocomplete_match function.  Setting the behavior to 0
    // it can match everything and work as a nice case insensitive comparator.
    clause.Condition("AUTOCOMPLETE_MATCH(").Param(":search_string")
          .Str(", h.url, page_title, tags, ")
          .Str(nsPrintfCString(17, "0, 0, 0, 0, %d, 0)",
                               mozIPlacesAutoComplete::MATCH_ANYWHERE_UNMODIFIED).get());
  }

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
  for (PRUint32 i = 0; i < transitions.Length(); ++i) {
    nsPrintfCString param(":transition%d_", i);
    clause.Condition("EXISTS (SELECT 1 FROM moz_historyvisits "
                             "WHERE place_id = h.id AND visit_type = "
              ).Param(param.get()).Str(" LIMIT 1)");
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

  // search terms
  if (NS_SUCCEEDED(aQuery->GetHasSearchTerms(&hasIt)) && hasIt) {
    rv = statement->BindStringByName(
      NS_LITERAL_CSTRING("search_string") + qIndex,
      aQuery->SearchTerms()
    );
    NS_ENSURE_SUCCESS(rv, rv);
  }

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
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetItemsWithAnno);
    rv = stmt->BindUTF8StringByName(
      NS_LITERAL_CSTRING("anno_name"), parentAnnotationToExclude
    );
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore = PR_FALSE;
    while (NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore) {
      PRInt64 folderId = 0;
      rv = stmt->GetInt64(0, &folderId);
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
        // cache or it has no parent.
        if (excludeFolders[queryIndex]->Contains(parentId) || parentId == -1) {
          continue;
        }

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

void
nsNavHistory::registerEmbedVisit(nsIURI* aURI,
                                 PRInt64 aTime)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  VisitHashKey* visit = mEmbedVisits.PutEntry(aURI);
  if (!visit) {
    NS_WARNING("Unable to register a EMBED visit.");
    return;
  }
  visit->visitTime = aTime;
}

bool
nsNavHistory::hasEmbedVisit(nsIURI* aURI) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  return !!mEmbedVisits.GetEntry(aURI);
}

void
nsNavHistory::clearEmbedVisits() {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  mEmbedVisits.Clear();
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
  // If this failed the query does not parse correctly, let the error pass and
  // handle it later.
  if (NS_SUCCEEDED(rv)) {
    // Check if this is a folder shortcut, so we can take a faster path.
    PRInt64 folderId = GetSimpleBookmarksQueryFolder(queries, options);
    if (folderId) {
      nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
      NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

      // This AddRefs for us.
      rv = bookmarks->ResultNodeForContainer(folderId, options, aNode);
      // If this failed the shortcut is pointing to nowhere, let the error pass
      // and handle it later.
      if (NS_SUCCEEDED(rv)) {
        // This is the query itemId, and is what is exposed by node.itemId.
        (*aNode)->GetAsFolder()->mQueryItemId = itemId;

        // Use the query item title, unless it's void (in that case use the 
        // concrete folder title).
        if (!aTitle.IsVoid()) {
          (*aNode)->mTitle = aTitle;
        }
      }
    }
    else {
      // This is a regular query.
      *aNode = new nsNavHistoryQueryResultNode(aTitle, EmptyCString(), aTime,
                                               queries, options);
      (*aNode)->mItemId = itemId;
      NS_ADDREF(*aNode);
    }
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("Generating a generic empty node for a broken query!");
    // This is a broken query, that either did not parse or points to not
    // existing data.  We don't want to return failure since that will kill the
    // whole result.  Instead make a generic empty query node.
    *aNode = new nsNavHistoryQueryResultNode(aTitle, aFavicon, aURI);
    (*aNode)->mItemId = itemId;
    // This is a perf hack to generate an empty query that skips filtering.
    (*aNode)->GetAsQuery()->Options()->SetExcludeItems(PR_TRUE);
    NS_ADDREF(*aNode);
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
      statement = GetStatement(mDBVisitToVisitResult);
      break;

    case nsNavHistoryQueryOptions::RESULTS_AS_URI:
      // URL results - want last visit time
    statement = GetStatement(mDBVisitToURLResult);  
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
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBBookmarkToUrlResult);
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

nsresult
nsNavHistory::URIToResultNode(nsIURI* aURI,
                              nsNavHistoryQueryOptions* aOptions,
                              nsNavHistoryResultNode** aResult)
{
  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBUrlToUrlResult);
  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  rv = stmt->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasMore) {
    NS_NOTREACHED("Trying to get a result node for an invalid url");
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(stmt, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return RowToResult(row, aOptions, aResult);
}

void
nsNavHistory::SendPageChangedNotification(nsIURI* aURI,
                                          PRUint32 aChangedAttribute,
                                          const nsAString& aNewValue,
                                          const nsACString& aGUID)
{
  MOZ_ASSERT(!aGUID.IsEmpty());
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver,
                   OnPageChanged(aURI, aChangedAttribute, aNewValue, aGUID));
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
  if (bundle) {
    nsAutoString intString;
    intString.AppendInt(aInt);
    const PRUnichar* strings[1] = { intString.get() };
    nsXPIDLString value;
    nsresult rv = bundle->FormatStringFromName(aName, strings,
                                               1, getter_Copies(value));
    if (NS_SUCCEEDED(rv)) {
      CopyUTF16toUTF8(value, aResult);
      return;
    }
  }
  CopyUTF16toUTF8(nsDependentString(aName), aResult);
}

void
nsNavHistory::GetStringFromName(const PRUnichar *aName, nsACString& aResult)
{
  nsIStringBundle *bundle = GetBundle();
  if (bundle) {
    nsXPIDLString value;
    nsresult rv = bundle->GetStringFromName(aName, getter_Copies(value));
    if (NS_SUCCEEDED(rv)) {
      CopyUTF16toUTF8(value, aResult);
      return;
    }
  }
  CopyUTF16toUTF8(nsDependentString(aName), aResult);
}

void
nsNavHistory::GetMonthName(PRInt32 aIndex, nsACString& aResult)
{
  nsIStringBundle *bundle = GetDateFormatBundle();
  if (bundle) {
    nsCString name = nsPrintfCString("month.%d.name", aIndex);
    nsXPIDLString value;
    nsresult rv = bundle->GetStringFromName(NS_ConvertUTF8toUTF16(name).get(),
                                            getter_Copies(value));
    if (NS_SUCCEEDED(rv)) {
      CopyUTF16toUTF8(value, aResult);
      return;
    }
  }
  aResult = nsPrintfCString("[%d]", aIndex);
}

void
nsNavHistory::GetMonthYear(PRInt32 aMonth, PRInt32 aYear, nsACString& aResult)
{
  nsIStringBundle *bundle = GetBundle();
  if (bundle) {
    nsCAutoString monthName;
    GetMonthName(aMonth, monthName);
    nsAutoString yearString;
    yearString.AppendInt(aYear);
    const PRUnichar* strings[2] = {
      NS_ConvertUTF8toUTF16(monthName).get()
    , yearString.get()
    };
    nsXPIDLString value;
    if (NS_SUCCEEDED(bundle->FormatStringFromName(
          NS_LITERAL_STRING("finduri-MonthYear").get(), strings, 2,
          getter_Copies(value)
        ))) {
      CopyUTF16toUTF8(value, aResult);
      return;
    }
  }
  aResult.AppendLiteral("finduri-MonthYear");
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

  // Make sure the page exists by fetching its GUID and the old title.
  nsAutoString title;
  nsCAutoString guid;
  {
    DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBGetURLPageInfo);
    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool hasURL = PR_FALSE;
    rv = stmt->ExecuteStep(&hasURL);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!hasURL) {
      // If the url is unknown, either the page had an embed visit, or we have
      // never seen it.  While the former is fine, the latter is an error.
      if (hasEmbedVisit(aURI)) {
        return NS_OK;
      }
      return NS_ERROR_NOT_AVAILABLE;
    }

    rv = stmt->GetString(kGetInfoIndex_Title, title);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetUTF8String(5, guid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // It is actually common to set the title to be the same thing it used to
  // be. For example, going to any web page will always cause a title to be set,
  // even though it will often be unchanged since the last visit. In these
  // cases, we can avoid DB writing and (most significantly) observer overhead.
  if ((aTitle.IsVoid() && title.IsVoid()) || aTitle == title)
    return NS_OK;

  DECLARE_AND_ASSIGN_SCOPED_LAZY_STMT(stmt, mDBSetPlaceTitle);
  if (aTitle.IsVoid())
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("page_title"));
  else {
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("page_title"),
                                StringHead(aTitle, TITLE_LENGTH_MAX));
  }
  NS_ENSURE_SUCCESS(rv, rv);
  rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(!guid.IsEmpty());
  NOTIFY_OBSERVERS(mCanNotify, mCacheObservers, mObservers,
                   nsINavHistoryObserver, OnTitleChanged(aURI, aTitle, guid));

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


// if we calculated a non-zero frecency we should unhide this place
// so that previously hidden (non-livebookmark item) bookmarks 
// will now appear in autocomplete
// if we calculated a zero frecency, we re-use the old hidden value.
nsresult
nsNavHistory::UpdateFrecency(PRInt64 aPlaceId)
{
  DECLARE_AND_ASSIGN_LAZY_STMT(updateFrecencyStmt, mDBUpdateFrecency);
  DECLARE_AND_ASSIGN_LAZY_STMT(updateHiddenStmt, mDBUpdateHiddenOnFrecency);

#define ASYNC_BIND(_stmt) \
  PR_BEGIN_MACRO \
    nsCOMPtr<mozIStorageBindingParamsArray> paramsArray; \
    nsresult rv = _stmt->NewBindingParamsArray(getter_AddRefs(paramsArray)); \
    NS_ENSURE_SUCCESS(rv, rv); \
    nsCOMPtr<mozIStorageBindingParams> params; \
    rv = paramsArray->NewBindingParams(getter_AddRefs(params)); \
    NS_ENSURE_SUCCESS(rv, rv); \
    rv = params->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlaceId); \
    NS_ENSURE_SUCCESS(rv, rv); \
    rv = paramsArray->AddParams(params); \
    NS_ENSURE_SUCCESS(rv, rv); \
    rv = _stmt->BindParameters(paramsArray); \
    NS_ENSURE_SUCCESS(rv, rv); \
  PR_END_MACRO

  ASYNC_BIND(updateFrecencyStmt);
  ASYNC_BIND(updateHiddenStmt);

  mozIStorageBaseStatement *stmts[] = {
    updateFrecencyStmt
  , updateHiddenStmt
  };

  nsCOMPtr<AsyncStatementCallbackNotifier> callback =
    new AsyncStatementCallbackNotifier(TOPIC_FRECENCY_UPDATED);
  nsCOMPtr<mozIStoragePendingStatement> ps;
  nsresult rv = mDBConn->ExecuteAsync(stmts, NS_ARRAY_LENGTH(stmts), callback,
                                      getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
#undef ASYNC_BIND
}


nsresult
nsNavHistory::FixInvalidFrecencies()
{
  // Find all places with invalid frecencies (frecency < 0) that occur when:
  // 1) we've done "clear private data"
  // 2) we've expired or deleted visits
  // 3) we've migrated from an older version, before global frecency
  //
  // From older versions, unmigrated bookmarks might be hidden, so we can't
  // exclude hidden places (by doing "WHERE hidden = 0") from our query, as we
  // want to calculate the frecency for those places and unhide them (if they
  // are not livemark items and not place: queries.)
  //
  // Note, we are not limiting ourselves to places with visits because we may
  // not have any if the place is a bookmark and we expired or deleted all the
  // visits.
  nsCOMPtr<mozIStorageStatement> fixInvalidFrecenciesStmt;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_places "
      "SET frecency = CALCULATE_FRECENCY(id) "
      "WHERE frecency < 0"),
    getter_AddRefs(fixInvalidFrecenciesStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<AsyncStatementCallbackNotifier> callback =
    new AsyncStatementCallbackNotifier(TOPIC_FRECENCY_UPDATED);
  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = fixInvalidFrecenciesStmt->ExecuteAsync(callback, getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


#ifdef MOZ_XUL

nsresult
nsNavHistory::AutoCompleteFeedback(PRInt32 aIndex,
                                   nsIAutoCompleteController *aController)
{
  // We do not track user choices in the location bar in private browsing mode.
  if (InPrivateBrowsingMode())
    return NS_OK;

  DECLARE_AND_ASSIGN_LAZY_STMT(stmt, mDBFeedbackIncrease);

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
  nsCOMPtr<AsyncStatementCallbackNotifier> callback =
    new AsyncStatementCallbackNotifier(TOPIC_AUTOCOMPLETE_FEEDBACK_UPDATED);
  nsCOMPtr<mozIStoragePendingStatement> canceler;
  rv = stmt->ExecuteAsync(callback, getter_AddRefs(canceler));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
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
      services::GetStringBundleService();
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
      services::GetStringBundleService();
    NS_ENSURE_TRUE(bundleService, nsnull);
    nsresult rv = bundleService->CreateBundle(
        "chrome://global/locale/dateFormat.properties",
        getter_AddRefs(mDateFormatBundle));
    NS_ENSURE_SUCCESS(rv, nsnull);
  }
  return mDateFormatBundle;
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
    mDBUrlToUrlResult,
    mDBUpdateFrecency,
    mDBUpdateHiddenOnFrecency,
  };

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(stmts); i++) {
    nsresult rv = nsNavHistory::FinalizeStatement(stmts[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Finalize the statementCaches on the correct threads.
  mStatements.FinalizeStatements();

  nsRefPtr<FinalizeStatementCacheProxy<mozIStorageStatement> > event =
    new FinalizeStatementCacheProxy<mozIStorageStatement>(
        mAsyncThreadStatements, NS_ISUPPORTS_CAST(nsINavHistoryService*, this));
  nsCOMPtr<nsIEventTarget> target = do_GetInterface(mDBConn);
  NS_ENSURE_TRUE(target, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

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
