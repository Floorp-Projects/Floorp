/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
  * Check for correct functionality of PlacesUtils.archiveBookmarksFile
  */

const PREFIX = "bookmarks-";
// The localized prefix must be "bigger" and associated to older backups.
const LOCALIZED_PREFIX = "segnalibri-";
const SUFFIX = ".json";
const NUMBER_OF_BACKUPS = 10;

function run_test() {
  do_test_pending();

  // Generate random dates.
  var dateObj = new Date();
  var dates = [];
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

  Task.spawn(function() {
    // Get and cleanup the backups folder.
    let backupFolderPath = yield PlacesBackups.getBackupFolder();
    let bookmarksBackupDir = new FileUtils.File(backupFolderPath);

    // Fake backups are created backwards to ensure we won't consider file
    // creation time.
    // Create fake backups for the newest dates.
    for (let i = dates.length - 1; i >= 0; i--) {
      let backupFilename;
      if (i > Math.floor(dates.length/2))
        backupFilename = PREFIX + dates[i] + SUFFIX;
      else
        backupFilename = LOCALIZED_PREFIX + dates[i] + SUFFIX;
      let backupFile = bookmarksBackupDir.clone();
      backupFile.append(backupFilename);
      backupFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
      do_check_true(backupFile.exists());
    }

    // Replace PlacesUtils getFormattedString so that it will return the localized
    // string we want.
    PlacesUtils.getFormattedString = function (aKey, aValue) {
      return LOCALIZED_PREFIX + aValue;
    }

    yield PlacesBackups.create(Math.floor(dates.length/2));
    // Add today's backup.
    dates.push(dateObj.toLocaleFormat("%Y-%m-%d"));

    // Check backups.
    for (var i = 0; i < dates.length; i++) {
      let backupFilename;
      let shouldExist;
      let backupFile;
      if (i > Math.floor(dates.length/2)) {
        let files = bookmarksBackupDir.directoryEntries;
        let rx = new RegExp("^" + PREFIX + dates[i] + "(_[0-9]+){0,1}" + SUFFIX + "$");
        while (files.hasMoreElements()) {
          let entry = files.getNext().QueryInterface(Ci.nsIFile);
          if (entry.leafName.match(rx)) {
            backupFilename = entry.leafName;
            backupFile = entry;
            break;
          }
        }
        shouldExist = true;
      }
      else {
        backupFilename = LOCALIZED_PREFIX + dates[i] + SUFFIX;
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

    do_test_finished();
  });
}
