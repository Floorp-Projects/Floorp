/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"

#include "nsNavHistory.h"

#include "mozIPlacesAutoComplete.h"
#include "nsNavBookmarks.h"
#include "nsAnnotationService.h"
#include "nsFaviconService.h"
#include "nsPlacesMacros.h"
#include "nsPlacesTriggers.h"
#include "DateTimeFormat.h"
#include "History.h"
#include "Helpers.h"

#include "nsTArray.h"
#include "nsCollationCID.h"
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
#include "nsIIDNService.h"
#include "nsQueryObject.h"
#include "nsThreadUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsMathUtils.h"
#include "mozilla/storage.h"
#include "mozilla/Preferences.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::places;

// The maximum number of things that we will store in the recent events list
// before calling ExpireNonrecentEvents. This number should be big enough so it
// is very difficult to get that many unconsumed events (for example, typed but
// never visited) in the RECENT_EVENT_THRESHOLD. Otherwise, we'll start
// checking each one for every page visit, which will be somewhat slower.
#define RECENT_EVENT_QUEUE_MAX_LENGTH 128

// preference ID strings
#define PREF_HISTORY_ENABLED "places.history.enabled"
#define PREF_MATCH_DIACRITICS "places.search.matchDiacritics"

#define PREF_FREC_NUM_VISITS "places.frecency.numVisits"
#define PREF_FREC_NUM_VISITS_DEF 10
#define PREF_FREC_FIRST_BUCKET_CUTOFF "places.frecency.firstBucketCutoff"
#define PREF_FREC_FIRST_BUCKET_CUTOFF_DEF 4
#define PREF_FREC_SECOND_BUCKET_CUTOFF "places.frecency.secondBucketCutoff"
#define PREF_FREC_SECOND_BUCKET_CUTOFF_DEF 14
#define PREF_FREC_THIRD_BUCKET_CUTOFF "places.frecency.thirdBucketCutoff"
#define PREF_FREC_THIRD_BUCKET_CUTOFF_DEF 31
#define PREF_FREC_FOURTH_BUCKET_CUTOFF "places.frecency.fourthBucketCutoff"
#define PREF_FREC_FOURTH_BUCKET_CUTOFF_DEF 90
#define PREF_FREC_FIRST_BUCKET_WEIGHT "places.frecency.firstBucketWeight"
#define PREF_FREC_FIRST_BUCKET_WEIGHT_DEF 100
#define PREF_FREC_SECOND_BUCKET_WEIGHT "places.frecency.secondBucketWeight"
#define PREF_FREC_SECOND_BUCKET_WEIGHT_DEF 70
#define PREF_FREC_THIRD_BUCKET_WEIGHT "places.frecency.thirdBucketWeight"
#define PREF_FREC_THIRD_BUCKET_WEIGHT_DEF 50
#define PREF_FREC_FOURTH_BUCKET_WEIGHT "places.frecency.fourthBucketWeight"
#define PREF_FREC_FOURTH_BUCKET_WEIGHT_DEF 30
#define PREF_FREC_DEFAULT_BUCKET_WEIGHT "places.frecency.defaultBucketWeight"
#define PREF_FREC_DEFAULT_BUCKET_WEIGHT_DEF 10
#define PREF_FREC_EMBED_VISIT_BONUS "places.frecency.embedVisitBonus"
#define PREF_FREC_EMBED_VISIT_BONUS_DEF 0
#define PREF_FREC_FRAMED_LINK_VISIT_BONUS "places.frecency.framedLinkVisitBonus"
#define PREF_FREC_FRAMED_LINK_VISIT_BONUS_DEF 0
#define PREF_FREC_LINK_VISIT_BONUS "places.frecency.linkVisitBonus"
#define PREF_FREC_LINK_VISIT_BONUS_DEF 100
#define PREF_FREC_TYPED_VISIT_BONUS "places.frecency.typedVisitBonus"
#define PREF_FREC_TYPED_VISIT_BONUS_DEF 2000
#define PREF_FREC_BOOKMARK_VISIT_BONUS "places.frecency.bookmarkVisitBonus"
#define PREF_FREC_BOOKMARK_VISIT_BONUS_DEF 75
#define PREF_FREC_DOWNLOAD_VISIT_BONUS "places.frecency.downloadVisitBonus"
#define PREF_FREC_DOWNLOAD_VISIT_BONUS_DEF 0
#define PREF_FREC_PERM_REDIRECT_VISIT_BONUS \
  "places.frecency.permRedirectVisitBonus"
#define PREF_FREC_PERM_REDIRECT_VISIT_BONUS_DEF 0
#define PREF_FREC_TEMP_REDIRECT_VISIT_BONUS \
  "places.frecency.tempRedirectVisitBonus"
#define PREF_FREC_TEMP_REDIRECT_VISIT_BONUS_DEF 0
#define PREF_FREC_REDIR_SOURCE_VISIT_BONUS \
  "places.frecency.redirectSourceVisitBonus"
#define PREF_FREC_REDIR_SOURCE_VISIT_BONUS_DEF 25
#define PREF_FREC_DEFAULT_VISIT_BONUS "places.frecency.defaultVisitBonus"
#define PREF_FREC_DEFAULT_VISIT_BONUS_DEF 0
#define PREF_FREC_UNVISITED_BOOKMARK_BONUS \
  "places.frecency.unvisitedBookmarkBonus"
#define PREF_FREC_UNVISITED_BOOKMARK_BONUS_DEF 140
#define PREF_FREC_UNVISITED_TYPED_BONUS "places.frecency.unvisitedTypedBonus"
#define PREF_FREC_UNVISITED_TYPED_BONUS_DEF 200
#define PREF_FREC_RELOAD_VISIT_BONUS "places.frecency.reloadVisitBonus"
#define PREF_FREC_RELOAD_VISIT_BONUS_DEF 0

// This is a 'hidden' pref for the purposes of unit tests.
#define PREF_FREC_DECAY_RATE "places.frecency.decayRate"
#define PREF_FREC_DECAY_RATE_DEF 0.975f
// An adaptive history entry is removed if unused for these many days.
#define ADAPTIVE_HISTORY_EXPIRE_DAYS 90

// In order to avoid calling PR_now() too often we use a cached "now" value
// for repeating stuff.  These are milliseconds between "now" cache refreshes.
#define RENEW_CACHED_NOW_TIMEOUT ((int32_t)3 * PR_MSEC_PER_SEC)

// character-set annotation
#define CHARSET_ANNO NS_LITERAL_CSTRING("URIProperties/characterSet")

// These macros are used when splitting history by date.
// These are the day containers and catch-all final container.
#define HISTORY_ADDITIONAL_DATE_CONT_NUM 3
// We use a guess of the number of months considering all of them 30 days
// long, but we split only the last 6 months.
#define HISTORY_DATE_CONT_NUM(_daysFromOldestVisit) \
  (HISTORY_ADDITIONAL_DATE_CONT_NUM +               \
   std::min(6, (int32_t)ceilf((float)_daysFromOldestVisit / 30)))
// Max number of containers, used to initialize the params hash.
#define HISTORY_DATE_CONT_LENGTH 8

// Initial length of the recent events cache.
#define RECENT_EVENTS_INITIAL_CACHE_LENGTH 64

// Observed topics.
#define TOPIC_IDLE_DAILY "idle-daily"
#define TOPIC_PREF_CHANGED "nsPref:changed"
#define TOPIC_PROFILE_TEARDOWN "profile-change-teardown"
#define TOPIC_PROFILE_CHANGE "profile-before-change"

static const char* kObservedPrefs[] = {PREF_HISTORY_ENABLED,
                                       PREF_MATCH_DIACRITICS,
                                       PREF_FREC_NUM_VISITS,
                                       PREF_FREC_FIRST_BUCKET_CUTOFF,
                                       PREF_FREC_SECOND_BUCKET_CUTOFF,
                                       PREF_FREC_THIRD_BUCKET_CUTOFF,
                                       PREF_FREC_FOURTH_BUCKET_CUTOFF,
                                       PREF_FREC_FIRST_BUCKET_WEIGHT,
                                       PREF_FREC_SECOND_BUCKET_WEIGHT,
                                       PREF_FREC_THIRD_BUCKET_WEIGHT,
                                       PREF_FREC_FOURTH_BUCKET_WEIGHT,
                                       PREF_FREC_DEFAULT_BUCKET_WEIGHT,
                                       PREF_FREC_EMBED_VISIT_BONUS,
                                       PREF_FREC_FRAMED_LINK_VISIT_BONUS,
                                       PREF_FREC_LINK_VISIT_BONUS,
                                       PREF_FREC_TYPED_VISIT_BONUS,
                                       PREF_FREC_BOOKMARK_VISIT_BONUS,
                                       PREF_FREC_DOWNLOAD_VISIT_BONUS,
                                       PREF_FREC_PERM_REDIRECT_VISIT_BONUS,
                                       PREF_FREC_TEMP_REDIRECT_VISIT_BONUS,
                                       PREF_FREC_REDIR_SOURCE_VISIT_BONUS,
                                       PREF_FREC_DEFAULT_VISIT_BONUS,
                                       PREF_FREC_UNVISITED_BOOKMARK_BONUS,
                                       PREF_FREC_UNVISITED_TYPED_BONUS,
                                       nullptr};

NS_IMPL_ADDREF(nsNavHistory)
NS_IMPL_RELEASE(nsNavHistory)

NS_IMPL_CLASSINFO(nsNavHistory, nullptr, nsIClassInfo::SINGLETON,
                  NS_NAVHISTORYSERVICE_CID)
NS_INTERFACE_MAP_BEGIN(nsNavHistory)
  NS_INTERFACE_MAP_ENTRY(nsINavHistoryService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(mozIStorageVacuumParticipant)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsINavHistoryService)
  NS_IMPL_QUERY_CLASSINFO(nsNavHistory)
NS_INTERFACE_MAP_END

// We don't care about flattening everything
NS_IMPL_CI_INTERFACE_GETTER(nsNavHistory, nsINavHistoryService)

namespace {

static nsCString GetSimpleBookmarksQueryParent(
    const RefPtr<nsNavHistoryQuery>& aQuery,
    const RefPtr<nsNavHistoryQueryOptions>& aOptions);
static void ParseSearchTermsFromQuery(const RefPtr<nsNavHistoryQuery>& aQuery,
                                      nsTArray<nsString>* aTerms);

void GetTagsSqlFragment(int64_t aTagsFolder, const nsACString& aRelation,
                        bool aHasSearchTerms, nsACString& _sqlFragment) {
  if (!aHasSearchTerms)
    _sqlFragment.AssignLiteral("null");
  else {
    // This subquery DOES NOT order tags for performance reasons.
    _sqlFragment.Assign(
        NS_LITERAL_CSTRING("(SELECT GROUP_CONCAT(t_t.title, ',') "
                           "FROM moz_bookmarks b_t "
                           "JOIN moz_bookmarks t_t ON t_t.id = +b_t.parent  "
                           "WHERE b_t.fk = ") +
        aRelation +
        NS_LITERAL_CSTRING(" "
                           "AND t_t.parent = ") +
        nsPrintfCString("%" PRId64, aTagsFolder) +
        NS_LITERAL_CSTRING(" "
                           ")"));
  }

  _sqlFragment.AppendLiteral(" AS tags ");
}

/**
 * Recalculates invalid frecencies in chunks on the storage thread, optionally
 * decays frecencies, and notifies history observers on the main thread.
 */
class FixAndDecayFrecencyRunnable final : public Runnable {
 public:
  explicit FixAndDecayFrecencyRunnable(Database* aDB, float aDecayRate)
      : Runnable("places::FixAndDecayFrecencyRunnable"),
        mDB(aDB),
        mDecayRate(aDecayRate),
        mDecayReason(mozIStorageStatementCallback::REASON_FINISHED) {}

  NS_IMETHOD
  Run() override {
    if (NS_IsMainThread()) {
      nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
      NS_ENSURE_STATE(navHistory);

      navHistory->DecayFrecencyCompleted(mDecayReason);
      return NS_OK;
    }

    MOZ_ASSERT(!NS_IsMainThread(),
               "Frecencies should be recalculated on async thread");

    nsCOMPtr<mozIStorageStatement> updateStmt = mDB->GetStatement(
        "UPDATE moz_places "
        "SET frecency = CALCULATE_FRECENCY(id) "
        "WHERE id IN ("
        "SELECT id FROM moz_places "
        "WHERE frecency < 0 "
        "ORDER BY frecency ASC "
        "LIMIT 400"
        ")");
    NS_ENSURE_STATE(updateStmt);
    nsresult rv = updateStmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageStatement> selectStmt = mDB->GetStatement(
        "SELECT id FROM moz_places WHERE frecency < 0 "
        "LIMIT 1");
    NS_ENSURE_STATE(selectStmt);
    bool hasResult = false;
    rv = selectStmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasResult) {
      // There are more invalid frecencies to fix. Re-dispatch to the async
      // storage thread for the next chunk.
      return NS_DispatchToCurrentThread(this);
    }

    mozStorageTransaction transaction(
        mDB->MainConn(), false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    if (NS_WARN_IF(NS_FAILED(DecayFrecencies()))) {
      mDecayReason = mozIStorageStatementCallback::REASON_ERROR;
    }

    // We've finished fixing and decaying frecencies. Trigger frecency updates
    // for all affected origins.
    nsCOMPtr<mozIStorageStatement> updateOriginFrecenciesStmt =
        mDB->GetStatement("DELETE FROM moz_updateoriginsupdate_temp");
    NS_ENSURE_STATE(updateOriginFrecenciesStmt);
    rv = updateOriginFrecenciesStmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    // Re-dispatch to the main thread to notify observers.
    return NS_DispatchToMainThread(this);
  }

 private:
  nsresult DecayFrecencies() {
    TimeStamp start = TimeStamp::Now();

    // Globally decay places frecency rankings to estimate reduced frecency
    // values of pages that haven't been visited for a while, i.e., they do
    // not get an updated frecency.  A scaling factor of .975 results in .5 the
    // original value after 28 days.
    // When changing the scaling factor, ensure that the barrier in
    // moz_places_afterupdate_frecency_trigger still ignores these changes.
    nsCOMPtr<mozIStorageStatement> decayFrecency = mDB->GetStatement(
        "UPDATE moz_places SET frecency = ROUND(frecency * :decay_rate) "
        "WHERE frecency > 0");
    NS_ENSURE_STATE(decayFrecency);
    nsresult rv = decayFrecency->BindDoubleByName(
        NS_LITERAL_CSTRING("decay_rate"), static_cast<double>(mDecayRate));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = decayFrecency->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    // Decay potentially unused adaptive entries (e.g. those that are at 1)
    // to allow better chances for new entries that will start at 1.
    nsCOMPtr<mozIStorageStatement> decayAdaptive = mDB->GetStatement(
        "UPDATE moz_inputhistory SET use_count = use_count * :decay_rate");
    NS_ENSURE_STATE(decayAdaptive);
    rv = decayAdaptive->BindDoubleByName(NS_LITERAL_CSTRING("decay_rate"),
                                         static_cast<double>(mDecayRate));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = decayAdaptive->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    // Delete any adaptive entries that won't help in ordering anymore.
    nsCOMPtr<mozIStorageStatement> deleteAdaptive = mDB->GetStatement(
        "DELETE FROM moz_inputhistory WHERE use_count < :use_count");
    NS_ENSURE_STATE(deleteAdaptive);
    rv = deleteAdaptive->BindDoubleByName(
        NS_LITERAL_CSTRING("use_count"),
        std::pow(static_cast<double>(mDecayRate),
                 ADAPTIVE_HISTORY_EXPIRE_DAYS));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = deleteAdaptive->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    Telemetry::AccumulateTimeDelta(
        Telemetry::PLACES_IDLE_FRECENCY_DECAY_TIME_MS, start);

    return NS_OK;
  }

