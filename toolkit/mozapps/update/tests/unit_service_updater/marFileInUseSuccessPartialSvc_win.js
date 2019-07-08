/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use partial MAR file patch apply success test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesPartialSuccess;
  gTestDirs = gTestDirsPartialSuccess;
  await setupUpdaterTest(FILE_PARTIAL_MAR, false);
  await runHelperFileInUse(
    gTestFiles[11].relPathDir + gTestFiles[11].fileName,
    false
  );
  runUpdate(STATE_SUCCEEDED, false, 0, true);
  await waitForHelperExit();
  await checkPostUpdateAppLog();
  standardInit();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, true);
  checkUpdateLogContains(ERR_BACKUP_DISCARD);
  checkUpdateLogContains(STATE_SUCCEEDED + "\n" + CALL_QUIT);
  await waitForUpdateXMLFiles();
  checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}
