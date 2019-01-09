/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Too long install directory path failure test */

/* The service cannot safely write update.status for this failure. */
const STATE_AFTER_RUNUPDATE_BASE = STATE_FAILED_INVALID_INSTALL_DIR_PATH_ERROR;
const STATE_AFTER_RUNUPDATE_SERVICE = AppConstants.EARLY_BETA_OR_EARLIER
    ? STATE_PENDING_SVC
    : STATE_FAILED_SERVICE_INVALID_INSTALL_DIR_PATH_ERROR;
const STATE_AFTER_RUNUPDATE = IS_SERVICE_TEST ? STATE_AFTER_RUNUPDATE_SERVICE
                                              : STATE_AFTER_RUNUPDATE_BASE;

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
  executeSoon(waitForUpdateXMLFiles);
}

/**
 * Called after the call to waitForUpdateXMLFiles finishes.
 */
function waitForUpdateXMLFilesFinished() {
  if (IS_SERVICE_TEST) {
    if (AppConstants.EARLY_BETA_OR_EARLIER) {
      checkUpdateManager(STATE_NONE, false, STATE_PENDING_SVC, 0, 1);
    } else {
      checkUpdateManager(STATE_NONE, false, STATE_FAILED,
                         SERVICE_INVALID_INSTALL_DIR_PATH_ERROR, 1);
    }
  } else {
    checkUpdateManager(STATE_NONE, false, STATE_FAILED,
                       INVALID_INSTALL_DIR_PATH_ERROR, 1);
  }

  waitForFilesInUse();
}
