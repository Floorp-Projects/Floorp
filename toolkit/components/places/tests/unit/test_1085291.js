add_task(async function() {
  // test that nodes inserted by incremental update for bookmarks of all types
  // have the extra bookmark properties (bookmarkGuid, dateAdded, lastModified).

  // getFolderContents opens the root node.
  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.toolbarGuid)
    .root;

  async function insertAndTest(bmInfo) {
    bmInfo = await PlacesUtils.bookmarks.insert(bmInfo);
    let node = root.getChild(root.childCount - 1);
    Assert.equal(node.bookmarkGuid, bmInfo.guid);
    Assert.equal(node.dateAdded, bmInfo.dateAdded * 1000);
    Assert.equal(node.lastModified, bmInfo.lastModified * 1000);
  }

  // Normal bookmark.
  await insertAndTest({
    parentGuid: root.bookmarkGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    title: "Test Bookmark",
    url: "http://test.url.tld",
  });

  // place: query
  await insertAndTest({
    parentGuid: root.bookmarkGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    title: "Test Query",
    url: `place:parent=${PlacesUtils.bookmarks.menuGuid}`,
  });

  // folder
  await insertAndTest({
    parentGuid: root.bookmarkGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "Test Folder",
  });

  // separator
  await insertAndTest({
    parentGuid: root.bookmarkGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
  });

  root.containerOpen = false;
});
