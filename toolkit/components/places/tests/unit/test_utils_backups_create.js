/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Check for correct functionality of bookmarks backups
 */

const NUMBER_OF_BACKUPS = 10;

async function createBackups(nBackups, dateObj, bookmarksBackupDir) {
  // Generate random dates.
  let dates = [];
  while (dates.length < nBackups) {
    // Use last year to ensure today's backup is the newest.
    let randomDate = new Date(
      dateObj.getFullYear() - 1,
      Math.floor(12 * Math.random()),
      Math.floor(28 * Math.random())
    );
    if (!dates.includes(randomDate.getTime())) {
      dates.push(randomDate.getTime());
    }
  }
  // Sort dates from oldest to newest.
  dates.sort();

  // Fake backups are created backwards to ensure we won't consider file
  // creation time.
  // Create fake backups for the newest dates.
  for (let i = dates.length - 1; i >= 0; i--) {
    let backupFilename = PlacesBackups.getFilenameForDate(new Date(dates[i]));
    let backupFile = bookmarksBackupDir.clone();
    backupFile.append(backupFilename);
    backupFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("0666", 8));
    info("Creating fake backup " + backupFile.leafName);
    if (!backupFile.exists()) {
      do_throw("Unable to create fake backup " + backupFile.leafName);
    }
  }

  return dates;
}

async function checkBackups(dates, bookmarksBackupDir) {
  // Check backups.  We have 11 dates but we the max number is 10 so the
  // oldest backup should have been removed.
  for (let i = 0; i < dates.length; i++) {
    let backupFilename;
    let shouldExist;
    let backupFile;
    if (i > 0) {
      let files = bookmarksBackupDir.directoryEntries;
      while (files.hasMoreElements()) {
        let entry = files.nextFile;
        if (PlacesBackups.filenamesRegex.test(entry.leafName)) {
          backupFilename = entry.leafName;
          backupFile = entry;
          break;
        }
      }
      shouldExist = true;
    } else {
      backupFilename = PlacesBackups.getFilenameForDate(new Date(dates[i]));
      backupFile = bookmarksBackupDir.clone();
      backupFile.append(backupFilename);
      shouldExist = false;
    }
    if (backupFile.exists() != shouldExist) {
      do_throw(
        "Backup should " +
          (shouldExist ? "" : "not") +
          " exist: " +
          backupFilename
      );
    }
  }
}

async function cleanupFiles(bookmarksBackupDir) {
  // Cleanup backups folder.
  // XXX: Can't use bookmarksBackupDir.remove(true) because file lock happens
  // on WIN XP.
  let files = bookmarksBackupDir.directoryEntries;
  while (files.hasMoreElements()) {
    let entry = files.nextFile;
    entry.remove(false);
  }
  // Clear cache to match the manual removing of files
  delete PlacesBackups._backupFiles;
  Assert.ok(!bookmarksBackupDir.directoryEntries.hasMoreElements());
}

add_task(async function test_create_backups() {
  let backupFolderPath = await PlacesBackups.getBackupFolder();
  let bookmarksBackupDir = new FileUtils.File(backupFolderPath);

  let dateObj = new Date();
  let dates = await createBackups(
    NUMBER_OF_BACKUPS,
    dateObj,
    bookmarksBackupDir
  );
  // Add today's backup.
  await PlacesBackups.create(NUMBER_OF_BACKUPS);
  dates.push(dateObj.getTime());
  await checkBackups(dates, bookmarksBackupDir);
  await cleanupFiles(bookmarksBackupDir);
});

add_task(async function test_saveBookmarks_with_no_backups() {
  let backupFolderPath = await PlacesBackups.getBackupFolder();
  let bookmarksBackupDir = new FileUtils.File(backupFolderPath);

  Services.prefs.setIntPref("browser.bookmarks.max_backups", 0);

  let filePath = do_get_tempdir().path + "/backup.json";
  await PlacesBackups.saveBookmarksToJSONFile(filePath);
  let files = bookmarksBackupDir.directoryEntries;
  Assert.ok(!files.hasMoreElements(), "Should have no backup files.");
  await OS.File.remove(filePath);
  // We don't need to call cleanupFiles as we are not creating any
  // backups but need to reset the cache.
  delete PlacesBackups._backupFiles;
});

add_task(async function test_saveBookmarks_with_backups() {
  let backupFolderPath = await PlacesBackups.getBackupFolder();
  let bookmarksBackupDir = new FileUtils.File(backupFolderPath);

  Services.prefs.setIntPref("browser.bookmarks.max_backups", NUMBER_OF_BACKUPS);

  let filePath = do_get_tempdir().path + "/backup.json";
  let dateObj = new Date();
  let dates = await createBackups(
    NUMBER_OF_BACKUPS,
    dateObj,
    bookmarksBackupDir
  );

  await PlacesBackups.saveBookmarksToJSONFile(filePath);
  dates.push(dateObj.getTime());
  await checkBackups(dates, bookmarksBackupDir);
  await OS.File.remove(filePath);
  await cleanupFiles(bookmarksBackupDir);
});
