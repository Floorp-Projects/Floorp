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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
 *   Dietrich Ayala <dietrich@mozilla.com>
 *   Marco Bonardo <mak77@bonardo.net>
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

/**
 * This class handles expiration of history for nsNavHistory. There is a 1-1
 * mapping between nsNavHistory class and a nsNavHistoryExpire class, the
 * code is separated for better understandability.
 */

#include "nsNavHistory.h"
#include "mozStorageHelper.h"
#include "nsNetUtil.h"
#include "nsIAnnotationService.h"
#include "nsPrintfCString.h"
#include "nsPlacesMacros.h"
#include "nsIIdleService.h"

struct nsNavHistoryExpireRecord {
  nsNavHistoryExpireRecord(mozIStorageStatement* statement);

  PRInt64 visitID;
  PRInt64 placeID;
  PRTime visitDate;
  nsCString uri;
  PRInt64 faviconID;
  PRBool hidden;
  PRBool bookmarked;
  PRBool erased; // set to true if/when the history entry is erased
};

// The time in ms to wait before kick-off partial expiration after preferences
// are changed.
#define EXPIRATION_PARTIAL_TIMEOUT 500

// The time in ms to wait between each partial expiration step.
#define EXPIRATION_PARTIAL_SUBSEQUENT_TIMEOUT ((PRUint32)10 * PR_MSEC_PER_SEC)

// Number of pages we'll expire at each partial expiration run.  Partial
// expiration runs when expiration preferences run.
#define EXPIRATION_PAGES_PER_RUN 6

// The time in ms the user should be idle before we run expiration.
// This will be repeated till we find enough entries to expire, otherwise
// we will wait for a longer timeout before checking again.
#define EXPIRATION_IDLE_TIMEOUT ((PRUint32)5 * 60 * PR_MSEC_PER_SEC)
// The time in ms the user should be idle before we run expiration when the
// previous call ran out of expirable pages.
#define EXPIRATION_IDLE_LONG_TIMEOUT EXPIRATION_IDLE_TIMEOUT * 10

// During idle we can expire a larger chunk of pages.
#define EXPIRATION_MAX_PAGES_AT_IDLE 100

// During shutdown we should cleanup any dangling moz_places record, but we
// cannot expire a too large number of entries since that would slowdown
// shutdown.
#define EXPIRATION_MAX_PAGES_AT_SHUTDOWN 100

// Expiration policy amounts in microseconds.
const PRTime EXPIRATION_POLICY_DAYS = ((PRTime)7 * 86400 * PR_USEC_PER_SEC);
const PRTime EXPIRATION_POLICY_WEEKS = ((PRTime)30 * 86400 * PR_USEC_PER_SEC);
const PRTime EXPIRATION_POLICY_MONTHS = ((PRTime)180 * 86400 * PR_USEC_PER_SEC);

// History preferences.
#define PREF_BRANCH_BASE                        "browser."
#define PREF_BROWSER_HISTORY_EXPIRE_DAYS        "history_expire_days"

// Sanitization preferences
#define PREF_SANITIZE_ON_SHUTDOWN   "privacy.sanitize.sanitizeOnShutdown"
#define PREF_SANITIZE_ITEM_HISTORY  "privacy.item.history"

nsNavHistoryExpire::nsNavHistoryExpire() :
    mNextExpirationTime(0)
{
  mHistory = nsNavHistory::GetHistoryService();
  NS_ASSERTION(mHistory, "History service should exist at this point.");
  mDBConn = mHistory->GetStorageConnection();
  NS_ASSERTION(mDBConn, "History service should have a valid database connection");

  // Initialize idle timer.
  InitializeIdleTimer(EXPIRATION_IDLE_TIMEOUT);
}

nsNavHistoryExpire::~nsNavHistoryExpire()
{
  // Cancel any pending timers.
  if (mPartialExpirationTimer) {
    mPartialExpirationTimer->Cancel();
    mPartialExpirationTimer = 0;
  }
  if (mIdleTimer) {
    mIdleTimer->Cancel();
    mIdleTimer = 0;
  }
}

void
nsNavHistoryExpire::InitializeIdleTimer(PRUint32 aTimeInMs)
{
  if (mIdleTimer) {
    mIdleTimer->Cancel();
    mIdleTimer = 0;
  }

  mIdleTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mIdleTimer) {
    (void)mIdleTimer->InitWithFuncCallback(IdleTimerCallback, this, aTimeInMs,
                                           nsITimer::TYPE_ONE_SHOT);
  }
}

