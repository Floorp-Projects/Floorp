/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
  Test download manager's interaction with the private browsing service.

  An overview of what this test does follows:

    * Create a download (Download-A) with specific details.
    * Check that Download-A is retrievable.
    * Enter private browsing mode.
    * Check that Download-A is not accessible.
    * Create another download (Download-B) with specific and different details.
    * Check that Download-B is retrievable.
    * Exit private browsing mode.
    * Check that Download-A is retrievable.
    * Check that Download-B is not accessible.
    * Create a new download (Download-C) with specific details.
      Start it and enter private browsing mode immediately after.
    * Check that Download-C has been paused.
    * Exit the private browsing mode.
    * Verify that Download-C resumes and finishes correctly now.
**/

Components.utils.import("resource://gre/modules/Services.jsm");

this.__defineGetter__("dm", function() {
  delete this.dm;
  return this.dm = Cc["@mozilla.org/download-manager;1"].
                   getService(Ci.nsIDownloadManager);
});

/**
 * Try to see if an active download is available using the |activeDownloads|
 * property.
 */
function is_active_download_available(aGUID, aSrc, aDst, aName, aPrivate) {
  let enumerator = aPrivate ? dm.activePrivateDownloads : dm.activeDownloads;
  while (enumerator.hasMoreElements()) {
    let download = enumerator.getNext();
    if (download.guid == aGUID &&
        download.source.spec == aSrc &&
        download.targetFile.path == aDst.path &&
        download.displayName == aName &&
        download.isPrivate == aPrivate)
      return true;
  }
  return false;
}

function expect_download_present(aGUID, aSrc, aDst, aName, aPrivate) {
  is_download_available(aGUID, aSrc, aDst, aName, aPrivate, true);
}

function expect_download_absent(aGUID, aSrc, aDst, aName, aPrivate) {
  is_download_available(aGUID, aSrc, aDst, aName, aPrivate, false);
}

/**
 * Try to see if a download is available using the |getDownloadByGUID| method.  The
 * download can both be active or inactive.
 */
function is_download_available(aGUID, aSrc, aDst, aName, aPrivate, present) {
  do_test_pending();
  dm.getDownloadByGUID(aGUID, function(status, download) {
    if (!present) {
      do_check_eq(download, null);
    } else {
      do_check_neq(download, null);
      do_check_eq(download.guid, aGUID);
      do_check_eq(download.source.spec, aSrc);
      do_check_eq(download.targetFile.path, aDst.path);
      do_check_eq(download.displayName, aName);
      do_check_eq(download.isPrivate, aPrivate);
    }
    do_test_finished();
  });
}

