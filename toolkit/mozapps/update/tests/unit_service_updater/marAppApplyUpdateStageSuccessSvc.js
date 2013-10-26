/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by applying an update in the background and
 * launching an application
 */

/**
 * The MAR file used for this test should not contain a version 2 update
 * manifest file (e.g. updatev2.manifest).
 */

let gAppTimer;
let gProcess;

function run_test() {
  if (!shouldRunServiceTest()) {
    return;
  }

  setupTestCommon(false);
  do_register_cleanup(end_test);

  if (IS_WIN) {
    Services.prefs.setBoolPref(PREF_APP_UPDATE_SERVICE_ENABLED, true);
  }

  removeUpdateDirsAndFiles();

  symlinkUpdateFilesIntoBundleDirectory();
  if (IS_WIN) {
    adjustPathsOnWindows();
  }

  if (!gAppBinPath) {
    do_throw("Main application binary not found... expected: " +
             APP_BIN_NAME + APP_BIN_SUFFIX);
    return;
  }

  // Don't attempt to show a prompt when the update is finished.
  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, true);

  gEnvSKipUpdateDirHashing = true;
  let channel = Services.prefs.getCharPref(PREF_APP_UPDATE_CHANNEL);
  let patches = getLocalPatchString(null, null, null, null, null, "true",
                                    STATE_PENDING_SVC);
  let updates = getLocalUpdateString(patches, null, null, null, null, null,
                                     null, null, null, null, null, null,
                                     null, "true", channel);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);

  // Read the application.ini and use its application version
  let processDir = getAppDir();
  let file = processDir.clone();
  file.append("application.ini");
  let ini = AUS_Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
            getService(AUS_Ci.nsIINIParserFactory).
            createINIParser(file);
  let version = ini.getString("App", "Version");
  writeVersionFile(version);
  writeStatusFile(STATE_PENDING_SVC);

  // This is the directory where the update files will be located
  let updateTestDir = getUpdateTestDir();
  try {
    removeDirRecursive(updateTestDir);
  }
  catch (e) {
    logTestInfo("unable to remove directory - path: " + updateTestDir.path +
                ", exception: " + e);
  }

  let updatesPatchDir = getUpdatesDir();
  updatesPatchDir.append("0");
  let mar = getTestDirFile(FILE_SIMPLE_MAR);
  mar.copyTo(updatesPatchDir, FILE_UPDATE_ARCHIVE);

  // Backup the updater.ini file if it exists by moving it. This prevents the
  // post update executable from being launched if it is specified.
//XXX disabled until bug 820933 and bug 820934 are fixed
if (0) {
  let updaterIni = processDir.clone();
  updaterIni.append(FILE_UPDATER_INI);
  if (updaterIni.exists()) {
    updaterIni.moveTo(processDir, FILE_UPDATER_INI_BAK);
  }
}

  // Backup the updater-settings.ini file if it exists by moving it.
  let updateSettingsIni = processDir.clone();
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
  if (updateSettingsIni.exists()) {
    updateSettingsIni.moveTo(processDir, FILE_UPDATE_SETTINGS_INI_BAK);
  }
  updateSettingsIni = processDir.clone();
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
  writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);

  reloadUpdateManagerData();
  do_check_true(!!gUpdateManager.activeUpdate);

  Services.obs.addObserver(gUpdateStagedObserver, "update-staged", false);

  setEnvironment();

  // Initiate a background update.
  AUS_Cc["@mozilla.org/updates/update-processor;1"].
    createInstance(AUS_Ci.nsIUpdateProcessor).
    processUpdate(gUpdateManager.activeUpdate);

  resetEnvironment();
}

function switchApp() {
  let launchBin = getLaunchBin();
  let args = getProcessArgs();
  logTestInfo("launching " + launchBin.path + " " + args.join(" "));

  gProcess = AUS_Cc["@mozilla.org/process/util;1"].
                createInstance(AUS_Ci.nsIProcess);
  gProcess.init(launchBin);

  gAppTimer = AUS_Cc["@mozilla.org/timer;1"].createInstance(AUS_Ci.nsITimer);
  gAppTimer.initWithCallback(gTimerCallback, APP_TIMER_TIMEOUT,
                             AUS_Ci.nsITimer.TYPE_ONE_SHOT);

  setEnvironment();

  gProcess.runAsync(args, args.length, gProcessObserver);

  resetEnvironment();
}

function end_test() {
  if (gProcess.isRunning) {
    logTestInfo("attempt to kill process");
    gProcess.kill();
  }

  if (gAppTimer) {
    logTestInfo("cancelling timer");
    gAppTimer.cancel();
    gAppTimer = null;
  }

  resetEnvironment();

  let processDir = getAppDir();
  // Restore the backup of the updater.ini if it exists.
  let updaterIni = processDir.clone();
  updaterIni.append(FILE_UPDATER_INI_BAK);
  if (updaterIni.exists()) {
    updaterIni.moveTo(processDir, FILE_UPDATER_INI);
  }

  // Restore the backed up updater-settings.ini if it exists.
  let updateSettingsIni = processDir.clone();
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI_BAK);
  if (updateSettingsIni.exists()) {
    updateSettingsIni.moveTo(processDir, FILE_UPDATE_SETTINGS_INI);
  }

  // Remove the files added by the update.
  let updateTestDir = getUpdateTestDir();
  try {
    logTestInfo("removing update test directory " + updateTestDir.path);
    removeDirRecursive(updateTestDir);
  }
  catch (e) {
    logTestInfo("unable to remove directory - path: " + updateTestDir.path +
                ", exception: " + e);
  }

  if (IS_UNIX) {
    // This will delete the launch script if it exists.
    getLaunchScript();
  }

  cleanupTestCommon();
}