void // static
nsNavHistoryExpire::IdleTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsNavHistoryExpire* expire = static_cast<nsNavHistoryExpire*>(aClosure);
  expire->mIdleTimer = 0;
  expire->OnIdle();
}

void
nsNavHistoryExpire::OnIdle()
{
  PRUint32 idleTime = 0;
  nsCOMPtr<nsIIdleService> idleService =
    do_GetService("@mozilla.org/widget/idleservice;1");
  if (idleService)
    (void)idleService->GetIdleTime(&idleTime);

  // If we've been idle for more than EXPIRATION_IDLE_TIMEOUT
  // we can expire a chunk of elements.
  if (idleTime < EXPIRATION_IDLE_TIMEOUT)
    return;

  mozStorageTransaction transaction(mDBConn, PR_TRUE);

  bool keepGoing = ExpireItems(EXPIRATION_MAX_PAGES_AT_IDLE);
  ExpireOrphans(EXPIRATION_MAX_PAGES_AT_IDLE);

  if (!keepGoing) {
    // We have expired enough entries, so there is no more need to be agressive
    // on idle for some time.
    InitializeIdleTimer(EXPIRATION_IDLE_LONG_TIMEOUT);
  }
  else
    InitializeIdleTimer(EXPIRATION_IDLE_TIMEOUT);
}

void
nsNavHistoryExpire::OnDeleteVisits()
{
  (void)ExpireAnnotations();
}

void
nsNavHistoryExpire::OnQuit()
{
  // Cancel any pending timers so we won't try to expire during shutdown.
  if (mPartialExpirationTimer) {
    mPartialExpirationTimer->Cancel();
    mPartialExpirationTimer = 0;
  }
  if (mIdleTimer) {
    mIdleTimer->Cancel();
    mIdleTimer = 0;
  }

  nsCOMPtr<nsIPrefBranch> prefs =
    do_GetService("@mozilla.org/preferences-service;1");
  if (prefs) {
    // Determine whether we can skip partially expiration of dangling entries
    // because we be doing a full expiration on shutdown in ClearHistory().
    PRBool sanitizeOnShutdown = PR_FALSE;
    (void)prefs->GetBoolPref(PREF_SANITIZE_ON_SHUTDOWN, &sanitizeOnShutdown);
    PRBool sanitizeHistory = PR_FALSE;
    (void)prefs->GetBoolPref(PREF_SANITIZE_ITEM_HISTORY, &sanitizeHistory);

    if (sanitizeHistory && sanitizeOnShutdown)
      return;
  }

  // Get rid of all records orphaned due to expiration.
  ExpireOrphans(EXPIRATION_MAX_PAGES_AT_SHUTDOWN);
}

nsresult
nsNavHistoryExpire::ClearHistory()
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // reset frecency for all items that will _not_ be deleted
  // Note, we set frecency to -visit_count since we use that value in our
  // idle query to figure out which places to recalcuate frecency first.
  // We must do this before deleting visits.
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_places_view SET frecency = -MAX(visit_count, 1) "
    "WHERE id IN("
      "SELECT h.id FROM moz_places_temp h "
      "WHERE "
        "EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) "
      "UNION ALL "
      "SELECT h.id FROM moz_places h "
      "WHERE "
        "EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) "
    ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Expire visits, then let the paranoid functions do the cleanup for us.
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_historyvisits_view"));
  NS_ENSURE_SUCCESS(rv, rv);

  // Expire all orphans.
  ExpireOrphans(-1);

  // Some of the remaining places could be place: urls or
  // unvisited livemark items, so setting the frecency to -1
  // will cause them to show up in the url bar autocomplete
  // call FixInvalidFrecenciesForExcludedPlaces to handle that scenario.
  rv = mHistory->FixInvalidFrecenciesForExcludedPlaces();
  if (NS_FAILED(rv))
    NS_WARNING("failed to fix invalid frecencies");

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  ENUMERATE_OBSERVERS(mHistory->canNotify(), mHistory->mCacheObservers,
                      mHistory->mObservers, nsINavHistoryObserver,
                      OnClearHistory());

  return NS_OK;
}

void
nsNavHistoryExpire::OnExpirationChanged()
{
  // Kick off partial expiration.
  // Subsequent steps will be on timer.
  StartPartialExpirationTimer(EXPIRATION_PARTIAL_TIMEOUT);
}

