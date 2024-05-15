/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UpdateLog: "resource://gre/modules/UpdateLog.sys.mjs",
});

const DIR_UPDATES = "updates";
const FILE_UPDATE_STATUS = "update.status";

const KEY_UPDROOT = "UpdRootD";
const KEY_OLD_UPDROOT = "OldUpdRootD";

// The pref prefix below should have the hash of the install path appended to
// ensure that this is a per-installation pref (i.e. to ensure that migration
// happens for every install rather than once per profile)
const PREF_PREFIX_UPDATE_DIR_MIGRATED = "app.update.migrated.updateDir3.";
const PREF_APP_UPDATE_ALTUPDATEDIRPATH = "app.update.altUpdateDirPath";

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

  return FileUtils.getDir(KEY_UPDROOT, []);
}

export function UpdateServiceStub() {
  LOG("UpdateServiceStub - Begin.");

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
    Services.prefs.setBoolPref(prefUpdateDirMigrated, true);
    try {
      migrateUpdateDirectory();
    } catch (ex) {
      // For the most part, migrateUpdateDirectory() catches its own errors.
      // But there are technically things that could happen that might not be
      // caught, like nsIFile.parent or nsIFile.append could unexpectedly fail.
      // So we will catch any errors here, just in case.
      LOG(
        `UpdateServiceStub:UpdateServiceStub Failed to migrate update ` +
          `directory. Exception: ${ex}`
      );
    }
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

/**
 * This function should be called when there are files in the old update
 * directory that may need to be migrated to the new update directory.
 */
function migrateUpdateDirectory() {
  LOG("UpdateServiceStub:migrateUpdateDirectory Performing migration");

  let sourceRootDir = FileUtils.getDir(KEY_OLD_UPDROOT, []);
  let destRootDir = FileUtils.getDir(KEY_UPDROOT, []);
  let hash = destRootDir.leafName;

  if (!sourceRootDir.exists()) {
    // Nothing to migrate.
    return;
  }

  // List of files to migrate. Each is specified as a list of path components.
  const toMigrate = [
    ["updates.xml"],
    ["active-update.xml"],
    ["update-config.json"],
    ["updates", "last-update.log"],
    ["updates", "backup-update.log"],
    ["updates", "downloading", FILE_UPDATE_STATUS],
    ["updates", "downloading", "update.mar"],
    ["updates", "0", FILE_UPDATE_STATUS],
    ["updates", "0", "update.mar"],
    ["updates", "0", "update.version"],
    ["updates", "0", "update.log"],
    ["backgroundupdate", "datareporting", "glean", "db", "data.safe.bin"],
  ];

  // Before we copy anything, double check that a different profile hasn't
  // already performed migration. If we don't have the necessary permissions to
  // remove the pre-migration files, we don't want to copy any old files and
  // potentially make the current update state inconsistent.
  for (let pathComponents of toMigrate) {
    // Assemble the destination nsIFile.
    let destFile = destRootDir.clone();
    for (let pathComponent of pathComponents) {
      destFile.append(pathComponent);
    }

    if (destFile.exists()) {
      LOG(
        `UpdateServiceStub:migrateUpdateDirectory Aborting migration because ` +
          `"${destFile.path}" already exists.`
      );
      return;
    }
  }

  // Before we migrate everything in toMigrate, there are a few things that
  // need special handling.
  let sourceRootParent = sourceRootDir.parent.parent;
  let destRootParent = destRootDir.parent.parent;

  let profileCountFile = sourceRootParent.clone();
  profileCountFile.append(`profile_count_${hash}.json`);
  migrateFile(profileCountFile, destRootParent);

  const updatePingPrefix = `uninstall_ping_${hash}_`;
  const updatePingSuffix = ".json";
  try {
    for (let file of sourceRootParent.directoryEntries) {
      if (
        file.leafName.startsWith(updatePingPrefix) &&
        file.leafName.endsWith(updatePingSuffix)
      ) {
        migrateFile(file, destRootParent);
      }
    }
  } catch (ex) {
    // migrateFile should catch its own errors, but it is possible that
    // sourceRootParent.directoryEntries could throw.
    LOG(
      `UpdateServiceStub:migrateUpdateDirectory Failed to migrate uninstall ` +
        `ping. Exception: ${ex}`
    );
  }

  // Migrate "backgroundupdate.moz_log" and child process logs like
  // "backgroundupdate.child-1.moz_log".
  const backgroundLogPrefix = `backgroundupdate`;
  const backgroundLogSuffix = ".moz_log";
  try {
    for (let file of sourceRootDir.directoryEntries) {
      if (
        file.leafName.startsWith(backgroundLogPrefix) &&
        file.leafName.endsWith(backgroundLogSuffix)
      ) {
        migrateFile(file, destRootDir);
      }
    }
  } catch (ex) {
    LOG(
      `UpdateServiceStub:migrateUpdateDirectory Failed to migrate background ` +
        `log file. Exception: ${ex}`
    );
  }

  const pendingPingRelDir =
    "backgroundupdate\\datareporting\\glean\\pending_pings";
  let pendingPingSourceDir = sourceRootDir.clone();
  pendingPingSourceDir.appendRelativePath(pendingPingRelDir);
  let pendingPingDestDir = destRootDir.clone();
  pendingPingDestDir.appendRelativePath(pendingPingRelDir);
  // Pending ping filenames are UUIDs.
  const pendingPingFilenameRegex =
    /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
  if (pendingPingSourceDir.exists()) {
    try {
      for (let file of pendingPingSourceDir.directoryEntries) {
        if (pendingPingFilenameRegex.test(file.leafName)) {
          migrateFile(file, pendingPingDestDir);
        }
      }
    } catch (ex) {
      // migrateFile should catch its own errors, but it is possible that
      // pendingPingSourceDir.directoryEntries could throw.
      LOG(
        `UpdateServiceStub:migrateUpdateDirectory Failed to migrate ` +
          `pending pings. Exception: ${ex}`
      );
    }
  }

  // Migrate everything in toMigrate.
  for (let pathComponents of toMigrate) {
    let filename = pathComponents.pop();

    // Assemble the source and destination nsIFile's.
    let sourceFile = sourceRootDir.clone();
    let destDir = destRootDir.clone();
    for (let pathComponent of pathComponents) {
      sourceFile.append(pathComponent);
      destDir.append(pathComponent);
    }
    sourceFile.append(filename);

    migrateFile(sourceFile, destDir);
  }

  // There is no reason to keep this file, and it often hangs around and could
  // interfere with cleanup.
  let updateLockFile = sourceRootParent.clone();
  updateLockFile.append(`UpdateLock-${hash}`);
  try {
    updateLockFile.remove(false);
  } catch (ex) {}

  // We want to recursively remove empty directories out of the sourceRootDir.
  // And if that was the only remaining update directory in sourceRootParent,
  // we want to remove that too. But we don't want to recurse into other update
  // directories in sourceRootParent.
  //
  // Potentially removes "C:\ProgramData\Mozilla\updates\<hash>" and
  // subdirectories.
  cleanupDir(sourceRootDir, true);
  // Potentially removes "C:\ProgramData\Mozilla\updates"
  cleanupDir(sourceRootDir.parent, false);
  // Potentially removes "C:\ProgramData\Mozilla"
  cleanupDir(sourceRootParent, false);
}

/**
 * Attempts to move the source file to the destination directory. If the file
 * cannot be moved, we attempt to copy it and remove the original. All errors
 * are logged, but no exceptions are thrown. Both arguments must be of type
 * nsIFile and are expected to be regular files.
 *
 * Non-existent files are silently ignored.
 *
 * The reason that we are migrating is to deal with problematic inherited
 * permissions. But, luckily, neither nsIFile.moveTo nor nsIFile.copyTo preserve
 * inherited permissions.
 */
function migrateFile(sourceFile, destDir) {
  if (!sourceFile.exists()) {
    return;
  }

  if (sourceFile.isDirectory()) {
    LOG(
      `UpdateServiceStub:migrateFile Aborting attempt to migrate ` +
        `"${sourceFile.path}" because it is a directory.`
    );
    return;
  }

  // Create destination directory.
  try {
    // Pass an arbitrary value for permissions. Windows doesn't use octal
    // permissions, so that value doesn't really do anything.
    destDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0);
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_FILE_ALREADY_EXISTS) {
      LOG(
        `UpdateServiceStub:migrateFile Unable to create destination ` +
          `directory "${destDir.path}": ${ex}`
      );
    }
  }

  try {
    sourceFile.moveTo(destDir, null);
    return;
  } catch (ex) {}

  try {
    sourceFile.copyTo(destDir, null);
  } catch (ex) {
    LOG(
      `UpdateServiceStub:migrateFile Failed to migrate file from ` +
        `"${sourceFile.path}" to "${destDir.path}". Exception: ${ex}`
    );
    return;
  }

  try {
    sourceFile.remove(false);
  } catch (ex) {
    LOG(
      `UpdateServiceStub:migrateFile Successfully migrated file from ` +
        `"${sourceFile.path}" to "${destDir.path}", but was unable to remove ` +
        `the original. Exception: ${ex}`
    );
  }
}

