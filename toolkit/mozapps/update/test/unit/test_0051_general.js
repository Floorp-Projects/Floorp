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

/* General nsIUpdateCheckListener onload error code statusText Tests */

// Errors tested:
// 2152398849, 2152398862, 2152398864, 2152398867, 2152398868, 2152398878,
// 2152398890, 2152398919, 2152398920, default (404)

const DIR_DATA = "data"
const URL_PREFIX = "http://localhost:4444/" + DIR_DATA + "/";

const PREF_APP_UPDATE_URL_OVERRIDE = "app.update.url.override";

const URI_UPDATES_PROPERTIES = "chrome://mozapps/locale/update/updates.properties";
const gUpdateBundle = AUS_Cc["@mozilla.org/intl/stringbundle;1"]
                       .getService(AUS_Ci.nsIStringBundleService)
                       .createBundle(URI_UPDATES_PROPERTIES);

var gStatusCode;
var gStatusText;
var gExpectedStatusCode;
var gExpectedStatusText;
var gCheckFunc;
var gNextRunFunc;

function run_test() {
  do_test_pending();
  startAUS();
  overrideXHR(callHandleEvent);
  do_timeout(0, "run_test_pt1()");
}

function end_test() {
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

// Callback function used by the custom XMLHttpRequest implemetation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent() {
  gXHR.status = gExpectedStatusCode;
  var e = { target: gXHR };
  gXHR.onload.handleEvent(e);
}

// Helper functions for testing nsIUpdateCheckListener onload error statusTexts
function run_test_helper(aUpdateXML, aMsg, aExpectedStatusCode,
                         aExpectedStatusText, aNextRunFunc) {
  gStatusCode = null;
  gStatusText = null;
  gCheckFunc = check_test_helper;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatusCode = aExpectedStatusCode;
  gExpectedStatusText = aExpectedStatusText;
  var url = URL_PREFIX + aUpdateXML;
  dump("Testing: " + aMsg + " - " + url + "\n");
  gPrefs.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper() {
  do_check_eq(gStatusCode, gExpectedStatusCode);
  do_check_eq(gStatusText, gExpectedStatusText);
  gNextRunFunc();
}

/**
 * The following tests use a custom XMLHttpRequest to return the status codes
 */

// failed (unknown reason)
function run_test_pt1() {
  run_test_helper("aus-0051_general-1.xml", "failed (unknown reason)",
                  2152398849, getStatusText("2152398849"), run_test_pt2);
}

// connection timed out
function run_test_pt2() {
  run_test_helper("aus-0051_general-2.xml", "connection timed out",
                  2152398862, getStatusText("2152398862"), run_test_pt3);
}

// network offline
function run_test_pt3() {
  run_test_helper("aus-0051_general-3.xml", "network offline",
                  2152398864, getStatusText("2152398864"), run_test_pt4);
}

// port not allowed
function run_test_pt4() {
  run_test_helper("aus-0051_general-4.xml", "port not allowed",
                  2152398867, getStatusText("2152398867"), run_test_pt5);
}

// no data was received
function run_test_pt5() {
  run_test_helper("aus-0051_general-5.xml", "no data was received",
                  2152398868, getStatusText("2152398868"), run_test_pt6);
}

// update server not found
function run_test_pt6() {
  run_test_helper("aus-0051_general-6.xml", "update server not found",
                  2152398878, getStatusText("2152398878"), run_test_pt7);
}

// proxy server not found
function run_test_pt7() {
  run_test_helper("aus-0051_general-7.xml", "proxy server not found",
                  2152398890, getStatusText("2152398890"), run_test_pt8);
}

// data transfer interrupted
function run_test_pt8() {
  run_test_helper("aus-0051_general-8.xml", "data transfer interrupted",
                  2152398919, getStatusText("2152398919"), run_test_pt9);
}

// proxy server connection refused
function run_test_pt9() {
  run_test_helper("aus-0051_general-9.xml", "proxy server connection refused",
                  2152398920, getStatusText("2152398920"), run_test_pt10);
}

// server certificate expired
function run_test_pt10() {
  run_test_helper("aus-0051_general-10.xml", "server certificate expired",
                  2153390069, getStatusText("2153390069"), run_test_pt11);
}

// default onload error message (error code 1152398920 is not defined)
function run_test_pt11() {
  run_test_helper("aus-0051_general-11.xml", "default onload error message",
                  1152398920, getStatusText("404"), end_test);
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
    gStatusCode = request.status;
    gStatusText = update.statusText;
    dump("onError: request.status = " + gStatusCode + ", update.statusText = " + gStatusText + "\n\n");
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
