/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  run_next_test();
}

add_task(function* test_execute() {
  PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.unfiledBookmarksFolderId, NetUtil.newURI("http://1.moz.org/"),
    PlacesUtils.bookmarks.DEFAULT_INDEX, "Bookmark 1"
  );
  let id1 = PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.unfiledBookmarksFolderId, NetUtil.newURI("place:folder=1234"),
    PlacesUtils.bookmarks.DEFAULT_INDEX, "Shortcut 1"
  );
  let id2 = PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.unfiledBookmarksFolderId, NetUtil.newURI("place:folder=-1"),
    PlacesUtils.bookmarks.DEFAULT_INDEX, "Shortcut 2"
  );
  PlacesUtils.bookmarks.insertBookmark(
    PlacesUtils.unfiledBookmarksFolderId, NetUtil.newURI("http://2.moz.org/"),
    PlacesUtils.bookmarks.DEFAULT_INDEX, "Bookmark 2"
  );

  // Add also a simple visit.
  yield PlacesTestUtils.addVisits(uri(("http://3.moz.org/")));

  // Query containing a broken folder shortcuts among results.
  let query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.unfiledBookmarksFolderId], 1);
  let options = PlacesUtils.history.getNewQueryOptions();
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  do_check_eq(root.childCount, 4);

  let shortcut = root.getChild(1);
  do_check_eq(shortcut.uri, "place:folder=1234");
  PlacesUtils.asContainer(shortcut);
  shortcut.containerOpen = true;
  do_check_eq(shortcut.childCount, 0);
  shortcut.containerOpen = false;
  // Remove the broken shortcut while the containing result is open.
  PlacesUtils.bookmarks.removeItem(id1);
  do_check_eq(root.childCount, 3);

  shortcut = root.getChild(1);
  do_check_eq(shortcut.uri, "place:folder=-1");
  PlacesUtils.asContainer(shortcut);
  shortcut.containerOpen = true;
  do_check_eq(shortcut.childCount, 0);
  shortcut.containerOpen = false;
  // Remove the broken shortcut while the containing result is open.
  PlacesUtils.bookmarks.removeItem(id2);
  do_check_eq(root.childCount, 2);

  root.containerOpen = false;

  // Broken folder shortcut as root node.
  query = PlacesUtils.history.getNewQuery();
  query.setFolders([1234], 1);
  options = PlacesUtils.history.getNewQueryOptions();
  root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 0);
  root.containerOpen = false;

  // Broken folder shortcut as root node with folder=-1.
  query = PlacesUtils.history.getNewQuery();
  query.setFolders([-1], 1);
  options = PlacesUtils.history.getNewQueryOptions();
  root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 0);
  root.containerOpen = false;
});
