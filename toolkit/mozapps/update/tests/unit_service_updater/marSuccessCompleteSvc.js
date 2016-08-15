/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Complete MAR File Patch Apply Test */

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  preventDistributionFiles();
  setupDistributionDir();
  setupUpdaterTest(FILE_COMPLETE_MAR, true);
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
  checkPostUpdateAppLog();
}

/**
 * Called after the call to checkPostUpdateAppLog finishes.
 */
function checkPostUpdateAppLogFinished() {
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
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS, false, false, true);
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
