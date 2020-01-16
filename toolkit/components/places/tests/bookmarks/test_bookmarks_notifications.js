/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

add_task(async function insert_separator_notification() {
  let observer = expectPlacesObserverNotifications(["bookmark-added"]);
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([
    {
      type: "bookmark-added",
      id: itemId,
      itemType: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      parentId,
      index: bm.index,
      url: bm.url,
      title: bm.title,
      dateAdded: bm.dateAdded,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});

add_task(async function insert_folder_notification() {
  let observer = expectPlacesObserverNotifications(["bookmark-added"]);
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "a folder",
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([
    {
      type: "bookmark-added",
      id: itemId,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentId,
      index: bm.index,
      url: bm.url,
      title: bm.title,
      dateAdded: bm.dateAdded,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});

add_task(async function insert_folder_notitle_notification() {
  let observer = expectPlacesObserverNotifications(["bookmark-added"]);
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  strictEqual(bm.title, "", "Should return empty string for untitled folder");
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([
    {
      type: "bookmark-added",
      id: itemId,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentId,
      index: bm.index,
      url: bm.url,
      title: bm.title,
      dateAdded: bm.dateAdded,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});

add_task(async function insert_bookmark_notification() {
  let observer = expectPlacesObserverNotifications(["bookmark-added"]);
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://example.com/"),
    title: "a bookmark",
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([
    {
      type: "bookmark-added",
      id: itemId,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentId,
      index: bm.index,
      url: bm.url,
      title: bm.title,
      dateAdded: bm.dateAdded,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});

add_task(async function insert_bookmark_notitle_notification() {
  let observer = expectPlacesObserverNotifications(["bookmark-added"]);
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://example.com/"),
  });
  strictEqual(bm.title, "", "Should return empty string for untitled bookmark");
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([
    {
      type: "bookmark-added",
      id: itemId,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentId,
      index: bm.index,
      url: bm.url,
      title: bm.title,
      dateAdded: bm.dateAdded,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});

add_task(async function insert_bookmark_tag_notification() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://tag.example.com/"),
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let tagFolder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.tagsGuid,
    title: "tag",
  });
  let placesObserver = expectPlacesObserverNotifications(["bookmark-added"]);
  let bookmarksObserver = expectNotifications();
  let tag = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: tagFolder.guid,
    url: new URL("http://tag.example.com/"),
  });
  let tagId = await PlacesUtils.promiseItemId(tag.guid);
  let tagParentId = await PlacesUtils.promiseItemId(tag.parentGuid);

  placesObserver.check([
    {
      type: "bookmark-added",
      id: tagId,
      parentId: tagParentId,
      index: tag.index,
      itemType: tag.type,
      url: tag.url,
      title: "",
      dateAdded: tag.dateAdded,
      guid: tag.guid,
      parentGuid: tag.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: true,
    },
  ]);

  bookmarksObserver.check([
    {
      name: "onItemChanged",
      arguments: [
        itemId,
        "tags",
        false,
        "",
        PlacesUtils.toPRTime(bm.lastModified),
        bm.type,
        parentId,
        bm.guid,
        bm.parentGuid,
        "",
        Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      ],
    },
  ]);
});

add_task(async function update_bookmark_lastModified() {
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function() {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://lastmod.example.com/"),
  });
  let observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    lastModified: new Date(),
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([
    {
      name: "onItemChanged",
      arguments: [
        itemId,
        "lastModified",
        false,
        `${PlacesUtils.toPRTime(bm.lastModified)}`,
        PlacesUtils.toPRTime(bm.lastModified),
        bm.type,
        parentId,
        bm.guid,
        bm.parentGuid,
        "",
        Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      ],
    },
  ]);
});

add_task(async function update_bookmark_title() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://title.example.com/"),
  });
  let observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    title: "new title",
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([
    {
      name: "onItemChanged",
      arguments: [
        itemId,
        "title",
        false,
        bm.title,
        PlacesUtils.toPRTime(bm.lastModified),
        bm.type,
        parentId,
        bm.guid,
        bm.parentGuid,
        "",
        Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      ],
    },
  ]);
});

add_task(async function update_bookmark_uri() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://url.example.com/"),
  });
  let observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    url: "http://mozilla.org/",
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([
    {
      name: "onItemChanged",
      arguments: [
        itemId,
        "uri",
        false,
        bm.url.href,
        PlacesUtils.toPRTime(bm.lastModified),
        bm.type,
        parentId,
        bm.guid,
        bm.parentGuid,
        "http://url.example.com/",
        Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      ],
    },
  ]);
});

add_task(async function update_move_same_folder() {
  // Ensure there are at least two items in place (others test do so for us,
  // but we don't have to depend on that).
  await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://move.example.com/"),
  });
  let bmItemId = await PlacesUtils.promiseItemId(bm.guid);
  let bmParentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  let bmOldIndex = bm.index;

  let observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  Assert.equal(bm.index, 0);
  observer.check([
    {
      name: "onItemMoved",
      arguments: [
        bmItemId,
        bmParentId,
        bmOldIndex,
        bmParentId,
        bm.index,
        bm.type,
        bm.guid,
        bm.parentGuid,
        bm.parentGuid,
        Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        "http://move.example.com/",
      ],
    },
  ]);

  // Test that we get the right index for DEFAULT_INDEX input.
  bmOldIndex = 0;
  observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
  });
  Assert.ok(bm.index > 0);
  observer.check([
    {
      name: "onItemMoved",
      arguments: [
        bmItemId,
        bmParentId,
        bmOldIndex,
        bmParentId,
        bm.index,
        bm.type,
        bm.guid,
        bm.parentGuid,
        bm.parentGuid,
        Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        bm.url,
      ],
    },
  ]);
});

