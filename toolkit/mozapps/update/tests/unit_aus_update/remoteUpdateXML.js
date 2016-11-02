/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var gNextRunFunc;
var gExpectedCount;

function run_test() {
  setupTestCommon();

  debugDump("testing remote update xml attributes");

  start_httpserver();
  setUpdateURLOverride(gURLData + gHTTPHandlerPath);
  setUpdateChannel("test_channel");

  // This test expects that the app.update.download.backgroundInterval
  // preference doesn't already exist.
  Services.prefs.deleteBranch("app.update.download.backgroundInterval");

  standardInit();
  do_execute_soon(run_test_pt01);
}

// Helper function for testing update counts returned from an update xml
function run_test_helper_pt1(aMsg, aExpectedCount, aNextRunFunc) {
  gUpdates = null;
  gUpdateCount = null;
  gCheckFunc = check_test_helper_pt1;
  gNextRunFunc = aNextRunFunc;
  gExpectedCount = aExpectedCount;
  debugDump(aMsg, Components.stack.caller);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_pt1() {
  Assert.equal(gUpdateCount, gExpectedCount,
               "the update count" + MSG_SHOULD_EQUAL);
  gNextRunFunc();
}

// update xml not found
function run_test_pt01() {
  run_test_helper_pt1("testing update xml not available",
                      null, run_test_pt02);
}

// one update available and the update's property values
function run_test_pt02() {
  debugDump("testing one update available and the update's property values");
  gUpdates = null;
  gUpdateCount = null;
  gCheckFunc = check_test_pt02;
  let patches = getRemotePatchString("complete", "http://complete/", "SHA1",
                                     "98db9dad8e1d80eda7e1170d0187d6f53e477059",
                                     "9856459");
  patches += getRemotePatchString("partial", "http://partial/", "SHA1",
                                  "e6678ca40ae7582316acdeddf3c133c9c8577de4",
                                  "1316138");
  let updates = getRemoteUpdateString(patches, "minor", "Minor Test",
                                      "version 2.1a1pre", "2.1a1pre",
                                      "20080811053724",
                                      "http://details/",
                                      "true",
                                      "true", "345600", "1200",
                                      "custom1_attr=\"custom1 value\"",
                                      "custom2_attr=\"custom2 value\"");
  gResponseBody = getRemoteUpdatesXMLString(updates);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt02() {
  // XXXrstrong - not specifying a detailsURL will cause a leak due to bug 470244
  // and until this is fixed this will not test the value for detailsURL when it
  // isn't specified in the update xml.
//  let defaultDetailsURL;
//  try {
    // Try using a default details URL supplied by the distribution
    // if the update XML does not supply one.
//    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
//                    getService(Ci.nsIURLFormatter);
//    defaultDetailsURL = formatter.formatURLPref(PREF_APP_UPDATE_URL_DETAILS);
//  } catch (e) {
//    defaultDetailsURL = "";
//  }

  Assert.equal(gUpdateCount, 1,
               "the update count" + MSG_SHOULD_EQUAL);
  let bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount).QueryInterface(Ci.nsIPropertyBag);
  Assert.equal(bestUpdate.type, "minor",
               "the update type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.name, "Minor Test",
               "the update name attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.displayVersion, "version 2.1a1pre",
               "the update displayVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.appVersion, "2.1a1pre",
               "the update appVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.buildID, "20080811053724",
               "the update buildID attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.detailsURL, "http://details/",
               "the update detailsURL attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(bestUpdate.showPrompt,
            "the update showPrompt attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(bestUpdate.showNeverForVersion,
            "the update showNeverForVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.promptWaitTime, "345600",
               "the update promptWaitTime attribute" + MSG_SHOULD_EQUAL);
  // The default and maximum value for backgroundInterval is 600
  Assert.equal(bestUpdate.getProperty("backgroundInterval"), "600",
               "the update backgroundInterval attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.serviceURL, gURLData + gHTTPHandlerPath + "?force=1",
               "the update serviceURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.channel, "test_channel",
               "the update channel attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!bestUpdate.isCompleteUpdate,
            "the update isCompleteUpdate attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!bestUpdate.isSecurityUpdate,
            "the update isSecurityUpdate attribute" + MSG_SHOULD_EQUAL);
  // Check that installDate is within 10 seconds of the current date.
  Assert.ok((Date.now() - bestUpdate.installDate) < 10000,
            "the update installDate attribute should be within 10 seconds of " +
            "the current time");
  Assert.ok(!bestUpdate.statusText,
            "the update statusText attribute" + MSG_SHOULD_EQUAL);
  // nsIUpdate:state returns an empty string when no action has been performed
  // on an available update
  Assert.equal(bestUpdate.state, "",
               "the update state attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.errorCode, 0,
               "the update errorCode attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.patchCount, 2,
               "the update patchCount attribute" + MSG_SHOULD_EQUAL);
  // XXX TODO - test nsIUpdate:serialize

  Assert.equal(bestUpdate.getProperty("custom1_attr"), "custom1 value",
               "the update custom1_attr property" + MSG_SHOULD_EQUAL);
  Assert.equal(bestUpdate.getProperty("custom2_attr"), "custom2 value",
               "the update custom2_attr property" + MSG_SHOULD_EQUAL);

  let patch = bestUpdate.getPatchAt(0);
  Assert.equal(patch.type, "complete",
               "the update patch type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.URL, "http://complete/",
               "the update patch URL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashFunction, "SHA1",
               "the update patch hashFunction attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashValue, "98db9dad8e1d80eda7e1170d0187d6f53e477059",
               "the update patch hashValue attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.size, 9856459,
               "the update patch size attribute" + MSG_SHOULD_EQUAL);
  // The value for patch.state can be the string 'null' as a valid value. This
  // is confusing if it returns null which is an invalid value since the test
  // failure output will show a failure for null == null. To lessen the
  // confusion first check that the typeof for patch.state is string.
  Assert.equal(typeof patch.state, "string",
               "the update patch state typeof value should equal |string|");
  Assert.equal(patch.state, STATE_NONE,
               "the update patch state attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!patch.selected,
            "the update patch selected attribute" + MSG_SHOULD_EQUAL);
  // XXX TODO - test nsIUpdatePatch:serialize

  patch = bestUpdate.getPatchAt(1);
  Assert.equal(patch.type, "partial",
               "the update patch type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.URL, "http://partial/",
               "the update patch URL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashFunction, "SHA1",
               "the update patch hashFunction attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashValue, "e6678ca40ae7582316acdeddf3c133c9c8577de4",
               "the update patch hashValue attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.size, 1316138,
               "the update patch size attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.state, STATE_NONE,
               "the update patch state attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!patch.selected,
            "the update patch selected attribute" + MSG_SHOULD_EQUAL);
  // XXX TODO - test nsIUpdatePatch:serialize

  run_test_pt03();
}

// Empty update xml (an empty xml file returns a root node name of parsererror)
function run_test_pt03() {
  gResponseBody = "<parsererror/>";
  run_test_helper_pt1("testing empty update xml",
                      null, run_test_pt04);
}

// no updates available
function run_test_pt04() {
  gResponseBody = getRemoteUpdatesXMLString("");
  run_test_helper_pt1("testing no updates available",
                      0, run_test_pt05);
}

// one update available with two patches
function run_test_pt05() {
  let patches = getRemotePatchString("complete");
  patches += getRemotePatchString("partial");
  let updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing one update available",
                      1, run_test_pt06);
}

