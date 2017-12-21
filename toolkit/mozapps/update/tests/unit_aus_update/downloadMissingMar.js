/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  // The network code that downloads the mar file accesses the profile to cache
  // the download, but the profile is only available after calling
  // do_get_profile in xpcshell tests. This prevents an error from being logged.
  do_get_profile();

  setupTestCommon();

  debugDump("testing invalid size mar download");

  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  standardInit();
  executeSoon(run_test_pt1);
}

// mar download with the mar not found
function run_test_pt1() {
  let patchProps = {url: gURLData + "missing.mar"};
  let patches = getRemotePatchString(patchProps);
  let updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  gUpdates = null;
  gUpdateCount = null;
  gStatusResult = null;
  gCheckFunc = check_test_helper_pt1_1;
  debugDump("mar download with the mar not found");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_pt1_1() {
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
