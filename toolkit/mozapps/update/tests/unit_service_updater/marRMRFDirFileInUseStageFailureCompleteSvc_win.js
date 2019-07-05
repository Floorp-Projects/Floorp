/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use inside removed dir complete MAR file staged patch apply failure
   test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_STAGE = gIsServiceTest ? STATE_APPLIED_SVC : STATE_APPLIED;
  const STATE_AFTER_RUNUPDATE = gIsServiceTest
    ? STATE_PENDING_SVC
    : STATE_PENDING;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  await runHelperFileInUse(
    gTestDirs[4].relPathDir +
      gTestDirs[4].subDirs[0] +
      gTestDirs[4].subDirFiles[0],
    true
  );
  await stageUpdate(STATE_AFTER_STAGE, true);
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getStageDirFile, true);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS, true);
  // Switch the application to the staged application that was updated.
  runUpdate(STATE_AFTER_RUNUPDATE, true, 1, true);
  await waitForHelperExit();
  standardInit();
  checkPostUpdateRunningFile(false);
  setTestFilesAndDirsForFailure();
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_RENAME_FILE);
  checkUpdateLogContains(
    ERR_MOVE_DESTDIR_7 + "\n" + STATE_FAILED_WRITE_ERROR + "\n" + CALL_QUIT
  );
  await waitForUpdateXMLFiles();
  checkUpdateManager(STATE_NONE, false, STATE_AFTER_RUNUPDATE, 0, 1);
  checkCallbackLog();
}
