/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Test product/channel MAR security check */

async function run_test() {
  if (!MOZ_VERIFY_MAR_SIGNATURE) {
    return;
  }

  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  setUpdateSettingsUseWrongChannel();
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  // If execv is used the updater process will turn into the callback process
  // and the updater's return code will be that of the callback process.
  runUpdate(
    STATE_FAILED_MAR_CHANNEL_MISMATCH_ERROR,
    false,
    USE_EXECV ? 0 : 1,
    false
  );
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(STATE_FAILED_MAR_CHANNEL_MISMATCH_ERROR);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(
    STATE_NONE,
    false,
    STATE_FAILED,
    MAR_CHANNEL_MISMATCH_ERROR,
    1
  );
  waitForFilesInUse();
}
