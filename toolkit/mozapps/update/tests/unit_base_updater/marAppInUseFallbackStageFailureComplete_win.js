/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Application in use complete MAR file staged patch apply failure fallback test */

function run_test() {
  gStageUpdate = true;
  setupTestCommon();
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_COMPLETE_MAR, false, false);

  // Launch the callback helper application so it is in use during the update.
  let callbackApp = getApplyDirFile("a/b/" + gCallbackBinFile);
  let args = [getApplyDirPath() + "a/b/", "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT];
  let callbackAppProcess = AUS_Cc["@mozilla.org/process/util;1"].
                           createInstance(AUS_Ci.nsIProcess);
  callbackAppProcess.init(callbackApp);
  callbackAppProcess.run(false, args, args.length);

  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  runUpdate(0, STATE_APPLIED, null);

  // Switch the application to the staged application that was updated.
  gStageUpdate = false;
  gSwitchApp = true;
  runUpdate(1, STATE_PENDING);
}

function checkUpdateApplied() {
  setupHelperFinish();
}

function checkUpdate() {
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_RENAME_FILE);
  checkCallbackAppLog();
}
