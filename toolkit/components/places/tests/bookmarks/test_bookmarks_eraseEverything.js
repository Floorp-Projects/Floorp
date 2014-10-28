/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* test_eraseEverything() {
  yield promiseAddVisits({ uri: NetUtil.newURI("http://example.com/") });
  yield promiseAddVisits({ uri: NetUtil.newURI("http://mozilla.org/") });
  let frecencyForExample = frecencyForUrl("http://example.com/");
  let frecencyForMozilla = frecencyForUrl("http://example.com/");
  Assert.ok(frecencyForExample > 0);
  Assert.ok(frecencyForMozilla > 0);
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let unfiledFolder = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                           type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(unfiledFolder);
  let unfiledBookmark = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                             type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                             url: "http://example.com/",
                                                             keyword: "kw1" });
  checkBookmarkObject(unfiledBookmark);
  let unfiledBookmarkInFolder =
    yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledFolder.guid,
                                         type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                         url: "http://mozilla.org/" });
  checkBookmarkObject(unfiledBookmarkInFolder);
  PlacesUtils.annotations.setItemAnnotation((yield PlacesUtils.promiseItemId(unfiledBookmarkInFolder.guid)),
                                            "testanno1", "testvalue1", 0, 0);

  let menuGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.bookmarksMenuFolderId);
  let menuFolder = yield PlacesUtils.bookmarks.insert({ parentGuid: menuGuid,
                                                        type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(menuFolder);
  let menuBookmark = yield PlacesUtils.bookmarks.insert({ parentGuid: menuGuid,
                                                          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                          url: "http://example.com/",
                                                          keyword: "kw2" });
  checkBookmarkObject(unfiledBookmark);
  let menuBookmarkInFolder =
    yield PlacesUtils.bookmarks.insert({ parentGuid: menuFolder.guid,
                                         type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                         url: "http://mozilla.org/" });
  checkBookmarkObject(menuBookmarkInFolder);
  PlacesUtils.annotations.setItemAnnotation((yield PlacesUtils.promiseItemId(menuBookmarkInFolder.guid)),
                                            "testanno1", "testvalue1", 0, 0);

  let toolbarGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.toolbarFolderId);
  let toolbarFolder = yield PlacesUtils.bookmarks.insert({ parentGuid: toolbarGuid,
                                                           type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(toolbarFolder);
  let toolbarBookmark = yield PlacesUtils.bookmarks.insert({ parentGuid: toolbarGuid,
                                                             type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                             url: "http://example.com/",
                                                             keyword: "kw3" });
  checkBookmarkObject(toolbarBookmark);
  let toolbarBookmarkInFolder =
    yield PlacesUtils.bookmarks.insert({ parentGuid: toolbarFolder.guid,
                                         type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                         url: "http://mozilla.org/" });
  checkBookmarkObject(toolbarBookmarkInFolder);
  PlacesUtils.annotations.setItemAnnotation((yield PlacesUtils.promiseItemId(toolbarBookmarkInFolder.guid)),
                                            "testanno1", "testvalue1", 0, 0);

  yield promiseAsyncUpdates();
  Assert.ok(frecencyForUrl("http://example.com/") > frecencyForExample);
  Assert.ok(frecencyForUrl("http://example.com/") > frecencyForMozilla);

  yield PlacesUtils.bookmarks.eraseEverything();

  Assert.equal(frecencyForUrl("http://example.com/"), frecencyForExample);
  Assert.equal(frecencyForUrl("http://example.com/"), frecencyForMozilla);

  // Check there are no orphan keywords or annotations.
  let conn = yield PlacesUtils.promiseDBConnection();
  let rows = yield conn.execute(`SELECT * FROM moz_keywords`);
  Assert.equal(rows.length, 0);
  rows = yield conn.execute(`SELECT * FROM moz_items_annos`);
  Assert.equal(rows.length, 0);
  rows = yield conn.execute(`SELECT * FROM moz_anno_attributes`);
  Assert.equal(rows.length, 0);
});

add_task(function* test_eraseEverything_roots() {
  yield PlacesUtils.bookmarks.eraseEverything();

  // Ensure the roots have not been removed.
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  Assert.ok(yield PlacesUtils.bookmarks.fetch(unfiledGuid));
  let toolbarGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.toolbarFolderId);
  Assert.ok(yield PlacesUtils.bookmarks.fetch(toolbarGuid));
  let menuGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.bookmarksMenuFolderId);
  Assert.ok(yield PlacesUtils.bookmarks.fetch(menuGuid));
  let tagsGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.tagsFolderId);
  Assert.ok(yield PlacesUtils.bookmarks.fetch(tagsGuid));
  let rootGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.placesRootId);
  Assert.ok(yield PlacesUtils.bookmarks.fetch(rootGuid));
});

function run_test() {
  run_next_test();
}
