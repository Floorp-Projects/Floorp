/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Public request gets times=0 cookie, completes
// Private request gets times=1 cookie, canceled
// Private resumed request sends times=1 cookie, completes

function run_test() {
  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  do_test_pending();
  let httpserv = new HttpServer();

  let times = 0;
  httpserv.registerPathHandler("/head_download_manager.js", function (meta, response) {
    response.setHeader("Content-Type", "text/plain", false);
    response.setStatusLine("1.1", !meta.hasHeader('range') ? 200 : 206);

    // Set a cookie if none is sent with the request. Public and private requests
    // should therefore receive different cookies, so we can tell if the resumed
    // request is actually treated as private or not.
    if (!meta.hasHeader('Cookie')) {
      do_check_true(times == 0 || times == 1);
      response.setHeader('Set-Cookie', 'times=' + times++);
    } else {
      do_check_eq(times, 2);
      do_check_eq(meta.getHeader('Cookie'), 'times=1');
    }
    let full = "";
    let body = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"; //60
    for (var i = 0; i < 1000; i++) {
      full += body;      
    }
    response.write(full);
  });
  httpserv.start(4444);

  let state = 0;

  let listener = {
    onDownloadStateChange: function(aState, aDownload) {
      switch (aDownload.state) {
        case downloadUtils.downloadManager.DOWNLOAD_DOWNLOADING:
          // We only care about the private download
          if (state != 1)
            break;

          state++;
          do_check_true(aDownload.resumable);

          aDownload.pause();
          do_check_eq(aDownload.state, downloadUtils.downloadManager.DOWNLOAD_PAUSED);

          do_execute_soon(function() {
            aDownload.resume();
          });
          break;

        case downloadUtils.downloadManager.DOWNLOAD_FINISHED:
          if (state == 0) {
            do_execute_soon(function() {
              // Perform an identical request but in private mode.
              // It should receive a different cookie than the
              // public request.

              state++;
                              
              addDownload({
                isPrivate: true,
                sourceURI: downloadCSource,
                downloadName: downloadCName + "!!!",
                runBeforeStart: function (aDownload) {
                  // Check that dl is retrievable
                  do_check_eq(downloadUtils.downloadManager.activePrivateDownloadCount, 1);
                }
              });
            });
          } else if (state == 2) {
            // We're done here.
            do_execute_soon(function() {
              httpserv.stop(do_test_finished);
            });
          }
          break;

        default:
          break;
      }
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { }
  };

  downloadUtils.downloadManager.addPrivacyAwareListener(listener);

  const downloadCSource = "http://localhost:4444/head_download_manager.js";
  const downloadCName = "download-C";

  // First a public download that completes without interruption.
  let dl = addDownload({
    isPrivate: false,
    sourceURI: downloadCSource,
    downloadName: downloadCName,
    runBeforeStart: function (aDownload) {
      // Check that dl is retrievable
      do_check_eq(downloadUtils.downloadManager.activeDownloadCount, 1);
    }
  });
}