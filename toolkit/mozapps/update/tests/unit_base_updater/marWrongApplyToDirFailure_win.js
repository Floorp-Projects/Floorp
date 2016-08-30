/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Test trying to use an apply-to directory different from the install
 * directory, which should fail.
 */

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_COMPLETE_MAR, false);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  overrideApplyToDir(getApplyDirPath() + "/../NoSuchDir");
  // If execv is used the updater process will turn into the callback process
  // and the updater's return code will be that of the callback process.
  runUpdateUsingUpdater(STATE_FAILED_INVALID_APPLYTO_DIR_ERROR, false,
                        (USE_EXECV ? 0 : 1));
}

/**
 * Called after the call to runUpdateUsingUpdater finishes.
 */
function runUpdateFinished() {
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  waitForFilesInUse();
}