  RefPtr<Database> mDB;
  float mDecayRate;
  uint16_t mDecayReason;
};

}  // namespace

// Queries rows indexes to bind or get values, if adding a new one, be sure to
// update nsNavBookmarks statements and its kGetChildrenIndex_* constants
const int32_t nsNavHistory::kGetInfoIndex_PageID = 0;
const int32_t nsNavHistory::kGetInfoIndex_URL = 1;
const int32_t nsNavHistory::kGetInfoIndex_Title = 2;
const int32_t nsNavHistory::kGetInfoIndex_RevHost = 3;
const int32_t nsNavHistory::kGetInfoIndex_VisitCount = 4;
const int32_t nsNavHistory::kGetInfoIndex_VisitDate = 5;
const int32_t nsNavHistory::kGetInfoIndex_FaviconURL = 6;
const int32_t nsNavHistory::kGetInfoIndex_ItemId = 7;
const int32_t nsNavHistory::kGetInfoIndex_ItemDateAdded = 8;
const int32_t nsNavHistory::kGetInfoIndex_ItemLastModified = 9;
const int32_t nsNavHistory::kGetInfoIndex_ItemParentId = 10;
const int32_t nsNavHistory::kGetInfoIndex_ItemTags = 11;
const int32_t nsNavHistory::kGetInfoIndex_Frecency = 12;
const int32_t nsNavHistory::kGetInfoIndex_Hidden = 13;
const int32_t nsNavHistory::kGetInfoIndex_Guid = 14;
const int32_t nsNavHistory::kGetInfoIndex_VisitId = 15;
const int32_t nsNavHistory::kGetInfoIndex_FromVisitId = 16;
const int32_t nsNavHistory::kGetInfoIndex_VisitType = 17;
// These columns are followed by corresponding constants in nsNavBookmarks.cpp,
// which must be kept in sync:
// nsNavBookmarks::kGetChildrenIndex_Guid = 18;
// nsNavBookmarks::kGetChildrenIndex_Position = 19;
// nsNavBookmarks::kGetChildrenIndex_Type = 20;
// nsNavBookmarks::kGetChildrenIndex_PlaceID = 21;

PLACES_FACTORY_SINGLETON_IMPLEMENTATION(nsNavHistory, gHistoryService)

nsNavHistory::nsNavHistory()
    : mCachedNow(0),
      mRecentTyped(RECENT_EVENTS_INITIAL_CACHE_LENGTH),
      mRecentLink(RECENT_EVENTS_INITIAL_CACHE_LENGTH),
      mRecentBookmark(RECENT_EVENTS_INITIAL_CACHE_LENGTH),
      mHistoryEnabled(true),
      mMatchDiacritics(false),
      mNumVisitsForFrecency(10),
      mDecayFrecencyPendingCount(0),
      mTagsFolder(-1),
      mDaysOfHistory(-1),
      mLastCachedStartOfDay(INT64_MAX),
      mLastCachedEndOfDay(0),
      mCanNotify(true)
#ifdef XP_WIN
      ,
      mCryptoProviderInitialized(false)
#endif
{
  NS_ASSERTION(!gHistoryService,
               "Attempting to create two instances of the service!");
#ifdef XP_WIN
  BOOL cryptoAcquired =
      CryptAcquireContext(&mCryptoProvider, 0, 0, PROV_RSA_FULL,
                          CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
  if (cryptoAcquired) {
    mCryptoProviderInitialized = true;
  }
#endif
  gHistoryService = this;
}

nsNavHistory::~nsNavHistory() {
  MOZ_ASSERT(NS_IsMainThread(), "Must be called on the main thread");

  // remove the static reference to the service. Check to make sure its us
  // in case somebody creates an extra instance of the service.
  NS_ASSERTION(gHistoryService == this,
               "Deleting a non-singleton instance of the service");

  if (gHistoryService == this) gHistoryService = nullptr;

#ifdef XP_WIN
  if (mCryptoProviderInitialized) {
    Unused << CryptReleaseContext(mCryptoProvider, 0);
  }
#endif
}

nsresult nsNavHistory::Init() {
  LoadPrefs();

  mDB = Database::GetDatabase();
  NS_ENSURE_STATE(mDB);

  /*****************************************************************************
   *** IMPORTANT NOTICE!
   ***
   *** Nothing after these add observer calls should return anything but NS_OK.
   *** If a failure code is returned, this nsNavHistory object will be held onto
   *** by the observer service and the preference service.
   ****************************************************************************/

  // Observe preferences changes.
  Preferences::AddWeakObservers(this, kObservedPrefs);

  nsCOMPtr<nsIObserverService> obsSvc = services::GetObserverService();
  if (obsSvc) {
    (void)obsSvc->AddObserver(this, TOPIC_PLACES_CONNECTION_CLOSED, true);
    (void)obsSvc->AddObserver(this, TOPIC_IDLE_DAILY, true);
  }

  // Don't add code that can fail here! Do it up above, before we add our
  // observers.

  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::GetDatabaseStatus(uint16_t* aDatabaseStatus) {
  NS_ENSURE_ARG_POINTER(aDatabaseStatus);
  *aDatabaseStatus = mDB->GetDatabaseStatus();
  return NS_OK;
}

uint32_t nsNavHistory::GetRecentFlags(nsIURI* aURI) {
  uint32_t result = 0;
  nsAutoCString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to get aURI's spec");

  if (NS_SUCCEEDED(rv)) {
    if (CheckIsRecentEvent(&mRecentTyped, spec)) result |= RECENT_TYPED;
    if (CheckIsRecentEvent(&mRecentLink, spec)) result |= RECENT_ACTIVATED;
    if (CheckIsRecentEvent(&mRecentBookmark, spec)) result |= RECENT_BOOKMARKED;
  }

  return result;
}

nsresult nsNavHistory::GetIdForPage(nsIURI* aURI, int64_t* _pageId,
                                    nsCString& _GUID) {
  *_pageId = 0;

  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT id, url, title, rev_host, visit_count, guid "
      "FROM moz_places "
      "WHERE url_hash = hash(:page_url) AND url = :page_url ");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasEntry = false;
  rv = stmt->ExecuteStep(&hasEntry);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hasEntry) {
    rv = stmt->GetInt64(0, _pageId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->GetUTF8String(5, _GUID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult nsNavHistory::GetOrCreateIdForPage(nsIURI* aURI, int64_t* _pageId,
                                            nsCString& _GUID) {
  nsresult rv = GetIdForPage(aURI, _pageId, _GUID);
  NS_ENSURE_SUCCESS(rv, rv);

  if (*_pageId != 0) {
    return NS_OK;
  }

  {
    // Create a new hidden, untyped and unvisited entry.
    nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
        "INSERT INTO moz_places (url, url_hash, rev_host, hidden, frecency, "
        "guid) "
        "VALUES (:page_url, hash(:page_url), :rev_host, :hidden, :frecency, "
        ":guid) ");
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
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
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), 1);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoCString spec;
    rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("frecency"),
                               IsQueryURI(spec) ? 0 : -1);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = GenerateGUID(_GUID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), _GUID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    *_pageId = sLastInsertedPlaceId;
  }

  {
    // Trigger the updates to the moz_origins tables
    nsCOMPtr<mozIStorageStatement> stmt =
        mDB->GetStatement("DELETE FROM moz_updateoriginsinsert_temp");
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);
  }

  return NS_OK;
}

void nsNavHistory::LoadPrefs() {
  // History preferences.
  mHistoryEnabled = Preferences::GetBool(PREF_HISTORY_ENABLED, true);
  mMatchDiacritics = Preferences::GetBool(PREF_MATCH_DIACRITICS, false);

  // Frecency preferences.
#define FRECENCY_PREF(_prop, _pref) \
  _prop = Preferences::GetInt(_pref, _pref##_DEF)

  FRECENCY_PREF(mNumVisitsForFrecency, PREF_FREC_NUM_VISITS);
  FRECENCY_PREF(mFirstBucketCutoffInDays, PREF_FREC_FIRST_BUCKET_CUTOFF);
  FRECENCY_PREF(mSecondBucketCutoffInDays, PREF_FREC_SECOND_BUCKET_CUTOFF);
  FRECENCY_PREF(mThirdBucketCutoffInDays, PREF_FREC_THIRD_BUCKET_CUTOFF);
  FRECENCY_PREF(mFourthBucketCutoffInDays, PREF_FREC_FOURTH_BUCKET_CUTOFF);
  FRECENCY_PREF(mEmbedVisitBonus, PREF_FREC_EMBED_VISIT_BONUS);
  FRECENCY_PREF(mFramedLinkVisitBonus, PREF_FREC_FRAMED_LINK_VISIT_BONUS);
  FRECENCY_PREF(mLinkVisitBonus, PREF_FREC_LINK_VISIT_BONUS);
  FRECENCY_PREF(mTypedVisitBonus, PREF_FREC_TYPED_VISIT_BONUS);
  FRECENCY_PREF(mBookmarkVisitBonus, PREF_FREC_BOOKMARK_VISIT_BONUS);
  FRECENCY_PREF(mDownloadVisitBonus, PREF_FREC_DOWNLOAD_VISIT_BONUS);
  FRECENCY_PREF(mPermRedirectVisitBonus, PREF_FREC_PERM_REDIRECT_VISIT_BONUS);
  FRECENCY_PREF(mTempRedirectVisitBonus, PREF_FREC_TEMP_REDIRECT_VISIT_BONUS);
  FRECENCY_PREF(mRedirectSourceVisitBonus, PREF_FREC_REDIR_SOURCE_VISIT_BONUS);
  FRECENCY_PREF(mDefaultVisitBonus, PREF_FREC_DEFAULT_VISIT_BONUS);
  FRECENCY_PREF(mUnvisitedBookmarkBonus, PREF_FREC_UNVISITED_BOOKMARK_BONUS);
  FRECENCY_PREF(mUnvisitedTypedBonus, PREF_FREC_UNVISITED_TYPED_BONUS);
  FRECENCY_PREF(mReloadVisitBonus, PREF_FREC_RELOAD_VISIT_BONUS);
  FRECENCY_PREF(mFirstBucketWeight, PREF_FREC_FIRST_BUCKET_WEIGHT);
  FRECENCY_PREF(mSecondBucketWeight, PREF_FREC_SECOND_BUCKET_WEIGHT);
  FRECENCY_PREF(mThirdBucketWeight, PREF_FREC_THIRD_BUCKET_WEIGHT);
  FRECENCY_PREF(mFourthBucketWeight, PREF_FREC_FOURTH_BUCKET_WEIGHT);
  FRECENCY_PREF(mDefaultWeight, PREF_FREC_DEFAULT_BUCKET_WEIGHT);

#undef FRECENCY_PREF
}

void nsNavHistory::UpdateDaysOfHistory(PRTime visitTime) {
  if (mDaysOfHistory == 0) {
    mDaysOfHistory = 1;
  }

  if (visitTime > mLastCachedEndOfDay || visitTime < mLastCachedStartOfDay) {
    mDaysOfHistory = -1;
  }
}

void nsNavHistory::NotifyTitleChange(nsIURI* aURI, const nsString& aTitle,
                                     const nsACString& aGUID) {
  MOZ_ASSERT(!aGUID.IsEmpty());
  NOTIFY_OBSERVERS(mCanNotify, mObservers, nsINavHistoryObserver,
                   OnTitleChanged(aURI, aTitle, aGUID));
}

void nsNavHistory::NotifyFrecencyChanged(const nsACString& aSpec,
                                         int32_t aNewFrecency,
                                         const nsACString& aGUID, bool aHidden,
                                         PRTime aLastVisitDate) {
  MOZ_ASSERT(!aGUID.IsEmpty());

  nsCOMPtr<nsIURI> uri;
  Unused << NS_NewURI(getter_AddRefs(uri), aSpec);
  // We cannot assert since some automated tests are checking this path.
  NS_WARNING_ASSERTION(uri,
                       "Invalid URI in nsNavHistory::NotifyFrecencyChanged");
  // Notify a frecency change only if we have a valid uri, otherwise
  // the observer couldn't gather any useful data from the notification.
  if (!uri) {
    return;
  }
  NOTIFY_OBSERVERS(
      mCanNotify, mObservers, nsINavHistoryObserver,
      OnFrecencyChanged(uri, aNewFrecency, aGUID, aHidden, aLastVisitDate));
}

void nsNavHistory::NotifyManyFrecenciesChanged() {
  NOTIFY_OBSERVERS(mCanNotify, mObservers, nsINavHistoryObserver,
                   OnManyFrecenciesChanged());
}

void nsNavHistory::DispatchFrecencyChangedNotification(
    const nsACString& aSpec, int32_t aNewFrecency, const nsACString& aGUID,
    bool aHidden, PRTime aLastVisitDate) const {
  Unused << NS_DispatchToMainThread(
      NewRunnableMethod<nsCString, int32_t, nsCString, bool, PRTime>(
          "nsNavHistory::NotifyFrecencyChanged",
          const_cast<nsNavHistory*>(this), &nsNavHistory::NotifyFrecencyChanged,
          aSpec, aNewFrecency, aGUID, aHidden, aLastVisitDate));
}

NS_IMETHODIMP
nsNavHistory::RecalculateOriginFrecencyStats(nsIObserver* aCallback) {
  RefPtr<nsNavHistory> self(this);
  nsMainThreadPtrHandle<nsIObserver> callback(
      !aCallback ? nullptr
                 : new nsMainThreadPtrHolder<nsIObserver>(
                       "nsNavHistory::RecalculateOriginFrecencyStats callback",
                       aCallback));

  nsCOMPtr<nsIEventTarget> target(do_GetInterface(mDB->MainConn()));
  NS_ENSURE_STATE(target);
  nsresult rv = target->Dispatch(NS_NewRunnableFunction(
      "nsNavHistory::RecalculateOriginFrecencyStats", [self, callback] {
        Unused << self->RecalculateOriginFrecencyStatsInternal();
        Unused << NS_DispatchToMainThread(NS_NewRunnableFunction(
            "nsNavHistory::RecalculateOriginFrecencyStats callback",
            [callback] {
              if (callback) {
                Unused << callback->Observe(nullptr, "", nullptr);
              }
            }));
      }));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult nsNavHistory::RecalculateOriginFrecencyStatsInternal() {
  nsCOMPtr<mozIStorageConnection> conn(mDB->MainConn());
  NS_ENSURE_STATE(conn);

  nsresult rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "INSERT OR REPLACE INTO moz_meta(key, value) VALUES "
      "( "
      "'" MOZ_META_KEY_ORIGIN_FRECENCY_COUNT
      "' , "
      "(SELECT COUNT(*) FROM moz_origins WHERE frecency > 0) "
      "), "
      "( "
      "'" MOZ_META_KEY_ORIGIN_FRECENCY_SUM
      "', "
      "(SELECT TOTAL(frecency) FROM moz_origins WHERE frecency > 0) "
      "), "
      "( "
      "'" MOZ_META_KEY_ORIGIN_FRECENCY_SUM_OF_SQUARES
      "' , "
      "(SELECT TOTAL(frecency * frecency) FROM moz_origins WHERE frecency > 0) "
      ") "));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

Atomic<int64_t> nsNavHistory::sLastInsertedPlaceId(0);
Atomic<int64_t> nsNavHistory::sLastInsertedVisitId(0);

void  // static
nsNavHistory::StoreLastInsertedId(const nsACString& aTable,
                                  const int64_t aLastInsertedId) {
  if (aTable.EqualsLiteral("moz_places")) {
    nsNavHistory::sLastInsertedPlaceId = aLastInsertedId;
  } else if (aTable.EqualsLiteral("moz_historyvisits")) {
    nsNavHistory::sLastInsertedVisitId = aLastInsertedId;
  } else {
    MOZ_ASSERT(false, "Trying to store the insert id for an unknown table?");
  }
}

int32_t nsNavHistory::GetDaysOfHistory() {
  MOZ_ASSERT(NS_IsMainThread(), "This can only be called on the main thread");

  if (mDaysOfHistory != -1) return mDaysOfHistory;

  // SQLite doesn't have a CEIL() function, so we must do that later.
  // We should also take into account timers resolution, that may be as bad as
  // 16ms on Windows, so in some cases the difference may be 0, if the
  // check is done near the visit.  Thus remember to check for NULL separately.
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      "SELECT CAST(( "
      "strftime('%s','now','localtime','utc') - "
      "(SELECT MIN(visit_date)/1000000 FROM moz_historyvisits) "
      ") AS DOUBLE) "
      "/86400, "
      "strftime('%s','now','localtime','+1 day','start of day','utc') * "
      "1000000");
  NS_ENSURE_TRUE(stmt, 0);
  mozStorageStatementScoper scoper(stmt);

  bool hasResult;
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    // If we get NULL, then there are no visits, otherwise there must always be
    // at least 1 day of history.
    bool hasNoVisits;
    (void)stmt->GetIsNull(0, &hasNoVisits);
    mDaysOfHistory =
        hasNoVisits
            ? 0
            : std::max(1, static_cast<int32_t>(ceil(stmt->AsDouble(0))));
    mLastCachedStartOfDay =
        NormalizeTime(nsINavHistoryQuery::TIME_RELATIVE_TODAY, 0);
    mLastCachedEndOfDay = stmt->AsInt64(1) - 1;  // Start of tomorrow - 1.
  }

  return mDaysOfHistory;
}

