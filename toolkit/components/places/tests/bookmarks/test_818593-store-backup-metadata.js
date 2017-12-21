/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * To confirm that metadata i.e. bookmark count is set and retrieved for
 * automatic backups.
 */
add_task(async function test_saveBookmarksToJSONFile_and_create() {
  // Add a bookmark
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Get Firefox!",
    url: "http://getfirefox.com/"
  });

  // Test saveBookmarksToJSONFile()
  let backupFile = FileUtils.getFile("TmpD", ["bookmarks.json"]);
  backupFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0600", 8));

  let nodeCount = await PlacesBackups.saveBookmarksToJSONFile(backupFile, true);
  Assert.ok(nodeCount > 0);
  Assert.ok(backupFile.exists());
  Assert.equal(backupFile.leafName, "bookmarks.json");

  // Ensure the backup would be copied to our backups folder when the original
  // backup is saved somewhere else.
  let recentBackup = await PlacesBackups.getMostRecentBackup();
  let matches = OS.Path.basename(recentBackup).match(PlacesBackups.filenamesRegex);
  Assert.equal(matches[2], nodeCount);
  Assert.equal(matches[3].length, 24);

  // Clear all backups in our backups folder.
  await PlacesBackups.create(0);
  Assert.equal((await PlacesBackups.getBackupFiles()).length, 0);

  // Test create() which saves bookmarks with metadata on the filename.
  await PlacesBackups.create();
  Assert.equal((await PlacesBackups.getBackupFiles()).length, 1);

  let mostRecentBackupFile = await PlacesBackups.getMostRecentBackup();
  Assert.notEqual(mostRecentBackupFile, null);
  matches = OS.Path.basename(recentBackup).match(PlacesBackups.filenamesRegex);
  Assert.equal(matches[2], nodeCount);
  Assert.equal(matches[3].length, 24);

  // Cleanup
  backupFile.remove(false);
  await PlacesBackups.create(0);
  await PlacesUtils.bookmarks.remove(bookmark);
});
