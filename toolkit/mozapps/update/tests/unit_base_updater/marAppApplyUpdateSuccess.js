/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by staging an update and launching an application to
 * apply it.
 */

const START_STATE = STATE_PENDING;
const END_STATE = STATE_SUCCEEDED;

function run_test() {
  if (MOZ_APP_NAME == "xulrunner") {
    logTestInfo("Unable to run this test on xulrunner");
    return;
  }

  setupTestCommon();
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setupUpdaterTest(FILE_COMPLETE_MAR);

  createUpdaterINI();
  setAppBundleModTime();

  let channel = Services.prefs.getCharPref(PREF_APP_UPDATE_CHANNEL);
  let patches = getLocalPatchString(null, null, null, null, null, "true",
                                    START_STATE);
  let updates = getLocalUpdateString(patches, null, null, null, null, null,
                                     null, null, null, null, null, null,
                                     null, "true", channel);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeVersionFile(getAppVersion());
  writeStatusFile(START_STATE);

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  do_timeout(TEST_CHECK_TIMEOUT, launchAppToApplyUpdate);
}

/**
 * Checks if the post update binary was properly launched for the platforms that
 * support launching post update process.
 */
function checkUpdateFinished() {
  if (IS_WIN || IS_MACOSX) {
    gCheckFunc = finishCheckUpdateFinished;
    checkPostUpdateAppLog();
  } else {
    finishCheckUpdateFinished();
  }
}

/**
 * Checks if the update has finished and if it has finished performs checks for
 * the test.
 */
function finishCheckUpdateFinished() {
  gTimeoutRuns++;
  // Don't proceed until the update's status state is the expected value.
  let state = readStatusState();
  if (state != END_STATE) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the update " +
               "status state to equal: " + END_STATE +
               ", current status state: " + state);
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
    }
    return;
  }

  // Don't proceed until the update log has been created.
  let log = getUpdatesPatchDir();
  log.append(FILE_UPDATE_LOG);
  if (!log.exists()) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the update log " +
               "to be created. Path: " + log.path);
    }
    do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
    return;
  }

  if (IS_WIN) {
    // Don't proceed until the updater binary is no longer in use.
    let updater = getUpdatesPatchDir();
    updater.append(FILE_UPDATER_BIN);
    if (updater.exists()) {
      if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
        do_throw("Exceeded while waiting for updater binary to no longer be " +
                 "in use");
      }
      try {
        updater.remove(false);
      } catch (e) {
        do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
        return;
      }
    }
  }

  checkAppBundleModTime();
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, false);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);

  standardInit();

  let update = gUpdateManager.getUpdateAt(0);
  Assert.equal(update.state, END_STATE,
               "the update state" + MSG_SHOULD_EQUAL);

  let updatesDir = getUpdatesPatchDir();
  Assert.ok(updatesDir.exists(), MSG_SHOULD_EXIST);

  log = getUpdatesPatchDir();
  log.append(FILE_UPDATE_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  log = getUpdatesDir();
  log.append(FILE_LAST_LOG);
  Assert.ok(log.exists(), MSG_SHOULD_EXIST);

  log = getUpdatesDir();
  log.append(FILE_BACKUP_LOG);
  Assert.ok(!log.exists(), MSG_SHOULD_NOT_EXIST);

  waitForFilesInUse();
}
