/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test a replace request for a staged update with a version file that specifies
 * an older version failure. The same check is used in nsUpdateDriver.cpp for
 * all update types which is why there aren't tests for the maintenance service
 * as well as for other update types.
 */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_STAGE = STATE_APPLIED;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  await setupUpdaterTest(FILE_COMPLETE_MAR, null, "", false);
  let patchProps = { state: STATE_AFTER_STAGE };
  let patches = getLocalPatchString(patchProps);
  let updateProps = { appVersion: "0.9" };
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  getUpdateDirFile(FILE_UPDATE_LOG).create(
    Ci.nsIFile.NORMAL_FILE_TYPE,
    PERMS_FILE
  );
  writeStatusFile(STATE_AFTER_STAGE);
  // Create the version file with an older version to simulate installing a new
  // version of the application while there is an update that has been staged.
  writeVersionFile("0.9");
  // Try to switch the application to the fake staged application.
  await runUpdateUsingApp(STATE_AFTER_STAGE);
  standardInit();
  checkPostUpdateRunningFile(false);
  setTestFilesAndDirsForFailure();
  checkFilesAfterUpdateFailure(getApplyDirFile);
  await waitForUpdateXMLFiles();
  checkUpdateManager(
    STATE_NONE,
    false,
    STATE_FAILED,
    ERR_OLDER_VERSION_OR_SAME_BUILD,
    1
  );

  let updatesDir = getUpdateDirFile(DIR_PATCH);
  Assert.ok(
    updatesDir.exists(),
    MSG_SHOULD_EXIST + getMsgPath(updatesDir.path)
  );

  let log = getUpdateDirFile(FILE_UPDATE_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST + getMsgPath(log.path));

  log = getUpdateDirFile(FILE_LAST_UPDATE_LOG);
  Assert.ok(log.exists(), MSG_SHOULD_EXIST + getMsgPath(log.path));

  log = getUpdateDirFile(FILE_BACKUP_UPDATE_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST + getMsgPath(log.path));

  waitForFilesInUse();
}
