/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function run_test() {
  setupTestCommon();

  debugDump(
    "testing update cleanup when reading the status file returns " +
      "STATUS_NONE, the version file is for a newer version, and the " +
      "update xml has an update with STATE_PENDING (Bug 601701)."
  );

  let patchProps = { state: STATE_PENDING };
  let patches = getLocalPatchString(patchProps);
  let updates = getLocalUpdateString({}, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeVersionFile("99.9");

  // Check that there are no active updates first so the updates directory is
  // cleaned up by the UpdateManager before the remaining tests.
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
    ERR_UPDATE_STATE_NONE,
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

  let versionFile = getUpdateDirFile(FILE_UPDATE_VERSION);
  Assert.ok(!versionFile.exists(), MSG_SHOULD_NOT_EXIST);

  doTestFinish();
}
