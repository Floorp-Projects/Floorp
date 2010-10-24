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

/* General Update Check Update XML Tests */

var gNextRunFunc;
var gExpectedCount;

function run_test() {
  do_test_pending();
  do_register_cleanup(end_test);
  removeUpdateDirsAndFiles();
  setUpdateURLOverride();
  setUpdateChannel();
  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  standardInit();
  do_timeout(0, run_test_pt01);
}

function end_test() {
  cleanUp();
}

// Helper function for testing update counts returned from an update xml
function run_test_helper_pt1(aMsg, aExpectedCount, aNextRunFunc) {
  gUpdates = null;
  gUpdateCount = null;
  gCheckFunc = check_test_helper_pt1;
  gNextRunFunc = aNextRunFunc;
  gExpectedCount = aExpectedCount;
  logTestInfo(aMsg, Components.stack.caller);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_pt1() {
  do_check_eq(gUpdateCount, gExpectedCount);
  gNextRunFunc();
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
    gXHR.responseXML = null;
  }
  var e = { target: gXHR };
  gXHR.onload.handleEvent(e);
}

// update xml not found
function run_test_pt01() {
  run_test_helper_pt1("testing update xml not available",
                      null, run_test_pt02);
}

// one update available and the update's property values
function run_test_pt02() {
  logTestInfo("testing one update available and the update's property values");
  gUpdates = null;
  gUpdateCount = null;
  gCheckFunc = check_test_pt02;
  var patches = getRemotePatchString("complete", "http://complete/", "SHA1",
                                     "98db9dad8e1d80eda7e1170d0187d6f53e477059",
                                     "9856459");
  patches += getRemotePatchString("partial", "http://partial/", "SHA1",
                                  "e6678ca40ae7582316acdeddf3c133c9c8577de4",
                                  "1316138");
  var updates = getRemoteUpdateString(patches, "minor", "Minor Test",
                                      "version 2.1a1pre", "2.1a1pre",
                                      "3.1a1pre", "20080811053724",
                                      "http://details/",
                                      "http://billboard/",
                                      "http://license/", "true",
                                      "true", "true", "4.1a1pre", "5.1a1pre",
                                      "custom1_attr=\"custom1 value\"",
                                      "custom2_attr=\"custom2 value\"");
  gResponseBody = getRemoteUpdatesXMLString(updates);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt02() {
  // XXXrstrong - not specifying a detailsURL will cause a leak due to bug 470244
  // and until this is fixed this will not test the value for detailsURL when it
  // isn't specified in the update xml.
//  var defaultDetailsURL;
//  try {
    // Try using a default details URL supplied by the distribution
    // if the update XML does not supply one.
//    var formatter = AUS_Cc["@mozilla.org/toolkit/URLFormatterService;1"].
//                    getService(AUS_Ci.nsIURLFormatter);
//    defaultDetailsURL = formatter.formatURLPref(PREF_APP_UPDATE_URL_DETAILS);
//  }
//  catch (e) {
//    defaultDetailsURL = "";
//  }

  do_check_eq(gUpdateCount, 1);
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount).QueryInterface(AUS_Ci.nsIPropertyBag);
  do_check_eq(bestUpdate.type, "minor");
  do_check_eq(bestUpdate.name, "Minor Test");
  do_check_eq(bestUpdate.displayVersion, "version 2.1a1pre");
  do_check_eq(bestUpdate.appVersion, "2.1a1pre");
  do_check_eq(bestUpdate.platformVersion, "3.1a1pre");
  do_check_eq(bestUpdate.buildID, "20080811053724");
  do_check_eq(bestUpdate.detailsURL, "http://details/");
  do_check_eq(bestUpdate.billboardURL, "http://billboard/");
  do_check_eq(bestUpdate.licenseURL, "http://license/");
  do_check_true(bestUpdate.showPrompt);
  do_check_true(bestUpdate.showNeverForVersion);
  do_check_true(bestUpdate.showSurvey);
  do_check_eq(bestUpdate.serviceURL, URL_HOST + "update.xml?force=1");
  do_check_eq(bestUpdate.channel, "test_channel");
  do_check_false(bestUpdate.isCompleteUpdate);
  do_check_false(bestUpdate.isSecurityUpdate);
  // Check that installDate is within 10 seconds of the current date.
  do_check_true((Date.now() - bestUpdate.installDate) < 10000);
  do_check_eq(bestUpdate.statusText, null);
  // nsIUpdate:state returns an empty string when no action has been performed
  // on an available update
  do_check_eq(bestUpdate.state, "");
  do_check_eq(bestUpdate.errorCode, 0);
  do_check_eq(bestUpdate.patchCount, 2);
  //XXX TODO - test nsIUpdate:serialize

  do_check_eq(bestUpdate.getProperty("custom1_attr"), "custom1 value");
  do_check_eq(bestUpdate.getProperty("custom2_attr"), "custom2 value");

  var patch = bestUpdate.getPatchAt(0);
  do_check_eq(patch.type, "complete");
  do_check_eq(patch.URL, "http://complete/");
  do_check_eq(patch.hashFunction, "SHA1");
  do_check_eq(patch.hashValue, "98db9dad8e1d80eda7e1170d0187d6f53e477059");
  do_check_eq(patch.size, 9856459);
  // The value for patch.state can be the string 'null' as a valid value. This
  // is confusing if it returns null which is an invalid value since the test
  // failure output will show a failure for null == null. To lessen the
  // confusion first check that the typeof for patch.state is string.
  do_check_eq(typeof(patch.state), "string");
  do_check_eq(patch.state, STATE_NONE);
  do_check_false(patch.selected);
  //XXX TODO - test nsIUpdatePatch:serialize

  patch = bestUpdate.getPatchAt(1);
  do_check_eq(patch.type, "partial");
  do_check_eq(patch.URL, "http://partial/");
  do_check_eq(patch.hashFunction, "SHA1");
  do_check_eq(patch.hashValue, "e6678ca40ae7582316acdeddf3c133c9c8577de4");
  do_check_eq(patch.size, 1316138);
  do_check_eq(patch.state, STATE_NONE);
  do_check_false(patch.selected);
  //XXX TODO - test nsIUpdatePatch:serialize

  run_test_pt03();
}

