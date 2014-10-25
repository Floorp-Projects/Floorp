/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* insert_separator_notification() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
                                                parentGuid: unfiledGuid});
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  null, null, bm.dateAdded,
                                  bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* insert_folder_notification() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                parentGuid: unfiledGuid,
                                                title: "a folder" });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  null, bm.title, bm.dateAdded,
                                  bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* insert_folder_notitle_notification() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                parentGuid: unfiledGuid });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  null, null, bm.dateAdded,
                                  bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* insert_bookmark_notification() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: new URL("http://example.com/"),
                                                title: "a bookmark" });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  bm.url, bm.title, bm.dateAdded,
                                  bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* insert_bookmark_notitle_notification() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: new URL("http://example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  bm.url, null, bm.dateAdded,
                                  bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* insert_bookmark_keyword_notification() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let observer = expectNotifications();
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: new URL("http://example.com/"),
                                                keyword: "kw" });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);
  observer.check([ { name: "onItemAdded",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  bm.url, null, bm.dateAdded,
                                  bm.guid, bm.parentGuid ] },
                   { name: "onItemChanged",
                     arguments: [ itemId, "keyword", false, bm.keyword,
                                  bm.lastModified, bm.type, parentId,
                                  bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* insert_bookmark_tag_notification() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: new URL("http://tag.example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let tagsGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.tagsFolderId);
  let tagFolder = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                       parentGuid: tagsGuid,
                                                       title: "tag" });
  let observer = expectNotifications();
  let tag = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 parentGuid: tagFolder.guid,
                                                 url: new URL("http://tag.example.com/") });
  let tagId = yield PlacesUtils.promiseItemId(tag.guid);
  let tagParentId = yield PlacesUtils.promiseItemId(tag.parentGuid);

  observer.check([ { name: "onItemAdded",
                     arguments: [ tagId, tagParentId, tag.index, tag.type,
                                  tag.url, null, tag.dateAdded,
                                  tag.guid, tag.parentGuid ] },
                   { name: "onItemChanged",
                     arguments: [ tagId, "tags", false, "",
                                  tag.lastModified, tag.type, tagParentId,
                                  tag.guid, tag.parentGuid ] },
                   { name: "onItemChanged",
                     arguments: [ itemId, "tags", false, "",
                                  bm.lastModified, bm.type, parentId,
                                  bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* update_bookmark_lastModified() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: new URL("http://lastmod.example.com/") });
  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            lastModified: new Date() });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "lastModified", false,
                                  `${bm.lastModified * 1000}`, bm.lastModified,
                                  bm.type, parentId, bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* update_bookmark_title() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: new URL("http://title.example.com/") });
  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            title: "new title" });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "title", false, bm.title,
                                  bm.lastModified, bm.type, parentId, bm.guid,
                                  bm.parentGuid ] }
                 ]);
});

add_task(function* update_bookmark_uri() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: new URL("http://url.example.com/") });
  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            url: "http://mozilla.org/" });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "uri", false, bm.url.href,
                                  bm.lastModified, bm.type, parentId, bm.guid,
                                  bm.parentGuid ] }
                 ]);
});

add_task(function* update_bookmark_keyword() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: new URL("http://keyword.example.com/") });
  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            keyword: "kw" });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  observer.check([ { name: "onItemChanged",
                     arguments: [ itemId, "keyword", false, bm.keyword,
                                  bm.lastModified, bm.type, parentId, bm.guid,
                                  bm.parentGuid ] }
                 ]);
});

