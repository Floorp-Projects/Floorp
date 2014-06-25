/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = NetUtil.newURI("http://mozilla.com/");
const TEST_SUBDOMAIN_URI = NetUtil.newURI("http://foobar.mozilla.com/");

add_test(function test_addPage()
{
  promiseAddVisits(TEST_URI).then(function () {
    do_check_eq(1, PlacesUtils.history.hasHistoryEntries);
    run_next_test();
  });
});

add_test(function test_removePage()
{
  PlacesUtils.bhistory.removePage(TEST_URI);
  do_check_eq(0, PlacesUtils.history.hasHistoryEntries);
  run_next_test();
});

add_test(function test_removePages()
{
  let pages = [];
  for (let i = 0; i < 8; i++) {
    pages.push(NetUtil.newURI(TEST_URI.spec + i));
  }

  promiseAddVisits(pages.map(function (uri) ({ uri: uri }))).then(function () {
    // Bookmarked item should not be removed from moz_places.
    const ANNO_INDEX = 1;
    const ANNO_NAME = "testAnno";
    const ANNO_VALUE = "foo";
    const BOOKMARK_INDEX = 2;
    PlacesUtils.annotations.setPageAnnotation(pages[ANNO_INDEX],
                                              ANNO_NAME, ANNO_VALUE, 0,
                                              Ci.nsIAnnotationService.EXPIRE_NEVER);
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         pages[BOOKMARK_INDEX],
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test bookmark");
    PlacesUtils.annotations.setPageAnnotation(pages[BOOKMARK_INDEX],
                                              ANNO_NAME, ANNO_VALUE, 0,
                                              Ci.nsIAnnotationService.EXPIRE_NEVER);
  
    PlacesUtils.bhistory.removePages(pages, pages.length);
    do_check_eq(0, PlacesUtils.history.hasHistoryEntries);
  
    // Check that the bookmark and its annotation still exist.
    do_check_true(PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0) > 0);
    do_check_eq(PlacesUtils.annotations.getPageAnnotation(pages[BOOKMARK_INDEX], ANNO_NAME),
                ANNO_VALUE);
  
    // Check the annotation on the non-bookmarked page does not exist anymore.
    try {
      PlacesUtils.annotations.getPageAnnotation(pages[ANNO_INDEX], ANNO_NAME);
      do_throw("did not expire expire_never anno on a not bookmarked item");
    } catch(ex) {}
  
    // Cleanup.
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
    promiseClearHistory().then(run_next_test);
  });
});

add_test(function test_removePagesByTimeframe()
{
  let visits = [];
  let startDate = Date.now() * 1000;
  for (let i = 0; i < 10; i++) {
    visits.push({
      uri: NetUtil.newURI(TEST_URI.spec + i),
      visitDate: startDate + i
    });
  }

  promiseAddVisits(visits).then(function () {
    // Delete all pages except the first and the last.
    PlacesUtils.bhistory.removePagesByTimeframe(startDate + 1, startDate + 8);
  
    // Check that we have removed the correct pages.
    for (let i = 0; i < 10; i++) {
      do_check_eq(page_in_database(NetUtil.newURI(TEST_URI.spec + i)) == 0,
                  i > 0 && i < 9);
    }
  
    // Clear remaining items and check that all pages have been removed.
    PlacesUtils.bhistory.removePagesByTimeframe(startDate, startDate + 9);
    do_check_eq(0, PlacesUtils.history.hasHistoryEntries);
    run_next_test();
  });
});

add_test(function test_removePagesFromHost()
{
  promiseAddVisits(TEST_URI).then(function () {
    PlacesUtils.bhistory.removePagesFromHost("mozilla.com", true);
    do_check_eq(0, PlacesUtils.history.hasHistoryEntries);
    run_next_test();
  });
});

add_test(function test_removePagesFromHost_keepSubdomains()
{
  promiseAddVisits([{ uri: TEST_URI }, { uri: TEST_SUBDOMAIN_URI }]).then(function () {
    PlacesUtils.bhistory.removePagesFromHost("mozilla.com", false);
    do_check_eq(1, PlacesUtils.history.hasHistoryEntries);
    run_next_test();
  });
});

add_test(function test_removeAllPages()
{
  PlacesUtils.bhistory.removeAllPages();
  do_check_eq(0, PlacesUtils.history.hasHistoryEntries);
  run_next_test();
});

function run_test()
{
  run_next_test();
}
