/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Patch Apply Failure Test */

function run_test() {
  setupTestCommon();
  gTestFiles = gTestFilesPartialSuccess;
  gTestFiles[11].originalFile = "partial.png";
  gTestDirs = gTestDirsPartialSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_PARTIAL_MAR);

  createUpdaterINI();
  setAppBundleModTime();

  // Note that on platforms where we use execv, we cannot trust the return code.
  runUpdate((USE_EXECV ? 0 : 1), STATE_FAILED_LOADSOURCE_ERROR_WRONG_SIZE,
            checkUpdateFinished);
}

/**
 * Checks if the update has finished and if it has finished performs checks for
 * the test.
 */
function checkUpdateFinished() {
  checkPostUpdateRunningFile(false);
  checkAppBundleModTime();
  checkFilesAfterUpdateFailure(getApplyDirFile, false, false);
  checkUpdateLogContents(LOG_PARTIAL_FAILURE);
  standardInit();
  checkCallbackAppLog();
}
