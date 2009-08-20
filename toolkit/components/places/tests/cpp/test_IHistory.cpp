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

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

// These variables are shared between part 1 and part 2 of the test.  Part 2
// sets the nsCOMPtr's to nsnull, freeing the reference.
namespace test_unvisited_does_not_notify {
  nsCOMPtr<nsIURI> testURI;
  nsCOMPtr<Link> link;
}
void
test_unvisted_does_not_notify_part1()
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
  link = new mock_Link(expect_no_visit);

  // Now, register our Link to be notified.
  nsCOMPtr<IHistory> history(do_get_IHistory());
  nsresult rv = history->RegisterVisitedCallback(testURI, link);
  do_check_success(rv);

  // Run the next test.
  run_next_test();
}

void
test_visited_notifies()
{
  // First, we add our test URI to history.
  nsCOMPtr<nsIURI> testURI(new_test_uri());
  addURI(testURI);

  // Create our test Link.  The callback function will release the reference we
  // have on the Link.
  Link* link = new mock_Link(expect_visit);
  NS_ADDREF(link);

  // Now, register our Link to be notified.
  nsCOMPtr<IHistory> history(do_get_IHistory());
  nsresult rv = history->RegisterVisitedCallback(testURI, link);
  do_check_success(rv);
  // Note: test will continue upon notification.
}

void
test_unvisted_does_not_notify_part2()
{
  using namespace test_unvisited_does_not_notify;

  // We would have had a failure at this point had the content node been told it
  // was visited.  Therefore, it is safe to unregister our content node.
  nsCOMPtr<IHistory> history(do_get_IHistory());
  nsresult rv = history->UnregisterVisitedCallback(testURI, link);
  do_check_success(rv);

  // Clear the stored variables now.
  testURI = nsnull;
  link = nsnull;

  // Run the next test.
  run_next_test();
}

void
test_same_uri_notifies_both()
{
  // First, we add our test URI to history.
  nsCOMPtr<nsIURI> testURI(new_test_uri());
  addURI(testURI);

  // Create our two test Links.  The callback function will release the
  // reference we have on the Links.  Only the second Link should run the next
  // test!
  Link* link1 = new mock_Link(expect_visit, false);
  NS_ADDREF(link1);
  Link* link2 = new mock_Link(expect_visit);
  NS_ADDREF(link2);

  // Now, register our Link to be notified.
  nsCOMPtr<IHistory> history(do_get_IHistory());
  nsresult rv = history->RegisterVisitedCallback(testURI, link1);
  do_check_success(rv);
  rv = history->RegisterVisitedCallback(testURI, link2);
  do_check_success(rv);

  // Note: test will continue upon notification.
}

void
test_unregistered_visited_does_not_notify()
{
  // This test must have a test that has a successful notification after it.
  // The Link would have been notified by now if we were buggy and notified
  // unregistered Links (due to request serialization).

  nsCOMPtr<nsIURI> testURI(new_test_uri());
  nsCOMPtr<Link> link(new mock_Link(expect_no_visit));

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
  Link* link = new mock_Link(expect_visit);
  NS_ADDREF(link);

  // Now, register our content node to be notified.
  nsCOMPtr<nsIURI> testURI(new_test_uri());
  nsCOMPtr<IHistory> history(do_get_IHistory());
  nsresult rv = history->RegisterVisitedCallback(testURI, link);
  do_check_success(rv);

  // Add ourselves to history.
  addURI(testURI);

  // Note: test will continue upon notification.
}

void
test_RegisterVisitedCallback_returns_before_notifying()
{
  // Add a URI so that it's already in history.
  nsCOMPtr<nsIURI> testURI(new_test_uri());
  addURI(testURI);

  // Create our test Link.
  nsCOMPtr<Link> link(new mock_Link(expect_no_visit));

  // Now, register our content node to be notified.  It should not be notified.
  nsCOMPtr<IHistory> history(do_get_IHistory());
  nsresult rv = history->RegisterVisitedCallback(testURI, link);
  do_check_success(rv);

  // Remove ourselves as an observer.  We would have failed if we had been
  // notified.
  rv = history->UnregisterVisitedCallback(testURI, link);
  do_check_success(rv);

  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Harness

/**
 * Note: for tests marked "Order Important!", please see the test for details.
 */
Test gTests[] = {
  TEST(test_unvisted_does_not_notify_part1), // Order Important!
  TEST(test_visited_notifies),
  TEST(test_unvisted_does_not_notify_part2), // Order Important!
  TEST(test_same_uri_notifies_both),
  TEST(test_unregistered_visited_does_not_notify), // Order Important!
  TEST(test_new_visit_notifies_waiting_Link),
  TEST(test_RegisterVisitedCallback_returns_before_notifying),
};

const char* file = __FILE__;
#define TEST_NAME "IHistory"
#define TEST_FILE file
#include "places_test_harness_tail.h"
