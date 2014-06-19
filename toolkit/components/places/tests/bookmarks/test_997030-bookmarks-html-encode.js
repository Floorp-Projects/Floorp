/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Checks that we don't encodeURI twice when creating bookmarks.html.
 */
 
function run_test() {
  run_next_test();
}

add_task(function() {
  let uri = NetUtil.newURI("http://bt.ktxp.com/search.php?keyword=%E5%A6%84%E6%83%B3%E5%AD%A6%E7%94%9F%E4%BC%9A");
  let bm = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                uri,
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "bookmark");

  let file =  OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.exported.997030.html");
  if ((yield OS.File.exists(file))) {
    yield OS.File.remove(file);
  }
  yield BookmarkHTMLUtils.exportToFile(file);

  // Remove the bookmarks, then restore the backup.
  PlacesUtils.bookmarks.removeItem(bm);
  yield BookmarkHTMLUtils.importFromFile(file, true);

  do_log_info("Checking first level");
  let root = PlacesUtils.getFolderContents(PlacesUtils.unfiledBookmarksFolderId).root;
  let node = root.getChild(0);
  do_check_eq(node.uri, uri.spec);

  root.containerOpen = false;
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
});
