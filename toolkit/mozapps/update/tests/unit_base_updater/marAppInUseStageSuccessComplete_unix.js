/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Application in use complete MAR file staged patch apply success test */

function run_test() {
  gStageUpdate = true;
  setupTestCommon();
  gTestFiles = gTestFilesCompleteSuccess;
  gTestFiles[gTestFiles.length - 1].originalContents = null;
  gTestFiles[gTestFiles.length - 1].compareContents = "FromComplete\n";
  gTestFiles[gTestFiles.length - 1].comparePerms = 0o644;
  gTestDirs = gTestDirsCompleteSuccess;
  setupUpdaterTest(FILE_COMPLETE_MAR);

  createUpdaterINI(false);
  setAppBundleModTime();

  // The tests don't support symlinks on gonk.
  if (IS_UNIX && !IS_TOOLKIT_GONK) {
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

  // Launch the callback helper application so it is in use during the update
  let callbackApp = getApplyDirFile(DIR_RESOURCES + gCallbackBinFile);
  callbackApp.permissions = PERMS_DIRECTORY;
  let args = [getApplyDirPath() + DIR_RESOURCES, "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT];
  let callbackAppProcess = Cc["@mozilla.org/process/util;1"].
                           createInstance(Ci.nsIProcess);
  callbackAppProcess.init(callbackApp);
  callbackAppProcess.run(false, args, args.length);

  do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
}

function doUpdate() {
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
  checkAppBundleModTime();
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, false);
  setupHelperFinish();
}

function checkUpdate() {
  // The tests don't support symlinks on gonk.
  if (IS_UNIX && !IS_TOOLKIT_GONK) {
    checkSymlink();
  }
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
          getApplyDirFile().path +"/" + DIR_RESOURCES + "link2", "change-perm"];
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
