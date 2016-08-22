/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File locked complete MAR file staged patch apply failure test */

const STATE_AFTER_STAGE = STATE_PENDING;

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_COMPLETE_MAR, false);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  runHelperLockFile(gTestFiles[3]);
}

/**
 * Called after the call to waitForHelperSleep finishes.
 */
function waitForHelperSleepFinished() {
  stageUpdate();
}

/**
 * Called after the call to stageUpdate finishes.
 */
function stageUpdateFinished() {
  checkPostUpdateRunningFile(false);
  // Files aren't checked after staging since this test locks a file which
  // prevents reading the file.
  checkUpdateLogContains(ERR_ENSURE_COPY);
  // Switch the application to the staged application that was updated.
  runUpdate(STATE_FAILED_WRITE_ERROR, false, 1, false);
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
  Assert.equal(readStatusFile(), STATE_PENDING,
               "the status file failure code" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.updateCount, 2,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.getUpdateAt(0).state, STATE_PENDING,
               "the update state" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.getUpdateAt(0).errorCode, WRITE_ERROR,
               "the update errorCode" + MSG_SHOULD_EQUAL);
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_RENAME_FILE);
  checkUpdateLogContains(ERR_BACKUP_CREATE_7);
  checkUpdateLogContains(STATE_FAILED_WRITE_ERROR + "\n" + CALL_QUIT);
  checkCallbackLog();
}
