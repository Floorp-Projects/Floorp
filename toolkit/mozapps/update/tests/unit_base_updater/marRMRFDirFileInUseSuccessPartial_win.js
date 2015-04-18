/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use inside removed dir partial MAR file patch apply success test */

function run_test() {
  setupTestCommon();
  gTestFiles = gTestFilesPartialSuccess;
  gTestDirs = gTestDirsPartialSuccess;
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
  let fileInUseProcess = Cc["@mozilla.org/process/util;1"].
                         createInstance(Ci.nsIProcess);
  fileInUseProcess.init(fileInUseBin);
  fileInUseProcess.run(false, args, args.length);

  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  runUpdate(0, STATE_SUCCEEDED, checkUpdateFinished);
}

function checkUpdateFinished() {
  setupHelperFinish();
}

function checkUpdate() {
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, true);
  checkUpdateLogContains(ERR_BACKUP_DISCARD);
  standardInit();
  checkCallbackAppLog();
}
