/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Patch Apply Test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesPartialSuccess;
  const channelPrefs = getTestFileByName(FILE_CHANNEL_PREFS);
  channelPrefs.originalContents = null;
  channelPrefs.compareContents = "FromPartial\n";
  channelPrefs.comparePerms = 0o644;
  gTestDirs = gTestDirsPartialSuccess;
  // The third parameter will test that a relative path that contains a
  // directory traversal to the post update binary doesn't execute.
  await setupUpdaterTest(FILE_PARTIAL_MAR, false, "test/../");
  runUpdate(STATE_SUCCEEDED, false, 0, true);
  checkAppBundleModTime();
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_PARTIAL_SUCCESS);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}
