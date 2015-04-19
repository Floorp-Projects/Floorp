/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by staging an update and launching an application to
 * apply it.
 */

const START_STATE = STATE_PENDING_SVC;
const END_STATE = STATE_PENDING;

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
                                    START_STATE);
  let updates = getLocalUpdateString(patches, null, null, null, null, null,
                                     null, null, null, null, null, null,
                                     null, "true", channel);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeVersionFile(getAppVersion());
  writeStatusFile(START_STATE);

  reloadUpdateManagerData();
  Assert.ok(!!gUpdateManager.activeUpdate,
            "the active update should be defined");

  lockDirectory(getAppBaseDir());

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
  if (gUpdateManager.activeUpdate.state != END_STATE) {
    if (++gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the " +
               "active update status state to equal: " +
               END_STATE +
               ", current state: " + gUpdateManager.activeUpdate.state);
    }
    do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    return;
  }

  do_timeout(TEST_CHECK_TIMEOUT, finishTest);
}

function finishTest() {
  checkPostUpdateRunningFile(false);
  Assert.equal(readStatusState(), END_STATE,
               "the status state" + MSG_SHOULD_EQUAL);
  checkFilesAfterUpdateFailure(getApplyDirFile, false, false);
  unlockDirectory(getAppBaseDir());
  standardInit();
  waitForFilesInUse();
}
