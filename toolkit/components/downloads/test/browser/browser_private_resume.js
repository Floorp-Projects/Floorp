/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let windowsToClose = [];
  let download;
  let httpServer = startServer();
  let listener = {
    onDownloadStateChange: function(aState, aDownload) {
      switch (aDownload.state) {
        case Services.downloads.DOWNLOAD_DOWNLOADING:
          ok(false, "Listener should not be notified on a private window");
          break;
      }
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { }
  };

  Services.downloads.addListener(listener);

  registerCleanupFunction(function() {
    Services.downloads.removeListener(listener);
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  function testOnWindow(aIsPrivate, aCallback) {
    whenNewWindowLoaded(aIsPrivate, function(win) {
      windowsToClose.push(win);
      aCallback(win);
    });
  }

  function testDownload(aWin, aIsPrivate) {
    let downloadInfo = {
      isPrivate: aIsPrivate,
      sourceURI: "http://localhost:4444/bigFile",
      downloadName: "download-" + aIsPrivate,
      runBeforeStart: function (aDownload) {
        is(Services.downloads.activeDownloadCount, aIsPrivate ? 0 : 1,
           "Check that download is retrievable");
      }
    }
    return addDownload(downloadInfo);
  }

  httpServer.registerPathHandler("/bigFile", function (meta, response) {
    response.setHeader("Content-Type", "text/plain", false);
    response.setStatusLine("1.1", !meta.hasHeader('range') ? 200 : 206);

    let full = "";
    let body = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"; //60
    for (let i = 0; i < 1000; i++) {
      full += body;
    }
    response.write(full);
  });

  testOnWindow(true, function(aWin) {
    download = testDownload(aWin, true);
    waitForDownloadState(download, Services.downloads.DOWNLOAD_DOWNLOADING, function() {
      is(download.resumable, true, "Private download must be resumable");
      isnot(download.cancelable, null, "Private download must be cancelable");
      download.pause();
      is(download.state, Services.downloads.DOWNLOAD_PAUSED,
         "Private download should be paused");
      download.resume();
      is(download.state, Services.downloads.DOWNLOAD_DOWNLOADING,
         "Private download should be resumed");
      download.cancel();
      is(Services.downloads.activeDownloadCount, 0,
         "There should not be active downloads");
      httpServer.stop(finish);
    });
  });
}
