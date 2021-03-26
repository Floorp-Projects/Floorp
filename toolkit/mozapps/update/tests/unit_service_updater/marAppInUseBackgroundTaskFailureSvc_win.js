/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Fail to apply a complete MAR when the application is in use and the callback is a background task. */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }

  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();

  // Add a dummy --backgroundtask arg; this will have no effect on the
  // callback (TestAUSHelper) but will cause the updater to detect
  // that this is a background task.
  gCallbackArgs = gCallbackArgs.concat(["--backgroundtask", "not_found"]);

  // Run the update with the helper file in use, expecting failure.
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  await runHelperFileInUse(DIR_RESOURCES + gCallbackBinFile, false);
  runUpdate(
    STATE_FAILED_WRITE_ERROR_BACKGROUND_TASK_SHARING_VIOLATION,
    false, // aSwitchApp
    1, // aExpectedExitValue
    true // aCheckSvcLog
  );
  await waitForHelperExit();

  standardInit();

  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_BGTASK_EXCLUSIVE);

  // Check that the update was reset to "pending".
  await waitForUpdateXMLFiles(
    true, // aActiveUpdateExists
    false // aUpdatesExists
  );
  checkUpdateManager(
    STATE_PENDING, // aStatusFileState
    true, // aHasActiveUpdate
    STATE_PENDING, // aUpdateStatusState
    WRITE_ERROR_BACKGROUND_TASK_SHARING_VIOLATION,
    0 // aUpdateCount
  );
  checkCallbackLog();
}
