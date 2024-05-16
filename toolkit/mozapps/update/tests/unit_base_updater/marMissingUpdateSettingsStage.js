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
  const STATE_AFTER_STAGE = STATE_FAILED;
  gTestFiles = gTestFilesCompleteSuccess;
  if (AppConstants.platform == "macosx") {
    // On macOS, the update settings Framework already exists. Remove it.
    const updateSettings = getTestFileByName(FILE_UPDATE_SETTINGS_FRAMEWORK);
    updateSettings.removeOriginalFile = true;
  } else {
    // On non-macOS, the update settings INI will normally be written out with
    // the contents specified by `originalContents`. Setting this to `null`
    // prevents anything from being written out.
    const updateSettings = getTestFileByName(FILE_UPDATE_SETTINGS_INI);
    updateSettings.originalContents = null;
  }
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  await stageUpdate(STATE_AFTER_STAGE, true, true);
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(STATE_FAILED_UPDATE_SETTINGS_FILE_CHANNEL);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(
    STATE_NONE,
    false,
    STATE_FAILED,
    UPDATE_SETTINGS_FILE_CHANNEL,
    1
  );
  waitForFilesInUse();
}
