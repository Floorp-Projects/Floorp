/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "places_test_harness.h"
#include "nsIPrefBranch.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "mozilla/StaticPrefs_layout.h"
#include "nsNetUtil.h"

#include "mock_Link.h"
using namespace mozilla;
using namespace mozilla::dom;

/**
 * This file tests the IHistory interface.
 */

////////////////////////////////////////////////////////////////////////////////
//// Helper Methods

void expect_visit(nsLinkState aState) {
  do_check_true(aState == eLinkState_Visited);
}

void expect_no_visit(nsLinkState aState) {
  do_check_true(aState == eLinkState_Unvisited);
}

already_AddRefed<nsIURI> new_test_uri() {
  // Create a unique spec.
  static int32_t specNumber = 0;
  nsCString spec = "http://mozilla.org/"_ns;
  spec.AppendInt(specNumber++);

  // Create the URI for the spec.
  nsCOMPtr<nsIURI> testURI;
  nsresult rv = NS_NewURI(getter_AddRefs(testURI), spec);
  do_check_success(rv);
  return testURI.forget();
}

class VisitURIObserver final : public nsIObserver {
  ~VisitURIObserver() = default;

 public:
  NS_DECL_ISUPPORTS

  explicit VisitURIObserver(int aExpectedVisits = 1)
      : mVisits(0), mExpectedVisits(aExpectedVisits) {
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    do_check_true(observerService);
    (void)observerService->AddObserver(this, "uri-visit-saved", false);
  }

  void WaitForNotification() {
    SpinEventLoopUntil([&]() { return mVisits >= mExpectedVisits; });
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    mVisits++;

    if (mVisits == mExpectedVisits) {
      nsCOMPtr<nsIObserverService> observerService =
          do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
      (void)observerService->RemoveObserver(this, "uri-visit-saved");
    }

    return NS_OK;
  }

 private:
  int mVisits;
  int mExpectedVisits;
};
NS_IMPL_ISUPPORTS(VisitURIObserver, nsIObserver)

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

void test_set_places_enabled() {
  // Ensure places is enabled for everyone.
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch =
      do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  do_check_success(rv);

  rv = prefBranch->SetBoolPref("places.history.enabled", true);
  do_check_success(rv);

  // Run the next test.
  run_next_test();
}

void test_wait_checkpoint() {
  // This "fake" test is here to wait for the initial WAL checkpoint we force
  // after creating the database schema, since that may happen at any time,
  // and cause concurrent readers to access an older checkpoint.
  nsCOMPtr<mozIStorageConnection> db = do_get_db();
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  db->CreateAsyncStatement("SELECT 1"_ns, getter_AddRefs(stmt));
  RefPtr<PlacesAsyncStatementSpinner> spinner =
      new PlacesAsyncStatementSpinner();
  nsCOMPtr<mozIStoragePendingStatement> pending;
  (void)stmt->ExecuteAsync(spinner, getter_AddRefs(pending));
  spinner->SpinUntilCompleted();

  // Run the next test.
  run_next_test();
}

// These variables are shared between part 1 and part 2 of the test.  Part 2
// sets the nsCOMPtr's to nullptr, freeing the reference.
namespace test_unvisited_does_not_notify {
nsCOMPtr<nsIURI> testURI;
RefPtr<mock_Link> testLink;
}  // namespace test_unvisited_does_not_notify
void test_unvisited_does_not_notify_part1() {
  using namespace test_unvisited_does_not_notify;

  // This test is done in two parts.  The first part registers for a URI that
  // should not be visited.  We then run another test that will also do a
  // lookup and will be notified.  Since requests are answered in the order they
  // are requested (at least as long as the same URI isn't asked for later), we
  // will know that the Link was not notified.

  // First, we need a test URI.
  testURI = new_test_uri();

  // Create our test Link.
  testLink = new mock_Link(expect_no_visit);

  // Now, register our Link to be notified.
  nsCOMPtr<IHistory> history = do_get_IHistory();
  history->RegisterVisitedCallback(testURI, testLink);

  // Run the next test.
  run_next_test();
}

