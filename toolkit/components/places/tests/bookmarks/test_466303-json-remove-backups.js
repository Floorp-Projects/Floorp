/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const NUMBER_OF_BACKUPS = 1;

function run_test() {
  do_test_pending();

  // Get bookmarkBackups directory
  var bookmarksBackupDir = PlacesBackups.folder;

  // Create an html dummy backup in the past
  var htmlBackupFile = bookmarksBackupDir.clone();
  htmlBackupFile.append("bookmarks-2008-01-01.html");
  if (htmlBackupFile.exists())
    htmlBackupFile.remove(false);
  do_check_false(htmlBackupFile.exists());
  htmlBackupFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
  do_check_true(htmlBackupFile.exists());

  // Create a json dummy backup in the past
  var jsonBackupFile = bookmarksBackupDir.clone();
  jsonBackupFile.append("bookmarks-2008-01-31.json");
  if (jsonBackupFile.exists())
    jsonBackupFile.remove(false);
  do_check_false(jsonBackupFile.exists());
  jsonBackupFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
  do_check_true(jsonBackupFile.exists());

  // Export bookmarks to JSON.
  var backupFilename = PlacesBackups.getFilenameForDate();
  var rx = new RegExp("^" + backupFilename.replace(/\.json/, "") + "(_[0-9]+){0,1}\.json$");
  var files = bookmarksBackupDir.directoryEntries;
  var entry;
  while (files.hasMoreElements()) {
    entry = files.getNext().QueryInterface(Ci.nsIFile);
    if (entry.leafName.match(rx))
      entry.remove(false);
  }

  Task.spawn(function() {
    yield PlacesBackups.create(NUMBER_OF_BACKUPS);
    files = bookmarksBackupDir.directoryEntries;
    while (files.hasMoreElements()) {
      entry = files.getNext().QueryInterface(Ci.nsIFile);
      if (entry.leafName.match(rx))
        lastBackupFile = entry;
    }
    do_check_true(lastBackupFile.exists());

    // Check that last backup has been retained
    do_check_false(htmlBackupFile.exists());
    do_check_false(jsonBackupFile.exists());
    do_check_true(lastBackupFile.exists());

    // cleanup
    lastBackupFile.remove(false);
    do_check_false(lastBackupFile.exists());

    do_test_finished();
  });
}