nsresult
nsNavHistoryExpire::DoPartialExpiration()
{
  bool keepGoing = ExpireItems(EXPIRATION_PAGES_PER_RUN);
  if (keepGoing)
    StartPartialExpirationTimer(EXPIRATION_PARTIAL_SUBSEQUENT_TIMEOUT);

  return NS_OK;
}

bool
nsNavHistoryExpire::ExpireItems(PRUint32 aNumToExpire)
{
  // Whether to keep going after this expiration step.
  bool keepGoing = true;

  // This transaction is important for performance. It makes the DB flush
  // everything to disk in one larger operation rather than many small ones.
  // Note that this transaction always commits.
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  PRInt64 expireTime;
  if (!aNumToExpire) {
    // Special case: erase all pages from history.
    expireTime = 0;
  }
  else {
    expireTime = PR_Now() - GetExpirationTimeAgo(mHistory->mExpireDaysMax);
  }

  // Find some visits to expire.
  nsTArray<nsNavHistoryExpireRecord> expiredVisits;
  nsresult rv = FindVisits(expireTime, aNumToExpire, expiredVisits);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "FindVisits Failed");

  // if we didn't find as many things to expire as we could have, then
  // we should note the next time we need to expire.
  if (expiredVisits.Length() < aNumToExpire) {
    keepGoing = false;
    ComputeNextExpirationTime();
  }

  rv = EraseVisits(expiredVisits);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "EraseVisits Failed");

  rv = EraseHistory(expiredVisits);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "EraseHistory Failed");

  // Send observer messages.
  nsCOMPtr<nsIURI> uri;
  for (PRUint32 i = 0; i < expiredVisits.Length(); i ++) {
    rv = NS_NewURI(getter_AddRefs(uri), expiredVisits[i].uri);
    if (NS_FAILED(rv)) {
      NS_WARNING("Trying to expire a corrupt uri?!");
      continue;
    }

    // FIXME bug 325241 provide a way to observe hidden elements
    if (expiredVisits[i].hidden) continue;

    ENUMERATE_OBSERVERS(mHistory->canNotify(), mHistory->mCacheObservers,
                        mHistory->mObservers, nsINavHistoryObserver,
                        OnPageExpired(uri, expiredVisits[i].visitDate,
                                      expiredVisits[i].erased));
  }

  // Don't worry about errors here, it doesn't affect our ability to continue.
  rv = EraseFavicons(expiredVisits);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "EraseFavicons Failed");
  rv = EraseAnnotations(expiredVisits);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "EraseAnnotations Failed");
  rv = ExpireAnnotations();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "ExpireAnnotarions Failed");

  rv = transaction.Commit();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Committing transaction Failed");

  return keepGoing;
}

void
nsNavHistoryExpire::ExpireOrphans(PRUint32 aNumToExpire)
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  nsresult rv = ExpireHistoryParanoid(aNumToExpire);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "ExpireHistoryParanoid Failed");

  rv = ExpireFaviconsParanoid();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "ExpireFaviconsParanoid Failed");

  rv = ExpireAnnotationsParanoid();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "ExpireAnnotationsParanoid Failed");

  rv = ExpireInputHistoryParanoid();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "ExpireInputHistoryParanoid Failed");

  rv = transaction.Commit();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Commit Transaction Failed");
}

/**
 * nsNavHistoryExpireRecord::nsNavHistoryExpireRecord
 *
 * Statement should be the one created in FindVisits. The parameters must
 * agree.
 */
nsNavHistoryExpireRecord::nsNavHistoryExpireRecord(
  mozIStorageStatement* statement)
{
  visitID = statement->AsInt64(0);
  placeID = statement->AsInt64(1);
  visitDate = statement->AsInt64(2);
  statement->GetUTF8String(3, uri);
  faviconID = statement->AsInt64(4);
  hidden = (statement->AsInt32(5) > 0);
  bookmarked = (statement->AsInt32(6) > 0);
  erased = PR_FALSE;
}

