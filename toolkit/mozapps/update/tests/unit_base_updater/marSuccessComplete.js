/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Complete MAR File Patch Apply Test */

function run_test() {
  setupTestCommon();
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  preventDistributionFiles();
  setupUpdaterTest(FILE_COMPLETE_MAR);
  if (IS_MACOSX) {
    // Create files in the old distribution directory location to verify that
    // the directory and its contents are moved to the new location on update.
    let testFile = getApplyDirFile(DIR_MACOS + "distribution/testFile", true);
    writeFile(testFile, "test\n");
    testFile = getApplyDirFile(DIR_MACOS + "distribution/test/testFile", true);
    writeFile(testFile, "test\n");
  }

  createUpdaterINI();
  setAppBundleModTime();

  runUpdate(0, STATE_SUCCEEDED, checkUpdateFinished);
}

/**
 * Checks if the post update binary was properly launched for the platforms that
 * support launching post update process.
 */
function checkUpdateFinished() {
  if (IS_WIN || IS_MACOSX) {
    gCheckFunc = finishCheckUpdateFinished;
    checkPostUpdateAppLog();
  } else {
    finishCheckUpdateFinished();
  }
}

/**
 * Checks if the update has finished and if it has finished performs checks for
 * the test.
 */
function finishCheckUpdateFinished() {
  let distributionDir = getApplyDirFile(DIR_RESOURCES + "distribution", true);
  if (IS_MACOSX) {
    Assert.ok(distributionDir.exists(), MSG_SHOULD_EXIST);

    let testFile = getApplyDirFile(DIR_RESOURCES + "distribution/testFile", true);
    Assert.ok(testFile.exists(), MSG_SHOULD_EXIST);

    testFile = getApplyDirFile(DIR_RESOURCES + "distribution/test/testFile", true);
    Assert.ok(testFile.exists(), MSG_SHOULD_EXIST);

    distributionDir = getApplyDirFile(DIR_MACOS + "distribution", true);
    Assert.ok(!distributionDir.exists(), MSG_SHOULD_NOT_EXIST);

    checkUpdateLogContains("Moving old distribution directory to new location");
  } else {
    debugDump("testing that files aren't added with an add-if instruction " +
              "when the file's destination directory doesn't exist");
    Assert.ok(!distributionDir.exists(), MSG_SHOULD_NOT_EXIST);
  }

  checkAppBundleModTime();
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, false);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS, true);
  standardInit();
  checkCallbackAppLog();
}
