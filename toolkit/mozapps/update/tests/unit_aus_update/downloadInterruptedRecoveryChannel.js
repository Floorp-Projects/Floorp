/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General MAR File Download Tests for the ChannelDownloader */

function run_test() {
  setupTestCommon();

  debugDump("testing recovery of mar download after pause and resume");

  // This test assumes speculative connections enabled.
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 6);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.http.speculative-parallel-limit");
  });

  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);
  start_httpserver({slowDownload: true});
  setUpdateURL(gURLData + gHTTPHandlerPath);
  standardInit();

  let patches = getRemotePatchString({});
  let updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);

  gUpdates = null;
  gUpdateCount = null;
  gStatusResult = null;
  gCheckFunc = downloadUpdate;
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

// The HttpServer must be stopped before calling do_test_finished
function finish_test() {
  stop_httpserver(doTestFinish);
}

class TestDownloadListener {
  constructor() {
    this.stoppedOnce = false;
  }

  onStartRequest(aRequest, aContext) { }

  onStatus(aRequest, aContext, aStatus, aStatusText) {
    if (!this.stoppedOnce) {
      Assert.equal(aStatus, Cr.NS_NET_STATUS_WAITING_FOR,
                   "the download status" + MSG_SHOULD_EQUAL);
      executeSoon(() => gAUS.pauseDownload());
    }
  }

  onProgress(aRequest, aContext, aProgress, aMaxProgress) { }

  onStopRequest(aRequest, aContext, aStatus) {
    if (!this.stoppedOnce) {
      // The first time we stop, it should be because we paused the download.
      this.stoppedOnce = true;
      Assert.equal(gBestUpdate.state, STATE_DOWNLOADING,
                   "the update state" + MSG_SHOULD_EQUAL);
      Assert.equal(aStatus, Cr.NS_BINDING_ABORTED,
                   "the download status" + MSG_SHOULD_EQUAL);
      executeSoon(resumeDownload);
    } else {
      // The second time we stop, it should be because the download is done.
      Assert.equal(gBestUpdate.state, STATE_PENDING,
                   "the update state" + MSG_SHOULD_EQUAL);
      Assert.equal(aStatus, Cr.NS_OK,
                   "the download status" + MSG_SHOULD_EQUAL);

      // Cleaning up the active update along with reloading the update manager
      // in doTestFinish will prevent writing the update xml files during
      // shutdown.
      gUpdateManager.cleanupActiveUpdate();
      executeSoon(waitForUpdateXMLFiles);
    }
  }

  QueryInterface(iid) {
    if (iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsIProgressEventSink)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}

let gBestUpdate;
let gListener = new TestDownloadListener();

function downloadUpdate() {
  Assert.equal(gUpdateCount, 1, "the update count" + MSG_SHOULD_EQUAL);
  gBestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  let state = gAUS.downloadUpdate(gBestUpdate, false);
  if (state == STATE_NONE || state == STATE_FAILED) {
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  }
  gAUS.addDownloadListener(gListener);
}

function resumeDownload() {
  gSlowDownloadContinue = true;

  let state = gAUS.downloadUpdate(gBestUpdate, false);
  if (state == STATE_NONE || state == STATE_FAILED) {
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  }

  // Resuming creates a new Downloader, and thus drops registered listeners.
  gAUS.addDownloadListener(gListener);
}

/**
 * Called after the call to waitForUpdateXMLFiles finishes.
 */
function waitForUpdateXMLFilesFinished() {
  stop_httpserver(doTestFinish);
}