add_task(async function update_move_different_folder() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://move.example.com/"),
  });
  let folder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let bmItemId = await PlacesUtils.promiseItemId(bm.guid);
  let bmOldParentId = await PlacesUtils.promiseItemId(
    PlacesUtils.bookmarks.unfiledGuid
  );
  let bmOldIndex = bm.index;

  let observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: folder.guid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
  });
  Assert.equal(bm.index, 0);
  let bmNewParentId = await PlacesUtils.promiseItemId(folder.guid);
  observer.check([
    {
      name: "onItemMoved",
      arguments: [
        bmItemId,
        bmOldParentId,
        bmOldIndex,
        bmNewParentId,
        bm.index,
        bm.type,
        bm.guid,
        PlacesUtils.bookmarks.unfiledGuid,
        bm.parentGuid,
        Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        "http://move.example.com/",
      ],
    },
  ]);
});

add_task(async function remove_bookmark() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://remove.example.com/"),
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectPlacesObserverNotifications(["bookmark-removed"]);
  await PlacesUtils.bookmarks.remove(bm.guid);
  observer.check([
    {
      type: "bookmark-removed",
      id: itemId,
      parentId,
      index: bm.index,
      url: bm.url,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      isTagging: false,
    },
  ]);
});

add_task(async function remove_multiple_bookmarks() {
  let bm1 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://remove.example.com/",
  });
  let bm2 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://remove1.example.com/",
  });
  let itemId1 = await PlacesUtils.promiseItemId(bm1.guid);
  let parentId1 = await PlacesUtils.promiseItemId(bm1.parentGuid);
  let itemId2 = await PlacesUtils.promiseItemId(bm2.guid);
  let parentId2 = await PlacesUtils.promiseItemId(bm2.parentGuid);

  let observer = expectPlacesObserverNotifications(["bookmark-removed"]);
  await PlacesUtils.bookmarks.remove([bm1, bm2]);
  observer.check([
    {
      type: "bookmark-removed",
      id: itemId1,
      parentId: parentId1,
      index: bm1.index,
      url: bm1.url,
      guid: bm1.guid,
      parentGuid: bm1.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      isTagging: false,
    },
    {
      type: "bookmark-removed",
      id: itemId2,
      parentId: parentId2,
      index: bm2.index - 1,
      url: bm2.url,
      guid: bm2.guid,
      parentGuid: bm2.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      isTagging: false,
    },
  ]);
});

