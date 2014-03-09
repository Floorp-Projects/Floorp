/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* File locked complete MAR file staged patch apply failure fallback test */

function run_test() {
  if (!shouldRunServiceTest()) {
    return;
  }

  gStageUpdate = true;
  setupTestCommon();
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_COMPLETE_MAR, false, false);

  // Exclusively lock an existing file so it is in use during the update.
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let helperDestDir = getApplyDirFile("a/b/");
  helperBin.copyTo(helperDestDir, FILE_HELPER_BIN);
  helperBin = getApplyDirFile("a/b/" + FILE_HELPER_BIN);
  // Strip off the first two directories so the path has to be from the helper's
  // working directory.
  let lockFileRelPath = gTestFiles[3].relPathDir.split("/");
  lockFileRelPath = lockFileRelPath.slice(2);
  lockFileRelPath = lockFileRelPath.join("/") + "/" + gTestFiles[3].fileName;
  let args = [getApplyDirPath() + "a/b/", "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT, lockFileRelPath];
  let lockFileProcess = AUS_Cc["@mozilla.org/process/util;1"].
                     createInstance(AUS_Ci.nsIProcess);
  lockFileProcess.init(helperBin);
  lockFileProcess.run(false, args, args.length);

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
  runUpdateUsingService(STATE_PENDING_SVC, STATE_FAILED);
}

function checkUpdateFinished() {
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
