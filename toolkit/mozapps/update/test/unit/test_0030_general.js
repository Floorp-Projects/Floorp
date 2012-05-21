/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General MAR File Download Tests */

var gNextRunFunc;
var gStatusResult;
var gExpectedStatusResult;

function run_test() {
  do_test_pending();
  do_register_cleanup(end_test);
  removeUpdateDirsAndFiles();
  setUpdateURLOverride();
  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  standardInit();
  // The HTTP server is only used for the mar file downloads which is slow
  start_httpserver(URL_PATH);
  do_execute_soon(run_test_pt1);
}

// The nsHttpServer must be stopped before calling do_test_finished
function finish_test() {
  stop_httpserver(do_test_finished);
}

function end_test() {
  cleanUp();
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
  }
  catch(e) {
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

function setResponseBody(aHashFunction, aHashValue) {
  var patches = getRemotePatchString(null, null, aHashFunction, aHashValue);
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
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt3);
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
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt5);
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
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt7);
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
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt9);
}

// mar download with a valid SHA512 hash
function run_test_pt9() {
  setResponseBody("SHA512", SHA512_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid SHA512 hash",
                      AUS_Cr.NS_OK, run_test_pt10);
}

// mar download with an invalid SHA384 hash
function run_test_pt10() {
  setResponseBody("SHA512", SHA512_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid SHA512 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt11);
}

// mar download with the mar not found
function run_test_pt11() {
  var patches = getRemotePatchString(null, URL_HOST + URL_PATH + "/missing.mar");
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("mar download with the mar not found",
                      AUS_Cr.NS_ERROR_UNEXPECTED, finish_test);
}

/* Update download listener - nsIRequestObserver */
const downloadListener = {
  onStartRequest: function DL_onStartRequest(request, context) {
  },

  onProgress: function DL_onProgress(request, context, progress, maxProgress) {
  },

  onStatus: function DL_onStatus(request, context, status, statusText) {
  },

  onStopRequest: function DL_onStopRequest(request, context, status) {
    gStatusResult = status;
    // Use a timeout to allow the request to complete
    do_execute_soon(gCheckFunc);
  },

  QueryInterface: function DL_QueryInterface(iid) {
    if (!iid.equals(AUS_Ci.nsIRequestObserver) &&
        !iid.equals(AUS_Ci.nsIProgressEventSink) &&
        !iid.equals(AUS_Ci.nsISupports))
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};
