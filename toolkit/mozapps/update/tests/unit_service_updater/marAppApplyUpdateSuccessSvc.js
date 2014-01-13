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

  if (!shouldRunServiceTest()) {
    return;
  }

  setupTestCommon();

  let channel = Services.prefs.getCharPref(PREF_APP_UPDATE_CHANNEL);
  let patches = getLocalPatchString(null, null, null, null, null, "true",
                                    STATE_PENDING);
  let updates = getLocalUpdateString(patches, null, null, null, null, null,
                                     null, null, null, null, null, null,
                                     null, "true", channel);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeVersionFile(getAppVersion());
  writeStatusFile(STATE_PENDING_SVC);

  // This is the directory where the files that will be updated are located.
  let updateTestDir = getUpdateTestDir();
  // Add the directory where the files will be added and add files that will be
  // removed.
  if (!updateTestDir.exists()) {
    updateTestDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
  }
  logTestInfo("update test directory path: " + updateTestDir.path);

  let file = updateTestDir.clone();
  file.append("UpdateTestRemoveFile");
  writeFile(file, "ToBeRemoved");

  file = updateTestDir.clone();
  file.append("UpdateTestAddFile");
  writeFile(file, "ToBeReplaced");

  file = updateTestDir.clone();
  file.append("removed-files");
  writeFile(file, "ToBeReplaced");

  let updatesPatchDir = getUpdatesPatchDir();
  let mar = getTestDirFile(FILE_SIMPLE_MAR);
  mar.copyTo(updatesPatchDir, FILE_UPDATE_ARCHIVE);

  let updateSettingsIni = getApplyDirFile(FILE_UPDATE_SETTINGS_INI, true);
  writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  runUpdateUsingService(STATE_PENDING_SVC, STATE_SUCCEEDED);
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

  // Log the contents of the update.log so it is simpler to diagnose a test
  // failure. For example, on Windows if the application binary is in use the
  // updater will not apply the update.
  let contents = readFile(log);
  logTestInfo("contents of " + log.path + ":\n" +  
              contents.replace(/\r\n/g, "\n"));

  standardInit();

  let update = gUpdateManager.getUpdateAt(0);
  do_check_eq(update.state, STATE_SUCCEEDED);

  let updateTestDir = getUpdateTestDir();

  let file = updateTestDir.clone();
  file.append("UpdateTestRemoveFile");
  do_check_false(file.exists());

  file = updateTestDir.clone();
  file.append("UpdateTestAddFile");
  do_check_true(file.exists());
  do_check_eq(readFileBytes(file), "UpdateTestAddFile\n");

  file = updateTestDir.clone();
  file.append("removed-files");
  do_check_true(file.exists());
  do_check_eq(readFileBytes(file), "update_test/UpdateTestRemoveFile\n");

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
