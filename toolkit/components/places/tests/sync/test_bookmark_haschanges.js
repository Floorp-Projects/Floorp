/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_no_changes() {
  let buf = await openMirror("nochanges");

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
        id: "toolbar",
        parentid: "places",
        type: "folder",
        children: [],
      },
      {
        id: "unfiled",
        parentid: "places",
        type: "folder",
        children: [],
      },
      {
        id: "mobile",
        parentid: "places",
        type: "folder",
        children: [],
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

  let controller = new AbortController();
  const wasMerged = await buf.merge(controller.signal);
  Assert.ok(!wasMerged);

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_changes_remote() {
  let buf = await openMirror("remote_changes");

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
      },
    ],
    { needsMerge: true }
  );

  let controller = new AbortController();
  const wasMerged = await buf.merge(controller.signal);
  Assert.ok(wasMerged);

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_changes_local() {
  let buf = await openMirror("local_changes");

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

  await PlacesUtils.bookmarks.update({
    guid: "mozBmk______",
    title: "New Mozilla!",
  });

  let controller = new AbortController();
  const wasMerged = await buf.merge(controller.signal);
  Assert.ok(wasMerged);

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_changes_deleted_bookmark() {
  let buf = await openMirror("delete_bookmark");

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

  let wait = PlacesTestUtils.waitForNotification(
    "bookmark-removed",
    events =>
      events.some(event => event.parentGuid == PlacesUtils.bookmarks.tagsGuid),
    "places"
  );
  await PlacesUtils.bookmarks.remove("mozBmk______");

  await wait;
  // Wait for everything to be finished
  await new Promise(resolve => Services.tm.dispatchToMainThread(resolve));
  let controller = new AbortController();
  const wasMerged = await buf.merge(controller.signal);
  Assert.ok(wasMerged);

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
