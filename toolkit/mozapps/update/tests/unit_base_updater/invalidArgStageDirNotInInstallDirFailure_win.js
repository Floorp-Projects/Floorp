/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Different install and working directories for a regular update failure */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_RUNUPDATE = gIsServiceTest
    ? STATE_FAILED_SERVICE_INVALID_APPLYTO_DIR_STAGED_ERROR
    : STATE_FAILED_INVALID_APPLYTO_DIR_STAGED_ERROR;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  let path = getApplyDirFile("..", false).path;
  runUpdate(STATE_AFTER_RUNUPDATE, true, 1, true, null, null, path, null);
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  await waitForUpdateXMLFiles();
  if (gIsServiceTest) {
    // The invalid argument service tests launch the maintenance service
    // directly so the unelevated updater doesn't handle the invalid argument.
    // By doing this it is possible to test that the maintenance service
    // properly handles the invalid argument but since the updater isn't used to
    // launch the maintenance service the update.status file isn't copied from
    // the secure log directory to the patch directory and the update manager
    // won't read the failure from the update.status file.
    await checkUpdateManager(STATE_NONE, false, STATE_PENDING_SVC, 0, 1);
  } else {
    await checkUpdateManager(
      STATE_NONE,
      false,
      STATE_FAILED,
      INVALID_APPLYTO_DIR_STAGED_ERROR,
      1
    );
  }

  waitForFilesInUse();
}
