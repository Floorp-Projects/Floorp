/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by staging an update and launching an application to
 * apply it.
 */

const STATE_AFTER_STAGE = STATE_PENDING;

function run_test() {
  if (!setupTestCommon()) {
    return;
  }

  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  createUpdateInProgressLockFile(getAppBaseDir());
  setupUpdaterTest(FILE_COMPLETE_MAR, false);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  stageUpdate();
}

/**
 * Called after the call to stageUpdate finishes.
 */
function stageUpdateFinished() {
  removeUpdateInProgressLockFile(getAppBaseDir());
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(PERFORMING_STAGED_UPDATE);
  checkUpdateLogContains(ERR_UPDATE_IN_PROGRESS);
  waitForFilesInUse();
}
