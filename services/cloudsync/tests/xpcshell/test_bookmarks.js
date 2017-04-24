/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/CloudSync.jsm");

function run_test() {
  run_next_test();
}

add_task(function* test_merge_bookmarks_flat() {
  try {
    let rootFolder = yield CloudSync().bookmarks.getRootFolder("TEST");
    ok(rootFolder.id, "root folder id is ok");

    let items = [{
      "id": "G_UL4ZhOyX8m",
      "type": rootFolder.BOOKMARK,
      "title": "reddit: the front page of the internet 1",
      "uri": "http://www.reddit.com",
      index: 2
    }, {
      "id": "G_UL4ZhOyX8n",
      "type": rootFolder.BOOKMARK,
      "title": "reddit: the front page of the internet 2",
      "uri": "http://www.reddit.com?1",
      index: 1
    }];
    yield rootFolder.mergeRemoteItems(items);

    let localItems = yield rootFolder.getLocalItems();
    equal(Object.keys(localItems).length, items.length, "found merged items");
  } finally {
    yield CloudSync().bookmarks.deleteRootFolder("TEST");
  }
});

add_task(function* test_merge_bookmarks_in_folders() {
  try {
    let rootFolder = yield CloudSync().bookmarks.getRootFolder("TEST");
    ok(rootFolder.id, "root folder id is ok");

    let items = [{
      "id": "G_UL4ZhOyX8m",
      "type": rootFolder.BOOKMARK,
      "title": "reddit: the front page of the internet 1",
      "uri": "http://www.reddit.com",
      index: 2
    }, {
      "id": "G_UL4ZhOyX8n",
      "type": rootFolder.BOOKMARK,
      parent: "G_UL4ZhOyX8x",
      "title": "reddit: the front page of the internet 2",
      "uri": "http://www.reddit.com/?a=å%20ä%20ö",
      index: 1
    }, {
      "id": "G_UL4ZhOyX8x",
      "type": rootFolder.FOLDER
    }];
    yield rootFolder.mergeRemoteItems(items);

    let localItems = yield rootFolder.getLocalItems();
    equal(localItems.length, items.length, "found merged items");

    localItems.forEach(function(item) {
      ok(item.id == "G_UL4ZhOyX8m" ||
         item.id == "G_UL4ZhOyX8n" ||
         item.id == "G_UL4ZhOyX8x");
      if (item.id == "G_UL4ZhOyX8n") {
        equal(item.parent, "G_UL4ZhOyX8x")
      } else {
        equal(item.parent, rootFolder.id);
      }
    });

    let folder = (yield rootFolder.getLocalItemsById(["G_UL4ZhOyX8x"]))[0];
    equal(folder.id, "G_UL4ZhOyX8x");
    equal(folder.type, rootFolder.FOLDER);

    let bookmark = (yield rootFolder.getLocalItemsById(["G_UL4ZhOyX8n"]))[0];
    equal(bookmark.id, "G_UL4ZhOyX8n");
    equal(bookmark.parent, "G_UL4ZhOyX8x");
    equal(bookmark.title, "reddit: the front page of the internet 2");
    equal(bookmark.index, 0);
    equal(bookmark.uri, "http://www.reddit.com/?a=%C3%A5%20%C3%A4%20%C3%B6");
  } finally {
    yield CloudSync().bookmarks.deleteRootFolder("TEST");
  }
});
