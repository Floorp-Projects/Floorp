add_task(function* test_folder_shortcuts() {
  let shortcutInfo = yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "place:folder=TOOLBAR"
  });

  let unfiledRoot =
    PlacesUtils.getFolderContents(PlacesUtils.unfiledBookmarksFolderId).root;
  let shortcutNode = unfiledRoot.getChild(unfiledRoot.childCount - 1);
  Assert.strictEqual(shortcutNode.itemId,
                     yield PlacesUtils.promiseItemId(shortcutInfo.guid));
  Assert.strictEqual(PlacesUtils.asQuery(shortcutNode).folderItemId,
                     PlacesUtils.toolbarFolderId);
  Assert.strictEqual(shortcutNode.bookmarkGuid, shortcutInfo.guid);
  Assert.strictEqual(PlacesUtils.asQuery(shortcutNode).targetFolderGuid,
                     PlacesUtils.bookmarks.toolbarGuid);

  // test that a node added incrementally also behaves just as well.
  shortcutInfo = yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "place:folder=BOOKMARKS_MENU"
  });
  shortcutNode = unfiledRoot.getChild(unfiledRoot.childCount - 1);
  Assert.strictEqual(shortcutNode.itemId,
                     yield PlacesUtils.promiseItemId(shortcutInfo.guid));
  Assert.strictEqual(PlacesUtils.asQuery(shortcutNode).folderItemId,
                     PlacesUtils.bookmarksMenuFolderId);
  Assert.strictEqual(shortcutNode.bookmarkGuid, shortcutInfo.guid);
  Assert.strictEqual(PlacesUtils.asQuery(shortcutNode).targetFolderGuid,
                     PlacesUtils.bookmarks.menuGuid);

  unfiledRoot.containerOpen = false;
});

add_task(function* test_plain_folder() {
  let folderInfo = yield PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid
  });

  let unfiledRoot =
    PlacesUtils.getFolderContents(PlacesUtils.unfiledBookmarksFolderId).root;
  let lastChild = unfiledRoot.getChild(unfiledRoot.childCount - 1);
  Assert.strictEqual(lastChild.bookmarkGuid, folderInfo.guid);
  Assert.strictEqual(PlacesUtils.asQuery(lastChild).targetFolderGuid,
                     folderInfo.guid);
});

add_task(function* test_non_item_query() {
  let options = PlacesUtils.history.getNewQueryOptions();
  let root = PlacesUtils.history.executeQuery(
    PlacesUtils.history.getNewQuery(), options).root;
  Assert.strictEqual(root.itemId, -1);
  Assert.strictEqual(PlacesUtils.asQuery(root).folderItemId, -1);
  Assert.strictEqual(root.bookmarkGuid, "");
  Assert.strictEqual(PlacesUtils.asQuery(root).targetFolderGuid, "");
});

function run_test() {
  run_next_test();
}
