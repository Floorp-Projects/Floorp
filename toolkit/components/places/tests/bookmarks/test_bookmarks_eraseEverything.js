/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function promiseManyFrecenciesChanged() {
  return new Promise(resolve => {
    let obs = new NavHistoryObserver();
    obs.onManyFrecenciesChanged = () => {
      Assert.ok(true, "onManyFrecenciesChanged is triggered.");
      PlacesUtils.history.removeObserver(obs)
      resolve();
    };

    PlacesUtils.history.addObserver(obs);
  });
}

add_task(async function test_eraseEverything() {
  await PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://example.com/") });
  await PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/") });
  let frecencyForExample = frecencyForUrl("http://example.com/");
  let frecencyForMozilla = frecencyForUrl("http://example.com/");
  Assert.ok(frecencyForExample > 0);
  Assert.ok(frecencyForMozilla > 0);
  let unfiledFolder = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                           type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(unfiledFolder);
  let unfiledBookmark = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                             type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                             url: "http://example.com/" });
  checkBookmarkObject(unfiledBookmark);
  let unfiledBookmarkInFolder =
    await PlacesUtils.bookmarks.insert({ parentGuid: unfiledFolder.guid,
                                         type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                         url: "http://mozilla.org/" });
  checkBookmarkObject(unfiledBookmarkInFolder);
  PlacesUtils.annotations.setItemAnnotation((await PlacesUtils.promiseItemId(unfiledBookmarkInFolder.guid)),
                                            "testanno1", "testvalue1", 0, 0);

  let menuFolder = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.menuGuid,
                                                        type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(menuFolder);
  let menuBookmark = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.menuGuid,
                                                          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                          url: "http://example.com/" });
  checkBookmarkObject(menuBookmark);
  let menuBookmarkInFolder =
    await PlacesUtils.bookmarks.insert({ parentGuid: menuFolder.guid,
                                         type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                         url: "http://mozilla.org/" });
  checkBookmarkObject(menuBookmarkInFolder);
  PlacesUtils.annotations.setItemAnnotation((await PlacesUtils.promiseItemId(menuBookmarkInFolder.guid)),
                                            "testanno1", "testvalue1", 0, 0);

  let toolbarFolder = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.toolbarGuid,
                                                           type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(toolbarFolder);
  let toolbarBookmark = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.toolbarGuid,
                                                             type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                             url: "http://example.com/" });
  checkBookmarkObject(toolbarBookmark);
  let toolbarBookmarkInFolder =
    await PlacesUtils.bookmarks.insert({ parentGuid: toolbarFolder.guid,
                                         type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                         url: "http://mozilla.org/" });
  checkBookmarkObject(toolbarBookmarkInFolder);
  PlacesUtils.annotations.setItemAnnotation((await PlacesUtils.promiseItemId(toolbarBookmarkInFolder.guid)),
                                            "testanno1", "testvalue1", 0, 0);

  await PlacesTestUtils.promiseAsyncUpdates();
  Assert.ok(frecencyForUrl("http://example.com/") > frecencyForExample);
  Assert.ok(frecencyForUrl("http://example.com/") > frecencyForMozilla);

  let manyFrecenciesPromise = promiseManyFrecenciesChanged();

  await PlacesUtils.bookmarks.eraseEverything();

  // Ensure we get an onManyFrecenciesChanged notification.
  await manyFrecenciesPromise;

  Assert.equal(frecencyForUrl("http://example.com/"), frecencyForExample);
  Assert.equal(frecencyForUrl("http://example.com/"), frecencyForMozilla);

  // Check there are no orphan annotations.
  let conn = await PlacesUtils.promiseDBConnection();
  let annoAttrs = await conn.execute(`SELECT id, name FROM moz_anno_attributes`);
  // Bug 1306445 will eventually remove the mobile root anno.
  Assert.equal(annoAttrs.length, 1);
  Assert.equal(annoAttrs[0].getResultByName("name"), PlacesUtils.MOBILE_ROOT_ANNO);
  let annos = await conn.execute(`SELECT item_id, anno_attribute_id FROM moz_items_annos`);
  Assert.equal(annos.length, 1);
  Assert.equal(annos[0].getResultByName("item_id"), PlacesUtils.mobileFolderId);
  Assert.equal(annos[0].getResultByName("anno_attribute_id"), annoAttrs[0].getResultByName("id"));
});

add_task(async function test_eraseEverything_roots() {
  await PlacesUtils.bookmarks.eraseEverything();

  // Ensure the roots have not been removed.
  Assert.ok(await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.unfiledGuid));
  Assert.ok(await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.toolbarGuid));
  Assert.ok(await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.menuGuid));
  Assert.ok(await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.tagsGuid));
  Assert.ok(await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.rootGuid));
});

add_task(async function test_eraseEverything_reparented() {
  // Create a folder with 1 bookmark in it...
  let folder1 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });
  let bookmark1 = await PlacesUtils.bookmarks.insert({
    parentGuid: folder1.guid,
    url: "http://example.com/"
  });
  // ...and a second folder.
  let folder2 = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });

  // Reparent the bookmark to the 2nd folder.
  bookmark1.parentGuid = folder2.guid;
  await PlacesUtils.bookmarks.update(bookmark1);

  // Erase everything.
  await PlacesUtils.bookmarks.eraseEverything();

  // All the above items should no longer be in the GUIDHelper cache.
  for (let guid of [folder1.guid, bookmark1.guid, folder2.guid]) {
    await Assert.rejects(PlacesUtils.promiseItemId(guid),
                         /no item found for the given GUID/);
  }
});
