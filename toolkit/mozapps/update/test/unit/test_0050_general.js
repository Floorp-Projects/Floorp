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

/* General nsIUpdateCheckListener onerror error code and statusText Tests */

// Errors tested:
// 200, 403, 404, 500, 2152398861, 2152398918, default (200)

var gNextRunFunc;
var gExpectedStatusCode;
var gExpectedStatusText;

function run_test() {
  do_test_pending();
  removeUpdateDirsAndFiles();
  startAUS();
  startUpdateChecker();
  getPrefBranch().setCharPref(PREF_APP_UPDATE_URL_OVERRIDE,
                              URL_HOST + "update.xml");
  do_timeout(0, "run_test_pt1()");
}

function end_test() {
  stop_httpserver(do_test_finished);
  cleanUp();
}

// Custom error httpd handler used to return error codes we can't simulate
function httpdErrorHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, gExpectedStatusCode, "Error");
}

// Helper functions for testing nsIUpdateCheckListener onerror error statusText
function run_test_helper(aMsg, aExpectedStatusCode, aExpectedStatusTextCode,
                         aNextRunFunc) {
  gStatusCode = null;
  gStatusText = null;
  gCheckFunc = check_test_helper;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatusCode = gResponseStatusCode = aExpectedStatusCode;
  gExpectedStatusText = getStatusText(aExpectedStatusTextCode);
  dump("Testing: " + aMsg + "\n");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper() {
  do_check_eq(gStatusCode, gExpectedStatusCode);
  do_check_eq(gStatusText, gExpectedStatusText);
  gNextRunFunc();
}

/**
 * The following tests do not use the http server
 */

// network is offline
function run_test_pt1() {
  toggleOffline(true);
  run_test_helper("run_test_pt1 - network is offline",
                  0, 2152398918, run_test_pt2);
}

// connection refused
function run_test_pt2() {
  toggleOffline(false);
  run_test_helper("run_test_pt2 - connection refused",
                  0, 2152398861, run_test_pt3);
}

/**
 * The following tests use the codes returned by the http server
 */

// file not found
function run_test_pt3() {
  start_httpserver(DIR_DATA);
  run_test_helper("run_test_pt3 - file not found",
                  404, 404, run_test_pt4);
}

// file malformed
function run_test_pt4() {
  gTestserver.registerPathHandler("/update.xml", pathHandler);
  gResponseBody = "<html><head></head><body></body></html>\n";
  run_test_helper("run_test_pt4 - file malformed",
                  200, 200, run_test_pt5);
}

// internal server error
function run_test_pt5() {
  gResponseBody = "\n";
  run_test_helper("run_test_pt5 - internal server error",
                  500, 500, run_test_pt6);
}

/**
 * The following tests use a custom error handler to return codes from the http
 * server
 */

// access denied
function run_test_pt6() {
  gTestserver.registerErrorHandler(404, httpdErrorHandler);
  run_test_helper("run_test_pt6 - access denied",
                  403, 403, run_test_pt7);
}

// default onerror error message (error code 399 is not defined)
function run_test_pt7() {
  run_test_helper("run_test_pt7 - default onerror error message",
                  399, 404, end_test);
}
