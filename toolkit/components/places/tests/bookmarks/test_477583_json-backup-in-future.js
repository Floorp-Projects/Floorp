/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  do_test_pending();

  let bookmarksBackupDir = PlacesBackups.folder;
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
  do_check_eq(name, "bookmarks-" + dateObj.toLocaleFormat("%Y-%m-%d") + ".json");
  let rx = new RegExp("^" + name.replace(/\.json/, "") + "(_[0-9]+){0,1}\.json$");
  let files = bookmarksBackupDir.directoryEntries;
  while (files.hasMoreElements()) {
    let entry = files.getNext().QueryInterface(Ci.nsIFile);
    if (entry.leafName.match(rx))
      entry.remove(false);
  }

  let futureBackupFile = bookmarksBackupDir.clone();
  futureBackupFile.append(name);
  futureBackupFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
  do_check_true(futureBackupFile.exists());

  do_check_eq(PlacesBackups.entries.length, 0);

  Task.spawn(function() {
    yield PlacesBackups.create();
    // Check that a backup for today has been created.
    do_check_eq(PlacesBackups.entries.length, 1);
    let mostRecentBackupFile = PlacesBackups.getMostRecent();
    do_check_neq(mostRecentBackupFile, null);
    let todayFilename = PlacesBackups.getFilenameForDate();
    rx = new RegExp("^" + todayFilename.replace(/\.json/, "") + "(_[0-9]+){0,1}\.json$");
    do_check_true(mostRecentBackupFile.leafName.match(rx).length > 0);

    // Check that future backup has been removed.
    do_check_false(futureBackupFile.exists());

    // Cleanup.
    mostRecentBackupFile.remove(false);
    do_check_false(mostRecentBackupFile.exists());

    do_test_finished()
  });
}
