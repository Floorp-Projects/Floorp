/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Checks that backups properly include all of the bookmarks if the hierarchy
 * in the database is unordered so that a hierarchy is defined before its
 * ancestor in the bookmarks table.
 */
add_task(async function() {
  let bm = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                NetUtil.newURI("http://mozilla.org/"),
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "bookmark");
  let f2 = PlacesUtils.bookmarks.createFolder(PlacesUtils.unfiledBookmarksFolderId, "f2",
                                              PlacesUtils.bookmarks.DEFAULT_INDEX);
  PlacesUtils.bookmarks.moveItem(bm, f2, PlacesUtils.bookmarks.DEFAULT_INDEX);
  let f1 = PlacesUtils.bookmarks.createFolder(PlacesUtils.unfiledBookmarksFolderId, "f1",
                                              PlacesUtils.bookmarks.DEFAULT_INDEX);
  PlacesUtils.bookmarks.moveItem(f2, f1, PlacesUtils.bookmarks.DEFAULT_INDEX);

  // Create a backup.
  await PlacesBackups.create();

  // Remove the bookmarks, then restore the backup.
  PlacesUtils.bookmarks.removeItem(f1);
  await BookmarkJSONUtils.importFromFile((await PlacesBackups.getMostRecentBackup()), true);

  do_print("Checking first level");
  let root = PlacesUtils.getFolderContents(PlacesUtils.unfiledBookmarksFolderId).root;
  let level1 = root.getChild(0);
  do_check_eq(level1.title, "f1");
  do_print("Checking second level");
  PlacesUtils.asContainer(level1).containerOpen = true;
  let level2 = level1.getChild(0);
  do_check_eq(level2.title, "f2");
  do_print("Checking bookmark");
  PlacesUtils.asContainer(level2).containerOpen = true;
  let bookmark = level2.getChild(0);
  do_check_eq(bookmark.title, "bookmark");
  level2.containerOpen = false;
  level1.containerOpen = false;
  root.containerOpen = false;
});
