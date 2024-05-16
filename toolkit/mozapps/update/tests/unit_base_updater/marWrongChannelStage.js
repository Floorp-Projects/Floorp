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
  const STATE_AFTER_STAGE = STATE_FAILED;
  gTestFiles = gTestFilesCompleteSuccess;
  setUpdateSettingsUseWrongChannel();
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  await stageUpdate(STATE_AFTER_STAGE, true, true);
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