add_task(async function remove_folder() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectPlacesObserverNotifications(["bookmark-removed"]);
  await PlacesUtils.bookmarks.remove(bm.guid);
  observer.check([
    {
      type: "bookmark-removed",
      id: itemId,
      parentId,
      index: bm.index,
      url: null,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      isTagging: false,
    },
  ]);
});

add_task(async function remove_bookmark_tag_notification() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://untag.example.com/"),
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let tagFolder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.tagsGuid,
    title: "tag",
  });
  let tag = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: tagFolder.guid,
    url: new URL("http://untag.example.com/"),
  });
  let tagId = await PlacesUtils.promiseItemId(tag.guid);
  let tagParentId = await PlacesUtils.promiseItemId(tag.parentGuid);

  let placesObserver = expectPlacesObserverNotifications(["bookmark-removed"]);
  let observer = expectNotifications();
  await PlacesUtils.bookmarks.remove(tag.guid);

  placesObserver.check([
    {
      type: "bookmark-removed",
      id: tagId,
      parentId: tagParentId,
      index: tag.index,
      url: tag.url,
      guid: tag.guid,
      parentGuid: tag.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      isTagging: true,
    },
  ]);
  observer.check([
    {
      name: "onItemChanged",
      arguments: [
        itemId,
        "tags",
        false,
        "",
        PlacesUtils.toPRTime(bm.lastModified),
        bm.type,
        parentId,
        bm.guid,
        bm.parentGuid,
        "",
        Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      ],
    },
  ]);
});

add_task(async function remove_folder_notification() {
  let folder1 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let folder1Id = await PlacesUtils.promiseItemId(folder1.guid);
  let folder1ParentId = await PlacesUtils.promiseItemId(folder1.parentGuid);

  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: folder1.guid,
    url: new URL("http://example.com/"),
  });
  let bmItemId = await PlacesUtils.promiseItemId(bm.guid);

  let folder2 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: folder1.guid,
  });
  let folder2Id = await PlacesUtils.promiseItemId(folder2.guid);

  let bm2 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: folder2.guid,
    url: new URL("http://example.com/"),
  });
  let bm2ItemId = await PlacesUtils.promiseItemId(bm2.guid);

  let observer = expectPlacesObserverNotifications(["bookmark-removed"]);
  await PlacesUtils.bookmarks.remove(folder1.guid);

  observer.check([
    {
      type: "bookmark-removed",
      id: bm2ItemId,
      parentId: folder2Id,
      index: bm2.index,
      url: bm2.url,
      guid: bm2.guid,
      parentGuid: bm2.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      isTagging: false,
    },
    {
      type: "bookmark-removed",
      id: folder2Id,
      parentId: folder1Id,
      index: folder2.index,
      url: null,
      guid: folder2.guid,
      parentGuid: folder2.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      isTagging: false,
    },
    {
      type: "bookmark-removed",
      id: bmItemId,
      parentId: folder1Id,
      index: bm.index,
      url: bm.url,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      isTagging: false,
    },
    {
      type: "bookmark-removed",
      id: folder1Id,
      parentId: folder1ParentId,
      index: folder1.index,
      url: null,
      guid: folder1.guid,
      parentGuid: folder1.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      isTagging: false,
    },
  ]);
});

