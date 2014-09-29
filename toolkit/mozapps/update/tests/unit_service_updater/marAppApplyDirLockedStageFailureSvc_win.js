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

  if (!shouldRunServiceTest()) {
    return;
  }

  setupTestCommon();
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_COMPLETE_MAR);

  createUpdaterINI(false);

  if (IS_WIN) {
    Services.prefs.setBoolPref(PREF_APP_UPDATE_SERVICE_ENABLED, true);
  }

  let channel = Services.prefs.getCharPref(PREF_APP_UPDATE_CHANNEL);
  let patches = getLocalPatchString(null, null, null, null, null, "true",
                                    STATE_PENDING_SVC);
  let updates = getLocalUpdateString(patches, null, null, null, null, null,
                                     null, null, null, null, null, null,
                                     null, "true", channel);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeVersionFile(getAppVersion());
  writeStatusFile(STATE_PENDING_SVC);

  reloadUpdateManagerData();
  do_check_true(!!gUpdateManager.activeUpdate);

  lockDirectory(getAppBaseDir());

  setupAppFilesAsync();
}

function setupAppFilesFinished() {
  // For Mac OS X set the last modified time for the root directory to a date in
  // the past to test that the last modified time is updated on a successful
  // update (bug 600098).
  if (IS_MACOSX) {
    let now = Date.now();
    let yesterday = now - (1000 * 60 * 60 * 24);
    let applyToDir = getApplyDirFile();
    applyToDir.lastModifiedTime = yesterday;
  }

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
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for update to equal: " +
               STATE_PENDING +
               ", current state: " + gUpdateManager.activeUpdate.state);
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    }
    return;
  }

  do_timeout(TEST_CHECK_TIMEOUT, finishTest);
}

function finishTest() {
  if (IS_WIN || IS_MACOSX) {
    let running = getPostUpdateFile(".running");
    logTestInfo("checking that the post update process running file doesn't " +
                "exist. Path: " + running.path);
    do_check_false(running.exists());
  }

  do_check_eq(readStatusState(), STATE_PENDING);
  checkFilesAfterUpdateFailure(getApplyDirFile, false, false);
  unlockDirectory(getAppBaseDir());
  waitForFilesInUse();
}
