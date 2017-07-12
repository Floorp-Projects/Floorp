// Test that result node for folder shortcuts get the target folder title if
// the shortcut itself has no title set.
add_task(async function() {
  let shortcutInfo = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "place:folder=TOOLBAR"
  });

  let unfiledRoot =
    PlacesUtils.getFolderContents(PlacesUtils.unfiledBookmarksFolderId).root;
  let shortcutNode = unfiledRoot.getChild(unfiledRoot.childCount - 1);
  Assert.equal(shortcutNode.bookmarkGuid, shortcutInfo.guid);

  let toolbarInfo =
    await PlacesUtils.bookmarks.fetch(PlacesUtils.bookmarks.toolbarGuid);
  Assert.equal(shortcutNode.title, toolbarInfo.title);

  unfiledRoot.containerOpen = false;
});
