/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File locked complete MAR file patch apply failure test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  await runHelperLockFile(getTestFileByName("searchpluginspng0.png"));
  runUpdate(STATE_FAILED_WRITE_ERROR, false, 1, true);
  await waitForHelperExit();
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_RENAME_FILE);
  checkUpdateLogContains(ERR_BACKUP_CREATE_7);
  checkUpdateLogContains(STATE_FAILED_WRITE_ERROR + "\n" + CALL_QUIT);
  await waitForUpdateXMLFiles(true, false);
  await checkUpdateManager(STATE_PENDING, true, STATE_PENDING, WRITE_ERROR, 0);
  checkCallbackLog();
}
