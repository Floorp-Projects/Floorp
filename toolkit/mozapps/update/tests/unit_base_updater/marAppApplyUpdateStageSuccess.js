/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by staging an update and launching an application to
 * apply it.
 */

/**
 * The MAR file used for this test should not contain a version 2 update
 * manifest file (e.g. updatev2.manifest).
 */

function run_test() {
  if (MOZ_APP_NAME == "xulrunner") {
    logTestInfo("Unable to run this test on xulrunner");
    return;
  }

  setupTestCommon();

  if (IS_WIN) {
    Services.prefs.setBoolPref(PREF_APP_UPDATE_SERVICE_ENABLED, false);
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

  let updatesPatchDir = getUpdatesPatchDir();
  let mar = getTestDirFile(FILE_SIMPLE_MAR);
  mar.copyTo(updatesPatchDir, FILE_UPDATE_ARCHIVE);

  let updateSettingsIni = getApplyDirFile(FILE_UPDATE_SETTINGS_INI, true);
  writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);

  reloadUpdateManagerData();
  do_check_true(!!gUpdateManager.activeUpdate);

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  stageUpdate();
}

/**
 * Checks if the update has finished being staged.
 */
function checkUpdateApplied() {
  gTimeoutRuns++;
  // Don't proceed until the update state is applied.
  if (gUpdateManager.activeUpdate.state != STATE_APPLIED) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for update to be " +
               "applied, current state is: " +
               gUpdateManager.activeUpdate.state);
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    }
    return;
  }

  // Don't proceed until the update status state is applied.
  let state = readStatusState();
  if (state != STATE_APPLIED) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the update " +
               "status state to equal " + STATE_APPLIED + ", " +
               "current status state: " + state);
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    }
    return;
  }

  // Don't proceed until the last update log has been created.
  let log;
  if (IS_WIN) {
    log = getUpdatesDir();
  } else {
    log = getUpdatedDir();
    if (IS_MACOSX) {
      log.append("Contents");
      log.append("MacOS");
    }
    log.append(DIR_UPDATES);
  }
  log.append(FILE_LAST_LOG);
  if (!log.exists()) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the update log " +
               "to be created. Path: " + log.path);
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    }
    return;
  }

  // Log the contents of the update.log so it is simpler to diagnose a test
  // failure.
  let contents = readFile(log);
  logTestInfo("contents of " + log.path + ":\n" +
              contents.replace(/\r\n/g, "\n"));

  let updatedDir = getUpdatedDir();
  logTestInfo("testing " + updatedDir.path + " should exist");
  do_check_true(updatedDir.exists());

  let updateTestDir = getUpdateTestDir();
  logTestInfo("testing " + updateTestDir.path + " shouldn't exist");
  do_check_false(updateTestDir.exists());

  updateTestDir = getUpdatedDir();
  updateTestDir.append("update_test");
  let file = updateTestDir.clone();
  file.append("UpdateTestRemoveFile");
  logTestInfo("testing " + file.path + " shouldn't exist");
  do_check_false(file.exists());

  file = updateTestDir.clone();
  file.append("UpdateTestAddFile");
  logTestInfo("testing " + file.path + " should exist");
  do_check_true(file.exists());
  do_check_eq(readFileBytes(file), "UpdateTestAddFile\n");

  file = updateTestDir.clone();
  file.append("removed-files");
  logTestInfo("testing " + file.path + " should exist");
  do_check_true(file.exists());
  do_check_eq(readFileBytes(file), "update_test/UpdateTestRemoveFile\n");

  log = getUpdatesDir();
  log.append("0");
  log.append(FILE_UPDATE_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  log = getUpdatesDir();
  log.append(FILE_LAST_LOG);
  if (IS_WIN) {
    logTestInfo("testing " + log.path + " should exist");
    do_check_true(log.exists());
  } else {
    logTestInfo("testing " + log.path + " shouldn't exist");
    do_check_false(log.exists());
  }

  log = getUpdatesDir();
  log.append(FILE_BACKUP_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  let updatesDir = getUpdatedDir();
  if (IS_MACOSX) {
    updatesDir.append("Contents");
    updatesDir.append("MacOS");
  }
  updatesDir.append("updates");
  log = updatesDir.clone();
  log.append("0");
  log.append(FILE_UPDATE_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  if (!IS_WIN) {
    log = updatesDir.clone();
    log.append(FILE_LAST_LOG);
    logTestInfo("testing " + log.path + " should exist");
    do_check_true(log.exists());
  }

  log = updatesDir.clone();
  log.append(FILE_BACKUP_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  updatesDir.append("0");
  logTestInfo("testing " + updatesDir.path + " shouldn't exist");
  do_check_false(updatesDir.exists());

  // On Windows, make sure not to use the maintenance service for switching
  // the app.
  if (IS_WIN) {
    writeStatusFile(STATE_APPLIED);
    do_check_eq(readStatusState(), STATE_APPLIED);
  }

  // Now, switch the updated version of the app
  do_timeout(TEST_CHECK_TIMEOUT, launchAppToApplyUpdate);
}

/**
 * Checks if the update has finished and if it has finished performs checks for
 * the test.
 */
function checkUpdateFinished() {
  gTimeoutRuns++;
  // Don't proceed until the update status state is succeeded.
  let state = readStatusState();
  if (state != STATE_SUCCEEDED) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the update " +
               "status state to equal " + STATE_SUCCEEDED + ", " +
               "current status state: " + state);
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
    }
    return;
  }

  // Don't proceed until the application was switched out with the staged update
  // successfully.
  let updatedDir = getUpdatedDir();
  if (updatedDir.exists()) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded while waiting for update dir to not exist");
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
    }
    return;
  }

  let updater = getUpdatesPatchDir();
  updater.append(FILE_UPDATER_BIN);
  if (updater.exists()) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded while waiting for updater binary to no longer be in " +
               "use");
    } else {
      try {
        updater.remove(false);
      } catch (e) {
        do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
        return;
      }
    }
  }

  let updateTestDir = getUpdateTestDir();

  let file = updateTestDir.clone();
  file.append("UpdateTestRemoveFile");
  logTestInfo("testing " + file.path + " shouldn't exist");
  do_check_false(file.exists());

  file = updateTestDir.clone();
  file.append("UpdateTestAddFile");
  logTestInfo("testing " + file.path + " should exist");
  do_check_true(file.exists());
  do_check_eq(readFileBytes(file), "UpdateTestAddFile\n");

  file = updateTestDir.clone();
  file.append("removed-files");
  logTestInfo("testing " + file.path + " should exist");
  do_check_true(file.exists());
  do_check_eq(readFileBytes(file), "update_test/UpdateTestRemoveFile\n");

  let log = getUpdatesDir();
  log.append("0");
  log.append(FILE_UPDATE_LOG);
  if (IS_WIN) {
    // On Windows, this log file is written to the AppData directory, and will
    // therefore exist.
    logTestInfo("testing " + log.path + " should exist");
    do_check_true(log.exists());
  } else {
    logTestInfo("testing " + log.path + " shouldn't exist");
    do_check_false(log.exists());
  }

  log = getUpdatesDir();
  log.append(FILE_LAST_LOG);
  logTestInfo("testing " + log.path + " should exist");
  do_check_true(log.exists());

  log = getUpdatesDir();
  log.append(FILE_BACKUP_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  let updatesDir = getUpdatesDir();
  updatesDir.append("0");
  logTestInfo("testing " + updatesDir.path + " should exist");
  do_check_true(updatesDir.exists());

  waitForFilesInUse();
}
