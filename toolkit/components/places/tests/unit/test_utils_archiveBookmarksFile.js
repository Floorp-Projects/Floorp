/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

 /**
  * Check for correct functionality of PlacesUtils.archiveBookmarksFile
  */

Components.utils.import("resource://gre/modules/utils.js");

const PREFIX = "bookmarks-";
// The localized prefix must be "bigger" and associated to older backups.
const LOCALIZED_PREFIX = "segnalibri-";
const SUFFIX = ".json";
const NUMBER_OF_BACKUPS = 10;

function run_test() {
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

  // Get and cleanup the backups folder.
  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  var bookmarksBackupDir = dirSvc.get("ProfD", Ci.nsILocalFile);
  bookmarksBackupDir.append("bookmarkbackups");
  if (bookmarksBackupDir.exists()) {
    bookmarksBackupDir.remove(true);
    do_check_false(bookmarksBackupDir.exists());
  }
  bookmarksBackupDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0777);

  // Fake backups are created backwards to ensure we won't consider file
  // creation time.
  // Create fake backups for the newest dates.
  for (let i = dates.length - 1; i >= 0; i--) {
    let backupFilename;
    if (i > Math.floor(dates.length/2))
      backupFilename = PREFIX + dates[i] + SUFFIX;
    else
      backupFilename = LOCALIZED_PREFIX + dates[i] + SUFFIX;
    dump("creating: " + backupFilename + "\n");
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

  PlacesUtils.archiveBookmarksFile(Math.floor(dates.length/2));
  // Add today's backup.
  dates.push(dateObj.toLocaleFormat("%Y-%m-%d"));

  // Check backups.
  for (var i = 0; i < dates.length; i++) {
    let backupFilename;
    let shouldExist;
    if (i > Math.floor(dates.length/2)) {
      backupFilename = PREFIX + dates[i] + SUFFIX;
      shouldExist = true;
    }
    else {
      backupFilename = LOCALIZED_PREFIX + dates[i] + SUFFIX;
      shouldExist = false;
    }
    var backupFile = bookmarksBackupDir.clone();
    backupFile.append(backupFilename);
    if (backupFile.exists() != shouldExist)
      do_throw("Backup should " + (shouldExist ? "" : "not") + " exist: " + backupFilename);
  }

  // Cleanup backups folder.
  bookmarksBackupDir.remove(true);
  do_check_false(bookmarksBackupDir.exists());
  bookmarksBackupDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0777);
}
