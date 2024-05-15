/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use complete MAR file patch apply success test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  const testFile = getTestFileByName("0exe0.exe");
  await runHelperFileInUse(testFile.relPathDir + testFile.fileName, false);
  runUpdate(STATE_SUCCEEDED, false, 0, true);
  await waitForHelperExit();
  await checkPostUpdateAppLog();
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, true);
  checkUpdateLogContains(ERR_BACKUP_DISCARD);
  checkUpdateLogContains(STATE_SUCCEEDED + "\n" + CALL_QUIT);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}