void test_visited_notifies() {
  // First, we add our test URI to history.
  nsCOMPtr<nsIURI> testURI = new_test_uri();
  addURI(testURI);

  // Create our test Link.  The callback function will release the reference we
  // have on the Link.
  RefPtr<Link> link = new mock_Link(expect_visit);

  // Now, register our Link to be notified.
  nsCOMPtr<IHistory> history = do_get_IHistory();
  history->RegisterVisitedCallback(testURI, link);

  // Note: test will continue upon notification.
}

void test_unvisited_does_not_notify_part2() {
  using namespace test_unvisited_does_not_notify;

  if (StaticPrefs::layout_css_notify_of_unvisited()) {
    SpinEventLoopUntil([&]() { return testLink->GotNotified(); });
  }

  // We would have had a failure at this point had the content node been told it
  // was visited. Therefore, now we change it so that it expects a visited
  // notification, and unregisters itself after addURI.
  testLink->AwaitNewNotification(expect_visit);
  addURI(testURI);

  // Clear the stored variables now.
  testURI = nullptr;
  testLink = nullptr;
}

void test_same_uri_notifies_both() {
  // First, we add our test URI to history.
  nsCOMPtr<nsIURI> testURI = new_test_uri();
  addURI(testURI);

  // Create our two test Links.  The callback function will release the
  // reference we have on the Links.  Only the second Link should run the next
  // test!
  RefPtr<Link> link1 = new mock_Link(expect_visit, false);
  RefPtr<Link> link2 = new mock_Link(expect_visit);

  // Now, register our Link to be notified.
  nsCOMPtr<IHistory> history = do_get_IHistory();
  history->RegisterVisitedCallback(testURI, link1);
  history->RegisterVisitedCallback(testURI, link2);

  // Note: test will continue upon notification.
}

void test_unregistered_visited_does_not_notify() {
  // This test must have a test that has a successful notification after it.
  // The Link would have been notified by now if we were buggy and notified
  // unregistered Links (due to request serialization).

  nsCOMPtr<nsIURI> testURI = new_test_uri();
  RefPtr<Link> link = new mock_Link(expect_no_visit, false);
  nsCOMPtr<IHistory> history(do_get_IHistory());
  history->RegisterVisitedCallback(testURI, link);

  // Unregister the Link.
  history->UnregisterVisitedCallback(testURI, link);

  // And finally add a visit for the URI.
  addURI(testURI);

  // If history tries to notify us, we'll either crash because the Link will
  // have been deleted (we are the only thing holding a reference to it), or our
  // expect_no_visit call back will produce a failure.  Either way, the test
  // will be reported as a failure.

  // Run the next test.
  run_next_test();
}

void test_new_visit_notifies_waiting_Link() {
  // Create our test Link.  The callback function will release the reference we
  // have on the link.
  //
  // Note that this will query the database and we'll get an _unvisited_
  // notification, then (after we addURI) a _visited_ one.
  RefPtr<mock_Link> link = new mock_Link(expect_no_visit);

  // Now, register our content node to be notified.
  nsCOMPtr<nsIURI> testURI = new_test_uri();
  nsCOMPtr<IHistory> history = do_get_IHistory();
  history->RegisterVisitedCallback(testURI, link);

  if (StaticPrefs::layout_css_notify_of_unvisited()) {
    SpinEventLoopUntil([&]() { return link->GotNotified(); });
  }

  link->AwaitNewNotification(expect_visit);

  // Add ourselves to history.
  addURI(testURI);

  // Note: test will continue upon notification.
}

void test_RegisterVisitedCallback_returns_before_notifying() {
  // Add a URI so that it's already in history.
  nsCOMPtr<nsIURI> testURI = new_test_uri();
  addURI(testURI);

  // Create our test Link.
  RefPtr<Link> link = new mock_Link(expect_no_visit, false);

  // Now, register our content node to be notified.  It should not be notified.
  nsCOMPtr<IHistory> history = do_get_IHistory();
  history->RegisterVisitedCallback(testURI, link);

  // Remove ourselves as an observer.  We would have failed if we had been
  // notified.
  history->UnregisterVisitedCallback(testURI, link);

  run_next_test();
}

namespace test_observer_topic_dispatched_helpers {
#define URI_VISITED u"visited"
#define URI_NOT_VISITED u"not visited"
#define URI_VISITED_RESOLUTION_TOPIC "visited-status-resolution"
class statusObserver final : public nsIObserver {
  ~statusObserver() = default;

