/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
});

add_task(async function insert_separator_notification() {
  let observer = expectPlacesObserverNotifications(["bookmark-added"]);
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let parentId = await PlacesTestUtils.promiseItemId(bm.parentGuid);
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
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let parentId = await PlacesTestUtils.promiseItemId(bm.parentGuid);
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
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let parentId = await PlacesTestUtils.promiseItemId(bm.parentGuid);
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
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let parentId = await PlacesTestUtils.promiseItemId(bm.parentGuid);
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
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let parentId = await PlacesTestUtils.promiseItemId(bm.parentGuid);
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
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);

  let tagFolder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.tagsGuid,
    title: "tag",
  });
  const observer = expectPlacesObserverNotifications([
    "bookmark-added",
    "bookmark-tags-changed",
  ]);
  let tag = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: tagFolder.guid,
    url: new URL("http://tag.example.com/"),
  });
  let tagId = await PlacesTestUtils.promiseItemId(tag.guid);
  let tagParentId = await PlacesTestUtils.promiseItemId(tag.parentGuid);

  observer.check([
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
    {
      type: "bookmark-tags-changed",
      id: itemId,
      itemType: bm.type,
      url: bm.url,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      tags: ["tag"],
      lastModified: bm.lastModified,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});

add_task(async function update_bookmark_lastModified() {
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function () {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://lastmod.example.com/"),
  });
  const observer = expectPlacesObserverNotifications(["bookmark-time-changed"]);
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    lastModified: new Date(),
  });
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);

  observer.check([
    {
      type: "bookmark-time-changed",
      id: itemId,
      itemType: bm.type,
      url: bm.url,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      dateAdded: bm.dateAdded,
      lastModified: bm.lastModified,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});

add_task(async function update_bookmark_title() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://title.example.com/"),
  });
  const observer = expectPlacesObserverNotifications([
    "bookmark-title-changed",
  ]);
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    title: "new title",
  });
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);

  observer.check([
    {
      type: "bookmark-title-changed",
      id: itemId,
      itemType: bm.type,
      url: bm.url,
      title: bm.title,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      lastModified: bm.lastModified,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});

add_task(async function update_bookmark_uri() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://url.example.com/"),
  });
  const observer = expectPlacesObserverNotifications(["bookmark-url-changed"]);
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    url: "http://mozilla.org/",
  });
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);

  observer.check([
    {
      type: "bookmark-url-changed",
      id: itemId,
      itemType: bm.type,
      url: bm.url.href,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
      lastModified: bm.lastModified,
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
  const dateAdded = new Date();
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://move.example.com/"),
    dateAdded,
  });
  let bmItemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let bmOldIndex = bm.index;

  let observer = expectPlacesObserverNotifications(["bookmark-moved"]);

  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 0,
  });
  Assert.equal(bm.index, 0);
  observer.check([
    {
      type: "bookmark-moved",
      id: bmItemId,
      itemType: bm.type,
      url: "http://move.example.com/",
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      index: bm.index,
      oldParentGuid: bm.parentGuid,
      oldIndex: bmOldIndex,
      isTagging: false,
      title: bm.title,
      tags: "",
      frecency: 1,
      hidden: false,
      visitCount: 0,
      dateAdded: dateAdded.getTime(),
      lastVisitDate: null,
    },
  ]);

  // Test that we get the right index for DEFAULT_INDEX input.
  bmOldIndex = 0;
  observer = expectPlacesObserverNotifications(["bookmark-moved"]);
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
  });
  Assert.ok(bm.index > 0);
  observer.check([
    {
      type: "bookmark-moved",
      id: bmItemId,
      itemType: bm.type,
      url: bm.url,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      index: bm.index,
      oldParentGuid: bm.parentGuid,
      oldIndex: bmOldIndex,
      isTagging: false,
      title: bm.title,
      tags: "",
      frecency: 1,
      hidden: false,
      visitCount: 0,
      dateAdded: dateAdded.getTime(),
      lastVisitDate: null,
    },
  ]);
});

