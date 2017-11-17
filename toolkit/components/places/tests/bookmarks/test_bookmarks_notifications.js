/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function insert_separator_notification() {
  let observer = expectNotifications();
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid});
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  null, "", bm.dateAdded * 1000,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function insert_folder_notification() {
  let observer = expectNotifications();
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                title: "a folder" });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  null, bm.title, bm.dateAdded * 1000,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function insert_folder_notitle_notification() {
  let observer = expectNotifications();
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  strictEqual(bm.title, "", "Should return empty string for untitled folder");
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  null, "", bm.dateAdded * 1000,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function insert_bookmark_notification() {
  let observer = expectNotifications();
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://example.com/"),
                                                title: "a bookmark" });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  bm.url, bm.title, bm.dateAdded * 1000,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function insert_bookmark_notitle_notification() {
  let observer = expectNotifications();
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://example.com/") });
  strictEqual(bm.title, "", "Should return empty string for untitled bookmark");
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  bm.url, "", bm.dateAdded * 1000,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function insert_bookmark_tag_notification() {
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://tag.example.com/") });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let tagFolder = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                       parentGuid: PlacesUtils.bookmarks.tagsGuid,
                                                       title: "tag" });
  let observer = expectNotifications();
  let tag = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 parentGuid: tagFolder.guid,
                                                 url: new URL("http://tag.example.com/") });
  let tagId = await PlacesUtils.promiseItemId(tag.guid);
  let tagParentId = await PlacesUtils.promiseItemId(tag.parentGuid);

  observer.check([ { name: "onItemAdded",
                     arguments: [ tagId, tagParentId, tag.index, tag.type,
                                  tag.url, "", tag.dateAdded * 1000,
                                  tag.guid, tag.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemChanged",
                     arguments: [ itemId, "tags", false, "",
                                  bm.lastModified * 1000, bm.type, parentId,
                                  bm.guid, bm.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function update_bookmark_lastModified() {
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://lastmod.example.com/") });
  let observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            lastModified: new Date() });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "lastModified", false,
                                  `${bm.lastModified * 1000}`, bm.lastModified * 1000,
                                  bm.type, parentId, bm.guid, bm.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function update_bookmark_title() {
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://title.example.com/") });
  let observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            title: "new title" });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "title", false, bm.title,
                                  bm.lastModified * 1000, bm.type, parentId, bm.guid,
                                  bm.parentGuid, "", Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function update_bookmark_uri() {
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://url.example.com/") });
  let observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            url: "http://mozilla.org/" });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "uri", false, bm.url.href,
                                  bm.lastModified * 1000, bm.type, parentId, bm.guid,
                                  bm.parentGuid, "http://url.example.com/",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function update_move_same_folder() {
  // Ensure there are at least two items in place (others test do so for us,
  // but we don't have to depend on that).
  await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
                                       parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://move.example.com/") });
  let bmItemId = await PlacesUtils.promiseItemId(bm.guid);
  let bmParentId = await PlacesUtils.promiseItemId(bm.parentGuid);
  let bmOldIndex = bm.index;

  let observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                            index: 0 });
  Assert.equal(bm.index, 0);
  observer.check([ { name: "onItemMoved",
                     arguments: [ bmItemId, bmParentId, bmOldIndex, bmParentId, bm.index,
                                  bm.type, bm.guid, bm.parentGuid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);

  // Test that we get the right index for DEFAULT_INDEX input.
  bmOldIndex = 0;
  observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                            index: PlacesUtils.bookmarks.DEFAULT_INDEX });
  Assert.ok(bm.index > 0);
  observer.check([ { name: "onItemMoved",
                     arguments: [ bmItemId, bmParentId, bmOldIndex, bmParentId, bm.index,
                                  bm.type, bm.guid, bm.parentGuid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function update_move_different_folder() {
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://move.example.com/") });
  let folder = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                    parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let bmItemId = await PlacesUtils.promiseItemId(bm.guid);
  let bmOldParentId = PlacesUtils.unfiledBookmarksFolderId;
  let bmOldIndex = bm.index;

  let observer = expectNotifications();
  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            parentGuid: folder.guid,
                                            index: PlacesUtils.bookmarks.DEFAULT_INDEX });
  Assert.equal(bm.index, 0);
  let bmNewParentId = await PlacesUtils.promiseItemId(folder.guid);
  observer.check([ { name: "onItemMoved",
                     arguments: [ bmItemId, bmOldParentId, bmOldIndex, bmNewParentId,
                                  bm.index, bm.type, bm.guid,
                                  PlacesUtils.bookmarks.unfiledGuid,
                                  bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function remove_bookmark() {
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://remove.example.com/") });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectNotifications();
  await PlacesUtils.bookmarks.remove(bm.guid);
  observer.check([ { name: "onItemRemoved",
                     arguments: [ itemId, parentId, bm.index, bm.type, bm.url,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function remove_multiple_bookmarks() {
  let bm1 = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://remove.example.com/" });
  let bm2 = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://remove1.example.com/" });
  let itemId1 = await PlacesUtils.promiseItemId(bm1.guid);
  let parentId1 = await PlacesUtils.promiseItemId(bm1.parentGuid);
  let itemId2 = await PlacesUtils.promiseItemId(bm2.guid);
  let parentId2 = await PlacesUtils.promiseItemId(bm2.parentGuid);

  let observer = expectNotifications();
  await PlacesUtils.bookmarks.remove([bm1, bm2]);
  observer.check([ { name: "onItemRemoved",
                     arguments: [ itemId1, parentId1, bm1.index, bm1.type, bm1.url,
                                  bm1.guid, bm1.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemRemoved",
                     arguments: [ itemId2, parentId2, bm2.index, bm2.type, bm2.url,
                                  bm2.guid, bm2.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function remove_folder() {
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectNotifications();
  await PlacesUtils.bookmarks.remove(bm.guid);
  observer.check([ { name: "onItemRemoved",
                     arguments: [ itemId, parentId, bm.index, bm.type, null,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function remove_bookmark_tag_notification() {
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://untag.example.com/") });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let tagFolder = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                       parentGuid: PlacesUtils.bookmarks.tagsGuid,
                                                       title: "tag" });
  let tag = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 parentGuid: tagFolder.guid,
                                                 url: new URL("http://untag.example.com/") });
  let tagId = await PlacesUtils.promiseItemId(tag.guid);
  let tagParentId = await PlacesUtils.promiseItemId(tag.parentGuid);

  let observer = expectNotifications();
  await PlacesUtils.bookmarks.remove(tag.guid);

  observer.check([ { name: "onItemRemoved",
                     arguments: [ tagId, tagParentId, tag.index, tag.type,
                                  tag.url, tag.guid, tag.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemChanged",
                     arguments: [ itemId, "tags", false, "",
                                  bm.lastModified * 1000, bm.type, parentId,
                                  bm.guid, bm.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function remove_folder_notification() {
  let folder1 = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let folder1Id = await PlacesUtils.promiseItemId(folder1.guid);
  let folder1ParentId = await PlacesUtils.promiseItemId(folder1.parentGuid);

  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: folder1.guid,
                                                url: new URL("http://example.com/") });
  let bmItemId = await PlacesUtils.promiseItemId(bm.guid);

  let folder2 = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: folder1.guid });
  let folder2Id = await PlacesUtils.promiseItemId(folder2.guid);

  let bm2 = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 parentGuid: folder2.guid,
                                                 url: new URL("http://example.com/") });
  let bm2ItemId = await PlacesUtils.promiseItemId(bm2.guid);

  let observer = expectNotifications();
  await PlacesUtils.bookmarks.remove(folder1.guid);

  observer.check([ { name: "onItemRemoved",
                     arguments: [ bm2ItemId, folder2Id, bm2.index, bm2.type,
                                  bm2.url, bm2.guid, bm2.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemRemoved",
                     arguments: [ folder2Id, folder1Id, folder2.index,
                                  folder2.type, null, folder2.guid,
                                  folder2.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemRemoved",
                     arguments: [ bmItemId, folder1Id, bm.index, bm.type,
                                  bm.url, bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemRemoved",
                     arguments: [ folder1Id, folder1ParentId, folder1.index,
                                  folder1.type, null, folder1.guid,
                                  folder1.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(async function eraseEverything_notification() {
  // Let's start from a clean situation.
  await PlacesUtils.bookmarks.eraseEverything();

  let folder1 = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let folder1Id = await PlacesUtils.promiseItemId(folder1.guid);
  let folder1ParentId = await PlacesUtils.promiseItemId(folder1.parentGuid);

  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: folder1.guid,
                                                url: new URL("http://example.com/") });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let folder2 = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let folder2Id = await PlacesUtils.promiseItemId(folder2.guid);
  let folder2ParentId = await PlacesUtils.promiseItemId(folder2.parentGuid);

  let toolbarBm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                       parentGuid: PlacesUtils.bookmarks.toolbarGuid,
                                                       url: new URL("http://example.com/") });
  let toolbarBmId = await PlacesUtils.promiseItemId(toolbarBm.guid);
  let toolbarBmParentId = await PlacesUtils.promiseItemId(toolbarBm.parentGuid);

  let menuBm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                    parentGuid: PlacesUtils.bookmarks.menuGuid,
                                                    url: new URL("http://example.com/") });
  let menuBmId = await PlacesUtils.promiseItemId(menuBm.guid);
  let menuBmParentId = await PlacesUtils.promiseItemId(menuBm.parentGuid);

  let observer = expectNotifications();
  await PlacesUtils.bookmarks.eraseEverything();

  // Bookmarks should always be notified before their parents.
  observer.check([ { name: "onItemRemoved",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  bm.url, bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemRemoved",
                     arguments: [ folder2Id, folder2ParentId, folder2.index,
                                  folder2.type, null, folder2.guid,
                                  folder2.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemRemoved",
                     arguments: [ folder1Id, folder1ParentId, folder1.index,
                                  folder1.type, null, folder1.guid,
                                  folder1.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemRemoved",
                     arguments: [ menuBmId, menuBmParentId,
                                  menuBm.index, menuBm.type,
                                  menuBm.url, menuBm.guid,
                                  menuBm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                    { name: "onItemRemoved",
                     arguments: [ toolbarBmId, toolbarBmParentId,
                                  toolbarBm.index, toolbarBm.type,
                                  toolbarBm.url, toolbarBm.guid,
                                  toolbarBm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                 ]);
});

add_task(async function eraseEverything_reparented_notification() {
  // Let's start from a clean situation.
  await PlacesUtils.bookmarks.eraseEverything();

  let folder1 = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let folder1Id = await PlacesUtils.promiseItemId(folder1.guid);
  let folder1ParentId = await PlacesUtils.promiseItemId(folder1.parentGuid);

  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: folder1.guid,
                                                url: new URL("http://example.com/") });
  let itemId = await PlacesUtils.promiseItemId(bm.guid);

  let folder2 = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let folder2Id = await PlacesUtils.promiseItemId(folder2.guid);
  let folder2ParentId = await PlacesUtils.promiseItemId(folder2.parentGuid);

  bm.parentGuid = folder2.guid;
  bm = await PlacesUtils.bookmarks.update(bm);
  let parentId = await PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectNotifications();
  await PlacesUtils.bookmarks.eraseEverything();

  // Bookmarks should always be notified before their parents.
  observer.check([ { name: "onItemRemoved",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  bm.url, bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemRemoved",
                     arguments: [ folder2Id, folder2ParentId, folder2.index,
                                  folder2.type, null, folder2.guid,
                                  folder2.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemRemoved",
                     arguments: [ folder1Id, folder1ParentId, folder1.index,
                                  folder1.type, null, folder1.guid,
                                  folder1.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                 ]);
});

add_task(async function reorder_notification() {
  let bookmarks = [
    { type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example1.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example2.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
    { type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
      url: "http://example3.com/",
      parentGuid: PlacesUtils.bookmarks.unfiledGuid
    },
  ];
  let sorted = [];
  for (let bm of bookmarks) {
    sorted.push(await PlacesUtils.bookmarks.insert(bm));
  }

  // Randomly reorder the array.
  sorted.sort(() => 0.5 - Math.random());

  let observer = expectNotifications();
  await PlacesUtils.bookmarks.reorder(PlacesUtils.bookmarks.unfiledGuid,
                                      sorted.map(bm => bm.guid));

  let expectedNotifications = [];
  for (let i = 0; i < sorted.length; ++i) {
    let child = sorted[i];
    let childId = await PlacesUtils.promiseItemId(child.guid);
    expectedNotifications.push({ name: "onItemMoved",
                                 arguments: [ childId,
                                              PlacesUtils.unfiledBookmarksFolderId,
                                              child.index,
                                              PlacesUtils.unfiledBookmarksFolderId,
                                              i,
                                              child.type,
                                              child.guid,
                                              child.parentGuid,
                                              child.parentGuid,
                                              Ci.nsINavBookmarksService.SOURCE_DEFAULT
                                            ] });
  }
  observer.check(expectedNotifications);
});

add_task(async function update_notitle_notification() {
  let toolbarBmURI = Services.io.newURI("https://example.com");
  let toolbarBmId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId,
                                         toolbarBmURI, 0, "Bookmark");
  let toolbarBmGuid = await PlacesUtils.promiseItemGuid(toolbarBmId);

  let menuFolder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    index: 0,
    title: "Folder"
  });
  let menuFolderId = await PlacesUtils.promiseItemId(menuFolder.guid);

  let observer = expectNotifications();

  PlacesUtils.bookmarks.setItemTitle(toolbarBmId, null);
  strictEqual(PlacesUtils.bookmarks.getItemTitle(toolbarBmId), "",
    "Legacy API should return empty string for untitled bookmark");

  let updatedMenuBm = await PlacesUtils.bookmarks.update({
    guid: menuFolder.guid,
    title: null,
  });
  strictEqual(updatedMenuBm.title, "",
    "Async API should return empty string for untitled bookmark");

  let toolbarBmModified =
    PlacesUtils.toDate(PlacesUtils.bookmarks.getItemLastModified(toolbarBmId));
  observer.check([{
    name: "onItemChanged",
    arguments: [toolbarBmId, "title", false, "", toolbarBmModified * 1000,
                PlacesUtils.bookmarks.TYPE_BOOKMARK,
                PlacesUtils.toolbarFolderId, toolbarBmGuid,
                PlacesUtils.bookmarks.toolbarGuid,
                "", PlacesUtils.bookmarks.SOURCES.DEFAULT],
  }, {
    name: "onItemChanged",
    arguments: [menuFolderId, "title", false, "",
                updatedMenuBm.lastModified * 1000,
                PlacesUtils.bookmarks.TYPE_FOLDER,
                PlacesUtils.bookmarksMenuFolderId, menuFolder.guid,
                PlacesUtils.bookmarks.menuGuid,
                "", PlacesUtils.bookmarks.SOURCES.DEFAULT],
  }]);
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
            if (arg && arg instanceof Ci.nsIURI)
              return new URL(arg.spec);
            return arg;
          });
          notifications.push({ name, arguments: args });
        };
      }

      if (name in target)
        return target[name];
      return undefined;
    }
  });
  PlacesUtils.bookmarks.addObserver(observer);
  return observer;
}