add_task(async function eraseEverything_notification() {
  // Let's start from a clean situation.
  await PlacesUtils.bookmarks.eraseEverything();

  let folder1 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let folder1Id = await PlacesUtils.promiseItemId(folder1.guid);
  let folder1ParentId = await PlacesUtils.promiseItemId(folder1.parentGuid);

  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: folder1.guid,
    url: new URL("http://example.com/"),
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let folder2 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let folder2Id = await PlacesUtils.promiseItemId(folder2.guid);
  let folder2ParentId = await PlacesUtils.promiseItemId(folder2.parentGuid);

  let toolbarBm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: new URL("http://example.com/"),
  });
  let toolbarBmId = await PlacesUtils.promiseItemId(toolbarBm.guid);
  let toolbarBmParentId = await PlacesUtils.promiseItemId(toolbarBm.parentGuid);

  let menuBm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: new URL("http://example.com/"),
  });
  let menuBmId = await PlacesUtils.promiseItemId(menuBm.guid);
  let menuBmParentId = await PlacesUtils.promiseItemId(menuBm.parentGuid);

  let observer = expectPlacesObserverNotifications(["bookmark-removed"]);
  await PlacesUtils.bookmarks.eraseEverything();

  // Bookmarks should always be notified before their parents.
  observer.check([
    {
      type: "bookmark-removed",
      id: itemId,
      parentId,
      index: bm.index,
      url: bm.url,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      isTagging: false,
    },
    {
      type: "bookmark-removed",
      id: folder2Id,
      parentId: folder2ParentId,
      index: folder2.index,
      url: null,
      guid: folder2.guid,
      parentGuid: folder2.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      isTagging: false,
    },
    {
      type: "bookmark-removed",
      id: folder1Id,
      parentId: folder1ParentId,
      index: folder1.index,
      url: null,
      guid: folder1.guid,
      parentGuid: folder1.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      isTagging: false,
    },
    {
      type: "bookmark-removed",
      id: menuBmId,
      parentId: menuBmParentId,
      index: menuBm.index,
      url: menuBm.url,
      guid: menuBm.guid,
      parentGuid: menuBm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      isTagging: false,
    },
    {
      type: "bookmark-removed",
      id: toolbarBmId,
      parentId: toolbarBmParentId,
      index: toolbarBm.index,
      url: toolbarBm.url,
      guid: toolbarBm.guid,
      parentGuid: toolbarBm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      isTagging: false,
    },
  ]);
});

add_task(async function eraseEverything_reparented_notification() {
  // Let's start from a clean situation.
  await PlacesUtils.bookmarks.eraseEverything();

  let folder1 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let folder1Id = await PlacesUtils.promiseItemId(folder1.guid);
  let folder1ParentId = await PlacesUtils.promiseItemId(folder1.parentGuid);

  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: folder1.guid,
    url: new URL("http://example.com/"),
  });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);

  let folder2 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let folder2Id = await PlacesUtils.promiseItemId(folder2.guid);
  let folder2ParentId = await PlacesUtils.promiseItemId(folder2.parentGuid);

  bm.parentGuid = folder2.guid;
  bm = await PlacesUtils.bookmarks.update(bm);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectPlacesObserverNotifications(["bookmark-removed"]);
  await PlacesUtils.bookmarks.eraseEverything();

  // Bookmarks should always be notified before their parents.
  observer.check([
    {
      type: "bookmark-removed",
      id: itemId,
      parentId,
      index: bm.index,
      url: bm.url,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      isTagging: false,
    },
    {
      type: "bookmark-removed",
      id: folder2Id,
      parentId: folder2ParentId,
      index: folder2.index,
      url: null,
      guid: folder2.guid,
      parentGuid: folder2.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      isTagging: false,
    },
    {
      type: "bookmark-removed",
      id: folder1Id,
      parentId: folder1ParentId,
      index: folder1.index,
      url: null,
      guid: folder1.guid,
      parentGuid: folder1.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      isTagging: false,
    },
  ]);
});

