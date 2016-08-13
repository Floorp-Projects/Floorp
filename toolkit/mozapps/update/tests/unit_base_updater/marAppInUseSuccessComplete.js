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
  runHelperFileInUse(DIR_RESOURCES + gCallbackBinFile, false);
}

/**
 * Called after the call to waitForHelperSleep finishes.
 */
function waitForHelperSleepFinished() {
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
  checkPostUpdateAppLog();
}

/**
 * Called after the call to checkPostUpdateAppLog finishes.
 */
function checkPostUpdateAppLogFinished() {
  checkAppBundleModTime();
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
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);
  checkCallbackLog();
}
