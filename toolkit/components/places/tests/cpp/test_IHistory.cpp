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
 * Mozilla Foundation.
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

#include "places_test_harness.h"
#include "nsIBrowserHistory.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "mock_Link.h"
using namespace mozilla::dom;

/**
 * This file tests the IHistory interface.
 */

////////////////////////////////////////////////////////////////////////////////
//// Helper Methods

void
expect_visit(nsLinkState aState)
{
  do_check_true(aState == eLinkState_Visited);
}

void
expect_no_visit(nsLinkState aState)
{
  do_check_true(aState == eLinkState_Unvisited);
}

already_AddRefed<nsIURI>
new_test_uri()
{
  // Create a unique spec.
  static PRInt32 specNumber = 0;
  nsCAutoString spec = NS_LITERAL_CSTRING("http://mozilla.org/");
  spec.AppendInt(specNumber++);

  // Create the URI for the spec.
  nsCOMPtr<nsIURI> testURI;
  nsresult rv = NS_NewURI(getter_AddRefs(testURI), spec);
  do_check_success(rv);
  return testURI.forget();
}

class VisitURIObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  VisitURIObserver(int aExpectedVisits = 1) :
    mVisits(0),
    mExpectedVisits(aExpectedVisits)
  {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
    do_check_true(observerService);
    (void)observerService->AddObserver(this,
                                       "uri-visit-saved",
                                       false);
  }

  void WaitForNotification()
  {
    while (mVisits < mExpectedVisits) {
      (void)NS_ProcessNextEvent();
    }
  }

  NS_IMETHOD Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const PRUnichar* aData)
  {
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
NS_IMPL_ISUPPORTS1(
  VisitURIObserver,
  nsIObserver
)

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

void
test_set_places_enabled()
{
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

// These variables are shared between part 1 and part 2 of the test.  Part 2
// sets the nsCOMPtr's to nsnull, freeing the reference.
namespace test_unvisited_does_not_notify {
  nsCOMPtr<nsIURI> testURI;
  nsRefPtr<Link> testLink;
}
void
test_unvisited_does_not_notify_part1()
{
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
  nsresult rv = history->RegisterVisitedCallback(testURI, testLink);
  do_check_success(rv);

  // Run the next test.
  run_next_test();
}

void
test_visited_notifies()
{
  // First, we add our test URI to history.
  nsCOMPtr<nsIURI> testURI = new_test_uri();
  addURI(testURI);

  // Create our test Link.  The callback function will release the reference we
  // have on the Link.
  nsRefPtr<Link> link = new mock_Link(expect_visit);

  // Now, register our Link to be notified.
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsresult rv = history->RegisterVisitedCallback(testURI, link);
  do_check_success(rv);

  link.forget(); // It will release itself when notified.
  // Note: test will continue upon notification.
}

void
test_unvisited_does_not_notify_part2()
{
  using namespace test_unvisited_does_not_notify;

  // We would have had a failure at this point had the content node been told it
  // was visited.  Therefore, it is safe to unregister our content node.
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsresult rv = history->UnregisterVisitedCallback(testURI, testLink);
  do_check_success(rv);

  // Clear the stored variables now.
  testURI = nsnull;
  testLink = nsnull;

  // Run the next test.
  run_next_test();
}

void
test_same_uri_notifies_both()
{
  // First, we add our test URI to history.
  nsCOMPtr<nsIURI> testURI = new_test_uri();
  addURI(testURI);

  // Create our two test Links.  The callback function will release the
  // reference we have on the Links.  Only the second Link should run the next
  // test!
  nsRefPtr<Link> link1 = new mock_Link(expect_visit, false);
  nsRefPtr<Link> link2 = new mock_Link(expect_visit);

  // Now, register our Link to be notified.
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsresult rv = history->RegisterVisitedCallback(testURI, link1);
  do_check_success(rv);
  rv = history->RegisterVisitedCallback(testURI, link2);
  do_check_success(rv);

  link1.forget(); // It will release itself when notified.
  link2.forget(); // It will release itself when notified.
  // Note: test will continue upon notification.
}

void
test_unregistered_visited_does_not_notify()
{
  // This test must have a test that has a successful notification after it.
  // The Link would have been notified by now if we were buggy and notified
  // unregistered Links (due to request serialization).

  nsCOMPtr<nsIURI> testURI = new_test_uri();
  nsRefPtr<Link> link = new mock_Link(expect_no_visit);

  // Now, register our Link to be notified.
  nsCOMPtr<IHistory> history(do_get_IHistory());
  nsresult rv = history->RegisterVisitedCallback(testURI, link);
  do_check_success(rv);

  // Unregister the Link.
  rv = history->UnregisterVisitedCallback(testURI, link);
  do_check_success(rv);

  // And finally add a visit for the URI.
  addURI(testURI);

  // If history tries to notify us, we'll either crash because the Link will
  // have been deleted (we are the only thing holding a reference to it), or our
  // expect_no_visit call back will produce a failure.  Either way, the test
  // will be reported as a failure.

  // Run the next test.
  run_next_test();
}

void
test_new_visit_notifies_waiting_Link()
{
  // Create our test Link.  The callback function will release the reference we
  // have on the link.
  nsRefPtr<Link> link = new mock_Link(expect_visit);

  // Now, register our content node to be notified.
  nsCOMPtr<nsIURI> testURI = new_test_uri();
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsresult rv = history->RegisterVisitedCallback(testURI, link);
  do_check_success(rv);

  // Add ourselves to history.
  addURI(testURI);

  link.forget(); // It will release itself when notified.
  // Note: test will continue upon notification.
}

void
test_RegisterVisitedCallback_returns_before_notifying()
{
  // Add a URI so that it's already in history.
  nsCOMPtr<nsIURI> testURI = new_test_uri();
  addURI(testURI);

  // Create our test Link.
  nsRefPtr<Link> link = new mock_Link(expect_no_visit);

  // Now, register our content node to be notified.  It should not be notified.
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsresult rv = history->RegisterVisitedCallback(testURI, link);
  do_check_success(rv);

  // Remove ourselves as an observer.  We would have failed if we had been
  // notified.
  rv = history->UnregisterVisitedCallback(testURI, link);
  do_check_success(rv);

  run_next_test();
}

namespace test_observer_topic_dispatched_helpers {
  #define URI_VISITED "visited"
  #define URI_NOT_VISITED "not visited"
  #define URI_VISITED_RESOLUTION_TOPIC "visited-status-resolution"
  class statusObserver : public nsIObserver
  {
  public:
    NS_DECL_ISUPPORTS

    statusObserver(nsIURI* aURI,
                   const bool aExpectVisit,
                   bool& _notified)
    : mURI(aURI)
    , mExpectVisit(aExpectVisit)
    , mNotified(_notified)
    {
      nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
      do_check_true(observerService);
      (void)observerService->AddObserver(this,
                                         URI_VISITED_RESOLUTION_TOPIC,
                                         false);
    }

    NS_IMETHOD Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const PRUnichar* aData)
    {
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
      bool visited = !!NS_LITERAL_STRING(URI_VISITED).Equals(aData);
      bool notVisited = !!NS_LITERAL_STRING(URI_NOT_VISITED).Equals(aData);
      do_check_true(visited || notVisited);

      // Check to make sure we got the state we expected.
      do_check_eq(visited, mExpectVisit);

      // Indicate that we've been notified.
      mNotified = true;

      // Remove ourselves as an observer.
      nsCOMPtr<nsIObserverService> observerService =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
      (void)observerService->RemoveObserver(this,
                                            URI_VISITED_RESOLUTION_TOPIC);
      return NS_OK;
    }
  private:
    nsCOMPtr<nsIURI> mURI;
    const bool mExpectVisit;
    bool& mNotified;
  };
  NS_IMPL_ISUPPORTS1(
    statusObserver,
    nsIObserver
  )
}
void
test_observer_topic_dispatched()
{
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
  nsRefPtr<Link> visitedLink = new mock_Link(expect_visit, false);
  nsRefPtr<Link> visitedLinkCopy = visitedLink;
  visitedLinkCopy.forget(); // It will release itself when notified.
  nsRefPtr<Link> notVisitedLink = new mock_Link(expect_no_visit);

  // Add the right observers for the URIs to check results.
  bool visitedNotified = false;
  nsCOMPtr<nsIObserver> visitedObs =
    new statusObserver(visitedURI, true, visitedNotified);
  bool notVisitedNotified = false;
  nsCOMPtr<nsIObserver> unvisitedObs =
    new statusObserver(notVisitedURI, false, notVisitedNotified);

  // Register our Links to be notified.
  nsCOMPtr<IHistory> history = do_get_IHistory();
  rv = history->RegisterVisitedCallback(visitedURI, visitedLink);
  do_check_success(rv);
  rv = history->RegisterVisitedCallback(notVisitedURI, notVisitedLink);
  do_check_success(rv);

  // Spin the event loop as long as we have not been properly notified.
  while (!visitedNotified || !notVisitedNotified) {
    (void)NS_ProcessNextEvent();
  }

  // Unregister our observer that would not have been released.
  rv = history->UnregisterVisitedCallback(notVisitedURI, notVisitedLink);
  do_check_success(rv);

  run_next_test();
}

void
test_visituri_inserts()
{
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();

  history->VisitURI(visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);

  nsRefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  PlaceRecord place;
  do_get_place(visitedURI, place);

  do_check_true(place.id > 0);
  do_check_false(place.hidden);
  do_check_false(place.typed);
  do_check_eq(place.visitCount, 1);

  run_next_test();
}

void
test_visituri_updates()
{
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();
  nsRefPtr<VisitURIObserver> finisher;

  history->VisitURI(visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);
  finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  history->VisitURI(visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);
  finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  PlaceRecord place;
  do_get_place(visitedURI, place);

  do_check_eq(place.visitCount, 2);

  run_next_test();
}

void
test_visituri_preserves_shown_and_typed()
{
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();

  history->VisitURI(visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);
  // this simulates the uri visit happening in a frame.  Normally frame
  // transitions would be hidden unless it was previously loaded top-level
  history->VisitURI(visitedURI, lastURI, 0);

  nsRefPtr<VisitURIObserver> finisher = new VisitURIObserver(2);
  finisher->WaitForNotification();

  PlaceRecord place;
  do_get_place(visitedURI, place);
  do_check_false(place.hidden);

  run_next_test();
}

void
test_visituri_creates_visit()
{
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();

  history->VisitURI(visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);
  nsRefPtr<VisitURIObserver> finisher = new VisitURIObserver();
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

void
test_visituri_transition_typed()
{
  nsCOMPtr<nsINavHistoryService> navHistory = do_get_NavHistory();
  nsCOMPtr<nsIBrowserHistory> browserHistory = do_QueryInterface(navHistory);
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();

  browserHistory->MarkPageAsTyped(visitedURI);
  history->VisitURI(visitedURI, lastURI, mozilla::IHistory::TOP_LEVEL);
  nsRefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  PlaceRecord place;
  VisitRecord visit;
  do_get_place(visitedURI, place);
  do_get_lastVisit(place.id, visit);

  do_check_true(visit.transitionType == nsINavHistoryService::TRANSITION_TYPED);

  run_next_test();
}

void
test_visituri_transition_embed()
{
  nsCOMPtr<nsINavHistoryService> navHistory = do_get_NavHistory();
  nsCOMPtr<nsIBrowserHistory> browserHistory = do_QueryInterface(navHistory);
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsCOMPtr<nsIURI> lastURI = new_test_uri();
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();

  history->VisitURI(visitedURI, lastURI, 0);
  nsRefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  PlaceRecord place;
  VisitRecord visit;
  do_get_place(visitedURI, place);
  do_get_lastVisit(place.id, visit);

  do_check_eq(place.id, 0);
  do_check_eq(visit.id, 0);

  run_next_test();
}

void
test_new_visit_adds_place_guid()
{
  // First, add a visit and wait.  This will also add a place.
  nsCOMPtr<nsIURI> visitedURI = new_test_uri();
  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsresult rv = history->VisitURI(visitedURI, NULL,
                                  mozilla::IHistory::TOP_LEVEL);
  do_check_success(rv);
  nsRefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  // Check that we have a guid for our visit.
  PlaceRecord place;
  do_get_place(visitedURI, place);
  do_check_eq(place.visitCount, 1);
  do_check_eq(place.guid.Length(), 12);

  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// IPC-only Tests

void
test_two_null_links_same_uri()
{
  // Tests that we do not crash when we have had two NULL Links passed to
  // RegisterVisitedCallback and then the visit occurs (bug 607469).  This only
  // happens in IPC builds.
  nsCOMPtr<nsIURI> testURI = new_test_uri();

  nsCOMPtr<IHistory> history = do_get_IHistory();
  nsresult rv = history->RegisterVisitedCallback(testURI, NULL);
  do_check_success(rv);
  rv = history->RegisterVisitedCallback(testURI, NULL);
  do_check_success(rv);

  rv = history->VisitURI(testURI, NULL, mozilla::IHistory::TOP_LEVEL);
  do_check_success(rv);

  nsRefPtr<VisitURIObserver> finisher = new VisitURIObserver();
  finisher->WaitForNotification();

  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Harness

/**
 * Note: for tests marked "Order Important!", please see the test for details.
 */
Test gTests[] = {
  TEST(test_set_places_enabled), // Must come first!
  TEST(test_unvisited_does_not_notify_part1), // Order Important!
  TEST(test_visited_notifies),
  TEST(test_unvisited_does_not_notify_part2), // Order Important!
  TEST(test_same_uri_notifies_both),
  TEST(test_unregistered_visited_does_not_notify), // Order Important!
  TEST(test_new_visit_notifies_waiting_Link),
  TEST(test_RegisterVisitedCallback_returns_before_notifying),
  TEST(test_observer_topic_dispatched),
  TEST(test_visituri_inserts),
  TEST(test_visituri_updates),
  TEST(test_visituri_preserves_shown_and_typed),
  TEST(test_visituri_creates_visit),
  TEST(test_visituri_transition_typed),
  TEST(test_visituri_transition_embed),
  TEST(test_new_visit_adds_place_guid),

  // The rest of these tests are tests that are only run in IPC builds.
  TEST(test_two_null_links_same_uri),
};

const char* file = __FILE__;
#define TEST_NAME "IHistory"
#define TEST_FILE file
#include "places_test_harness_tail.h"
