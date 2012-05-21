/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test()
{
  // We're testing migration to this version from one version below
  var targetVersion = 8;

  // First import the downloads.sqlite file
  importDatabaseFile("v" + (targetVersion - 1) + ".sqlite");

  // Init the download manager which will try migrating to the new version
  var dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  var dbConn = dm.DBConnection;

  // Check schema version
  do_check_true(dbConn.schemaVersion >= targetVersion);

  // Make sure all the columns are there
  var stmt = dbConn.createStatement(
    "SELECT name, source, target, tempPath, startTime, endTime, state, " +
           "referrer, entityID, currBytes, maxBytes, mimeType, " +
           "preferredApplication, preferredAction, autoResume " +
    "FROM moz_downloads " +
    "WHERE id = 28");
  stmt.executeStep();

  // This data is based on the original values in the table
  var data = [
    "firefox-3.0a9pre.en-US.linux-i686.tar.bz2",
    "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/firefox-3.0a9pre.en-US.linux-i686.tar.bz2",
    "file:///Users/Ed/Desktop/firefox-3.0a9pre.en-US.linux-i686.tar.bz2",
    "/Users/Ed/Desktop/+EZWafFQ.bz2.part",
    1192469856209164,
    1192469877017396,
    Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    "http://ftp.mozilla.org/pub/mozilla.org/firefox/nightly/latest-trunk/",
    "%2210e66c1-8a2d6b-9b33f380%22/9055595/Mon, 15 Oct 2007 11:45:34 GMT",
    1210772,
    9055595,
    "application/x-bzip2",
    "AAAAAAGqAAIAAQxNYWNpbnRvc2ggSEQAAAAAAAAAAAAAAAAAAAC+91IESCsAAAAHc5UUU3R1ZmZJdCBFeHBhbmRlci5hcHAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAdzmb3SGI8AAAAAAAAAAP////8AAAkgAAAAAAAAAAAAAAAAAAAAFFN0dWZmSXQgU3RhbmRhcmQgOS4wABAACAAAvveYVAAAABEACAAAvdJs7wAAAAEACAAHc5UAAAAWAAIAQ01hY2ludG9zaCBIRDpBcHBsaWNhdGlvbnM6U3R1ZmZJdCBTdGFuZGFyZCA5LjA6U3R1ZmZJdCBFeHBhbmRlci5hcHAAAA4AKgAUAFMAdAB1AGYAZgBJAHQAIABFAHgAcABhAG4AZABlAHIALgBhAHAAcAAPABoADABNAGEAYwBpAG4AdABvAHMAaAAgAEgARAASADZBcHBsaWNhdGlvbnMvU3R1ZmZJdCBTdGFuZGFyZCA5LjAvU3R1ZmZJdCBFeHBhbmRlci5hcHAAEwABLwD//wAA",
    2,
    // For the new columns added, check for null or default values
    0,
  ];

  // Make sure the values are correct after the migration
  var i = 0;
  do_check_eq(data[i], stmt.getString(i++));
  do_check_eq(data[i], stmt.getUTF8String(i++));
  do_check_eq(data[i], stmt.getUTF8String(i++));
  do_check_eq(data[i], stmt.getString(i++));
  do_check_eq(data[i], stmt.getInt64(i++));
  do_check_eq(data[i], stmt.getInt64(i++));
  do_check_eq(data[i], stmt.getInt32(i++));
  do_check_eq(data[i], stmt.getUTF8String(i++));
  do_check_eq(data[i], stmt.getUTF8String(i++));
  do_check_eq(data[i], stmt.getInt64(i++));
  do_check_eq(data[i], stmt.getInt64(i++));
  do_check_eq(data[i], stmt.getUTF8String(i++));
  do_check_eq(data[i], stmt.getUTF8String(i++));
  do_check_eq(data[i], stmt.getInt32(i++));
  do_check_eq(data[i], stmt.getInt32(i++));

  stmt.reset();
  stmt.finalize();

  cleanup();
}
