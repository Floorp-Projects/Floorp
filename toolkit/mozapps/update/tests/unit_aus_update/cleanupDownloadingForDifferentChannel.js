/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

async function run_test() {
  setupTestCommon();

  debugDump(
    "testing removal of an active update for a channel that is not " +
      "valid due to switching channels (Bug 486275)."
  );

  let patchProps = { state: STATE_DOWNLOADING };
  let patches = getLocalPatchString(patchProps);
  let updateProps = { appVersion: "1.0" };
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  setUpdateChannel("original_channel");

  await standardInit();

  Assert.ok(
    !(await gUpdateManager.getDownloadingUpdate()),
    "there should not be a downloading update"
  );
  Assert.ok(
    !(await gUpdateManager.getReadyUpdate()),
    "there should not be a ready update"
  );
  const history = await gUpdateManager.getHistory();
  Assert.equal(
    history.length,
    1,
    "the update manager update count" + MSG_SHOULD_EQUAL
  );
  let update = history[0];
  Assert.equal(
    update.state,
    STATE_FAILED,
    "the first update state" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.errorCode,
    ERR_CHANNEL_CHANGE,
    "the first update errorCode" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.statusText,
    getString("statusFailed"),
    "the first update statusText " + MSG_SHOULD_EQUAL
  );
  await waitForUpdateXMLFiles();

  let dir = getUpdateDirFile(DIR_PATCH);
  Assert.ok(dir.exists(), MSG_SHOULD_EXIST);

  let statusFile = getUpdateDirFile(FILE_UPDATE_STATUS);
  Assert.ok(!statusFile.exists(), MSG_SHOULD_NOT_EXIST);

  doTestFinish();
}
