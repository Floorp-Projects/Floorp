/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = NetUtil.newURI("http://mozilla.com/");
const TEST_SUBDOMAIN_URI = NetUtil.newURI("http://foobar.mozilla.com/");

add_task(async function test_addPage() {
  await PlacesTestUtils.addVisits(TEST_URI);
  do_check_eq(1, PlacesUtils.history.hasHistoryEntries);
});

add_task(async function test_removePage() {
  await PlacesUtils.history.remove(TEST_URI);
  do_check_eq(0, PlacesUtils.history.hasHistoryEntries);
});

add_task(async function test_removePages() {
  let pages = [];
  for (let i = 0; i < 8; i++) {
    pages.push(NetUtil.newURI(TEST_URI.spec + i));
  }

  await PlacesTestUtils.addVisits(pages.map(uri => ({ uri })));
  // Bookmarked item should not be removed from moz_places.
  const ANNO_INDEX = 1;
  const ANNO_NAME = "testAnno";
  const ANNO_VALUE = "foo";
  const BOOKMARK_INDEX = 2;
  PlacesUtils.annotations.setPageAnnotation(pages[ANNO_INDEX],
                                            ANNO_NAME, ANNO_VALUE, 0,
                                            Ci.nsIAnnotationService.EXPIRE_NEVER);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: pages[BOOKMARK_INDEX],
    title: "test bookmark"
  });
  PlacesUtils.annotations.setPageAnnotation(pages[BOOKMARK_INDEX],
                                            ANNO_NAME, ANNO_VALUE, 0,
                                            Ci.nsIAnnotationService.EXPIRE_NEVER);

  await PlacesUtils.history.remove(pages);
  do_check_eq(0, PlacesUtils.history.hasHistoryEntries);

  // Check that the bookmark and its annotation still exist.
  let folder = await PlacesUtils.getFolderContents(PlacesUtils.unfiledBookmarksFolderId);
  do_check_eq(folder.root.childCount, 1);
  do_check_eq(PlacesUtils.annotations.getPageAnnotation(pages[BOOKMARK_INDEX], ANNO_NAME),
              ANNO_VALUE);

  // Check the annotation on the non-bookmarked page does not exist anymore.
  try {
    PlacesUtils.annotations.getPageAnnotation(pages[ANNO_INDEX], ANNO_NAME);
    do_throw("did not expire expire_never anno on a not bookmarked item");
  } catch (ex) {}

  // Cleanup.
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesTestUtils.clearHistory();
});

add_task(async function test_removePagesByTimeframe() {
  let visits = [];
  let startDate = (Date.now() - 10000) * 1000;
  for (let i = 0; i < 10; i++) {
    visits.push({
      uri: NetUtil.newURI(TEST_URI.spec + i),
      visitDate: startDate + i * 1000
    });
  }

  await PlacesTestUtils.addVisits(visits);

  // Delete all pages except the first and the last.
  PlacesUtils.bhistory.removePagesByTimeframe(startDate + 1000, startDate + 8000);

  // Check that we have removed the correct pages.
  for (let i = 0; i < 10; i++) {
    do_check_eq(page_in_database(NetUtil.newURI(TEST_URI.spec + i)) == 0,
                i > 0 && i < 9);
  }

  // Clear remaining items and check that all pages have been removed.
  PlacesUtils.bhistory.removePagesByTimeframe(startDate, startDate + 9000);
  do_check_eq(0, PlacesUtils.history.hasHistoryEntries);
});

add_task(async function test_removePagesFromHost() {
  await PlacesTestUtils.addVisits(TEST_URI);
  PlacesUtils.bhistory.removePagesFromHost("mozilla.com", true);
  do_check_eq(0, PlacesUtils.history.hasHistoryEntries);
});

add_task(async function test_removePagesFromHost_keepSubdomains() {
  await PlacesTestUtils.addVisits([{ uri: TEST_URI }, { uri: TEST_SUBDOMAIN_URI }]);
  PlacesUtils.bhistory.removePagesFromHost("mozilla.com", false);
  do_check_eq(1, PlacesUtils.history.hasHistoryEntries);
});

add_task(async function test_history_clear() {
  await PlacesTestUtils.clearHistory();
  do_check_eq(0, PlacesUtils.history.hasHistoryEntries);
});

add_task(async function test_getObservers() {
  // Ensure that getObservers() invalidates the hasHistoryEntries cache.
  await PlacesTestUtils.addVisits(TEST_URI);
  do_check_eq(1, PlacesUtils.history.hasHistoryEntries);
  // This is just for testing purposes, never do it.
  return new Promise((resolve, reject) => {
    DBConn().executeSimpleSQLAsync("DELETE FROM moz_historyvisits", {
      handleError(error) {
        reject(error);
      },
      handleResult(result) {
      },
      handleCompletion(result) {
        // Just invoking getObservers should be enough to invalidate the cache.
        PlacesUtils.history.getObservers();
        do_check_eq(0, PlacesUtils.history.hasHistoryEntries);
        resolve();
      }
    });
  });
});
