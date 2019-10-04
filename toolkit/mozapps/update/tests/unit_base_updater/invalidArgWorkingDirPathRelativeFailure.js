/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Relative working directory path failure test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_RUNUPDATE = gIsServiceTest
    ? STATE_FAILED_SERVICE_INVALID_WORKING_DIR_PATH_ERROR
    : STATE_FAILED_INVALID_WORKING_DIR_PATH_ERROR;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  runUpdate(STATE_AFTER_RUNUPDATE, false, 1, true, null, null, "test", null);
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  await waitForUpdateXMLFiles();
  if (gIsServiceTest) {
    checkUpdateManager(
      STATE_NONE,
      false,
      STATE_FAILED,
      SERVICE_INVALID_WORKING_DIR_PATH_ERROR,
      1
    );
  } else {
    checkUpdateManager(
      STATE_NONE,
      false,
      STATE_FAILED,
      INVALID_WORKING_DIR_PATH_ERROR,
      1
    );
  }

  waitForFilesInUse();
}
