/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Application Update Service.
 *
 * The Initial Developer of the Original Code is
 * Robert Strong <robert.bugzilla@gmail.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

/* General nsIUpdateCheckListener onload and onerror error code and statusText
   Tests */

// Errors tested:
// 200, 403, 404, 500, 2152398849, 2152398862, 2152398864, 2152398867,
// 2152398868, 2152398878, 2152398890, 2152398919, 2152398920, 2153390069,
// 2152398918, 2152398861

var gNextRunFunc;
var gExpectedStatusCode;
var gExpectedStatusText;

function run_test() {
  do_test_pending();
  do_register_cleanup(end_test);
  removeUpdateDirsAndFiles();
  setUpdateURLOverride();
  standardInit();
  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  do_timeout(0, run_test_pt1);
}

function end_test() {
  cleanUp();
}

// Callback function used by the custom XMLHttpRequest implementation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent() {
  gXHR.status = gExpectedStatusCode;
  var e = { target: gXHR };
  gXHR.onload.handleEvent(e);
}

// Helper functions for testing nsIUpdateCheckListener statusText
function run_test_helper(aNextRunFunc, aExpectedStatusCode, aMsg) {
  gStatusCode = null;
  gStatusText = null;
  gCheckFunc = check_test_helper;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatusCode = aExpectedStatusCode;
  logTestInfo(aMsg, Components.stack.caller);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper() {
  do_check_eq(gStatusCode, gExpectedStatusCode);
  var expectedStatusText = getStatusText(gExpectedStatusCode);
  do_check_eq(gStatusText, expectedStatusText);
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
  logTestInfo("testing default onerror error message");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt1() {
  do_check_eq(gStatusCode, gExpectedStatusCode);
  var expectedStatusText = getStatusText(404);
  do_check_eq(gStatusText, expectedStatusText);
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
  run_test_helper(run_test_pt7, AUS_Cr.NS_BINDING_FAILED,
                  "testing failed (unknown reason)");
}

// connection timed out - NS_ERROR_NET_TIMEOUT (2152398862)
function run_test_pt7() {
  run_test_helper(run_test_pt8, AUS_Cr.NS_ERROR_NET_TIMEOUT,
                  "testing connection timed out");
}

// network offline - NS_ERROR_OFFLINE (2152398864)
function run_test_pt8() {
  run_test_helper(run_test_pt9, AUS_Cr.NS_ERROR_OFFLINE,
                  "testing network offline");
}

// port not allowed - NS_ERROR_PORT_ACCESS_NOT_ALLOWED (2152398867)
function run_test_pt9() {
  run_test_helper(run_test_pt10, AUS_Cr.NS_ERROR_PORT_ACCESS_NOT_ALLOWED,
                  "testing port not allowed");
}

// no data was received - NS_ERROR_NET_RESET (2152398868)
function run_test_pt10() {
  run_test_helper(run_test_pt11, AUS_Cr.NS_ERROR_NET_RESET,
                  "testing no data was received");
}

// update server not found - NS_ERROR_UNKNOWN_HOST (2152398878)
function run_test_pt11() {
  run_test_helper(run_test_pt12, AUS_Cr.NS_ERROR_UNKNOWN_HOST,
                  "testing update server not found");
}

// proxy server not found - NS_ERROR_UNKNOWN_PROXY_HOST (2152398890)
function run_test_pt12() {
  run_test_helper(run_test_pt13, AUS_Cr.NS_ERROR_UNKNOWN_PROXY_HOST,
                  "testing proxy server not found");
}

// data transfer interrupted - NS_ERROR_NET_INTERRUPT (2152398919)
function run_test_pt13() {
  run_test_helper(run_test_pt14, AUS_Cr.NS_ERROR_NET_INTERRUPT,
                  "testing data transfer interrupted");
}

// proxy server connection refused - NS_ERROR_PROXY_CONNECTION_REFUSED (2152398920)
function run_test_pt14() {
  run_test_helper(run_test_pt15, AUS_Cr.NS_ERROR_PROXY_CONNECTION_REFUSED,
                  "testing proxy server connection refused");
}

// server certificate expired - 2153390069
function run_test_pt15() {
  run_test_helper(run_test_pt16, 2153390069,
                  "testing server certificate expired");
}

// network is offline - NS_ERROR_DOCUMENT_NOT_CACHED (2152398918)
function run_test_pt16() {
  run_test_helper(run_test_pt17, AUS_Cr.NS_ERROR_DOCUMENT_NOT_CACHED,
                  "testing network is offline");
}

// connection refused - NS_ERROR_CONNECTION_REFUSED (2152398861)
function run_test_pt17() {
  run_test_helper(do_test_finished, AUS_Cr.NS_ERROR_CONNECTION_REFUSED,
                  "testing connection refused");
}
