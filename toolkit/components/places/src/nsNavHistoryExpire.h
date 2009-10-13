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

class mozIStorageConnection;
class nsNavHistory;
struct nsNavHistoryExpireRecord;

class nsNavHistoryExpire
{
public:
  nsNavHistoryExpire();
  ~nsNavHistoryExpire();

  /**
   * Called by history when visits are deleted from history.
   * This kicks off an expiration of page annotations.
   */
  void OnDeleteVisits();

  /**
   * Manages shutdown work and final expiration of orphans.
   */
  void OnQuit();

  /**
   * Called when the expiration length in days has changed. We clear any
   * next expiration time, meaning that we'll try to expire stuff next time,
   * and recompute the value if there's still nothing to expire.
   */
  void OnExpirationChanged();

  /**
   * Performance: ExpireItems sends notifications. We may want to disable this
   * for clear history cases. However, my initial tests show that the
   * notifications are not a significant part of clear history time.
   */
  nsresult ClearHistory();

  /**

   * Tries to expire aNumToExpire pages and their associated data.
   *
   * @param aNumToExpire
   *        Number of history pages to expire, pass 0 to expire every page
   *        in history.
   * @return true if we did not expire enough items and we should keep expiring.
   *         false otherwise.
   */
  bool ExpireItems(PRUint32 aNumToExpire);

  /**
   * Tries to expire aNumToExpire items that are orphans.
   *
   * @param aNumToExpire
   *        Limits how many orphan moz_places we worry about.
   *        Everything else (favicons, annotations, and input history) is
   *        completely expired.
   */
  void ExpireOrphans(PRUint32 aNumToExpire);

protected:
  nsNavHistory *mHistory;
  mozIStorageConnection *mDBConn;

  nsCOMPtr<nsITimer> mPartialExpirationTimer;
  void StartPartialExpirationTimer(PRUint32 aMilleseconds);
  static void PartialExpirationTimerCallback(nsITimer *aTimer, void *aClosure);

  // When we have found nothing to expire, we compute the time the next item
  // will expire. This is that time so we won't try to expire anything until
  // then. It is 0 when we don't need to wait to expire stuff.
  PRTime mNextExpirationTime;

  /**
   * This computes mNextExpirationTime. See that var in the header file.
   * It is passed the number of microseconds that things expire in.
   */
  void ComputeNextExpirationTime();

  nsresult DoPartialExpiration();

  /**
   * Creates the idle timer.  We expire visits and orphans on idle.
   */
  void InitializeIdleTimer(PRUint32 aTimeInMs);
  nsCOMPtr<nsITimer> mIdleTimer;
  static void IdleTimerCallback(nsITimer *aTimer, void *aClosure);

  /**
   * We usually expire periodically, but that could not be fast enough, so on idle
   * we want to expire a bigget chunk of items to help partial expiration.
   * This way we try to hit when the user is not going to suffer from UI hangs.
   */
  void OnIdle();

  PRTime GetExpirationTimeAgo(PRInt32 aExpireDays);

  nsresult ExpireAnnotations();

  /**
   * Find visits to expire, meeting the following criteria:
   *
   *   - With a visit date older than browser.history_expire_days ago.
   *   - With a visit date older than browser.history_expire_days_min ago
   *     if we have more than browser.history_expire_sites unique urls.
   *
   * aExpireThreshold is the time at which we will delete visits before.
   * If it is zero, we will match everything.
   *
   * aNumToExpire is the maximum number of visits to find. If it is 0, then
   * we will get all matching visits.
   */
  nsresult FindVisits(PRTime aExpireThreshold, PRUint32 aNumToExpire,
                      nsTArray<nsNavHistoryExpireRecord> &aRecords);

  nsresult EraseVisits(const nsTArray<nsNavHistoryExpireRecord> &aRecords);

  /**
   * This erases records in moz_places when there are no more visits.
   * We need to be careful not to delete: bookmarks, items that still have
   * visits and place: URIs.
   *
   * This will modify the input by setting the erased flag on each of the
   * array elements according to whether the history item was erased or not.
   */
  nsresult EraseHistory(nsTArray<nsNavHistoryExpireRecord> &aRecords);

  nsresult EraseFavicons(const nsTArray<nsNavHistoryExpireRecord> &aRecords);

  /**
   * Periodic expiration of annotations that have time-sensitive
   * expiration policies.
   *
   * @note Always specify the exact policy constant, as they're
   * not guaranteed to be in numerical order.
   */
  nsresult EraseAnnotations(const nsTArray<nsNavHistoryExpireRecord> &aRecords);

  /**
   * Deletes any dangling history entries that aren't associated with any
   * visits, bookmarks or "place:" URIs.
   *
   *    The aMaxRecords parameter is an optional cap on the number of 
   *    records to delete. If its value is -1, all records will be deleted.
   */
  nsresult ExpireHistoryParanoid(PRInt32 aMaxRecords);

  /**
   * Deletes any dangling favicons that aren't associated with any pages.
   */
  nsresult ExpireFaviconsParanoid();

  /**
   * Deletes session annotations, dangling annotations
   * and annotation names that are unused.
   */
  nsresult ExpireAnnotationsParanoid();

  /**
   * Deletes dangling input history
   */
  nsresult ExpireInputHistoryParanoid();
};
