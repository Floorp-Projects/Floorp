/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by staging an update and launching an application to
 * apply it.
 */

Components.utils.import("resource://gre/modules/ctypes.jsm");

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
  setupUpdaterTest(FILE_COMPLETE_MAR);

  createUpdaterINI(true);

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

function customLaunchAppToApplyUpdate() {
  logTestInfo("start - locking installation directory");
  const LPCWSTR = ctypes.char16_t.ptr;
  const DWORD = ctypes.uint32_t;
  const LPVOID = ctypes.voidptr_t;
  const GENERIC_READ = 0x80000000;
  const FILE_SHARE_READ = 1;
  const FILE_SHARE_WRITE = 2;
  const OPEN_EXISTING = 3;
  const FILE_FLAG_BACKUP_SEMANTICS = 0x02000000;
  const INVALID_HANDLE_VALUE = LPVOID(0xffffffff);
  let kernel32 = ctypes.open("kernel32");
  let CreateFile = kernel32.declare("CreateFileW", ctypes.default_abi,
                                    LPVOID, LPCWSTR, DWORD, DWORD,
                                    LPVOID, DWORD, DWORD, LPVOID);
  gHandle = CreateFile(getAppBaseDir().path, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, LPVOID(0),
                       OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, LPVOID(0));
  do_check_neq(gHandle.toString(), INVALID_HANDLE_VALUE.toString());
  kernel32.close();
  logTestInfo("finish - locking installation directory");
}

/**
 * Checks if the update has finished being staged.
 */
function checkUpdateApplied() {
  gTimeoutRuns++;
  // Don't proceed until the active update's state is the expected value.
  if (gUpdateManager.activeUpdate.state != STATE_APPLIED_SVC) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for update to equal: " +
               STATE_APPLIED_SVC +
               ", current state: " + gUpdateManager.activeUpdate.state);
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    }
    return;
  }

  // Don't proceed until the update's status state is the expected value.
  let state = readStatusState();
  if (state != STATE_APPLIED_SVC) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the update " +
               "status state to equal: " +
               STATE_APPLIED_SVC +
               ", current status state: " + state);
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    }
    return;
  }

  // Don't proceed until the last update log has been created.
  let log;
  if (IS_WIN || IS_MACOSX) {
    log = getUpdatesDir();
  } else {
    log = getStageDirFile(null, true);
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

  if (IS_WIN || IS_MACOSX) {
    let running = getPostUpdateFile(".running");
    logTestInfo("checking that the post update process running file doesn't " +
                "exist. Path: " + running.path);
    do_check_false(running.exists());
  }

  checkFilesAfterUpdateSuccess(getStageDirFile, true, false);

  log = getUpdatesPatchDir();
  log.append(FILE_UPDATE_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  log = getUpdatesDir();
  log.append(FILE_LAST_LOG);
  if (IS_WIN || IS_MACOSX) {
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

  let updatesDir = getStageDirFile(DIR_UPDATES + "/0", true);
  logTestInfo("testing " + updatesDir.path + " shouldn't exist");
  do_check_false(updatesDir.exists());

  log = getStageDirFile(DIR_UPDATES + "/0/" + FILE_UPDATE_LOG, true);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  log = getStageDirFile(DIR_UPDATES + "/" + FILE_LAST_LOG, true);
  if (IS_WIN || IS_MACOSX) {
    logTestInfo("testing " + log.path + " shouldn't exist");
    do_check_false(log.exists());
  } else {
    logTestInfo("testing " + log.path + " should exist");
    do_check_true(log.exists());
  }

  log = getStageDirFile(DIR_UPDATES + "/" + FILE_BACKUP_LOG, true);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  // Switch the application to the staged application that was updated by
  // launching the application.
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
  if (state != STATE_SUCCEEDED) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the update " +
               "status state to equal: " +
               STATE_SUCCEEDED +
               ", current status state: " + state);
    } else {
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
    }
    return;
  }

  // Don't proceed until the application was switched out with the staged update
  // successfully.
  let updatedDir = getStageDirFile(null, true);
  if (updatedDir.exists()) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded while waiting for updated dir to not exist. Path: " +
               updatedDir.path);
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

  if (IS_WIN || IS_MACOSX) {
    let running = getPostUpdateFile(".running");
    logTestInfo("checking that the post update process running file exists. " +
                "Path: " + running.path);
    do_check_true(running.exists());
  }

  checkFilesAfterUpdateSuccess(getApplyDirFile, false, false);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS);
  checkCallbackAppLog();

  standardInit();

  let update = gUpdateManager.getUpdateAt(0);
  do_check_eq(update.state, STATE_SUCCEEDED);

  let updatesDir = getUpdatesPatchDir();
  logTestInfo("testing " + updatesDir.path + " should exist");
  do_check_true(updatesDir.exists());

  let log = getUpdatesPatchDir();
  log.append(FILE_UPDATE_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  log = getUpdatesDir();
  log.append(FILE_LAST_LOG);
  logTestInfo("testing " + log.path + " should exist");
  do_check_true(log.exists());

  log = getUpdatesDir();
  log.append(FILE_BACKUP_LOG);
  logTestInfo("testing " + log.path + " should exist");
  do_check_true(log.exists());

  waitForFilesInUse();
}