// one update available and the update's property default values
function run_test_pt03() {
  logTestInfo("testing one update available and the update's property values " +
              "with the format prior to bug 530872");
  gUpdates = null;
  gUpdateCount = null;
  gCheckFunc = check_test_pt03;
  var patches = getRemotePatchString("complete", "http://complete/", "SHA1",
                                     "98db9dad8e1d80eda7e1170d0187d6f53e477059",
                                     "9856459");
  var updates = getRemoteUpdateString(patches, "major", "Major Test",
                                      null, null,
                                      "5.1a1pre", "20080811053724",
                                      "http://details/",
                                      null, null, null, null, null,
                                      "version 4.1a1pre", "4.1a1pre");
  gResponseBody = getRemoteUpdatesXMLString(updates);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt03() {
  do_check_eq(gUpdateCount, 1);
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  do_check_eq(bestUpdate.type, "major");
  do_check_eq(bestUpdate.name, "Major Test");
  do_check_eq(bestUpdate.displayVersion, "version 4.1a1pre");
  do_check_eq(bestUpdate.appVersion, "4.1a1pre");
  do_check_eq(bestUpdate.platformVersion, "5.1a1pre");
  do_check_eq(bestUpdate.buildID, "20080811053724");
  do_check_eq(bestUpdate.detailsURL, "http://details/");
  do_check_eq(bestUpdate.billboardURL, "http://details/");
  do_check_eq(bestUpdate.licenseURL, null);
  do_check_true(bestUpdate.showPrompt);
  do_check_true(bestUpdate.showNeverForVersion);
  do_check_false(bestUpdate.showSurvey);
  do_check_eq(bestUpdate.serviceURL, URL_HOST + "update.xml?force=1");
  do_check_eq(bestUpdate.channel, "test_channel");
  do_check_false(bestUpdate.isCompleteUpdate);
  do_check_false(bestUpdate.isSecurityUpdate);
  // Check that installDate is within 10 seconds of the current date.
  do_check_true((Date.now() - bestUpdate.installDate) < 10000);
  do_check_eq(bestUpdate.statusText, null);
  // nsIUpdate:state returns an empty string when no action has been performed
  // on an available update
  do_check_eq(bestUpdate.state, "");
  do_check_eq(bestUpdate.errorCode, 0);
  do_check_eq(bestUpdate.patchCount, 1);
  //XXX TODO - test nsIUpdate:serialize

  var patch = bestUpdate.getPatchAt(0);
  do_check_eq(patch.type, "complete");
  do_check_eq(patch.URL, "http://complete/");
  do_check_eq(patch.hashFunction, "SHA1");
  do_check_eq(patch.hashValue, "98db9dad8e1d80eda7e1170d0187d6f53e477059");
  do_check_eq(patch.size, 9856459);
  // The value for patch.state can be the string 'null' as a valid value. This
  // is confusing if it returns null which is an invalid value since the test
  // failure output will show a failure for null == null. To lessen the
  // confusion first check that the typeof for patch.state is string.
  do_check_eq(typeof(patch.state), "string");
  do_check_eq(patch.state, STATE_NONE);
  do_check_false(patch.selected);
  //XXX TODO - test nsIUpdatePatch:serialize

  run_test_pt04();
}

// Empty update xml
function run_test_pt04() {
  gResponseBody = "\n";
  run_test_helper_pt1("testing empty update xml",
                      null, run_test_pt05);
}

// no updates available
function run_test_pt05() {
  gResponseBody = getRemoteUpdatesXMLString("");
  run_test_helper_pt1("testing no updates available",
                      0, run_test_pt06);
}

// one update available with two patches
function run_test_pt06() {
  var patches = getRemotePatchString("complete");
  patches += getRemotePatchString("partial");
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing one update available",
                      1, run_test_pt07);
}

