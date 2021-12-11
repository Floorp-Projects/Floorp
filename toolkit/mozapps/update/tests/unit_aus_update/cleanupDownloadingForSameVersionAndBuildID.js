/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

async function run_test() {
  setupTestCommon();

  debugDump(
    "testing removal of an update download in progress for the " +
      "same version of the application with the same application " +
      "build id on startup (Bug 536547)"
  );

  let patchProps = { state: STATE_DOWNLOADING };
  let patches = getLocalPatchString(patchProps);
  let updateProps = { appVersion: "1.0", buildID: "2007010101" };
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_DOWNLOADING);

  standardInit();

  Assert.ok(
    !gUpdateManager.downloadingUpdate,
    "there should not be a downloading update"
  );
  Assert.ok(!gUpdateManager.readyUpdate, "there should not be a ready update");
  Assert.equal(
    gUpdateManager.getUpdateCount(),
    1,
    "the update manager update count" + MSG_SHOULD_EQUAL
  );
  let update = gUpdateManager.getUpdateAt(0);
  Assert.equal(
    update.state,
    STATE_FAILED,
    "the first update state" + MSG_SHOULD_EQUAL
  );
  Assert.equal(
    update.errorCode,
    ERR_OLDER_VERSION_OR_SAME_BUILD,
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
