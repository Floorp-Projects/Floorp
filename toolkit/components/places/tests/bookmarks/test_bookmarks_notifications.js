/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* insert_separator_notification() {
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid});
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  null, null, bm.dateAdded * 1000,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* insert_folder_notification() {
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                title: "a folder" });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  null, bm.title, bm.dateAdded * 1000,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* insert_folder_notitle_notification() {
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  null, null, bm.dateAdded * 1000,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* insert_bookmark_notification() {
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://example.com/"),
                                                title: "a bookmark" });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  bm.url, bm.title, bm.dateAdded * 1000,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* insert_bookmark_notitle_notification() {
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  bm.url, null, bm.dateAdded * 1000,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* insert_bookmark_tag_notification() {
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://tag.example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let tagFolder = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                       parentGuid: PlacesUtils.bookmarks.tagsGuid,
                                                       title: "tag" });
  let observer = expectNotifications();
  let tag = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 parentGuid: tagFolder.guid,
                                                 url: new URL("http://tag.example.com/") });
  let tagId = yield PlacesUtils.promiseItemId(tag.guid);
  let tagParentId = yield PlacesUtils.promiseItemId(tag.parentGuid);

  observer.check([ { name: "onItemAdded",
                     arguments: [ tagId, tagParentId, tag.index, tag.type,
                                  tag.url, null, tag.dateAdded * 1000,
                                  tag.guid, tag.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] },
                   { name: "onItemChanged",
                     arguments: [ itemId, "tags", false, "",
                                  bm.lastModified * 1000, bm.type, parentId,
                                  bm.guid, bm.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* update_bookmark_lastModified() {
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://lastmod.example.com/") });
  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            lastModified: new Date() });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "lastModified", false,
                                  `${bm.lastModified * 1000}`, bm.lastModified * 1000,
                                  bm.type, parentId, bm.guid, bm.parentGuid, "",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* update_bookmark_title() {
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://title.example.com/") });
  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            title: "new title" });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "title", false, bm.title,
                                  bm.lastModified * 1000, bm.type, parentId, bm.guid,
                                  bm.parentGuid, "", Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* update_bookmark_uri() {
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://url.example.com/") });
  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            url: "http://mozilla.org/" });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "uri", false, bm.url.href,
                                  bm.lastModified * 1000, bm.type, parentId, bm.guid,
                                  bm.parentGuid, "http://url.example.com/",
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* update_move_same_folder() {
  // Ensure there are at least two items in place (others test do so for us,
  // but we don't have to depend on that).
  yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
                                       parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://move.example.com/") });
  let bmItemId = yield PlacesUtils.promiseItemId(bm.guid);
  let bmParentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  let bmOldIndex = bm.index;

  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
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
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                            index: PlacesUtils.bookmarks.DEFAULT_INDEX });
  Assert.ok(bm.index > 0);
  observer.check([ { name: "onItemMoved",
                     arguments: [ bmItemId, bmParentId, bmOldIndex, bmParentId, bm.index,
                                  bm.type, bm.guid, bm.parentGuid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* update_move_different_folder() {
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://move.example.com/") });
  let folder = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                    parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let bmItemId = yield PlacesUtils.promiseItemId(bm.guid);
  let bmOldParentId = PlacesUtils.unfiledBookmarksFolderId;
  let bmOldIndex = bm.index;

  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            parentGuid: folder.guid,
                                            index: PlacesUtils.bookmarks.DEFAULT_INDEX });
  Assert.equal(bm.index, 0);
  let bmNewParentId = yield PlacesUtils.promiseItemId(folder.guid);
  observer.check([ { name: "onItemMoved",
                     arguments: [ bmItemId, bmOldParentId, bmOldIndex, bmNewParentId,
                                  bm.index, bm.type, bm.guid,
                                  PlacesUtils.bookmarks.unfiledGuid,
                                  bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* remove_bookmark() {
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://remove.example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.remove(bm.guid);
  // TODO (Bug 653910): onItemAnnotationRemoved notified even if there were no
  // annotations.
  observer.check([ { name: "onItemRemoved",
                     arguments: [ itemId, parentId, bm.index, bm.type, bm.url,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* remove_folder() {
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.remove(bm.guid);
  observer.check([ { name: "onItemRemoved",
                     arguments: [ itemId, parentId, bm.index, bm.type, null,
                                  bm.guid, bm.parentGuid,
                                  Ci.nsINavBookmarksService.SOURCE_DEFAULT ] }
                 ]);
});

add_task(function* remove_bookmark_tag_notification() {
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: new URL("http://untag.example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let tagFolder = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                       parentGuid: PlacesUtils.bookmarks.tagsGuid,
                                                       title: "tag" });
  let tag = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 parentGuid: tagFolder.guid,
                                                 url: new URL("http://untag.example.com/") });
  let tagId = yield PlacesUtils.promiseItemId(tag.guid);
  let tagParentId = yield PlacesUtils.promiseItemId(tag.parentGuid);

  let observer = expectNotifications();
  yield PlacesUtils.bookmarks.remove(tag.guid);

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

add_task(function* remove_folder_notification() {
  let folder1 = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let folder1Id = yield PlacesUtils.promiseItemId(folder1.guid);
  let folder1ParentId = yield PlacesUtils.promiseItemId(folder1.parentGuid);

  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: folder1.guid,
                                                url: new URL("http://example.com/") });
  let bmItemId = yield PlacesUtils.promiseItemId(bm.guid);

  let folder2 = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: folder1.guid });
  let folder2Id = yield PlacesUtils.promiseItemId(folder2.guid);

  let bm2 = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 parentGuid: folder2.guid,
                                                 url: new URL("http://example.com/") });
  let bm2ItemId = yield PlacesUtils.promiseItemId(bm2.guid);

  let observer = expectNotifications();
  yield PlacesUtils.bookmarks.remove(folder1.guid);

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

add_task(function* eraseEverything_notification() {
  // Let's start from a clean situation.
  yield PlacesUtils.bookmarks.eraseEverything();

  let folder1 = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let folder1Id = yield PlacesUtils.promiseItemId(folder1.guid);
  let folder1ParentId = yield PlacesUtils.promiseItemId(folder1.parentGuid);

  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: folder1.guid,
                                                url: new URL("http://example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let folder2 = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let folder2Id = yield PlacesUtils.promiseItemId(folder2.guid);
  let folder2ParentId = yield PlacesUtils.promiseItemId(folder2.parentGuid);

  let toolbarBm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                       parentGuid: PlacesUtils.bookmarks.toolbarGuid,
                                                       url: new URL("http://example.com/") });
  let toolbarBmId = yield PlacesUtils.promiseItemId(toolbarBm.guid);
  let toolbarBmParentId = yield PlacesUtils.promiseItemId(toolbarBm.parentGuid);

  let menuBm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                    parentGuid: PlacesUtils.bookmarks.menuGuid,
                                                    url: new URL("http://example.com/") });
  let menuBmId = yield PlacesUtils.promiseItemId(menuBm.guid);
  let menuBmParentId = yield PlacesUtils.promiseItemId(menuBm.parentGuid);

  let observer = expectNotifications();
  yield PlacesUtils.bookmarks.eraseEverything();

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

add_task(function* eraseEverything_reparented_notification() {
  // Let's start from a clean situation.
  yield PlacesUtils.bookmarks.eraseEverything();

  let folder1 = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let folder1Id = yield PlacesUtils.promiseItemId(folder1.guid);
  let folder1ParentId = yield PlacesUtils.promiseItemId(folder1.parentGuid);

  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: folder1.guid,
                                                url: new URL("http://example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);

  let folder2 = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  let folder2Id = yield PlacesUtils.promiseItemId(folder2.guid);
  let folder2ParentId = yield PlacesUtils.promiseItemId(folder2.parentGuid);

  bm.parentGuid = folder2.guid;
  bm = yield PlacesUtils.bookmarks.update(bm);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectNotifications();
  yield PlacesUtils.bookmarks.eraseEverything();

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

add_task(function* reorder_notification() {
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
    sorted.push(yield PlacesUtils.bookmarks.insert(bm));
  }

  // Randomly reorder the array.
  sorted.sort(() => 0.5 - Math.random());

  let observer = expectNotifications();
  yield PlacesUtils.bookmarks.reorder(PlacesUtils.bookmarks.unfiledGuid,
                                      sorted.map(bm => bm.guid));

  let expectedNotifications = [];
  for (let i = 0; i < sorted.length; ++i) {
    let child = sorted[i];
    let childId = yield PlacesUtils.promiseItemId(child.guid);
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
            if (arg && typeof(arg) == "number" && arg >= Date.now() * 1000)
              return new Date(parseInt(arg / 1000));
            return arg;
          });
          notifications.push({ name: name, arguments: args });
        }
      }

      if (name in target)
        return target[name];
      return undefined;
    }
  });
  PlacesUtils.bookmarks.addObserver(observer, false);
  return observer;
}

function run_test() {
  run_next_test();
}
