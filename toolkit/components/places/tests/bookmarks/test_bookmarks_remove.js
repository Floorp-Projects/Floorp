/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* invalid_input_throws() {
  Assert.throws(() => PlacesUtils.bookmarks.remove(),
                /Input should be a valid object/);
  Assert.throws(() => PlacesUtils.bookmarks.remove(null),
                /Input should be a valid object/);

  Assert.throws(() => PlacesUtils.bookmarks.remove("test"),
                /Invalid value for property 'guid'/);
  Assert.throws(() => PlacesUtils.bookmarks.remove(123),
                /Invalid value for property 'guid'/);

  Assert.throws(() => PlacesUtils.bookmarks.remove({ guid: "test" }),
                /Invalid value for property 'guid'/);
  Assert.throws(() => PlacesUtils.bookmarks.remove({ guid: null }),
                /Invalid value for property 'guid'/);
  Assert.throws(() => PlacesUtils.bookmarks.remove({ guid: 123 }),
                /Invalid value for property 'guid'/);

  Assert.throws(() => PlacesUtils.bookmarks.remove({ parentGuid: "test" }),
                /Invalid value for property 'parentGuid'/);
  Assert.throws(() => PlacesUtils.bookmarks.remove({ parentGuid: null }),
                /Invalid value for property 'parentGuid'/);
  Assert.throws(() => PlacesUtils.bookmarks.remove({ parentGuid: 123 }),
                /Invalid value for property 'parentGuid'/);

  Assert.throws(() => PlacesUtils.bookmarks.remove({ url: "http://te st/" }),
                /Invalid value for property 'url'/);
  Assert.throws(() => PlacesUtils.bookmarks.remove({ url: null }),
                /Invalid value for property 'url'/);
  Assert.throws(() => PlacesUtils.bookmarks.remove({ url: -10 }),
                /Invalid value for property 'url'/);
});

add_task(function* remove_nonexistent_guid() {
  try {
    yield PlacesUtils.bookmarks.remove({ guid: "123456789012"});
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/No bookmarks found for the provided GUID/.test(ex));
  }
});

add_task(function* remove_roots_fail() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  try {
    yield PlacesUtils.bookmarks.remove(unfiledGuid);
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/It's not possible to remove Places root folders/.test(ex));
  }

  let placesRootGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.placesRootId);
  try {
    yield PlacesUtils.bookmarks.remove(placesRootGuid);
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/It's not possible to remove Places root folders/.test(ex));
  }
});

add_task(function* remove_bookmark() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm1);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, unfiledGuid);
  Assert.equal(bm2.index, 0);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
  Assert.equal(bm2.url.href, "http://example.com/");
  Assert.equal(bm2.title, "a bookmark");
  Assert.ok(!("keyword" in bm2));
});


add_task(function* remove_bookmark_orphans() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "a bookmark",
                                                 keyword: "test"});
  checkBookmarkObject(bm1);
  PlacesUtils.annotations.setItemAnnotation((yield PlacesUtils.promiseItemId(bm1.guid)),
                                            "testanno", "testvalue", 0, 0);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);
  Assert.equal(bm2.keyword, "test");

  // Check there are no orphan keywords or annotations.
  let conn = yield PlacesUtils.promiseDBConnection();
  let rows = yield conn.execute(`SELECT * FROM moz_keywords`);
  Assert.equal(rows.length, 0);
  rows = yield conn.execute(`SELECT * FROM moz_items_annos`);
  Assert.equal(rows.length, 0);
  // removeItemAnnotations doesn't remove orphan annotations, cause it likely
  // relies on expiration to do so.
  //rows = yield conn.execute(`SELECT * FROM moz_anno_attributes`);
  //Assert.equal(rows.length, 0);
});

add_task(function* remove_bookmark_empty_title() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "" });
  checkBookmarkObject(bm1);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.index, 0);
  Assert.ok(!("title" in bm2));
  Assert.ok(!("keyword" in bm2));
});

add_task(function* remove_folder() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                 title: "a folder" });
  checkBookmarkObject(bm1);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, unfiledGuid);
  Assert.equal(bm2.index, 0);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_FOLDER);
  Assert.equal(bm2.title, "a folder");
  Assert.ok(!("url" in bm2));
  Assert.ok(!("keyword" in bm2));
});

add_task(function* remove_folder_empty_title() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                 title: "" });
  checkBookmarkObject(bm1);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.index, 0);
  Assert.ok(!("title" in bm2));
});

add_task(function* remove_separator() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_SEPARATOR });
  checkBookmarkObject(bm1);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, unfiledGuid);
  Assert.equal(bm2.index, 0);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_SEPARATOR);
  Assert.ok(!("url" in bm2));
  Assert.ok(!("title" in bm2));
  Assert.ok(!("keyword" in bm2));
});

function run_test() {
  run_next_test();
}
