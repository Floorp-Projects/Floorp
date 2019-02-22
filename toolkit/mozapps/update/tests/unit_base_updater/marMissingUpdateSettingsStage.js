/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Test update-settings.ini missing channel MAR security check */

const STATE_AFTER_STAGE = STATE_FAILED;
gStagingRemovedUpdate = true;

async function run_test() {
  if (!MOZ_VERIFY_MAR_SIGNATURE) {
    return;
  }

  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestFiles[gTestFiles.length - 2].originalContents = null;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  stageUpdate(true);
}

/**
 * Called after the call to stageUpdate finishes.
 */
async function stageUpdateFinished() {
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(STATE_FAILED_UPDATE_SETTINGS_FILE_CHANNEL);
  await waitForUpdateXMLFiles();
  checkUpdateManager(STATE_NONE, false, STATE_FAILED,
                     UPDATE_SETTINGS_FILE_CHANNEL, 1);
  waitForFilesInUse();
}
