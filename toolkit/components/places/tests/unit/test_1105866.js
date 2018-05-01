add_task(async function test_folder_shortcuts() {
  let shortcutInfo = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: `place:parent=${PlacesUtils.bookmarks.toolbarGuid}`
  });

  let unfiledRoot =
    PlacesUtils.getFolderContents(PlacesUtils.bookmarks.unfiledGuid).root;
  let shortcutNode = unfiledRoot.getChild(unfiledRoot.childCount - 1);
  Assert.strictEqual(shortcutNode.itemId,
                     await PlacesUtils.promiseItemId(shortcutInfo.guid));
  Assert.strictEqual(PlacesUtils.asQuery(shortcutNode).folderItemId,
                     PlacesUtils.toolbarFolderId);
  Assert.strictEqual(shortcutNode.bookmarkGuid, shortcutInfo.guid);
  Assert.strictEqual(PlacesUtils.asQuery(shortcutNode).targetFolderGuid,
                     PlacesUtils.bookmarks.toolbarGuid);

  // test that a node added incrementally also behaves just as well.
  shortcutInfo = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: `place:parent=${PlacesUtils.bookmarks.menuGuid}`
  });
  shortcutNode = unfiledRoot.getChild(unfiledRoot.childCount - 1);
  Assert.strictEqual(shortcutNode.itemId,
                     await PlacesUtils.promiseItemId(shortcutInfo.guid));
  Assert.strictEqual(PlacesUtils.asQuery(shortcutNode).folderItemId,
                     PlacesUtils.bookmarksMenuFolderId);
  Assert.strictEqual(shortcutNode.bookmarkGuid, shortcutInfo.guid);
  Assert.strictEqual(PlacesUtils.asQuery(shortcutNode).targetFolderGuid,
                     PlacesUtils.bookmarks.menuGuid);

  unfiledRoot.containerOpen = false;
});

add_task(async function test_plain_folder() {
  let folderInfo = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid
  });

  let unfiledRoot =
    PlacesUtils.getFolderContents(PlacesUtils.bookmarks.unfiledGuid).root;
  let lastChild = unfiledRoot.getChild(unfiledRoot.childCount - 1);
  Assert.strictEqual(lastChild.bookmarkGuid, folderInfo.guid);
  Assert.strictEqual(PlacesUtils.asQuery(lastChild).targetFolderGuid,
                     folderInfo.guid);
});

add_task(async function test_non_item_query() {
  let options = PlacesUtils.history.getNewQueryOptions();
  let root = PlacesUtils.history.executeQuery(
    PlacesUtils.history.getNewQuery(), options).root;
  Assert.strictEqual(root.itemId, -1);
  Assert.strictEqual(PlacesUtils.asQuery(root).folderItemId, -1);
  Assert.strictEqual(root.bookmarkGuid, "");
  Assert.strictEqual(PlacesUtils.asQuery(root).targetFolderGuid, "");
});
