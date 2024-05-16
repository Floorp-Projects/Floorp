/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by staging an update and launching an application to
 * apply it.
 */

async function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  const STATE_AFTER_STAGE = STATE_PENDING_SVC;
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  createUpdateInProgressLockFile(getGREBinDir());
  await setupUpdaterTest(FILE_COMPLETE_MAR, false);
  await stageUpdate(STATE_AFTER_STAGE, false);
  removeUpdateInProgressLockFile(getGREBinDir());
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  checkUpdateLogContains(PERFORMING_STAGED_UPDATE);
  checkUpdateLogContains(ERR_UPDATE_IN_PROGRESS);
  await waitForUpdateXMLFiles(true, false);
  await checkUpdateManager(STATE_AFTER_STAGE, true, STATE_AFTER_STAGE, 0, 0);
  waitForFilesInUse();
}
