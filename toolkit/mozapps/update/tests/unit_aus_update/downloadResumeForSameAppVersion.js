/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();
  start_httpserver();

  debugDump("testing resuming an update download in progress for the same " +
            "version of the application on startup (Bug 485624)");

  let patchProps = {state: STATE_DOWNLOADING};
  let patches = getLocalPatchString(patchProps);
  let updateProps = {appVersion: "1.0"};
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  standardInit();

  Assert.equal(gUpdateManager.updateCount, 0,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.activeUpdate.state, STATE_DOWNLOADING,
               "the update manager activeUpdate state attribute" +
               MSG_SHOULD_EQUAL);

  // Pausing the download along with reloading the update manager in
  // doTestFinish will prevent writing the update xml files during shutdown.
  gAUS.pauseDownload();
  gUpdateManager.cleanupActiveUpdate();
  executeSoon(waitForUpdateXMLFiles);
}

/**
 * Called after the call to waitForUpdateXMLFiles finishes.
 */
function waitForUpdateXMLFilesFinished() {
  stop_httpserver(doTestFinish);
}
