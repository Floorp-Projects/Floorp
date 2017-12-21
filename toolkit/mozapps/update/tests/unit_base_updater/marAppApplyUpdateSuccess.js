/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by staging an update and launching an application to
 * apply it.
 */

function run_test() {
  if (!setupTestCommon()) {
    return;
  }

  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  // The third parameter will test that a full path to the post update binary
  // doesn't execute.
  setupUpdaterTest(FILE_COMPLETE_MAR, undefined,
                   getApplyDirFile(null, true).path + "/");
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  runUpdateUsingApp(STATE_SUCCEEDED);
}

/**
 * Called after the call to runUpdateUsingApp finishes.
 */
function runUpdateFinished() {
  checkAppBundleModTime();
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);
  executeSoon(waitForUpdateXMLFiles);
}

/**
 * Called after the call to waitForUpdateXMLFiles finishes.
 */
function waitForUpdateXMLFilesFinished() {
  checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);

  let updatesDir = getUpdatesPatchDir();
  Assert.ok(updatesDir.exists(),
            MSG_SHOULD_EXIST + getMsgPath(updatesDir.path));

  let log = getUpdateLog(FILE_UPDATE_LOG);
  Assert.ok(!log.exists(),
            MSG_SHOULD_NOT_EXIST + getMsgPath(log.path));

  log = getUpdateLog(FILE_LAST_UPDATE_LOG);
  Assert.ok(log.exists(),
            MSG_SHOULD_EXIST + getMsgPath(log.path));

  log = getUpdateLog(FILE_BACKUP_UPDATE_LOG);
  Assert.ok(!log.exists(),
            MSG_SHOULD_NOT_EXIST + getMsgPath(log.path));

  waitForFilesInUse();
}
