/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* test_eraseEverything() {
  yield PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://example.com/") });
  yield PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/") });
  let frecencyForExample = frecencyForUrl("http://example.com/");
  let frecencyForMozilla = frecencyForUrl("http://example.com/");
  Assert.ok(frecencyForExample > 0);
  Assert.ok(frecencyForMozilla > 0);
  let unfiledFolder = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                           type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(unfiledFolder);
  let unfiledBookmark = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                             type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                             url: "http://example.com/" });
  checkBookmarkObject(unfiledBookmark);
  let unfiledBookmarkInFolder =
    yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledFolder.guid,
                                         type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                         url: "http://mozilla.org/" });
  checkBookmarkObject(unfiledBookmarkInFolder);
  PlacesUtils.annotations.setItemAnnotation((yield PlacesUtils.promiseItemId(unfiledBookmarkInFolder.guid)),
                                            "testanno1", "testvalue1", 0, 0);

  let menuFolder = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.menuGuid,
                                                        type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(menuFolder);
  let menuBookmark = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.menuGuid,
                                                          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                          url: "http://example.com/" });
  checkBookmarkObject(unfiledBookmark);
  let menuBookmarkInFolder =
    yield PlacesUtils.bookmarks.insert({ parentGuid: menuFolder.guid,
                                         type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                         url: "http://mozilla.org/" });
  checkBookmarkObject(menuBookmarkInFolder);
  PlacesUtils.annotations.setItemAnnotation((yield PlacesUtils.promiseItemId(menuBookmarkInFolder.guid)),
                                            "testanno1", "testvalue1", 0, 0);

  let toolbarFolder = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.toolbarGuid,
                                                           type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(toolbarFolder);
  let toolbarBookmark = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.toolbarGuid,
                                                             type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                             url: "http://example.com/" });
  checkBookmarkObject(toolbarBookmark);
  let toolbarBookmarkInFolder =
    yield PlacesUtils.bookmarks.insert({ parentGuid: toolbarFolder.guid,
                                         type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                         url: "http://mozilla.org/" });
  checkBookmarkObject(toolbarBookmarkInFolder);
  PlacesUtils.annotations.setItemAnnotation((yield PlacesUtils.promiseItemId(toolbarBookmarkInFolder.guid)),
                                            "testanno1", "testvalue1", 0, 0);

  yield PlacesTestUtils.promiseAsyncUpdates();
  Assert.ok(frecencyForUrl("http://example.com/") > frecencyForExample);
  Assert.ok(frecencyForUrl("http://example.com/") > frecencyForMozilla);

  yield PlacesUtils.bookmarks.eraseEverything();

  Assert.equal(frecencyForUrl("http://example.com/"), frecencyForExample);
  Assert.equal(frecencyForUrl("http://example.com/"), frecencyForMozilla);

  // Check there are no orphan annotations.
  let conn = yield PlacesUtils.promiseDBConnection();
  let rows = yield conn.execute(`SELECT * FROM moz_items_annos`);
  Assert.equal(rows.length, 0);
  rows = yield conn.execute(`SELECT * FROM moz_anno_attributes`);
  Assert.equal(rows.length, 0);
});

add_task(function* test_eraseEverything_roots() {
  yield PlacesUtils.bookmarks.eraseEverything();

  // Ensure the roots have not been removed.
  Assert.ok(yield PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.unfiledGuid));
  Assert.ok(yield PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.toolbarGuid));
  Assert.ok(yield PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.menuGuid));
  Assert.ok(yield PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.tagsGuid));
  Assert.ok(yield PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.rootGuid));
});

function run_test() {
  run_next_test();
}
