/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "nsMemory.h"
#include "nsThreadUtils.h"
#include "nsDocShellCID.h"

#include "nsToolkitCompsCID.h"
#include "nsINavHistoryService.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "mozilla/IHistory.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStorageStatementCallback.h"
#include "mozIStoragePendingStatement.h"
#include "nsPIPlacesDatabase.h"
#include "nsIObserver.h"
#include "prinrval.h"
#include "prtime.h"
#include "mozilla/Attributes.h"

#define WAITFORTOPIC_TIMEOUT_SECONDS 5


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
class WaitForTopicSpinner final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  explicit WaitForTopicSpinner(const char* const aTopic)
  : mTopicReceived(false)
  , mStartTime(PR_IntervalNow())
  {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    do_check_true(observerService);
    (void)observerService->AddObserver(this, aTopic, false);
  }

  void Spin() {
    while (!mTopicReceived) {
      if ((PR_IntervalNow() - mStartTime) > (WAITFORTOPIC_TIMEOUT_SECONDS * PR_USEC_PER_SEC)) {
        // Timed out waiting for the topic.
        do_check_true(false);
        break;
      }
      (void)NS_ProcessNextEvent();
    }
  }

  NS_IMETHOD Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const char16_t* aData) override
  {
    mTopicReceived = true;
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    do_check_true(observerService);
    (void)observerService->RemoveObserver(this, aTopic);
    return NS_OK;
  }

private:
  ~WaitForTopicSpinner() {}

  bool mTopicReceived;
  PRIntervalTime mStartTime;
};
NS_IMPL_ISUPPORTS(
  WaitForTopicSpinner,
  nsIObserver
)

/**
 * Spins current thread until an async statement is executed.
 */
class AsyncStatementSpinner final : public mozIStorageStatementCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  AsyncStatementSpinner();
  void SpinUntilCompleted();
  uint16_t completionReason;

protected:
  ~AsyncStatementSpinner() {}

  volatile bool mCompleted;
};

NS_IMPL_ISUPPORTS(AsyncStatementSpinner,
                  mozIStorageStatementCallback)

AsyncStatementSpinner::AsyncStatementSpinner()
: completionReason(0)
, mCompleted(false)
{
}

NS_IMETHODIMP
AsyncStatementSpinner::HandleResult(mozIStorageResultSet *aResultSet)
{
  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementSpinner::HandleError(mozIStorageError *aError)
{
  return NS_OK;
}

NS_IMETHODIMP
AsyncStatementSpinner::HandleCompletion(uint16_t aReason)
{
  completionReason = aReason;
  mCompleted = true;
  return NS_OK;
}

void AsyncStatementSpinner::SpinUntilCompleted()
{
  nsCOMPtr<nsIThread> thread(::do_GetCurrentThread());
  nsresult rv = NS_OK;
  bool processed = true;
  while (!mCompleted && NS_SUCCEEDED(rv)) {
    rv = thread->ProcessNextEvent(true, &processed);
  }
}

struct PlaceRecord
{
  int64_t id;
  int32_t hidden;
  int32_t typed;
  int32_t visitCount;
  nsCString guid;
};

struct VisitRecord
{
  int64_t id;
  int64_t lastVisitId;
  int32_t transitionType;
};

already_AddRefed<mozilla::IHistory>
do_get_IHistory()
{
  nsCOMPtr<mozilla::IHistory> history = do_GetService(NS_IHISTORY_CONTRACTID);
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

  nsCOMPtr<mozIStorageConnection> dbConn;
  nsresult rv = database->GetDBConnection(getter_AddRefs(dbConn));
  do_check_success(rv);
  return dbConn.forget();
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
    "WHERE url_hash = hash(?1) AND url = ?1"
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
do_get_lastVisit(int64_t placeId, VisitRecord& result)
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

void
do_wait_async_updates() {
  nsCOMPtr<mozIStorageConnection> db = do_get_db();
  nsCOMPtr<mozIStorageAsyncStatement> stmt;

  db->CreateAsyncStatement(NS_LITERAL_CSTRING("BEGIN EXCLUSIVE"),
                           getter_AddRefs(stmt));
  nsCOMPtr<mozIStoragePendingStatement> pending;
  (void)stmt->ExecuteAsync(nullptr, getter_AddRefs(pending));

  db->CreateAsyncStatement(NS_LITERAL_CSTRING("COMMIT"),
                           getter_AddRefs(stmt));
  RefPtr<AsyncStatementSpinner> spinner = new AsyncStatementSpinner();
  (void)stmt->ExecuteAsync(spinner, getter_AddRefs(pending));

  spinner->SpinUntilCompleted();
}

/**
 * Adds a URI to the database.
 *
 * @param aURI
 *        The URI to add to the database.
 */
void
addURI(nsIURI* aURI)
{
  nsCOMPtr<mozilla::IHistory> history = do_GetService(NS_IHISTORY_CONTRACTID);
  do_check_true(history);
  nsresult rv = history->VisitURI(aURI, nullptr, mozilla::IHistory::TOP_LEVEL);
  do_check_success(rv);

  do_wait_async_updates();
}

static const char TOPIC_PROFILE_CHANGE[] = "profile-before-change";
static const char TOPIC_PLACES_CONNECTION_CLOSED[] = "places-connection-closed";

class WaitForConnectionClosed final : public nsIObserver
{
  RefPtr<WaitForTopicSpinner> mSpinner;

  ~WaitForConnectionClosed() {}

public:
  NS_DECL_ISUPPORTS

  WaitForConnectionClosed()
  {
    nsCOMPtr<nsIObserverService> os =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    MOZ_ASSERT(os);
    if (os) {
      MOZ_ALWAYS_SUCCEEDS(os->AddObserver(this, TOPIC_PROFILE_CHANGE, false));
    }
    mSpinner = new WaitForTopicSpinner(TOPIC_PLACES_CONNECTION_CLOSED);
  }

  NS_IMETHOD Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const char16_t* aData) override
  {
    nsCOMPtr<nsIObserverService> os =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    MOZ_ASSERT(os);
    if (os) {
      MOZ_ALWAYS_SUCCEEDS(os->RemoveObserver(this, aTopic));
    }

    mSpinner->Spin();

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(WaitForConnectionClosed, nsIObserver)
