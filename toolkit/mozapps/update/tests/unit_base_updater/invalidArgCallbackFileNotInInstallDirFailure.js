/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Callback file not in install directory or a sub-directory of the install
   directory failure */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_RUNUPDATE = STATE_FAILED_INVALID_CALLBACK_DIR_ERROR;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  let path = getTestDirFile(FILE_HELPER_BIN).path;
  runUpdate(STATE_AFTER_RUNUPDATE, false, 1, true, null, null, null, path);
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(
    STATE_NONE,
    false,
    STATE_FAILED,
    INVALID_CALLBACK_DIR_ERROR,
    1
  );
  waitForFilesInUse();
}
