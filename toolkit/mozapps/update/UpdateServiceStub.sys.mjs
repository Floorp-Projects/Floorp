/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UpdateLog: "resource://gre/modules/UpdateLog.sys.mjs",
});

if (AppConstants.ENABLE_WEBDRIVER) {
  XPCOMUtils.defineLazyServiceGetter(
    lazy,
    "Marionette",
    "@mozilla.org/remote/marionette;1",
    "nsIMarionette"
  );

  XPCOMUtils.defineLazyServiceGetter(
    lazy,
    "RemoteAgent",
    "@mozilla.org/remote/agent;1",
    "nsIRemoteAgent"
  );
} else {
  lazy.Marionette = { running: false };
  lazy.RemoteAgent = { running: false };
}

const DIR_UPDATES = "updates";
const FILE_UPDATE_STATUS = "update.status";

const KEY_UPDROOT = "UpdRootD";
const KEY_OLD_UPDROOT = "OldUpdRootD";

// The pref prefix below should have the hash of the install path appended to
// ensure that this is a per-installation pref (i.e. to ensure that migration
// happens for every install rather than once per profile)
const PREF_PREFIX_UPDATE_DIR_MIGRATED = "app.update.migrated.updateDir3.";
const PREF_APP_UPDATE_ALTUPDATEDIRPATH = "app.update.altUpdateDirPath";
const PREF_APP_UPDATE_DISABLEDFORTESTING = "app.update.disabledForTesting";

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

export class UpdateServiceStub {
  #initUpdatePromise;
  #initStubHasRun = false;

  /**
   * See nsIUpdateService.idl
   */
  async init() {
    await this.#init(false);
  }
  async initUpdate() {
    await this.#init(true);
  }

  #initOnlyStub(updateDir) {
    // We may need to migrate update data
    let prefUpdateDirMigrated =
      PREF_PREFIX_UPDATE_DIR_MIGRATED + updateDir.leafName;
    if (
      AppConstants.platform == "win" &&
      !Services.prefs.getBoolPref(prefUpdateDirMigrated, false)
    ) {
      Services.prefs.setBoolPref(prefUpdateDirMigrated, true);
      try {
        migrateUpdateDirectory();
      } catch (ex) {
        // For the most part, migrateUpdateDirectory() catches its own
        // errors. But there are technically things that could happen that
        // might not be caught, like nsIFile.parent or nsIFile.append could
        // unexpectedly fail.
        // So we will catch any errors here, just in case.
        LOG(
          `UpdateServiceStub:UpdateServiceStub Failed to migrate update ` +
            `directory. Exception: ${ex}`
        );
      }
    }
  }

  async #initUpdate() {
    // Ensure that the constructors for the update services have run.
    const aus = Cc["@mozilla.org/updates/update-service;1"].getService(
      Ci.nsIApplicationUpdateService
    );
    Cc["@mozilla.org/updates/update-manager;1"].getService(Ci.nsIUpdateManager);
    Cc["@mozilla.org/updates/update-checker;1"].getService(Ci.nsIUpdateChecker);

    // Run update service initialization
    await aus.internal.init();
  }

  async #init(force_update_init) {
    if (this.updateDisabled) {
      return;
    }

    // We call into this from many places to ensure that initialization is done,
    // so we want to optimize for the case where initialization is already
    // finished.
    if (this.#initUpdatePromise) {
      try {
        await this.#initUpdatePromise;
      } catch (ex) {
        // This will already have been logged when the error first happened, we
        // don't need to log it again now.
      }
      return;
    }

    LOG(`UpdateServiceStub - Begin (force_update_init=${force_update_init})`);

    let initUpdate = force_update_init;
    try {
      const updateDir = getUpdateBaseDirNoCreate();
      if (!this.#initStubHasRun) {
        this.#initStubHasRun = true;
        try {
          this.#initOnlyStub(updateDir);
        } catch (ex) {
          LOG(
            `UpdateServiceStub - Stub initialization failed: ${ex}: ${ex.stack}`
          );
        }
      }

      try {
        if (!initUpdate) {
          const statusFile = updateDir.clone();
          statusFile.append(DIR_UPDATES);
          statusFile.append("0");
          statusFile.append(FILE_UPDATE_STATUS);
          initUpdate = statusFile.exists();
        }
      } catch (ex) {
        LOG(
          `UpdateServiceStub - Failed to generate the status file: ${ex}: ${ex.stack}`
        );
      }
    } catch (e) {
      LOG(
        `UpdateServiceStub - Failed to get update directory: ${e}: ${e.stack}`
      );
    }

    if (initUpdate) {
      this.#initUpdatePromise = this.#initUpdate();
      try {
        await this.#initUpdatePromise;
      } catch (ex) {
        LOG(`UpdateServiceStub - Init failed: ${ex}: ${ex.stack}`);
      }
    }
  }

  /**
   * See nsIUpdateService.idl
   */
  get updateDisabledForTesting() {
    return (
      (Cu.isInAutomation ||
        lazy.Marionette.running ||
        lazy.RemoteAgent.running) &&
      Services.prefs.getBoolPref(PREF_APP_UPDATE_DISABLEDFORTESTING, false)
    );
  }

  /**
   * See nsIUpdateService.idl
   */
  get updateDisabled() {
    return (
      (Services.policies && !Services.policies.isAllowed("appUpdate")) ||
      this.updateDisabledForTesting ||
      Services.sysinfo.getProperty("isPackagedApp")
    );
  }

  async observe(_subject, topic, _data) {
    switch (topic) {
      // This is sort of the "default" way of being initialized. The
      // "@mozilla.org/updates/update-service-stub;1" contract definition in
      // `components.conf` registers us for this notification, which we use to
      // trigger initialization. Note, however, that this calls only `init` and
      // not `initUpdate`.
      case "profile-after-change":
        await this.init();
        break;
    }
  }

  classID = Components.ID("{e43b0010-04ba-4da6-b523-1f92580bc150}");
  QueryInterface = ChromeUtils.generateQI([
    Ci.nsIApplicationUpdateServiceStub,
    Ci.nsIObserver,
  ]);
}

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
