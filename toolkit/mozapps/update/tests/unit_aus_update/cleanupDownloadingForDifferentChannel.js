/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();

  debugDump("testing removal of an active update for a channel that is not " +
            "valid due to switching channels (Bug 486275).");

  let patchProps = {state: STATE_DOWNLOADING};
  let patches = getLocalPatchString(patchProps);
  let updateProps = {appVersion: "1.0"};
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  setUpdateChannel("original_channel");

  standardInit();

  Assert.ok(!gUpdateManager.activeUpdate,
            "there should not be an active update");
  Assert.equal(gUpdateManager.updateCount, 1,
               "the update manager update count" + MSG_SHOULD_EQUAL);
  let update = gUpdateManager.getUpdateAt(0);
  Assert.equal(update.state, STATE_FAILED,
               "the first update state" + MSG_SHOULD_EQUAL);
  Assert.equal(update.errorCode, ERR_CHANNEL_CHANGE,
               "the first update errorCode" + MSG_SHOULD_EQUAL);
  Assert.equal(update.statusText, getString("statusFailed"),
               "the first update statusText " + MSG_SHOULD_EQUAL);
  executeSoon(waitForUpdateXMLFiles);
}

/**
 * Called after the call to waitForUpdateXMLFiles finishes.
 */
function waitForUpdateXMLFilesFinished() {
  let dir = getUpdatesDir();
  dir.append(DIR_PATCH);
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST);

  let statusFile = dir.clone();
  statusFile.append(FILE_UPDATE_STATUS);
  Assert.ok(!statusFile.exists(), MSG_SHOULD_NOT_EXIST);

  doTestFinish();
}
