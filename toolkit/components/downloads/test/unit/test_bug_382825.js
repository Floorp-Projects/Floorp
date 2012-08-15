/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This tests the retryDownload function of nsIDownloadManager.  This function
// was added in Bug 382825.

const nsIDownloadManager = Ci.nsIDownloadManager;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDownloadManager);

function test_retry_canceled()
{
  var dl = addDownload();

  // since we are going to be retrying a failed download, we need to inflate
  // this so it doesn't stop our server
  gDownloadCount++;

  dm.cancelDownload(dl.id);

  do_check_eq(nsIDownloadManager.DOWNLOAD_CANCELED, dl.state);

  // Our download object will no longer be updated.
  dm.retryDownload(dl.id);
}

function test_retry_bad()
{
  try {
    dm.retryDownload(0);
    do_throw("Hey!  We expect to get an exception with this!");
  } catch(e) {
    do_check_eq(Components.lastResult, Cr.NS_ERROR_NOT_AVAILABLE);
  }
}

var tests = [test_retry_canceled, test_retry_bad];

var httpserv = null;
function run_test()
{
  httpserv = new HttpServer();
  httpserv.registerDirectory("/", do_get_cwd());
  httpserv.start(4444);

  dm.addListener(getDownloadListener());

  for (var i = 0; i < tests.length; i++)
    tests[i]();
}
