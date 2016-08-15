/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Patch Apply Failure Test */

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesPartialSuccess;
  gTestFiles[11].originalFile = "partial.png";
  gTestDirs = gTestDirsPartialSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_PARTIAL_MAR, false);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  // If execv is used the updater process will turn into the callback process
  // and the updater's return code will be that of the callback process.
  runUpdate(STATE_FAILED_LOADSOURCE_ERROR_WRONG_SIZE, false,
            (USE_EXECV ? 0 : 1), true);
}

/**
 * Called after the call to runUpdate finishes.
 */
function runUpdateFinished() {
  checkAppBundleModTime();
  standardInit();
  Assert.equal(readStatusFile(), STATE_NONE,
               "the status file failure code" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.updateCount, 1,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.getUpdateAt(0).state, STATE_FAILED,
               "the update state" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.getUpdateAt(0).errorCode, LOADSOURCE_ERROR_WRONG_SIZE,
               "the update errorCode" + MSG_SHOULD_EQUAL);
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContents(LOG_PARTIAL_FAILURE);
  checkCallbackLog();
}
