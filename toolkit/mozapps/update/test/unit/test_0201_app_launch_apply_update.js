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

const TEST_ID = "0201";

// Backup the updater.ini and use a custom one to prevent the updater from
// launching a post update executable.
const FILE_UPDATER_INI_BAK = "updater.ini.bak";

// Number of milliseconds for each do_timeout call.
const CHECK_TIMEOUT_MILLI = 1000;

// How many of CHECK_TIMEOUT_MILLI to wait before we abort the test.
const MAX_TIMEOUT_RUNS = 300;

// Maximum number of milliseconds the process that is launched can run before
// the test will try to kill it.
const APP_TIMER_TIMEOUT = 120000;

let gAppTimer;
let gProcess;
let gActiveUpdate;
let gTimeoutRuns = 0;

// Override getUpdatesRootDir on Mac because we need to apply the update
// inside the bundle directory.
function symlinkUpdateFilesIntoBundleDirectory() {
  if (!shouldAdjustPathsOnMac()) {
    return;
  }
  // Symlink active-update.xml and updates/ inside the dist/bin directory
  // to point to the bundle directory.
  // This is necessary because in order to test the code which actually ships
  // with Firefox, we need to perform the update inside the bundle directory,
  // whereas xpcshell runs from dist/bin/, and the updater service code looks
  // at the current process directory to find things like these two files.

  Components.utils.import("resource://gre/modules/ctypes.jsm");
  let libc = ctypes.open("/usr/lib/libc.dylib");
  // We need these two low level APIs because their functionality is not
  // provided in nsIFile APIs.
  let symlink = libc.declare("symlink", ctypes.default_abi, ctypes.int,
                             ctypes.char.ptr, ctypes.char.ptr);
  let unlink = libc.declare("unlink", ctypes.default_abi, ctypes.int,
                            ctypes.char.ptr);

  // Symlink active-update.xml
  let dest = getAppDir();
  dest.append("active-update.xml");
  if (!dest.exists()) {
    dest.create(dest.NORMAL_FILE_TYPE, 0644);
  }
  do_check_true(dest.exists());
  let source = getUpdatesRootDir();
  source.append("active-update.xml");
  unlink(source.path);
  let ret = symlink(dest.path, source.path);
  do_check_eq(ret, 0);
  do_check_true(source.exists());

  // Symlink updates/
  let dest2 = getAppDir();
  dest2.append("updates");
  if (dest2.exists()) {
    dest2.remove(true);
  }
  dest2.create(dest.DIRECTORY_TYPE, 0755);
  do_check_true(dest2.exists());
  let source2 = getUpdatesRootDir();
  source2.append("updates");
  if (source2.exists()) {
    source2.remove(true);
  }
  ret = symlink(dest2.path, source2.path);
  do_check_eq(ret, 0);
  do_check_true(source2.exists());

  // Cleanup the symlinks when the test is finished.
  do_register_cleanup(function() {
    let ret = unlink(source.path);
    do_check_false(source.exists());
    let ret = unlink(source2.path);
    do_check_false(source2.exists());
  });

  // Now, make sure that getUpdatesRootDir returns the application bundle
  // directory, to make the various stuff in the test framework to work
  // correctly.
  getUpdatesRootDir = getAppDir;
}

function run_test() {
  if (APP_BIN_NAME == "xulrunner") {
    logTestInfo("Unable to run this test on xulrunner");
    return;
  }

  do_test_pending();
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
                                    STATE_PENDING);
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
  writeStatusFile(STATE_PENDING);

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
  let mar = do_get_file("data/simple.mar");
  mar.copyTo(updatesPatchDir, FILE_UPDATE_ARCHIVE);

  reloadUpdateManagerData();
  gActiveUpdate = gUpdateManager.activeUpdate;
  do_check_true(!!gActiveUpdate);

  // Backup the updater.ini file if it exists by moving it. This prevents the
  // post update executable from being launched if it is specified.
  let updaterIni = processDir.clone();
  updaterIni.append(FILE_UPDATER_INI);
  if (updaterIni.exists()) {
    updaterIni.moveTo(processDir, FILE_UPDATER_INI_BAK);
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

  // Initiate a background update.
  AUS_Cc["@mozilla.org/updates/update-processor;1"].
    createInstance(AUS_Ci.nsIUpdateProcessor).
    processUpdate(gActiveUpdate);

  checkUpdateApplied();
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
    if (IS_MACOSX) {
      // This will delete the version script and version file if they exist.
      getVersionScriptAndFile();
    }
  }

  cleanUp();
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
      do_timeout(CHECK_TIMEOUT_MILLI, checkUpdateApplied);
    return;
  }

  let updatedDir = getAppDir();
  if (IS_MACOSX) {
    updatedDir = updatedDir.parent.parent;
  }
  updatedDir.append(UPDATED_DIR_SUFFIX.replace("/", ""));
  logTestInfo("testing " + updatedDir.path + " should exist");
  do_check_true(updatedDir.exists());

  let log;
  if (IS_WIN) {
    log = getUpdatesDir();
  } else {
    log = updatedDir.clone();
    if (IS_MACOSX) {
      log.append("Contents");
      log.append("MacOS");
    }
    log.append("updates");
  }
  log.append(FILE_LAST_LOG);
  if (!log.exists()) {
    do_timeout(CHECK_TIMEOUT_MILLI, checkUpdateApplied);
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
  if (IS_WIN) {
    // On Windows this file lives outside of the app directory, so it should
    // exist.
    logTestInfo("testing " + log.path + " should exist");
    do_check_true(log.exists());
  } else {
    logTestInfo("testing " + log.path + " shouldn't exist");
    do_check_false(log.exists());
  }

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

  // Now, switch the updated version of the app
  do_timeout(CHECK_TIMEOUT_MILLI, switchApp);
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
        do_timeout(CHECK_TIMEOUT_MILLI, checkUpdateFinished);
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
        do_timeout(CHECK_TIMEOUT_MILLI, checkUpdateFinished);
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
  if (updatedDir.exists()) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS)
      do_throw("Exceeded whilst waiting for update dir to not exist");
    else
      do_timeout(CHECK_TIMEOUT_MILLI, checkUpdateFinished);
    return;
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
