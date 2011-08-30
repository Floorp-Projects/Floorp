/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * What this is aimed to test:
 *
 * Expiration can be manually triggered through a debug topic, but that should
 * only expire orphan entries, unless -1 is passed as limit.
 */

let gNow = getExpirablePRTime();

add_test(function test_expire_orphans()
{
  // Add visits to 2 pages and force a orphan expiration. Visits should survive.
  PlacesUtils.history.addVisit(NetUtil.newURI("http://page1.mozilla.org/"),
                               gNow++, null,
                               PlacesUtils.history.TRANSITION_TYPED, false, 0);
  PlacesUtils.history.addVisit(NetUtil.newURI("http://page2.mozilla.org/"),
                               gNow++, null,
                               PlacesUtils.history.TRANSITION_TYPED, false, 0);
  // Create a orphan place.
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                NetUtil.newURI("http://page3.mozilla.org/"),
                                                PlacesUtils.bookmarks.DEFAULT_INDEX, "");
  PlacesUtils.bookmarks.removeItem(id);

  // Observe expiration.
  Services.obs.addObserver(function (aSubject, aTopic, aData)
  {
    Services.obs.removeObserver(arguments.callee, aTopic);

    // Check that visits survived.
    do_check_eq(visits_in_database("http://page1.mozilla.org/"), 1);
    do_check_eq(visits_in_database("http://page2.mozilla.org/"), 1);
    do_check_false(page_in_database("http://page3.mozilla.org/"));

    // Clean up.
    waitForClearHistory(run_next_test);
  }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  // Expire now.
  force_expiration_step(0);
});

add_test(function test_expire_orphans_optionalarg()
{
  // Add visits to 2 pages and force a orphan expiration. Visits should survive.
  PlacesUtils.history.addVisit(NetUtil.newURI("http://page1.mozilla.org/"),
                               gNow++, null,
                               PlacesUtils.history.TRANSITION_TYPED, false, 0);
  PlacesUtils.history.addVisit(NetUtil.newURI("http://page2.mozilla.org/"),
                               gNow++, null,
                               PlacesUtils.history.TRANSITION_TYPED, false, 0);
  // Create a orphan place.
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                NetUtil.newURI("http://page3.mozilla.org/"),
                                                PlacesUtils.bookmarks.DEFAULT_INDEX, "");
  PlacesUtils.bookmarks.removeItem(id);

  // Observe expiration.
  Services.obs.addObserver(function (aSubject, aTopic, aData)
  {
    Services.obs.removeObserver(arguments.callee, aTopic);

    // Check that visits survived.
    do_check_eq(visits_in_database("http://page1.mozilla.org/"), 1);
    do_check_eq(visits_in_database("http://page2.mozilla.org/"), 1);
    do_check_false(page_in_database("http://page3.mozilla.org/"));

    // Clean up.
    waitForClearHistory(run_next_test);
  }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  // Expire now.
  force_expiration_step();
});

add_test(function test_expire_limited()
{
  // Add visits to 2 pages and force a single expiration.
  // Only 1 page should survive.
  PlacesUtils.history.addVisit(NetUtil.newURI("http://page1.mozilla.org/"),
                               gNow++, null,
                               PlacesUtils.history.TRANSITION_TYPED, false, 0);
  PlacesUtils.history.addVisit(NetUtil.newURI("http://page2.mozilla.org/"),
                               gNow++, null,
                               PlacesUtils.history.TRANSITION_TYPED, false, 0);
  // Observe expiration.
  Services.obs.addObserver(function (aSubject, aTopic, aData)
  {
    Services.obs.removeObserver(arguments.callee, aTopic);

    // Check that visits to the more recent page survived.
    do_check_false(page_in_database("http://page1.mozilla.org/"));
    do_check_eq(visits_in_database("http://page2.mozilla.org/"), 1);

    // Clean up.
    waitForClearHistory(run_next_test);
  }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  // Expire now.
  force_expiration_step(1);
});

add_test(function test_expire_unlimited()
{
  // Add visits to 2 pages and force a single expiration.
  // Only 1 page should survive.
  PlacesUtils.history.addVisit(NetUtil.newURI("http://page1.mozilla.org/"),
                               gNow++, null,
                               PlacesUtils.history.TRANSITION_TYPED, false, 0);
  PlacesUtils.history.addVisit(NetUtil.newURI("http://page2.mozilla.org/"),
                               gNow++, null,
                               PlacesUtils.history.TRANSITION_TYPED, false, 0);
  // Observe expiration.
  Services.obs.addObserver(function (aSubject, aTopic, aData)
  {
    Services.obs.removeObserver(arguments.callee, aTopic);

    // Check that visits to the more recent page survived.
    do_check_false(page_in_database("http://page1.mozilla.org/"));
    do_check_false(page_in_database("http://page2.mozilla.org/"));

    // Clean up.
    waitForClearHistory(run_next_test);
  }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  // Expire now.
  force_expiration_step(-1);
});

function run_test()
{
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h
  // Set maxPages to a low value, so it's easy to go over it.
  setMaxPages(1);

  run_next_test();
}