nsresult
nsNavHistoryExpire::FindVisits(PRTime aExpireThreshold, PRUint32 aNumToExpire,
                               nsTArray<nsNavHistoryExpireRecord>& aRecords)
{
  // Select a limited number of visits older than a time.
  nsCOMPtr<mozIStorageStatement> selectStatement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT v.id, v.place_id, v.visit_date, IFNULL(h_t.url, h.url), "
             "IFNULL(h_t.favicon_id, h.favicon_id), "
             "IFNULL(h_t.hidden, h.hidden), b.fk "
      "FROM moz_historyvisits_temp v "
      "LEFT JOIN moz_places_temp AS h_t ON h_t.id = v.place_id "
      "LEFT JOIN moz_places AS h ON h.id = v.place_id "
      "LEFT JOIN moz_bookmarks b ON b.fk = v.place_id "
      "WHERE visit_date < ?1 "
      "UNION ALL "
      "SELECT v.id, v.place_id, v.visit_date, IFNULL(h_t.url, h.url), "
             "IFNULL(h_t.favicon_id, h.favicon_id), "
             "IFNULL(h_t.hidden, h.hidden), b.fk "
      "FROM moz_historyvisits v "
      "LEFT JOIN moz_places_temp AS h_t ON h_t.id = v.place_id "
      "LEFT JOIN moz_places AS h ON h.id = v.place_id "
      "LEFT JOIN moz_bookmarks b ON b.fk = v.place_id "
      "WHERE visit_date < ?1 "
      "ORDER BY v.visit_date ASC "
      "LIMIT ?2 "),
    getter_AddRefs(selectStatement));
    NS_ENSURE_SUCCESS(rv, rv);

  // Use browser.history_expire_days or match all visits.
  PRTime expireMaxTime = aExpireThreshold ? aExpireThreshold : LL_MAXINT;
  rv = selectStatement->BindInt64Parameter(0, expireMaxTime);
  NS_ENSURE_SUCCESS(rv, rv);

  // Use LIMIT -1 to not limit.
  PRInt32 numToExpire = aNumToExpire ? aNumToExpire : -1;
  rv = selectStatement->BindInt64Parameter(1, numToExpire);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(selectStatement->ExecuteStep(&hasMore)) && hasMore) {
    nsNavHistoryExpireRecord record(selectStatement);
    aRecords.AppendElement(record);
  }

  // If we have found less than aNumToExpire over-max-age records, and we are
  // over the unique urls cap, select records older than the min-age cap .
  if (aRecords.Length() < aNumToExpire) {
    // check the number of visited unique urls in the db.
    nsCOMPtr<mozIStorageStatement> countStatement;
    rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT "
          "(SELECT count(*) FROM moz_places_temp WHERE visit_count > 0) + "
          "(SELECT count(*) FROM moz_places WHERE visit_count > 0 AND "
            "id NOT IN (SELECT id FROM moz_places_temp))"),
      getter_AddRefs(countStatement));
    NS_ENSURE_SUCCESS(rv, rv);

    hasMore = PR_FALSE;
    // initialize to mExpiresites to avoid expiring if something goes wrong.
    PRInt32 pageCount = mHistory->mExpireSites;
    if (NS_SUCCEEDED(countStatement->ExecuteStep(&hasMore)) && hasMore) {
      rv = countStatement->GetInt32(0, &pageCount);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Don't find any more pages to expire if we have not reached the urls cap.
    if (pageCount <= mHistory->mExpireSites)
        return NS_OK;

    rv = selectStatement->Reset();
    NS_ENSURE_SUCCESS(rv, rv);

    // browser.history_expire_days_min
    PRTime expireMinTime = PR_Now() -
                           GetExpirationTimeAgo(mHistory->mExpireDaysMin);
    rv = selectStatement->BindInt64Parameter(0, expireMinTime);
    NS_ENSURE_SUCCESS(rv, rv);
    
    numToExpire = aNumToExpire - aRecords.Length();
    rv = selectStatement->BindInt64Parameter(1, numToExpire);
    NS_ENSURE_SUCCESS(rv, rv);

    hasMore = PR_FALSE;
    while (NS_SUCCEEDED(selectStatement->ExecuteStep(&hasMore)) && hasMore) {
      nsNavHistoryExpireRecord record(selectStatement);
      aRecords.AppendElement(record);
    }
  }

  return NS_OK;
}

