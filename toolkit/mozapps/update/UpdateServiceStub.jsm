/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/FileUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/AppConstants.jsm", this);

const DIR_UPDATES = "updates";
const FILE_UPDATE_STATUS = "update.status";
const FILE_ACTIVE_UPDATE_XML = "active-update.xml";
const FILE_LAST_UPDATE_LOG = "last-update.log";
const FILE_UPDATES_XML = "updates.xml";
const FILE_UPDATE_LOG = "update.log";
const FILE_UPDATE_MESSAGES = "update_messages.log";
const FILE_BACKUP_MESSAGES = "update_messages_old.log";

const KEY_UPDROOT = "UpdRootD";
const KEY_OLD_UPDROOT = "OldUpdRootD";
const KEY_PROFILE_DIR = "ProfD";

// The pref prefix below should have the hash of the install path appended to
// ensure that this is a per-installation pref (i.e. to ensure that migration
// happens for every install rather than once per profile)
const PREF_PREFIX_UPDATE_DIR_MIGRATED = "app.update.migrated.updateDir2.";
const PREF_APP_UPDATE_ALTUPDATEDIRPATH = "app.update.altUpdateDirPath";
const PREF_APP_UPDATE_LOG = "app.update.log";
const PREF_APP_UPDATE_FILE_LOGGING = "app.update.log.file";

XPCOMUtils.defineLazyGetter(this, "gLogEnabled", function aus_gLogEnabled() {
  return Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG, false);
});

function getUpdateBaseDirNoCreate() {
  if (Cu.isInAutomation) {
    // This allows tests to use an alternate updates directory so they can test
    // startup behavior.
    const MAGIC_TEST_ROOT_PREFIX = "<test-root>";
    const PREF_TEST_ROOT = "mochitest.testRoot";
    let alternatePath = Services.prefs.getCharPref(
      PREF_APP_UPDATE_ALTUPDATEDIRPATH,
      null
    );
    if (alternatePath && alternatePath.startsWith(MAGIC_TEST_ROOT_PREFIX)) {
      let testRoot = Services.prefs.getCharPref(PREF_TEST_ROOT);
      let relativePath = alternatePath.substring(MAGIC_TEST_ROOT_PREFIX.length);
      if (AppConstants.platform == "win") {
        relativePath = relativePath.replace(/\//g, "\\");
      }
      alternatePath = testRoot + relativePath;
      let updateDir = Cc["@mozilla.org/file/local;1"].createInstance(
        Ci.nsIFile
      );
      updateDir.initWithPath(alternatePath);
      LOG(
        "getUpdateBaseDirNoCreate returning test directory, path: " +
          updateDir.path
      );
      return updateDir;
    }
  }

  return FileUtils.getDir(KEY_UPDROOT, [], false);
}

function UpdateServiceStub() {
  let updateDir = getUpdateBaseDirNoCreate();
  let prefUpdateDirMigrated =
    PREF_PREFIX_UPDATE_DIR_MIGRATED + updateDir.leafName;

  let statusFile = updateDir;
  statusFile.append(DIR_UPDATES);
  statusFile.append("0");
  statusFile.append(FILE_UPDATE_STATUS);
  updateDir = null; // We don't need updateDir anymore, plus now its nsIFile
  // contains the status file's path

  // We may need to migrate update data
  if (
    AppConstants.platform == "win" &&
    !Services.prefs.getBoolPref(prefUpdateDirMigrated, false)
  ) {
    migrateUpdateDirectory();
    Services.prefs.setBoolPref(prefUpdateDirMigrated, true);
  }

  // Prevent file logging from persisting for more than a session by disabling
  // it on startup.
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_FILE_LOGGING, false)) {
    deactivateUpdateLogFile();
  }

  // If the update.status file exists then initiate post update processing.
  if (statusFile.exists()) {
    let aus = Cc["@mozilla.org/updates/update-service;1"]
      .getService(Ci.nsIApplicationUpdateService)
      .QueryInterface(Ci.nsIObserver);
    aus.observe(null, "post-update-processing", "");
  }
}
UpdateServiceStub.prototype = {
  observe() {},
  classID: Components.ID("{e43b0010-04ba-4da6-b523-1f92580bc150}"),
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
};

var EXPORTED_SYMBOLS = ["UpdateServiceStub"];

function deactivateUpdateLogFile() {
  LOG("Application update file logging being automatically turned off");
  Services.prefs.setBoolPref(PREF_APP_UPDATE_FILE_LOGGING, false);
  let logFile = Services.dirsvc.get(KEY_PROFILE_DIR, Ci.nsIFile);
  logFile.append(FILE_UPDATE_MESSAGES);

  try {
    logFile.moveTo(null, FILE_BACKUP_MESSAGES);
  } catch (e) {
    LOG(
      "Failed to backup update messages log (" +
        e +
        "). Attempting to " +
        "remove it."
    );
    try {
      logFile.remove(false);
    } catch (e) {
      LOG("Also failed to remove the update messages log: " + e);
    }
  }
}

