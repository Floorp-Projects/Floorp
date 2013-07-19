/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);

function run_test()
{
  let server = new HttpServer();
  server.start(-1);
  let dl = addDownload(server);
  do_test_pending();

  do_print(dl.guid);
  do_check_true(/^[a-zA-Z0-9\-_]{12}$/.test(dl.guid));

  dm.getDownloadByGUID(dl.guid, function(status, result) {
    do_check_eq(dl, result, "should get back some download as requested");
    dl.cancel();

    dm.getDownloadByGUID("nonexistent", function(status, result) {
      do_check_eq(result, null, "should get back no download");
      do_check_eq(Components.results.NS_ERROR_NOT_AVAILABLE, status,
                  "should pass NS_ERROR_NOT_AVAILABLE on failure");
      do_test_finished();
    });
  });
}
