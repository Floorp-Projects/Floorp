/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File locked partial MAR file patch apply failure test */

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesPartialSuccess;
  gTestDirs = gTestDirsPartialSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_PARTIAL_MAR, false);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  runHelperLockFile(gTestFiles[2]);
}

/**
 * Called after the call to waitForHelperSleep finishes.
 */
function waitForHelperSleepFinished() {
  runUpdate(STATE_FAILED_READ_ERROR, false, 1, true);
}

/**
 * Called after the call to runUpdate finishes.
 */
function runUpdateFinished() {
  waitForHelperExit();
}

/**
 * Called after the call to waitForHelperExit finishes.
 */
function waitForHelperExitFinished() {
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_UNABLE_OPEN_DEST);
  checkCallbackLog();
}
