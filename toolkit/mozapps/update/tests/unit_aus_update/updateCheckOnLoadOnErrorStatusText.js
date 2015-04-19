/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Errors tested:
// 200, 403, 404, 500, 2152398849, 2152398862, 2152398864, 2152398867,
// 2152398868, 2152398878, 2152398890, 2152398919, 2152398920, 2153390069,
// 2152398918, 2152398861

var gNextRunFunc;
var gExpectedStatusCode;
var gExpectedStatusText;

function run_test() {
  setupTestCommon();

  debugDump("testing nsIUpdateCheckListener onload and onerror error code " +
            "and statusText values");

  setUpdateURLOverride();
  standardInit();
  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  do_execute_soon(run_test_pt1);
}

// Callback function used by the custom XMLHttpRequest implementation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent(aXHR) {
  aXHR.status = gExpectedStatusCode;
  let e = { target: aXHR };
  aXHR.onload(e);
}

// Helper functions for testing nsIUpdateCheckListener statusText
function run_test_helper(aNextRunFunc, aExpectedStatusCode, aMsg) {
  gStatusCode = null;
  gStatusText = null;
  gCheckFunc = check_test_helper;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatusCode = aExpectedStatusCode;
  debugDump(aMsg, Components.stack.caller);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper() {
  Assert.equal(gStatusCode, gExpectedStatusCode,
               "the download status code" + MSG_SHOULD_EQUAL);
  let expectedStatusText = getStatusText(gExpectedStatusCode);
  Assert.equal(gStatusText, expectedStatusText,
               "the update status text" + MSG_SHOULD_EQUAL);
  gNextRunFunc();
}

/**
 * The following tests use a custom XMLHttpRequest to return the status codes
 */

// default onerror error message (error code 399 is not defined)
function run_test_pt1() {
  gStatusCode = null;
  gStatusText = null;
  gCheckFunc = check_test_pt1;
  gExpectedStatusCode = 399;
  debugDump("testing default onerror error message");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt1() {
  Assert.equal(gStatusCode, gExpectedStatusCode,
               "the download status code" + MSG_SHOULD_EQUAL);
  let expectedStatusText = getStatusText(404);
  Assert.equal(gStatusText, expectedStatusText,
               "the update status text" + MSG_SHOULD_EQUAL);
  run_test_pt2();
}

// file malformed - 200
function run_test_pt2() {
  run_test_helper(run_test_pt3, 200,
                  "testing file malformed");
}

// access denied - 403
function run_test_pt3() {
  run_test_helper(run_test_pt4, 403,
                  "testing access denied");
}

// file not found - 404
function run_test_pt4() {
  run_test_helper(run_test_pt5, 404,
                  "testing file not found");
}

// internal server error - 500
function run_test_pt5() {
  run_test_helper(run_test_pt6, 500,
                  "testing internal server error");
}

// failed (unknown reason) - NS_BINDING_FAILED (2152398849)
function run_test_pt6() {
  run_test_helper(run_test_pt7, Cr.NS_BINDING_FAILED,
                  "testing failed (unknown reason)");
}

// connection timed out - NS_ERROR_NET_TIMEOUT (2152398862)
function run_test_pt7() {
  run_test_helper(run_test_pt8, Cr.NS_ERROR_NET_TIMEOUT,
                  "testing connection timed out");
}

// network offline - NS_ERROR_OFFLINE (2152398864)
function run_test_pt8() {
  run_test_helper(run_test_pt9, Cr.NS_ERROR_OFFLINE,
                  "testing network offline");
}

// port not allowed - NS_ERROR_PORT_ACCESS_NOT_ALLOWED (2152398867)
function run_test_pt9() {
  run_test_helper(run_test_pt10, Cr.NS_ERROR_PORT_ACCESS_NOT_ALLOWED,
                  "testing port not allowed");
}

// no data was received - NS_ERROR_NET_RESET (2152398868)
function run_test_pt10() {
  run_test_helper(run_test_pt11, Cr.NS_ERROR_NET_RESET,
                  "testing no data was received");
}

// update server not found - NS_ERROR_UNKNOWN_HOST (2152398878)
function run_test_pt11() {
  run_test_helper(run_test_pt12, Cr.NS_ERROR_UNKNOWN_HOST,
                  "testing update server not found");
}

// proxy server not found - NS_ERROR_UNKNOWN_PROXY_HOST (2152398890)
function run_test_pt12() {
  run_test_helper(run_test_pt13, Cr.NS_ERROR_UNKNOWN_PROXY_HOST,
                  "testing proxy server not found");
}

// data transfer interrupted - NS_ERROR_NET_INTERRUPT (2152398919)
function run_test_pt13() {
  run_test_helper(run_test_pt14, Cr.NS_ERROR_NET_INTERRUPT,
                  "testing data transfer interrupted");
}

// proxy server connection refused - NS_ERROR_PROXY_CONNECTION_REFUSED (2152398920)
function run_test_pt14() {
  run_test_helper(run_test_pt15, Cr.NS_ERROR_PROXY_CONNECTION_REFUSED,
                  "testing proxy server connection refused");
}

// server certificate expired - 2153390069
function run_test_pt15() {
  run_test_helper(run_test_pt16, 2153390069,
                  "testing server certificate expired");
}

// network is offline - NS_ERROR_DOCUMENT_NOT_CACHED (2152398918)
function run_test_pt16() {
  run_test_helper(run_test_pt17, Cr.NS_ERROR_DOCUMENT_NOT_CACHED,
                  "testing network is offline");
}

// connection refused - NS_ERROR_CONNECTION_REFUSED (2152398861)
function run_test_pt17() {
  run_test_helper(doTestFinish, Cr.NS_ERROR_CONNECTION_REFUSED,
                  "testing connection refused");
}
