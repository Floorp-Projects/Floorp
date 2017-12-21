/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Different install and working directories for a regular update failure */

const STATE_AFTER_RUNUPDATE =
  IS_SERVICE_TEST ? STATE_FAILED_SERVICE_INVALID_APPLYTO_DIR_STAGED_ERROR
                  : STATE_FAILED_INVALID_APPLYTO_DIR_STAGED_ERROR;

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
  let path = getApplyDirFile("..", false).path;
  runUpdate(STATE_AFTER_RUNUPDATE, true, 1, true, null, null, path, null);
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
  let errorCode = IS_SERVICE_TEST ? SERVICE_INVALID_APPLYTO_DIR_STAGED_ERROR
                                  : INVALID_APPLYTO_DIR_STAGED_ERROR;
  checkUpdateManager(STATE_NONE, false, STATE_FAILED, errorCode, 1);
  waitForFilesInUse();
}