// three updates available each with two patches
function run_test_pt06() {
  let patches = getRemotePatchString("complete");
  patches += getRemotePatchString("partial");
  let updates = getRemoteUpdateString(patches);
  updates += getRemoteUpdateString(patches);
  updates += getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing three updates available",
                      3, run_test_pt07);
}

// one update with complete and partial patches with size 0 specified in the
// update xml
function run_test_pt07() {
  let patches = getRemotePatchString("complete", null, null, null, "0");
  patches += getRemotePatchString("partial", null, null, null, "0");
  let updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing one update with complete and partial " +
                      "patches with size 0", 0, run_test_pt08);
}

// one update with complete patch with size 0 specified in the update xml
function run_test_pt08() {
  let patches = getRemotePatchString("complete", null, null, null, "0");
  let updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing one update with complete patch with size 0",
                      0, run_test_pt9);
}

// one update with partial patch with size 0 specified in the update xml
function run_test_pt9() {
  let patches = getRemotePatchString("partial", null, null, null, "0");
  let updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing one update with partial patch with size 0",
                      0, run_test_pt10);
}

// check that updates for older versions of the application aren't selected
function run_test_pt10() {
  let patches = getRemotePatchString("complete");
  patches += getRemotePatchString("partial");
  let updates = getRemoteUpdateString(patches, "minor", null, null, "1.0pre");
  updates += getRemoteUpdateString(patches, "minor", null, null, "1.0a");
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing two updates older than the current version",
                      2, check_test_pt10);
}

function check_test_pt10() {
  let bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  Assert.ok(!bestUpdate,
            "there should be no update available");
  run_test_pt11();
}

// check that updates for the current version of the application are selected
function run_test_pt11() {
  let patches = getRemotePatchString("complete");
  patches += getRemotePatchString("partial");
  let updates = getRemoteUpdateString(patches, "minor", null, "version 1.0");
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("testing one update equal to the current version",
                      1, check_test_pt11);
}

function check_test_pt11() {
  let bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  Assert.ok(!!bestUpdate,
            "there should be one update available");
  Assert.equal(bestUpdate.displayVersion, "version 1.0",
               "the update displayVersion attribute" + MSG_SHOULD_EQUAL);

  doTestFinish();
}