add_task(async function reorder_notification() {
  let bookmarks = [
    {
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example1.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    },
    {
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    },
    {
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    },
    {
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example2.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    },
    {
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example3.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    },
  ];
  let sorted = [];
  for (let bm of bookmarks) {
    sorted.push(await PlacesUtils.bookmarks.insert(bm));
  }

  // Randomly reorder the array.
  sorted.sort(() => 0.5 - Math.random());

  let observer = expectNotifications();
  await PlacesUtils.bookmarks.reorder(
    PlacesUtils.bookmarks.unfiledGuid,
    sorted.map(bm => bm.guid)
  );

  let expectedNotifications = [];
  let unfiledBookmarksFolderId = await PlacesUtils.promiseItemId(
    PlacesUtils.bookmarks.unfiledGuid
  );
  for (let i = 0; i < sorted.length; ++i) {
    let child = sorted[i];
    let childId = await PlacesUtils.promiseItemId(child.guid);
    expectedNotifications.push({
      name: "onItemMoved",
      arguments: [
        childId,
        unfiledBookmarksFolderId,
        child.index,
        unfiledBookmarksFolderId,
        i,
        child.type,
        child.guid,
        child.parentGuid,
        child.parentGuid,
        Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        child.url,
      ],
    });
  }
  observer.check(expectedNotifications);
});

add_task(async function update_notitle_notification() {
  let toolbarBmURI = Services.io.newURI("https://example.com");
  let toolbarBmId = PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.toolbarFolderId,
    toolbarBmURI,
    0,
    "Bookmark"
  );
  let toolbarBmGuid = await PlacesUtils.promiseItemGuid(toolbarBmId);

  let menuFolder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "Folder",
  });
  let menuFolderId = await PlacesUtils.promiseItemId(menuFolder.guid);

  let observer = expectNotifications();

  PlacesUtils.bookmarks.setItemTitle(toolbarBmId, null);
  strictEqual(
    PlacesUtils.bookmarks.getItemTitle(toolbarBmId),
    "",
    "Legacy API should return empty string for untitled bookmark"
  );

  let updatedMenuBm = await PlacesUtils.bookmarks.update({
    guid: menuFolder.guid,
    title: null,
  });
  strictEqual(
    updatedMenuBm.title,
    "",
    "Async API should return empty string for untitled bookmark"
  );

  let toolbarBmModified = await PlacesUtils.bookmarks.fetch(toolbarBmGuid);
  observer.check([
    {
      name: "onItemChanged",
      arguments: [
        toolbarBmId,
        "title",
        false,
        "",
        PlacesUtils.toPRTime(toolbarBmModified.lastModified),
        PlacesUtils.bookmarks.TYPE_BOOKMARK,
        PlacesUtils.toolbarFolderId,
        toolbarBmGuid,
        PlacesUtils.bookmarks.toolbarGuid,
        "",
        PlacesUtils.bookmarks.SOURCES.DEFAULT,
      ],
    },
    {
      name: "onItemChanged",
      arguments: [
        menuFolderId,
        "title",
        false,
        "",
        PlacesUtils.toPRTime(updatedMenuBm.lastModified),
        PlacesUtils.bookmarks.TYPE_FOLDER,
        PlacesUtils.bookmarksMenuFolderId,
        menuFolder.guid,
        PlacesUtils.bookmarks.menuGuid,
        "",
        PlacesUtils.bookmarks.SOURCES.DEFAULT,
      ],
    },
  ]);
});

function expectNotifications() {
  let notifications = [];
  let observer = new Proxy(NavBookmarkObserver, {
    get(target, name) {
      if (name == "check") {
        PlacesUtils.bookmarks.removeObserver(observer);
        return expectedNotifications =>
          Assert.deepEqual(notifications, expectedNotifications);
      }

      if (name.startsWith("onItem")) {
        return (...origArgs) => {
          let args = Array.from(origArgs, arg => {
            if (arg && arg instanceof Ci.nsIURI) {
              return new URL(arg.spec);
            }
            return arg;
          });
          notifications.push({ name, arguments: args });
        };
      }

      if (name in target) {
        return target[name];
      }
      return undefined;
    },
  });
  PlacesUtils.bookmarks.addObserver(observer);
  return observer;
}
