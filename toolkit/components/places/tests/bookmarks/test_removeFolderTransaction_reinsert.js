/**
 * This test ensures that reinserting a folder within a transaction gives it
 * a different GUID, and passes the GUID to the observers.
 */

add_task(function* test_removeFolderTransaction_reinsert() {
  let folder = yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "Test folder",
  });
  let folderId = yield PlacesUtils.promiseItemId(folder.guid);
  let fx = yield PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    title: "Get Firefox!",
    url: "http://getfirefox.com",
  });
  let fxId = yield PlacesUtils.promiseItemId(fx.guid);
  let tb = yield PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    title: "Get Thunderbird!",
    url: "http://getthunderbird.com",
  });
  let tbId = yield PlacesUtils.promiseItemId(tb.guid);

  let notifications = [];
  function checkNotifications(expected, message) {
    deepEqual(notifications, expected, message);
    notifications.length = 0;
  }

  let observer = {
    onItemAdded(itemId, parentId, index, type, uri, title, dateAdded, guid,
                parentGuid) {
      notifications.push(["onItemAdded", itemId, parentId, guid, parentGuid]);
    },
    onItemRemoved(itemId, parentId, index, type, uri, guid, parentGuid) {
      notifications.push(["onItemRemoved", itemId, parentId, guid, parentGuid]);
    },
  };
  PlacesUtils.bookmarks.addObserver(observer, false);
  PlacesUtils.registerShutdownFunction(function() {
    PlacesUtils.bookmarks.removeObserver(observer);
  });

  let transaction = PlacesUtils.bookmarks.getRemoveFolderTransaction(folderId);
  deepEqual(notifications, [], "We haven't executed the transaction yet");

  transaction.doTransaction();
  checkNotifications([
    ["onItemRemoved", tbId, folderId, tb.guid, folder.guid],
    ["onItemRemoved", fxId, folderId, fx.guid, folder.guid],
    ["onItemRemoved", folderId, PlacesUtils.bookmarksMenuFolderId, folder.guid,
      PlacesUtils.bookmarks.menuGuid],
  ], "Executing transaction should remove folder and its descendants");

  transaction.undoTransaction();
  // At this point, the restored folder has the same ID, but a different GUID.
  let newFolderGuid = yield PlacesUtils.promiseItemGuid(folderId);
  checkNotifications([
    ["onItemAdded", folderId, PlacesUtils.bookmarksMenuFolderId, newFolderGuid,
      PlacesUtils.bookmarks.menuGuid],
  ], "Undo should reinsert folder with same ID and different GUID");

  transaction.redoTransaction();
  checkNotifications([
    ["onItemRemoved", folderId, PlacesUtils.bookmarksMenuFolderId,
      newFolderGuid, PlacesUtils.bookmarks.menuGuid],
  ], "Redo should forward new GUID to observer");
});
