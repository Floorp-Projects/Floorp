/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const NUMBER_OF_BACKUPS = 1;

function run_test() {
  // Get bookmarkBackups directory
  var bookmarksBackupDir = PlacesUtils.backups.folder;

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
  var backupFilename = PlacesUtils.backups.getFilenameForDate();
  var lastBackupFile = bookmarksBackupDir.clone();
  lastBackupFile.append(backupFilename);
  if (lastBackupFile.exists())
    lastBackupFile.remove(false);
  do_check_false(lastBackupFile.exists());
  PlacesUtils.backups.create(NUMBER_OF_BACKUPS);
  do_check_true(lastBackupFile.exists());

  // Check that last backup has been retained
  do_check_false(htmlBackupFile.exists());
  do_check_false(jsonBackupFile.exists());
  do_check_true(lastBackupFile.exists());

  // cleanup
  lastBackupFile.remove(false);
  do_check_false(lastBackupFile.exists());
}
