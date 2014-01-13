/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const PREF_APP_UPDATE_MIGRATE_APP_DIR = "app.update.migrated.updateDir";

function clearTaskbarIDHash(exePath, appInfoName) {
  let registry = AUS_Cc["@mozilla.org/windows-registry-key;1"].
                 createInstance(AUS_Ci.nsIWindowsRegKey);
  try {
    registry.open(AUS_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                  "Software\\Mozilla\\" + appInfoName + "\\TaskBarIDs",
                  AUS_Ci.nsIWindowsRegKey.ACCESS_ALL);
    registry.removeValue(exePath);
  } catch (e) {
  }
  finally {
    registry.close();
  }
}

function setTaskbarIDHash(exePath, hash, appInfoName) {
  let registry = AUS_Cc["@mozilla.org/windows-registry-key;1"].
                 createInstance(AUS_Ci.nsIWindowsRegKey);
  try {
    registry.create(AUS_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "Software\\Mozilla\\" + appInfoName + "\\TaskBarIDs",
                    AUS_Ci.nsIWindowsRegKey.ACCESS_WRITE);
    registry.writeStringValue(exePath, hash);
  } catch (e) {
  }
  finally {
    registry.close();
  }
};

function getMigrated() {
  var migrated = 0;
  try {
    migrated = Services.prefs.getBoolPref(PREF_APP_UPDATE_MIGRATE_APP_DIR);
  } catch (e) {
  }
  return migrated;
}

/* General Update Manager Tests */

function run_test() {
  setupTestCommon();

  standardInit();

  var appinfo = AUS_Cc["@mozilla.org/xre/app-info;1"].
                getService(AUS_Ci.nsIXULAppInfo).
                QueryInterface(AUS_Ci.nsIXULRuntime);

   // Obtain the old update root leaf
  var updateLeafName;
  var exeFile = FileUtils.getFile(XRE_EXECUTABLE_FILE, []);
  var programFiles = FileUtils.getFile("ProgF", []);
  if (exeFile.path.substring(0, programFiles.path.length).toLowerCase() ==
      programFiles.path.toLowerCase()) {
    updateLeafName = exeFile.parent.leafName;
  } else {
    updateLeafName = appinfo.name;
  }

  // Obtain the old update root
  var oldUpdateRoot;
  if (appinfo.vendor) {
    oldUpdateRoot = FileUtils.getDir("LocalAppData", [appinfo.vendor,
                                                      appinfo.name,
                                                      updateLeafName], false);
  } else {
    oldUpdateRoot = FileUtils.getDir("LocalAppData", [appinfo.name,
                                                      updateLeafName], false);
  }
  // Obtain the new update root
  var newUpdateRoot = FileUtils.getDir("UpdRootD", [], false);

  ///////////////////////////////////////////////////////////
  // Setting a taskbar ID without the old update dir existing should set the
  // pref so a migration isn't retried.

  // Remove the old and new update root directories
  try {
    oldUpdateRoot.remove(true);
  } catch (e) {
  }
  try {
    newUpdateRoot.remove(true);
  } catch (e) {
  }
  Services.prefs.setBoolPref(PREF_APP_UPDATE_MIGRATE_APP_DIR, false);
  setTaskbarIDHash(exeFile.parent.path, "AAAAAAAA", appinfo.name);
  initUpdateServiceStub();
  do_check_eq(getMigrated(), 1);

  ///////////////////////////////////////////////////////////
  // An application without a taskbar ID should bail early without a pref
  // being set, so that if a taskbar is set for the application, the migration
  // will be retried.

  Services.prefs.setBoolPref(PREF_APP_UPDATE_MIGRATE_APP_DIR, false);
  clearTaskbarIDHash(exeFile.parent.path, appinfo.name);
  initUpdateServiceStub();
  do_check_eq(getMigrated(), 0);

  ///////////////////////////////////////////////////////////
  // Migrating files should work

  Services.prefs.setBoolPref(PREF_APP_UPDATE_MIGRATE_APP_DIR, false);
  setTaskbarIDHash(exeFile.parent.path, "AAAAAAAA", appinfo.name);
  var oldUpdateDirs = oldUpdateRoot.clone();
  oldUpdateDirs.append("updates");
  oldUpdateDirs.append("0");
  oldUpdateDirs.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  // Get an array of all of the files we want to migrate.
  // We do this to create them in the old update directory.
  var filesToMigrate = [FILE_UPDATES_DB, FILE_UPDATE_ACTIVE,
                        ["updates", FILE_LAST_LOG], ["updates", FILE_BACKUP_LOG],
                        ["updates", "0", FILE_UPDATE_STATUS]];
  // Move each of those files to the new directory
  filesToMigrate.forEach(relPath => {
    let oldFile = oldUpdateRoot.clone();
    if (relPath instanceof Array) {
      relPath.forEach(relPathPart => {
        oldFile.append(relPathPart);
      });
    } else {
      oldFile.append(relPath);
    }
    oldFile.create(AUS_Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  });
  // Do the migration
  initUpdateServiceStub();
  doTestFinish();
  return;
  // Now verify that each of the files exist in the new update directory
  filesToMigrate.forEach(relPath => {
    let newFile = newUpdateRoot.clone();
    let oldFile = oldUpdateRoot.clone();
    if (relPath instanceof Array) {
      relPath.forEach(relPathPart => {
        newFile.append(relPathPart);
        oldFile.append(relPathPart);
      });
    } else {
      newFile.append(relPath);
      oldFile.append(relPath);
    }
    // The file should be mimgrated, except for FILE_UPDATE_STATUS
    // which gets consumed by post update after it is migrated..
    if (newFile.leafName != FILE_UPDATE_STATUS) {
      do_check_true(newFile.exists());
    }
    do_check_false(oldFile.exists());
  });

  doTestFinish();
}

function end_test() {
  var appinfo = AUS_Cc["@mozilla.org/xre/app-info;1"].
                getService(AUS_Ci.nsIXULAppInfo).
                QueryInterface(AUS_Ci.nsIXULRuntime);
  var exeFile = FileUtils.getFile(XRE_EXECUTABLE_FILE, []);
  clearTaskbarIDHash(exeFile.parent.path, appinfo.name);
}
