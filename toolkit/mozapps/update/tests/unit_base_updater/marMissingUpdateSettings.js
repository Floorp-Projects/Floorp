/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Test update-settings.ini missing channel MAR security check */

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
  // If execv is used the updater process will turn into the callback process
  // and the updater's return code will be that of the callback process.
  runUpdate(
    STATE_FAILED_UPDATE_SETTINGS_FILE_CHANNEL,
    false,
    USE_EXECV ? 0 : 1,
    false
  );
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(STATE_FAILED_UPDATE_SETTINGS_FILE_CHANNEL);
  await waitForUpdateXMLFiles();
  checkUpdateManager(
    STATE_NONE,
    false,
    STATE_FAILED,
    UPDATE_SETTINGS_FILE_CHANNEL,
    1
  );
  waitForFilesInUse();
}
