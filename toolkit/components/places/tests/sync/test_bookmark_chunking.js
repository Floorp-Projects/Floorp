/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// These tests ensure we correctly chunk statements that exceed SQLite's
// binding parameter limit.

// Inserts 1500 unfiled bookmarks. Using `PlacesUtils.bookmarks.insertTree`
// is an order of magnitude slower, so we write bookmarks directly into the
// database.
async function insertManyUnfiledBookmarks(db, url) {
  await db.executeCached(
    `
    INSERT OR IGNORE INTO moz_places(id, url, url_hash, rev_host, hidden,
                                     frecency, guid)
    VALUES((SELECT id FROM moz_places
            WHERE url_hash = hash(:url) AND
                  url = :url), :url, hash(:url), :revHost, 0, -1,
           generate_guid())`,
    { url: url.href, revHost: PlacesUtils.getReversedHost(url) }
  );

  let guids = [];

  for (let position = 0; position < 1500; ++position) {
    let title = position.toString(10);
    let guid = title.padStart(12, "A");
    await db.executeCached(
      `
      INSERT INTO moz_bookmarks(guid, parent, fk, position, type, title,
                                syncStatus, syncChangeCounter)
      VALUES(:guid, (SELECT id FROM moz_bookmarks WHERE guid = :parentGuid),
             (SELECT id FROM moz_places WHERE url_hash = hash(:url) AND
                                              url = :url),
             :position, :type, :title, :syncStatus, 1)`,
      {
        guid,
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        position,
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        title,
        syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
      }
    );
    guids.push(guid);
  }

  return guids;
}

add_task(async function test_merged_item_chunking() {
  let buf = await openMirror("merged_item_chunking");

  info("Set up local tree with 1500 bookmarks");
  let localGuids = await buf.db.executeTransaction(function() {
    let url = new URL("http://example.com/a");
    return insertManyUnfiledBookmarks(buf.db, url);
  });
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Set up remote tree with 1500 bookmarks");
  let toolbarRecord = makeRecord({
    id: "toolbar",
    parentid: "places",
    type: "folder",
    children: [],
  });
  let records = [toolbarRecord];
  for (let i = 0; i < 1500; ++i) {
    let title = i.toString(10);
    let guid = title.padStart(12, "B");
    toolbarRecord.children.push(guid);
    records.push(
      makeRecord({
        id: guid,
        parentid: "toolbar",
        type: "bookmark",
        title,
        bmkUri: "http://example.com/b",
      })
    );
  }
  await buf.store(shuffle(records));

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(
    await buf.fetchUnmergedGuids(),
    [PlacesUtils.bookmarks.unfiledGuid],
    "Should leave unfiled with new remote structure unmerged"
  );

  let localChildRecordIds = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
    "toolbar"
  );
  deepEqual(
    localChildRecordIds,
    toolbarRecord.children,
    "Should apply all remote toolbar children"
  );

  let guidsToUpload = Object.keys(changesToUpload);
  deepEqual(
    guidsToUpload.sort(),
    ["unfiled", ...localGuids].sort(),
    "Should upload unfiled and all new local children"
  );

  await storeChangesInMirror(buf, changesToUpload);
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_deletion_chunking() {
  let buf = await openMirror("deletion_chunking");

  info("Set up local tree with 1500 bookmarks");
  let guids = await buf.db.executeTransaction(function() {
    let url = new URL("http://example.com/a");
    return insertManyUnfiledBookmarks(buf.db, url);
  });
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Delete them all on the server");
  let records = [
    makeRecord({
      id: "unfiled",
      parentid: "places",
      type: "folder",
      children: [],
    }),
  ];
  for (let guid of guids) {
    records.push(
      makeRecord({
        id: guid,
        deleted: true,
      })
    );
  }
  await buf.store(shuffle(records));

  info("Apply remote");
  let changesToUpload = await buf.apply();
  deepEqual(await buf.fetchUnmergedGuids(), [], "Should merge all items");
  deepEqual(changesToUpload, {}, "Should take all remote deletions");

  let tombstones = await PlacesTestUtils.fetchSyncTombstones();
  deepEqual(tombstones, [], "Shouldn't store tombstones for remote deletions");

  let localChildRecordIds = await PlacesSyncUtils.bookmarks.fetchChildRecordIds(
    "unfiled"
  );
  deepEqual(
    localChildRecordIds,
    [],
    "Should delete all unfiled children locally"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_weak_upload_chunking() {
  let buf = await openMirror("weak_upload_chunking");

  info("Set up empty local tree");
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Set up remote tree with 1500 bookmarks");
  let toolbarRecord = makeRecord({
    id: "toolbar",
    parentid: "places",
    type: "folder",
    children: [],
  });
  let records = [toolbarRecord];
  for (let i = 0; i < 1500; ++i) {
    let title = i.toString(10);
    let guid = title.padStart(12, "B");
    toolbarRecord.children.push(guid);
    records.push(
      makeRecord({
        id: guid,
        parentid: "toolbar",
        type: "bookmark",
        title,
        bmkUri: "http://example.com/b",
      })
    );
  }
  await buf.store(shuffle(records));

  info("Apply remote");
  let changesToUpload = await buf.apply({
    weakUpload: toolbarRecord.children,
  });

  let guidsToUpload = Object.keys(changesToUpload);
  deepEqual(
    guidsToUpload.sort(),
    toolbarRecord.children.sort(),
    "Should weakly upload records that haven't changed locally"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
