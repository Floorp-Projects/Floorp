/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Too long install directory path failure test */

const STATE_AFTER_RUNUPDATE =
  IS_SERVICE_TEST ? STATE_FAILED_SERVICE_INVALID_INSTALL_DIR_PATH_ERROR
                  : STATE_FAILED_INVALID_INSTALL_DIR_PATH_ERROR;

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_COMPLETE_MAR, false);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  let path = "123456789";
  if (IS_WIN) {
    path = "\\" + path;
    path = path.repeat(30); // 300 characters
    path = "C:" + path;
  } else {
    path = "/" + path;
    path = path.repeat(1000); // 10000 characters
  }

  runUpdate(STATE_AFTER_RUNUPDATE, false, 1, true, null, path, null, null);
}

/**
 * Called after the call to runUpdateUsingUpdater finishes.
 */
function runUpdateFinished() {
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  waitForFilesInUse();
}
