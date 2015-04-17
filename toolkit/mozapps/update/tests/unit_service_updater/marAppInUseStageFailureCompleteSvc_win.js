/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Application in use complete MAR file staged patch apply failure test */

const START_STATE = STATE_PENDING_SVC;
const END_STATE = STATE_FAILED_WRITE_ERROR;

function run_test() {
  if (!shouldRunServiceTest()) {
    return;
  }

  gStageUpdate = true;
  setupTestCommon();
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_COMPLETE_MAR);

  // Launch the callback helper application so it is in use during the update.
  let callbackApp = getApplyDirFile(DIR_RESOURCES + gCallbackBinFile);
  let args = [getApplyDirPath() + DIR_RESOURCES, "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT];
  let callbackAppProcess = Cc["@mozilla.org/process/util;1"].
                           createInstance(Ci.nsIProcess);
  callbackAppProcess.init(callbackApp);
  callbackAppProcess.run(false, args, args.length);

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  runUpdateUsingService(START_STATE, STATE_APPLIED);
}

function checkUpdateFinished() {
  // Switch the application to the staged application that was updated.
  gStageUpdate = false;
  gSwitchApp = true;
  gDisableReplaceFallback = true;
  runUpdate(1, END_STATE, checkUpdateApplied);
}

function checkUpdateApplied() {
  setupHelperFinish();
}

function checkUpdate() {
  checkFilesAfterUpdateFailure(getApplyDirFile, true, false);
  checkUpdateLogContains(ERR_RENAME_FILE);
  standardInit();
  checkCallbackAppLog();
}
