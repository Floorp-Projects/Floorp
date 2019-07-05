// Test that result node for folder shortcuts get the target folder title if
// the shortcut itself has no title set.
add_task(async function() {
  let folder = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "fake",
  });

  let shortcutInfo = await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: `place:parent=${folder.guid}`,
  });

  let unfiledRoot = PlacesUtils.getFolderContents(
    PlacesUtils.bookmarks.unfiledGuid
  ).root;
  let shortcutNode = unfiledRoot.getChild(unfiledRoot.childCount - 1);
  Assert.equal(shortcutNode.bookmarkGuid, shortcutInfo.guid);

  Assert.equal(shortcutNode.title, folder.title);

  unfiledRoot.containerOpen = false;
});
