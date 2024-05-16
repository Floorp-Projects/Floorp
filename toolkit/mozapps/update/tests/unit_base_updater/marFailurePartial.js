/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Patch Apply Failure Test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesPartialSuccess;
  getTestFileByName("0exe0.exe").originalFile = "partial.png";
  gTestDirs = gTestDirsPartialSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_PARTIAL_MAR, false);
  // If execv is used the updater process will turn into the callback process
  // and the updater's return code will be that of the callback process.
  runUpdate(
    STATE_FAILED_LOADSOURCE_ERROR_WRONG_SIZE,
    false,
    USE_EXECV ? 0 : 1,
    true
  );
  checkAppBundleModTime();
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContents(LOG_PARTIAL_FAILURE);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(
    STATE_NONE,
    false,
    STATE_FAILED,
    LOADSOURCE_ERROR_WRONG_SIZE,
    1
  );
  checkCallbackLog();
}
