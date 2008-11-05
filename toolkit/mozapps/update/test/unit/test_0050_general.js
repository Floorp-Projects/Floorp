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

const DIR_DATA = "data"
const URL_PREFIX = "http://localhost:4444/" + DIR_DATA + "/";

const PREF_APP_UPDATE_URL_OVERRIDE = "app.update.url.override";

const URI_UPDATES_PROPERTIES = "chrome://mozapps/locale/update/updates.properties";
const gUpdateBundle = AUS_Cc["@mozilla.org/intl/stringbundle;1"]
                       .getService(AUS_Ci.nsIStringBundleService)
                       .createBundle(URI_UPDATES_PROPERTIES);

var gStatus;
var gStatusText;
var gExpectedStatusText;
var gCheckFunc;
var gNextRunFunc;

function run_test() {
  do_test_pending();
  startAUS();
  do_timeout(0, "run_test_pt1()");
}

function end_test() {
  stop_httpserver();
  do_test_finished();
}

// Returns human readable status text from the updates.properties bundle
function getStatusText(aErrCode) {
  try {
    return gUpdateBundle.GetStringFromName("check_error-" + aErrCode);
  }
  catch (e) {
  }
  return null;
}

// Custom error httpd handler used to return error codes we can't simulate
function httpdErrorHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, gExpectedStatus, "Error");
}

// Helper functions for testing nsIUpdateCheckListener onerror error statusText
function run_test_helper(aUpdateXML, aMsg, aExpectedStatus,
                         aExpectedStatusText, aNextRunFunc) {
  gStatus = null;
  gStatusText = null;
  gCheckFunc = check_test_helper;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatus = aExpectedStatus;
  gExpectedStatusText = aExpectedStatusText;
  var url = URL_PREFIX + aUpdateXML;
  dump("Testing: " + aMsg + " - " + url + "\n");
  gPrefs.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper() {
  do_check_eq(gStatus, gExpectedStatus);
  do_check_eq(gStatusText, gExpectedStatusText);
  gNextRunFunc();
}

/**
 * The following tests do not use the http server
 */

// network is offline
function run_test_pt1() {
  toggleOffline(true);
  run_test_helper("aus-0050_general-1.xml", "network is offline",
                  0, getStatusText("2152398918"), run_test_pt2);
}

// connection refused
function run_test_pt2() {
  toggleOffline(false);
  run_test_helper("aus-0050_general-2.xml", "connection refused",
                  0, getStatusText("2152398861"), run_test_pt3);
}

/**
 * The following tests use the codes returned by the http server
 */

// file malformed
function run_test_pt3() {
  start_httpserver(DIR_DATA);
  run_test_helper("aus-0050_general-3.xml", "file malformed",
                  200, getStatusText("200"), run_test_pt4);
}

// file not found
function run_test_pt4() {
  run_test_helper("aus-0050_general-4.xml", "file not found",
                  404, getStatusText("404"), run_test_pt5);
}

// internal server error
function run_test_pt5() {
  gTestserver.registerContentType("sjs", "sjs");
  run_test_helper("aus-0050_general-5.sjs", "internal server error",
                  500, getStatusText("500"), run_test_pt6);
}

/**
 * The following tests use a custom error handler to return codes from the http
 * server
 */

// access denied
function run_test_pt6() {
  gTestserver.registerErrorHandler(404, httpdErrorHandler);
  run_test_helper("aus-0050_general-6.xml", "access denied",
                  403, getStatusText("403"), run_test_pt7);
}

// default onerror error message (error code 399 is not defined)
function run_test_pt7() {
  run_test_helper("aus-0050_general-7.xml", "default onerror error message",
                  399, getStatusText("404"), end_test);
}


// Update check listener
const updateCheckListener = {
  onProgress: function(request, position, totalSize) {
  },

  onCheckComplete: function(request, updates, updateCount) {
    dump("onCheckComplete request.status = " + request.status + "\n\n");
    // Use a timeout to allow the XHR to complete
    do_timeout(0, "gCheckFunc()");
  },

  onError: function(request, update) {
    gStatus = request.status;
    gStatusText = update.statusText;
    dump("onError: request.status = " + gStatus + ", update.statusText = " + gStatusText + "\n\n");
    // Use a timeout to allow the XHR to complete
    do_timeout(0, "gCheckFunc()");
  },

  QueryInterface: function(aIID) {
    if (!aIID.equals(AUS_Ci.nsIUpdateCheckListener) &&
        !aIID.equals(AUS_Ci.nsISupports))
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};