function run_test() {
  if (oldDownloadManagerDisabled()) {
    return;
  }

  let prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);

  do_test_pending();
  let httpserv = new HttpServer();
  httpserv.registerDirectory("/", do_get_cwd());
  httpserv.start(-1);

  let tmpDir = do_get_tempdir();
  const nsIWBP = Ci.nsIWebBrowserPersist;

  // make sure we're starting with an empty DB
  do_check_eq(dm.activeDownloadCount, 0);
  do_check_eq(dm.activePrivateDownloadCount, 0);

  let listener = {
    phase: 1,
    handledC: false,
    onDownloadStateChange: function(aState, aDownload)
    {
      switch (aDownload.state) {
        case dm.DOWNLOAD_FAILED:
        case dm.DOWNLOAD_CANCELED:
        case dm.DOWNLOAD_DIRTY:
        case dm.DOWNLOAD_BLOCKED_POLICY:
          // Fail!
          if (aDownload.targetFile.exists())
            aDownload.targetFile.remove(false);
          dm.removeListener(this);
          do_throw("Download failed (name: " + aDownload.displayName + ", state: " + aDownload.state + ")");
          do_test_finished();
          break;

        // We need to wait until Download-C has started, because otherwise it
        // won't be resumable, so the private browsing mode won't correctly pause it.
        case dm.DOWNLOAD_DOWNLOADING:
          if (aDownload.guid == downloadC && !this.handledC && this.phase == 2) {
            // Sanity check: Download-C must be resumable
            do_check_true(dlC.resumable);

            // Check that Download-A is accessible
            expect_download_present(downloadA, downloadASource,
              fileA, downloadAName, false);

            // Check that Download-B is not accessible
            expect_download_absent(downloadB, downloadBSource,
              fileB, downloadBName, true);

            // Check that Download-C is accessible
            expect_download_present(downloadC, downloadCSource,
              fileC, downloadCName, false);

            // only perform these checks the first time that Download-C is started
            this.handledC = true;
          }
          break;

        case dm.DOWNLOAD_FINISHED:
          do_check_true(aDownload.targetFile.exists());
          aDownload.targetFile.remove(false);
          this.onDownloadFinished();
          break;
      }
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { },
    onDownloadFinished: function () {
      switch (this.phase) {
        case 1: {
          do_check_eq(dm.activeDownloadCount, 0);

          // Create Download-B
          let dlB = addDownload(httpserv, {
            isPrivate: true,
            targetFile: fileB,
            sourceURI: downloadBSource,
            downloadName: downloadBName,
            runBeforeStart: function (aDownload) {
              // Check that Download-B is retrievable
              do_check_eq(dm.activePrivateDownloadCount, 1);
              do_check_eq(dm.activeDownloadCount, 0);
              do_check_true(is_active_download_available(aDownload.guid,
                downloadBSource, fileB, downloadBName, true));
              expect_download_present(aDownload.guid,
                downloadBSource, fileB, downloadBName, true);
            }
          });
          downloadB = dlB.guid;

          // wait for Download-B to finish
          ++this.phase;
        }
        break;

        case 2: {
          do_check_eq(dm.activeDownloadCount, 0);

          // Simulate last private browsing context triggering cleanup
          Services.obs.notifyObservers(null, "last-pb-context-exited", null);

          // Create Download-C
          dlC = addDownload(httpserv, {
            isPrivate: false,
            targetFile: fileC,
            sourceURI: downloadCSource,
            downloadName: downloadCName,
            runBeforeStart: function (aDownload) {
              // Check that Download-C is retrievable
              do_check_eq(dm.activePrivateDownloadCount, 0);
              do_check_true(is_active_download_available(aDownload.guid,
                downloadCSource, fileC, downloadCName, false));
              expect_download_present(aDownload.guid,
                downloadCSource, fileC, downloadCName, false);
            }
          });
          downloadC = dlC.guid;

          // wait for Download-C to finish
          ++this.phase;

          // Check that Download-A is accessible
          expect_download_present(downloadA, downloadASource,
            fileA, downloadAName, false);

          // Check that Download-B is not accessible
          expect_download_absent(downloadB, downloadBSource,
            fileB, downloadBName, true);
        }
        break;

        case 3: {
          do_check_eq(dm.activePrivateDownloadCount, 0);

          // Check that Download-A is accessible
          expect_download_present(downloadA, downloadASource,
            fileA, downloadAName, false);

          // Check that Download-B is not accessible
          expect_download_absent(downloadB, downloadBSource,
            fileB, downloadBName, true);

          // Check that Download-C is accessible
          expect_download_present(downloadC, downloadCSource,
            fileC, downloadCName, false);

          // cleanup
          dm.removeListener(this);
          httpserv.stop(do_test_finished);
        }
        break;

        default:
          do_throw("Unexpected phase: " + this.phase);
          break;
      }
    }
  };

  dm.addPrivacyAwareListener(listener);
  dm.addPrivacyAwareListener(getDownloadListener());

  // properties of Download-A
  let downloadA = -1;
  const downloadASource = "data:image/png;base64,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";
  const downloadADest = "download-file-A";
  const downloadAName = "download-A";

  // properties of Download-B
  let downloadB = -1;
  const downloadBSource = "data:application/octet-stream;base64,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";
  const downloadBDest = "download-file-B";
  const downloadBName = "download-B";

  // properties of Download-C
  let downloadC = -1;
  const downloadCSource = "http://localhost:" +
                          httpserv.identity.primaryPort +
                          "/head_download_manager.js";
  const downloadCDest = "download-file-C";
  const downloadCName = "download-C";

  // Create all target files
  let fileA = tmpDir.clone();
  fileA.append(downloadADest);
  fileA.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
  let fileB = tmpDir.clone();
  fileB.append(downloadBDest);
  fileB.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
  let fileC = tmpDir.clone();
  fileC.append(downloadCDest);
  fileC.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);

  // use js closures to access dlC
  let dlC;

  // Create Download-A
  let dlA = addDownload(httpserv, {
    isPrivate: false,
    targetFile: fileA,
    sourceURI: downloadASource,
    downloadName: downloadAName,
    runBeforeStart: function (aDownload) {
      // Check that Download-A is retrievable
      do_check_eq(dm.activePrivateDownloadCount, 0);
      do_check_true(is_active_download_available(aDownload.guid, downloadASource, fileA, downloadAName, false));
      expect_download_present(aDownload.guid, downloadASource, fileA, downloadAName, false);
    }
  });
  downloadA = dlA.guid;

  // wait for Download-A to finish
}
