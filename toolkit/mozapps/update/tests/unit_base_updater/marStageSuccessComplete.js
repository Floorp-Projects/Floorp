/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Complete MAR File Staged Patch Apply Test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_STAGE = gIsServiceTest ? STATE_APPLIED_SVC : STATE_APPLIED;
  gTestFiles = gTestFilesCompleteSuccess;
  const channelPrefs = getTestFileByName(FILE_CHANNEL_PREFS);
  channelPrefs.originalContents = null;
  channelPrefs.compareContents = "FromComplete\n";
  channelPrefs.comparePerms = 0o644;
  gTestDirs = gTestDirsCompleteSuccess;
  setupSymLinks();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  await stageUpdate(STATE_AFTER_STAGE, true);
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getStageDirFile, true);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS, true);
  // Switch the application to the staged application that was updated.
  runUpdate(STATE_SUCCEEDED, true, 0, true);
  await checkPostUpdateAppLog();
  checkAppBundleModTime();
  checkSymLinks();
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, true);
  checkUpdateLogContents(LOG_REPLACE_SUCCESS, false, true);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}

/**
 * Setup symlinks for the test.
 */
function setupSymLinks() {
  // Don't test symlinks on Mac OS X in this test since it tends to timeout.
  // It is tested on Mac OS X in marAppInUseStageSuccessComplete_unix.js
  if (AppConstants.platform == "linux") {
    removeSymlink();
    createSymlink();
    registerCleanupFunction(removeSymlink);
    gTestFiles.splice(gTestFiles.length - 3, 0, {
      description: "Readable symlink",
      fileName: "link",
      relPathDir: DIR_RESOURCES,
      originalContents: "test",
      compareContents: "test",
      originalFile: null,
      compareFile: null,
      originalPerms: 0o666,
      comparePerms: 0o666,
    });
  }
}

/**
 * Checks the state of the symlinks for the test.
 */
function checkSymLinks() {
  // Don't test symlinks on Mac OS X in this test since it tends to timeout.
  // It is tested on Mac OS X in marAppInUseStageSuccessComplete_unix.js
  if (AppConstants.platform == "linux") {
    checkSymlink();
  }
}
