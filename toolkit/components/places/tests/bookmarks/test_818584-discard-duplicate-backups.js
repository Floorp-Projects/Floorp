/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Checks that automatically created bookmark backups are discarded if they are
 * duplicate of an existing ones.
 */
function run_test() {
  run_next_test();
}

add_task(function*() {
  // Create a backup for yesterday in the backups folder.
  let backupFolder = yield PlacesBackups.getBackupFolder();
  let dateObj = new Date();
  dateObj.setDate(dateObj.getDate() - 1);
  let oldBackupName = PlacesBackups.getFilenameForDate(dateObj);
  let oldBackup = OS.Path.join(backupFolder, oldBackupName);
  let {count: count, hash: hash} = yield BookmarkJSONUtils.exportToFile(oldBackup);
  do_check_true(count > 0);
  do_check_eq(hash.length, 24);
  oldBackupName = oldBackupName.replace(/\.json/, "_" + count + "_" + hash + ".json");
  yield OS.File.move(oldBackup, OS.Path.join(backupFolder, oldBackupName));

  // Create a backup.
  // This should just rename the existing backup, so in the end there should be
  // only one backup with today's date.
  yield PlacesBackups.create();

  // Get the hash of the generated backup
  let backupFiles = yield PlacesBackups.getBackupFiles();
  do_check_eq(backupFiles.length, 1);

  let matches = OS.Path.basename(backupFiles[0]).match(PlacesBackups.filenamesRegex);
  do_check_eq(matches[1], PlacesBackups.toISODateString(new Date()));
  do_check_eq(matches[2], count);
  do_check_eq(matches[3], hash);

  // Add a bookmark and create another backup.
  let bookmarkId = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.bookmarksMenuFolder,
                                                        uri("http://foo.com"),
                                                        PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                        "foo");
  // We must enforce a backup since one for today already exists.  The forced
  // backup will replace the existing one.
  yield PlacesBackups.create(undefined, true);
  do_check_eq(backupFiles.length, 1);
  let recentBackup = yield PlacesBackups.getMostRecentBackup();
  do_check_neq(recentBackup, OS.Path.join(backupFolder, oldBackupName));
  matches = OS.Path.basename(recentBackup).match(PlacesBackups.filenamesRegex);
  do_check_eq(matches[1], PlacesBackups.toISODateString(new Date()));
  do_check_eq(matches[2], count + 1);
  do_check_neq(matches[3], hash);

  // Clean up
  PlacesUtils.bookmarks.removeItem(bookmarkId);
  yield PlacesBackups.create(0);
});
