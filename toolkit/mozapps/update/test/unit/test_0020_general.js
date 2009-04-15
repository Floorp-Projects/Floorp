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

/* General Update Check Tests */

const DIR_DATA = "data"
const URL_PREFIX = "http://localhost:4444/" + DIR_DATA + "/";

const PREF_APP_UPDATE_URL_OVERRIDE = "app.update.url.override";

var gUpdates;
var gUpdateCount;
var gStatus;
var gCheckFunc;
var gNextRunFunc;
var gExpectedCount;

function run_test() {
  do_test_pending();
  startAUS();
  start_httpserver(DIR_DATA);
  do_timeout(0, "run_test_pt1()");
}

function end_test() {
  stop_httpserver(do_test_finished);
}

// Helper function for testing update counts returned from an update xml
function run_test_helper_pt1(aUpdateXML, aMsg, aExpectedCount, aNextRunFunc) {
  gUpdates = null;
  gUpdateCount = null;
  gStatus = null;
  gCheckFunc = check_test_helper_pt1;
  gNextRunFunc = aNextRunFunc;
  gExpectedCount = aExpectedCount;
  var url = URL_PREFIX + aUpdateXML;
  dump("Testing: " + aMsg + " - " + url + "\n");
  gPrefs.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_pt1() {
  do_check_eq(gUpdateCount, gExpectedCount);
  gNextRunFunc();
}

// one update available and the update's property values
function run_test_pt1() {
  gUpdates = null;
  gUpdateCount = null;
  gStatus = null;
  gCheckFunc = check_test_pt1;
  var url = URL_PREFIX + "aus-0020_general-1.xml";
  dump("Testing: one update available and the update's property values - " +
       url + "\n");
  gPrefs.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  var defaults = gPrefs.QueryInterface(AUS_Ci.nsIPrefService)
                   .getDefaultBranch(null);
  defaults.setCharPref("app.update.channel", "bogus_channel");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt1() {
  do_check_eq(gUpdateCount, 1);
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  do_check_eq(bestUpdate.type, "minor");
  do_check_eq(bestUpdate.name, "XPCShell Test");
  do_check_eq(bestUpdate.version, "1.1a1pre");
  do_check_eq(bestUpdate.platformVersion, "2.1a1pre");
  do_check_eq(bestUpdate.extensionVersion, "3.1a1pre");
  do_check_eq(bestUpdate.buildID, "20080811053724");
  do_check_eq(bestUpdate.detailsURL, "http://dummydetails/");
  do_check_eq(bestUpdate.licenseURL, "http://dummylicense/");
  do_check_eq(bestUpdate.serviceURL, URL_PREFIX + "aus-0020_general-1.xml?force=1");
  do_check_eq(bestUpdate.channel, "bogus_channel");
  do_check_false(bestUpdate.isCompleteUpdate);
  do_check_false(bestUpdate.isSecurityUpdate);
  do_check_eq(bestUpdate.installDate, 0);
  do_check_eq(bestUpdate.statusText, null);
  // nsIUpdate:state returns an empty string when no action has been performed
  // on an available update
  do_check_eq(bestUpdate.state, "");
  do_check_eq(bestUpdate.errorCode, 0);
  do_check_eq(bestUpdate.patchCount, 2);
  //XXX TODO - test nsIUpdate:serialize

  var type = "complete";
  var patch = bestUpdate.getPatchAt(0);
  do_check_eq(patch.type, type);
  do_check_eq(patch.URL, "http://" + type + "/");
  do_check_eq(patch.hashFunction, "SHA1");
  do_check_eq(patch.hashValue, "98db9dad8e1d80eda7e1170d0187d6f53e477059");
  do_check_eq(patch.size, 9856459);
  // nsIUpdatePatch:state returns the string "null" when no action has been
  // performed on an available update
  do_check_eq(patch.state, "null");
  do_check_false(patch.selected);
  //XXX TODO - test nsIUpdatePatch:serialize

  type = "partial";
  patch = bestUpdate.getPatchAt(1);
  do_check_eq(patch.type, type);
  do_check_eq(patch.URL, "http://" + type + "/");
  do_check_eq(patch.hashFunction, "SHA1");
  do_check_eq(patch.hashValue, "e6678ca40ae7582316acdeddf3c133c9c8577de4");
  do_check_eq(patch.size, 1316138);
  // nsIUpdatePatch:state returns the string "null" when no action has been
  // performed on an available update
  do_check_eq(patch.state, "null");
  do_check_false(patch.selected);
  //XXX TODO - test nsIUpdatePatch:serialize

  run_test_pt2();
}

// Empty update xml
function run_test_pt2() {
  run_test_helper_pt1("aus-0020_general-2.xml",
                      "empty update xml",
                      null, run_test_pt3);
}

// update xml not found
function run_test_pt3() {
  run_test_helper_pt1("aus-0020_general-3.xml",
                      "update xml not available",
                      null, run_test_pt4);
}

// no updates available
function run_test_pt4() {
  run_test_helper_pt1("aus-0020_general-4.xml",
                      "no updates available",
                      0, run_test_pt5);
}

// one update available with two patches
function run_test_pt5() {
  run_test_helper_pt1("aus-0020_general-5.xml",
                      "one update available",
                      1, run_test_pt6);
}

// three updates available each with two patches
function run_test_pt6() {
  run_test_helper_pt1("aus-0020_general-6.xml",
                      "three updates available",
                      3, run_test_pt7);
}

// one update with complete and partial patches with size 0 specified in the
// update xml
function run_test_pt7() {
  run_test_helper_pt1("aus-0020_general-7.xml",
                      "one update with complete and partial patches with size 0",
                      0, run_test_pt8);
}

// one update with complete patch with size 0 specified in the update xml
function run_test_pt8() {
  run_test_helper_pt1("aus-0020_general-8.xml",
                      "one update with complete patch with size 0",
                      0, run_test_pt9);
}

// one update with partial patch with size 0 specified in the update xml
function run_test_pt9() {
  run_test_helper_pt1("aus-0020_general-9.xml",
                      "one update with partial patch with size 0",
                      0, end_test);
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
