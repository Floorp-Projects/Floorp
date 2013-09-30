/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Offline retry test (Bug 794211) */

const TEST_ID = "bug794211";

// Needs to be in sync w/ nsUpdateService.js
const NETWORK_ERROR_OFFLINE = 111;

function run_test() {
  do_test_pending();

  // adjustGeneralPaths registers a cleanup function that calls end_test.
  adjustGeneralPaths();

  logTestInfo("test when an update check fails because the network is " +
              "offline that we check again when the network comes online. " +
              "(Bug 794211)");
  removeUpdateDirsAndFiles();
  setUpdateURLOverride();
  Services.prefs.setBoolPref(PREF_APP_UPDATE_AUTO, false);

  overrideXHR(null);
  overrideUpdatePrompt(updatePrompt);
  standardInit();

  do_execute_soon(run_test_pt1);
}

function run_test_pt1() {
  gResponseBody = null;
  gCheckFunc = check_test_pt1;
  gXHRCallback = xhr_pt1;
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function xhr_pt1() {
  gXHR.status = AUS_Cr.NS_ERROR_OFFLINE;
  gXHR.onerror({ target: gXHR });
}

function check_test_pt1(request, update) {
  do_check_eq(gStatusCode, AUS_Cr.NS_ERROR_OFFLINE);
  do_check_eq(update.errorCode, NETWORK_ERROR_OFFLINE);

  // Forward the error to AUS, which should register the online observer
  gAUS.onError(request, update);

  // Trigger another check by notifying the offline status observer
  gXHRCallback = xhr_pt2;
  Services.obs.notifyObservers(gAUS, "network:offline-status-changed", "online");
}

var updatePrompt = {
  showUpdateAvailable: function(update) {
    check_test_pt2(update);
  }
};

function xhr_pt2() {
  var patches = getLocalPatchString();
  var updates = getLocalUpdateString(patches);
  var responseBody = getLocalUpdatesXMLString(updates);

  gXHR.status = 200;
  gXHR.responseText = responseBody;
  try {
    var parser = AUS_Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(AUS_Ci.nsIDOMParser);
    gXHR.responseXML = parser.parseFromString(responseBody, "application/xml");
  }
  catch(e) { }
  gXHR.onload({ target: gXHR });
}

function check_test_pt2(update) {
  // We just verify that there are updates to know the check succeeded.
  do_check_neq(update, null);
  do_check_eq(update.name, "App Update Test");

  do_test_finished();
}

function end_test() {
  cleanUp();
}