// three updates available each with two patches
function run_test_pt07() {
  var patches = getRemotePatchString("complete");
  patches += getRemotePatchString("partial");
  var updates = getRemoteUpdateString(patches);
  updates += getRemoteUpdateString(patches);
  updates += getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing three updates available",
                      3, run_test_pt08);
}

// one update with complete and partial patches with size 0 specified in the
// update xml
function run_test_pt08() {
  var patches = getRemotePatchString("complete", null, null, null, "0");
  patches += getRemotePatchString("partial", null, null, null, "0");
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing one update with complete and partial " +
                      "patches with size 0", 0, run_test_pt09);
}

// one update with complete patch with size 0 specified in the update xml
function run_test_pt09() {
  var patches = getRemotePatchString("complete", null, null, null, "0");
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing one update with complete patch with size 0",
                      0, run_test_pt10);
}

// one update with partial patch with size 0 specified in the update xml
function run_test_pt10() {
  var patches = getRemotePatchString("partial", null, null, null, "0");
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing one update with partial patch with size 0",
                      0, run_test_pt11);
}

// check that updates for older versions of the application aren't selected
function run_test_pt11() {
  var patches = getRemotePatchString("complete");
  patches += getRemotePatchString("partial");
  var updates = getRemoteUpdateString(patches, "minor", null, null, "1.0pre");
  updates += getRemoteUpdateString(patches, "minor", null, null, "1.0a");
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing two updates older than the current version",
                      2, check_test_pt11);
}

function check_test_pt11() {
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  do_check_eq(bestUpdate, null);
  run_test_pt12();
}

// check that updates for the current version of the application are selected
function run_test_pt12() {
  var patches = getRemotePatchString("complete");
  patches += getRemotePatchString("partial");
  var updates = getRemoteUpdateString(patches, "minor", null, "version 1.0");
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing one update equal to the current version",
                      1, check_test_pt12);
}

function check_test_pt12() {
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  do_check_neq(bestUpdate, null);
  do_check_eq(bestUpdate.displayVersion, "version 1.0");
  do_test_finished();
}
