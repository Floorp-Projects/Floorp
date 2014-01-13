/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var gNextRunFunc;
var gExpectedStatusResult;

function run_test() {
  // This test needs access to omni.ja to read the update.locale file so don't
  // use a custom directory for the application directory.
  gUseTestAppDir = false;
  setupTestCommon();

  logTestInfo("testing mar download and mar hash verification");

  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);
  // The HTTP server is only used for the mar file downloads since it is slow
  start_httpserver();
  setUpdateURLOverride(gURLData + "update.xml");
  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  standardInit();
  do_execute_soon(run_test_pt1);
}

// The HttpServer must be stopped before calling do_test_finished
function finish_test() {
  stop_httpserver(doTestFinish);
}

// Callback function used by the custom XMLHttpRequest implementation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent() {
  gXHR.status = 400;
  gXHR.responseText = gResponseBody;
  try {
    var parser = AUS_Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(AUS_Ci.nsIDOMParser);
    gXHR.responseXML = parser.parseFromString(gResponseBody, "application/xml");
  } catch(e) {
  }
  var e = { target: gXHR };
  gXHR.onload(e);
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
  logTestInfo(aMsg, Components.stack.caller);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_pt1_1() {
  do_check_eq(gUpdateCount, 1);
  gCheckFunc = check_test_helper_pt1_2;
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  var state = gAUS.downloadUpdate(bestUpdate, false);
  if (state == STATE_NONE || state == STATE_FAILED)
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  gAUS.addDownloadListener(downloadListener);
}

function check_test_helper_pt1_2() {
  do_check_eq(gStatusResult, gExpectedStatusResult);
  gAUS.removeDownloadListener(downloadListener);
  gNextRunFunc();
}

// The following 3 functions are a workaround for GONK due to Bug 828858 and
// can be removed after it is fixed and the callers are changed to use the
// regular helper functions.
function run_test_helper_bug828858_pt1(aMsg, aExpectedStatusResult, aNextRunFunc) {
  gUpdates = null;
  gUpdateCount = null;
  gStatusResult = null;
  gCheckFunc = check_test_helper_bug828858_pt1_1;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatusResult = aExpectedStatusResult;
  logTestInfo(aMsg, Components.stack.caller);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_bug828858_pt1_1() {
  do_check_eq(gUpdateCount, 1);
  gCheckFunc = check_test_helper_bug828858_pt1_2;
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  var state = gAUS.downloadUpdate(bestUpdate, false);
  if (state == STATE_NONE || state == STATE_FAILED)
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  gAUS.addDownloadListener(downloadListener);
}

function check_test_helper_bug828858_pt1_2() {
  if (gStatusResult == AUS_Cr.NS_ERROR_CONTENT_CORRUPTED) {
    do_check_eq(gStatusResult, AUS_Cr.NS_ERROR_CONTENT_CORRUPTED);
  } else {
    do_check_eq(gStatusResult, gExpectedStatusResult);
  }
  gAUS.removeDownloadListener(downloadListener);
  gNextRunFunc();
}

function setResponseBody(aHashFunction, aHashValue, aSize) {
  var patches = getRemotePatchString(null, null,
                                     aHashFunction, aHashValue, aSize);
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
}

// mar download with a valid MD5 hash
function run_test_pt1() {
  setResponseBody("MD5", MD5_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid MD5 hash",
                      AUS_Cr.NS_OK, run_test_pt2);
}

// mar download with an invalid MD5 hash
function run_test_pt2() {
  setResponseBody("MD5", MD5_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid MD5 hash",
                      AUS_Cr.NS_ERROR_CORRUPTED_CONTENT, run_test_pt3);
}

// mar download with a valid SHA1 hash
function run_test_pt3() {
  setResponseBody("SHA1", SHA1_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid SHA1 hash",
                      AUS_Cr.NS_OK, run_test_pt4);
}

// mar download with an invalid SHA1 hash
function run_test_pt4() {
  setResponseBody("SHA1", SHA1_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid SHA1 hash",
                      AUS_Cr.NS_ERROR_CORRUPTED_CONTENT, run_test_pt5);
}

// mar download with a valid SHA256 hash
function run_test_pt5() {
  setResponseBody("SHA256", SHA256_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid SHA256 hash",
                      AUS_Cr.NS_OK, run_test_pt6);
}

// mar download with an invalid SHA256 hash
function run_test_pt6() {
  setResponseBody("SHA256", SHA256_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid SHA256 hash",
                      AUS_Cr.NS_ERROR_CORRUPTED_CONTENT, run_test_pt7);
}

// mar download with a valid SHA384 hash
function run_test_pt7() {
  setResponseBody("SHA384", SHA384_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid SHA384 hash",
                      AUS_Cr.NS_OK, run_test_pt8);
}

// mar download with an invalid SHA384 hash
function run_test_pt8() {
  setResponseBody("SHA384", SHA384_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid SHA384 hash",
                      AUS_Cr.NS_ERROR_CORRUPTED_CONTENT, run_test_pt9);
}

// mar download with a valid SHA512 hash
function run_test_pt9() {
  setResponseBody("SHA512", SHA512_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid SHA512 hash",
                      AUS_Cr.NS_OK, run_test_pt10);
}

// mar download with an invalid SHA512 hash
function run_test_pt10() {
  setResponseBody("SHA512", SHA512_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid SHA512 hash",
                      AUS_Cr.NS_ERROR_CORRUPTED_CONTENT, run_test_pt11);
}

// mar download with the mar not found
function run_test_pt11() {
  var patches = getRemotePatchString(null, gURLData + "missing.mar");
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("mar download with the mar not found",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt12);
}

// mar download with a valid MD5 hash but invalid file size
function run_test_pt12() {
  const arbitraryFileSize = 1024000;
  setResponseBody("MD5", MD5_HASH_SIMPLE_MAR, arbitraryFileSize);
  if (IS_TOOLKIT_GONK) {
    // There seems to be a race on the web server side when the patchFile is
    // stored on the SDCard. Sometimes, the webserver will serve up an error
    // 416 and the contents of the file, and sometimes it will serve up an error
    // 200 and no contents. This can cause either NS_ERROR_UNEXPECTED or
    // NS_ERROR_CONTENT_CORRUPTED.
    // Bug 828858 was filed to follow up on this issue.
    run_test_helper_bug828858_pt1("mar download with a valid MD5 hash but invalid file size",
                                  AUS_Cr.NS_ERROR_UNEXPECTED, finish_test);
  } else {
    run_test_helper_pt1("mar download with a valid MD5 hash but invalid file size",
                        AUS_Cr.NS_ERROR_UNEXPECTED, finish_test);
  }
}
