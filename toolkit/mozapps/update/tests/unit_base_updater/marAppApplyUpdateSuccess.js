/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by staging an update and launching an application to
 * apply it.
 */

function run_test() {
  if (MOZ_APP_NAME == "xulrunner") {
    logTestInfo("Unable to run this test on xulrunner");
    return;
  }

  setupTestCommon();
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setupUpdaterTest(FILE_COMPLETE_MAR, false, false);

  // For Mac OS X set the last modified time for the root directory to a date in
  // the past to test that the last modified time is updated on a successful
  // update (bug 600098).
  if (IS_MACOSX) {
    let now = Date.now();
    let yesterday = now - (1000 * 60 * 60 * 24);
    let applyToDir = getApplyDirFile();
    applyToDir.lastModifiedTime = yesterday;
  }

  let channel = Services.prefs.getCharPref(PREF_APP_UPDATE_CHANNEL);
  let patches = getLocalPatchString(null, null, null, null, null, "true",
                                    STATE_PENDING);
  let updates = getLocalUpdateString(patches, null, null, null, null, null,
                                     null, null, null, null, null, null,
                                     null, "true", channel);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeVersionFile(getAppVersion());
  writeStatusFile(STATE_PENDING);

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  do_timeout(TEST_CHECK_TIMEOUT, launchAppToApplyUpdate);
}

/**
 * Checks if the update has finished and if it has finished performs checks for
 * the test.
 */
function checkUpdateFinished() {
  gTimeoutRuns++;
  // Don't proceed until the update's status state is the expected value.
  let state = readStatusState();
  if (state != STATE_SUCCEEDED) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the update " +
               "status state to equal: " + STATE_SUCCEEDED +
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
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
    }
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
      } else {
        try {
          updater.remove(false);
        } catch (e) {
          do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
          return;
        }
      }
    }
  }

  if (IS_MACOSX) {
    logTestInfo("testing last modified time on the apply to directory has " +
                "changed after a successful update (bug 600098)");
    let now = Date.now();
    let applyToDir = getApplyDirFile();
    let timeDiff = Math.abs(applyToDir.lastModifiedTime - now);
    do_check_true(timeDiff < MAC_MAX_TIME_DIFFERENCE);
  }

  checkFilesAfterUpdateSuccess();
  // Sorting on Linux is different so skip this check for now.
  if (!IS_UNIX) {
    checkUpdateLogContents(LOG_COMPLETE_SUCCESS);
  }

  checkCallbackAppLog();

  standardInit();

  let update = gUpdateManager.getUpdateAt(0);
  do_check_eq(update.state, STATE_SUCCEEDED);

  log = getUpdatesPatchDir();
  log.append(FILE_UPDATE_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  log = getUpdatesDir();
  log.append(FILE_LAST_LOG);
  logTestInfo("testing " + log.path + " should exist");
  do_check_true(log.exists());

  log = getUpdatesDir();
  log.append(FILE_BACKUP_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  let updatesPatchDir = getUpdatesPatchDir();
  logTestInfo("testing " + updatesPatchDir.path + " should exist");
  do_check_true(updatesPatchDir.exists());

  waitForFilesInUse();
}