PRTime nsNavHistory::GetNow() {
  if (!mCachedNow) {
    mCachedNow = PR_Now();
    if (!mExpireNowTimer) mExpireNowTimer = NS_NewTimer();
    if (mExpireNowTimer)
      mExpireNowTimer->InitWithNamedFuncCallback(
          expireNowTimerCallback, this, RENEW_CACHED_NOW_TIMEOUT,
          nsITimer::TYPE_ONE_SHOT, "nsNavHistory::GetNow");
  }
  return mCachedNow;
}

void nsNavHistory::expireNowTimerCallback(nsITimer* aTimer, void* aClosure) {
  nsNavHistory* history = static_cast<nsNavHistory*>(aClosure);
  if (history) {
    history->mCachedNow = 0;
    history->mExpireNowTimer = nullptr;
  }
}

/**
 * Code borrowed from mozilla/xpfe/components/history/src/nsGlobalHistory.cpp
 * Pass in a pre-normalized now and a date, and we'll find the difference since
 * midnight on each of the days.
 */
static PRTime NormalizeTimeRelativeToday(PRTime aTime) {
  // round to midnight this morning
  PRExplodedTime explodedTime;
  PR_ExplodeTime(aTime, PR_LocalTimeParameters, &explodedTime);

  // set to midnight (0:00)
  explodedTime.tm_min = explodedTime.tm_hour = explodedTime.tm_sec =
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

PRTime  // static
nsNavHistory::NormalizeTime(uint32_t aRelative, PRTime aOffset) {
  PRTime ref;
  switch (aRelative) {
    case nsINavHistoryQuery::TIME_RELATIVE_EPOCH:
      return aOffset;
    case nsINavHistoryQuery::TIME_RELATIVE_TODAY:
      ref = NormalizeTimeRelativeToday(PR_Now());
      break;
    case nsINavHistoryQuery::TIME_RELATIVE_NOW:
      ref = PR_Now();
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid relative time");
      return 0;
  }
  return ref + aOffset;
}

// nsNavHistory::DomainNameFromURI
//
//    This does the www.mozilla.org -> mozilla.org and
//    foo.theregister.co.uk -> theregister.co.uk conversion
void nsNavHistory::DomainNameFromURI(nsIURI* aURI, nsACString& aDomainName) {
  // lazily get the effective tld service
  if (!mTLDService)
    mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);

  if (mTLDService) {
    // get the base domain for a given hostname.
    // e.g. for "images.bbc.co.uk", this would be "bbc.co.uk".
    nsresult rv = mTLDService->GetBaseDomain(aURI, 0, aDomainName);
    if (NS_SUCCEEDED(rv)) return;
  }

  // just return the original hostname
  // (it's also possible the host is an IP address)
  aURI->GetAsciiHost(aDomainName);
}

bool nsNavHistory::hasHistoryEntries() { return GetDaysOfHistory() > 0; }

// Call this method before visiting a URL in order to help determine the
// transition type of the visit.
//
// @see MarkPageAsTyped

NS_IMETHODIMP
nsNavHistory::MarkPageAsFollowedBookmark(nsIURI* aURI) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // don't add when history is disabled
  if (IsHistoryDisabled()) return NS_OK;

  nsAutoCString uriString;
  nsresult rv = aURI->GetSpec(uriString);
  NS_ENSURE_SUCCESS(rv, rv);

  mRecentBookmark.Put(uriString, GetNow());

  if (mRecentBookmark.Count() > RECENT_EVENT_QUEUE_MAX_LENGTH)
    ExpireNonrecentEvents(&mRecentBookmark);

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
nsNavHistory::CanAddURI(nsIURI* aURI, bool* canAdd) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(canAdd);

  // If history is disabled, don't add any entry.
  if (IsHistoryDisabled()) {
    *canAdd = false;
    return NS_OK;
  }

  return CanAddURIToHistory(aURI, canAdd);
}

// nsNavHistory::CanAddURIToHistory
//
//    Helper for nsNavHistory::CanAddURI to be callable from a child process

// static
nsresult
nsNavHistory::CanAddURIToHistory(nsIURI* aURI, bool* aCanAdd) {
  // Default to false.
  *aCanAdd = false;

  // If the url length is over a threshold, don't add it.
  nsCString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  if (spec.Length() > MaxURILength()) {
    return NS_OK;
  }

  nsAutoCString scheme;
  rv = aURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  // first check the most common cases
  if (scheme.EqualsLiteral("http") || scheme.EqualsLiteral("https")) {
    *aCanAdd = true;
    return NS_OK;
  }

  // now check for all bad things
  *aCanAdd =
      !scheme.EqualsLiteral("about") && !scheme.EqualsLiteral("blob") &&
      !scheme.EqualsLiteral("chrome") && !scheme.EqualsLiteral("data") &&
      !scheme.EqualsLiteral("imap") && !scheme.EqualsLiteral("javascript") &&
      !scheme.EqualsLiteral("mailbox") && !scheme.EqualsLiteral("moz-anno") &&
      !scheme.EqualsLiteral("news") && !scheme.EqualsLiteral("page-icon") &&
      !scheme.EqualsLiteral("resource") && !scheme.EqualsLiteral("view-source");

  return NS_OK;
}

// nsNavHistory::MaxURILength

// static
uint32_t nsNavHistory::MaxURILength() {
  // Duplicates Database::MaxUrlLength() for use in
  // child processes without a database instance.
  static uint32_t maxSpecLength = 0;
  if (!maxSpecLength) {
    maxSpecLength = Preferences::GetInt(PREF_HISTORY_MAXURLLEN,
                                        PREF_HISTORY_MAXURLLEN_DEFAULT);
    if (maxSpecLength < 255 || maxSpecLength > INT32_MAX) {
      maxSpecLength = PREF_HISTORY_MAXURLLEN_DEFAULT;
    }
  }
  return maxSpecLength;
}

// nsNavHistory::GetNewQuery

NS_IMETHODIMP
nsNavHistory::GetNewQuery(nsINavHistoryQuery** _retval) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG_POINTER(_retval);

  RefPtr<nsNavHistoryQuery> query = new nsNavHistoryQuery();
  query.forget(_retval);
  return NS_OK;
}

// nsNavHistory::GetNewQueryOptions

NS_IMETHODIMP
nsNavHistory::GetNewQueryOptions(nsINavHistoryQueryOptions** _retval) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG_POINTER(_retval);

  RefPtr<nsNavHistoryQueryOptions> queryOptions =
      new nsNavHistoryQueryOptions();
  queryOptions.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::ExecuteQuery(nsINavHistoryQuery* aQuery,
                           nsINavHistoryQueryOptions* aOptions,
                           nsINavHistoryResult** _retval) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aQuery);
  NS_ENSURE_ARG(aOptions);
  NS_ENSURE_ARG_POINTER(_retval);

  // Clone the input query and options, because the caller might change the
  // objects, but we always want to reflect the original parameters.
  nsCOMPtr<nsINavHistoryQuery> queryClone;
  aQuery->Clone(getter_AddRefs(queryClone));
  NS_ENSURE_STATE(queryClone);
  RefPtr<nsNavHistoryQuery> query = do_QueryObject(queryClone);
  NS_ENSURE_STATE(query);
  nsCOMPtr<nsINavHistoryQueryOptions> optionsClone;
  aOptions->Clone(getter_AddRefs(optionsClone));
  NS_ENSURE_STATE(optionsClone);
  RefPtr<nsNavHistoryQueryOptions> options = do_QueryObject(optionsClone);
  NS_ENSURE_STATE(options);

  // Create the root node.
  RefPtr<nsNavHistoryContainerResultNode> rootNode;

  nsCString folderGuid = GetSimpleBookmarksQueryParent(query, options);
  if (!folderGuid.IsEmpty()) {
    // In the simple case where we're just querying children of a single
    // bookmark folder, we can more efficiently generate results.
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
    RefPtr<nsNavHistoryResultNode> tempRootNode;
    nsresult rv = bookmarks->ResultNodeForContainer(
        folderGuid, options, getter_AddRefs(tempRootNode));
    if (NS_SUCCEEDED(rv)) {
      rootNode = tempRootNode->GetAsContainer();
    } else {
      NS_WARNING("Generating a generic empty node for a broken query!");
      // This is a perf hack to generate an empty query that skips filtering.
      options->SetExcludeItems(true);
    }
  }

  if (!rootNode) {
    // Either this is not a folder shortcut, or is a broken one.  In both cases
    // just generate a query node.
    nsAutoCString queryUri;
    nsresult rv = QueryToQueryString(query, options, queryUri);
    NS_ENSURE_SUCCESS(rv, rv);
    rootNode = new nsNavHistoryQueryResultNode(EmptyCString(), 0, queryUri,
                                               query, options);
  }

  // Create the result that will hold nodes.  Inject batching status into it.
  RefPtr<nsNavHistoryResult> result =
      new nsNavHistoryResult(rootNode, query, options);
  result.forget(_retval);
  return NS_OK;
}

// determine from our nsNavHistoryQuery array and nsNavHistoryQueryOptions
// if this is the place query from the history menu.
// from browser-menubar.inc, our history menu query is:
// place:sort=4&maxResults=10
// note, any maxResult > 0 will still be considered a history menu query
// or if this is the place query from the old "Most Visited" item in some
// profiles: folder: place:sort=8&maxResults=10 note, any maxResult > 0 will
// still be considered a Most Visited menu query
static bool IsOptimizableHistoryQuery(
    const RefPtr<nsNavHistoryQuery>& aQuery,
    const RefPtr<nsNavHistoryQueryOptions>& aOptions, uint16_t aSortMode) {
  if (aOptions->QueryType() != nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY)
    return false;

  if (aOptions->ResultType() != nsINavHistoryQueryOptions::RESULTS_AS_URI)
    return false;

  if (aOptions->SortingMode() != aSortMode) return false;

  if (aOptions->MaxResults() <= 0) return false;

  if (aOptions->ExcludeItems()) return false;

  if (aOptions->IncludeHidden()) return false;

  if (aQuery->MinVisits() != -1 || aQuery->MaxVisits() != -1) return false;

  if (aQuery->BeginTime() || aQuery->BeginTimeReference()) return false;

  if (aQuery->EndTime() || aQuery->EndTimeReference()) return false;

  if (!aQuery->SearchTerms().IsEmpty()) return false;

  if (aQuery->OnlyBookmarked()) return false;

  if (aQuery->DomainIsHost() || !aQuery->Domain().IsEmpty()) return false;

  if (aQuery->AnnotationIsNot() || !aQuery->Annotation().IsEmpty())
    return false;

  if (aQuery->Parents().Length() > 0) return false;

  if (aQuery->Tags().Length() > 0) return false;

  if (aQuery->Transitions().Length() > 0) return false;

  return true;
}

static bool NeedToFilterResultSet(const RefPtr<nsNavHistoryQuery>& aQuery,
                                  nsNavHistoryQueryOptions* aOptions) {
  return aOptions->ExcludeQueries();
}

// ** Helper class for ConstructQueryString **/

class PlacesSQLQueryBuilder {
 public:
  PlacesSQLQueryBuilder(const nsCString& aConditions,
                        const RefPtr<nsNavHistoryQuery>& aQuery,
                        const RefPtr<nsNavHistoryQueryOptions>& aOptions,
                        bool aUseLimit, nsNavHistory::StringHash& aAddParams,
                        bool aHasSearchTerms);

  nsresult GetQueryString(nsCString& aQueryString);

 private:
  nsresult Select();

  nsresult SelectAsURI();
  nsresult SelectAsVisit();
  nsresult SelectAsDay();
  nsresult SelectAsSite();
  nsresult SelectAsTag();
  nsresult SelectAsRoots();
  nsresult SelectAsLeftPane();

  nsresult Where();
  nsresult GroupBy();
  nsresult OrderBy();
  nsresult Limit();

  void OrderByColumnIndexAsc(int32_t aIndex);
  void OrderByColumnIndexDesc(int32_t aIndex);
  // Use these if you want a case insensitive sorting.
  void OrderByTextColumnIndexAsc(int32_t aIndex);
  void OrderByTextColumnIndexDesc(int32_t aIndex);

  const nsCString& mConditions;
  bool mUseLimit;
  bool mHasSearchTerms;

  uint16_t mResultType;
  uint16_t mQueryType;
  bool mIncludeHidden;
  uint16_t mSortingMode;
  uint32_t mMaxResults;

  nsCString mQueryString;
  nsCString mGroupBy;
  bool mHasDateColumns;
  bool mSkipOrderBy;

  nsNavHistory::StringHash& mAddParams;
};

