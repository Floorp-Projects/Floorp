/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Application in use complete MAR file patch apply success test */

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
function setupUpdaterTestFinished() {
  runHelperPIDPersists(DIR_RESOURCES + gCallbackBinFile, false);
}

/**
 * Called after the call to waitForHelperSleep finishes.
 */
function waitForHelperSleepFinished() {
  if (!gPIDPersistProcess.pid) {
    do_execute_soon(waitForHelperSleepFinished);
    return;
  }
  runUpdate(STATE_SUCCEEDED, false, 0, true);
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
  Assert.equal(readStatusState(), STATE_NONE,
               "the status file state" + MSG_SHOULD_EQUAL);
  Assert.ok(!gUpdateManager.activeUpdate,
            "the active update should not be defined");
  Assert.equal(gUpdateManager.updateCount, 1,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.getUpdateAt(0).state, STATE_SUCCEEDED,
               "the update state" + MSG_SHOULD_EQUAL);
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContains(ERR_PARENT_PID_PERSISTS);
  // The callback application is not launched by the updater when the parent pid
  // doesn't exit.
  waitForFilesInUse();
}
