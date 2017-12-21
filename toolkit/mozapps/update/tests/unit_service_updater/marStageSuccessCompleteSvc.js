/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Complete MAR File Staged Patch Apply Test */

const STATE_AFTER_STAGE = IS_SERVICE_TEST ? STATE_APPLIED_SVC : STATE_APPLIED;

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestFiles[gTestFiles.length - 1].originalContents = null;
  gTestFiles[gTestFiles.length - 1].compareContents = "FromComplete\n";
  gTestFiles[gTestFiles.length - 1].comparePerms = 0o644;
  gTestDirs = gTestDirsCompleteSuccess;
  setupSymLinks();
  setupUpdaterTest(FILE_COMPLETE_MAR, false);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
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
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, true);
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
  // Don't test symlinks on Mac OS X in this test since it tends to timeout.
  // It is tested on Mac OS X in marAppInUseStageSuccessComplete_unix.js
  if (IS_UNIX && !IS_MACOSX) {
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
  // Don't test symlinks on Mac OS X in this test since it tends to timeout.
  // It is tested on Mac OS X in marAppInUseStageSuccessComplete_unix.js
  if (IS_UNIX && !IS_MACOSX) {
    checkSymlink();
  }
}
