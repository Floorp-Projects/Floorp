/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const UNVISITED_BOOKMARK_BONUS = 140;

function promiseFrecencyChanged(expectedURI, expectedFrecency) {
  return PlacesTestUtils.waitForNotification("onFrecencyChanged",
    (uri, newFrecency) => {
      Assert.equal(uri.spec, expectedURI, "onFrecencyChanged is triggered for the correct uri.");
      Assert.equal(newFrecency, expectedFrecency, "onFrecencyChanged has the expected frecency");
      return true;
    }, "history");
}

add_task(async function setup() {
  Services.prefs.setIntPref("places.frecency.unvisitedBookmarkBonus", UNVISITED_BOOKMARK_BONUS);
});

add_task(async function invalid_input_throws() {
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

add_task(async function remove_nonexistent_guid() {
  try {
    await PlacesUtils.bookmarks.remove({ guid: "123456789012"});
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/No bookmarks found for the provided GUID/.test(ex));
  }
});

add_task(async function remove_roots_fail() {
  let guids = [PlacesUtils.bookmarks.rootGuid,
               PlacesUtils.bookmarks.unfiledGuid,
               PlacesUtils.bookmarks.menuGuid,
               PlacesUtils.bookmarks.toolbarGuid,
               PlacesUtils.bookmarks.tagsGuid,
               PlacesUtils.bookmarks.mobileGuid];
  for (let guid of guids) {
    Assert.throws(() => PlacesUtils.bookmarks.remove(guid),
                  /It's not possible to remove Places root folders\./);
  }
});

add_task(async function remove_bookmark() {
  // When removing a bookmark we need to check the frecency. First we confirm
  // that there is a normal update when it is inserted.
  let frecencyChangedPromise = promiseFrecencyChanged("http://example.com/",
    UNVISITED_BOOKMARK_BONUS);
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm1);

  await frecencyChangedPromise;

  // This second one checks the frecency is changed when we remove the bookmark.
  frecencyChangedPromise = promiseFrecencyChanged("http://example.com/", 0);

  await PlacesUtils.bookmarks.remove(bm1.guid);

  await frecencyChangedPromise;
});

add_task(async function remove_multiple_bookmarks() {
  // When removing a bookmark we need to check the frecency. First we confirm
  // that there is a normal update when it is inserted.
  let frecencyChangedPromise = promiseFrecencyChanged("http://example.com/",
    UNVISITED_BOOKMARK_BONUS);
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm1);

  let frecencyChangedPromise1 = promiseFrecencyChanged("http://example1.com/",
    UNVISITED_BOOKMARK_BONUS);
  let bm2 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example1.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm2);

  await Promise.all([frecencyChangedPromise, frecencyChangedPromise1]);

  // We should get an onManyFrecenciesChanged notification with the removal of
  // multiple bookmarks.
  let manyFrencenciesPromise =
    PlacesTestUtils.waitForNotification("onManyFrecenciesChanged", () => true, "history");

  await PlacesUtils.bookmarks.remove([bm1, bm2]);

  await manyFrencenciesPromise;
});

add_task(async function remove_bookmark_orphans() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm1);
  await setItemAnnotation(bm1.guid, "testanno", "testvalue");

  await PlacesUtils.bookmarks.remove(bm1.guid);

  // Check there are no orphan annotations.
  let conn = await PlacesUtils.promiseDBConnection();
  let annoAttrs = await conn.execute(`SELECT id, name FROM moz_anno_attributes`);
  Assert.equal(annoAttrs.length, 0);
  let annos = await conn.execute(`SELECT item_id, anno_attribute_id FROM moz_items_annos`);
  Assert.equal(annos.length, 0);
});

add_task(async function remove_bookmark_empty_title() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "" });
  checkBookmarkObject(bm1);

  await PlacesUtils.bookmarks.remove(bm1.guid);
  Assert.strictEqual((await PlacesUtils.bookmarks.fetch(bm1.guid)), null);
});

add_task(async function remove_folder() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                 title: "a folder" });
  checkBookmarkObject(bm1);

  await PlacesUtils.bookmarks.remove(bm1.guid);
  Assert.strictEqual((await PlacesUtils.bookmarks.fetch(bm1.guid)), null);

  // No wait for onManyFrecenciesChanged in this test as the folder doesn't have
  // any children that would need updating.
});

add_task(async function test_contents_removed() {
  let folder1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                     type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     title: "a folder" });
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: folder1.guid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "" });

  let skipDescendantsObserver = expectNotifications(true);
  let receiveAllObserver = expectNotifications(false);
  let frecencyChangedPromise = promiseFrecencyChanged("http://example.com/", 0);
  await PlacesUtils.bookmarks.remove(folder1);
  Assert.strictEqual((await PlacesUtils.bookmarks.fetch(folder1.guid)), null);
  Assert.strictEqual((await PlacesUtils.bookmarks.fetch(bm1.guid)), null);

  await frecencyChangedPromise;

  let expectedNotifications = [{
    name: "onItemRemoved",
    arguments: {
      guid: folder1.guid,
    },
  }];

  // If we're skipping descendents, we'll only be notified of the folder.
  skipDescendantsObserver.check(expectedNotifications);

  // Note: Items of folders get notified first.
  expectedNotifications.unshift({
    name: "onItemRemoved",
    arguments: {
      guid: bm1.guid,
    },
  });

  // If we don't skip descendents, we'll be notified of the folder and the
  // bookmark.
  receiveAllObserver.check(expectedNotifications);
});


add_task(async function test_nested_contents_removed() {
  let folder1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                     type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     title: "a folder" });
  let folder2 = await PlacesUtils.bookmarks.insert({ parentGuid: folder1.guid,
                                                     type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     title: "a folder" });
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: folder2.guid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "" });

  let frecencyChangedPromise = promiseFrecencyChanged("http://example.com/", 0);
  await PlacesUtils.bookmarks.remove(folder1);
  Assert.strictEqual((await PlacesUtils.bookmarks.fetch(folder1.guid)), null);
  Assert.strictEqual((await PlacesUtils.bookmarks.fetch(folder2.guid)), null);
  Assert.strictEqual((await PlacesUtils.bookmarks.fetch(bm1.guid)), null);

  await frecencyChangedPromise;
});

add_task(async function remove_folder_empty_title() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                 title: "" });
  checkBookmarkObject(bm1);

  await PlacesUtils.bookmarks.remove(bm1.guid);
  Assert.strictEqual((await PlacesUtils.bookmarks.fetch(bm1.guid)), null);
});

add_task(async function remove_separator() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_SEPARATOR });
  checkBookmarkObject(bm1);

  await PlacesUtils.bookmarks.remove(bm1.guid);
  Assert.strictEqual((await PlacesUtils.bookmarks.fetch(bm1.guid)), null);
});

add_task(async function test_nested_content_fails_when_not_allowed() {
  let folder1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                     type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     title: "a folder" });
  await PlacesUtils.bookmarks.insert({ parentGuid: folder1.guid,
                                       type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                       title: "a folder" });
  await Assert.rejects(PlacesUtils.bookmarks.remove(folder1, {preventRemovalOfNonEmptyFolders: true}),
                       /Cannot remove a non-empty folder./);
});
