/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  do_test_pending();

  (async function() {
    let backupFolder = await PlacesBackups.getBackupFolder();
    let bookmarksBackupDir = new FileUtils.File(backupFolder);
    // Remove all files from backups folder.
    let files = bookmarksBackupDir.directoryEntries;
    while (files.hasMoreElements()) {
      let entry = files.getNext().QueryInterface(Ci.nsIFile);
      entry.remove(false);
    }

    // Create a json dummy backup in the future.
    let dateObj = new Date();
    dateObj.setYear(dateObj.getFullYear() + 1);
    let name = PlacesBackups.getFilenameForDate(dateObj);
    Assert.equal(name, "bookmarks-" + PlacesBackups.toISODateString(dateObj) + ".json");
    files = bookmarksBackupDir.directoryEntries;
    while (files.hasMoreElements()) {
      let entry = files.getNext().QueryInterface(Ci.nsIFile);
      if (PlacesBackups.filenamesRegex.test(entry.leafName))
        entry.remove(false);
    }

    let futureBackupFile = bookmarksBackupDir.clone();
    futureBackupFile.append(name);
    futureBackupFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);
    Assert.ok(futureBackupFile.exists());

    Assert.equal((await PlacesBackups.getBackupFiles()).length, 0);

    await PlacesBackups.create();
    // Check that a backup for today has been created.
    Assert.equal((await PlacesBackups.getBackupFiles()).length, 1);
    let mostRecentBackupFile = await PlacesBackups.getMostRecentBackup();
    Assert.notEqual(mostRecentBackupFile, null);
    Assert.ok(PlacesBackups.filenamesRegex.test(OS.Path.basename(mostRecentBackupFile)));

    // Check that future backup has been removed.
    Assert.ok(!futureBackupFile.exists());

    // Cleanup.
    mostRecentBackupFile = new FileUtils.File(mostRecentBackupFile);
    mostRecentBackupFile.remove(false);
    Assert.ok(!mostRecentBackupFile.exists());

    do_test_finished();
  })();
}
