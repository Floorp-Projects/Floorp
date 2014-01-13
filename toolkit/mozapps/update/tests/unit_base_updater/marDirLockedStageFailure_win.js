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

  reloadUpdateManagerData();
  do_check_true(!!gUpdateManager.activeUpdate);

  lockDirectory(getAppBaseDir());

  let updateSettingsIni = getApplyDirFile(FILE_UPDATE_SETTINGS_INI, true);
  writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  stageUpdate();
}

function end_test() {
  resetEnvironment();
}

/**
 * Checks if staging the update has failed.
 */
function checkUpdateApplied() {
  // Don't proceed until the update has failed, and reset to pending.
  if (gUpdateManager.activeUpdate.state != STATE_PENDING) {
    if (++gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for update to be " +
               STATE_PENDING + ", current state is: " +
               gUpdateManager.activeUpdate.state);
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    }
    return;
  }

  do_timeout(TEST_CHECK_TIMEOUT, finishTest);
}

function finishTest() {
  do_check_eq(readStatusState(), STATE_PENDING);
  unlockDirectory(getAppBaseDir());
  waitForFilesInUse();
}