 public:
  NS_DECL_ISUPPORTS

  statusObserver(nsIURI* aURI, const bool aExpectVisit, bool& _notified)
      : mURI(aURI), mExpectVisit(aExpectVisit), mNotified(_notified) {
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    do_check_true(observerService);
    (void)observerService->AddObserver(this, URI_VISITED_RESOLUTION_TOPIC,
                                       false);
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    // Make sure we got notified of the right topic.
    do_check_false(strcmp(aTopic, URI_VISITED_RESOLUTION_TOPIC));

    // If this isn't for our URI, do not do anything.
    nsCOMPtr<nsIURI> notifiedURI = do_QueryInterface(aSubject);
    do_check_true(notifiedURI);

    bool isOurURI;
    nsresult rv = notifiedURI->Equals(mURI, &isOurURI);
    do_check_success(rv);
    if (!isOurURI) {
      return NS_OK;
    }

    // Check that we have either the visited or not visited string.
    bool visited = !!nsLiteralString(URI_VISITED).Equals(aData);
    bool notVisited = !!nsLiteralString(URI_NOT_VISITED).Equals(aData);
    do_check_true(visited || notVisited);

    // Check to make sure we got the state we expected.
    do_check_eq(visited, mExpectVisit);

    // Indicate that we've been notified.
    mNotified = true;

    // Remove ourselves as an observer.
    nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    (void)observerService->RemoveObserver(this, URI_VISITED_RESOLUTION_TOPIC);
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIURI> mURI;
  const bool mExpectVisit;
  bool& mNotified;
};
NS_IMPL_ISUPPORTS(statusObserver, nsIObserver)
}  // namespace test_observer_topic_dispatched_helpers
void test_observer_topic_dispatched() {
  using namespace test_observer_topic_dispatched_helpers;

  // Create two URIs, making sure only one is in history.
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();
  nsCOMPtr<nsIURI> notVisitedURI = new_test_uri();
  bool urisEqual;
  nsresult rv = visitedURI->Equals(notVisitedURI, &urisEqual);
  do_check_success(rv);
  do_check_false(urisEqual);
  addURI(visitedURI);

  // Need two Link objects as well - one for each URI.
  RefPtr<Link> visitedLink = new mock_Link(expect_visit, false);
  RefPtr<Link> notVisitedLink = new mock_Link(expect_no_visit, false);
  RefPtr<Link> visitedLinkCopy = visitedLink;

  // Add the right observers for the URIs to check results.
  bool visitedNotified = false;
  nsCOMPtr<nsIObserver> visitedObs =
      new statusObserver(visitedURI, true, visitedNotified);
  bool notVisitedNotified = false;
  nsCOMPtr<nsIObserver> unvisitedObs =
      new statusObserver(notVisitedURI, false, notVisitedNotified);

  // Register our Links to be notified.
  nsCOMPtr<IHistory> history = do_get_IHistory();
  history->RegisterVisitedCallback(visitedURI, visitedLink);
  history->RegisterVisitedCallback(notVisitedURI, notVisitedLink);

  // Spin the event loop as long as we have not been properly notified.
  SpinEventLoopUntil([&]() { return visitedNotified && notVisitedNotified; });

  // Unregister our observer that would not have been released.
  history->UnregisterVisitedCallback(notVisitedURI, notVisitedLink);

  run_next_test();
}

void test_visituri_inserts() {
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();

  history->VisitURI(nullptr, visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);

  RefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  PlaceRecord place;
  do_get_place(visitedURI, place);

  do_check_true(place.id > 0);
  do_check_false(place.hidden);
  do_check_false(place.typed);
  do_check_eq(place.visitCount, 1);

  run_next_test();
}

void test_visituri_updates() {
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();
  RefPtr<VisitURIObserver> finisher;

  history->VisitURI(nullptr, visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);
  finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  history->VisitURI(nullptr, visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);
  finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  PlaceRecord place;
  do_get_place(visitedURI, place);

  do_check_eq(place.visitCount, 2);

  run_next_test();
}

void test_visituri_preserves_shown_and_typed() {
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();

  history->VisitURI(nullptr, visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);
  // this simulates the uri visit happening in a frame.  Normally frame
  // transitions would be hidden unless it was previously loaded top-level
  history->VisitURI(nullptr, visitedURI, lastURI, 0);

  RefPtr<VisitURIObserver> finisher = new VisitURIObserver(2);
  finisher->WaitForNotification();

  PlaceRecord place;
  do_get_place(visitedURI, place);
  do_check_false(place.hidden);

  run_next_test();
}

void test_visituri_creates_visit() {
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();

  history->VisitURI(nullptr, visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);
  RefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  PlaceRecord place;
  VisitRecord visit;
  do_get_place(visitedURI, place);
  do_get_lastVisit(place.id, visit);

  do_check_true(visit.id > 0);
  do_check_eq(visit.lastVisitId, 0);
  do_check_eq(visit.transitionType, nsINavHistoryService::TRANSITION_LINK);

  run_next_test();
}

void test_visituri_transition_typed() {
  nsCOMPtr<nsINavHistoryService> navHistory = do_get_NavHistory();
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();

  navHistory->MarkPageAsTyped(visitedURI);
  history->VisitURI(nullptr, visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);
  RefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  PlaceRecord place;
  VisitRecord visit;
  do_get_place(visitedURI, place);
  do_get_lastVisit(place.id, visit);

  do_check_true(visit.transitionType == nsINavHistoryService::TRANSITION_TYPED);

  run_next_test();
}

void test_visituri_transition_embed() {
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();

  history->VisitURI(nullptr, visitedURI, lastURI, 0);
  RefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  PlaceRecord place;
  VisitRecord visit;
  do_get_place(visitedURI, place);
  do_get_lastVisit(place.id, visit);

  do_check_eq(place.id, 0);
  do_check_eq(visit.id, 0);

  run_next_test();
}

void test_new_visit_adds_place_guid() {
  // First, add a visit and wait.  This will also add a place.
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsresult rv = history->VisitURI(nullptr, visitedURI, nullptr,
                                  mozilla::IHistory::TOP_LEVEL);
  do_check_success(rv);
  RefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  // Check that we have a guid for our visit.
  PlaceRecord place;
  do_get_place(visitedURI, place);
  do_check_eq(place.visitCount, 1);
  do_check_eq(place.guid.Length(), 12u);

  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// IPC-only Tests

void test_two_null_links_same_uri() {
  // Tests that we do not crash when we have had two nullptr Links passed to
  // RegisterVisitedCallback and then the visit occurs (bug 607469).  This only
  // happens in IPC builds.
  nsCOMPtr<nsIURI> testURI = new_test_uri();

  nsCOMPtr<IHistory> history = do_get_IHistory();
  history->RegisterVisitedCallback(testURI, nullptr);
  history->RegisterVisitedCallback(testURI, nullptr);

  nsresult rv = history->VisitURI(nullptr, testURI, nullptr,
                                  mozilla::IHistory::TOP_LEVEL);
  do_check_success(rv);

  RefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Harness

/**
 * Note: for tests marked "Order Important!", please see the test for details.
 */
Test gTests[] = {
    PTEST(test_set_places_enabled),               // Must come first!
    PTEST(test_wait_checkpoint),                  // Must come second!
    PTEST(test_unvisited_does_not_notify_part1),  // Order Important!
    PTEST(test_visited_notifies),
    PTEST(test_unvisited_does_not_notify_part2),  // Order Important!
    PTEST(test_same_uri_notifies_both),
    PTEST(test_unregistered_visited_does_not_notify),  // Order Important!
    PTEST(test_new_visit_notifies_waiting_Link),
    PTEST(test_RegisterVisitedCallback_returns_before_notifying),
    PTEST(test_observer_topic_dispatched),
    PTEST(test_visituri_inserts),
    PTEST(test_visituri_updates),
    PTEST(test_visituri_preserves_shown_and_typed),
    PTEST(test_visituri_creates_visit),
    PTEST(test_visituri_transition_typed),
    PTEST(test_visituri_transition_embed),
    PTEST(test_new_visit_adds_place_guid),

    // The rest of these tests are tests that are only run in IPC builds.
    PTEST(test_two_null_links_same_uri),
};

#define TEST_NAME "IHistory"
#include "places_test_harness_tail.h"
