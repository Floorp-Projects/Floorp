/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
  Make sure that the download manager service is given a chance to cancel the
  private browsing mode transition.
**/

Components.utils.import("resource://testing-common/MockRegistrar.jsm");

var Cm = Components.manager;

var promptService = {
  _buttonChoice: 0,
  _called: false,
  wasCalled: function() {
    let called = this._called;
    this._called = false;
    return called;
  },
  sayCancel: function() {
    this._buttonChoice = 1;
    this._called = false;
  },
  sayProceed: function() {
    this._buttonChoice = 0;
    this._called = false;
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIPromptService) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  confirmEx: function(parent, title, text, buttonFlags,
                      button0Title, button1Title, button2Title,
                      checkMsg, checkState) {
    this._called = true;
    return this._buttonChoice;
  }
};

var mockCID =
  MockRegistrar.register("@mozilla.org/embedcomp/prompt-service;1",
                         promptService);

this.__defineGetter__("dm", function() {
  delete this.dm;
  return this.dm = Cc["@mozilla.org/download-manager;1"].
                   getService(Ci.nsIDownloadManager);
});

function trigger_pb_cleanup(expected)
{
  var obs = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  var cancel = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
  cancel.data = false;
  obs.notifyObservers(cancel, "last-pb-context-exiting", null);
  do_check_eq(expected, cancel.data);
  if (!expected)
    obs.notifyObservers(cancel, "last-pb-context-exited", null);
}

function run_test() {
  if (oldDownloadManagerDisabled()) {
    return;
  }

  function finishTest() {
    // Cancel Download-G
    dlG.cancel();
    dlG.remove();
    dm.cleanUp();
    dm.cleanUpPrivate();
    do_check_eq(dm.activeDownloadCount, 0);
    do_check_eq(dm.activePrivateDownloadCount, 0);

    dm.removeListener(listener);
    httpserv.stop(do_test_finished);

    // Unregister the mock so we do not leak
    MockRegistrar.unregister(mockCID);
  }

  do_test_pending();
  let httpserv = new HttpServer();
  httpserv.registerDirectory("/file/", do_get_cwd());
  httpserv.registerPathHandler("/noresume", function (meta, response) {
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Accept-Ranges", "none", false);
    response.write("foo");
  });
  httpserv.start(-1);

  let tmpDir = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties).
               get("TmpD", Ci.nsIFile);

  // make sure we're starting with an empty DB
  do_check_eq(dm.activeDownloadCount, 0);

  let listener = {
    onDownloadStateChange: function(aState, aDownload)
    {
      switch (aDownload.state) {
        case dm.DOWNLOAD_QUEUED:
        case dm.DOWNLOAD_DOWNLOADING:
          if (aDownload.targetFile.equals(dlD.targetFile)) {
            // Sanity check: Download-D must not be resumable
            do_check_false(dlD.resumable);

            // Cancel the transition
            promptService.sayCancel();
            trigger_pb_cleanup(true);
            do_check_true(promptService.wasCalled());
            do_check_eq(dm.activePrivateDownloadCount, 1);

            promptService.sayProceed();
            trigger_pb_cleanup(false);
            do_check_true(promptService.wasCalled());
            do_check_eq(dm.activePrivateDownloadCount, 0);
            do_check_eq(dlD.state, dm.DOWNLOAD_CANCELED);

            // Create Download-E
            dlE = addDownload(httpserv, {
              isPrivate: true,
              targetFile: fileE,
              sourceURI: downloadESource,
              downloadName: downloadEName
            });

            // Wait for Download-E to start
          } else if (aDownload.targetFile.equals(dlE.targetFile)) {
            // Sanity check: Download-E must be resumable
            do_check_true(dlE.resumable);

            promptService.sayCancel();
            trigger_pb_cleanup(true);
            do_check_true(promptService.wasCalled());
            do_check_eq(dm.activePrivateDownloadCount, 1);

            promptService.sayProceed();
            trigger_pb_cleanup(false);
            do_check_true(promptService.wasCalled());
            do_check_eq(dm.activePrivateDownloadCount, 0);
            do_check_eq(dlE.state, dm.DOWNLOAD_CANCELED);

            // Create Download-F
            dlF = addDownload(httpserv, {
              isPrivate: true,
              targetFile: fileF,
              sourceURI: downloadFSource,
              downloadName: downloadFName
            });

            // Wait for Download-F to start
          } else if (aDownload.targetFile.equals(dlF.targetFile)) {
            // Sanity check: Download-F must be resumable
            do_check_true(dlF.resumable);
            dlF.pause();

          } else if (aDownload.targetFile.equals(dlG.targetFile)) {
            // Sanity check: Download-G must not be resumable
            do_check_false(dlG.resumable);

            promptService.sayCancel();
            trigger_pb_cleanup(false);
            do_check_false(promptService.wasCalled());
            do_check_eq(dm.activeDownloadCount, 1);
            do_check_eq(dlG.state, dm.DOWNLOAD_DOWNLOADING);
            finishTest();
          }
          break;

        case dm.DOWNLOAD_PAUSED:
          if (aDownload.targetFile.equals(dlF.targetFile)) {
            promptService.sayProceed();
            trigger_pb_cleanup(false);
            do_check_true(promptService.wasCalled());
            do_check_eq(dm.activePrivateDownloadCount, 0);
            do_check_eq(dlF.state, dm.DOWNLOAD_CANCELED);

            // Create Download-G
            dlG = addDownload(httpserv, {
              isPrivate: false,
              targetFile: fileG,
              sourceURI: downloadGSource,
              downloadName: downloadGName
            });

            // Wait for Download-G to start
          }
          break;
      }
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { }
  };

  dm.addPrivacyAwareListener(listener);

  const PORT = httpserv.identity.primaryPort;

  // properties of Download-D
  const downloadDSource = "http://localhost:" + PORT + "/noresume";
  const downloadDDest = "download-file-D";
  const downloadDName = "download-D";

  // properties of Download-E
  const downloadESource = "http://localhost:" + PORT + "/file/head_download_manager.js";
  const downloadEDest = "download-file-E";
  const downloadEName = "download-E";

  // properties of Download-F
  const downloadFSource = "http://localhost:" + PORT + "/file/head_download_manager.js";
  const downloadFDest = "download-file-F";
  const downloadFName = "download-F";

  // properties of Download-G
  const downloadGSource = "http://localhost:" + PORT + "/noresume";
  const downloadGDest = "download-file-G";
  const downloadGName = "download-G";

  // Create all target files
  let fileD = tmpDir.clone();
  fileD.append(downloadDDest);
  fileD.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
  let fileE = tmpDir.clone();
  fileE.append(downloadEDest);
  fileE.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
  let fileF = tmpDir.clone();
  fileF.append(downloadFDest);
  fileF.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
  let fileG = tmpDir.clone();
  fileG.append(downloadGDest);
  fileG.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);

  // Create Download-D
  let dlD = addDownload(httpserv, {
    isPrivate: true,
    targetFile: fileD,
    sourceURI: downloadDSource,
    downloadName: downloadDName
  });

  let dlE, dlF, dlG;

  // wait for Download-D to start
}
