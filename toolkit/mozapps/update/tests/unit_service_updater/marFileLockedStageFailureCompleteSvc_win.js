/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File locked complete MAR file staged patch apply failure test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_STAGE = STATE_PENDING;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  await runHelperLockFile(gTestFiles[3]);
  await stageUpdate(STATE_AFTER_STAGE, true);
  checkPostUpdateRunningFile(false);
  // Files aren't checked after staging since this test locks a file which
  // prevents reading the file.
  checkUpdateLogContains(ERR_ENSURE_COPY);
  // Switch the application to the staged application that was updated.
  runUpdate(STATE_FAILED_WRITE_ERROR, false, 1, false);
  await waitForHelperExit();
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_RENAME_FILE);
  checkUpdateLogContains(ERR_BACKUP_CREATE_7);
  checkUpdateLogContains(STATE_FAILED_WRITE_ERROR + "\n" + CALL_QUIT);
  await waitForUpdateXMLFiles(true, false);
  checkUpdateManager(STATE_PENDING, true, STATE_PENDING, WRITE_ERROR, 0);
  checkCallbackLog();
}
