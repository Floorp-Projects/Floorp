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
    executeSoon(waitForHelperSleepFinished);
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
  checkPostUpdateAppLog();
}

/**
 * Called after the call to checkPostUpdateAppLog finishes.
 */
function checkPostUpdateAppLogFinished() {
  standardInit();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContains(ERR_PARENT_PID_PERSISTS);
  executeSoon(waitForUpdateXMLFiles);
}

/**
 * Called after the call to waitForUpdateXMLFiles finishes.
 */
function waitForUpdateXMLFilesFinished() {
  checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}
