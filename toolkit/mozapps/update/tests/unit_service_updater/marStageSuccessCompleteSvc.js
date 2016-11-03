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
  setupDistributionDir();
  setupSymLinks();
  setupUpdaterTest(FILE_COMPLETE_MAR, false);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  stageUpdate();
}

/**
 * Called after the call to stageUpdate finishes.
 */
function stageUpdateFinished() {
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getStageDirFile, true);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS_STAGE, true);
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
  Assert.equal(readStatusState(), STATE_NONE,
               "the status file state" + MSG_SHOULD_EQUAL);
  Assert.ok(!gUpdateManager.activeUpdate,
            "the active update should not be defined");
  Assert.equal(gUpdateManager.updateCount, 1,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.getUpdateAt(0).state, STATE_SUCCEEDED,
               "the update state" + MSG_SHOULD_EQUAL);
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, true);
  checkUpdateLogContents(LOG_REPLACE_SUCCESS, false, true);
  checkDistributionDir();
  checkCallbackLog();
}

/**
 * Setup the state of the distribution directory for the test.
 */
function setupDistributionDir() {
  if (IS_MACOSX) {
    // Create files in the old distribution directory location to verify that
    // the directory and its contents are removed when there is a distribution
    // directory in the new location.
    let testFile = getApplyDirFile(DIR_MACOS + "distribution/testFile", true);
    writeFile(testFile, "test\n");
    testFile = getApplyDirFile(DIR_MACOS + "distribution/test1/testFile", true);
    writeFile(testFile, "test\n");
  }
}

/**
 * Checks the state of the distribution directory for the test.
 */
function checkDistributionDir() {
  if (IS_MACOSX) {
    let distributionDir = getApplyDirFile(DIR_MACOS + "distribution", true);
    Assert.ok(!distributionDir.exists(),
              MSG_SHOULD_NOT_EXIST + getMsgPath(distributionDir.path));
    checkUpdateLogContains(REMOVE_OLD_DIST_DIR);
  }
}

/**
 * Setup symlinks for the test.
 */
function setupSymLinks() {
  // Don't test symlinks on Mac OS X in this test since it tends to timeout.
  // It is tested on Mac OS X in marAppInUseStageSuccessComplete_unix.js
  // The tests don't support symlinks on gonk.
  if (IS_UNIX && !IS_MACOSX && !IS_TOOLKIT_GONK) {
    removeSymlink();
    createSymlink();
    do_register_cleanup(removeSymlink);
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
  // The tests don't support symlinks on gonk.
  if (IS_UNIX && !IS_MACOSX && !IS_TOOLKIT_GONK) {
    checkSymlink();
  }
}
