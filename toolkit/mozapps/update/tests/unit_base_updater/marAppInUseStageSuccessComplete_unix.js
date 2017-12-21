/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Application in use complete MAR file staged patch apply success test */

const START_STATE = STATE_PENDING;
const STATE_AFTER_STAGE = STATE_APPLIED;

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestFiles[gTestFiles.length - 1].originalContents = null;
  gTestFiles[gTestFiles.length - 1].compareContents = "FromComplete\n";
  gTestFiles[gTestFiles.length - 1].comparePerms = 0o644;
  gTestDirs = gTestDirsCompleteSuccess;
  setupUpdaterTest(FILE_COMPLETE_MAR, true);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  setupSymLinks();
  runHelperFileInUse(DIR_RESOURCES + gCallbackBinFile, false);
}

/**
 * Called after the call to waitForHelperSleep finishes.
 */
function waitForHelperSleepFinished() {
  stageUpdate(true);
}

/**
 * Called after the call to stageUpdate finishes.
 */
function stageUpdateFinished() {
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getStageDirFile, true);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS, true);
  // Switch the application to the staged application that was updated.
  runUpdate(STATE_SUCCEEDED, true, 0, true);
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
  checkSymLinks();
  standardInit();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_REPLACE_SUCCESS, false, true);
  executeSoon(waitForUpdateXMLFiles);
}

/**
 * Called after the call to waitForUpdateXMLFiles finishes.
 */
function waitForUpdateXMLFilesFinished() {
  checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}

/**
 * Setup symlinks for the test.
 */
function setupSymLinks() {
  if (IS_UNIX) {
    removeSymlink();
    createSymlink();
    registerCleanupFunction(removeSymlink);
    gTestFiles.splice(gTestFiles.length - 3, 0,
      {
        description: "Readable symlink",
        fileName: "link",
        relPathDir: DIR_RESOURCES,
        originalContents: "test",
        compareContents: "test",
        originalFile: null,
        compareFile: null,
        originalPerms: 0o666,
        comparePerms: 0o666
      });
  }
}

/**
 * Checks the state of the symlinks for the test.
 */
function checkSymLinks() {
  if (IS_UNIX) {
    checkSymlink();
  }
}
