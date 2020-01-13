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

  let listener = events => {
    for (let event of events) {
      notifications.push([
        event.type,
        event.id,
        event.parentId,
        event.guid,
        event.parentGuid,
      ]);
    }
  };
  let observer = {
    __proto__: NavBookmarkObserver.prototype,
  };
  PlacesUtils.bookmarks.addObserver(observer);
  PlacesUtils.observers.addListener(
    ["bookmark-added", "bookmark-removed"],
    listener
  );
  PlacesUtils.registerShutdownFunction(function() {
    PlacesUtils.bookmarks.removeObserver(observer);
    PlacesUtils.observers.removeListener(
      ["bookmark-added", "bookmark-removed"],
      listener
    );
  });

  let transaction = PlacesTransactions.Remove({ guid: folder.guid });

  let folderId = await PlacesUtils.promiseItemId(folder.guid);
  let fxId = await PlacesUtils.promiseItemId(fx.guid);
  let tbId = await PlacesUtils.promiseItemId(tb.guid);

  await transaction.transact();

  checkNotifications(
    [
      ["bookmark-removed", tbId, folderId, tb.guid, folder.guid],
      ["bookmark-removed", fxId, folderId, fx.guid, folder.guid],
      [
        "bookmark-removed",
        folderId,
        PlacesUtils.bookmarksMenuFolderId,
        folder.guid,
        PlacesUtils.bookmarks.menuGuid,
      ],
    ],
    "Executing transaction should remove folder and its descendants"
  );

  await PlacesTransactions.undo();

  folderId = await PlacesUtils.promiseItemId(folder.guid);
  fxId = await PlacesUtils.promiseItemId(fx.guid);
  tbId = await PlacesUtils.promiseItemId(tb.guid);

  checkNotifications(
    [
      [
        "bookmark-added",
        folderId,
        PlacesUtils.bookmarksMenuFolderId,
        folder.guid,
        PlacesUtils.bookmarks.menuGuid,
      ],
      ["bookmark-added", fxId, folderId, fx.guid, folder.guid],
      ["bookmark-added", tbId, folderId, tb.guid, folder.guid],
    ],
    "Undo should reinsert folder with different id but same GUID"
  );

  await PlacesTransactions.redo();

  checkNotifications(
    [
      ["bookmark-removed", tbId, folderId, tb.guid, folder.guid],
      ["bookmark-removed", fxId, folderId, fx.guid, folder.guid],
      [
        "bookmark-removed",
        folderId,
        PlacesUtils.bookmarksMenuFolderId,
        folder.guid,
        PlacesUtils.bookmarks.menuGuid,
      ],
    ],
    "Redo should pass the GUID to observer"
  );
});
