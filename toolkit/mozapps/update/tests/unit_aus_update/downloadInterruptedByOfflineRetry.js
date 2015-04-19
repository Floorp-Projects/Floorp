/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Needs to be in sync w/ nsUpdateService.js
const NETWORK_ERROR_OFFLINE = 111;

function run_test() {
  setupTestCommon();

  debugDump("testing when an update check fails because the network is " +
            "offline that we check again when the network comes online " +
            "(Bug 794211).");

  setUpdateURLOverride();
  Services.prefs.setBoolPref(PREF_APP_UPDATE_AUTO, false);

  overrideXHR(xhr_pt1);
  overrideUpdatePrompt(updatePrompt);
  standardInit();

  do_execute_soon(run_test_pt1);
}

function run_test_pt1() {
  gResponseBody = null;
  gCheckFunc = check_test_pt1;
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function xhr_pt1(aXHR) {
  aXHR.status = Cr.NS_ERROR_OFFLINE;
  aXHR.onerror({ target: aXHR });
}

function check_test_pt1(request, update) {
  Assert.equal(gStatusCode, Cr.NS_ERROR_OFFLINE,
               "the download status code" + MSG_SHOULD_EQUAL);
  Assert.equal(update.errorCode, NETWORK_ERROR_OFFLINE,
               "the update error code" + MSG_SHOULD_EQUAL);

  // Forward the error to AUS, which should register the online observer
  gAUS.onError(request, update);

  // Trigger another check by notifying the offline status observer
  overrideXHR(xhr_pt2);
  Services.obs.notifyObservers(gAUS, "network:offline-status-changed", "online");
}

const updatePrompt = {
  showUpdateAvailable: function(update) {
    check_test_pt2(update);
  }
};

function xhr_pt2(aXHR) {
  let patches = getLocalPatchString();
  let updates = getLocalUpdateString(patches);
  let responseBody = getLocalUpdatesXMLString(updates);

  aXHR.status = 200;
  aXHR.responseText = responseBody;
  try {
    let parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(Ci.nsIDOMParser);
    aXHR.responseXML = parser.parseFromString(responseBody, "application/xml");
  } catch (e) {
  }
  aXHR.onload({ target: aXHR });
}

function check_test_pt2(update) {
  // We just verify that there are updates to know the check succeeded.
  Assert.ok(!!update, "there should be an update");
  Assert.equal(update.name, "App Update Test",
               "the update name" + MSG_SHOULD_EQUAL);

  doTestFinish();
}
