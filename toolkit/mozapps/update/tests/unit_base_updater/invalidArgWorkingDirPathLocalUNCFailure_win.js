/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Working directory path local UNC failure test */

/* The service cannot safely write update.status for this failure because the
 * check is done before validating the installed updater. */
const STATE_AFTER_RUNUPDATE_BASE = STATE_FAILED_INVALID_WORKING_DIR_PATH_ERROR;
const STATE_AFTER_RUNUPDATE_SERVICE = AppConstants.EARLY_BETA_OR_EARLIER
    ? STATE_PENDING_SVC
    : STATE_FAILED_SERVICE_INVALID_WORKING_DIR_PATH_ERROR;
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
  let path = "\\\\.\\" + getApplyDirFile(null, false).path;
  runUpdate(STATE_AFTER_RUNUPDATE, false, 1, true, null, null, path, null);
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
                         SERVICE_INVALID_WORKING_DIR_PATH_ERROR, 1);
    }
  } else {
    checkUpdateManager(STATE_NONE, false, STATE_FAILED,
                       INVALID_WORKING_DIR_PATH_ERROR, 1);
  }

  waitForFilesInUse();
}
