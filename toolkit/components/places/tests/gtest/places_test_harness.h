/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsMemory.h"
#include "nsThreadUtils.h"
#include "nsDocShellCID.h"

#include "nsToolkitCompsCID.h"
#include "nsServiceManagerUtils.h"
#include "nsINavHistoryService.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "mozilla/IHistory.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStorageStatementCallback.h"
#include "mozIStoragePendingStatement.h"
#include "nsIObserver.h"
#include "prinrval.h"
#include "prtime.h"
#include "mozilla/Attributes.h"

#define WAITFORTOPIC_TIMEOUT_SECONDS 5

#define do_check_true(aCondition) EXPECT_TRUE(aCondition)

#define do_check_false(aCondition) EXPECT_FALSE(aCondition)

#define do_check_success(aResult) do_check_true(NS_SUCCEEDED(aResult))

#define do_check_eq(aExpected, aActual) do_check_true(aExpected == aActual)

struct Test {
  void (*func)(void);
  const char* const name;
};
#define PTEST(aName) \
  { aName, #aName }

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
class WaitForTopicSpinner final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS

  explicit WaitForTopicSpinner(const char* const aTopic)
      : mTopicReceived(false), mStartTime(PR_IntervalNow()) {
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    do_check_true(observerService);
    (void)observerService->AddObserver(this, aTopic, false);
  }

  void Spin() {
    bool timedOut = false;
    mozilla::SpinEventLoopUntil([&]() -> bool {
      if (mTopicReceived) {
        return true;
      }

      if ((PR_IntervalNow() - mStartTime) >
          (WAITFORTOPIC_TIMEOUT_SECONDS * PR_USEC_PER_SEC)) {
        timedOut = true;
        return true;
      }

      return false;
    });

    if (timedOut) {
      // Timed out waiting for the topic.
      do_check_true(false);
    }
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    mTopicReceived = true;
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    do_check_true(observerService);
    (void)observerService->RemoveObserver(this, aTopic);
    return NS_OK;
  }

 private:
  ~WaitForTopicSpinner() = default;

  bool mTopicReceived;
  PRIntervalTime mStartTime;
};
NS_IMPL_ISUPPORTS(WaitForTopicSpinner, nsIObserver)

/**
 * Spins current thread until an async statement is executed.
 */
class PlacesAsyncStatementSpinner final : public mozIStorageStatementCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  PlacesAsyncStatementSpinner();
  void SpinUntilCompleted();
  uint16_t completionReason;

 protected:
  ~PlacesAsyncStatementSpinner() = default;

  volatile bool mCompleted;
};

NS_IMPL_ISUPPORTS(PlacesAsyncStatementSpinner, mozIStorageStatementCallback)

PlacesAsyncStatementSpinner::PlacesAsyncStatementSpinner()
    : completionReason(0), mCompleted(false) {}

NS_IMETHODIMP
PlacesAsyncStatementSpinner::HandleResult(mozIStorageResultSet* aResultSet) {
  return NS_OK;
}

NS_IMETHODIMP
PlacesAsyncStatementSpinner::HandleError(mozIStorageError* aError) {
  return NS_OK;
}

NS_IMETHODIMP
PlacesAsyncStatementSpinner::HandleCompletion(uint16_t aReason) {
  completionReason = aReason;
  mCompleted = true;
  return NS_OK;
}

void PlacesAsyncStatementSpinner::SpinUntilCompleted() {
  nsCOMPtr<nsIThread> thread(::do_GetCurrentThread());
  nsresult rv = NS_OK;
  bool processed = true;
  while (!mCompleted && NS_SUCCEEDED(rv)) {
    rv = thread->ProcessNextEvent(true, &processed);
  }
}

struct PlaceRecord {
  int64_t id;
  int32_t hidden;
  int32_t typed;
  int32_t visitCount;
  nsCString guid;
};

struct VisitRecord {
  int64_t id;
  int64_t lastVisitId;
  int32_t transitionType;
};

already_AddRefed<mozilla::IHistory> do_get_IHistory() {
  nsCOMPtr<mozilla::IHistory> history = do_GetService(NS_IHISTORY_CONTRACTID);
  do_check_true(history);
  return history.forget();
}

