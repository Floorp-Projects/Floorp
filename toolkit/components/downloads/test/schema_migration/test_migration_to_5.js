/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests migration from v4 to v5

function run_test()
{
  // First import the downloads.sqlite file
  importDatabaseFile("v4.sqlite");

  // ok, now it is OK to init the download manager - this will perform the
  // migration!
  var dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  var dbConn = dm.DBConnection;

  // check schema version
  do_check_true(dbConn.schemaVersion >= 5);

  // Check that the columns exist (no throw) and entries are correct
  var stmt = dbConn.createStatement(
    "SELECT name, source, target, startTime, endTime, state, referrer, " +
           "entityID, tempPath " +
    "FROM moz_downloads " +
    "WHERE id = 27");
  stmt.executeStep();
  do_check_eq("Firefox 2.0.0.6.dmg", stmt.getString(0));
  do_check_eq("http://ftp-mozilla.netscape.com/pub/mozilla.org/firefox/releases/2.0.0.6/mac/en-US/Firefox%202.0.0.6.dmg",
              stmt.getUTF8String(1));
  do_check_eq("file:///Users/sdwilsh/Desktop/Firefox%202.0.0.6.dmg",
              stmt.getUTF8String(2));
  do_check_eq(1187390974170783, stmt.getInt64(3));
  do_check_eq(1187391001257446, stmt.getInt64(4));
  do_check_eq(1, stmt.getInt32(5));
  do_check_eq("http://www.mozilla.com/en-US/products/download.html?product=firefox-2.0.0.6&os=osx&lang=en-US",stmt.getUTF8String(6));
  do_check_true(stmt.getIsNull(7));
  do_check_true(stmt.getIsNull(8));
  stmt.finalize();

  cleanup();
}
