/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Make sure we remove old, now-unused downloads.rdf (pre-Firefox 3 storage)
// when starting the download manager.

function run_test()
{
  if (oldDownloadManagerDisabled()) {
    return;
  }

  // Create the downloads.rdf file
  importDownloadsFile("empty_downloads.rdf");

  // Make sure it got created
  let rdfFile = dirSvc.get("DLoads", Ci.nsIFile);
  do_check_true(rdfFile.exists());

  // Initialize the download manager, which will delete downloads.rdf
  Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
  do_check_false(rdfFile.exists());
}
