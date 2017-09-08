/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var gNextRunFunc;
var gExpectedStatusResult;

function run_test() {
  // The network code that downloads the mar file accesses the profile to cache
  // the download, but the profile is only available after calling
  // do_get_profile in xpcshell tests. This prevents an error from being logged.
  do_get_profile();

  setupTestCommon();

  debugDump("testing invalid mar downloads");

  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  standardInit();
  do_execute_soon(run_test_pt1);
}

// The HttpServer must be stopped before calling do_test_finished
function finish_test() {
  stop_httpserver(doTestFinish);
}

// Helper function for testing mar downloads that have the correct size
// specified in the update xml.
function run_test_helper_pt1(aMsg, aExpectedStatusResult, aNextRunFunc) {
  gUpdates = null;
  gUpdateCount = null;
  gStatusResult = null;
  gCheckFunc = check_test_helper_pt1_1;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatusResult = aExpectedStatusResult;
  debugDump(aMsg, Components.stack.caller);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_pt1_1() {
  Assert.equal(gUpdateCount, 1,
               "the update count" + MSG_SHOULD_EQUAL);
  gCheckFunc = check_test_helper_pt1_2;
  let bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  let state = gAUS.downloadUpdate(bestUpdate, false);
  if (state == STATE_NONE || state == STATE_FAILED) {
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  }
  gAUS.addDownloadListener(downloadListener);
}

function check_test_helper_pt1_2() {
  Assert.equal(gStatusResult, gExpectedStatusResult,
               "the download status result" + MSG_SHOULD_EQUAL);
  gAUS.removeDownloadListener(downloadListener);
  gNextRunFunc();
}

// mar download with the mar not found
function run_test_pt1() {
  let patchProps = {url: gURLData + "missing.mar"};
  let patches = getRemotePatchString(patchProps);
  let updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("mar download with the mar not found",
                      Cr.NS_ERROR_UNEXPECTED, run_test_pt2);
}

// mar download with an invalid file size
function run_test_pt2() {
  let patchProps = {size: "1024000"};
  let patches = getRemotePatchString(patchProps);
  let updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("mar download with an invalid file size",
                      Cr.NS_ERROR_UNEXPECTED, finish_test);
}
