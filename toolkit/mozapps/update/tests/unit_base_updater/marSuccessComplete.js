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

  // For Mac OS X set the last modified time for the root directory to a date in
  // the past to test that the last modified time is updated on a successful
  // update (bug 600098).
  if (IS_MACOSX) {
    let now = Date.now();
    let yesterday = now - (1000 * 60 * 60 * 24);
    let applyToDir = getApplyDirFile();
    applyToDir.lastModifiedTime = yesterday;
  }

  runUpdate(0, STATE_SUCCEEDED);
}

/**
 * Checks if the post update binary was properly launched for the platforms that
 * support launching post update process.
 */
function checkUpdateApplied() {
  if (IS_WIN || IS_MACOSX) {
    gCheckFunc = finishCheckUpdateApplied;
    checkPostUpdateAppLog();
  } else {
    finishCheckUpdateApplied();
  }
}

/**
 * Checks if the update has finished and if it has finished performs checks for
 * the test.
 */
function finishCheckUpdateApplied() {
  if (IS_MACOSX) {
    logTestInfo("testing last modified time on the apply to directory has " +
                "changed after a successful update (bug 600098)");
    let now = Date.now();
    let applyToDir = getApplyDirFile();
    let timeDiff = Math.abs(applyToDir.lastModifiedTime - now);
    do_check_true(timeDiff < MAC_MAX_TIME_DIFFERENCE);
  }

  let distributionDir = getApplyDirFile(DIR_RESOURCES + "distribution", true);
  if (IS_MACOSX) {
    logTestInfo("testing that the distribution directory is moved from the " +
                "old location to the new location");
    logTestInfo("testing " + distributionDir.path + " should exist");
    do_check_true(distributionDir.exists());

    let testFile = getApplyDirFile(DIR_RESOURCES + "distribution/testFile", true);
    logTestInfo("testing " + testFile.path + " should exist");
    do_check_true(testFile.exists());

    testFile = getApplyDirFile(DIR_RESOURCES + "distribution/test/testFile", true);
    logTestInfo("testing " + testFile.path + " should exist");
    do_check_true(testFile.exists());

    distributionDir = getApplyDirFile(DIR_MACOS + "distribution", true);
    logTestInfo("testing " + distributionDir.path + " shouldn't exist");
    do_check_false(distributionDir.exists());

    checkUpdateLogContains("Moving old distribution directory to new location");
  } else {
    logTestInfo("testing that files aren't added with an add-if instruction " +
                "when the file's destination directory doesn't exist");
    logTestInfo("testing " + distributionDir.path + " shouldn't exist");
    do_check_false(distributionDir.exists());
  }

  checkFilesAfterUpdateSuccess(getApplyDirFile, false, false);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS, true);
  checkCallbackAppLog();
}