PlacesSQLQueryBuilder::PlacesSQLQueryBuilder(
    const nsCString& aConditions, const RefPtr<nsNavHistoryQuery>& aQuery,
    const RefPtr<nsNavHistoryQueryOptions>& aOptions, bool aUseLimit,
    nsNavHistory::StringHash& aAddParams, bool aHasSearchTerms)
    : mConditions(aConditions),
      mUseLimit(aUseLimit),
      mHasSearchTerms(aHasSearchTerms),
      mResultType(aOptions->ResultType()),
      mQueryType(aOptions->QueryType()),
      mIncludeHidden(aOptions->IncludeHidden()),
      mSortingMode(aOptions->SortingMode()),
      mMaxResults(aOptions->MaxResults()),
      mSkipOrderBy(false),
      mAddParams(aAddParams) {
  mHasDateColumns =
      (mQueryType == nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS);
  // Force the default sorting mode for tag queries.
  if (mSortingMode == nsINavHistoryQueryOptions::SORT_BY_NONE &&
      aQuery->Tags().Length() > 0) {
    mSortingMode = nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING;
  }
}

nsresult PlacesSQLQueryBuilder::GetQueryString(nsCString& aQueryString) {
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

nsresult PlacesSQLQueryBuilder::Select() {
  nsresult rv;

  switch (mResultType) {
    case nsINavHistoryQueryOptions::RESULTS_AS_URI:
      rv = SelectAsURI();
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    case nsINavHistoryQueryOptions::RESULTS_AS_VISIT:
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

    case nsINavHistoryQueryOptions::RESULTS_AS_TAGS_ROOT:
      rv = SelectAsTag();
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    case nsINavHistoryQueryOptions::RESULTS_AS_ROOTS_QUERY:
      rv = SelectAsRoots();
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    case nsINavHistoryQueryOptions::RESULTS_AS_LEFT_PANE_QUERY:
      rv = SelectAsLeftPane();
      NS_ENSURE_SUCCESS(rv, rv);
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Invalid result type");
  }
  return NS_OK;
}

nsresult PlacesSQLQueryBuilder::SelectAsURI() {
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  nsAutoCString tagsSqlFragment;

  switch (mQueryType) {
    case nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY:
      GetTagsSqlFragment(history->GetTagsFolder(), NS_LITERAL_CSTRING("h.id"),
                         mHasSearchTerms, tagsSqlFragment);

      mQueryString = NS_LITERAL_CSTRING(
                         "SELECT h.id, h.url, h.title AS page_title, "
                         "h.rev_host, h.visit_count, "
                         "h.last_visit_date, null, null, null, null, null, ") +
                     tagsSqlFragment +
                     NS_LITERAL_CSTRING(
                         ", h.frecency, h.hidden, h.guid, "
                         "null, null, null "
                         "FROM moz_places h "
                         // WHERE 1 is a no-op since additonal conditions will
                         // start with AND.
                         "WHERE 1 "
                         "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
                         "{ADDITIONAL_CONDITIONS} ");
      break;

    case nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS:

      GetTagsSqlFragment(history->GetTagsFolder(), NS_LITERAL_CSTRING("b.fk"),
                         mHasSearchTerms, tagsSqlFragment);
      mQueryString =
          NS_LITERAL_CSTRING(
              "SELECT b.fk, h.url, b.title AS page_title, "
              "h.rev_host, h.visit_count, h.last_visit_date, null, b.id, "
              "b.dateAdded, b.lastModified, b.parent, ") +
          tagsSqlFragment +
          NS_LITERAL_CSTRING(
              ", h.frecency, h.hidden, h.guid,"
              "null, null, null, b.guid, b.position, b.type, b.fk "
              "FROM moz_bookmarks b "
              "JOIN moz_places h ON b.fk = h.id "
              "WHERE NOT EXISTS "
              "(SELECT id FROM moz_bookmarks "
              "WHERE id = b.parent AND parent = ") +
          nsPrintfCString("%" PRId64, history->GetTagsFolder()) +
          NS_LITERAL_CSTRING(
              ") "
              "AND NOT h.url_hash BETWEEN hash('place', 'prefix_lo') AND "
              "hash('place', 'prefix_hi') "
              "{ADDITIONAL_CONDITIONS}");
      break;

    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
  return NS_OK;
}

nsresult PlacesSQLQueryBuilder::SelectAsVisit() {
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  nsAutoCString tagsSqlFragment;
  GetTagsSqlFragment(history->GetTagsFolder(), NS_LITERAL_CSTRING("h.id"),
                     mHasSearchTerms, tagsSqlFragment);
  mQueryString =
      NS_LITERAL_CSTRING(
          "SELECT h.id, h.url, h.title AS page_title, h.rev_host, "
          "h.visit_count, "
          "v.visit_date, null, null, null, null, null, ") +
      tagsSqlFragment +
      NS_LITERAL_CSTRING(
          ", h.frecency, h.hidden, h.guid, "
          "v.id, v.from_visit, v.visit_type "
          "FROM moz_places h "
          "JOIN moz_historyvisits v ON h.id = v.place_id "
          // WHERE 1 is a no-op since additonal conditions will start with AND.
          "WHERE 1 "
          "{QUERY_OPTIONS_VISITS} {QUERY_OPTIONS_PLACES} "
          "{ADDITIONAL_CONDITIONS} ");

  return NS_OK;
}

nsresult PlacesSQLQueryBuilder::SelectAsDay() {
  mSkipOrderBy = true;

  // Sort child queries based on sorting mode if it's provided, otherwise
  // fallback to default sort by title ascending.
  uint16_t sortingMode = nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING;
  if (mSortingMode != nsINavHistoryQueryOptions::SORT_BY_NONE &&
      mResultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY)
    sortingMode = mSortingMode;

  uint16_t resultType =
      mResultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY
          ? (uint16_t)nsINavHistoryQueryOptions::RESULTS_AS_URI
          : (uint16_t)nsINavHistoryQueryOptions::RESULTS_AS_SITE_QUERY;

  // beginTime will become the node's time property, we don't use endTime
  // because it could overlap, and we use time to sort containers and find
  // insert position in a result.
  mQueryString = nsPrintfCString(
      "SELECT null, "
      "'place:type=%d&sort=%d&beginTime='||beginTime||'&endTime='||endTime, "
      "dayTitle, null, null, beginTime, null, null, null, null, null, null, "
      "null, null, null "
      "FROM (",  // TOUTER BEGIN
      resultType, sortingMode);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_STATE(history);

  int32_t daysOfHistory = history->GetDaysOfHistory();
  for (int32_t i = 0; i <= HISTORY_DATE_CONT_NUM(daysOfHistory); i++) {
    nsAutoCString dateName;
    // Timeframes are calculated as BeginTime <= container < EndTime.
    // Notice times can't be relative to now, since to recognize a query we
    // must ensure it won't change based on the time it is built.
    // So, to select till now, we really select till start of tomorrow, that is
    // a fixed timestamp.
    // These are used as limits for the inside containers.
    nsAutoCString sqlFragmentContainerBeginTime, sqlFragmentContainerEndTime;
    // These are used to query if the container should be visible.
    nsAutoCString sqlFragmentSearchBeginTime, sqlFragmentSearchEndTime;
    switch (i) {
      case 0:
        // Today
        history->GetStringFromName("finduri-AgeInDays-is-0", dateName);
        // From start of today
        sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of day','utc')*1000000)");
        // To now (tomorrow)
        sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of day','+1 "
            "day','utc')*1000000)");
        // Search for the same timeframe.
        sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
        sqlFragmentSearchEndTime = sqlFragmentContainerEndTime;
        break;
      case 1:
        // Yesterday
        history->GetStringFromName("finduri-AgeInDays-is-1", dateName);
        // From start of yesterday
        sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of day','-1 "
            "day','utc')*1000000)");
        // To start of today
        sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of day','utc')*1000000)");
        // Search for the same timeframe.
        sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
        sqlFragmentSearchEndTime = sqlFragmentContainerEndTime;
        break;
      case 2:
        // Last 7 days
        history->GetAgeInDaysString(7, "finduri-AgeInDays-last-is", dateName);
        // From start of 7 days ago
        sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of day','-7 "
            "days','utc')*1000000)");
        // To now (tomorrow)
        sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of day','+1 "
            "day','utc')*1000000)");
        // This is an overlapped container, but we show it only if there are
        // visits older than yesterday.
        sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
        sqlFragmentSearchEndTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of day','-1 "
            "day','utc')*1000000)");
        break;
      case 3:
        // This month
        history->GetStringFromName("finduri-AgeInMonths-is-0", dateName);
        // From start of this month
        sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of "
            "month','utc')*1000000)");
        // To now (tomorrow)
        sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of day','+1 "
            "day','utc')*1000000)");
        // This is an overlapped container, but we show it only if there are
        // visits older than 7 days ago.
        sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
        sqlFragmentSearchEndTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of day','-7 "
            "days','utc')*1000000)");
        break;
      default:
        if (i == HISTORY_ADDITIONAL_DATE_CONT_NUM + 6) {
          // Older than 6 months
          history->GetAgeInDaysString(6, "finduri-AgeInMonths-isgreater",
                                      dateName);
          // From start of epoch
          sqlFragmentContainerBeginTime =
              NS_LITERAL_CSTRING("(datetime(0, 'unixepoch')*1000000)");
          // To start of 6 months ago ( 5 months + this month).
          sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
              "(strftime('%s','now','localtime','start of month','-5 "
              "months','utc')*1000000)");
          // Search for the same timeframe.
          sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
          sqlFragmentSearchEndTime = sqlFragmentContainerEndTime;
          break;
        }
        int32_t MonthIndex = i - HISTORY_ADDITIONAL_DATE_CONT_NUM;
        // Previous months' titles are month's name if inside this year,
        // month's name and year for previous years.
        PRExplodedTime tm;
        PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &tm);
        uint16_t currentYear = tm.tm_year;
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
        if (tm.tm_year < currentYear) {
          nsNavHistory::GetMonthYear(tm, dateName);
        } else {
          nsNavHistory::GetMonthName(tm, dateName);
        }

        // From start of MonthIndex + 1 months ago
        sqlFragmentContainerBeginTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of month','-");
        sqlFragmentContainerBeginTime.AppendInt(MonthIndex);
        sqlFragmentContainerBeginTime.AppendLiteral(" months','utc')*1000000)");
        // To start of MonthIndex months ago
        sqlFragmentContainerEndTime = NS_LITERAL_CSTRING(
            "(strftime('%s','now','localtime','start of month','-");
        sqlFragmentContainerEndTime.AppendInt(MonthIndex - 1);
        sqlFragmentContainerEndTime.AppendLiteral(" months','utc')*1000000)");
        // Search for the same timeframe.
        sqlFragmentSearchBeginTime = sqlFragmentContainerBeginTime;
        sqlFragmentSearchEndTime = sqlFragmentContainerEndTime;
        break;
    }

    nsPrintfCString dateParam("dayTitle%d", i);
    mAddParams.Put(dateParam, dateName);

    nsPrintfCString dayRange(
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
        dateParam.get(), sqlFragmentContainerBeginTime.get(),
        sqlFragmentContainerEndTime.get(), sqlFragmentSearchBeginTime.get(),
        sqlFragmentSearchEndTime.get(), nsINavHistoryService::TRANSITION_EMBED,
        nsINavHistoryService::TRANSITION_FRAMED_LINK);

    mQueryString.Append(dayRange);

    if (i < HISTORY_DATE_CONT_NUM(daysOfHistory))
      mQueryString.AppendLiteral(" UNION ALL ");
  }

  mQueryString.AppendLiteral(") ");  // TOUTER END

  return NS_OK;
}

nsresult PlacesSQLQueryBuilder::SelectAsSite() {
  nsAutoCString localFiles;

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_STATE(history);

  history->GetStringFromName("localhost", localFiles);
  mAddParams.Put(NS_LITERAL_CSTRING("localhost"), localFiles);

  // If there are additional conditions the query has to join on visits too.
  nsAutoCString visitsJoin;
  nsAutoCString additionalConditions;
  nsAutoCString timeConstraints;
  if (!mConditions.IsEmpty()) {
    visitsJoin.AssignLiteral("JOIN moz_historyvisits v ON v.place_id = h.id ");
    additionalConditions.AssignLiteral(
        "{QUERY_OPTIONS_VISITS} "
        "{QUERY_OPTIONS_PLACES} "
        "{ADDITIONAL_CONDITIONS} ");
    timeConstraints.AssignLiteral(
        "||'&beginTime='||:begin_time||"
        "'&endTime='||:end_time");
  }

  mQueryString = nsPrintfCString(
      "SELECT null, 'place:type=%d&sort=%d&domain=&domainIsHost=true'%s, "
      ":localhost, :localhost, null, null, null, null, null, null, null, "
      "null, null, null "
      "WHERE EXISTS ( "
      "SELECT h.id FROM moz_places h "
      "%s "
      "WHERE h.hidden = 0 "
      "AND h.visit_count > 0 "
      "AND h.url_hash BETWEEN hash('file', 'prefix_lo') AND "
      "hash('file', 'prefix_hi') "
      "%s "
      "LIMIT 1 "
      ") "
      "UNION ALL "
      "SELECT null, "
      "'place:type=%d&sort=%d&domain='||host||'&domainIsHost=true'%s, "
      "host, host, null, null, null, null, null, null, null, "
      "null, null, null "
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
      nsINavHistoryQueryOptions::RESULTS_AS_URI, mSortingMode,
      timeConstraints.get(), visitsJoin.get(), additionalConditions.get(),
      nsINavHistoryQueryOptions::RESULTS_AS_URI, mSortingMode,
      timeConstraints.get(), visitsJoin.get(), additionalConditions.get());

  return NS_OK;
}

nsresult PlacesSQLQueryBuilder::SelectAsTag() {
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_STATE(history);

  // This allows sorting by date fields what is not possible with
  // other history queries.
  mHasDateColumns = true;

  // TODO (Bug 1449939): This is likely wrong, since the tag name should
  // probably be urlencoded, and we have no util for that in SQL, yet.
  // We could encode the tag when the user sets it though.
  mQueryString = nsPrintfCString(
      "SELECT null, 'place:tag=' || title, "
      "title, null, null, null, null, null, dateAdded, "
      "lastModified, null, null, null, null, null, null "
      "FROM moz_bookmarks "
      "WHERE parent = %" PRId64,
      history->GetTagsFolder());

  return NS_OK;
}

nsresult PlacesSQLQueryBuilder::SelectAsRoots() {
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_STATE(history);

  nsAutoCString toolbarTitle;
  nsAutoCString menuTitle;
  nsAutoCString unfiledTitle;

  history->GetStringFromName("BookmarksToolbarFolderTitle", toolbarTitle);
  mAddParams.Put(NS_LITERAL_CSTRING("BookmarksToolbarFolderTitle"),
                 toolbarTitle);
  history->GetStringFromName("BookmarksMenuFolderTitle", menuTitle);
  mAddParams.Put(NS_LITERAL_CSTRING("BookmarksMenuFolderTitle"), menuTitle);
  history->GetStringFromName("OtherBookmarksFolderTitle", unfiledTitle);
  mAddParams.Put(NS_LITERAL_CSTRING("OtherBookmarksFolderTitle"), unfiledTitle);

  nsAutoCString mobileString;

  if (Preferences::GetBool(MOBILE_BOOKMARKS_PREF, false)) {
    nsAutoCString mobileTitle;
    history->GetStringFromName("MobileBookmarksFolderTitle", mobileTitle);
    mAddParams.Put(NS_LITERAL_CSTRING("MobileBookmarksFolderTitle"),
                   mobileTitle);

    mobileString = NS_LITERAL_CSTRING(
        ","
        "(null, 'place:parent=" MOBILE_ROOT_GUID
        "', :MobileBookmarksFolderTitle, null, null, null, "
        "null, null, 0, 0, null, null, null, null, "
        "'" MOBILE_BOOKMARKS_VIRTUAL_GUID "', null) ");
  }

  mQueryString =
      NS_LITERAL_CSTRING(
          "SELECT * FROM ("
          "VALUES(null, 'place:parent=" TOOLBAR_ROOT_GUID
          "', :BookmarksToolbarFolderTitle, null, null, null, "
          "null, null, 0, 0, null, null, null, null, 'toolbar____v', null), "
          "(null, 'place:parent=" MENU_ROOT_GUID
          "', :BookmarksMenuFolderTitle, null, null, null, "
          "null, null, 0, 0, null, null, null, null, 'menu_______v', null), "
          "(null, 'place:parent=" UNFILED_ROOT_GUID
          "', :OtherBookmarksFolderTitle, null, null, null, "
          "null, null, 0, 0, null, null, null, null, 'unfiled____v', null) ") +
      mobileString + NS_LITERAL_CSTRING(")");

  return NS_OK;
}

