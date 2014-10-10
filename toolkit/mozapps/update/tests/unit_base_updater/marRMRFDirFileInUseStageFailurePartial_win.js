/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use inside removed dir partial MAR file staged patch apply failure
   test */

function run_test() {
  gStageUpdate = true;
  setupTestCommon();
  gTestFiles = gTestFilesPartialSuccess;
  gTestDirs = gTestDirsPartialSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_PARTIAL_MAR);

  let fileInUseBin = getApplyDirFile(gTestDirs[2].relPathDir +
                                     gTestDirs[2].files[0]);
  // Remove the empty file created for the test so the helper application can
  // replace it.
  fileInUseBin.remove(false);

  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let fileInUseDir = getApplyDirFile(gTestDirs[2].relPathDir);
  helperBin.copyTo(fileInUseDir, gTestDirs[2].files[0]);

  // Launch an existing file so it is in use during the update.
  let args = [getApplyDirPath() + DIR_RESOURCES, "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT];
  let fileInUseProcess = AUS_Cc["@mozilla.org/process/util;1"].
                         createInstance(AUS_Ci.nsIProcess);
  fileInUseProcess.init(fileInUseBin);
  fileInUseProcess.run(false, args, args.length);

  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  runUpdate(0, STATE_APPLIED, null);

  // Switch the application to the staged application that was updated.
  gStageUpdate = false;
  gSwitchApp = true;
  gDisableReplaceFallback = true;
  runUpdate(1, STATE_FAILED_WRITE_ERROR);
}

function checkUpdateApplied() {
  setupHelperFinish();
}

function checkUpdate() {
  checkFilesAfterUpdateFailure(getApplyDirFile, true, false);
  checkUpdateLogContains(ERR_RENAME_FILE);
  checkCallbackAppLog();
}