/**
 * This function should be called when there are files in the old update
 * directory that may need to be migrated to the new update directory.
 */
function migrateUpdateDirectory() {
  let sourceRootDir = FileUtils.getDir(KEY_OLD_UPDROOT, [], false);
  let destRootDir = FileUtils.getDir(KEY_UPDROOT, [], false);

  if (!sourceRootDir.exists()) {
    LOG(
      "UpdateServiceStub:_migrateUpdateDirectory - Abort: No migration " +
        "necessary. Nothing to migrate."
    );
    return;
  }

  if (destRootDir.exists()) {
    // Migration must have already been done by another user
    LOG(
      "UpdateServiceStub:_migrateUpdateDirectory - migrated and unmigrated " +
        "update directories found. Deleting the unmigrated directory: " +
        sourceRootDir.path
    );
    try {
      sourceRootDir.remove(true);
    } catch (e) {
      LOG(
        "UpdateServiceStub:_migrateUpdateDirectory - Deletion of " +
          "unmigrated directory failed. Exception: " +
          e
      );
    }
    return;
  }

  let sourceUpdateDir = sourceRootDir.clone();
  sourceUpdateDir.append(DIR_UPDATES);
  let destUpdateDir = destRootDir.clone();
  destUpdateDir.append(DIR_UPDATES);

  let sourcePatchDir = sourceUpdateDir.clone();
  sourcePatchDir.append("0");
  let destPatchDir = destUpdateDir.clone();
  destPatchDir.append("0");

  let sourceStatusFile = sourcePatchDir.clone();
  sourceStatusFile.append(FILE_UPDATE_STATUS);
  let destStatusFile = destPatchDir.clone();
  destStatusFile.append(FILE_UPDATE_STATUS);

  let sourceActiveXML = sourceRootDir.clone();
  sourceActiveXML.append(FILE_ACTIVE_UPDATE_XML);
  try {
    sourceActiveXML.moveTo(destRootDir, sourceActiveXML.leafName);
  } catch (e) {
    LOG(
      "UpdateServiceStub:_migrateUpdateDirectory - Unable to move active " +
        "update XML file. Exception: " +
        e
    );
  }

  let sourceUpdateXML = sourceRootDir.clone();
  sourceUpdateXML.append(FILE_UPDATES_XML);
  try {
    sourceUpdateXML.moveTo(destRootDir, sourceUpdateXML.leafName);
  } catch (e) {
    LOG(
      "UpdateServiceStub:_migrateUpdateDirectory - Unable to move " +
        "update XML file. Exception: " +
        e
    );
  }

  let sourceUpdateLog = sourcePatchDir.clone();
  sourceUpdateLog.append(FILE_UPDATE_LOG);
  try {
    sourceUpdateLog.moveTo(destPatchDir, sourceUpdateLog.leafName);
  } catch (e) {
    LOG(
      "UpdateServiceStub:_migrateUpdateDirectory - Unable to move " +
        "update log file. Exception: " +
        e
    );
  }

  let sourceLastUpdateLog = sourceUpdateDir.clone();
  sourceLastUpdateLog.append(FILE_LAST_UPDATE_LOG);
  try {
    sourceLastUpdateLog.moveTo(destUpdateDir, sourceLastUpdateLog.leafName);
  } catch (e) {
    LOG(
      "UpdateServiceStub:_migrateUpdateDirectory - Unable to move " +
        "last-update log file. Exception: " +
        e
    );
  }

  try {
    sourceStatusFile.moveTo(destStatusFile.parent, destStatusFile.leafName);
  } catch (e) {
    LOG(
      "UpdateServiceStub:_migrateUpdateDirectory - Unable to move update " +
        "status file. Exception: " +
        e
    );
  }

  // Remove all remaining files in the old update directory. We don't need
  // them anymore
  try {
    sourceRootDir.remove(true);
  } catch (e) {
    LOG(
      "UpdateServiceStub:_migrateUpdateDirectory - Deletion of old update " +
        "directory failed. Exception: " +
        e
    );
  }
}

/**
 * Logs a string to the error console.
 * @param   string
 *          The string to write to the error console.
 */
function LOG(string) {
  if (gLogEnabled) {
    dump("*** AUS:SVC " + string + "\n");
    Services.console.logStringMessage("AUS:SVC " + string);
  }
}
