/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();

  debugDump("testing mar download with interrupted recovery count exceeded");

  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);
  start_httpserver({errorDownload: true});
  setUpdateURL(gURLData + gHTTPHandlerPath);
  Services.prefs.setIntPref(PREF_APP_UPDATE_SOCKET_MAXERRORS, 2);
  Services.prefs.setIntPref(PREF_APP_UPDATE_RETRYTIMEOUT, 0);
  let patches = getRemotePatchString({});
  let updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  gCheckFunc = updateCheckCompleted;
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

/**
 * Since gCheckFunc value is the updateCheckCompleted function at this stage
 * this is called after the update check completes in updateCheckListener.
 */
function updateCheckCompleted() {
  Assert.equal(gUpdateCount, 1,
               "the update count" + MSG_SHOULD_EQUAL);
  let bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  let state = gAUS.downloadUpdate(bestUpdate, false);
  if (state == STATE_NONE || state == STATE_FAILED) {
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  }
  gAUS.addDownloadListener(downloadListener);
}

/**
 * Called after the download listener onStopRequest is called.
 */
function downloadListenerStop() {
  Assert.equal(gStatusResult, Cr.NS_ERROR_UNEXPECTED,
               "the download status result" + MSG_SHOULD_EQUAL);
  gAUS.removeDownloadListener(downloadListener);
  executeSoon(waitForUpdateXMLFiles);
}

/**
 * Called after the call to waitForUpdateXMLFiles finishes.
 */
function waitForUpdateXMLFilesFinished() {
  // The HttpServer must be stopped before calling do_test_finished
  stop_httpserver(doTestFinish);
}