add_task(async function update_move_different_folder() {
  const dateAdded = new Date();
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://move.example.com/"),
    dateAdded,
  });
  let folder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let bmItemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let bmOldIndex = bm.index;

  const observer = expectPlacesObserverNotifications(["bookmark-moved"]);
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: folder.guid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
  });
  Assert.equal(bm.index, 0);
  observer.check([
    {
      type: "bookmark-moved",
      id: bmItemId,
      itemType: bm.type,
      url: "http://move.example.com/",
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      index: bm.index,
      oldParentGuid: PlacesUtils.bookmarks.unfiledGuid,
      oldIndex: bmOldIndex,
      isTagging: false,
      title: bm.title,
      tags: "",
      frecency: 1,
      hidden: false,
      visitCount: 0,
      dateAdded: dateAdded.getTime(),
      lastVisitDate: null,
    },
  ]);
});

add_task(async function update_move_tag_folder() {
  const dateAdded = new Date();
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://move.example.com/"),
    dateAdded,
  });
  let folder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.tagsGuid,
    title: "tag",
  });
  let bmItemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let bmOldIndex = bm.index;

  const observer = expectPlacesObserverNotifications(["bookmark-moved"]);
  bm = await PlacesUtils.bookmarks.update({
    guid: bm.guid,
    parentGuid: folder.guid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
  });
  Assert.equal(bm.index, 0);
  observer.check([
    {
      type: "bookmark-moved",
      id: bmItemId,
      itemType: bm.type,
      url: "http://move.example.com/",
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      index: bm.index,
      oldParentGuid: PlacesUtils.bookmarks.unfiledGuid,
      oldIndex: bmOldIndex,
      isTagging: true,
      title: bm.title,
      tags: "tag",
      frecency: 1,
      hidden: false,
      visitCount: 0,
      dateAdded: dateAdded.getTime(),
      lastVisitDate: null,
    },
  ]);
});

add_task(async function remove_bookmark() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://remove.example.com/"),
  });
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let parentId = await PlacesTestUtils.promiseItemId(bm.parentGuid);

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
  let itemId1 = await PlacesTestUtils.promiseItemId(bm1.guid);
  let parentId1 = await PlacesTestUtils.promiseItemId(bm1.parentGuid);
  let itemId2 = await PlacesTestUtils.promiseItemId(bm2.guid);
  let parentId2 = await PlacesTestUtils.promiseItemId(bm2.parentGuid);

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
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let parentId = await PlacesTestUtils.promiseItemId(bm.parentGuid);

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
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);

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
  let tagId = await PlacesTestUtils.promiseItemId(tag.guid);
  let tagParentId = await PlacesTestUtils.promiseItemId(tag.parentGuid);

  const observer = expectPlacesObserverNotifications([
    "bookmark-removed",
    "bookmark-tags-changed",
  ]);
  await PlacesUtils.bookmarks.remove(tag.guid);

  observer.check([
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
    {
      type: "bookmark-tags-changed",
      id: itemId,
      itemType: bm.type,
      url: bm.url,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      tags: [],
      lastModified: bm.lastModified,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});

add_task(async function rename_bookmark_tag_notification() {
  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL("http://renametag.example.com/"),
  });
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);

  let tagFolder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.tagsGuid,
    title: "tag",
  });
  let tag = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: tagFolder.guid,
    url: new URL("http://renametag.example.com/"),
  });
  let tagParentId = await PlacesTestUtils.promiseItemId(tag.parentGuid);

  const observer = expectPlacesObserverNotifications([
    "bookmark-title-changed",
    "bookmark-tags-changed",
  ]);
  tagFolder = await PlacesUtils.bookmarks.update({
    guid: tagFolder.guid,
    title: "renamed",
  });

  observer.check([
    {
      type: "bookmark-title-changed",
      id: tagParentId,
      title: "renamed",
      guid: tagFolder.guid,
      url: "",
      lastModified: tagFolder.lastModified,
      parentGuid: tagFolder.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      isTagging: true,
    },
    {
      type: "bookmark-tags-changed",
      id: itemId,
      itemType: bm.type,
      url: bm.url,
      guid: bm.guid,
      parentGuid: bm.parentGuid,
      tags: ["renamed"],
      lastModified: bm.lastModified,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});

add_task(async function remove_folder_notification() {
  let folder1 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let folder1Id = await PlacesTestUtils.promiseItemId(folder1.guid);
  let folder1ParentId = await PlacesTestUtils.promiseItemId(folder1.parentGuid);

  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: folder1.guid,
    url: new URL("http://example.com/"),
  });
  let bmItemId = await PlacesTestUtils.promiseItemId(bm.guid);

  let folder2 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: folder1.guid,
  });
  let folder2Id = await PlacesTestUtils.promiseItemId(folder2.guid);

  let bm2 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: folder2.guid,
    url: new URL("http://example.com/"),
  });
  let bm2ItemId = await PlacesTestUtils.promiseItemId(bm2.guid);

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

