/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is places test code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#include "TestHarness.h"
#include "nsMemory.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsDocShellCID.h"

#include "nsToolkitCompsCID.h"
#include "nsINavHistoryService.h"
#include "nsIObserverService.h"
#include "mozilla/IHistory.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "nsPIPlacesDatabase.h"
#include "nsIObserver.h"
#include "prinrval.h"

#define TOPIC_FRECENCY_UPDATED "places-frecency-updated"
#define WAITFORTOPIC_TIMEOUT_SECONDS 5

using namespace mozilla;

static size_t gTotalTests = 0;
static size_t gPassedTests = 0;

#define do_check_true(aCondition) \
  PR_BEGIN_MACRO \
    gTotalTests++; \
    if (aCondition) { \
      gPassedTests++; \
    } else { \
      fail("%s | Expected true, got false at line %d", __FILE__, __LINE__); \
    } \
  PR_END_MACRO

#define do_check_false(aCondition) \
  PR_BEGIN_MACRO \
    gTotalTests++; \
    if (!aCondition) { \
      gPassedTests++; \
    } else { \
      fail("%s | Expected false, got true at line %d", __FILE__, __LINE__); \
    } \
  PR_END_MACRO

#define do_check_success(aResult) \
  do_check_true(NS_SUCCEEDED(aResult))

#ifdef LINUX
// XXX Linux opt builds on tinderbox are orange due to linking with stdlib.
// This is sad and annoying, but it's a workaround that works.
#define do_check_eq(aExpected, aActual) \
  do_check_true(aExpected == aActual)
#else
#include <sstream>

#define do_check_eq(aActual, aExpected) \
  PR_BEGIN_MACRO \
    gTotalTests++; \
    if (aExpected == aActual) { \
      gPassedTests++; \
    } else { \
      std::ostringstream temp; \
      temp << __FILE__ << " | Expected '" << aExpected << "', got '"; \
      temp << aActual <<"' at line " << __LINE__; \
      fail(temp.str().c_str()); \
    } \
  PR_END_MACRO
#endif

struct Test
{
  void (*func)(void);
  const char* const name;
};
#define TEST(aName) \
  {aName, #aName}

/**
 * Runs the next text.
 */
void run_next_test();

/**
 * To be used around asynchronous work.
 */
void do_test_pending();
void do_test_finished();

/**
 * Spins current thread until a topic is received.
 */
class WaitForTopicSpinner : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  WaitForTopicSpinner(const char* const aTopic)
  : mTopic(aTopic)
  , mTopicReceived(false)
  , mStartTime(PR_IntervalNow())
  {
  }

  void Spin() {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    if (observerService) {
      (void)observerService->AddObserver(this, mTopic, PR_FALSE);

      while (!mTopicReceived) {
        if (PR_IntervalNow() - mStartTime > WAITFORTOPIC_TIMEOUT_SECONDS * PR_USEC_PER_SEC) {
          // Timed out waiting for the topic.
          do_check_true(false);
          break;
        }
        (void)NS_ProcessNextEvent();
      }

      (void)observerService->RemoveObserver(this, mTopic);
    }
  }

  NS_IMETHOD Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const PRUnichar* aData)
  {
    do_check_false(strcmp(aTopic, mTopic));
    mTopicReceived = true;
    return NS_OK;
  }

private:
  const char* const mTopic;
  bool mTopicReceived;
  PRIntervalTime mStartTime;
};
NS_IMPL_ISUPPORTS1(
  WaitForTopicSpinner,
  nsIObserver
)

/**
 * Adds a URI to the database.
 *
 * @param aURI
 *        The URI to add to the database.
 */
void
addURI(nsIURI* aURI)
{
  nsCOMPtr<nsINavHistoryService> hist =
    do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);

  PRInt64 id;
  nsresult rv = hist->AddVisit(aURI, PR_Now(), nsnull,
                               nsINavHistoryService::TRANSITION_LINK, PR_FALSE,
                               0, &id);
  do_check_success(rv);

  // Wait for frecency update.
  nsRefPtr<WaitForTopicSpinner> spinner =
    new WaitForTopicSpinner(TOPIC_FRECENCY_UPDATED);
  spinner->Spin();
}

struct PlaceRecord
{
  PRInt64 id;
  PRInt32 hidden;
  PRInt32 typed;
  PRInt32 visitCount;
  nsCString guid;
};

