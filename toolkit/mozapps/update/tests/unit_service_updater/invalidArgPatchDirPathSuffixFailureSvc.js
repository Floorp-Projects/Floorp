/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Patch directory path must end with \updates\0 failure test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_RUNUPDATE = gIsServiceTest
    ? STATE_PENDING_SVC
    : STATE_PENDING;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  let path = getUpdateDirFile(DIR_PATCH).parent.path;
  runUpdate(STATE_AFTER_RUNUPDATE, false, 1, true, path, null, null, null);
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(STATE_NONE, false, STATE_AFTER_RUNUPDATE, 0, 1);
  waitForFilesInUse();
}
