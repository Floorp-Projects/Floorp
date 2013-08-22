/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file ensures that the download manager service can be instantiated with
// a certain downloads.sqlite file that had incorrect data.

importDownloadsFile("bug_409179_downloads.sqlite");

function run_test()
{
  if (oldDownloadManagerDisabled()) {
    return;
  }

  var caughtException = false;
  try {
    var dm = Cc["@mozilla.org/download-manager;1"].
             getService(Ci.nsIDownloadManager);
  } catch (e) {
    caughtException = true;
  }
  do_check_false(caughtException);
}
