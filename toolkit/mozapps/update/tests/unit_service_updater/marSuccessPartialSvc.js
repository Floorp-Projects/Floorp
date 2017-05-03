/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Patch Apply Test */

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesPartialSuccess;
  gTestFiles[gTestFiles.length - 1].originalContents = null;
  gTestFiles[gTestFiles.length - 1].compareContents = "FromPartial\n";
  gTestFiles[gTestFiles.length - 1].comparePerms = 0o644;
  gTestFiles[gTestFiles.length - 2].originalContents = null;
  gTestFiles[gTestFiles.length - 2].compareContents = "FromPartial\n";
  gTestFiles[gTestFiles.length - 2].comparePerms = 0o644;
  gTestDirs = gTestDirsPartialSuccess;
  setupDistributionDir();
  // The third parameter will test that a relative path that contains a
  // directory traversal to the post update binary doesn't execute.
  setupUpdaterTest(FILE_PARTIAL_MAR, false, "test/../");
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  runUpdate(STATE_SUCCEEDED, false, 0, true);
}

/**
 * Called after the call to runUpdate finishes.
 */
function runUpdateFinished() {
  checkAppBundleModTime();
  standardInit();
  Assert.equal(readStatusState(), STATE_NONE,
               "the status file state" + MSG_SHOULD_EQUAL);
  Assert.ok(!gUpdateManager.activeUpdate,
            "the active update should not be defined");
  Assert.equal(gUpdateManager.updateCount, 1,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.getUpdateAt(0).state, STATE_SUCCEEDED,
               "the update state" + MSG_SHOULD_EQUAL);
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_PARTIAL_SUCCESS);
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
    testFile = getApplyDirFile(DIR_MACOS + "distribution/test/testFile", true);
    writeFile(testFile, "test\n");
  }
}

/**
 * Checks the state of the distribution directory.
 */
function checkDistributionDir() {
  if (IS_MACOSX) {
    let distributionDir = getApplyDirFile(DIR_MACOS + "distribution", true);
    Assert.ok(!distributionDir.exists(),
              MSG_SHOULD_NOT_EXIST + getMsgPath(distributionDir.path));
    checkUpdateLogContains(REMOVE_OLD_DIST_DIR);
  }
}
