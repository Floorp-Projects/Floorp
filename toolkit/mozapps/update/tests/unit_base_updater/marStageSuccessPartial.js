/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Staged Patch Apply Test */

const STATE_AFTER_STAGE = IS_SERVICE_TEST ? STATE_APPLIED_SVC : STATE_APPLIED;

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesPartialSuccess;
  gTestFiles[gTestFiles.length - 2].originalContents = null;
  gTestFiles[gTestFiles.length - 2].compareContents = "FromPartial\n";
  gTestFiles[gTestFiles.length - 2].comparePerms = 0o644;
  gTestDirs = gTestDirsPartialSuccess;
  preventDistributionFiles();
  setupDistributionDir();
  setupUpdaterTest(FILE_PARTIAL_MAR, true);
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
  checkUpdateLogContents(LOG_PARTIAL_SUCCESS_STAGE, true, false, true);
  // Switch the application to the staged application that was updated.
  runUpdate(STATE_SUCCEEDED, true, 0, false);
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
  standardInit();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, true);
  checkUpdateLogContents(LOG_REPLACE_SUCCESS, false, true, true);
  checkDistributionDir();
  checkCallbackLog();
}

/**
 * Setup the state of the distribution directory for the test.
 */
function setupDistributionDir() {
  if (IS_MACOSX) {
    // Create files in the old distribution directory location to verify that
    // the directory and its contents are moved to the new location on update.
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
  let distributionDir = getApplyDirFile(DIR_RESOURCES + "distribution", true);
  if (IS_MACOSX) {
    Assert.ok(distributionDir.exists(),
              MSG_SHOULD_EXIST + getMsgPath(distributionDir.path));

    let testFile = getApplyDirFile(DIR_RESOURCES + "distribution/testFile", true);
    Assert.ok(testFile.exists(),
              MSG_SHOULD_EXIST + getMsgPath(testFile.path));

    testFile = getApplyDirFile(DIR_RESOURCES + "distribution/test/testFile", true);
    Assert.ok(testFile.exists(),
              MSG_SHOULD_EXIST + getMsgPath(testFile.path));

    distributionDir = getApplyDirFile(DIR_MACOS + "distribution", true);
    Assert.ok(!distributionDir.exists(),
              MSG_SHOULD_NOT_EXIST + getMsgPath(distributionDir.path));

    checkUpdateLogContains(MOVE_OLD_DIST_DIR);
  } else {
    debugDump("testing that files aren't added with an add-if instruction " +
              "when the file's destination directory doesn't exist");
    Assert.ok(!distributionDir.exists(),
              MSG_SHOULD_NOT_EXIST + getMsgPath(distributionDir.path));
  }
}
