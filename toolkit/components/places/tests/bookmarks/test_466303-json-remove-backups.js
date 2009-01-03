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
 * The Original Code is Bug 466303 code.
 *
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net>
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

Components.utils.import("resource://gre/modules/utils.js");

const NUMBER_OF_BACKUPS = 1;

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);

function run_test() {
  // Get bookmarkBackups directory
  var bookmarksBackupDir = dirSvc.get("ProfD", Ci.nsILocalFile);
  bookmarksBackupDir.append("bookmarkbackups");
  if (!bookmarksBackupDir.exists()) {
    bookmarksBackupDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
    do_check_true(bookmarksBackupDir.exists());
  }

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

  // Export bookmarks
  var date = new Date().toLocaleFormat("%Y-%m-%d");
  var backupFilename = "bookmarks-" + date + ".json";
  var lastBackupFile = bookmarksBackupDir.clone();
  lastBackupFile.append(backupFilename);
  if (lastBackupFile.exists())
    lastBackupFile.remove(false);
  do_check_false(lastBackupFile.exists());
  PlacesUtils.archiveBookmarksFile(NUMBER_OF_BACKUPS);
  do_check_true(lastBackupFile.exists());

  // Check that last backup has been retained
  do_check_false(htmlBackupFile.exists());
  do_check_false(jsonBackupFile.exists());
  do_check_true(lastBackupFile.exists());

  // cleanup
  lastBackupFile.remove(false);
  do_check_false(lastBackupFile.exists());
}
