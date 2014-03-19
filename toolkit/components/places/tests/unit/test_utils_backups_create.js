/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
  * Check for correct functionality of bookmarks backups
  */

const NUMBER_OF_BACKUPS = 10;

function run_test() {
  run_next_test();
}

add_task(function () {
  // Generate random dates.
  let dateObj = new Date();
  let dates = [];
  while (dates.length < NUMBER_OF_BACKUPS) {
    // Use last year to ensure today's backup is the newest.
    let randomDate = new Date(dateObj.getFullYear() - 1,
                              Math.floor(12 * Math.random()),
                              Math.floor(28 * Math.random()));
    let dateString = randomDate.toLocaleFormat("%Y-%m-%d");
    if (dates.indexOf(dateString) == -1)
      dates.push(dateString);
  }
  // Sort dates from oldest to newest.
  dates.sort();

  // Get and cleanup the backups folder.
  let backupFolderPath = yield PlacesBackups.getBackupFolder();
  let bookmarksBackupDir = new FileUtils.File(backupFolderPath);

  // Fake backups are created backwards to ensure we won't consider file
  // creation time.
  // Create fake backups for the newest dates.
  for (let i = dates.length - 1; i >= 0; i--) {
    let backupFilename = PlacesBackups.getFilenameForDate(new Date(dates[i]));
    let backupFile = bookmarksBackupDir.clone();
    backupFile.append(backupFilename);
    backupFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0666", 8));
    do_log_info("Creating fake backup " + backupFile.leafName);
    if (!backupFile.exists())
      do_throw("Unable to create fake backup " + backupFile.leafName);
  }

  yield PlacesBackups.create(NUMBER_OF_BACKUPS);
  // Add today's backup.
  dates.push(dateObj.toLocaleFormat("%Y-%m-%d"));

  // Check backups.  We have 11 dates but we the max number is 10 so the
  // oldest backup should have been removed.
  for (let i = 0; i < dates.length; i++) {
    let backupFilename;
    let shouldExist;
    let backupFile;
    if (i > 0) {
      let files = bookmarksBackupDir.directoryEntries;
      while (files.hasMoreElements()) {
        let entry = files.getNext().QueryInterface(Ci.nsIFile);
        if (PlacesBackups.filenamesRegex.test(entry.leafName)) {
          backupFilename = entry.leafName;
          backupFile = entry;
          break;
        }
      }
      shouldExist = true;
    }
    else {
      backupFilename = PlacesBackups.getFilenameForDate(new Date(dates[i]));
      backupFile = bookmarksBackupDir.clone();
      backupFile.append(backupFilename);
      shouldExist = false;
    }
    if (backupFile.exists() != shouldExist)
      do_throw("Backup should " + (shouldExist ? "" : "not") + " exist: " + backupFilename);
  }

  // Cleanup backups folder.
  // XXX: Can't use bookmarksBackupDir.remove(true) because file lock happens
  // on WIN XP.
  let files = bookmarksBackupDir.directoryEntries;
  while (files.hasMoreElements()) {
    let entry = files.getNext().QueryInterface(Ci.nsIFile);
    entry.remove(false);
  }
  do_check_false(bookmarksBackupDir.directoryEntries.hasMoreElements());
});
