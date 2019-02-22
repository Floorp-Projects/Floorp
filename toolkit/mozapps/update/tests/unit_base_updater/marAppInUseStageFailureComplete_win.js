/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Application in use complete MAR file staged patch apply failure test */

const STATE_AFTER_STAGE = IS_SERVICE_TEST ? STATE_APPLIED_SVC : STATE_APPLIED;
const STATE_AFTER_RUNUPDATE = IS_SERVICE_TEST ? STATE_PENDING_SVC : STATE_PENDING;

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setupUpdaterTest(FILE_COMPLETE_MAR, false);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
async function setupUpdaterTestFinished() {
  await runHelperFileInUse(DIR_RESOURCES + gCallbackBinFile, false);
  stageUpdate(true);
}

/**
 * Called after the call to stageUpdate finishes.
 */
async function stageUpdateFinished() {
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
  checkUpdateLogContains(ERR_MOVE_DESTDIR_7 + "\n" +
                         STATE_FAILED_WRITE_ERROR + "\n" +
                         CALL_QUIT);
  await waitForUpdateXMLFiles();
  checkUpdateManager(STATE_NONE, false, STATE_AFTER_RUNUPDATE, 0, 1);
  checkCallbackLog();
}