add_task(function* remove_bookmark() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: new URL("http://remove.example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.remove(bm.guid);
  // TODO (Bug 653910): onItemAnnotationRemoved notified even if there were no
  // annotations.
  observer.check([ { name: "onItemRemoved",
                     arguments: [ itemId, parentId, bm.index, bm.type, bm.url,
                                  bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* remove_folder() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                parentGuid: unfiledGuid });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let observer = expectNotifications();
  bm = yield PlacesUtils.bookmarks.remove(bm.guid);
  observer.check([ { name: "onItemRemoved",
                     arguments: [ itemId, parentId, bm.index, bm.type, null,
                                  bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* remove_bookmark_tag_notification() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: new URL("http://untag.example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let tagsGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.tagsFolderId);
  let tagFolder = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                       parentGuid: tagsGuid,
                                                       title: "tag" });
  let tag = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 parentGuid: tagFolder.guid,
                                                 url: new URL("http://untag.example.com/") });
  let tagId = yield PlacesUtils.promiseItemId(tag.guid);
  let tagParentId = yield PlacesUtils.promiseItemId(tag.parentGuid);

  let observer = expectNotifications();
  let removed = yield PlacesUtils.bookmarks.remove(tag.guid);

  observer.check([ { name: "onItemRemoved",
                     arguments: [ tagId, tagParentId, tag.index, tag.type,
                                  tag.url, tag.guid, tag.parentGuid ] },
                   { name: "onItemChanged",
                     arguments: [ itemId, "tags", false, "",
                                  bm.lastModified, bm.type, parentId,
                                  bm.guid, bm.parentGuid ] }
                 ]);
});

add_task(function* eraseEverything_notification() {
  // Let's start from a clean situation.
  yield PlacesUtils.bookmarks.eraseEverything();

  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let folder1 = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: unfiledGuid });
  let folder1Id = yield PlacesUtils.promiseItemId(folder1.guid);
  let folder1ParentId = yield PlacesUtils.promiseItemId(folder1.parentGuid);

  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: folder1.guid,
                                                url: new URL("http://example.com/") });
  let itemId = yield PlacesUtils.promiseItemId(bm.guid);
  let parentId = yield PlacesUtils.promiseItemId(bm.parentGuid);

  let folder2 = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                     parentGuid: unfiledGuid });
  let folder2Id = yield PlacesUtils.promiseItemId(folder2.guid);
  let folder2ParentId = yield PlacesUtils.promiseItemId(folder2.parentGuid);

  let toolbarGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.toolbarFolderId);
  let toolbarBm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                       parentGuid: toolbarGuid,
                                                       url: new URL("http://example.com/") });
  let toolbarBmId = yield PlacesUtils.promiseItemId(toolbarBm.guid);
  let toolbarBmParentId = yield PlacesUtils.promiseItemId(toolbarBm.parentGuid);

  let menuGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.bookmarksMenuFolderId);
  let menuBm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                    parentGuid: menuGuid,
                                                    url: new URL("http://example.com/") });
  let menuBmId = yield PlacesUtils.promiseItemId(menuBm.guid);
  let menuBmParentId = yield PlacesUtils.promiseItemId(menuBm.parentGuid);

  let observer = expectNotifications();
  let removed = yield PlacesUtils.bookmarks.eraseEverything();

  observer.check([ { name: "onItemRemoved",
                     arguments: [ menuBmId, menuBmParentId,
                                  menuBm.index, menuBm.type,
                                  menuBm.url, menuBm.guid,
                                  menuBm.parentGuid ] },
                   { name: "onItemRemoved",
                     arguments: [ toolbarBmId, toolbarBmParentId,
                                  toolbarBm.index, toolbarBm.type,
                                  toolbarBm.url, toolbarBm.guid,
                                  toolbarBm.parentGuid ] },
                   { name: "onItemRemoved",
                     arguments: [ folder2Id, folder2ParentId, folder2.index,
                                  folder2.type, null, folder2.guid,
                                  folder2.parentGuid ] },
                   { name: "onItemRemoved",
                     arguments: [ itemId, parentId, bm.index, bm.type,
                                  bm.url, bm.guid, bm.parentGuid ] },
                   { name: "onItemRemoved",
                     arguments: [ folder1Id, folder1ParentId, folder1.index,
                                  folder1.type, null, folder1.guid,
                                  folder1.parentGuid ] }
                 ]);
});

function expectNotifications() {
  let notifications = [];
  let observer = new Proxy(NavBookmarkObserver, {
    get(target, name) {
      if (name == "check") {
        PlacesUtils.bookmarks.removeObserver(observer);
        return expectedNotifications => {;
          for (let notification of notifications) {
            do_print(`Received ${notification.name}: ${JSON.stringify(notification.arguments)}`);

            if (expectedNotifications.length == 0)
              throw new Error("Received more notifications than expected");
            let expected = expectedNotifications.shift();

            Assert.equal(notification.name, expected.name);
            Assert.equal(notification.arguments.length, expected.arguments.length);

            for (let arg of notification.arguments) {
              let expectedArg = expected.arguments.shift();
              Assert.deepEqual(arg, expectedArg);
            }
          }
          Assert.equal(expectedNotifications.length, 0);
        }
      }

      if (name.startsWith("onItem")) {
        return () => {
          let args = Array.from(arguments, arg => {
            if (arg && arg instanceof Ci.nsIURI)
              return new URL(arg.spec);
            if (arg && typeof(arg) == "number" && arg >= Date.now() * 1000)
              return new Date(parseInt(arg/1000));
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