nsresult PlacesSQLQueryBuilder::SelectAsLeftPane() {
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_STATE(history);

  nsAutoCString historyTitle;
  nsAutoCString downloadsTitle;
  nsAutoCString tagsTitle;
  nsAutoCString allBookmarksTitle;

  history->GetStringFromName("OrganizerQueryHistory", historyTitle);
  mAddParams.Put(NS_LITERAL_CSTRING("OrganizerQueryHistory"), historyTitle);
  history->GetStringFromName("OrganizerQueryDownloads", downloadsTitle);
  mAddParams.Put(NS_LITERAL_CSTRING("OrganizerQueryDownloads"), downloadsTitle);
  history->GetStringFromName("TagsFolderTitle", tagsTitle);
  mAddParams.Put(NS_LITERAL_CSTRING("TagsFolderTitle"), tagsTitle);
  history->GetStringFromName("OrganizerQueryAllBookmarks", allBookmarksTitle);
  mAddParams.Put(NS_LITERAL_CSTRING("OrganizerQueryAllBookmarks"),
                 allBookmarksTitle);

  mQueryString = nsPrintfCString(
      "SELECT * FROM ("
      "VALUES"
      "(null, 'place:type=%d&sort=%d', :OrganizerQueryHistory, null, null, "
      "null, "
      "null, null, 0, 0, null, null, null, null, 'history____v', null), "
      "(null, 'place:transition=%d&sort=%d', :OrganizerQueryDownloads, null, "
      "null, null, "
      "null, null, 0, 0, null, null, null, null, 'downloads__v', null), "
      "(null, 'place:type=%d&sort=%d', :TagsFolderTitle, null, null, null, "
      "null, null, 0, 0, null, null, null, null, 'tags_______v', null), "
      "(null, 'place:type=%d', :OrganizerQueryAllBookmarks, null, null, null, "
      "null, null, 0, 0, null, null, null, null, 'allbms_____v', null) "
      ")",
      nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY,
      nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING,
      nsINavHistoryService::TRANSITION_DOWNLOAD,
      nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING,
      nsINavHistoryQueryOptions::RESULTS_AS_TAGS_ROOT,
      nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING,
      nsINavHistoryQueryOptions::RESULTS_AS_ROOTS_QUERY);
  return NS_OK;
}

