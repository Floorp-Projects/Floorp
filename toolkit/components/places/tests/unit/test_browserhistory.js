/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = NetUtil.newURI("http://mozilla.com/");
const TEST_SUBDOMAIN_URI = NetUtil.newURI("http://foobar.mozilla.com/");

async function checkEmptyHistory() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.executeCached("SELECT count(*) FROM moz_historyvisits");
  return !rows[0].getResultByIndex(0);
}

add_task(async function test_addPage() {
  await PlacesTestUtils.addVisits(TEST_URI);
  Assert.ok(!await checkEmptyHistory(), "History has entries");
});

add_task(async function test_removePage() {
  await PlacesUtils.history.remove(TEST_URI);
  Assert.ok(await checkEmptyHistory(), "History is empty");
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
  Assert.ok(await checkEmptyHistory(), "History is empty");

  // Check that the bookmark and its annotation still exist.
  let folder = await PlacesUtils.getFolderContents(PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(folder.root.childCount, 1);
  Assert.equal(PlacesUtils.annotations.getPageAnnotation(pages[BOOKMARK_INDEX], ANNO_NAME),
               ANNO_VALUE);

  // Check the annotation on the non-bookmarked page does not exist anymore.
  try {
    PlacesUtils.annotations.getPageAnnotation(pages[ANNO_INDEX], ANNO_NAME);
    do_throw("did not expire expire_never anno on a not bookmarked item");
  } catch (ex) {}

  // Cleanup.
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
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
  await PlacesUtils.history.removeByFilter({
    beginDate: PlacesUtils.toDate(startDate + 1000),
    endDate: PlacesUtils.toDate(startDate + 8000)
  });

  // Check that we have removed the correct pages.
  for (let i = 0; i < 10; i++) {
    Assert.equal(page_in_database(NetUtil.newURI(TEST_URI.spec + i)) == 0,
                 i > 0 && i < 9);
  }

  // Clear remaining items and check that all pages have been removed.
  await PlacesUtils.history.removeByFilter({
    beginDate: PlacesUtils.toDate(startDate),
    endDate: PlacesUtils.toDate(startDate + 9000)
  });
  Assert.ok(await checkEmptyHistory(), "History is empty");
});

add_task(async function test_removePagesFromHost() {
  await PlacesTestUtils.addVisits(TEST_URI);
  await PlacesUtils.history.removeByFilter({ host: ".mozilla.com" });
  Assert.ok(await checkEmptyHistory(), "History is empty");
});

add_task(async function test_removePagesFromHost_keepSubdomains() {
  await PlacesTestUtils.addVisits([{ uri: TEST_URI }, { uri: TEST_SUBDOMAIN_URI }]);
  await PlacesUtils.history.removeByFilter({ host: "mozilla.com" });
  Assert.ok(!await checkEmptyHistory(), "History has entries");
});

add_task(async function test_history_clear() {
  await PlacesUtils.history.clear();
  Assert.ok(await checkEmptyHistory(), "History is empty");
});
