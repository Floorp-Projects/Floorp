/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_bookmark_unknown_fields() {
  let buf = await openMirror("unknown_fields");

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "mozBmk______",
        url: "https://mozilla.org",
        title: "Mozilla",
        tags: ["moz", "dot", "org"],
      },
    ],
  });
  await storeRecords(
    buf,
    shuffle([
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        children: ["mozBmk______"],
      },
      {
        id: "mozBmk______",
        parentid: "menu",
        type: "bookmark",
        title: "Mozilla",
        bmkUri: "https://mozilla.org",
        tags: ["moz", "dot", "org"],
        unknownStr: "an unknown field",
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  await storeRecords(
    buf,
    [
      {
        id: "mozBmk______",
        parentid: "menu",
        type: "bookmark",
        title: "New Mozilla",
        bmkUri: "https://mozilla.org",
        tags: ["moz", "dot", "org"],
        unknownStr: "a new unknown field",
      },
    ],
    { needsMerge: true }
  );

  let controller = new AbortController();
  const wasMerged = await buf.merge(controller.signal);
  Assert.ok(wasMerged);

  let itemRows = await buf.db.execute(`SELECT guid, unknownFields FROM items`);

  let updatedBookmark = itemRows.find(
    row => row.getResultByName("guid") == "mozBmk______"
  );
  deepEqual(JSON.parse(updatedBookmark.getResultByName("unknownFields")), {
    unknownStr: "a new unknown field",
  });

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_changes_unknown_fields_all_types() {
  let buf = await openMirror("unknown_fields_all");

  await storeRecords(
    buf,
    [
      {
        id: "menu",
        parentid: "places",
        type: "folder",
        title: "menu",
        children: ["bookmarkAAAA", "separatorAAA", "queryAAAAAAA"],
        unknownFolderField: "an unknown folder field",
      },
      {
        id: "bookmarkAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "Mozilla2",
        bmkUri: "https://mozilla.org",
        tags: ["moz", "dot", "org"],
        unknownStrField: "an unknown bookmark field",
        unknownStrObj: { newField: "unknown pt deux" },
      },
      {
        id: "separatorAAA",
        parentid: "menu",
        type: "separator",
        unknownSepField: "an unknown separator field",
      },
      {
        id: "queryAAAAAAA",
        parentid: "menu",
        type: "bookmark",
        title: "a query",
        bmkUri: "place:foo",
        unknownQueryField: "an unknown query field",
      },
    ],
    { needsMerge: true }
  );

  await PlacesTestUtils.markBookmarksAsSynced();

  let changesToUpload = await buf.apply();
  // Should be no local changes needing to be uploaded
  deepEqual(changesToUpload, {});

  // Make updates to all the type of bookmarks
  await PlacesUtils.bookmarks.update({
    guid: "menu________",
    title: "updated menu",
  });
  await PlacesUtils.bookmarks.update({
    guid: "bookmarkAAAA",
    title: "Mozilla3",
  });
  await PlacesUtils.bookmarks.update({ guid: "separatorAAA", index: 2 });
  await PlacesUtils.bookmarks.update({
    guid: "queryAAAAAAA",
    title: "an updated query",
  });

  // We should now have a bunch of changes to upload
  changesToUpload = await buf.apply();
  const { menu, bookmarkAAAA, separatorAAA, queryAAAAAAA } = changesToUpload;

  // Validate we have the updated title as well as the unknown fields
  Assert.equal(menu.cleartext.title, "updated menu");
  Assert.equal(menu.cleartext.unknownFolderField, "an unknown folder field");

  // Test bookmark unknown fields
  Assert.equal(bookmarkAAAA.cleartext.title, "Mozilla3");
  Assert.equal(
    bookmarkAAAA.cleartext.unknownStrField,
    "an unknown bookmark field"
  );
  deepEqual(bookmarkAAAA.cleartext.unknownStrObj, {
    newField: "unknown pt deux",
  });

  // Test separator unknown fields
  Assert.equal(
    separatorAAA.cleartext.unknownSepField,
    "an unknown separator field"
  );

  // Test query unknown fields
  Assert.equal(queryAAAAAAA.cleartext.title, "an updated query");
  Assert.equal(
    queryAAAAAAA.cleartext.unknownQueryField,
    "an unknown query field"
  );

  let itemRows = await buf.db.execute(`SELECT guid, unknownFields FROM items`);

  // Test bookmark correctly JSON'd in the mirror
  let remoteBookmark = itemRows.find(
    row => row.getResultByName("guid") == "bookmarkAAAA"
  );
  deepEqual(JSON.parse(remoteBookmark.getResultByName("unknownFields")), {
    unknownStrField: "an unknown bookmark field",
    unknownStrObj: { newField: "unknown pt deux" },
  });

  // Test folder correctly JSON'd in the mirror
  let remoteFolder = itemRows.find(
    row => row.getResultByName("guid") == "menu________"
  );
  deepEqual(JSON.parse(remoteFolder.getResultByName("unknownFields")), {
    unknownFolderField: "an unknown folder field",
  });
  // Test query correctly JSON'd in the mirror
  let remoteQuery = itemRows.find(
    row => row.getResultByName("guid") == "queryAAAAAAA"
  );
  deepEqual(JSON.parse(remoteQuery.getResultByName("unknownFields")), {
    unknownQueryField: "an unknown query field",
  });
  // Test separator correctly JSON'd in the mirror
  let remoteSeparator = itemRows.find(
    row => row.getResultByName("guid") == "separatorAAA"
  );
  deepEqual(JSON.parse(remoteSeparator.getResultByName("unknownFields")), {
    unknownSepField: "an unknown separator field",
  });

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
