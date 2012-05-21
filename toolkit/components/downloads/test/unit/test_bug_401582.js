/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests that downloads in the scanning state are set to a completed state
// upon service initialization.

importDownloadsFile("bug_401582_downloads.sqlite");

const nsIDownloadManager = Ci.nsIDownloadManager;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDownloadManager);

function test_noScanningDownloads()
{
  var stmt = dm.DBConnection.createStatement(
    "SELECT * " +
    "FROM moz_downloads " +
    "WHERE state = ?1");
  stmt.bindByIndex(0, nsIDownloadManager.DOWNLOAD_SCANNING);

  do_check_false(stmt.executeStep());
  stmt.reset();
  stmt.finalize();
}

var tests = [test_noScanningDownloads];

function run_test()
{
  for (var i = 0; i < tests.length; i++)
    tests[i]();
}