/**
 * If recurse is true, recurses through the directory's contents. Any empty
 * directories are removed. Directories with remaining files are left behind.
 *
 * If recurse if false, we delete the directory passed as long as it is empty.
 *
 * All errors are silenced and not thrown.
 *
 * Returns true if the directory passed in was removed. Otherwise false.
 */
function cleanupDir(dir, recurse) {
  let directoryEmpty = true;
  try {
    for (let file of dir.directoryEntries) {
      if (!recurse) {
        // If we aren't recursing, bail out after we find a single file. The
        // directory isn't empty so we can't delete it, and we aren't going to
        // clean out and remove any other directories.
        return false;
      }
      if (file.isDirectory()) {
        if (!cleanupDir(file, recurse)) {
          directoryEmpty = false;
        }
      } else {
        directoryEmpty = false;
      }
    }
  } catch (ex) {
    // If any of our nsIFile calls fail, just err on the side of caution and
    // don't delete anything.
    return false;
  }

  if (directoryEmpty) {
    try {
      dir.remove(false);
      return true;
    } catch (ex) {}
  }
  return false;
}

/**
 * Logs a string to the error console.
 * @param   string
 *          The string to write to the error console.
 */
function LOG(string) {
  lazy.UpdateLog.logPrefixedString("AUS:STB", string);
}
