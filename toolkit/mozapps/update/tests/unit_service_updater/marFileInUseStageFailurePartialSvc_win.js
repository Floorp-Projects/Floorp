/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use partial MAR file staged patch apply failure test */

const STATE_AFTER_STAGE = IS_SERVICE_TEST ? STATE_APPLIED_SVC : STATE_APPLIED;
const STATE_AFTER_RUNUPDATE = IS_SERVICE_TEST ? STATE_PENDING_SVC : STATE_PENDING;

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesPartialSuccess;
  gTestDirs = gTestDirsPartialSuccess;
  await setupUpdaterTest(FILE_PARTIAL_MAR, false);
  await runHelperFileInUse(gTestFiles[11].relPathDir + gTestFiles[11].fileName,
                           false);
  stageUpdate(true);
}

/**
 * Called after the call to stageUpdate finishes.
 */
async function stageUpdateFinished() {
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getStageDirFile, true);
  checkUpdateLogContents(LOG_PARTIAL_SUCCESS, true);
  // Switch the application to the staged application that was updated.
  runUpdate(STATE_AFTER_RUNUPDATE, true, 1, true);
  await waitForHelperExit();
  standardInit();
  checkPostUpdateRunningFile(false);
  setTestFilesAndDirsForFailure();
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_RENAME_FILE);
  checkUpdateLogContains(ERR_MOVE_DESTDIR_7 + "\n" +
                         STATE_FAILED_WRITE_ERROR + "\n" +
                         CALL_QUIT);
  await waitForUpdateXMLFiles();
  checkUpdateManager(STATE_NONE, false, STATE_AFTER_RUNUPDATE, 0, 1);
  checkCallbackLog();
}
