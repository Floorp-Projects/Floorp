/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests migration from v1 to v2

function run_test()
{
  // First import the downloads.sqlite file
  importDatabaseFile("v1.sqlite");

  // ok, now it is OK to init the download manager - this will perform the
  // migration!
  var dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  var dbConn = dm.DBConnection;
  var stmt = null;

  // check schema version
  do_check_true(dbConn.schemaVersion >= 2);

  // Check that the column no longer exists
  try {
    // throws when it doesn't exist
    stmt = dbConn.createStatement("SELECT iconURL FROM moz_downloads");
    do_throw("should not get here");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FAILURE, e.result);
  }

  // now we check the entries
  stmt = dbConn.createStatement(
    "SELECT name, source, target, startTime, endTime, state " +
    "FROM moz_downloads " +
    "WHERE id = 2");
  stmt.executeStep();
  do_check_eq("381603.patch", stmt.getString(0));
  do_check_eq("https://bugzilla.mozilla.org/attachment.cgi?id=266520",
              stmt.getUTF8String(1));
  do_check_eq("file:///Users/sdwilsh/Desktop/381603.patch",
              stmt.getUTF8String(2));
  do_check_eq(1180493839859230, stmt.getInt64(3));
  do_check_eq(1180493839859230, stmt.getInt64(4));
  do_check_eq(1, stmt.getInt32(5));
  stmt.finalize();

  cleanup();
}

