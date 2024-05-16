/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by launching an application to apply it.
 */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  // The third parameter will test that a full path to the post update binary
  // doesn't execute.
  await setupUpdaterTest(
    FILE_COMPLETE_MAR,
    undefined,
    getApplyDirFile(null, true).path + "/"
  );
  await runUpdateUsingApp(STATE_SUCCEEDED);
  checkAppBundleModTime();
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);

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
