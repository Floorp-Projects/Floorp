/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * To confirm that metadata i.e. bookmark count is set and retrieved for
 * automatic backups.
 */
function run_test() {
  run_next_test();
}

add_task(function* test_saveBookmarksToJSONFile_and_create()
{
  // Add a bookmark
  let uri = NetUtil.newURI("http://getfirefox.com/");
  let bookmarkId =
    PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.unfiledBookmarksFolderId, uri,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Get Firefox!");

  // Test saveBookmarksToJSONFile()
  let backupFile = FileUtils.getFile("TmpD", ["bookmarks.json"]);
  backupFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, parseInt("0600", 8));

  let nodeCount = yield PlacesBackups.saveBookmarksToJSONFile(backupFile, true);
  do_check_true(nodeCount > 0);
  do_check_true(backupFile.exists());
  do_check_eq(backupFile.leafName, "bookmarks.json");

  // Ensure the backup would be copied to our backups folder when the original
  // backup is saved somewhere else.
  let recentBackup = yield PlacesBackups.getMostRecentBackup();
  let matches = OS.Path.basename(recentBackup).match(PlacesBackups.filenamesRegex);
  do_check_eq(matches[2], nodeCount);
  do_check_eq(matches[3].length, 24);

  // Clear all backups in our backups folder.
  yield PlacesBackups.create(0);
  do_check_eq((yield PlacesBackups.getBackupFiles()).length, 0);

  // Test create() which saves bookmarks with metadata on the filename.
  yield PlacesBackups.create();
  do_check_eq((yield PlacesBackups.getBackupFiles()).length, 1);

  let mostRecentBackupFile = yield PlacesBackups.getMostRecentBackup();
  do_check_neq(mostRecentBackupFile, null);
  matches = OS.Path.basename(recentBackup).match(PlacesBackups.filenamesRegex);
  do_check_eq(matches[2], nodeCount);
  do_check_eq(matches[3].length, 24);

  // Cleanup
  backupFile.remove(false);
  yield PlacesBackups.create(0);
  PlacesUtils.bookmarks.removeItem(bookmarkId);
});
