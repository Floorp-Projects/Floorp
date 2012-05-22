/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Test applying an update by staging an update and launching an application */

/**
 * The MAR file used for this test should not contain a version 2 update
 * manifest file (e.g. updatev2.manifest).
 */

const TEST_ID = "0200_svc";

// Backup the updater.ini and use a custom one to prevent the updater from
// launching a post update executable.
const FILE_UPDATER_INI_BAK = "updater.ini.bak";

// Number of milliseconds for each do_timeout call.
const CHECK_TIMEOUT_MILLI = 1000;

// Maximum number of milliseconds the process that is launched can run before
// the test will try to kill it.
const APP_TIMER_TIMEOUT = 15000;

function run_test() {
  if (!shouldRunServiceTest()) {
    return;
  }

  do_test_pending();
  do_register_cleanup(end_test);

  removeUpdateDirsAndFiles();

  if (!gAppBinPath) {
    do_throw("Main application binary not found... expected: " +
             APP_BIN_NAME + APP_BIN_SUFFIX);
    return;
  }

  let channel = Services.prefs.getCharPref(PREF_APP_UPDATE_CHANNEL);
  let patches = getLocalPatchString(null, null, null, null, null, "true",
                                    STATE_PENDING);
  let updates = getLocalUpdateString(patches, null, null, null, null, null,
                                     null, null, null, null, null, null,
                                     null, "true", channel);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);

  // Read the application.ini and use its application version
  let processDir = getCurrentProcessDir();
  let file = processDir.clone();
  file.append("application.ini");
  let ini = AUS_Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
            getService(AUS_Ci.nsIINIParserFactory).
            createINIParser(file);
  let version = ini.getString("App", "Version");
  writeVersionFile(version);

  // This is the directory where the update files will be located
  let updateTestDir = getUpdateTestDir();
  try {
    removeDirRecursive(updateTestDir);
  }
  catch (e) {
    logTestInfo("unable to remove directory - path: " + updateTestDir.path +
                ", exception: " + e);
  }

  // Add the directory where the update files will be added and add files that
  // will be removed.
  if (!updateTestDir.exists()) {
    updateTestDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
  }
  logTestInfo("update test directory path: " + updateTestDir.path);

  file = updateTestDir.clone();
  file.append("UpdateTestRemoveFile");
  writeFile(file, "ToBeRemoved");

  file = updateTestDir.clone();
  file.append("UpdateTestAddFile");
  writeFile(file, "ToBeReplaced");

  file = updateTestDir.clone();
  file.append("removed-files");
  writeFile(file, "ToBeReplaced");

  let updatesRootDir = processDir.clone();
  updatesRootDir.append("updates");
  updatesRootDir.append("0");
  let mar = do_get_file("data/simple.mar");
  mar.copyTo(updatesRootDir, FILE_UPDATE_ARCHIVE);

  // Backup the updater.ini
  let updaterIni = processDir.clone();
  updaterIni.append(FILE_UPDATER_INI);
  updaterIni.moveTo(processDir, FILE_UPDATER_INI_BAK);
  // Create a new updater.ini to avoid applications that provide a post update
  // executable.
  let updaterIniContents = "[Strings]\n" +
                           "Title=Update Test\n" +
                           "Info=Application Update XPCShell Test - " +
                           "test_0200_general.js\n";
  updaterIni = processDir.clone();
  updaterIni.append(FILE_UPDATER_INI);
  writeFile(updaterIni, updaterIniContents);

  getUpdatesDir = function() {
    var updatesDir = processDir.clone();
    updatesDir.append("updates");
    return updatesDir;
  }
  getApplyDirPath = function() {
    return processDir.path;
  }
  getApplyDirFile = function (aRelPath, allowNonexistent) {
    let base = AUS_Cc["@mozilla.org/file/local;1"].
               createInstance(AUS_Ci.nsILocalFile);
    base.initWithPath(getApplyDirPath());
    let path = (aRelPath ? aRelPath : "");
    let bits = path.split("/");
    for (let i = 0; i < bits.length; i++) {
      if (bits[i]) {
        if (bits[i] == "..")
          base = base.parent;
        else
          base.append(bits[i]);
      }
    }

    if (!allowNonexistent && !base.exists()) {
      _passed = false;
      var stack = Components.stack.caller;
      _dump("TEST-UNEXPECTED-FAIL | " + stack.filename + " | [" +
            stack.name + " : " + stack.lineNumber + "] " + base.path +
            " does not exist\n");
    }

    return base;
  }
  runUpdateUsingService(STATE_PENDING_SVC, STATE_SUCCEEDED, checkUpdateFinished, updatesRootDir);
}

function end_test() {
  resetEnvironment();

  let processDir = getCurrentProcessDir();
  // Restore the backed up updater.ini
  let updaterIni = processDir.clone();
  updaterIni.append(FILE_UPDATER_INI_BAK);
  updaterIni.moveTo(processDir, FILE_UPDATER_INI);

  // Remove the copy of the application executable used for the test on
  // Windows if it exists.
  let appBinCopy = processDir.clone();
  appBinCopy.append(FILE_WIN_TEST_EXE);
  if (appBinCopy.exists()) {
    appBinCopy.remove(false);
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

  // This will delete the app console log file if it exists.
  getAppConsoleLogPath();

  cleanUp();
}

/**
 * Gets the directory where the update adds / removes the files contained in the
 * update.
 *
 * @return  nsIFile for the directory where the update adds / removes the files
 *          contained in the update mar.
 */
function getUpdateTestDir() {
  let updateTestDir = getCurrentProcessDir();
  updateTestDir.append("update_test");
  return updateTestDir;
}

/**
 * Checks if the update has finished and if it has finished performs checks for
 * the test.
 */
function checkUpdateFinished() {
  // Don't proceed until the update.log has been created.
  let log = getUpdatesDir();
  log.append("0");
  log.append(FILE_UPDATE_LOG);
  if (!log.exists()) {
    do_timeout(CHECK_TIMEOUT_MILLI, checkUpdateFinished);
    return;
  }

  // Log the contents of the update.log so it is simpler to diagnose a test
  // failure. For example, on Windows if the application binary is in use the
  // updater will not apply the update.
  let contents = readFile(log);
  logTestInfo("contents of " + log.path + ":\n" +  
              contents.replace(/\r\n/g, "\n"));

  if (contents.indexOf("NS_main: file in use") != -1) {
    do_throw("the application can't be in use when running this test");
  }

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

  updatesDir.append("0");
  logTestInfo("testing " + updatesDir.path + " should exist");
  do_check_true(updatesDir.exists());

  Services.dirsvc.unregisterProvider(gDirProvider);
  removeCallbackCopy();
}

// On Vista XRE_UPDATE_ROOT_DIR can be a directory other than the one in the
// application directory. This will reroute it back to the one in the
// application directory.
var gDirProvider = {
  getFile: function DP_getFile(prop, persistent) {
    persistent.value = true;
    if (prop == XRE_UPDATE_ROOT_DIR)
      return getCurrentProcessDir();
    return null;
  },
  QueryInterface: function(iid) {
    if (iid.equals(AUS_Ci.nsIDirectoryServiceProvider) ||
        iid.equals(AUS_Ci.nsISupports))
      return this;
    throw AUS_Cr.NS_ERROR_NO_INTERFACE;
  }
};
Services.dirsvc.QueryInterface(AUS_Ci.nsIDirectoryService).registerProvider(gDirProvider);