function shouldAdjustPathsOnMac() {
  // When running xpcshell tests locally, xpcshell and firefox-bin do not live
  // in the same directory.
  let dir = getCurrentProcessDir();
  return (IS_MACOSX && dir.leafName != "MacOS");
}

/**
 * Gets the directory where the update adds / removes the files contained in the
 * update.
 *
 * @return  nsIFile for the directory where the update adds / removes the files
 *          contained in the update mar.
 */
function getUpdateTestDir() {
  let updateTestDir = getAppDir();
  if (IS_MACOSX) {
    updateTestDir = updateTestDir.parent.parent;
  }
  updateTestDir.append("update_test");
  return updateTestDir;
}

/**
 * Checks if the update has finished being applied in the background.
 */
function checkUpdateApplied() {
  gTimeoutRuns++;
  // Don't proceed until the update has been applied.
  if (gUpdateManager.activeUpdate.state != STATE_APPLIED_PLATFORM) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS)
      do_throw("Exceeded MAX_TIMEOUT_RUNS whilst waiting for update to be " +
               "applied, current state is: " +
               gUpdateManager.activeUpdate.state);
    else
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    return;
  }

  let updatedDir = getAppDir();
  if (IS_MACOSX) {
    updatedDir = updatedDir.parent.parent;
  }
  updatedDir.append(UPDATED_DIR_SUFFIX.replace("/", ""));
  logTestInfo("testing " + updatedDir.path + " should exist");
  do_check_true(updatedDir.exists());

  let log = getUpdatesDir();
  log.append(FILE_LAST_LOG);
  if (!log.exists()) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS)
      do_throw("Exceeded MAX_TIMEOUT_RUNS whilst waiting for update log to " +
               "be created");
    else
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    return;
  }

  // Don't proceed until the update status is no longer pending or applying.
  let status = readStatusFile();
  do_check_eq(status, STATE_APPLIED_PLATFORM);

  // On Windows, make sure not to use the maintenance service for switching
  // the app.
  if (IS_WIN) {
    writeStatusFile(STATE_APPLIED);
    status = readStatusFile();
    do_check_eq(status, STATE_APPLIED);
  }

  // Log the contents of the update.log so it is simpler to diagnose a test
  // failure.
  let contents = readFile(log);
  logTestInfo("contents of " + log.path + ":\n" +
              contents.replace(/\r\n/g, "\n"));

  let updateTestDir = getUpdateTestDir();
  logTestInfo("testing " + updateTestDir.path + " shouldn't exist");
  do_check_false(updateTestDir.exists());

  updateTestDir = updatedDir.clone();
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

  let updatesDir = getUpdatesDir();
  log = updatesDir.clone();
  log.append("0");
  log.append(FILE_UPDATE_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  log = updatesDir.clone();
  log.append(FILE_LAST_LOG);
  logTestInfo("testing " + log.path + " should exist");
  do_check_true(log.exists());

  log = updatesDir.clone();
  log.append(FILE_BACKUP_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  updatesDir = updatedDir.clone();
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

  updatesDir.append("0");
  logTestInfo("testing " + updatesDir.path + " shouldn't exist");
  do_check_false(updatesDir.exists());

  // Now, switch the updated version of the app
  do_timeout(TEST_CHECK_TIMEOUT, switchApp);
}

/**
 * Checks if the update has finished and if it has finished performs checks for
 * the test.
 */
function checkUpdateFinished() {
  gTimeoutRuns++;
  // Don't proceed until the update status is no longer applied.
  try {
    let status = readStatusFile();
    if (status != STATE_SUCCEEDED) {
      if (gTimeoutRuns > MAX_TIMEOUT_RUNS)
        do_throw("Exceeded MAX_TIMEOUT_RUNS whilst waiting for state to " +
                 "change to succeeded, current status: " + status);
      else
        do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
      return;
    }
  } catch (e) {
    // Ignore exceptions if the status file is not found
  }

  try {
    // This will delete the app console log file if it exists.
    getAppConsoleLogPath();
  } catch (e) {
    if (e.result == Components.results.NS_ERROR_FILE_IS_LOCKED) {
      // This might happen on Windows in case the callback application has not
      // finished its job yet.  So, we'll wait some more.
      if (gTimeoutRuns > MAX_TIMEOUT_RUNS)
        do_throw("Exceeded whilst waiting for file to be unlocked");
      else
        do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
      return;
    } else {
      do_throw("getAppConsoleLogPath threw: " + e);
    }
  }

  // At this point we need to see if the application was switched successfully.

  let updatedDir = getAppDir();
  if (IS_MACOSX) {
    updatedDir = updatedDir.parent.parent;
  }
  updatedDir.append(UPDATED_DIR_SUFFIX.replace("/", ""));
  logTestInfo("testing " + updatedDir.path + " shouldn't exist");
  do_check_false(updatedDir.exists());

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

  let updatesDir = getUpdatesDir();
  log = updatesDir.clone();
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

  log = updatesDir.clone();
  log.append(FILE_LAST_LOG);
  logTestInfo("testing " + log.path + " should exist");
  do_check_true(log.exists());

  log = updatesDir.clone();
  log.append(FILE_BACKUP_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  updatesDir.append("0");
  logTestInfo("testing " + updatesDir.path + " should exist");
  do_check_true(updatesDir.exists());

  waitForFilesInUse();
}