already_AddRefed<nsINavHistoryService> do_get_NavHistory() {
  nsCOMPtr<nsINavHistoryService> serv =
      do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);
  do_check_true(serv);
  return serv.forget();
}

already_AddRefed<mozIStorageConnection> do_get_db() {
  nsCOMPtr<nsINavHistoryService> history = do_get_NavHistory();
  do_check_true(history);

  nsCOMPtr<mozIStorageConnection> dbConn;
  nsresult rv = history->GetDBConnection(getter_AddRefs(dbConn));
  do_check_success(rv);
  return dbConn.forget();
}

/**
 * Get the place record from the database.
 *
 * @param aURI The unique URI of the place we are looking up
 * @param result Out parameter where the result is stored
 */
void do_get_place(nsIURI* aURI, PlaceRecord& result) {
  nsCOMPtr<mozIStorageConnection> dbConn = do_get_db();
  nsCOMPtr<mozIStorageStatement> stmt;

  nsCString spec;
  nsresult rv = aURI->GetSpec(spec);
  do_check_success(rv);

  rv = dbConn->CreateStatement(
      nsLiteralCString(
          "SELECT id, hidden, typed, visit_count, guid FROM moz_places "
          "WHERE url_hash = hash(?1) AND url = ?1"),
      getter_AddRefs(stmt));
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
void do_get_lastVisit(int64_t placeId, VisitRecord& result) {
  nsCOMPtr<mozIStorageConnection> dbConn = do_get_db();
  nsCOMPtr<mozIStorageStatement> stmt;

  nsresult rv = dbConn->CreateStatement(
      nsLiteralCString(
          "SELECT id, from_visit, visit_type FROM moz_historyvisits "
          "WHERE place_id=?1 "
          "LIMIT 1"),
      getter_AddRefs(stmt));
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

void do_wait_async_updates() {
  nsCOMPtr<mozIStorageConnection> db = do_get_db();
  nsCOMPtr<mozIStorageAsyncStatement> stmt;

  db->CreateAsyncStatement("BEGIN EXCLUSIVE"_ns, getter_AddRefs(stmt));
  nsCOMPtr<mozIStoragePendingStatement> pending;
  (void)stmt->ExecuteAsync(nullptr, getter_AddRefs(pending));

  db->CreateAsyncStatement("COMMIT"_ns, getter_AddRefs(stmt));
  RefPtr<PlacesAsyncStatementSpinner> spinner =
      new PlacesAsyncStatementSpinner();
  (void)stmt->ExecuteAsync(spinner, getter_AddRefs(pending));

  spinner->SpinUntilCompleted();
}

/**
 * Adds a URI to the database.
 *
 * @param aURI
 *        The URI to add to the database.
 */
void addURI(nsIURI* aURI) {
  nsCOMPtr<mozilla::IHistory> history = do_GetService(NS_IHISTORY_CONTRACTID);
  do_check_true(history);
  nsresult rv =
      history->VisitURI(nullptr, aURI, nullptr, mozilla::IHistory::TOP_LEVEL);
  do_check_success(rv);

  do_wait_async_updates();
}

static const char TOPIC_PROFILE_CHANGE_QM[] = "profile-before-change-qm";
static const char TOPIC_PLACES_CONNECTION_CLOSED[] = "places-connection-closed";

class WaitForConnectionClosed final : public nsIObserver {
  RefPtr<WaitForTopicSpinner> mSpinner;

  ~WaitForConnectionClosed() = default;

 public:
  NS_DECL_ISUPPORTS

  WaitForConnectionClosed() {
    nsCOMPtr<nsIObserverService> os =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    MOZ_ASSERT(os);
    if (os) {
      // The places-connection-closed notification happens because of things
      // that occur during profile-before-change, so we use the stage after that
      // to wait for it.
      MOZ_ALWAYS_SUCCEEDS(
          os->AddObserver(this, TOPIC_PROFILE_CHANGE_QM, false));
    }
    mSpinner = new WaitForTopicSpinner(TOPIC_PLACES_CONNECTION_CLOSED);
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
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
