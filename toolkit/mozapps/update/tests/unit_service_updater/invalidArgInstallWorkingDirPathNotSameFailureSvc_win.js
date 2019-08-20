/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Different install and working directories for a regular update failure */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_RUNUPDATE = gIsServiceTest
    ? STATE_FAILED_SERVICE_INVALID_APPLYTO_DIR_ERROR
    : STATE_FAILED_INVALID_APPLYTO_DIR_ERROR;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  let path = getApplyDirFile("..", false).path;
  runUpdate(STATE_AFTER_RUNUPDATE, false, 1, true, null, null, path, null);
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  await waitForUpdateXMLFiles();
  if (gIsServiceTest) {
    checkUpdateManager(
      STATE_NONE,
      false,
      STATE_FAILED,
      SERVICE_INVALID_APPLYTO_DIR_ERROR,
      1
    );
  } else {
    checkUpdateManager(
      STATE_NONE,
      false,
      STATE_FAILED,
      INVALID_APPLYTO_DIR_ERROR,
      1
    );
  }

  waitForFilesInUse();
}