nsresult PlacesSQLQueryBuilder::Where() {
  // Set query options
  nsAutoCString additionalVisitsConditions;
  nsAutoCString additionalPlacesConditions;

  if (!mIncludeHidden) {
    additionalPlacesConditions += NS_LITERAL_CSTRING("AND hidden = 0 ");
  }

  if (mQueryType == nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY) {
    // last_visit_date is updated for any kind of visit, so it's a good
    // indicator whether the page has visits.
    additionalPlacesConditions +=
        NS_LITERAL_CSTRING("AND last_visit_date NOTNULL ");
  }

  if (mResultType == nsINavHistoryQueryOptions::RESULTS_AS_URI &&
      !additionalVisitsConditions.IsEmpty()) {
    // URI results don't join on visits.
    nsAutoCString tmp = additionalVisitsConditions;
    additionalVisitsConditions =
        "AND EXISTS (SELECT 1 FROM moz_historyvisits WHERE place_id = h.id ";
    additionalVisitsConditions.Append(tmp);
    additionalVisitsConditions.AppendLiteral("LIMIT 1)");
  }

  mQueryString.ReplaceSubstring("{QUERY_OPTIONS_VISITS}",
                                additionalVisitsConditions.get());
  mQueryString.ReplaceSubstring("{QUERY_OPTIONS_PLACES}",
                                additionalPlacesConditions.get());

  // If we used WHERE already, we inject the conditions
  // in place of {ADDITIONAL_CONDITIONS}
  if (mQueryString.Find("{ADDITIONAL_CONDITIONS}") != kNotFound) {
    nsAutoCString innerCondition;
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

nsresult PlacesSQLQueryBuilder::GroupBy() {
  mQueryString += mGroupBy;
  return NS_OK;
}

nsresult PlacesSQLQueryBuilder::OrderBy() {
  if (mSkipOrderBy) return NS_OK;

  // Sort clause: we will sort later, but if it comes out of the DB sorted,
  // our later sort will be basically free. The DB can sort these for free
  // most of the time anyway, because it has indices over these items.
  switch (mSortingMode) {
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
      else if (mSortingMode ==
               nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING)
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
      break;  // Sort later in nsNavHistoryQueryResultNode::FillChildren()
    case nsINavHistoryQueryOptions::SORT_BY_FRECENCY_ASCENDING:
      OrderByColumnIndexAsc(nsNavHistory::kGetInfoIndex_Frecency);
      break;
    case nsINavHistoryQueryOptions::SORT_BY_FRECENCY_DESCENDING:
      OrderByColumnIndexDesc(nsNavHistory::kGetInfoIndex_Frecency);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid sorting mode");
  }
  return NS_OK;
}

void PlacesSQLQueryBuilder::OrderByColumnIndexAsc(int32_t aIndex) {
  mQueryString += nsPrintfCString(" ORDER BY %d ASC", aIndex + 1);
}

void PlacesSQLQueryBuilder::OrderByColumnIndexDesc(int32_t aIndex) {
  mQueryString += nsPrintfCString(" ORDER BY %d DESC", aIndex + 1);
}

void PlacesSQLQueryBuilder::OrderByTextColumnIndexAsc(int32_t aIndex) {
  mQueryString +=
      nsPrintfCString(" ORDER BY %d COLLATE NOCASE ASC", aIndex + 1);
}

void PlacesSQLQueryBuilder::OrderByTextColumnIndexDesc(int32_t aIndex) {
  mQueryString +=
      nsPrintfCString(" ORDER BY %d COLLATE NOCASE DESC", aIndex + 1);
}

nsresult PlacesSQLQueryBuilder::Limit() {
  if (mUseLimit && mMaxResults > 0) {
    mQueryString += NS_LITERAL_CSTRING(" LIMIT ");
    mQueryString.AppendInt(mMaxResults);
    mQueryString.Append(' ');
  }
  return NS_OK;
}

nsresult nsNavHistory::ConstructQueryString(
    const RefPtr<nsNavHistoryQuery>& aQuery,
    const RefPtr<nsNavHistoryQueryOptions>& aOptions, nsCString& queryString,
    bool& aParamsPresent, nsNavHistory::StringHash& aAddParams) {
  // For information about visit_type see nsINavHistoryService.idl.
  // visitType == 0 is undefined (see bug #375777 for details).
  // Some sites, especially Javascript-heavy ones, load things in frames to
  // display them, resulting in a lot of these entries. This is the reason
  // why such visits are filtered out.
  nsresult rv;
  aParamsPresent = false;

  int32_t sortingMode = aOptions->SortingMode();
  NS_ASSERTION(
      sortingMode >= nsINavHistoryQueryOptions::SORT_BY_NONE &&
          sortingMode <= nsINavHistoryQueryOptions::SORT_BY_FRECENCY_DESCENDING,
      "Invalid sortingMode found while building query!");

  bool hasSearchTerms = !aQuery->SearchTerms().IsEmpty();

  nsAutoCString tagsSqlFragment;
  GetTagsSqlFragment(GetTagsFolder(), NS_LITERAL_CSTRING("h.id"),
                     hasSearchTerms, tagsSqlFragment);

  if (IsOptimizableHistoryQuery(
          aQuery, aOptions,
          nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING) ||
      IsOptimizableHistoryQuery(
          aQuery, aOptions,
          nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING)) {
    // Generate an optimized query for the history menu and the old most visited
    // bookmark that was inserted into profiles.
    queryString =
        NS_LITERAL_CSTRING(
            "SELECT h.id, h.url, h.title AS page_title, h.rev_host, "
            "h.visit_count, h.last_visit_date, "
            "null, null, null, null, null, ") +
        tagsSqlFragment +
        NS_LITERAL_CSTRING(
            ", h.frecency, h.hidden, h.guid, "
            "null, null, null "
            "FROM moz_places h "
            "WHERE h.hidden = 0 "
            "AND EXISTS (SELECT id FROM moz_historyvisits WHERE place_id = "
            "h.id "
            "AND visit_type NOT IN ") +
        nsPrintfCString("(0,%d,%d) ", nsINavHistoryService::TRANSITION_EMBED,
                        nsINavHistoryService::TRANSITION_FRAMED_LINK) +
        NS_LITERAL_CSTRING(
            "LIMIT 1) "
            "{QUERY_OPTIONS} ");

    queryString.AppendLiteral("ORDER BY ");
    if (sortingMode == nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING)
      queryString.AppendLiteral("last_visit_date DESC ");
    else
      queryString.AppendLiteral("visit_count DESC ");

    queryString.AppendLiteral("LIMIT ");
    queryString.AppendInt(aOptions->MaxResults());

    nsAutoCString additionalQueryOptions;

    queryString.ReplaceSubstring("{QUERY_OPTIONS}",
                                 additionalQueryOptions.get());
    return NS_OK;
  }

  // If the query is a tag query, the type is bookmarks.
  if (!aQuery->Tags().IsEmpty()) {
    aOptions->SetQueryType(nsNavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS);
  }

  nsAutoCString conditions;
  nsCString queryClause;
  rv = QueryToSelectClause(aQuery, aOptions, &queryClause);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!queryClause.IsEmpty()) {
    // TODO: This should be set on a case basis, not blindly.
    aParamsPresent = true;
    conditions += queryClause;
  }

  // Determine whether we can push maxResults constraints into the query
  // as LIMIT, or if we need to do result count clamping later
  // using FilterResultSet()
  bool useLimitClause = !NeedToFilterResultSet(aQuery, aOptions);

  PlacesSQLQueryBuilder queryStringBuilder(
      conditions, aQuery, aOptions, useLimitClause, aAddParams, hasSearchTerms);
  rv = queryStringBuilder.GetQueryString(queryString);
  NS_ENSURE_SUCCESS(rv, rv);

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

nsresult nsNavHistory::GetQueryResults(
    nsNavHistoryQueryResultNode* aResultNode,
    const RefPtr<nsNavHistoryQuery>& aQuery,
    const RefPtr<nsNavHistoryQueryOptions>& aOptions,
    nsCOMArray<nsNavHistoryResultNode>* aResults) {
  NS_ENSURE_ARG_POINTER(aQuery);
  NS_ENSURE_ARG_POINTER(aOptions);
  NS_ASSERTION(aResults->Count() == 0, "Initial result array must be empty");

  nsCString queryString;
  bool paramsPresent = false;
  nsNavHistory::StringHash addParams(HISTORY_DATE_CONT_LENGTH);
  nsresult rv = ConstructQueryString(aQuery, aOptions, queryString,
                                     paramsPresent, addParams);
  NS_ENSURE_SUCCESS(rv, rv);

  // create statement
  nsCOMPtr<mozIStorageStatement> statement = mDB->GetStatement(queryString);
#ifdef DEBUG
  if (!statement) {
    nsCOMPtr<mozIStorageConnection> conn = mDB->MainConn();
    if (conn) {
      nsAutoCString lastErrorString;
      (void)conn->GetLastErrorString(lastErrorString);
      int32_t lastError = 0;
      (void)conn->GetLastError(&lastError);
      printf(
          "Places failed to create a statement from this query:\n%s\nStorage "
          "error (%d): %s\n",
          queryString.get(), lastError, lastErrorString.get());
    }
  }
#endif
  NS_ENSURE_STATE(statement);
  mozStorageStatementScoper scoper(statement);

  if (paramsPresent) {
    rv = BindQueryClauseParameters(statement, aQuery, aOptions);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  for (auto iter = addParams.Iter(); !iter.Done(); iter.Next()) {
    nsresult rv = statement->BindUTF8StringByName(iter.Key(), iter.Data());
    if (NS_FAILED(rv)) {
      break;
    }
  }

  // Optimize the case where there is no need for any post-query filtering.
  if (NeedToFilterResultSet(aQuery, aOptions)) {
    // Generate the top-level results.
    nsCOMArray<nsNavHistoryResultNode> toplevel;
    rv = ResultsAsList(statement, aOptions, &toplevel);
    NS_ENSURE_SUCCESS(rv, rv);

    FilterResultSet(aResultNode, toplevel, aResults, aQuery, aOptions);
  } else {
    rv = ResultsAsList(statement, aOptions, aResults);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::AddObserver(nsINavHistoryObserver* aObserver, bool aOwnsWeak) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aObserver);

  if (NS_WARN_IF(!mCanNotify)) return NS_ERROR_UNEXPECTED;

  return mObservers.AppendWeakElementUnlessExists(aObserver, aOwnsWeak);
}

NS_IMETHODIMP
nsNavHistory::RemoveObserver(nsINavHistoryObserver* aObserver) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aObserver);

  return mObservers.RemoveWeakElement(aObserver);
}

NS_IMETHODIMP
nsNavHistory::GetObservers(
    nsTArray<RefPtr<nsINavHistoryObserver>>& aObservers) {
  aObservers.Clear();

  // Clear any cached value, cause it's very likely the consumer has made
  // changes to history and is now trying to notify them.
  mDaysOfHistory = -1;

  if (!mCanNotify) return NS_OK;

  // Then add the other observers.
  for (uint32_t i = 0; i < mObservers.Length(); ++i) {
    nsCOMPtr<nsINavHistoryObserver> observer =
        mObservers.ElementAt(i).GetValue();
    // Skip nullified weak observers.
    if (observer) {
      aObservers.AppendElement(observer.forget());
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::GetHistoryDisabled(bool* _retval) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = IsHistoryDisabled();
  return NS_OK;
}

// Call this method before visiting a URL in order to help determine the
// transition type of the visit.
//
// @see MarkPageAsFollowedBookmark

NS_IMETHODIMP
nsNavHistory::MarkPageAsTyped(nsIURI* aURI) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // don't add when history is disabled
  if (IsHistoryDisabled()) return NS_OK;

  nsAutoCString uriString;
  nsresult rv = aURI->GetSpec(uriString);
  NS_ENSURE_SUCCESS(rv, rv);

  mRecentTyped.Put(uriString, GetNow());

  if (mRecentTyped.Count() > RECENT_EVENT_QUEUE_MAX_LENGTH)
    ExpireNonrecentEvents(&mRecentTyped);

  return NS_OK;
}

// Call this method before visiting a URL in order to help determine the
// transition type of the visit.
//
// @see MarkPageAsTyped

NS_IMETHODIMP
nsNavHistory::MarkPageAsFollowedLink(nsIURI* aURI) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);

  // don't add when history is disabled
  if (IsHistoryDisabled()) return NS_OK;

  nsAutoCString uriString;
  nsresult rv = aURI->GetSpec(uriString);
  NS_ENSURE_SUCCESS(rv, rv);

  mRecentLink.Put(uriString, GetNow());

  if (mRecentLink.Count() > RECENT_EVENT_QUEUE_MAX_LENGTH)
    ExpireNonrecentEvents(&mRecentLink);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageVacuumParticipant

NS_IMETHODIMP
nsNavHistory::GetDatabaseConnection(mozIStorageConnection** _DBConnection) {
  return GetDBConnection(_DBConnection);
}

NS_IMETHODIMP
nsNavHistory::GetExpectedDatabasePageSize(int32_t* _expectedPageSize) {
  NS_ENSURE_STATE(mDB);
  NS_ENSURE_STATE(mDB->MainConn());
  return mDB->MainConn()->GetDefaultPageSize(_expectedPageSize);
}

NS_IMETHODIMP
nsNavHistory::OnBeginVacuum(bool* _vacuumGranted) {
  // TODO: Check if we have to deny the vacuum in some heavy-load case.
  // We could maybe want to do that during batches?
  *_vacuumGranted = true;
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::OnEndVacuum(bool aSucceeded) {
  NS_WARNING_ASSERTION(aSucceeded, "Places.sqlite vacuum failed.");
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::GetDBConnection(mozIStorageConnection** _DBConnection) {
  NS_ENSURE_ARG_POINTER(_DBConnection);
  nsCOMPtr<mozIStorageConnection> connection = mDB->MainConn();
  connection.forget(_DBConnection);

  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::GetShutdownClient(nsIAsyncShutdownClient** _shutdownClient) {
  NS_ENSURE_ARG_POINTER(_shutdownClient);
  nsCOMPtr<nsIAsyncShutdownClient> client = mDB->GetClientsShutdown();
  if (!client) {
    return NS_ERROR_UNEXPECTED;
  }
  client.forget(_shutdownClient);
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::GetConnectionShutdownClient(
    nsIAsyncShutdownClient** _shutdownClient) {
  NS_ENSURE_ARG_POINTER(_shutdownClient);
  nsCOMPtr<nsIAsyncShutdownClient> client = mDB->GetConnectionShutdown();
  if (!client) {
    return NS_ERROR_UNEXPECTED;
  }
  client.forget(_shutdownClient);
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::AsyncExecuteLegacyQuery(nsINavHistoryQuery* aQuery,
                                      nsINavHistoryQueryOptions* aOptions,
                                      mozIStorageStatementCallback* aCallback,
                                      mozIStoragePendingStatement** _stmt) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aQuery);
  NS_ENSURE_ARG(aOptions);
  NS_ENSURE_ARG(aCallback);
  NS_ENSURE_ARG_POINTER(_stmt);

  RefPtr<nsNavHistoryQuery> query = do_QueryObject(aQuery);
  NS_ENSURE_STATE(query);
  RefPtr<nsNavHistoryQueryOptions> options = do_QueryObject(aOptions);
  NS_ENSURE_ARG(options);

  nsCString queryString;
  bool paramsPresent = false;
  nsNavHistory::StringHash addParams(HISTORY_DATE_CONT_LENGTH);
  nsresult rv = ConstructQueryString(query, options, queryString, paramsPresent,
                                     addParams);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageAsyncStatement> statement =
      mDB->GetAsyncStatement(queryString);
  NS_ENSURE_STATE(statement);

#ifdef DEBUG
  if (NS_FAILED(rv)) {
    nsCOMPtr<mozIStorageConnection> conn = mDB->MainConn();
    if (conn) {
      nsAutoCString lastErrorString;
      (void)mDB->MainConn()->GetLastErrorString(lastErrorString);
      int32_t lastError = 0;
      (void)mDB->MainConn()->GetLastError(&lastError);
      printf(
          "Places failed to create a statement from this query:\n%s\nStorage "
          "error (%d): %s\n",
          queryString.get(), lastError, lastErrorString.get());
    }
  }
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  if (paramsPresent) {
    rv = BindQueryClauseParameters(statement, query, options);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  for (auto iter = addParams.Iter(); !iter.Done(); iter.Next()) {
    nsresult rv = statement->BindUTF8StringByName(iter.Key(), iter.Data());
    if (NS_FAILED(rv)) {
      break;
    }
  }

  rv = statement->ExecuteAsync(aCallback, _stmt);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsNavHistory::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* aData) {
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  if (strcmp(aTopic, TOPIC_PROFILE_TEARDOWN) == 0 ||
      strcmp(aTopic, TOPIC_PROFILE_CHANGE) == 0 ||
      strcmp(aTopic, TOPIC_SIMULATE_PLACES_SHUTDOWN) == 0) {
    // These notifications are used by tests to simulate a Places shutdown.
    // They should just be forwarded to the Database handle.
    mDB->Observe(aSubject, aTopic, aData);
  }

  else if (strcmp(aTopic, TOPIC_PLACES_CONNECTION_CLOSED) == 0) {
    // Don't even try to notify observers from this point on, the category
    // cache would init services that could try to use our APIs.
    mCanNotify = false;
    mObservers.Clear();
  }

  else if (strcmp(aTopic, TOPIC_PREF_CHANGED) == 0) {
    LoadPrefs();
  }

  else if (strcmp(aTopic, TOPIC_IDLE_DAILY) == 0) {
    (void)DecayFrecency();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::DecayFrecency() {
  float decayRate =
      Preferences::GetFloat(PREF_FREC_DECAY_RATE, PREF_FREC_DECAY_RATE_DEF);
  if (decayRate > 1.0f) {
    MOZ_ASSERT(false, "The frecency decay rate should not be greater than 1.0");
    decayRate = PREF_FREC_DECAY_RATE_DEF;
  }

  RefPtr<FixAndDecayFrecencyRunnable> runnable =
      new FixAndDecayFrecencyRunnable(mDB, decayRate);
  nsCOMPtr<nsIEventTarget> target = do_GetInterface(mDB->MainConn());
  NS_ENSURE_STATE(target);

  mDecayFrecencyPendingCount++;
  return target->Dispatch(runnable, NS_DISPATCH_NORMAL);
}

void nsNavHistory::DecayFrecencyCompleted(uint16_t reason) {
  MOZ_ASSERT(mDecayFrecencyPendingCount > 0);
  mDecayFrecencyPendingCount--;
  if (mozIStorageStatementCallback::REASON_FINISHED == reason) {
    NotifyManyFrecenciesChanged();
  }
}

bool nsNavHistory::IsFrecencyDecaying() const {
  return mDecayFrecencyPendingCount > 0;
}

// Query stuff *****************************************************************

// Helper class for QueryToSelectClause
//
// This class helps to build part of the WHERE clause.

class ConditionBuilder {
 public:
  ConditionBuilder& Condition(const char* aStr) {
    if (!mClause.IsEmpty()) mClause.AppendLiteral(" AND ");
    Str(aStr);
    return *this;
  }

  ConditionBuilder& Str(const char* aStr) {
    mClause.Append(' ');
    mClause.Append(aStr);
    mClause.Append(' ');
    return *this;
  }

  ConditionBuilder& Param(const char* aParam) {
    mClause.Append(' ');
    mClause.Append(aParam);
    mClause.Append(' ');
    return *this;
  }

  void GetClauseString(nsCString& aResult) { aResult = mClause; }

 private:
  nsCString mClause;
};

// nsNavHistory::QueryToSelectClause
//
//    THE BEHAVIOR SHOULD BE IN SYNC WITH BindQueryClauseParameters
//
//    I don't check return values from the query object getters because there's
//    no way for those to fail.

nsresult nsNavHistory::QueryToSelectClause(
    const RefPtr<nsNavHistoryQuery>& aQuery,
    const RefPtr<nsNavHistoryQueryOptions>& aOptions, nsCString* aClause) {
  bool hasIt;
  // We don't use the value from options here - we post filter if that
  // is set.
  bool excludeQueries = false;

  ConditionBuilder clause;

  if ((NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt) ||
      (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt)) {
    clause.Condition(
        "EXISTS (SELECT 1 FROM moz_historyvisits "
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
  int32_t searchBehavior = mozIPlacesAutoComplete::BEHAVIOR_HISTORY |
                           mozIPlacesAutoComplete::BEHAVIOR_BOOKMARK;
  if (!aQuery->SearchTerms().IsEmpty()) {
    // Re-use the autocomplete_match function.  Setting the behavior to match
    // history or typed history or bookmarks or open pages will match almost
    // everything.
    clause.Condition("AUTOCOMPLETE_MATCH(")
        .Param(":search_string")
        .Str(", h.url, page_title, tags, ")
        .Str(nsPrintfCString("1, 1, 1, 1, %d, %d)",
                             mozIPlacesAutoComplete::MATCH_ANYWHERE_UNMODIFIED,
                             searchBehavior)
                 .get());
    // Serching by terms implicitly exclude queries.
    excludeQueries = true;
  }

  // min and max visit count
  if (aQuery->MinVisits() >= 0)
    clause.Condition("h.visit_count >=").Param(":min_visits");

  if (aQuery->MaxVisits() >= 0)
    clause.Condition("h.visit_count <=").Param(":max_visits");

  // only bookmarked, has no affect on bookmarks-only queries
  if (aOptions->QueryType() !=
          nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS &&
      aQuery->OnlyBookmarked())
    clause.Condition("EXISTS (SELECT b.fk FROM moz_bookmarks b WHERE b.type = ")
        .Str(nsPrintfCString("%d", nsNavBookmarks::TYPE_BOOKMARK).get())
        .Str("AND b.fk = h.id)");

  // domain
  if (!aQuery->Domain().IsVoid()) {
    bool domainIsHost = false;
    aQuery->GetDomainIsHost(&domainIsHost);
    if (domainIsHost)
      clause.Condition("h.rev_host =").Param(":domain_lower");
    else
      // see domain setting in BindQueryClauseParameters for why we do this
      clause.Condition("h.rev_host >=")
          .Param(":domain_lower")
          .Condition("h.rev_host <")
          .Param(":domain_upper");
  }

  // URI
  if (aQuery->Uri()) {
    clause.Condition("h.url_hash = hash(")
        .Param(":uri")
        .Str(")")
        .Condition("h.url =")
        .Param(":uri");
  }

  // annotation
  if (!aQuery->Annotation().IsEmpty()) {
    clause.Condition("");
    if (aQuery->AnnotationIsNot()) clause.Str("NOT");
    clause
        .Str(
            "EXISTS "
            "(SELECT h.id "
            "FROM moz_annos anno "
            "JOIN moz_anno_attributes annoname "
            "ON anno.anno_attribute_id = annoname.id "
            "WHERE anno.place_id = h.id "
            "AND annoname.name = ")
        .Param(":anno")
        .Str(")");
    // annotation-based queries don't get the common conditions, so you get
    // all URLs with that annotation
  }

  // tags
  const nsTArray<nsString>& tags = aQuery->Tags();
  if (tags.Length() > 0) {
    clause.Condition("h.id");
    if (aQuery->TagsAreNot()) clause.Str("NOT");
    clause
        .Str(
            "IN "
            "(SELECT bms.fk "
            "FROM moz_bookmarks bms "
            "JOIN moz_bookmarks tags ON bms.parent = tags.id "
            "WHERE tags.parent =")
        .Param(":tags_folder")
        .Str("AND lower(tags.title) IN (");
    for (uint32_t i = 0; i < tags.Length(); ++i) {
      nsPrintfCString param(":tag%d_", i);
      clause.Param(param.get());
      if (i < tags.Length() - 1) clause.Str(",");
    }
    clause.Str(")");
    if (!aQuery->TagsAreNot()) {
      clause.Str("GROUP BY bms.fk HAVING count(*) >=").Param(":tag_count");
    }
    clause.Str(")");
  }

  // transitions
  const nsTArray<uint32_t>& transitions = aQuery->Transitions();
  for (uint32_t i = 0; i < transitions.Length(); ++i) {
    nsPrintfCString param(":transition%d_", i);
    clause
        .Condition(
            "h.id IN (SELECT place_id FROM moz_historyvisits "
            "WHERE visit_type = ")
        .Param(param.get())
        .Str(")");
  }

  // parents
  const nsTArray<nsCString>& parents = aQuery->Parents();
  if (parents.Length() > 0) {
    aOptions->SetQueryType(nsNavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS);
    clause.Condition(
        "b.parent IN( "
        "WITH RECURSIVE parents(id) AS ( "
        "SELECT id FROM moz_bookmarks WHERE GUID IN (");

    for (uint32_t i = 0; i < parents.Length(); ++i) {
      nsPrintfCString param(":parentguid%d_", i);
      clause.Param(param.get());
      if (i < parents.Length() - 1) {
        clause.Str(",");
      }
    }
    clause.Str(
        ") "
        "UNION ALL "
        "SELECT b2.id "
        "FROM moz_bookmarks b2 "
        "JOIN parents p ON b2.parent = p.id "
        "WHERE b2.type = 2 "
        ") "
        "SELECT id FROM parents "
        ")");
  }

  if (excludeQueries) {
    // Serching by terms implicitly exclude queries and folder shortcuts.
    clause.Condition(
        "NOT h.url_hash BETWEEN hash('place', 'prefix_lo') AND "
        "hash('place', 'prefix_hi')");
  }

  clause.GetClauseString(*aClause);
  return NS_OK;
}

// nsNavHistory::BindQueryClauseParameters
//
//    THE BEHAVIOR SHOULD BE IN SYNC WITH QueryToSelectClause

nsresult nsNavHistory::BindQueryClauseParameters(
    mozIStorageBaseStatement* statement,
    const RefPtr<nsNavHistoryQuery>& aQuery,
    const RefPtr<nsNavHistoryQueryOptions>& aOptions) {
  nsresult rv;

  bool hasIt;
  // begin time
  if (NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt) {
    PRTime time =
        NormalizeTime(aQuery->BeginTimeReference(), aQuery->BeginTime());
    rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("begin_time"), time);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // end time
  if (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt) {
    PRTime time = NormalizeTime(aQuery->EndTimeReference(), aQuery->EndTime());
    rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("end_time"), time);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // search terms
  if (!aQuery->SearchTerms().IsEmpty()) {
    rv = statement->BindStringByName(NS_LITERAL_CSTRING("search_string"),
                                     aQuery->SearchTerms());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // min and max visit count
  int32_t visits = aQuery->MinVisits();
  if (visits >= 0) {
    rv = statement->BindInt32ByName(NS_LITERAL_CSTRING("min_visits"), visits);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  visits = aQuery->MaxVisits();
  if (visits >= 0) {
    rv = statement->BindInt32ByName(NS_LITERAL_CSTRING("max_visits"), visits);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // domain (see GetReversedHostname for more info on reversed host names)
  if (!aQuery->Domain().IsVoid()) {
    nsString revDomain;
    GetReversedHostname(NS_ConvertUTF8toUTF16(aQuery->Domain()), revDomain);

    if (aQuery->DomainIsHost()) {
      rv = statement->BindStringByName(NS_LITERAL_CSTRING("domain_lower"),
                                       revDomain);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // for "mozilla.org" do query >= "gro.allizom." AND < "gro.allizom/"
      // which will get everything starting with "gro.allizom." while using the
      // index (using SUBSTRING() causes indexes to be discarded).
      NS_ASSERTION(revDomain[revDomain.Length() - 1] == '.',
                   "Invalid rev. host");
      rv = statement->BindStringByName(NS_LITERAL_CSTRING("domain_lower"),
                                       revDomain);
      NS_ENSURE_SUCCESS(rv, rv);
      revDomain.Truncate(revDomain.Length() - 1);
      revDomain.Append(char16_t('/'));
      rv = statement->BindStringByName(NS_LITERAL_CSTRING("domain_upper"),
                                       revDomain);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // URI
  if (aQuery->Uri()) {
    rv = URIBinder::Bind(statement, NS_LITERAL_CSTRING("uri"), aQuery->Uri());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // annotation
  if (!aQuery->Annotation().IsEmpty()) {
    rv = statement->BindUTF8StringByName(NS_LITERAL_CSTRING("anno"),
                                         aQuery->Annotation());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // tags
  const nsTArray<nsString>& tags = aQuery->Tags();
  if (tags.Length() > 0) {
    for (uint32_t i = 0; i < tags.Length(); ++i) {
      nsPrintfCString paramName("tag%d_", i);
      nsString utf16Tag = tags[i];
      ToLowerCase(utf16Tag);
      NS_ConvertUTF16toUTF8 tag(utf16Tag);
      rv = statement->BindUTF8StringByName(paramName, tag);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    int64_t tagsFolder = GetTagsFolder();
    rv = statement->BindInt64ByName(NS_LITERAL_CSTRING("tags_folder"),
                                    tagsFolder);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!aQuery->TagsAreNot()) {
      rv = statement->BindInt32ByName(NS_LITERAL_CSTRING("tag_count"),
                                      tags.Length());
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // transitions
  const nsTArray<uint32_t>& transitions = aQuery->Transitions();
  for (uint32_t i = 0; i < transitions.Length(); ++i) {
    nsPrintfCString paramName("transition%d_", i);
    rv = statement->BindInt64ByName(paramName, transitions[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // parents
  const nsTArray<nsCString>& parents = aQuery->Parents();
  for (uint32_t i = 0; i < parents.Length(); ++i) {
    nsPrintfCString paramName("parentguid%d_", i);
    rv = statement->BindUTF8StringByName(paramName, parents[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// nsNavHistory::ResultsAsList
//

nsresult nsNavHistory::ResultsAsList(
    mozIStorageStatement* statement, nsNavHistoryQueryOptions* aOptions,
    nsCOMArray<nsNavHistoryResultNode>* aResults) {
  nsresult rv;
  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(statement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore = false;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasMore)) && hasMore) {
    RefPtr<nsNavHistoryResultNode> result;
    rv = RowToResult(row, aOptions, getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);
    aResults->AppendElement(result.forget());
  }
  return NS_OK;
}

const int64_t UNDEFINED_URN_VALUE = -1;

// Create a urn (like
// urn:places-persist:place:group=0&group=1&sort=1&type=1,,%28local%20files%29)
// to be used to persist the open state of this container
nsresult CreatePlacesPersistURN(nsNavHistoryQueryResultNode* aResultNode,
                                int64_t aValue, const nsCString& aTitle,
                                nsCString& aURN) {
  nsAutoCString uri;
  nsresult rv = aResultNode->GetUri(uri);
  NS_ENSURE_SUCCESS(rv, rv);

  aURN.AssignLiteral("urn:places-persist:");
  aURN.Append(uri);

  aURN.Append(',');
  if (aValue != UNDEFINED_URN_VALUE) aURN.AppendInt(aValue);

  aURN.Append(',');
  if (!aTitle.IsEmpty()) {
    nsAutoCString escapedTitle;
    bool success = NS_Escape(aTitle, escapedTitle, url_XAlphas);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
    aURN.Append(escapedTitle);
  }

  return NS_OK;
}

int64_t nsNavHistory::GetTagsFolder() {
  // cache our tags folder
  // note, we can't do this in nsNavHistory::Init(),
  // as getting the bookmarks service would initialize it.
  if (mTagsFolder == -1) {
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, -1);

    nsresult rv = bookmarks->GetTagsFolder(&mTagsFolder);
    NS_ENSURE_SUCCESS(rv, -1);
  }
  return mTagsFolder;
}

// nsNavHistory::FilterResultSet
//
// This does some post-query-execution filtering:
//   - searching on title, url and tags
//   - limit count
//
// Note:  changes to filtering in FilterResultSet()
// may require changes to NeedToFilterResultSet()

// static
nsresult nsNavHistory::FilterResultSet(
    nsNavHistoryQueryResultNode* aQueryNode,
    const nsCOMArray<nsNavHistoryResultNode>& aSet,
    nsCOMArray<nsNavHistoryResultNode>* aFiltered,
    const RefPtr<nsNavHistoryQuery>& aQuery,
    nsNavHistoryQueryOptions* aOptions) {
  // parse the search terms
  nsTArray<nsString> terms;
  ParseSearchTermsFromQuery(aQuery, &terms);

  bool excludeQueries = aOptions->ExcludeQueries();
  for (int32_t nodeIndex = 0; nodeIndex < aSet.Count(); nodeIndex++) {
    if (excludeQueries && aSet[nodeIndex]->IsQuery()) {
      continue;
    }

    if (aSet[nodeIndex]->mItemId != -1 && aQueryNode &&
        aQueryNode->mItemId == aSet[nodeIndex]->mItemId) {
      continue;
    }

    // If there are search terms, we are already getting only uri nodes,
    // thus we don't need to filter node types. Though, we must check for
    // matching terms.
    if (terms.Length()) {
      // Filter based on search terms.
      // Convert title and url for the current node to UTF16 strings.
      NS_ConvertUTF8toUTF16 nodeTitle(aSet[nodeIndex]->mTitle);
      // Unescape the URL for search terms matching.
      nsAutoCString cNodeURL(aSet[nodeIndex]->mURI);
      NS_ConvertUTF8toUTF16 nodeURL(NS_UnescapeURL(cNodeURL));

      // Determine if every search term matches anywhere in the title, url or
      // tag.
      bool matchAllTerms = true;
      for (int32_t termIndex = terms.Length() - 1;
           termIndex >= 0 && matchAllTerms; termIndex--) {
        nsString& term = terms.ElementAt(termIndex);
        // True if any of them match; false makes us quit the loop
        matchAllTerms =
            CaseInsensitiveFindInReadable(term, nodeTitle) ||
            CaseInsensitiveFindInReadable(term, nodeURL) ||
            CaseInsensitiveFindInReadable(term, aSet[nodeIndex]->mTags);
      }
      // Skip the node if we don't match all terms in the title, url or tag
      if (!matchAllTerms) {
        continue;
      }
    }

    aFiltered->AppendObject(aSet[nodeIndex]);

    // Stop once we have reached max results.
    if (aOptions->MaxResults() > 0 &&
        (uint32_t)aFiltered->Count() >= aOptions->MaxResults())
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::MakeGuid(nsACString& aGuid) {
  if (NS_FAILED(GenerateGUID(aGuid))) {
    MOZ_ASSERT(false, "Shouldn't fail to create a guid!");
    aGuid.SetIsVoid(true);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::HashURL(const nsACString& aSpec, const nsACString& aMode,
                      uint64_t* _hash) {
  return places::HashURL(aSpec, aMode, _hash);
}

// nsNavHistory::CheckIsRecentEvent
//
//    Sees if this URL happened "recently."
//
//    It is always removed from our recent list no matter what. It only counts
//    as "recent" if the event happened more recently than our event
//    threshold ago.

bool nsNavHistory::CheckIsRecentEvent(RecentEventHash* hashTable,
                                      const nsACString& url) {
  PRTime eventTime;
  if (hashTable->Get(url, reinterpret_cast<int64_t*>(&eventTime))) {
    hashTable->Remove(url);
    if (eventTime > GetNow() - RECENT_EVENT_THRESHOLD) return true;
    return false;
  }
  return false;
}

// nsNavHistory::ExpireNonrecentEvents
//
//    This goes through our

void nsNavHistory::ExpireNonrecentEvents(RecentEventHash* hashTable) {
  int64_t threshold = GetNow() - RECENT_EVENT_THRESHOLD;
  for (auto iter = hashTable->Iter(); !iter.Done(); iter.Next()) {
    if (iter.Data() < threshold) {
      iter.Remove();
    }
  }
}

// nsNavHistory::RowToResult
//
//    Here, we just have a generic row. It could be a query, URL, visit,
//    or full visit.

nsresult nsNavHistory::RowToResult(mozIStorageValueArray* aRow,
                                   nsNavHistoryQueryOptions* aOptions,
                                   nsNavHistoryResultNode** aResult) {
  NS_ASSERTION(aRow && aOptions && aResult, "Null pointer in RowToResult");

  // URL
  nsAutoCString url;
  nsresult rv = aRow->GetUTF8String(kGetInfoIndex_URL, url);
  NS_ENSURE_SUCCESS(rv, rv);
  // In case of data corruption URL may be null, but our UI code prefers an
  // empty string.
  if (url.IsVoid()) {
    MOZ_ASSERT(false, "Found a NULL url in moz_places");
    url.SetIsVoid(false);
  }

  // title
  nsAutoCString title;
  bool isNull;
  rv = aRow->GetIsNull(kGetInfoIndex_Title, &isNull);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!isNull) {
    rv = aRow->GetUTF8String(kGetInfoIndex_Title, title);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  uint32_t accessCount = aRow->AsInt32(kGetInfoIndex_VisitCount);
  PRTime time = aRow->AsInt64(kGetInfoIndex_VisitDate);

  // itemId
  int64_t itemId = aRow->AsInt64(kGetInfoIndex_ItemId);
  int64_t parentId = -1;
  if (itemId == 0) {
    // This is not a bookmark.  For non-bookmarks we use a -1 itemId value.
    // Notice ids in sqlite tables start from 1, so itemId cannot ever be 0.
    itemId = -1;
  } else {
    // This is a bookmark, so it has a parent.
    int64_t itemParentId = aRow->AsInt64(kGetInfoIndex_ItemParentId);
    if (itemParentId > 0) {
      // The Places root has parent == 0, but that item id does not really
      // exist. We want to set the parent only if it's a real one.
      parentId = itemParentId;
    }
  }

  if (IsQueryURI(url)) {
    // Special case "place:" URIs: turn them into containers.
    if (itemId != -1) {
      // We should never expose the history title for query nodes if the
      // bookmark-item's title is set to null (the history title may be the
      // query string without the place: prefix). Thus we call getItemTitle
      // explicitly. Doing this in the SQL query would be less performant since
      // it should be done for all results rather than only for queries.
      nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
      NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

      rv = bookmarks->GetItemTitle(itemId, title);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsAutoCString guid;
    if (itemId != -1) {
      rv = aRow->GetUTF8String(nsNavBookmarks::kGetChildrenIndex_Guid, guid);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (aOptions->ResultType() ==
            nsNavHistoryQueryOptions::RESULTS_AS_ROOTS_QUERY ||
        aOptions->ResultType() ==
            nsNavHistoryQueryOptions::RESULTS_AS_LEFT_PANE_QUERY) {
      rv = aRow->GetUTF8String(kGetInfoIndex_Guid, guid);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    RefPtr<nsNavHistoryResultNode> resultNode;
    rv = QueryRowToResult(itemId, guid, url, title, accessCount, time,
                          getter_AddRefs(resultNode));
    NS_ENSURE_SUCCESS(rv, rv);

    if (itemId != -1 || aOptions->ResultType() ==
                            nsNavHistoryQueryOptions::RESULTS_AS_TAGS_ROOT) {
      // RESULTS_AS_TAGS_ROOT has date columns
      resultNode->mDateAdded = aRow->AsInt64(kGetInfoIndex_ItemDateAdded);
      resultNode->mLastModified = aRow->AsInt64(kGetInfoIndex_ItemLastModified);
      if (resultNode->IsFolder()) {
        // If it's a simple folder node (i.e. a shortcut to another folder),
        // apply our options for it. However, if the parent type was tag query,
        // we do not apply them, because it would not yield any results.
        resultNode->GetAsContainer()->mOptions = aOptions;
      }
    }

    resultNode.forget(aResult);
    return rv;
  } else if (aOptions->ResultType() ==
             nsNavHistoryQueryOptions::RESULTS_AS_URI) {
    RefPtr<nsNavHistoryResultNode> resultNode =
        new nsNavHistoryResultNode(url, title, accessCount, time);

    if (itemId != -1) {
      resultNode->mItemId = itemId;
      resultNode->mFolderId = parentId;
      resultNode->mDateAdded = aRow->AsInt64(kGetInfoIndex_ItemDateAdded);
      resultNode->mLastModified = aRow->AsInt64(kGetInfoIndex_ItemLastModified);

      rv = aRow->GetUTF8String(nsNavBookmarks::kGetChildrenIndex_Guid,
                               resultNode->mBookmarkGuid);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    resultNode->mFrecency = aRow->AsInt32(kGetInfoIndex_Frecency);
    resultNode->mHidden = !!aRow->AsInt32(kGetInfoIndex_Hidden);

    nsAutoString tags;
    rv = aRow->GetString(kGetInfoIndex_ItemTags, tags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!tags.IsVoid()) {
      resultNode->mTags.Assign(tags);
    }

    rv = aRow->GetUTF8String(kGetInfoIndex_Guid, resultNode->mPageGuid);
    NS_ENSURE_SUCCESS(rv, rv);

    resultNode.forget(aResult);
    return NS_OK;
  }

  if (aOptions->ResultType() == nsNavHistoryQueryOptions::RESULTS_AS_VISIT) {
    RefPtr<nsNavHistoryResultNode> resultNode =
        new nsNavHistoryResultNode(url, title, accessCount, time);

    nsAutoString tags;
    rv = aRow->GetString(kGetInfoIndex_ItemTags, tags);
    if (!tags.IsVoid()) resultNode->mTags.Assign(tags);

    rv = aRow->GetUTF8String(kGetInfoIndex_Guid, resultNode->mPageGuid);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aRow->GetInt64(kGetInfoIndex_VisitId, &resultNode->mVisitId);
    NS_ENSURE_SUCCESS(rv, rv);

    int64_t fromVisitId;
    rv = aRow->GetInt64(kGetInfoIndex_FromVisitId, &fromVisitId);
    NS_ENSURE_SUCCESS(rv, rv);

    if (fromVisitId > 0) {
      resultNode->mFromVisitId = fromVisitId;
    }

    resultNode->mTransitionType = aRow->AsInt32(kGetInfoIndex_VisitType);

    resultNode.forget(aResult);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

// nsNavHistory::QueryRowToResult
//
//    Called by RowToResult when the URI is a place: URI to generate the proper
//    folder or query node.

nsresult nsNavHistory::QueryRowToResult(int64_t itemId,
                                        const nsACString& aBookmarkGuid,
                                        const nsACString& aURI,
                                        const nsACString& aTitle,
                                        uint32_t aAccessCount, PRTime aTime,
                                        nsNavHistoryResultNode** aNode) {
  // Only assert if the itemId is set. In some cases (e.g. virtual queries), we
  // have a guid, but not an itemId.
  if (itemId != -1) {
    MOZ_ASSERT(!aBookmarkGuid.IsEmpty());
  }

  nsCOMPtr<nsINavHistoryQuery> query;
  nsCOMPtr<nsINavHistoryQueryOptions> options;
  nsresult rv =
      QueryStringToQuery(aURI, getter_AddRefs(query), getter_AddRefs(options));
  RefPtr<nsNavHistoryResultNode> resultNode;
  RefPtr<nsNavHistoryQuery> queryObj = do_QueryObject(query);
  NS_ENSURE_STATE(queryObj);
  RefPtr<nsNavHistoryQueryOptions> optionsObj = do_QueryObject(options);
  NS_ENSURE_STATE(optionsObj);
  // If this failed the query does not parse correctly, let the error pass and
  // handle it later.
  if (NS_SUCCEEDED(rv)) {
    // Check if this is a folder shortcut, so we can take a faster path.
    nsCString targetFolderGuid =
        GetSimpleBookmarksQueryParent(queryObj, optionsObj);
    if (!targetFolderGuid.IsEmpty()) {
      nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
      NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

      rv = bookmarks->ResultNodeForContainer(targetFolderGuid, optionsObj,
                                             getter_AddRefs(resultNode));
      // If this failed the shortcut is pointing to nowhere, let the error pass
      // and handle it later.
      if (NS_SUCCEEDED(rv)) {
        // At this point the node is set up like a regular folder node. Here
        // we make the necessary change to make it a folder shortcut.
        resultNode->mItemId = itemId;
        resultNode->mBookmarkGuid = aBookmarkGuid;
        resultNode->GetAsFolder()->mTargetFolderGuid = targetFolderGuid;

        // Use the query item title, unless it's empty (in that case use the
        // concrete folder title).
        if (!aTitle.IsEmpty()) {
          resultNode->mTitle = aTitle;
        }
      }
    } else {
      // This is a regular query.
      resultNode = new nsNavHistoryQueryResultNode(aTitle, aTime, aURI,
                                                   queryObj, optionsObj);
      resultNode->mItemId = itemId;
      resultNode->mBookmarkGuid = aBookmarkGuid;
    }
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("Generating a generic empty node for a broken query!");
    // This is a broken query, that either did not parse or points to not
    // existing data.  We don't want to return failure since that will kill the
    // whole result.  Instead make a generic empty query node.
    resultNode =
        new nsNavHistoryQueryResultNode(aTitle, 0, aURI, queryObj, optionsObj);
    resultNode->mItemId = itemId;
    resultNode->mBookmarkGuid = aBookmarkGuid;
    // This is a perf hack to generate an empty query that skips filtering.
    resultNode->GetAsQuery()->Options()->SetExcludeItems(true);
  }

  resultNode.forget(aNode);
  return NS_OK;
}

// nsNavHistory::VisitIdToResultNode
//
//    Used by the query results to create new nodes on the fly when
//    notifications come in. This just creates a node for the given visit ID.

nsresult nsNavHistory::VisitIdToResultNode(int64_t visitId,
                                           nsNavHistoryQueryOptions* aOptions,
                                           nsNavHistoryResultNode** aResult) {
  nsAutoCString tagsFragment;
  GetTagsSqlFragment(GetTagsFolder(), NS_LITERAL_CSTRING("h.id"), true,
                     tagsFragment);

  nsCOMPtr<mozIStorageStatement> statement;
  switch (aOptions->ResultType()) {
    case nsNavHistoryQueryOptions::RESULTS_AS_VISIT:
      // visit query - want exact visit time
      // Should match kGetInfoIndex_* (see GetQueryResults)
      statement = mDB->GetStatement(
          NS_LITERAL_CSTRING(
              "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
              "v.visit_date, null, null, null, null, null, ") +
          tagsFragment +
          NS_LITERAL_CSTRING(", h.frecency, h.hidden, h.guid, "
                             "v.id, v.from_visit, v.visit_type "
                             "FROM moz_places h "
                             "JOIN moz_historyvisits v ON h.id = v.place_id "
                             "WHERE v.id = :visit_id "));
      break;

    case nsNavHistoryQueryOptions::RESULTS_AS_URI:
      // URL results - want last visit time
      // Should match kGetInfoIndex_* (see GetQueryResults)
      statement = mDB->GetStatement(
          NS_LITERAL_CSTRING(
              "SELECT h.id, h.url, h.title, h.rev_host, h.visit_count, "
              "h.last_visit_date, null, null, null, null, null, ") +
          tagsFragment +
          NS_LITERAL_CSTRING(", h.frecency, h.hidden, h.guid, "
                             "null, null, null "
                             "FROM moz_places h "
                             "JOIN moz_historyvisits v ON h.id = v.place_id "
                             "WHERE v.id = :visit_id "));
      break;

    default:
      // Query base types like RESULTS_AS_*_QUERY handle additions
      // by registering their own observers when they are expanded.
      return NS_OK;
  }
  NS_ENSURE_STATE(statement);
  mozStorageStatementScoper scoper(statement);

  nsresult rv =
      statement->BindInt64ByName(NS_LITERAL_CSTRING("visit_id"), visitId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore = false;
  rv = statement->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasMore) {
    MOZ_ASSERT_UNREACHABLE("Trying to get a result node for an invalid visit");
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(statement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return RowToResult(row, aOptions, aResult);
}

nsresult nsNavHistory::BookmarkIdToResultNode(
    int64_t aBookmarkId, nsNavHistoryQueryOptions* aOptions,
    nsNavHistoryResultNode** aResult) {
  nsAutoCString tagsFragment;
  GetTagsSqlFragment(GetTagsFolder(), NS_LITERAL_CSTRING("h.id"), true,
                     tagsFragment);
  // Should match kGetInfoIndex_*
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      NS_LITERAL_CSTRING(
          "SELECT b.fk, h.url, b.title, "
          "h.rev_host, h.visit_count, h.last_visit_date, null, b.id, "
          "b.dateAdded, b.lastModified, b.parent, ") +
      tagsFragment +
      NS_LITERAL_CSTRING(", h.frecency, h.hidden, h.guid, "
                         "null, null, null, b.guid, b.position, b.type, b.fk "
                         "FROM moz_bookmarks b "
                         "JOIN moz_places h ON b.fk = h.id "
                         "WHERE b.id = :item_id "));
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv =
      stmt->BindInt64ByName(NS_LITERAL_CSTRING("item_id"), aBookmarkId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore = false;
  rv = stmt->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasMore) {
    MOZ_ASSERT_UNREACHABLE(
        "Trying to get a result node for an invalid "
        "bookmark identifier");
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(stmt, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return RowToResult(row, aOptions, aResult);
}

nsresult nsNavHistory::URIToResultNode(nsIURI* aURI,
                                       nsNavHistoryQueryOptions* aOptions,
                                       nsNavHistoryResultNode** aResult) {
  nsAutoCString tagsFragment;
  GetTagsSqlFragment(GetTagsFolder(), NS_LITERAL_CSTRING("h.id"), true,
                     tagsFragment);
  // Should match kGetInfoIndex_*
  nsCOMPtr<mozIStorageStatement> stmt = mDB->GetStatement(
      NS_LITERAL_CSTRING("SELECT h.id, :page_url, COALESCE(b.title, h.title), "
                         "h.rev_host, h.visit_count, h.last_visit_date, null, "
                         "b.id, b.dateAdded, b.lastModified, b.parent, ") +
      tagsFragment +
      NS_LITERAL_CSTRING(
          ", h.frecency, h.hidden, h.guid, "
          "null, null, null, b.guid, b.position, b.type, b.fk "
          "FROM moz_places h "
          "LEFT JOIN moz_bookmarks b ON b.fk = h.id "
          "WHERE h.url_hash = hash(:page_url) AND h.url = :page_url "));
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore = false;
  rv = stmt->ExecuteStep(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasMore) {
    MOZ_ASSERT_UNREACHABLE("Trying to get a result node for an invalid url");
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<mozIStorageValueArray> row = do_QueryInterface(stmt, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return RowToResult(row, aOptions, aResult);
}

void nsNavHistory::SendPageChangedNotification(nsIURI* aURI,
                                               uint32_t aChangedAttribute,
                                               const nsAString& aNewValue,
                                               const nsACString& aGUID) {
  MOZ_ASSERT(!aGUID.IsEmpty());
  NOTIFY_OBSERVERS(mCanNotify, mObservers, nsINavHistoryObserver,
                   OnPageChanged(aURI, aChangedAttribute, aNewValue, aGUID));
}

void nsNavHistory::GetAgeInDaysString(int32_t aInt, const char* aName,
                                      nsACString& aResult) {
  nsIStringBundle* bundle = GetBundle();
  if (bundle) {
    AutoTArray<nsString, 1> strings;
    strings.AppendElement()->AppendInt(aInt);
    nsAutoString value;
    nsresult rv = bundle->FormatStringFromName(aName, strings, value);
    if (NS_SUCCEEDED(rv)) {
      CopyUTF16toUTF8(value, aResult);
      return;
    }
  }
  aResult.Assign(aName);
}

void nsNavHistory::GetStringFromName(const char* aName, nsACString& aResult) {
  nsIStringBundle* bundle = GetBundle();
  if (bundle) {
    nsAutoString value;
    nsresult rv = bundle->GetStringFromName(aName, value);
    if (NS_SUCCEEDED(rv)) {
      CopyUTF16toUTF8(value, aResult);
      return;
    }
  }
  aResult.Assign(aName);
}

// static
void nsNavHistory::GetMonthName(const PRExplodedTime& aTime,
                                nsACString& aResult) {
  nsAutoString month;
  nsresult rv = DateTimeFormat::FormatPRExplodedTime(
      kDateFormatMonthLong, kTimeFormatNone, &aTime, month);
  if (NS_FAILED(rv)) {
    aResult = nsPrintfCString("[%d]", aTime.tm_month + 1);
    return;
  }
  CopyUTF16toUTF8(month, aResult);
}

// static
void nsNavHistory::GetMonthYear(const PRExplodedTime& aTime,
                                nsACString& aResult) {
  nsAutoString monthYear;
  nsresult rv = DateTimeFormat::FormatPRExplodedTime(
      kDateFormatYearMonthLong, kTimeFormatNone, &aTime, monthYear);
  if (NS_FAILED(rv)) {
    aResult = nsPrintfCString("[%d-%d]", aTime.tm_month + 1, aTime.tm_year);
    return;
  }
  CopyUTF16toUTF8(monthYear, aResult);
}

namespace {

// GetSimpleBookmarksQueryParent
//
//    Determines if this is a simple bookmarks query for a
//    folder with no other constraints. In these common cases, we can more
//    efficiently compute the results.
//
//    A simple bookmarks query will result in a hierarchical tree of
//    bookmark items, folders and separators.
//
//    Returns the folder ID if it is a simple folder query, 0 if not.
static nsCString GetSimpleBookmarksQueryParent(
    const RefPtr<nsNavHistoryQuery>& aQuery,
    const RefPtr<nsNavHistoryQueryOptions>& aOptions) {
  if (aQuery->Parents().Length() != 1) return EmptyCString();

  bool hasIt;
  if (NS_SUCCEEDED(aQuery->GetHasBeginTime(&hasIt)) && hasIt)
    return EmptyCString();
  if (NS_SUCCEEDED(aQuery->GetHasEndTime(&hasIt)) && hasIt)
    return EmptyCString();
  if (!aQuery->Domain().IsVoid()) return EmptyCString();
  if (aQuery->Uri()) return EmptyCString();
  if (!aQuery->SearchTerms().IsEmpty()) return EmptyCString();
  if (aQuery->Tags().Length() > 0) return EmptyCString();
  if (aOptions->MaxResults() > 0) return EmptyCString();

  // Don't care about onlyBookmarked flag, since specifying a bookmark
  // folder is inferring onlyBookmarked.

  return aQuery->Parents()[0];
}

// ParseSearchTermsFromQuery
//
//    Construct an array of search terms from the given query.
//    Within a query, all the terms are ANDed together.
//
//    This just breaks the query up into words. We don't do anything fancy,
//    not even quoting. We do, however, strip quotes, because people might
//    try to input quotes expecting them to do something and get no results
//    back.

inline bool isQueryWhitespace(char16_t ch) { return ch == ' '; }

void ParseSearchTermsFromQuery(const RefPtr<nsNavHistoryQuery>& aQuery,
                               nsTArray<nsString>* aTerms) {
  int32_t lastBegin = -1;
  if (!aQuery->SearchTerms().IsEmpty()) {
    const nsString& searchTerms = aQuery->SearchTerms();
    for (uint32_t j = 0; j < searchTerms.Length(); j++) {
      if (isQueryWhitespace(searchTerms[j]) || searchTerms[j] == '"') {
        if (lastBegin >= 0) {
          // found the end of a word
          aTerms->AppendElement(
              Substring(searchTerms, lastBegin, j - lastBegin));
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
      aTerms->AppendElement(Substring(searchTerms, lastBegin));
  }
}

}  // namespace

nsresult nsNavHistory::UpdateFrecency(int64_t aPlaceId) {
  nsCOMPtr<mozIStorageAsyncStatement> updateFrecencyStmt =
      mDB->GetAsyncStatement(
          "UPDATE moz_places "
          "SET frecency = NOTIFY_FRECENCY("
          "CALCULATE_FRECENCY(:page_id), url, guid, hidden, last_visit_date"
          ") "
          "WHERE id = :page_id");
  NS_ENSURE_STATE(updateFrecencyStmt);
  nsresult rv = updateFrecencyStmt->BindInt64ByName(
      NS_LITERAL_CSTRING("page_id"), aPlaceId);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<mozIStorageAsyncStatement> updateHiddenStmt = mDB->GetAsyncStatement(
      "UPDATE moz_places "
      "SET hidden = 0 "
      "WHERE id = :page_id AND frecency <> 0");
  NS_ENSURE_STATE(updateHiddenStmt);
  rv = updateHiddenStmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"),
                                         aPlaceId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageConnection> conn = mDB->MainConn();
  if (!conn) {
    return NS_ERROR_UNEXPECTED;
  }

  nsTArray<RefPtr<mozIStorageBaseStatement>> stmts = {
      ToRefPtr(std::move(updateFrecencyStmt)),
      ToRefPtr(std::move(updateHiddenStmt)),
  };
  nsCOMPtr<mozIStoragePendingStatement> ps;
  rv = conn->ExecuteAsync(stmts, nullptr, getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  // Trigger frecency updates for all affected origins.
  nsCOMPtr<mozIStorageAsyncStatement> updateOriginFrecenciesStmt =
      mDB->GetAsyncStatement("DELETE FROM moz_updateoriginsupdate_temp");
  NS_ENSURE_STATE(updateOriginFrecenciesStmt);
  rv = updateOriginFrecenciesStmt->ExecuteAsync(nullptr, getter_AddRefs(ps));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsICollation* nsNavHistory::GetCollation() {
  if (mCollation) return mCollation;

  // collation
  nsCOMPtr<nsICollationFactory> cfact =
      do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID);
  NS_ENSURE_TRUE(cfact, nullptr);
  nsresult rv = cfact->CreateCollation(getter_AddRefs(mCollation));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return mCollation;
}

nsIStringBundle* nsNavHistory::GetBundle() {
  if (!mBundle) {
    nsCOMPtr<nsIStringBundleService> bundleService =
        services::GetStringBundleService();
    NS_ENSURE_TRUE(bundleService, nullptr);
    nsresult rv = bundleService->CreateBundle(
        "chrome://places/locale/places.properties", getter_AddRefs(mBundle));
    NS_ENSURE_SUCCESS(rv, nullptr);
  }
  return mBundle;
}
