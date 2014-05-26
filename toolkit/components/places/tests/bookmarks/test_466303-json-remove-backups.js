/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Since PlacesBackups.getbackupFiles() is a lazy getter, these tests must
// run in the given order, to avoid making it out-of-sync.

add_task(function check_max_backups_is_respected() {
  // Get bookmarkBackups directory
  let backupFolder = yield PlacesBackups.getBackupFolder();

  // Create 2 json dummy backups in the past.
  let oldJsonPath = OS.Path.join(backupFolder, "bookmarks-2008-01-01.json");
  let oldJsonFile = yield OS.File.open(oldJsonPath, { truncate: true });
  oldJsonFile.close();
  do_check_true(yield OS.File.exists(oldJsonPath));

  let jsonPath = OS.Path.join(backupFolder, "bookmarks-2008-01-31.json");
  let jsonFile = yield OS.File.open(jsonPath, { truncate: true });
  jsonFile.close();
  do_check_true(yield OS.File.exists(jsonPath));

  // Export bookmarks to JSON.
  // Allow 2 backups, the older one should be removed.
  yield PlacesBackups.create(2);
  let backupFilename = PlacesBackups.getFilenameForDate();

  let count = 0;
  let lastBackupPath = null;
  let iterator = new OS.File.DirectoryIterator(backupFolder);
  try {
    yield iterator.forEach(aEntry => {
      count++;
      if (PlacesBackups.filenamesRegex.test(aEntry.name))
        lastBackupPath = aEntry.path;
    });
  } finally {
    iterator.close();
  }

  do_check_eq(count, 2);
  do_check_neq(lastBackupPath, null);
  do_check_false(yield OS.File.exists(oldJsonPath));
  do_check_true(yield OS.File.exists(jsonPath));
});

add_task(function check_max_backups_greater_than_backups() {
  // Get bookmarkBackups directory
  let backupFolder = yield PlacesBackups.getBackupFolder();

  // Export bookmarks to JSON.
  // Allow 3 backups, none should be removed.
  yield PlacesBackups.create(3);
  let backupFilename = PlacesBackups.getFilenameForDate();

  let count = 0;
  let lastBackupPath = null;
  let iterator = new OS.File.DirectoryIterator(backupFolder);
  try {
    yield iterator.forEach(aEntry => {
      count++;
      if (PlacesBackups.filenamesRegex.test(aEntry.name))
        lastBackupPath = aEntry.path;
    });
  } finally {
    iterator.close();
  }
  do_check_eq(count, 2);
  do_check_neq(lastBackupPath, null);
});

add_task(function check_max_backups_null() {
  // Get bookmarkBackups directory
  let backupFolder = yield PlacesBackups.getBackupFolder();

  // Export bookmarks to JSON.
  // Allow infinite backups, none should be removed, a new one is not created
  // since one for today already exists.
  yield PlacesBackups.create(null);
  let backupFilename = PlacesBackups.getFilenameForDate();

  let count = 0;
  let lastBackupPath = null;
  let iterator = new OS.File.DirectoryIterator(backupFolder);
  try {
    yield iterator.forEach(aEntry => {
      count++;
      if (PlacesBackups.filenamesRegex.test(aEntry.name))
        lastBackupPath = aEntry.path;
    });
  } finally {
    iterator.close();
  }
  do_check_eq(count, 2);
  do_check_neq(lastBackupPath, null);
});

add_task(function check_max_backups_undefined() {
  // Get bookmarkBackups directory
  let backupFolder = yield PlacesBackups.getBackupFolder();

  // Export bookmarks to JSON.
  // Allow infinite backups, none should be removed, a new one is not created
  // since one for today already exists.
  yield PlacesBackups.create();
  let backupFilename = PlacesBackups.getFilenameForDate();

  let count = 0;
  let lastBackupPath = null;
  let iterator = new OS.File.DirectoryIterator(backupFolder);
  try {
    yield iterator.forEach(aEntry => {
      count++;
      if (PlacesBackups.filenamesRegex.test(aEntry.name))
        lastBackupPath = aEntry.path;
    });
  } finally {
    iterator.close();
  }
  do_check_eq(count, 2);
  do_check_neq(lastBackupPath, null);
});

function run_test() {
  run_next_test();
}
