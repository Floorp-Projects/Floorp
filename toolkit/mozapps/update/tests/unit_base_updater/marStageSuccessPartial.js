/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Staged Patch Apply Test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_STAGE = gIsServiceTest ? STATE_APPLIED_SVC : STATE_APPLIED;
  gTestFiles = gTestFilesPartialSuccess;
  const channelPrefs = getTestFileByName(FILE_CHANNEL_PREFS);
  channelPrefs.originalContents = null;
  channelPrefs.compareContents = "FromPartial\n";
  channelPrefs.comparePerms = 0o644;
  gTestDirs = gTestDirsPartialSuccess;
  preventDistributionFiles();
  await setupUpdaterTest(FILE_PARTIAL_MAR, true);
  await stageUpdate(STATE_AFTER_STAGE, true);
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getStageDirFile, true);
  checkUpdateLogContents(LOG_PARTIAL_SUCCESS, true, false, true);
  // Switch the application to the staged application that was updated.
  runUpdate(STATE_SUCCEEDED, true, 0, true);
  await checkPostUpdateAppLog();
  checkAppBundleModTime();
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, true);
  checkUpdateLogContents(LOG_REPLACE_SUCCESS, false, true, true);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}