add_task(async function multiple_tags() {
  const BOOKMARK_URL = "http://multipletags.example.com/";
  const TAG_NAMES = ["tag1", "tag2", "tag3", "tag4", "tag5", "tag6"];

  const bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: new URL(BOOKMARK_URL),
  });
  const itemId = await PlacesTestUtils.promiseItemId(bm.guid);

  info("Register all tags");
  const tagFolders = await Promise.all(
    TAG_NAMES.map(tagName =>
      PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        parentGuid: PlacesUtils.bookmarks.tagsGuid,
        title: tagName,
      })
    )
  );

  info("Test adding tags to bookmark");
  for (let i = 0; i < tagFolders.length; i++) {
    const tagFolder = tagFolders[i];
    const expectedTagNames = TAG_NAMES.slice(0, i + 1);

    const observer = expectPlacesObserverNotifications([
      "bookmark-tags-changed",
    ]);

    await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      parentGuid: tagFolder.guid,
      url: new URL(BOOKMARK_URL),
    });

    observer.check([
      {
        type: "bookmark-tags-changed",
        id: itemId,
        itemType: bm.type,
        url: bm.url,
        guid: bm.guid,
        parentGuid: bm.parentGuid,
        tags: expectedTagNames,
        lastModified: bm.lastModified,
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        isTagging: false,
      },
    ]);
  }

  info("Test removing tags from bookmark");
  for (const removedLength of [1, 2, 3]) {
    const removedTags = tagFolders.splice(0, removedLength);

    const observer = expectPlacesObserverNotifications([
      "bookmark-tags-changed",
    ]);

    // We can remove multiple tags at one time.
    await PlacesUtils.bookmarks.remove(removedTags);

    const expectedResults = [];

    for (let i = 0; i < removedLength; i++) {
      TAG_NAMES.splice(0, 1);
      const expectedTagNames = [...TAG_NAMES];

      expectedResults.push({
        type: "bookmark-tags-changed",
        id: itemId,
        itemType: bm.type,
        url: bm.url,
        guid: bm.guid,
        parentGuid: bm.parentGuid,
        tags: expectedTagNames,
        lastModified: bm.lastModified,
        source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
        isTagging: false,
      });
    }

    observer.check(expectedResults);
  }
});

