/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const DIR_UPDATES         = "updates";
const FILE_UPDATES_DB     = "updates.xml";
const FILE_UPDATE_ACTIVE  = "active-update.xml";
const FILE_LAST_LOG       = "last-update.log";
const FILE_BACKUP_LOG     = "backup-update.log";
const FILE_UPDATE_STATUS  = "update.status";

const KEY_UPDROOT         = "UpdRootD";

#ifdef XP_WIN

const PREF_APP_UPDATE_MIGRATE_APP_DIR = "app.update.migrated.updateDir";


function getTaskbarIDHash(rootKey, exePath, appInfoName) {
  let registry = Cc["@mozilla.org/windows-registry-key;1"].
                 createInstance(Ci.nsIWindowsRegKey);
  try {
    registry.open(rootKey, "Software\\Mozilla\\" + appInfoName + "\\TaskBarIDs",
                  Ci.nsIWindowsRegKey.ACCESS_READ);
    if (registry.hasValue(exePath)) {
      return registry.readStringValue(exePath);
    }
  } catch (ex) {
  } finally {
    registry.close();
  }
  return undefined;
};

/*
 * Migrates old update directory files to the new update directory
 * which is based on a hash of the installation.
 */
function migrateOldUpdateDir() {
  // Get the old udpate root leaf dir. It is based on the sub directory of
  // program files, or if the exe path is not inside program files, the appname.
  var appinfo = Components.classes["@mozilla.org/xre/app-info;1"].
                getService(Components.interfaces.nsIXULAppInfo).
                QueryInterface(Components.interfaces.nsIXULRuntime);
  var updateLeafName;
  var programFiles = FileUtils.getFile("ProgF", []);
  var exeFile = FileUtils.getFile("XREExeF", []);
  if (exeFile.path.substring(0, programFiles.path.length).toLowerCase() ==
      programFiles.path.toLowerCase()) {
    updateLeafName = exeFile.parent.leafName;
  } else {
    updateLeafName = appinfo.name;
  }

  // Get the old update root dir
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
  var newUpdateRoot = FileUtils.getDir("UpdRootD", [], true);

  // If there is no taskbar ID then we want to retry this migration
  // at a later time if the application gets a taskbar ID.
  var taskbarID = getTaskbarIDHash(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                                   exeFile.parent.path, appinfo.name);
  if (!taskbarID) {
    taskbarID = getTaskbarIDHash(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                                 exeFile.parent.path, appinfo.name);
    if (!taskbarID) {
      return;
    }
  }

  Services.prefs.setBoolPref(PREF_APP_UPDATE_MIGRATE_APP_DIR, true);

  // Sanity checks only to ensure we don't delete something we don't mean to.
  if (oldUpdateRoot.path.toLowerCase() == newUpdateRoot.path.toLowerCase() ||
      updateLeafName.length == 0) {
    return;
  }

  // If the old update root doesn't exist then we have already migrated
  // or else there is no work to do.
  if (!oldUpdateRoot.exists()) {
    return;
  }

  // Get an array of all of the files we want to migrate.
  // We do this so we don't copy anything extra.
  var filesToMigrate = [FILE_UPDATES_DB, FILE_UPDATE_ACTIVE,
                        ["updates", FILE_LAST_LOG], ["updates", FILE_BACKUP_LOG],
                        ["updates", "0", FILE_UPDATE_STATUS]];

  // Move each of those files to the new directory
  filesToMigrate.forEach(relPath => {
    let oldFile = oldUpdateRoot.clone();
    let newFile = newUpdateRoot.clone();
    if (relPath instanceof Array) {
      relPath.forEach(relPathPart => {
        oldFile.append(relPathPart);
        newFile.append(relPathPart);
      });
    } else {
      oldFile.append(relPath);
      newFile.append(relPath);
    }

    try {
      if (!newFile.exists()) {
        oldFile.moveTo(newFile.parent, newFile.leafName);
      }
    } catch (e) {
      Components.utils.reportError(e);
    }
  });

  oldUpdateRoot.remove(true);
}
#endif

/**
 * Gets the specified directory at the specified hierarchy under the update root
 * directory without creating it if it doesn't exist.
 * @param   pathArray
 *          An array of path components to locate beneath the directory
 *          specified by |key|
 * @return  nsIFile object for the location specified.
 */
function getUpdateDirNoCreate(pathArray) {
  return FileUtils.getDir(KEY_UPDROOT, pathArray, false);
}

function UpdateServiceStub() {
#ifdef XP_WIN
  // Don't attempt this migration more than once for perf reasons
  var migrated = 0;
  try {
    migrated = Services.prefs.getBoolPref(PREF_APP_UPDATE_MIGRATE_APP_DIR);
  } catch (e) {
  }

  if (!migrated) {
    try {
      migrateOldUpdateDir();
    } catch (e) {
      Components.utils.reportError(e);
    }
  }
#endif

  let statusFile = getUpdateDirNoCreate([DIR_UPDATES, "0"]);
  statusFile.append(FILE_UPDATE_STATUS);
  // If the update.status file exists then initiate post update processing.
  if (statusFile.exists()) {
    let aus = Components.classes["@mozilla.org/updates/update-service;1"].
              getService(Ci.nsIApplicationUpdateService).
              QueryInterface(Ci.nsIObserver);
    aus.observe(null, "post-update-processing", "");
  }
}
UpdateServiceStub.prototype = {
  observe: function(){},
  classID: Components.ID("{e43b0010-04ba-4da6-b523-1f92580bc150}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([UpdateServiceStub]);
