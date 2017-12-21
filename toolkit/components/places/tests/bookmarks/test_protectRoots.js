/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  const ROOTS = [
    PlacesUtils.bookmarksMenuFolderId,
    PlacesUtils.toolbarFolderId,
    PlacesUtils.unfiledBookmarksFolderId,
    PlacesUtils.tagsFolderId,
    PlacesUtils.placesRootId,
    PlacesUtils.mobileFolderId,
  ];

  for (let root of ROOTS) {
    Assert.ok(PlacesUtils.isRootItem(root));

    try {
      PlacesUtils.bookmarks.removeItem(root);
      do_throw("Trying to remove a root should throw");
    } catch (ex) {}

    try {
      PlacesUtils.bookmarks.moveItem(root, PlacesUtils.placesRootId, 0);
      do_throw("Trying to move a root should throw");
    } catch (ex) {}

    try {
      PlacesUtils.bookmarks.removeFolderChildren(root);
      if (root == PlacesUtils.placesRootId)
        do_throw("Trying to remove children of the main root should throw");
    } catch (ex) {
      if (root != PlacesUtils.placesRootId)
        do_throw("Trying to remove children of other roots should not throw");
    }
  }
}
