/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Application in use complete MAR file staged patch apply success test */

const START_STATE = STATE_PENDING;
const STATE_AFTER_STAGE = STATE_APPLIED;

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestFiles[gTestFiles.length - 1].originalContents = null;
  gTestFiles[gTestFiles.length - 1].compareContents = "FromComplete\n";
  gTestFiles[gTestFiles.length - 1].comparePerms = 0o644;
  gTestDirs = gTestDirsCompleteSuccess;
  setupUpdaterTest(FILE_COMPLETE_MAR, true);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  setupSymLinks();
  runHelperFileInUse(DIR_RESOURCES + gCallbackBinFile, false);
}

/**
 * Called after the call to waitForHelperSleep finishes.
 */
function waitForHelperSleepFinished() {
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
  waitForHelperExit();
}

/**
 * Called after the call to waitForHelperExit finishes.
 */
function waitForHelperExitFinished() {
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
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_REPLACE_SUCCESS, false, true);
  checkCallbackLog();
}

/**
 * Setup symlinks for the test.
 */
function setupSymLinks() {
  // The tests don't support symlinks on gonk.
  if (IS_UNIX && !IS_TOOLKIT_GONK) {
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
  if (IS_UNIX && !IS_TOOLKIT_GONK) {
    checkSymlink();
  }
}
