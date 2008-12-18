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
  nsNavHistoryExpire(nsNavHistory* aHistory);
  ~nsNavHistoryExpire();

  void OnAddURI(PRTime aNow);
  void OnDeleteURI();
  void OnQuit();
  nsresult ClearHistory();
  void OnExpirationChanged();
  nsresult ExpireItems(PRUint32 aNumToExpire, PRBool* aKeepGoing);

protected:

  nsNavHistory* mHistory;

  nsCOMPtr<nsITimer> mTimer;
  PRBool mTimerSet;

  // Set when we try to expire something and find there is nothing to expire.
  // This short-curcuits the shutdown logic by indicating that there probably
  // isn't anything important we need to expire.
  PRBool mAnyEmptyRuns;

  // When we have found nothing to expire, we compute the time the next item
  // will expire. This is that time so we won't try to expire anything until
  // then. It is 0 when we don't need to wait to expire stuff.
  PRTime mNextExpirationTime;
  void ComputeNextExpirationTime(mozIStorageConnection* aConnection);

  // global statistics
  PRUint32 mAddCount;
  PRUint32 mExpiredItems;

  nsresult DoPartialExpiration();

  nsresult ExpireAnnotations(mozIStorageConnection* aConnection);

  // parts of ExpireItems
  nsresult FindVisits(PRTime aExpireThreshold, PRUint32 aNumToExpire,
                      mozIStorageConnection* aConnection,
                      nsTArray<nsNavHistoryExpireRecord>& aRecords);
  nsresult EraseVisits(mozIStorageConnection* aConnection,
                       const nsTArray<nsNavHistoryExpireRecord>& aRecords);
  nsresult EraseHistory(mozIStorageConnection* aConnection,
                        nsTArray<nsNavHistoryExpireRecord>& aRecords);
  nsresult EraseFavicons(mozIStorageConnection* aConnection,
                         const nsTArray<nsNavHistoryExpireRecord>& aRecords);
  nsresult EraseAnnotations(mozIStorageConnection* aConnection,
                            const nsTArray<nsNavHistoryExpireRecord>& aRecords);

  // paranoid checks
  nsresult ExpireHistoryParanoid(mozIStorageConnection* aConnection, PRInt32 aMaxRecords);
  nsresult ExpireFaviconsParanoid(mozIStorageConnection* aConnection);
  nsresult ExpireAnnotationsParanoid(mozIStorageConnection* aConnection);
  nsresult ExpireInputHistoryParanoid(mozIStorageConnection* aConnection);

  PRBool ExpireForDegenerateRuns();

  nsresult StartTimer(PRUint32 aMilleseconds);
  static void TimerCallback(nsITimer* aTimer, void* aClosure);

  PRTime GetExpirationTimeAgo(PRInt32 aExpireDays);
};