nsresult
nsNavHistoryExpire::EraseVisits(
    const nsTArray<nsNavHistoryExpireRecord>& aRecords)
{
  // build a comma separated string of visit ids to delete
  // also build a comma separated string of place ids to reset frecency
  nsCString deletedVisitIds;
  nsCString placeIds;
  nsTArray<PRInt64> deletedPlaceIdsArray, deletedVisitIdsArray;
  for (PRUint32 i = 0; i < aRecords.Length(); i ++) {
    // Do not add comma separator for the first visit id
    if (deletedVisitIdsArray.IndexOf(aRecords[i].visitID) == -1) {
      if (!deletedVisitIds.IsEmpty())
        deletedVisitIds.AppendLiteral(",");  
      deletedVisitIds.AppendInt(aRecords[i].visitID);
    }

    // Do not add comma separator for the first place id
    if (deletedPlaceIdsArray.IndexOf(aRecords[i].placeID) == -1) {
      if (!placeIds.IsEmpty())
        placeIds.AppendLiteral(",");
      placeIds.AppendInt(aRecords[i].placeID);
    }
  }

  if (deletedVisitIds.IsEmpty())
    return NS_OK;

  // Reset the frecencies for the places that won't have any visits after
  // we delete them and make sure they aren't bookmarked either. This means we
  // keep the old frecencies when possible as an estimate for the new frecency
  // unless we know it has to be invalidated.
  // We must do this before deleting visits
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE moz_places_view "
    "SET frecency = -MAX(visit_count, 1) "
    "WHERE id IN ( "
      "SELECT h.id FROM moz_places_temp h "
      "WHERE "
        "NOT EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) AND "
        "NOT EXISTS ( "
          "SELECT v.id FROM moz_historyvisits_temp v "
          "WHERE v.place_id = h.id "
          "AND v.id NOT IN (") + deletedVisitIds + NS_LITERAL_CSTRING(") "
        ") AND "
        "NOT EXISTS ( "
          "SELECT v.id FROM moz_historyvisits v "
          "WHERE v.place_id = h.id "
          "AND v.id NOT IN (") + deletedVisitIds + NS_LITERAL_CSTRING(") "
        ") AND "
        "h.id IN (") + placeIds + NS_LITERAL_CSTRING(") "
      "UNION ALL "
      "SELECT h.id FROM moz_places h "
      "WHERE "
        "NOT EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) AND "
        "NOT EXISTS ( "
          "SELECT v.id FROM moz_historyvisits_temp v "
          "WHERE v.place_id = h.id "
          "AND v.id NOT IN (") + deletedVisitIds + NS_LITERAL_CSTRING(") "
        ") AND "
        "NOT EXISTS ( "
          "SELECT v.id FROM moz_historyvisits v "
          "WHERE v.place_id = h.id "
          "AND v.id NOT IN (") + deletedVisitIds + NS_LITERAL_CSTRING(") "
        ") AND "
        "h.id IN (") + placeIds + NS_LITERAL_CSTRING(") "
    ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->ExecuteSimpleSQL(
    NS_LITERAL_CSTRING("DELETE FROM moz_historyvisits_view WHERE id IN (") +
    deletedVisitIds +
    NS_LITERAL_CSTRING(")"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistoryExpire::EraseHistory(
    nsTArray<nsNavHistoryExpireRecord>& aRecords)
{
  // build a comma separated string of place ids to delete
  nsCString deletedPlaceIds;
  nsTArray<PRInt64> deletedPlaceIdsArray;
  for (PRUint32 i = 0; i < aRecords.Length(); i ++) {
    // IF bookmarked entries OR "place" URIs do not delete
    if (aRecords[i].bookmarked ||
        StringBeginsWith(aRecords[i].uri, NS_LITERAL_CSTRING("place:")))
      continue;
    // avoid trying to delete the same place id twice
    if (deletedPlaceIdsArray.IndexOf(aRecords[i].placeID) == -1) {
      // Do not add comma separator for the first entry
      if (!deletedPlaceIds.IsEmpty())
        deletedPlaceIds.AppendLiteral(",");
      deletedPlaceIdsArray.AppendElement(aRecords[i].placeID);
      deletedPlaceIds.AppendInt(aRecords[i].placeID);
    }
    aRecords[i].erased = PR_TRUE;
  }

  if (deletedPlaceIds.IsEmpty())
    return NS_OK;

  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_places_view WHERE id IN( "
        "SELECT h.id "
        "FROM moz_places h "
        "WHERE h.id IN(") + deletedPlaceIds + NS_LITERAL_CSTRING(") "
          "AND NOT EXISTS "
            "(SELECT id FROM moz_historyvisits WHERE place_id = h.id LIMIT 1) "
          "AND NOT EXISTS "
            "(SELECT id FROM moz_historyvisits_temp WHERE place_id = h.id LIMIT 1) "
          "AND NOT EXISTS "
            "(SELECT id FROM moz_bookmarks WHERE fk = h.id LIMIT 1) "
          "AND SUBSTR(h.url, 1, 6) <> 'place:' "
        "UNION ALL "
        "SELECT h.id "
        "FROM moz_places_temp h "
        "WHERE h.id IN(") + deletedPlaceIds + NS_LITERAL_CSTRING(") "
          "AND NOT EXISTS "
            "(SELECT id FROM moz_historyvisits WHERE place_id = h.id LIMIT 1) "
          "AND NOT EXISTS "
            "(SELECT id FROM moz_historyvisits_temp WHERE place_id = h.id LIMIT 1) "
          "AND NOT EXISTS "
            "(SELECT id FROM moz_bookmarks WHERE fk = h.id LIMIT 1) "
          "AND SUBSTR(h.url, 1, 6) <> 'place:' "
      ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistoryExpire::EraseFavicons(
    const nsTArray<nsNavHistoryExpireRecord>& aRecords)
{
  // build a comma separated string of favicon ids to delete
  nsCString deletedFaviconIds;
  nsTArray<PRInt64> deletedFaviconIdsArray;  
  for (PRUint32 i = 0; i < aRecords.Length(); i ++) {
    // IF main entry not expired OR no favicon DO NOT DELETE
    if (!aRecords[i].erased || aRecords[i].faviconID == 0)
      continue;
    // avoid trying to delete the same favicon id twice
    if (deletedFaviconIdsArray.IndexOf(aRecords[i].faviconID) == -1) {
      // Do not add comma separator for the first entry
      if (!deletedFaviconIds.IsEmpty())
        deletedFaviconIds.AppendLiteral(",");
      deletedFaviconIdsArray.AppendElement(aRecords[i].faviconID);
      deletedFaviconIds.AppendInt(aRecords[i].faviconID);
    }
  }

  if (deletedFaviconIds.IsEmpty())
    return NS_OK;

  // delete only if favicon id is not referenced
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_favicons WHERE id IN ( "
        "SELECT f.id FROM moz_favicons f "
        "LEFT JOIN moz_places h ON f.id = h.favicon_id "
        "LEFT JOIN moz_places_temp h_t ON f.id = h_t.favicon_id "
        "WHERE f.id IN (") + deletedFaviconIds + NS_LITERAL_CSTRING(") "
        "AND h.favicon_id IS NULL "
        "AND h_t.favicon_id IS NULL "
      ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistoryExpire::EraseAnnotations(
    const nsTArray<nsNavHistoryExpireRecord>& aRecords)
{
  // remove annotations for the set of records passed in
  nsCString placeIds;
  nsTArray<PRInt64> deletedPlaceIdsArray;
  for (PRUint32 i = 0; i < aRecords.Length(); i ++) {
    // avoid trying to delete the same place id twice
    if (deletedPlaceIdsArray.IndexOf(aRecords[i].placeID) == -1) {
      // Do not add comma separator for the first entry
      if (!placeIds.IsEmpty())
        placeIds.AppendLiteral(",");
      deletedPlaceIdsArray.AppendElement(aRecords[i].placeID);
      placeIds.AppendInt(aRecords[i].placeID);
    }
  }
  
  if (placeIds.IsEmpty())
    return NS_OK;
    
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DELETE FROM moz_annos WHERE place_id in (") +
      placeIds + NS_LITERAL_CSTRING(") AND expiration != ") +
      nsPrintfCString("%d", nsIAnnotationService::EXPIRE_NEVER));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
nsNavHistoryExpire::ExpireAnnotations()
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // Note: The COALESCE is used to cover a short period where NULLs were inserted
  // into the lastModified column.
  PRTime now = PR_Now();
  nsCOMPtr<mozIStorageStatement> expirePagesStatement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_annos "
      "WHERE expiration = ?1 AND "
        "(?2 > MAX(COALESCE(lastModified, 0), dateAdded))"),
    getter_AddRefs(expirePagesStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<mozIStorageStatement> expireItemsStatement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_items_annos "
      "WHERE expiration = ?1 AND "
        "(?2 > MAX(COALESCE(lastModified, 0), dateAdded))"),
    getter_AddRefs(expireItemsStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // remove days annos
  rv = expirePagesStatement->BindInt32Parameter(0, nsIAnnotationService::EXPIRE_DAYS);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expirePagesStatement->BindInt64Parameter(1, (now - EXPIRATION_POLICY_DAYS));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expirePagesStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expirePagesStatement->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  // remove days item annos
  rv = expireItemsStatement->BindInt32Parameter(0, nsIAnnotationService::EXPIRE_DAYS);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expireItemsStatement->BindInt64Parameter(1, (now - EXPIRATION_POLICY_DAYS));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expireItemsStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expireItemsStatement->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  // remove weeks annos
  rv = expirePagesStatement->BindInt32Parameter(0, nsIAnnotationService::EXPIRE_WEEKS);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expirePagesStatement->BindInt64Parameter(1, (now - EXPIRATION_POLICY_WEEKS));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expirePagesStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expirePagesStatement->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  // remove weeks item annos
  rv = expireItemsStatement->BindInt32Parameter(0, nsIAnnotationService::EXPIRE_WEEKS);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expireItemsStatement->BindInt64Parameter(1, (now - EXPIRATION_POLICY_WEEKS));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expireItemsStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expireItemsStatement->Reset();
  NS_ENSURE_SUCCESS(rv, rv);

  // remove months annos
  rv = expirePagesStatement->BindInt32Parameter(0, nsIAnnotationService::EXPIRE_MONTHS);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expirePagesStatement->BindInt64Parameter(1, (now - EXPIRATION_POLICY_MONTHS));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expirePagesStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // remove months item annos
  rv = expireItemsStatement->BindInt32Parameter(0, nsIAnnotationService::EXPIRE_MONTHS);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expireItemsStatement->BindInt64Parameter(1, (now - EXPIRATION_POLICY_MONTHS));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = expireItemsStatement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // remove EXPIRE_WITH_HISTORY annos for pages without visits
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_annos WHERE expiration = ") +
        nsPrintfCString("%d", nsIAnnotationService::EXPIRE_WITH_HISTORY) +
        NS_LITERAL_CSTRING(" AND NOT EXISTS "
          "(SELECT id FROM moz_historyvisits_temp "
          "WHERE place_id = moz_annos.place_id LIMIT 1) "
        "AND NOT EXISTS "
          "(SELECT id FROM moz_historyvisits "
          "WHERE place_id = moz_annos.place_id LIMIT 1)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistoryExpire::ExpireHistoryParanoid(PRInt32 aMaxRecords)
{
  nsCAutoString query(
    "DELETE FROM moz_places_view WHERE id IN ("
      "SELECT h.id FROM moz_places h "
      "LEFT JOIN moz_historyvisits v ON h.id = v.place_id "
      "LEFT JOIN moz_historyvisits_temp v_t ON h.id = v_t.place_id "
      "LEFT JOIN moz_bookmarks b ON h.id = b.fk "
      "WHERE v.id IS NULL "
        "AND v_t.id IS NULL "
        "AND b.id IS NULL "
        "AND SUBSTR(h.url, 1, 6) <> 'place:' "
      "UNION ALL "
      "SELECT h.id FROM moz_places_temp h "
      "LEFT JOIN moz_historyvisits v ON h.id = v.place_id "
      "LEFT JOIN moz_historyvisits_temp v_t ON h.id = v_t.place_id "
      "LEFT JOIN moz_bookmarks b ON h.id = b.fk "
      "WHERE v.id IS NULL "
        "AND v_t.id IS NULL "
        "AND b.id IS NULL "
        "AND SUBSTR(h.url, 1, 6) <> 'place:'");
  if (aMaxRecords != -1) {
    query.AppendLiteral(" LIMIT ");
    query.AppendInt(aMaxRecords);
  }
  query.AppendLiteral(")");
  nsresult rv = mDBConn->ExecuteSimpleSQL(query);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistoryExpire::ExpireFaviconsParanoid()
{
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_favicons WHERE id IN ("
        "SELECT f.id FROM moz_favicons f "
        "LEFT JOIN moz_places h ON f.id = h.favicon_id "
        "LEFT JOIN moz_places_temp h_t ON f.id = h_t.favicon_id "
        "WHERE h.favicon_id IS NULL "
          "AND h_t.favicon_id IS NULL "
      ")"));
  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}

nsresult
nsNavHistoryExpire::ExpireAnnotationsParanoid()
{
  // delete session annos
  nsCAutoString session_query = NS_LITERAL_CSTRING(
    "DELETE FROM moz_annos WHERE expiration = ") +
    nsPrintfCString("%d", nsIAnnotationService::EXPIRE_SESSION);
  nsresult rv = mDBConn->ExecuteSimpleSQL(session_query);
  NS_ENSURE_SUCCESS(rv, rv);

  // delete all uri annos w/o a corresponding place id
  // or without any visits *and* not EXPIRE_NEVER.
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_annos WHERE id IN ("
        "SELECT a.id FROM moz_annos a "
        "LEFT JOIN moz_places h ON a.place_id = h.id "
        "LEFT JOIN moz_places_temp h_t ON a.place_id = h_t.id "
        "LEFT JOIN moz_historyvisits v ON a.place_id = v.place_id "
        "LEFT JOIN moz_historyvisits_temp v_t ON a.place_id = v_t.place_id "
        "WHERE (h.id IS NULL AND h_t.id IS NULL) "
          "OR (v.id IS NULL AND v_t.id IS NULL AND a.expiration != ") +
            nsPrintfCString("%d", nsIAnnotationService::EXPIRE_NEVER) +
          NS_LITERAL_CSTRING(")"
      ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  // delete item annos w/o a corresponding item id
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DELETE FROM moz_items_annos WHERE id IN "
      "(SELECT a.id FROM moz_items_annos a "
      "LEFT OUTER JOIN moz_bookmarks b ON a.item_id = b.id "
      "WHERE b.id IS NULL)"));
  NS_ENSURE_SUCCESS(rv, rv);

  // delete all anno names w/o a corresponding anno
  rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_anno_attributes WHERE id IN (" 
        "SELECT n.id FROM moz_anno_attributes n "
        "LEFT JOIN moz_annos a ON n.id = a.anno_attribute_id "
        "LEFT JOIN moz_items_annos t ON n.id = t.anno_attribute_id "
        "WHERE a.anno_attribute_id IS NULL "
          "AND t.anno_attribute_id IS NULL "
      ")"));
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
nsNavHistoryExpire::ExpireInputHistoryParanoid()
{
  // Delete dangling input history that have no associated pages
  nsresult rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "DELETE FROM moz_inputhistory WHERE place_id IN ( "
        "SELECT place_id FROM moz_inputhistory "
        "LEFT JOIN moz_places h ON h.id = place_id "
        "LEFT JOIN moz_places_temp h_t ON h_t.id = place_id "
        "WHERE h.id IS NULL "
          "AND h_t.id IS NULL "
      ")"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsNavHistoryExpire::ComputeNextExpirationTime()
{
  mNextExpirationTime = 0;

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT MIN(visit_date) FROM moz_historyvisits"),
    getter_AddRefs(statement));
  NS_ASSERTION(NS_SUCCEEDED(rv), "Could not create statement");
  if (NS_FAILED(rv)) return;

  PRBool hasMore;
  rv = statement->ExecuteStep(&hasMore);
  if (NS_FAILED(rv) || !hasMore)
    return; // no items, we'll leave mNextExpirationTime = 0 and try to expire
            // again next time

  PRTime minTime = statement->AsInt64(0);
  mNextExpirationTime = minTime + GetExpirationTimeAgo(mHistory->mExpireDaysMax);
}

void
nsNavHistoryExpire::StartPartialExpirationTimer(PRUint32 aMilleseconds)
{
  if (mPartialExpirationTimer) {
    mPartialExpirationTimer->Cancel();
    mPartialExpirationTimer = 0;
  }

  mPartialExpirationTimer = do_CreateInstance("@mozilla.org/timer;1");
  if(mPartialExpirationTimer) {
    (void)mPartialExpirationTimer->InitWithFuncCallback(
      PartialExpirationTimerCallback, this, aMilleseconds,
      nsITimer::TYPE_ONE_SHOT);
  }
}

void // static
nsNavHistoryExpire::PartialExpirationTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsNavHistoryExpire* expire = static_cast<nsNavHistoryExpire*>(aClosure);
  expire->mPartialExpirationTimer = 0;
  expire->DoPartialExpiration();
}

PRTime
nsNavHistoryExpire::GetExpirationTimeAgo(PRInt32 aExpireDays)
{
  // Prevent Int64 overflow for people that type in huge numbers.
  // This number is 2^63 / 24 / 60 / 60 / 1000000 (reversing the math below)
  const PRInt32 maxDays = 106751991;
  if (aExpireDays > maxDays)
    aExpireDays = maxDays;

  // compute how long ago to expire from
  // seconds per day = 86400 = 24*60*60
  return (PRTime)aExpireDays * 86400 * PR_USEC_PER_SEC;
}