struct VisitRecord
{
  PRInt64 id;
  PRInt64 lastVisitId;
  PRInt32 transitionType;
};

already_AddRefed<IHistory>
do_get_IHistory()
{
  nsCOMPtr<IHistory> history = do_GetService(NS_IHISTORY_CONTRACTID);
  do_check_true(history);
  return history.forget();
}

already_AddRefed<nsINavHistoryService>
do_get_NavHistory()
{
  nsCOMPtr<nsINavHistoryService> serv =
    do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);
  do_check_true(serv);
  return serv.forget();
}

already_AddRefed<mozIStorageConnection>
do_get_db()
{
  nsCOMPtr<nsINavHistoryService> history = do_get_NavHistory();
  nsCOMPtr<nsPIPlacesDatabase> database = do_QueryInterface(history);
  do_check_true(database);

  mozIStorageConnection* dbConn;
  nsresult rv = database->GetDBConnection(&dbConn);
  do_check_success(rv);
  return dbConn;
}

/**
 * Get the place record from the database.
 *
 * @param aURI The unique URI of the place we are looking up
 * @param result Out parameter where the result is stored
 */
void
do_get_place(nsIURI* aURI, PlaceRecord& result)
{
  nsCOMPtr<mozIStorageConnection> dbConn = do_get_db();
  nsCOMPtr<mozIStorageStatement> stmt;

  nsCString spec;
  nsresult rv = aURI->GetSpec(spec);
  do_check_success(rv);

  rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, hidden, typed, visit_count, guid FROM moz_places "
    "WHERE url=?1 "
  ), getter_AddRefs(stmt));
  do_check_success(rv);

  rv = stmt->BindUTF8StringByIndex(0, spec);
  do_check_success(rv);

  bool hasResults;
  rv = stmt->ExecuteStep(&hasResults);
  do_check_success(rv);
  if (!hasResults) {
    result.id = 0;
    return;
  }

  rv = stmt->GetInt64(0, &result.id);
  do_check_success(rv);
  rv = stmt->GetInt32(1, &result.hidden);
  do_check_success(rv);
  rv = stmt->GetInt32(2, &result.typed);
  do_check_success(rv);
  rv = stmt->GetInt32(3, &result.visitCount);
  do_check_success(rv);
  rv = stmt->GetUTF8String(4, result.guid);
  do_check_success(rv);
}

/**
 * Gets the most recent visit to a place.
 *
 * @param placeID ID from the moz_places table
 * @param result Out parameter where visit is stored
 */
void
do_get_lastVisit(PRInt64 placeId, VisitRecord& result)
{
  nsCOMPtr<mozIStorageConnection> dbConn = do_get_db();
  nsCOMPtr<mozIStorageStatement> stmt;

  nsresult rv = dbConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, from_visit, visit_type FROM moz_historyvisits "
    "WHERE place_id=?1 "
    "LIMIT 1"
  ), getter_AddRefs(stmt));
  do_check_success(rv);

  rv = stmt->BindInt64ByIndex(0, placeId);
  do_check_success(rv);

  bool hasResults;
  rv = stmt->ExecuteStep(&hasResults);
  do_check_success(rv);

  if (!hasResults) {
    result.id = 0;
    return;
  }

  rv = stmt->GetInt64(0, &result.id);
  do_check_success(rv);
  rv = stmt->GetInt64(1, &result.lastVisitId);
  do_check_success(rv);
  rv = stmt->GetInt32(2, &result.transitionType);
  do_check_success(rv);
}

static const char TOPIC_PROFILE_TEARDOWN[] = "profile-change-teardown";
static const char TOPIC_PROFILE_CHANGE[] = "profile-before-change";

class ShutdownObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  ShutdownObserver()
  {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    do_check_true(observerService);
    observerService->AddObserver(this,
                                 NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                                 PR_FALSE);
  }

  NS_IMETHOD Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const PRUnichar* aData)
  {
    if (strcmp(aTopic, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID) == 0) {
      nsCOMPtr<nsIObserverService> os =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
      (void)os->NotifyObservers(nsnull, TOPIC_PROFILE_TEARDOWN, nsnull);
      (void)os->NotifyObservers(nsnull, TOPIC_PROFILE_CHANGE, nsnull);
    }
      return NS_OK;
  }
};
NS_IMPL_ISUPPORTS1(
  ShutdownObserver,
  nsIObserver
)
