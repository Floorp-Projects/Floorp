/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Checks that automatically created bookmark backups are discarded if they are
 * duplicate of an existing ones.
 */
add_task(async function() {
  // Create a backup for yesterday in the backups folder.
  let backupFolder = await PlacesBackups.getBackupFolder();
  let dateObj = new Date();
  dateObj.setDate(dateObj.getDate() - 1);
  let oldBackupName = PlacesBackups.getFilenameForDate(dateObj);
  let oldBackup = OS.Path.join(backupFolder, oldBackupName);
  let { count: count, hash: hash } = await BookmarkJSONUtils.exportToFile(
    oldBackup
  );
  Assert.ok(count > 0);
  Assert.equal(hash.length, 24);
  oldBackupName = oldBackupName.replace(
    /\.json/,
    "_" + count + "_" + hash + ".json"
  );
  await OS.File.move(oldBackup, OS.Path.join(backupFolder, oldBackupName));

  // Create a backup.
  // This should just rename the existing backup, so in the end there should be
  // only one backup with today's date.
  await PlacesBackups.create();

  // Get the hash of the generated backup
  let backupFiles = await PlacesBackups.getBackupFiles();
  Assert.equal(backupFiles.length, 1);

  let matches = OS.Path.basename(backupFiles[0]).match(
    PlacesBackups.filenamesRegex
  );
  Assert.equal(matches[1], PlacesBackups.toISODateString(new Date()));
  Assert.equal(matches[2], count);
  Assert.equal(matches[3], hash);

  // Add a bookmark and create another backup.
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "foo",
    url: "http://foo.com",
  });

  // We must enforce a backup since one for today already exists.  The forced
  // backup will replace the existing one.
  await PlacesBackups.create(undefined, true);
  Assert.equal(backupFiles.length, 1);
  let recentBackup = await PlacesBackups.getMostRecentBackup();
  Assert.notEqual(recentBackup, OS.Path.join(backupFolder, oldBackupName));
  matches = OS.Path.basename(recentBackup).match(PlacesBackups.filenamesRegex);
  Assert.equal(matches[1], PlacesBackups.toISODateString(new Date()));
  Assert.equal(matches[2], count + 1);
  Assert.notEqual(matches[3], hash);

  // Clean up
  await PlacesUtils.bookmarks.remove(bookmark);
  await PlacesBackups.create(0);
});
