/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Complete MAR File Staged Patch Apply Test */

function run_test() {
  gStageUpdate = true;
  setupTestCommon();
  gTestFiles = gTestFilesCompleteSuccess;
  gTestFiles[gTestFiles.length - 1].originalContents = null;
  gTestFiles[gTestFiles.length - 1].compareContents = "FromComplete\n";
  gTestFiles[gTestFiles.length - 1].comparePerms = 0o644;
  gTestDirs = gTestDirsCompleteSuccess;
  setupUpdaterTest(FILE_COMPLETE_MAR);
  if (IS_MACOSX) {
    // Create files in the old distribution directory location to verify that
    // the directory and its contents are removed when there is a distribution
    // directory in the new location.
    let testFile = getApplyDirFile(DIR_MACOS + "distribution/testFile", true);
    writeFile(testFile, "test\n");
    testFile = getApplyDirFile(DIR_MACOS + "distribution/test1/testFile", true);
    writeFile(testFile, "test\n");
  }

  createUpdaterINI(false);

  // For Mac OS X set the last modified time for the root directory to a date in
  // the past to test that the last modified time is updated on a successful
  // update (bug 600098).
  if (IS_MACOSX) {
    let now = Date.now();
    let yesterday = now - (1000 * 60 * 60 * 24);
    let applyToDir = getApplyDirFile();
    applyToDir.lastModifiedTime = yesterday;
  }

  // Don't test symlinks on Mac OS X in this test since it tends to timeout.
  // It is tested on Mac OS X in marAppInUseStageSuccessComplete_unix.js
  if (IS_UNIX && !IS_MACOSX) {
    removeSymlink();
    createSymlink();
    do_register_cleanup(removeSymlink);
    gTestFiles.splice(gTestFiles.length - 3, 0,
    {
      description      : "Readable symlink",
      fileName         : "link",
      relPathDir       : DIR_RESOURCES,
      originalContents : "test",
      compareContents  : "test",
      originalFile     : null,
      compareFile      : null,
      originalPerms    : 0o666,
      comparePerms     : 0o666
    });
  }

  runUpdate(0, STATE_APPLIED, null);

  checkFilesAfterUpdateSuccess(getStageDirFile, true, false);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);

  if (IS_WIN || IS_MACOSX) {
    // Check that the post update process was not launched when staging an
    // update.
    do_check_false(getPostUpdateFile(".running").exists());
  }

  // Switch the application to the staged application that was updated.
  gStageUpdate = false;
  gSwitchApp = true;
  do_timeout(TEST_CHECK_TIMEOUT, function() {
    runUpdate(0, STATE_SUCCEEDED);
  });
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

  if (IS_MACOSX) {
    logTestInfo("testing that the distribution directory is removed from the " +
                "old location when there is a distribution directory in the " +
                "new location");
    let distributionDir = getApplyDirFile(DIR_MACOS + "distribution", true);
    logTestInfo("testing " + distributionDir.path + " shouldn't exist");
    do_check_false(distributionDir.exists());

    checkUpdateLogContains("removing old distribution directory");
  }

  if (IS_UNIX && !IS_MACOSX) {
    checkSymlink();
  }
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, false);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);
  checkCallbackAppLog();
}

function runHelperProcess(args) {
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let process = AUS_Cc["@mozilla.org/process/util;1"].
                createInstance(AUS_Ci.nsIProcess);
  process.init(helperBin);
  logTestInfo("Running " + helperBin.path + " " + args.join(" "));
  process.run(true, args, args.length);
  do_check_eq(process.exitValue, 0);
}

function createSymlink() {
  let args = ["setup-symlink", "moz-foo", "moz-bar", "target",
              getApplyDirFile().path + "/" + DIR_RESOURCES + "link"];
  runHelperProcess(args);
  getApplyDirFile(DIR_RESOURCES + "link", false).permissions = 0o666;
  
  args = ["setup-symlink", "moz-foo2", "moz-bar2", "target2",
          getApplyDirFile().path + "/" + DIR_RESOURCES + "link2", "change-perm"];
  runHelperProcess(args);
}

function removeSymlink() {
  let args = ["remove-symlink", "moz-foo", "moz-bar", "target",
              getApplyDirFile().path + "/" + DIR_RESOURCES + "link"];
  runHelperProcess(args);
  args = ["remove-symlink", "moz-foo2", "moz-bar2", "target2",
          getApplyDirFile().path + "/" + DIR_RESOURCES + "link2"];
  runHelperProcess(args);
}

function checkSymlink() {
  let args = ["check-symlink",
              getApplyDirFile().path + "/" + DIR_RESOURCES + "link"];
  runHelperProcess(args);
}
