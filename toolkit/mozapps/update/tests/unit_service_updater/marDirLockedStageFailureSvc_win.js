/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test applying an update by applying an update in the background and
 * launching an application
 */

/**
 * This test is identical to test_0201_app_launch_apply_update_svc.js, except
 * that it locks the application directory when the test is launched to
 * check if the alternate updated directory logic works correctly.
 */

/**
 * The MAR file used for this test should not contain a version 2 update
 * manifest file (e.g. updatev2.manifest).
 */

function run_test() {
  if (!shouldRunServiceTest()) {
    return;
  }

  setupTestCommon(false);
  do_register_cleanup(end_test);

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

  let channel = Services.prefs.getCharPref(PREF_APP_UPDATE_CHANNEL);
  let patches = getLocalPatchString(null, null, null, null, null, "true",
                                    STATE_PENDING_SVC);
  let updates = getLocalUpdateString(patches, null, null, null, null, null,
                                     null, null, null, null, null, null,
                                     null, "true", channel);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);

  // Read the application.ini and use its application version
  let processDir = getAppDir();
  lockDirectory(processDir);
  let file = processDir.clone();
  file.append("application.ini");
  let ini = AUS_Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
            getService(AUS_Ci.nsIINIParserFactory).
            createINIParser(file);
  let version = ini.getString("App", "Version");
  writeVersionFile(version);
  writeStatusFile(STATE_PENDING_SVC);

  // Remove the old updated directory which might be left over from previous tests.
  let oldUpdatedDir = processDir.clone();
  oldUpdatedDir.append(UPDATED_DIR_SUFFIX.replace("/", ""));
  if (oldUpdatedDir.exists()) {
    oldUpdatedDir.remove(true);
  }

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

function end_test() {
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
 * Checks if the update has failed being applied in the background.
 */
function checkUpdateApplied() {
  // Don't proceed until the update has failed, and reset to pending.
  if (gUpdateManager.activeUpdate.state != STATE_PENDING) {
    if (++gTimeoutRuns > MAX_TIMEOUT_RUNS)
      do_throw("Exceeded MAX_TIMEOUT_RUNS whist waiting for pending state to finish");
    else
      do_timeout(TEST_CHECK_TIMEOUT, checkUpdateApplied);
    return;
  }

  do_timeout(TEST_CHECK_TIMEOUT, finishTest);
}

function finishTest() {
  // Don't proceed until the update status is pending.
  let status = readStatusFile();
  do_check_eq(status, STATE_PENDING);

  unlockDirectory(getAppDir());

  removeCallbackCopy();
}
