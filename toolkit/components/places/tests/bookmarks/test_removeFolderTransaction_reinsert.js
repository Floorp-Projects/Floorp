/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/**
 * This test ensures that reinserting a folder within a transaction gives it
 * the same GUID, and passes it to the observers.
 */

add_task(async function test_removeFolderTransaction_reinsert() {
  let folder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "Test folder",
  });
  let fx = await PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    title: "Get Firefox!",
    url: "http://getfirefox.com",
  });
  let tb = await PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    title: "Get Thunderbird!",
    url: "http://getthunderbird.com",
  });

  let notifications = [];
  function checkNotifications(expected, message) {
    deepEqual(notifications, expected, message);
    notifications.length = 0;
  }

  let observer = {
    __proto__: NavBookmarkObserver.prototype,
    onItemAdded(itemId, parentId, index, type, uri, title, dateAdded, guid,
                parentGuid) {
      notifications.push(["onItemAdded", itemId, parentId, guid, parentGuid]);
    },
    onItemRemoved(itemId, parentId, index, type, uri, guid, parentGuid) {
      notifications.push(["onItemRemoved", itemId, parentId, guid, parentGuid]);
    },
  };
  PlacesUtils.bookmarks.addObserver(observer);
  PlacesUtils.registerShutdownFunction(function() {
    PlacesUtils.bookmarks.removeObserver(observer);
  });

  let transaction = PlacesTransactions.Remove({guid: folder.guid});

  let folderId = await PlacesUtils.promiseItemId(folder.guid);
  let fxId = await PlacesUtils.promiseItemId(fx.guid);
  let tbId = await PlacesUtils.promiseItemId(tb.guid);

  await transaction.transact();

  checkNotifications([
    ["onItemRemoved", tbId, folderId, tb.guid, folder.guid],
    ["onItemRemoved", fxId, folderId, fx.guid, folder.guid],
    ["onItemRemoved", folderId, PlacesUtils.bookmarksMenuFolderId, folder.guid,
      PlacesUtils.bookmarks.menuGuid],
  ], "Executing transaction should remove folder and its descendants");

  await PlacesTransactions.undo();

  folderId = await PlacesUtils.promiseItemId(folder.guid);
  fxId = await PlacesUtils.promiseItemId(fx.guid);
  tbId = await PlacesUtils.promiseItemId(tb.guid);

  checkNotifications([
    ["onItemAdded", folderId, PlacesUtils.bookmarksMenuFolderId, folder.guid,
      PlacesUtils.bookmarks.menuGuid],
    ["onItemAdded", fxId, folderId, fx.guid, folder.guid],
    ["onItemAdded", tbId, folderId, tb.guid, folder.guid],
  ], "Undo should reinsert folder with different id but same GUID");

  await PlacesTransactions.redo();

  checkNotifications([
    ["onItemRemoved", tbId, folderId, tb.guid, folder.guid],
    ["onItemRemoved", fxId, folderId, fx.guid, folder.guid],
    ["onItemRemoved", folderId, PlacesUtils.bookmarksMenuFolderId, folder.guid,
      PlacesUtils.bookmarks.menuGuid],
  ], "Redo should pass the GUID to observer");
});
