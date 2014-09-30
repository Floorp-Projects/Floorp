/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File in use partial MAR file patch apply success test */

function run_test() {
  if (!shouldRunServiceTest()) {
    return;
  }

  setupTestCommon();
  gTestFiles = gTestFilesPartialSuccess;
  gTestDirs = gTestDirsPartialSuccess;
  setupUpdaterTest(FILE_PARTIAL_MAR);

  // Launch an existing file so it is in use during the update.
  let fileInUseBin = getApplyDirFile(gTestFiles[11].relPathDir +
                                     gTestFiles[11].fileName);
  let args = [getApplyDirPath() + DIR_RESOURCES, "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT];
  let fileInUseProcess = AUS_Cc["@mozilla.org/process/util;1"].
                         createInstance(AUS_Ci.nsIProcess);
  fileInUseProcess.init(fileInUseBin);
  fileInUseProcess.run(false, args, args.length);

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  runUpdateUsingService(STATE_PENDING_SVC, STATE_SUCCEEDED);
}

function checkUpdateFinished() {
  setupHelperFinish();
}

function checkUpdate() {
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, true);
  checkUpdateLogContains(ERR_BACKUP_DISCARD);
  checkCallbackServiceLog();
}