add_task(async function eraseEverything_notification() {
  // Let's start from a clean situation.
  await PlacesUtils.bookmarks.eraseEverything();

  let folder1 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let folder1Id = await PlacesTestUtils.promiseItemId(folder1.guid);
  let folder1ParentId = await PlacesTestUtils.promiseItemId(folder1.parentGuid);

  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: folder1.guid,
    url: new URL("http://example.com/"),
  });
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);
  let parentId = await PlacesTestUtils.promiseItemId(bm.parentGuid);

  let folder2 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let folder2Id = await PlacesTestUtils.promiseItemId(folder2.guid);
  let folder2ParentId = await PlacesTestUtils.promiseItemId(folder2.parentGuid);

  let toolbarBm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: new URL("http://example.com/"),
  });
  let toolbarBmId = await PlacesTestUtils.promiseItemId(toolbarBm.guid);
  let toolbarBmParentId = await PlacesTestUtils.promiseItemId(
    toolbarBm.parentGuid
  );

  let menuBm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: new URL("http://example.com/"),
  });
  let menuBmId = await PlacesTestUtils.promiseItemId(menuBm.guid);
  let menuBmParentId = await PlacesTestUtils.promiseItemId(menuBm.parentGuid);

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
  let folder1Id = await PlacesTestUtils.promiseItemId(folder1.guid);
  let folder1ParentId = await PlacesTestUtils.promiseItemId(folder1.parentGuid);

  let bm = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: folder1.guid,
    url: new URL("http://example.com/"),
  });
  let itemId = await PlacesTestUtils.promiseItemId(bm.guid);

  let folder2 = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let folder2Id = await PlacesTestUtils.promiseItemId(folder2.guid);
  let folder2ParentId = await PlacesTestUtils.promiseItemId(folder2.parentGuid);

  bm.parentGuid = folder2.guid;
  bm = await PlacesUtils.bookmarks.update(bm);
  let parentId = await PlacesTestUtils.promiseItemId(bm.parentGuid);

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
  const dateAdded = new Date();
  let bookmarks = [
    {
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example1.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      dateAdded,
    },
    {
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      dateAdded,
    },
    {
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      dateAdded,
    },
    {
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example2.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      dateAdded,
    },
    {
      type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example3.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      dateAdded,
    },
  ];
  let sorted = [];
  for (let bm of bookmarks) {
    sorted.push(await PlacesUtils.bookmarks.insert(bm));
  }

  // Randomly reorder the array.
  sorted.sort(() => 0.5 - Math.random());
  // Ensure there's at least one item out of place, since random does not
  // necessarily mean they are unordered.
  if (sorted[0].url == bookmarks[0].url) {
    sorted.push(sorted.shift());
  }

  const observer = expectPlacesObserverNotifications(["bookmark-moved"]);
  await PlacesUtils.bookmarks.reorder(
    PlacesUtils.bookmarks.unfiledGuid,
    sorted.map(bm => bm.guid)
  );

  let expectedNotifications = [];
  for (let i = 0; i < sorted.length; ++i) {
    let child = sorted[i];
    let childId = await PlacesTestUtils.promiseItemId(child.guid);
    expectedNotifications.push({
      type: "bookmark-moved",
      id: childId,
      itemType: child.type,
      url: child.url || "",
      guid: child.guid,
      parentGuid: child.parentGuid,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      index: i,
      oldParentGuid: child.parentGuid,
      oldIndex: child.index,
      isTagging: false,
      title: child.title,
      tags: "",
      frecency: child.url ? 1 : 0,
      hidden: false,
      visitCount: 0,
      dateAdded: dateAdded.getTime(),
      lastVisitDate: null,
    });
  }

  observer.check(expectedNotifications);
});

add_task(async function update_notitle_notification() {
  let toolbarBmURI = Services.io.newURI("https://example.com");
  let toolbarItemId = await PlacesTestUtils.promiseItemId(
    PlacesUtils.bookmarks.toolbarGuid
  );
  let toolbarBmId = PlacesUtils.bookmarks.insertBookmark(
    toolbarItemId,
    toolbarBmURI,
    0,
    "Bookmark"
  );
  let toolbarBmGuid = await PlacesTestUtils.promiseItemGuid(toolbarBmId);

  let menuFolder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "Folder",
  });
  let menuFolderId = await PlacesTestUtils.promiseItemId(menuFolder.guid);

  const observer = expectPlacesObserverNotifications([
    "bookmark-title-changed",
  ]);

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
      type: "bookmark-title-changed",
      id: toolbarBmId,
      itemType: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: toolbarBmURI.spec,
      title: "",
      guid: toolbarBmGuid,
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      lastModified: toolbarBmModified.lastModified,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
    {
      type: "bookmark-title-changed",
      id: menuFolderId,
      itemType: PlacesUtils.bookmarks.TYPE_FOLDER,
      url: "",
      title: "",
      guid: menuFolder.guid,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      lastModified: updatedMenuBm.lastModified,
      source: Ci.nsINavBookmarksService.SOURCE_DEFAULT,
      isTagging: false,
    },
  ]);
});
