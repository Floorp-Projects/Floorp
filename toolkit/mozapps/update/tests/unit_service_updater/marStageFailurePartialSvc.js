/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Staged Patch Apply Failure Test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_STAGE = STATE_FAILED;
  gTestFiles = gTestFilesPartialSuccess;
  getTestFileByName("0exe0.exe").originalFile = "partial.png";
  gTestDirs = gTestDirsPartialSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_PARTIAL_MAR, false);
  await stageUpdate(STATE_AFTER_STAGE, true, true);
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_LOADSOURCEFILE_FAILED);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(
    STATE_NONE,
    false,
    STATE_FAILED,
    LOADSOURCE_ERROR_WRONG_SIZE,
    1
  );
  waitForFilesInUse();
}
