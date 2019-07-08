/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_explicit_weakupload() {
  let buf = await openMirror("weakupload");

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
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  let changesToUpload = await buf.apply({
    weakUpload: ["mozBmk______"],
  });

  ok("mozBmk______" in changesToUpload);
  equal(changesToUpload.mozBmk______.counter, 0);

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_explicit_weakupload_with_dateAdded() {
  let buf = await openMirror("explicit_weakupload_with_dateAdded");

  info("Set up mirror");
  let dateAdded = new Date();
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [
      {
        guid: "mozBmk______",
        url: "https://mozilla.org",
        title: "Mozilla",
        dateAdded,
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
        dateAdded: dateAdded.getTime(),
      },
    ]),
    { needsMerge: false }
  );
  await PlacesTestUtils.markBookmarksAsSynced();

  info("Make remote change with older date added");
  await storeRecords(buf, [
    {
      id: "mozBmk______",
      parentid: "menu",
      type: "bookmark",
      title: "Firefox",
      bmkUri: "http://getfirefox.com/",
      dateAdded: dateAdded.getTime() + 5000,
    },
  ]);

  info("Explicitly request changed item for weak upload");
  let changesToUpload = await buf.apply({
    weakUpload: ["mozBmk______"],
  });
  deepEqual(changesToUpload, {
    mozBmk______: {
      tombstone: false,
      counter: 0,
      synced: false,
      cleartext: {
        id: "mozBmk______",
        type: "bookmark",
        title: "Firefox",
        bmkUri: "http://getfirefox.com/",
        parentid: "menu",
        hasDupe: true,
        parentName: "menu",
        dateAdded: dateAdded.getTime(),
      },
    },
  });

  let localInfo = await PlacesUtils.bookmarks.fetch("mozBmk______");
  equal(localInfo.title, "Firefox", "Should take new title from mirror");
  equal(
    localInfo.url.href,
    "http://getfirefox.com/",
    "Should take new URL from mirror"
  );
  equal(
    localInfo.dateAdded.getTime(),
    dateAdded.getTime(),
    "Should keep older local date added"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
