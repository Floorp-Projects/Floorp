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
  let guids = [PlacesUtils.bookmarks.rootGuid,
               PlacesUtils.bookmarks.unfiledGuid,
               PlacesUtils.bookmarks.menuGuid,
               PlacesUtils.bookmarks.toolbarGuid,
               PlacesUtils.bookmarks.tagsGuid,
               PlacesUtils.bookmarks.mobileGuid];
  for (let guid of guids) {
    Assert.throws(() => PlacesUtils.bookmarks.remove(guid),
                  /It's not possible to remove Places root folders/);
  }
});

add_task(function* remove_normal_folder_under_root_succeeds() {
  let folder = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.rootGuid,
                                                    type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(folder);
  let removed_folder = yield PlacesUtils.bookmarks.remove(folder);
  Assert.deepEqual(folder, removed_folder);
  Assert.strictEqual((yield PlacesUtils.bookmarks.fetch(folder.guid)), null);
});

add_task(function* remove_bookmark() {
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm1);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm2.index, 0);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
  Assert.equal(bm2.url.href, "http://example.com/");
  Assert.equal(bm2.title, "a bookmark");
});


add_task(function* remove_bookmark_orphans() {
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm1);
  PlacesUtils.annotations.setItemAnnotation((yield PlacesUtils.promiseItemId(bm1.guid)),
                                            "testanno", "testvalue", 0, 0);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);

  // Check there are no orphan annotations.
  let conn = yield PlacesUtils.promiseDBConnection();
  let annoAttrs = yield conn.execute(`SELECT id, name FROM moz_anno_attributes`);
  // Bug 1306445 will eventually remove the mobile root anno.
  Assert.equal(annoAttrs.length, 1);
  Assert.equal(annoAttrs[0].getResultByName("name"), PlacesUtils.MOBILE_ROOT_ANNO);
  let annos = yield conn.execute(`SELECT item_id, anno_attribute_id FROM moz_items_annos`);
  Assert.equal(annos.length, 1);
  Assert.equal(annos[0].getResultByName("item_id"), PlacesUtils.mobileFolderId);
  Assert.equal(annos[0].getResultByName("anno_attribute_id"), annoAttrs[0].getResultByName("id"));
});

add_task(function* remove_bookmark_empty_title() {
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "" });
  checkBookmarkObject(bm1);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.index, 0);
  Assert.ok(!("title" in bm2));
});

add_task(function* remove_folder() {
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                 title: "a folder" });
  checkBookmarkObject(bm1);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm2.index, 0);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_FOLDER);
  Assert.equal(bm2.title, "a folder");
  Assert.ok(!("url" in bm2));
});

add_task(function* test_nested_contents_removed() {
  let folder1 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                     type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     title: "a folder" });
  let folder2 = yield PlacesUtils.bookmarks.insert({ parentGuid: folder1.guid,
                                                     type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     title: "a folder" });
  let sep = yield PlacesUtils.bookmarks.insert({ parentGuid: folder2.guid,
                                                 type: PlacesUtils.bookmarks.TYPE_SEPARATOR });
  yield PlacesUtils.bookmarks.remove(folder1);
  Assert.strictEqual((yield PlacesUtils.bookmarks.fetch(folder1.guid)), null);
  Assert.strictEqual((yield PlacesUtils.bookmarks.fetch(folder2.guid)), null);
  Assert.strictEqual((yield PlacesUtils.bookmarks.fetch(sep.guid)), null);
});

add_task(function* remove_folder_empty_title() {
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
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
  let bm1 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_SEPARATOR });
  checkBookmarkObject(bm1);

  let bm2 = yield PlacesUtils.bookmarks.remove(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm2.index, 0);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_SEPARATOR);
  Assert.ok(!("url" in bm2));
  Assert.ok(!("title" in bm2));
});

add_task(function* test_nested_content_fails_when_not_allowed() {
  let folder1 = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                     type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     title: "a folder" });
  yield PlacesUtils.bookmarks.insert({ parentGuid: folder1.guid,
                                       type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                       title: "a folder" });
  yield Assert.rejects(PlacesUtils.bookmarks.remove(folder1, {preventRemovalOfNonEmptyFolders: true}),
                       /Cannot remove a non-empty folder./);
});

function run_test() {
  run_next_test();
}
