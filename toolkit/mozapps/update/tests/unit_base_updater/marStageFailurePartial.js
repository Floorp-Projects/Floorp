/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Staged Patch Apply Failure Test */

const STATE_AFTER_STAGE = STATE_FAILED;
gStagingRemovedUpdate = true;

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesPartialSuccess;
  gTestFiles[11].originalFile = "partial.png";
  gTestDirs = gTestDirsPartialSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_PARTIAL_MAR, false);
  stageUpdate(true);
}

/**
 * Called after the call to stageUpdate finishes.
 */
async function stageUpdateFinished() {
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(ERR_LOADSOURCEFILE_FAILED);
  await waitForUpdateXMLFiles();
  checkUpdateManager(STATE_NONE, false, STATE_FAILED,
                     LOADSOURCE_ERROR_WRONG_SIZE, 1);
  waitForFilesInUse();
}
