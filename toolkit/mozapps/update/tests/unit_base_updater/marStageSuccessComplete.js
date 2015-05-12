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
  setAppBundleModTime();

  // Don't test symlinks on Mac OS X in this test since it tends to timeout.
  // It is tested on Mac OS X in marAppInUseStageSuccessComplete_unix.js
  // The tests don't support symlinks on gonk.
  if (IS_UNIX && !IS_MACOSX && !IS_TOOLKIT_GONK) {
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
  checkPostUpdateRunningFile(false);

  // Switch the application to the staged application that was updated.
  gStageUpdate = false;
  gSwitchApp = true;
  do_timeout(TEST_CHECK_TIMEOUT, function() {
    runUpdate(0, STATE_SUCCEEDED, checkUpdateApplied);
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
  checkPostUpdateRunningFile(true);

  if (IS_MACOSX) {
    let distributionDir = getApplyDirFile(DIR_MACOS + "distribution", true);
    Assert.ok(!distributionDir.exists(), MSG_SHOULD_NOT_EXIST);
    checkUpdateLogContains("removing old distribution directory");
  }

  // The tests don't support symlinks on gonk.
  if (IS_UNIX && !IS_MACOSX && !IS_TOOLKIT_GONK) {
    checkSymlink();
  }
  checkAppBundleModTime();
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, false);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);
  standardInit();
  checkCallbackAppLog();
}

function runHelperProcess(args) {
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let process = Cc["@mozilla.org/process/util;1"].
                createInstance(Ci.nsIProcess);
  process.init(helperBin);
  debugDump("Running " + helperBin.path + " " + args.join(" "));
  process.run(true, args, args.length);
  Assert.equal(process.exitValue, 0,
               "the helper process exit value should be 0");
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
