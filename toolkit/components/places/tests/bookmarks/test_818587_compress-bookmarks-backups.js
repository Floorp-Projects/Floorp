/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function compress_bookmark_backups_test() {
  // Check for jsonlz4 extension
  let todayFilename = PlacesBackups.getFilenameForDate(new Date(2014, 4, 15), true);
  do_check_eq(todayFilename, "bookmarks-2014-05-15.jsonlz4");

  await PlacesBackups.create();

  // Check that a backup for today has been created and the regex works fine for lz4.
  do_check_eq((await PlacesBackups.getBackupFiles()).length, 1);
  let mostRecentBackupFile = await PlacesBackups.getMostRecentBackup();
  do_check_neq(mostRecentBackupFile, null);
  do_check_true(PlacesBackups.filenamesRegex.test(OS.Path.basename(mostRecentBackupFile)));

  // The most recent backup file has to be removed since saveBookmarksToJSONFile
  // will otherwise over-write the current backup, since it will be made on the
  // same date
  await OS.File.remove(mostRecentBackupFile);
  do_check_false((await OS.File.exists(mostRecentBackupFile)));

  // Check that, if the user created a custom backup out of the default
  // backups folder, it gets copied (compressed) into it.
  let jsonFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.json");
  await PlacesBackups.saveBookmarksToJSONFile(jsonFile);
  do_check_eq((await PlacesBackups.getBackupFiles()).length, 1);

  // Check if import works from lz4 compressed json
  let uri = NetUtil.newURI("http://www.mozilla.org/en-US/");
  let bm  = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                 uri,
                                                 PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                 "bookmark");

  // Force create a compressed backup, Remove the bookmark, the restore the backup
  await PlacesBackups.create(undefined, true);
  let recentBackup = await PlacesBackups.getMostRecentBackup();
  PlacesUtils.bookmarks.removeItem(bm);
  await BookmarkJSONUtils.importFromFile(recentBackup, true);
  let root = PlacesUtils.getFolderContents(PlacesUtils.unfiledBookmarksFolderId).root;
  let node = root.getChild(0);
  do_check_eq(node.uri, uri.spec);

  root.containerOpen = false;
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);

  // Cleanup.
  await OS.File.remove(jsonFile);
});
