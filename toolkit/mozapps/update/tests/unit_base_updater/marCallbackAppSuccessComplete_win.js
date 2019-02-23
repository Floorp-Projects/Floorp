/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Replace app binary complete MAR file patch apply success test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  gCallbackBinFile = "exe0.exe";
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  runUpdate(STATE_SUCCEEDED, false, 0, true);
  await checkPostUpdateAppLog();
  standardInit();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);
  await waitForUpdateXMLFiles();
  checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}
