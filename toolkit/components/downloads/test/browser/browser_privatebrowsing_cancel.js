/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let windowsToClose = [];
  let publicDownload;
  let privateDownload;
  let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  let httpServer = startServer();
  let listener = {
    onDownloadStateChange: function(aState, aDownload) {
      switch (aDownload.state) {
        case Services.downloads.DOWNLOAD_QUEUED:
        case Services.downloads.DOWNLOAD_DOWNLOADING:
          if (aDownload.targetFile.equals(publicDownload.targetFile)) {
            is(Services.downloads.activeDownloadCount, 1,
              "Check public download is active");
            is(publicDownload.resumable, false,
               "Download must not be resumable");
            isnot(publicDownload.cancelable, null,
                  "Download must be cancelable");
            // Cancel the download
            Services.downloads.cancelDownload(publicDownload.id);
          }
          break;
        case Services.downloads.DOWNLOAD_CANCELED:
          if (aDownload.targetFile.equals(publicDownload.targetFile)) {
            is(publicDownload.resumable, false,
               "Download must not be resumable");
            is(publicDownload.cancelable, null,
               "Download must not be cancelable");
            testOnWindow(true, function(aWin) {
              is(Services.downloads.activeDownloadCount, 0,
                 "Check public download is not active on private window");
              privateDownload = testDownload(aWin, true);
              // Try to cancel the download but will fail since it's private
              try {
                Services.downloads.cancelDownload(privateDownload.id);
                ok(false, "Cancel private download should have failed.");
              } catch (e) {
                testOnWindow(false, function(aWin) {
                  is(Services.downloads.activeDownloadCount, 0,
                   "Check that there are no active downloads");
                  cleanUp();
                });
              }
            });
          }
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

  function cleanUp() {
    Services.downloads.removeDownload(publicDownload.id);
    if (publicDownload.targetFile.exists()) {
      publicDownload.targetFile.remove(false);
    }
    if (privateDownload.targetFile.exists()) {
      privateDownload.targetFile.remove(false);
    }
    Services.downloads.cleanUp();
    is(Services.downloads.activeDownloadCount, 0,
      "Make sure we're starting with an empty DB");
    httpServer.stop(finish);
  }

  function testOnWindow(aIsPrivate, aCallback) {
    whenNewWindowLoaded(aIsPrivate, function(win) {
      windowsToClose.push(win);
      aCallback(win);
    });
  }

  function createFile(aFileName) {
    let file = tmpDir.clone();
    file.append(aFileName);
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
    return file;
  }

  function testDownload(aWin, aIsPrivate) {
    let file = createFile("download-file-" + aIsPrivate);
    let sourceURI =
      aIsPrivate ? "file/browser_privatebrowsing_cancel.js" : "noresume";
    let downloadInfo = {
      isPrivate: aIsPrivate,
      targetFile: file,
      sourceURI: "http://localhost:4444/" + sourceURI,
      downloadName: "download-" + aIsPrivate
    }
    return addDownload(downloadInfo);
  }

  // Make sure we're starting with an empty DB
  is(Services.downloads.activeDownloadCount, 0,
     "Make sure we're starting with an empty DB");

  testOnWindow(false, function(aWin) {
    publicDownload = testDownload(aWin, false);
  });
}
