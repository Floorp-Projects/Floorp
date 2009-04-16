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

/* General MAR File Download Tests */

const DIR_DATA = "data"
const URL_PREFIX = "http://localhost:4444/" + DIR_DATA + "/";

const PREF_APP_UPDATE_URL_OVERRIDE = "app.update.url.override";

var gUpdates;
var gUpdateCount;
var gStatus;
var gCheckFunc;
var gNextRunFunc;
var gExpectedResult;

function run_test() {
  do_test_pending();
  startAUS();
  start_httpserver(DIR_DATA);
  do_timeout(0, "run_test_pt1()");
}

function end_test() {
  stop_httpserver(do_test_finished);
}

// Helper function for testing mar downloads that have the correct size
// specified in the update xml.
function run_test_helper_pt1(aUpdateXML, aMsg, aResult, aNextRunFunc) {
  gUpdates = null;
  gUpdateCount = null;
  gStatus = null;
  gCheckFunc = check_test_helper_pt1_1;
  gNextRunFunc = aNextRunFunc;
  gExpectedResult = aResult;
  var url = URL_PREFIX + aUpdateXML;
  dump("Testing: " + aMsg + " - " + url + "\n");
  gPrefs.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_pt1_1() {
  do_check_eq(gUpdateCount, 1);
  gCheckFunc = check_test_helper_pt1_2;
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  var state = gAUS.downloadUpdate(bestUpdate, false);
  if (state == "null" || state == "failed")
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  gAUS.addDownloadListener(downloadListener);
}

function check_test_helper_pt1_2() {
  do_check_eq(gStatus, gExpectedResult);
  gAUS.removeDownloadListener(downloadListener);
  gNextRunFunc();
}

// mar download with a valid MD5 hash
function run_test_pt1() {
  run_test_helper_pt1("aus-0030_general-1.xml",
                      "mar download with a valid MD5 hash",
                      AUS_Cr.NS_OK, run_test_pt2);
}

// mar download with an invalid MD5 hash
function run_test_pt2() {
  run_test_helper_pt1("aus-0030_general-2.xml",
                      "mar download with an invalid MD5 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt3);
}

// mar download with a valid SHA1 hash
function run_test_pt3() {
  run_test_helper_pt1("aus-0030_general-3.xml",
                      "mar download with a valid SHA1 hash",
                      AUS_Cr.NS_OK, run_test_pt4);
}

// mar download with an invalid SHA1 hash
function run_test_pt4() {
  run_test_helper_pt1("aus-0030_general-4.xml",
                      "mar download with an invalid SHA1 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt5);
}

// mar download with a valid SHA256 hash
function run_test_pt5() {
  run_test_helper_pt1("aus-0030_general-5.xml",
                      "mar download with a valid SHA256 hash",
                      AUS_Cr.NS_OK, run_test_pt6);
}

// mar download with an invalid SHA256 hash
function run_test_pt6() {
  run_test_helper_pt1("aus-0030_general-6.xml",
                      "mar download with an invalid SHA256 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt7);
}

// mar download with a valid SHA384 hash
function run_test_pt7() {
  run_test_helper_pt1("aus-0030_general-7.xml",
                      "mar download with a valid SHA384 hash",
                      AUS_Cr.NS_OK, run_test_pt8);
}

// mar download with an invalid SHA384 hash
function run_test_pt8() {
  run_test_helper_pt1("aus-0030_general-8.xml",
                      "mar download with an invalid SHA384 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt9);
}

// mar download with a valid SHA512 hash
function run_test_pt9() {
  run_test_helper_pt1("aus-0030_general-9.xml",
                      "mar download with a valid SHA512 hash",
                      AUS_Cr.NS_OK, run_test_pt10);
}

// mar download with an invalid SHA384 hash
function run_test_pt10() {
  run_test_helper_pt1("aus-0030_general-10.xml",
                      "mar download with an invalid SHA512 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt11);
}

// mar download with the mar not found
function run_test_pt11() {
  run_test_helper_pt1("aus-0030_general-11.xml",
                      "mar download with the mar not found",
                      AUS_Cr.NS_ERROR_UNEXPECTED, end_test);
}

// Update check listener
const updateCheckListener = {
  onProgress: function(request, position, totalSize) {
  },

  onCheckComplete: function(request, updates, updateCount) {
    gUpdateCount = updateCount;
    gUpdates = updates;
    dump("onCheckComplete url = " + request.channel.originalURI.spec + "\n\n");
    // Use a timeout to allow the XHR to complete
    do_timeout(0, "gCheckFunc()");
  },

  onError: function(request, update) {
    dump("onError url = " + request.channel.originalURI.spec + "\n\n");
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

/* Update download listener - nsIRequestObserver */
const downloadListener = {
  onStartRequest: function(request, context) {
  },

  onProgress: function(request, context, progress, maxProgress) {
  },

  onStatus: function(request, context, status, statusText) {
  },

  onStopRequest: function(request, context, status) {
    gStatus = status;
    // Use a timeout to allow the request to complete
    do_timeout(0, "gCheckFunc()");
  },

  QueryInterface: function(iid) {
    if (!iid.equals(AUS_Ci.nsIRequestObserver) &&
        !iid.equals(AUS_Ci.nsIProgressEventSink) &&
        !iid.equals(AUS_Ci.nsISupports))
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};
