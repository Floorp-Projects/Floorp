/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Too long install directory path failure test */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_RUNUPDATE = gIsServiceTest
    ? STATE_FAILED_SERVICE_INVALID_INSTALL_DIR_PATH_ERROR
    : STATE_FAILED_INVALID_INSTALL_DIR_PATH_ERROR;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  let path = "123456789";
  if (AppConstants.platform == "win") {
    path = "\\" + path;
    path = path.repeat(30); // 300 characters
    path = "C:" + path;
  } else {
    path = "/" + path;
    path = path.repeat(1000); // 10000 characters
  }
  runUpdate(STATE_AFTER_RUNUPDATE, false, 1, true, null, path, null, null);
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
      INVALID_INSTALL_DIR_PATH_ERROR,
      1
    );
  }

  waitForFilesInUse();
}
