/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by staging an update and launching an application to
 * apply it.
 */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_STAGE = gIsServiceTest ? STATE_APPLIED_SVC : STATE_APPLIED;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  await stageUpdate(STATE_AFTER_STAGE, true);
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getStageDirFile, true, false);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS, true);
  lockDirectory(getGREBinDir().path);
  // Switch the application to the staged application that was updated.
  await runUpdateUsingApp(STATE_SUCCEEDED);
  await checkPostUpdateAppLog();
  checkAppBundleModTime();
  standardInit();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);
  await waitForUpdateXMLFiles();
  checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);

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
  Assert.ok(log.exists(), MSG_SHOULD_EXIST + getMsgPath(log.path));

  waitForFilesInUse();
}
