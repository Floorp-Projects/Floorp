/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Patch app binary partial MAR file staged patch apply success test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_STAGE = gIsServiceTest ? STATE_APPLIED_SVC : STATE_APPLIED;
  gTestFiles = gTestFilesPartialSuccess;
  gTestDirs = gTestDirsPartialSuccess;
  gCallbackBinFile = "exe0.exe";
  await setupUpdaterTest(FILE_PARTIAL_MAR, false);
  await stageUpdate(STATE_AFTER_STAGE, true);
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getStageDirFile, true);
  checkUpdateLogContents(LOG_PARTIAL_SUCCESS, true);
  // Switch the application to the staged application that was updated.
  runUpdate(STATE_SUCCEEDED, true, 0, true);
  await checkPostUpdateAppLog();
  standardInit();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, true);
  checkUpdateLogContents(LOG_REPLACE_SUCCESS, false, true);
  await waitForUpdateXMLFiles();
  checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}
