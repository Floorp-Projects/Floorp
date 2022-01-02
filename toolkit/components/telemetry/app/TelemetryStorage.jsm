/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TelemetryStorage", "Policy"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryStorage::";

const Telemetry = Services.telemetry;
const Utils = TelemetryUtils;

// Compute the path of the pings archive on the first use.
const DATAREPORTING_DIR = "datareporting";
const PINGS_ARCHIVE_DIR = "archived";
const ABORTED_SESSION_FILE_NAME = "aborted-session-ping";
const SESSION_STATE_FILE_NAME = "session-state.json";

XPCOMUtils.defineLazyGetter(this, "gDataReportingDir", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, DATAREPORTING_DIR);
});
XPCOMUtils.defineLazyGetter(this, "gPingsArchivePath", function() {
  return OS.Path.join(gDataReportingDir, PINGS_ARCHIVE_DIR);
});
XPCOMUtils.defineLazyGetter(this, "gAbortedSessionFilePath", function() {
  return OS.Path.join(gDataReportingDir, ABORTED_SESSION_FILE_NAME);
});
ChromeUtils.defineModuleGetter(
  this,
  "CommonUtils",
  "resource://services-common/utils.js"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryHealthPing",
  "resource://gre/modules/HealthPing.jsm"
);
// Maxmimum time, in milliseconds, archive pings should be retained.
const MAX_ARCHIVED_PINGS_RETENTION_MS = 60 * 24 * 60 * 60 * 1000; // 60 days

// Maximum space the archive can take on disk (in Bytes).
const ARCHIVE_QUOTA_BYTES = 120 * 1024 * 1024; // 120 MB
// Maximum space the outgoing pings can take on disk, for Desktop (in Bytes).
const PENDING_PINGS_QUOTA_BYTES_DESKTOP = 15 * 1024 * 1024; // 15 MB
// Maximum space the outgoing pings can take on disk, for Mobile (in Bytes).
const PENDING_PINGS_QUOTA_BYTES_MOBILE = 1024 * 1024; // 1 MB

// The maximum size a pending/archived ping can take on disk.
const PING_FILE_MAXIMUM_SIZE_BYTES = 1024 * 1024; // 1 MB

// This special value is submitted when the archive is outside of the quota.
const ARCHIVE_SIZE_PROBE_SPECIAL_VALUE = 300;

// This special value is submitted when the pending pings is outside of the quota, as
// we don't know the size of the pings above the quota.
const PENDING_PINGS_SIZE_PROBE_SPECIAL_VALUE = 17;

const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

/**
 * This is thrown by |TelemetryStorage.loadPingFile| when reading the ping
 * from the disk fails.
 */
function PingReadError(
  message = "Error reading the ping file",
  becauseNoSuchFile = false
) {
  Error.call(this, message);
  let error = new Error();
  this.name = "PingReadError";
  this.message = message;
  this.stack = error.stack;
  this.becauseNoSuchFile = becauseNoSuchFile;
}
PingReadError.prototype = Object.create(Error.prototype);
PingReadError.prototype.constructor = PingReadError;

/**
 * This is thrown by |TelemetryStorage.loadPingFile| when parsing the ping JSON
 * content fails.
 */
function PingParseError(message = "Error parsing ping content") {
  Error.call(this, message);
  let error = new Error();
  this.name = "PingParseError";
  this.message = message;
  this.stack = error.stack;
}
PingParseError.prototype = Object.create(Error.prototype);
PingParseError.prototype.constructor = PingParseError;

/**
 * This is a policy object used to override behavior for testing.
 */
var Policy = {
  now: () => new Date(),
  getArchiveQuota: () => ARCHIVE_QUOTA_BYTES,
  getPendingPingsQuota: () =>
    AppConstants.platform == "android"
      ? PENDING_PINGS_QUOTA_BYTES_MOBILE
      : PENDING_PINGS_QUOTA_BYTES_DESKTOP,
  /**
   * @param {string} id The ID of the ping that will be written into the file. Can be "*" to
   *                    make a pattern to find all pings for this installation.
   * @return
   *         {
   *           directory: <nsIFile>, // Directory to save pings
   *           file: <string>, // File name for this ping (or pattern for all pings)
   *         }
   */
  getUninstallPingPath: id => {
    // UpdRootD is e.g. C:\ProgramData\Mozilla\updates\<PATH HASH>
    const updateDirectory = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
    const installPathHash = updateDirectory.leafName;

    return {
      // e.g. C:\ProgramData\Mozilla
      directory: updateDirectory.parent.parent.clone(),
      file: `uninstall_ping_${installPathHash}_${id}.json`,
    };
  },
};

/**
 * Wait for all promises in iterable to resolve or reject. This function
 * always resolves its promise with undefined, and never rejects.
 */
function waitForAll(it) {
  let dummy = () => {};
  let promises = Array.from(it, p => p.catch(dummy));
  return Promise.all(promises);
}

/**
 * Permanently intern the given string. This is mainly used for the ping.type
 * strings that can be excessively duplicated in the _archivedPings map. Do not
 * pass large or temporary strings to this function.
 */
function internString(str) {
  return Symbol.keyFor(Symbol.for(str));
}

var TelemetryStorage = {
  get pingDirectoryPath() {
    return OS.Path.join(OS.Constants.Path.profileDir, "saved-telemetry-pings");
  },

  /**
   * The maximum size a ping can have, in bytes.
   */
  get MAXIMUM_PING_SIZE() {
    return PING_FILE_MAXIMUM_SIZE_BYTES;
  },
  /**
   * Shutdown & block on any outstanding async activity in this module.
   *
   * @return {Promise} Promise that is resolved when shutdown is complete.
   */
  shutdown() {
    return TelemetryStorageImpl.shutdown();
  },

  /**
   * Save an archived ping to disk.
   *
   * @param {object} ping The ping data to archive.
   * @return {promise} Promise that is resolved when the ping is successfully archived.
   */
  saveArchivedPing(ping) {
    return TelemetryStorageImpl.saveArchivedPing(ping);
  },

  /**
   * Load an archived ping from disk.
   *
   * @param {string} id The pings id.
   * @return {promise<object>} Promise that is resolved with the ping data.
   */
  loadArchivedPing(id) {
    return TelemetryStorageImpl.loadArchivedPing(id);
  },

  /**
   * Get a list of info on the archived pings.
   * This will scan the archive directory and grab basic data about the existing
   * pings out of their filename.
   *
   * @return {promise<sequence<object>>}
   */
  loadArchivedPingList() {
    return TelemetryStorageImpl.loadArchivedPingList();
  },

  /**
   * Clean the pings archive by removing old pings.
   * This will scan the archive directory.
   *
   * @return {Promise} Resolved when the cleanup task completes.
   */
  runCleanPingArchiveTask() {
    return TelemetryStorageImpl.runCleanPingArchiveTask();
  },

  /**
   * Run the task to enforce the pending pings quota.
   *
   * @return {Promise} Resolved when the cleanup task completes.
   */
  runEnforcePendingPingsQuotaTask() {
    return TelemetryStorageImpl.runEnforcePendingPingsQuotaTask();
  },

  /**
   * Run the task to remove all the pending pings
   *
   * @return {Promise} Resolved when the pings are removed.
   */
  runRemovePendingPingsTask() {
    return TelemetryStorageImpl.runRemovePendingPingsTask();
  },

  /**
   * Remove all pings that are stored in the userApplicationDataDir
   * under the "Pending Pings" sub-directory.
   */
  removeAppDataPings() {
    return TelemetryStorageImpl.removeAppDataPings();
  },

  /**
   * Reset the storage state in tests.
   */
  reset() {
    return TelemetryStorageImpl.reset();
  },

  /**
   * Test method that allows waiting on the archive clean task to finish.
   */
  testCleanupTaskPromise() {
    return TelemetryStorageImpl._cleanArchiveTask || Promise.resolve();
  },

  /**
   * Test method that allows waiting on the pending pings quota task to finish.
   */
  testPendingQuotaTaskPromise() {
    return (
      TelemetryStorageImpl._enforcePendingPingsQuotaTask || Promise.resolve()
    );
  },

  /**
   * Save a pending - outgoing - ping to disk and track it.
   *
   * @param {Object} ping The ping data.
   * @return {Promise} Resolved when the ping was saved.
   */
  savePendingPing(ping) {
    return TelemetryStorageImpl.savePendingPing(ping);
  },

  /**
   * Saves session data to disk.
   * @param {Object}  sessionData The session data.
   * @return {Promise} Resolved when the data was saved.
   */
  saveSessionData(sessionData) {
    return TelemetryStorageImpl.saveSessionData(sessionData);
  },

  /**
   * Loads session data from a session data file.
   * @return {Promise<object>} Resolved with the session data in object form.
   */
  loadSessionData() {
    return TelemetryStorageImpl.loadSessionData();
  },

  /**
   * Load a pending ping from disk by id.
   *
   * @param {String} id The pings id.
   * @return {Promise} Resolved with the loaded ping data.
   */
  loadPendingPing(id) {
    return TelemetryStorageImpl.loadPendingPing(id);
  },

  /**
   * Remove a pending ping from disk by id.
   *
   * @param {String} id The pings id.
   * @return {Promise} Resolved when the ping was removed.
   */
  removePendingPing(id) {
    return TelemetryStorageImpl.removePendingPing(id);
  },

  /**
   * Returns a list of the currently pending pings in the format:
   * {
   *   id: <string>, // The pings UUID.
   *   lastModificationDate: <number>, // Timestamp of the pings last modification.
   * }
   * This populates the list by scanning the disk.
   *
   * @return {Promise<sequence>} Resolved with the ping list.
   */
  loadPendingPingList() {
    return TelemetryStorageImpl.loadPendingPingList();
  },

  /**
   * Returns a list of the currently pending pings in the format:
   * {
   *   id: <string>, // The pings UUID.
   *   lastModificationDate: <number>, // Timestamp of the pings last modification.
   * }
   * This does not scan pending pings on disk.
   *
   * @return {sequence} The current pending ping list.
   */
  getPendingPingList() {
    return TelemetryStorageImpl.getPendingPingList();
  },

  /**
   * Save an aborted-session ping to disk. This goes to a special location so
   * it is not picked up as a pending ping.
   *
   * @param {object} ping The ping data to save.
   * @return {promise} Promise that is resolved when the ping is successfully saved.
   */
  saveAbortedSessionPing(ping) {
    return TelemetryStorageImpl.saveAbortedSessionPing(ping);
  },

  /**
   * Load the aborted-session ping from disk if present.
   *
   * @return {promise<object>} Promise that is resolved with the ping data if found.
   *                           Otherwise returns null.
   */
  loadAbortedSessionPing() {
    return TelemetryStorageImpl.loadAbortedSessionPing();
  },

  /**
   * Remove the aborted-session ping if present.
   *
   * @return {promise} Promise that is resolved once the ping is removed.
   */
  removeAbortedSessionPing() {
    return TelemetryStorageImpl.removeAbortedSessionPing();
  },

  /**
   * Save an uninstall ping to disk, removing any old ones from this
   * installation first.
   * This is stored independently from other pings, and only read by
   * the Windows uninstaller.
   *
   * WINDOWS ONLY, does nothing and resolves immediately on other platforms.
   *
   * @return {promise} Promise that is resolved when the ping has been saved.
   */
  saveUninstallPing(ping) {
    return TelemetryStorageImpl.saveUninstallPing(ping);
  },

  /**
   * Remove all uninstall pings from this installation.
   *
   * WINDOWS ONLY, does nothing and resolves immediately on other platforms.
   *
   * @return {promise} Promise that is resolved when the pings have been removed.
   */
  removeUninstallPings() {
    return TelemetryStorageImpl.removeUninstallPings();
  },

  /**
   * Save a single ping to a file.
   *
   * @param {object} ping The content of the ping to save.
   * @param {string} file The destination file.
   * @param {bool} overwrite If |true|, the file will be overwritten if it exists,
   * if |false| the file will not be overwritten and no error will be reported if
   * the file exists.
   * @returns {promise}
   */
  savePingToFile(ping, file, overwrite) {
    return TelemetryStorageImpl.savePingToFile(ping, file, overwrite);
  },

  /**
   * Save a ping to its file.
   *
   * @param {object} ping The content of the ping to save.
   * @param {bool} overwrite If |true|, the file will be overwritten
   * if it exists.
   * @returns {promise}
   */
  savePing(ping, overwrite) {
    return TelemetryStorageImpl.savePing(ping, overwrite);
  },

  /**
   * Add a ping to the saved pings directory so that it gets saved
   * and sent along with other pings.
   *
   * @param {Object} pingData The ping object.
   * @return {Promise} A promise resolved when the ping is saved to the pings directory.
   */
  addPendingPing(pingData) {
    return TelemetryStorageImpl.addPendingPing(pingData);
  },

  /**
   * Remove the file for a ping
   *
   * @param {object} ping The ping.
   * @returns {promise}
   */
  cleanupPingFile(ping) {
    return TelemetryStorageImpl.cleanupPingFile(ping);
  },

  /**
   * Loads a ping file.
   * @param {String} aFilePath The path of the ping file.
   * @return {Promise<Object>} A promise resolved with the ping content or rejected if the
   *                           ping contains invalid data.
   */
  async loadPingFile(aFilePath) {
    return TelemetryStorageImpl.loadPingFile(aFilePath);
  },

  /**
   * Remove FHR database files. This is temporary and will be dropped in
   * the future.
   * @return {Promise} Resolved when the database files are deleted.
   */
  removeFHRDatabase() {
    return TelemetryStorageImpl.removeFHRDatabase();
  },

  /**
   * Only used in tests, builds an archived ping path from the ping metadata.
   * @param {String} aPingId The ping id.
   * @param {Object} aDate The ping creation date.
   * @param {String} aType The ping type.
   * @return {String} The full path to the archived ping.
   */
  _testGetArchivedPingPath(aPingId, aDate, aType) {
    return getArchivedPingPath(aPingId, aDate, aType);
  },

  /**
   * Only used in tests, this helper extracts ping metadata from a given filename.
   *
   * @param fileName {String} The filename.
   * @return {Object} Null if the filename didn't match the expected form.
   *                  Otherwise an object with the extracted data in the form:
   *                  { timestamp: <number>,
   *                    id: <string>,
   *                    type: <string> }
   */
  _testGetArchivedPingDataFromFileName(aFileName) {
    return TelemetryStorageImpl._getArchivedPingDataFromFileName(aFileName);
  },

  /**
   * Only used in tests, this helper allows cleaning up the pending ping storage.
   */
  testClearPendingPings() {
    return TelemetryStorageImpl.runRemovePendingPingsTask();
  },
};

/**
 * This object allows the serialisation of asynchronous tasks. This is particularly
 * useful to serialise write access to the disk in order to prevent race conditions
 * to corrupt the data being written.
 * We are using this to synchronize saving to the file that TelemetrySession persists
 * its state in.
 */
function SaveSerializer() {
  this._queuedOperations = [];
  this._queuedInProgress = false;
  this._log = Log.repository.getLoggerWithMessagePrefix(
    LOGGER_NAME,
    LOGGER_PREFIX
  );
}

SaveSerializer.prototype = {
  /**
   * Enqueues an operation to a list to serialise their execution in order to prevent race
   * conditions. Useful to serialise access to disk.
   *
   * @param {Function} aFunction The task function to enqueue. It must return a promise.
   * @return {Promise} A promise resolved when the enqueued task completes.
   */
  enqueueTask(aFunction) {
    let promise = new Promise((resolve, reject) =>
      this._queuedOperations.push([aFunction, resolve, reject])
    );

    if (this._queuedOperations.length == 1) {
      this._popAndPerformQueuedOperation();
    }
    return promise;
  },

  /**
   * Make sure to flush all the pending operations.
   * @return {Promise} A promise resolved when all the pending operations have completed.
   */
  flushTasks() {
    let dummyTask = () => new Promise(resolve => resolve());
    return this.enqueueTask(dummyTask);
  },

  /**
   * Pop a task from the queue, executes it and continue to the next one.
   * This function recursively pops all the tasks.
   */
  _popAndPerformQueuedOperation() {
    if (!this._queuedOperations.length || this._queuedInProgress) {
      return;
    }

    this._log.trace(
      "_popAndPerformQueuedOperation - Performing queued operation."
    );
    let [func, resolve, reject] = this._queuedOperations.shift();
    let promise;

    try {
      this._queuedInProgress = true;
      promise = func();
    } catch (ex) {
      this._log.warn(
        "_popAndPerformQueuedOperation - Queued operation threw during execution. ",
        ex
      );
      this._queuedInProgress = false;
      reject(ex);
      this._popAndPerformQueuedOperation();
      return;
    }

    if (!promise || typeof promise.then != "function") {
      let msg = "Queued operation did not return a promise: " + func;
      this._log.warn("_popAndPerformQueuedOperation - " + msg);

      this._queuedInProgress = false;
      reject(new Error(msg));
      this._popAndPerformQueuedOperation();
      return;
    }

    promise.then(
      result => {
        this._queuedInProgress = false;
        resolve(result);
        this._popAndPerformQueuedOperation();
      },
      error => {
        this._log.warn(
          "_popAndPerformQueuedOperation - Failure when performing queued operation.",
          error
        );
        this._queuedInProgress = false;
        reject(error);
        this._popAndPerformQueuedOperation();
      }
    );
  },
};

var TelemetryStorageImpl = {
  _logger: null,
  // Used to serialize aborted session ping writes to disk.
  _abortedSessionSerializer: new SaveSerializer(),
  // Used to serialize session state writes to disk.
  _stateSaveSerializer: new SaveSerializer(),

  // Tracks the archived pings in a Map of (id -> {timestampCreated, type}).
  // We use this to cache info on archived pings to avoid scanning the disk more than once.
  _archivedPings: new Map(),
  // A set of promises for pings currently being archived
  _activelyArchiving: new Set(),
  // Track the archive loading task to prevent multiple tasks from being executed.
  _scanArchiveTask: null,
  // Track the archive cleanup task.
  _cleanArchiveTask: null,
  // Whether we already scanned the archived pings on disk.
  _scannedArchiveDirectory: false,

  // Track the pending ping removal task.
  _removePendingPingsTask: null,

  // This tracks all the pending async ping save activity.
  _activePendingPingSaves: new Set(),

  // Tracks the pending pings in a Map of (id -> {timestampCreated, type}).
  // We use this to cache info on pending pings to avoid scanning the disk more than once.
  _pendingPings: new Map(),

  // Track the pending pings enforce quota task.
  _enforcePendingPingsQuotaTask: null,

  // Track the shutdown process to bail out of the clean up task quickly.
  _shutdown: false,

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX
      );
    }

    return this._logger;
  },

  /**
   * Shutdown & block on any outstanding async activity in this module.
   *
   * @return {Promise} Promise that is resolved when shutdown is complete.
   */
  async shutdown() {
    this._shutdown = true;

    // If the following tasks are still running, block on them. They will bail out as soon
    // as possible.
    await this._abortedSessionSerializer.flushTasks().catch(ex => {
      this._log.error("shutdown - failed to flush aborted-session writes", ex);
    });

    if (this._cleanArchiveTask) {
      await this._cleanArchiveTask.catch(ex => {
        this._log.error("shutdown - the archive cleaning task failed", ex);
      });
    }

    if (this._enforcePendingPingsQuotaTask) {
      await this._enforcePendingPingsQuotaTask.catch(ex => {
        this._log.error("shutdown - the pending pings quota task failed", ex);
      });
    }

    if (this._removePendingPingsTask) {
      await this._removePendingPingsTask.catch(ex => {
        this._log.error("shutdown - the pending pings removal task failed", ex);
      });
    }

    // Wait on pending pings still being saved. While OS.File should have shutdown
    // blockers in place, we a) have seen weird errors being reported that might
    // indicate a bad shutdown path and b) might have completion handlers hanging
    // off the save operations that don't expect to be late in shutdown.
    await this.promisePendingPingSaves();
  },

  /**
   * Save an archived ping to disk.
   *
   * @param {object} ping The ping data to archive.
   * @return {promise} Promise that is resolved when the ping is successfully archived.
   */
  saveArchivedPing(ping) {
    let promise = this._saveArchivedPingTask(ping);
    this._activelyArchiving.add(promise);
    promise.then(
      r => {
        this._activelyArchiving.delete(promise);
      },
      e => {
        this._activelyArchiving.delete(promise);
      }
    );
    return promise;
  },

  async _saveArchivedPingTask(ping) {
    const creationDate = new Date(ping.creationDate);
    if (this._archivedPings.has(ping.id)) {
      const data = this._archivedPings.get(ping.id);
      if (data.timestampCreated > creationDate.getTime()) {
        this._log.error(
          "saveArchivedPing - trying to overwrite newer ping with the same id"
        );
        return Promise.reject(
          new Error("trying to overwrite newer ping with the same id")
        );
      }
      this._log.warn(
        "saveArchivedPing - overwriting older ping with the same id"
      );
    }

    // Get the archived ping path and append the lz4 suffix to it (so we have 'jsonlz4').
    const filePath =
      getArchivedPingPath(ping.id, creationDate, ping.type) + "lz4";
    await OS.File.makeDir(OS.Path.dirname(filePath), {
      ignoreExisting: true,
      from: OS.Constants.Path.profileDir,
    });
    await this.savePingToFile(
      ping,
      filePath,
      /* overwrite*/ true,
      /* compressed*/ true
    );

    this._archivedPings.set(ping.id, {
      timestampCreated: creationDate.getTime(),
      type: internString(ping.type),
    });

    Telemetry.getHistogramById("TELEMETRY_ARCHIVE_SESSION_PING_COUNT").add();
    return undefined;
  },

  /**
   * Load an archived ping from disk.
   *
   * @param {string} id The pings id.
   * @return {promise<object>} Promise that is resolved with the ping data.
   */
  async loadArchivedPing(id) {
    const data = this._archivedPings.get(id);
    if (!data) {
      this._log.trace("loadArchivedPing - no ping with id: " + id);
      return Promise.reject(
        new Error("TelemetryStorage.loadArchivedPing - no ping with id " + id)
      );
    }

    const path = getArchivedPingPath(
      id,
      new Date(data.timestampCreated),
      data.type
    );
    const pathCompressed = path + "lz4";

    // Purge pings which are too big.
    let checkSize = async function(path) {
      const fileSize = (await OS.File.stat(path)).size;
      if (fileSize > PING_FILE_MAXIMUM_SIZE_BYTES) {
        Telemetry.getHistogramById(
          "TELEMETRY_DISCARDED_ARCHIVED_PINGS_SIZE_MB"
        ).add(Math.floor(fileSize / 1024 / 1024));
        Telemetry.getHistogramById(
          "TELEMETRY_PING_SIZE_EXCEEDED_ARCHIVED"
        ).add();
        await OS.File.remove(path, { ignoreAbsent: true });
        throw new Error(
          "loadArchivedPing - exceeded the maximum ping size: " + fileSize
        );
      }
    };

    let ping;
    try {
      // Try to load a compressed version of the archived ping first.
      this._log.trace(
        "loadArchivedPing - loading ping from: " + pathCompressed
      );
      await checkSize(pathCompressed);
      ping = await this.loadPingFile(pathCompressed, /* compressed*/ true);
    } catch (ex) {
      if (!ex.becauseNoSuchFile) {
        throw ex;
      }
      // If that fails, look for the uncompressed version.
      this._log.trace(
        "loadArchivedPing - compressed ping not found, loading: " + path
      );
      await checkSize(path);
      ping = await this.loadPingFile(path, /* compressed*/ false);
    }

    return ping;
  },

  /**
   * Saves session data to disk.
   */
  saveSessionData(sessionData) {
    return this._stateSaveSerializer.enqueueTask(() =>
      this._saveSessionData(sessionData)
    );
  },

  async _saveSessionData(sessionData) {
    let dataDir = OS.Path.join(OS.Constants.Path.profileDir, DATAREPORTING_DIR);
    await OS.File.makeDir(dataDir);

    let filePath = OS.Path.join(gDataReportingDir, SESSION_STATE_FILE_NAME);
    try {
      await CommonUtils.writeJSON(sessionData, filePath);
    } catch (e) {
      this._log.error(
        "_saveSessionData - Failed to write session data to " + filePath,
        e
      );
      Telemetry.getHistogramById("TELEMETRY_SESSIONDATA_FAILED_SAVE").add(1);
    }
  },

  /**
   * Loads session data from the session data file.
   * @return {Promise<Object>} A promise resolved with an object on success,
   *                           with null otherwise.
   */
  loadSessionData() {
    return this._stateSaveSerializer.enqueueTask(() => this._loadSessionData());
  },

  async _loadSessionData() {
    const dataFile = OS.Path.join(
      OS.Constants.Path.profileDir,
      DATAREPORTING_DIR,
      SESSION_STATE_FILE_NAME
    );
    let content;
    try {
      content = await OS.File.read(dataFile, { encoding: "utf-8" });
    } catch (ex) {
      this._log.info("_loadSessionData - can not load session data file", ex);
      Telemetry.getHistogramById("TELEMETRY_SESSIONDATA_FAILED_LOAD").add(1);
      return null;
    }

    let data;
    try {
      data = JSON.parse(content);
    } catch (ex) {
      this._log.error("_loadSessionData - failed to parse session data", ex);
      Telemetry.getHistogramById("TELEMETRY_SESSIONDATA_FAILED_PARSE").add(1);
      return null;
    }

    return data;
  },

  /**
   * Remove an archived ping from disk.
   *
   * @param {string} id The pings id.
   * @param {number} timestampCreated The pings creation timestamp.
   * @param {string} type The pings type.
   * @return {promise<object>} Promise that is resolved when the pings is removed.
   */
  async _removeArchivedPing(id, timestampCreated, type) {
    this._log.trace(
      "_removeArchivedPing - id: " +
        id +
        ", timestampCreated: " +
        timestampCreated +
        ", type: " +
        type
    );
    const path = getArchivedPingPath(id, new Date(timestampCreated), type);
    const pathCompressed = path + "lz4";

    this._log.trace("_removeArchivedPing - removing ping from: " + path);
    await OS.File.remove(path, { ignoreAbsent: true });
    await OS.File.remove(pathCompressed, { ignoreAbsent: true });
    // Remove the ping from the cache.
    this._archivedPings.delete(id);
  },

  /**
   * Clean the pings archive by removing old pings.
   *
   * @return {Promise} Resolved when the cleanup task completes.
   */
  runCleanPingArchiveTask() {
    // If there's an archive cleaning task already running, return it.
    if (this._cleanArchiveTask) {
      return this._cleanArchiveTask;
    }

    // Make sure to clear |_cleanArchiveTask| once done.
    let clear = () => (this._cleanArchiveTask = null);
    // Since there's no archive cleaning task running, start it.
    this._cleanArchiveTask = this._cleanArchive().then(clear, clear);
    return this._cleanArchiveTask;
  },

  /**
   * Removes pings which are too old from the pings archive.
   * @return {Promise} Resolved when the ping age check is complete.
   */
  async _purgeOldPings() {
    this._log.trace("_purgeOldPings");

    const nowDate = Policy.now();
    const startTimeStamp = nowDate.getTime();
    let dirIterator = new OS.File.DirectoryIterator(gPingsArchivePath);
    let subdirs = (await dirIterator.nextBatch()).filter(e => e.isDir);
    dirIterator.close();

    // Keep track of the newest removed month to update the cache, if needed.
    let newestRemovedMonthTimestamp = null;
    let evictedDirsCount = 0;
    let maxDirAgeInMonths = 0;

    // Walk through the monthly subdirs of the form <YYYY-MM>/
    for (let dir of subdirs) {
      if (this._shutdown) {
        this._log.trace(
          "_purgeOldPings - Terminating the clean up task due to shutdown"
        );
        return;
      }

      if (!isValidArchiveDir(dir.name)) {
        this._log.warn(
          "_purgeOldPings - skipping invalidly named subdirectory " + dir.path
        );
        continue;
      }

      const archiveDate = getDateFromArchiveDir(dir.name);
      if (!archiveDate) {
        this._log.warn(
          "_purgeOldPings - skipping invalid subdirectory date " + dir.path
        );
        continue;
      }

      // If this archive directory is older than allowed, remove it.
      if (
        startTimeStamp - archiveDate.getTime() >
        MAX_ARCHIVED_PINGS_RETENTION_MS
      ) {
        try {
          await OS.File.removeDir(dir.path);
          evictedDirsCount++;

          // Update the newest removed month.
          newestRemovedMonthTimestamp = Math.max(
            archiveDate,
            newestRemovedMonthTimestamp
          );
        } catch (ex) {
          this._log.error("_purgeOldPings - Unable to remove " + dir.path, ex);
        }
      } else {
        // We're not removing this directory, so record the age for the oldest directory.
        const dirAgeInMonths = Utils.getElapsedTimeInMonths(
          archiveDate,
          nowDate
        );
        maxDirAgeInMonths = Math.max(dirAgeInMonths, maxDirAgeInMonths);
      }
    }

    // Trigger scanning of the archived pings.
    await this.loadArchivedPingList();

    // Refresh the cache: we could still skip this, but it's cheap enough to keep it
    // to avoid introducing task dependencies.
    if (newestRemovedMonthTimestamp) {
      // Scan the archive cache for pings older than the newest directory pruned above.
      for (let [id, info] of this._archivedPings) {
        const timestampCreated = new Date(info.timestampCreated);
        if (timestampCreated.getTime() > newestRemovedMonthTimestamp) {
          continue;
        }
        // Remove outdated pings from the cache.
        this._archivedPings.delete(id);
      }
    }

    const endTimeStamp = Policy.now().getTime();

    // Save the time it takes to evict old directories and the eviction count.
    Telemetry.getHistogramById("TELEMETRY_ARCHIVE_EVICTED_OLD_DIRS").add(
      evictedDirsCount
    );
    Telemetry.getHistogramById("TELEMETRY_ARCHIVE_EVICTING_DIRS_MS").add(
      Math.ceil(endTimeStamp - startTimeStamp)
    );
    Telemetry.getHistogramById("TELEMETRY_ARCHIVE_OLDEST_DIRECTORY_AGE").add(
      maxDirAgeInMonths
    );
  },

  /**
   * Enforce a disk quota for the pings archive.
   * @return {Promise} Resolved when the quota check is complete.
   */
  async _enforceArchiveQuota() {
    this._log.trace("_enforceArchiveQuota");
    let startTimeStamp = Policy.now().getTime();

    // Build an ordered list, from newer to older, of archived pings.
    let pingList = Array.from(this._archivedPings, p => ({
      id: p[0],
      timestampCreated: p[1].timestampCreated,
      type: p[1].type,
    }));

    pingList.sort((a, b) => b.timestampCreated - a.timestampCreated);

    // If our archive is too big, we should reduce it to reach 90% of the quota.
    const SAFE_QUOTA = Policy.getArchiveQuota() * 0.9;
    // The index of the last ping to keep. Pings older than this one will be deleted if
    // the archive exceeds the quota.
    let lastPingIndexToKeep = null;
    let archiveSizeInBytes = 0;

    // Find the disk size of the archive.
    for (let i = 0; i < pingList.length; i++) {
      if (this._shutdown) {
        this._log.trace(
          "_enforceArchiveQuota - Terminating the clean up task due to shutdown"
        );
        return;
      }

      let ping = pingList[i];

      // Get the size for this ping.
      const fileSize = await getArchivedPingSize(
        ping.id,
        new Date(ping.timestampCreated),
        ping.type
      );
      if (!fileSize) {
        this._log.warn(
          "_enforceArchiveQuota - Unable to find the size of ping " + ping.id
        );
        continue;
      }

      // Enforce a maximum file size limit on archived pings.
      if (fileSize > PING_FILE_MAXIMUM_SIZE_BYTES) {
        this._log.error(
          "_enforceArchiveQuota - removing file exceeding size limit, size: " +
            fileSize
        );
        // We just remove the ping from the disk, we don't bother removing it from pingList
        // since it won't contribute to the quota.
        await this._removeArchivedPing(
          ping.id,
          ping.timestampCreated,
          ping.type
        ).catch(e =>
          this._log.error(
            "_enforceArchiveQuota - failed to remove archived ping" + ping.id
          )
        );
        Telemetry.getHistogramById(
          "TELEMETRY_DISCARDED_ARCHIVED_PINGS_SIZE_MB"
        ).add(Math.floor(fileSize / 1024 / 1024));
        Telemetry.getHistogramById(
          "TELEMETRY_PING_SIZE_EXCEEDED_ARCHIVED"
        ).add();
        continue;
      }

      archiveSizeInBytes += fileSize;

      if (archiveSizeInBytes < SAFE_QUOTA) {
        // We save the index of the last ping which is ok to keep in order to speed up ping
        // pruning.
        lastPingIndexToKeep = i;
      } else if (archiveSizeInBytes > Policy.getArchiveQuota()) {
        // Ouch, our ping archive is too big. Bail out and start pruning!
        break;
      }
    }

    // Save the time it takes to check if the archive is over-quota.
    Telemetry.getHistogramById("TELEMETRY_ARCHIVE_CHECKING_OVER_QUOTA_MS").add(
      Math.round(Policy.now().getTime() - startTimeStamp)
    );

    let submitProbes = (sizeInMB, evictedPings, elapsedMs) => {
      Telemetry.getHistogramById("TELEMETRY_ARCHIVE_SIZE_MB").add(sizeInMB);
      Telemetry.getHistogramById("TELEMETRY_ARCHIVE_EVICTED_OVER_QUOTA").add(
        evictedPings
      );
      Telemetry.getHistogramById(
        "TELEMETRY_ARCHIVE_EVICTING_OVER_QUOTA_MS"
      ).add(elapsedMs);
    };

    // Check if we're using too much space. If not, submit the archive size and bail out.
    if (archiveSizeInBytes < Policy.getArchiveQuota()) {
      submitProbes(Math.round(archiveSizeInBytes / 1024 / 1024), 0, 0);
      return;
    }

    this._log.info(
      "_enforceArchiveQuota - archive size: " +
        archiveSizeInBytes +
        "bytes" +
        ", safety quota: " +
        SAFE_QUOTA +
        "bytes"
    );

    startTimeStamp = Policy.now().getTime();
    let pingsToPurge = pingList.slice(lastPingIndexToKeep + 1);

    // Remove all the pings older than the last one which we are safe to keep.
    for (let ping of pingsToPurge) {
      if (this._shutdown) {
        this._log.trace(
          "_enforceArchiveQuota - Terminating the clean up task due to shutdown"
        );
        return;
      }

      // This list is guaranteed to be in order, so remove the pings at its
      // beginning (oldest).
      await this._removeArchivedPing(ping.id, ping.timestampCreated, ping.type);
    }

    const endTimeStamp = Policy.now().getTime();
    submitProbes(
      ARCHIVE_SIZE_PROBE_SPECIAL_VALUE,
      pingsToPurge.length,
      Math.ceil(endTimeStamp - startTimeStamp)
    );
  },

  async _cleanArchive() {
    this._log.trace("cleanArchiveTask");

    if (!(await OS.File.exists(gPingsArchivePath))) {
      return;
    }

    // Remove pings older than allowed.
    try {
      await this._purgeOldPings();
    } catch (ex) {
      this._log.error(
        "_cleanArchive - There was an error removing old directories",
        ex
      );
    }

    // Make sure we respect the archive disk quota.
    await this._enforceArchiveQuota();
  },

  /**
   * Run the task to enforce the pending pings quota.
   *
   * @return {Promise} Resolved when the cleanup task completes.
   */
  async runEnforcePendingPingsQuotaTask() {
    // If there's a cleaning task already running, return it.
    if (this._enforcePendingPingsQuotaTask) {
      return this._enforcePendingPingsQuotaTask;
    }

    // Since there's no quota enforcing task running, start it.
    try {
      this._enforcePendingPingsQuotaTask = this._enforcePendingPingsQuota();
      await this._enforcePendingPingsQuotaTask;
    } finally {
      this._enforcePendingPingsQuotaTask = null;
    }
    return undefined;
  },

  /**
   * Enforce a disk quota for the pending pings.
   * @return {Promise} Resolved when the quota check is complete.
   */
  async _enforcePendingPingsQuota() {
    this._log.trace("_enforcePendingPingsQuota");
    let startTimeStamp = Policy.now().getTime();

    // Build an ordered list, from newer to older, of pending pings.
    let pingList = Array.from(this._pendingPings, p => ({
      id: p[0],
      lastModificationDate: p[1].lastModificationDate,
    }));

    pingList.sort((a, b) => b.lastModificationDate - a.lastModificationDate);

    // If our pending pings directory is too big, we should reduce it to reach 90% of the quota.
    const SAFE_QUOTA = Policy.getPendingPingsQuota() * 0.9;
    // The index of the last ping to keep. Pings older than this one will be deleted if
    // the pending pings directory size exceeds the quota.
    let lastPingIndexToKeep = null;
    let pendingPingsSizeInBytes = 0;

    // Find the disk size of the pending pings directory.
    for (let i = 0; i < pingList.length; i++) {
      if (this._shutdown) {
        this._log.trace(
          "_enforcePendingPingsQuota - Terminating the clean up task due to shutdown"
        );
        return;
      }

      let ping = pingList[i];

      // Get the size for this ping.
      const fileSize = await getPendingPingSize(ping.id);
      if (!fileSize) {
        this._log.warn(
          "_enforcePendingPingsQuota - Unable to find the size of ping " +
            ping.id
        );
        continue;
      }

      pendingPingsSizeInBytes += fileSize;
      if (pendingPingsSizeInBytes < SAFE_QUOTA) {
        // We save the index of the last ping which is ok to keep in order to speed up ping
        // pruning.
        lastPingIndexToKeep = i;
      } else if (pendingPingsSizeInBytes > Policy.getPendingPingsQuota()) {
        // Ouch, our pending pings directory size is too big. Bail out and start pruning!
        break;
      }
    }

    // Save the time it takes to check if the pending pings are over-quota.
    Telemetry.getHistogramById("TELEMETRY_PENDING_CHECKING_OVER_QUOTA_MS").add(
      Math.round(Policy.now().getTime() - startTimeStamp)
    );

    let recordHistograms = (sizeInMB, evictedPings, elapsedMs) => {
      Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_SIZE_MB").add(
        sizeInMB
      );
      Telemetry.getHistogramById(
        "TELEMETRY_PENDING_PINGS_EVICTED_OVER_QUOTA"
      ).add(evictedPings);
      Telemetry.getHistogramById(
        "TELEMETRY_PENDING_EVICTING_OVER_QUOTA_MS"
      ).add(elapsedMs);
    };

    // Check if we're using too much space. If not, bail out.
    if (pendingPingsSizeInBytes < Policy.getPendingPingsQuota()) {
      recordHistograms(Math.round(pendingPingsSizeInBytes / 1024 / 1024), 0, 0);
      return;
    }

    this._log.info(
      "_enforcePendingPingsQuota - size: " +
        pendingPingsSizeInBytes +
        "bytes" +
        ", safety quota: " +
        SAFE_QUOTA +
        "bytes"
    );

    startTimeStamp = Policy.now().getTime();
    let pingsToPurge = pingList.slice(lastPingIndexToKeep + 1);

    // Remove all the pings older than the last one which we are safe to keep.
    for (let ping of pingsToPurge) {
      if (this._shutdown) {
        this._log.trace(
          "_enforcePendingPingsQuota - Terminating the clean up task due to shutdown"
        );
        return;
      }

      // This list is guaranteed to be in order, so remove the pings at its
      // beginning (oldest).
      await this.removePendingPing(ping.id);
    }

    const endTimeStamp = Policy.now().getTime();
    // We don't know the size of the pending pings directory if we are above the quota,
    // since we stop scanning once we reach the quota. We use a special value to show
    // this condition.
    recordHistograms(
      PENDING_PINGS_SIZE_PROBE_SPECIAL_VALUE,
      pingsToPurge.length,
      Math.ceil(endTimeStamp - startTimeStamp)
    );
  },

  /**
   * Reset the storage state in tests.
   */
  reset() {
    this._shutdown = false;
    this._scannedArchiveDirectory = false;
    this._archivedPings = new Map();
    this._scannedPendingDirectory = false;
    this._pendingPings = new Map();
  },

  /**
   * Get a list of info on the archived pings.
   * This will scan the archive directory and grab basic data about the existing
   * pings out of their filename.
   *
   * @return {promise<sequence<object>>}
   */
  async loadArchivedPingList() {
    // If there's an archive loading task already running, return it.
    if (this._scanArchiveTask) {
      return this._scanArchiveTask;
    }

    await waitForAll(this._activelyArchiving);

    if (this._scannedArchiveDirectory) {
      this._log.trace(
        "loadArchivedPingList - Archive already scanned, hitting cache."
      );
      return this._archivedPings;
    }

    // Since there's no archive loading task running, start it.
    let result;
    try {
      this._scanArchiveTask = this._scanArchive();
      result = await this._scanArchiveTask;
    } finally {
      this._scanArchiveTask = null;
    }
    return result;
  },

  async _scanArchive() {
    this._log.trace("_scanArchive");

    let submitProbes = (pingCount, dirCount) => {
      Telemetry.getHistogramById("TELEMETRY_ARCHIVE_SCAN_PING_COUNT").add(
        pingCount
      );
      Telemetry.getHistogramById("TELEMETRY_ARCHIVE_DIRECTORIES_COUNT").add(
        dirCount
      );
    };

    if (!(await OS.File.exists(gPingsArchivePath))) {
      submitProbes(0, 0);
      return new Map();
    }

    let dirIterator = new OS.File.DirectoryIterator(gPingsArchivePath);
    let subdirs = (await dirIterator.nextBatch())
      .filter(e => e.isDir)
      .filter(e => isValidArchiveDir(e.name));
    dirIterator.close();

    // Walk through the monthly subdirs of the form <YYYY-MM>/
    for (let dir of subdirs) {
      this._log.trace("_scanArchive - checking in subdir: " + dir.path);
      let pingIterator = new OS.File.DirectoryIterator(dir.path);
      let pings = (await pingIterator.nextBatch()).filter(e => !e.isDir);
      pingIterator.close();

      // Now process any ping files of the form "<timestamp>.<uuid>.<type>.[json|jsonlz4]".
      for (let p of pings) {
        // data may be null if the filename doesn't match the above format.
        let data = this._getArchivedPingDataFromFileName(p.name);
        if (!data) {
          continue;
        }

        // In case of conflicts, overwrite only with newer pings.
        if (this._archivedPings.has(data.id)) {
          const overwrite =
            data.timestamp > this._archivedPings.get(data.id).timestampCreated;
          this._log.warn(
            "_scanArchive - have seen this id before: " +
              data.id +
              ", overwrite: " +
              overwrite
          );
          if (!overwrite) {
            continue;
          }

          await this._removeArchivedPing(
            data.id,
            data.timestampCreated,
            data.type
          ).catch(e =>
            this._log.warn("_scanArchive - failed to remove ping", e)
          );
        }

        this._archivedPings.set(data.id, {
          timestampCreated: data.timestamp,
          type: internString(data.type),
        });
      }
    }

    // Mark the archive as scanned, so we no longer hit the disk.
    this._scannedArchiveDirectory = true;
    // Update the ping and directories count histograms.
    submitProbes(this._archivedPings.size, subdirs.length);
    return this._archivedPings;
  },

  /**
   * Save a single ping to a file.
   *
   * @param {object} ping The content of the ping to save.
   * @param {string} file The destination file.
   * @param {bool} overwrite If |true|, the file will be overwritten if it exists,
   * if |false| the file will not be overwritten and no error will be reported if
   * the file exists.
   * @param {bool} [compress=false] If |true|, the file will use lz4 compression. Otherwise no
   * compression will be used.
   * @returns {promise}
   */
  async savePingToFile(ping, filePath, overwrite, compress = false) {
    try {
      this._log.trace("savePingToFile - path: " + filePath);
      let pingString = JSON.stringify(ping);
      let options = { tmpPath: filePath + ".tmp", noOverwrite: !overwrite };
      if (compress) {
        options.compression = "lz4";
      }
      await OS.File.writeAtomic(filePath, pingString, options);
    } catch (e) {
      if (!e.becauseExists) {
        throw e;
      }
    }
  },

  /**
   * Save a ping to its file.
   *
   * @param {object} ping The content of the ping to save.
   * @param {bool} overwrite If |true|, the file will be overwritten
   * if it exists.
   * @returns {promise}
   */
  async savePing(ping, overwrite) {
    await getPingDirectory();
    let file = pingFilePath(ping);
    await this.savePingToFile(ping, file, overwrite);
    return file;
  },

  /**
   * Add a ping to the saved pings directory so that it gets saved
   * and sent along with other pings.
   * Note: that the original ping file will not be modified.
   *
   * @param {Object} ping The ping object.
   * @return {Promise} A promise resolved when the ping is saved to the pings directory.
   */
  addPendingPing(ping) {
    return this.savePendingPing(ping);
  },

  /**
   * Remove the file for a ping
   *
   * @param {object} ping The ping.
   * @returns {promise}
   */
  cleanupPingFile(ping) {
    return OS.File.remove(pingFilePath(ping));
  },

  savePendingPing(ping) {
    let p = this.savePing(ping, true).then(path => {
      this._pendingPings.set(ping.id, {
        path,
        lastModificationDate: Policy.now().getTime(),
      });
      this._log.trace("savePendingPing - saved ping with id " + ping.id);
    });
    this._trackPendingPingSaveTask(p);
    return p;
  },

  async loadPendingPing(id) {
    this._log.trace("loadPendingPing - id: " + id);
    let info = this._pendingPings.get(id);
    if (!info) {
      this._log.trace("loadPendingPing - unknown id " + id);
      throw new Error(
        "TelemetryStorage.loadPendingPing - no ping with id " + id
      );
    }

    // Try to get the dimension of the ping. If that fails, update the histograms.
    let fileSize = 0;
    try {
      fileSize = (await OS.File.stat(info.path)).size;
    } catch (e) {
      if (!(e instanceof OS.File.Error) || !e.becauseNoSuchFile) {
        throw e;
      }
      // Fall through and let |loadPingFile| report the error.
    }

    // Purge pings which are too big.
    if (fileSize > PING_FILE_MAXIMUM_SIZE_BYTES) {
      await this.removePendingPing(id);
      Telemetry.getHistogramById(
        "TELEMETRY_DISCARDED_PENDING_PINGS_SIZE_MB"
      ).add(Math.floor(fileSize / 1024 / 1024));
      Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_PENDING").add();

      // Currently we don't have the ping type available without loading the ping from disk.
      // Bug 1384903 will fix that.
      TelemetryHealthPing.recordDiscardedPing("<unknown>");
      throw new Error(
        "loadPendingPing - exceeded the maximum ping size: " + fileSize
      );
    }

    // Try to load the ping file. Update the related histograms on failure.
    let ping;
    try {
      ping = await this.loadPingFile(info.path, false);
    } catch (e) {
      // If we failed to load the ping, check what happened and update the histogram.
      if (e instanceof PingReadError) {
        Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_READ").add();
      } else if (e instanceof PingParseError) {
        Telemetry.getHistogramById(
          "TELEMETRY_PENDING_LOAD_FAILURE_PARSE"
        ).add();
      }

      // Remove the ping from the cache, so we don't try to load it again.
      this._pendingPings.delete(id);
      // Then propagate the rejection.
      throw e;
    }

    return ping;
  },

  removePendingPing(id) {
    let info = this._pendingPings.get(id);
    if (!info) {
      this._log.trace("removePendingPing - unknown id " + id);
      return Promise.resolve();
    }

    this._log.trace(
      "removePendingPing - deleting ping with id: " +
        id +
        ", path: " +
        info.path
    );
    this._pendingPings.delete(id);
    return OS.File.remove(info.path).catch(ex =>
      this._log.error("removePendingPing - failed to remove ping", ex)
    );
  },

  /**
   * Track any pending ping save tasks through the promise passed here.
   * This is needed to block on any outstanding ping save activity.
   *
   * @param {Object<Promise>} The save promise to track.
   */
  _trackPendingPingSaveTask(promise) {
    let clear = () => this._activePendingPingSaves.delete(promise);
    promise.then(clear, clear);
    this._activePendingPingSaves.add(promise);
  },

  /**
   * Return a promise that allows to wait on pending pings being saved.
   * @return {Object<Promise>} A promise resolved when all the pending pings save promises
   *         are resolved.
   */
  promisePendingPingSaves() {
    // Make sure to wait for all the promises, even if they reject. We don't need to log
    // the failures here, as they are already logged elsewhere.
    return waitForAll(this._activePendingPingSaves);
  },

  /**
   * Run the task to remove all the pending pings
   *
   * @return {Promise} Resolved when the pings are removed.
   */
  async runRemovePendingPingsTask() {
    // If we already have a pending pings removal task active, return that.
    if (this._removePendingPingsTask) {
      return this._removePendingPingsTask;
    }

    // Start the task to remove all pending pings. Also make sure to clear the task once done.
    try {
      this._removePendingPingsTask = this.removePendingPings();
      await this._removePendingPingsTask;
    } finally {
      this._removePendingPingsTask = null;
    }
    return undefined;
  },

  async removePendingPings() {
    this._log.trace("removePendingPings - removing all pending pings");

    // Wait on pending pings still being saved, so so we don't miss removing them.
    await this.promisePendingPingSaves();

    // Individually remove existing pings, so we don't interfere with operations expecting
    // the pending pings directory to exist.
    const directory = TelemetryStorage.pingDirectoryPath;
    let iter = new OS.File.DirectoryIterator(directory);

    try {
      if (!(await iter.exists())) {
        this._log.trace(
          "removePendingPings - the pending pings directory doesn't exist"
        );
        return;
      }

      let files = (await iter.nextBatch()).filter(e => !e.isDir);
      for (let file of files) {
        try {
          await OS.File.remove(file.path);
        } catch (ex) {
          this._log.error(
            "removePendingPings - failed to remove file " + file.path,
            ex
          );
          continue;
        }
      }
    } finally {
      await iter.close();
    }
  },

  /**
   * Iterate through all pings in the userApplicationDataDir under the "Pending Pings" sub-directory
   * and yield each file.
   */
  async *_iterateAppDataPings() {
    this._log.trace("_iterateAppDataPings");

    // The test suites might not create and define the "UAppData" directory.
    // We account for that here instead of manually going through each test using
    // telemetry to manually create the directory and define the constant.
    if (!OS.Constants.Path.userApplicationDataDir) {
      this._log.trace(
        "_iterateAppDataPings - userApplicationDataDir is not defined. Is this a test?"
      );
      return;
    }

    const appDataPendingPings = OS.Path.join(
      OS.Constants.Path.userApplicationDataDir,
      "Pending Pings"
    );

    // Iterate through the pending ping files.
    let iter = new OS.File.DirectoryIterator(appDataPendingPings);
    try {
      // Check if appDataPendingPings exists and bail out if it doesn't.
      if (!(await iter.exists())) {
        this._log.trace(
          "_iterateAppDataPings - the AppData pending pings directory doesn't exist."
        );
        return;
      }

      let files = (await iter.nextBatch()).filter(e => !e.isDir);
      for (let file of files) {
        yield file;
      }
    } finally {
      await iter.close();
    }
  },

  /**
   * Remove all pings that are stored in the userApplicationDataDir
   * under the "Pending Pings" sub-directory.
   */
  async removeAppDataPings() {
    this._log.trace("removeAppDataPings");

    for await (const file of this._iterateAppDataPings()) {
      try {
        await OS.File.remove(file.path);
      } catch (ex) {
        this._log.error(
          "removeAppDataPings - failed to remove file " + file.path,
          ex
        );
      }
    }
  },

  /**
   * Migrate pings that are stored in the userApplicationDataDir
   * under the "Pending Pings" sub-directory.
   */
  async _migrateAppDataPings() {
    this._log.trace("_migrateAppDataPings");

    for await (const file of this._iterateAppDataPings()) {
      try {
        // Load the ping data from the original file.
        const pingData = await this.loadPingFile(file.path);

        // Save it among the pending pings in the user profile, overwrite on
        // ping id collision.
        await TelemetryStorage.savePing(pingData, true);
      } catch (ex) {
        this._log.error(
          "_migrateAppDataPings - failed to load or migrate file. Removing. " +
            file.path,
          ex
        );
      }

      try {
        // Finally remove the file.
        await OS.File.remove(file.path);
      } catch (ex) {
        this._log.error(
          "_migrateAppDataPings - failed to remove file " + file.path,
          ex
        );
      }
    }
  },

  loadPendingPingList() {
    // If we already have a pending scanning task active, return that.
    if (this._scanPendingPingsTask) {
      return this._scanPendingPingsTask;
    }

    if (this._scannedPendingDirectory) {
      this._log.trace(
        "loadPendingPingList - Pending already scanned, hitting cache."
      );
      return Promise.resolve(this._buildPingList());
    }

    // Since there's no pending pings scan task running, start it.
    // Also make sure to clear the task once done.
    this._scanPendingPingsTask = this._scanPendingPings().then(
      pings => {
        this._scanPendingPingsTask = null;
        return pings;
      },
      ex => {
        this._scanPendingPingsTask = null;
        throw ex;
      }
    );
    return this._scanPendingPingsTask;
  },

  getPendingPingList() {
    return this._buildPingList();
  },

  async _scanPendingPings() {
    this._log.trace("_scanPendingPings");

    // Before pruning the pending pings, migrate over the ones from the user
    // application data directory (mainly crash pings that failed to be sent).
    await this._migrateAppDataPings();

    let directory = TelemetryStorage.pingDirectoryPath;
    let iter = new OS.File.DirectoryIterator(directory);
    let exists = await iter.exists();

    try {
      if (!exists) {
        return [];
      }

      let files = (await iter.nextBatch()).filter(e => !e.isDir);

      for (let file of files) {
        if (this._shutdown) {
          return [];
        }

        let info;
        try {
          info = await OS.File.stat(file.path);
        } catch (ex) {
          this._log.error(
            "_scanPendingPings - failed to stat file " + file.path,
            ex
          );
          continue;
        }

        // Enforce a maximum file size limit on pending pings.
        if (info.size > PING_FILE_MAXIMUM_SIZE_BYTES) {
          this._log.error(
            "_scanPendingPings - removing file exceeding size limit " +
              file.path
          );
          try {
            await OS.File.remove(file.path);
          } catch (ex) {
            this._log.error(
              "_scanPendingPings - failed to remove file " + file.path,
              ex
            );
          } finally {
            Telemetry.getHistogramById(
              "TELEMETRY_DISCARDED_PENDING_PINGS_SIZE_MB"
            ).add(Math.floor(info.size / 1024 / 1024));
            Telemetry.getHistogramById(
              "TELEMETRY_PING_SIZE_EXCEEDED_PENDING"
            ).add();

            // Currently we don't have the ping type available without loading the ping from disk.
            // Bug 1384903 will fix that.
            TelemetryHealthPing.recordDiscardedPing("<unknown>");
          }
          continue;
        }

        let id = OS.Path.basename(file.path);
        if (!UUID_REGEX.test(id)) {
          this._log.trace("_scanPendingPings - filename is not a UUID: " + id);
          id = Utils.generateUUID();
        }

        this._pendingPings.set(id, {
          path: file.path,
          lastModificationDate: info.lastModificationDate.getTime(),
        });
      }
    } finally {
      await iter.close();
    }

    this._scannedPendingDirectory = true;
    return this._buildPingList();
  },

  _buildPingList() {
    const list = Array.from(this._pendingPings, p => ({
      id: p[0],
      lastModificationDate: p[1].lastModificationDate,
    }));

    list.sort((a, b) => b.lastModificationDate - a.lastModificationDate);
    return list;
  },

  /**
   * Loads a ping file.
   * @param {String} aFilePath The path of the ping file.
   * @param {Boolean} [aCompressed=false] If |true|, expects the file to be compressed using lz4.
   * @return {Promise<Object>} A promise resolved with the ping content or rejected if the
   *                           ping contains invalid data.
   * @throws {PingReadError} There was an error while reading the ping file from the disk.
   * @throws {PingParseError} There was an error while parsing the JSON content of the ping file.
   */
  async loadPingFile(aFilePath, aCompressed = false) {
    let options = {};
    if (aCompressed) {
      options.compression = "lz4";
    }

    let array;
    try {
      array = await OS.File.read(aFilePath, options);
    } catch (e) {
      this._log.trace("loadPingfile - unreadable ping " + aFilePath, e);
      throw new PingReadError(e.message, e.becauseNoSuchFile);
    }

    let decoder = new TextDecoder();
    let string = decoder.decode(array);
    let ping;
    try {
      ping = JSON.parse(string);
    } catch (e) {
      this._log.trace("loadPingfile - unparseable ping " + aFilePath, e);
      await OS.File.remove(aFilePath).catch(ex => {
        this._log.error(
          "loadPingFile - failed removing unparseable ping file",
          ex
        );
      });
      throw new PingParseError(e.message);
    }

    return ping;
  },

  /**
   * Archived pings are saved with file names of the form:
   * "<timestamp>.<uuid>.<type>.[json|jsonlz4]"
   * This helper extracts that data from a given filename.
   *
   * @param fileName {String} The filename.
   * @return {Object} Null if the filename didn't match the expected form.
   *                  Otherwise an object with the extracted data in the form:
   *                  { timestamp: <number>,
   *                    id: <string>,
   *                    type: <string> }
   */
  _getArchivedPingDataFromFileName(fileName) {
    // Extract the parts.
    let parts = fileName.split(".");
    if (parts.length != 4) {
      this._log.trace("_getArchivedPingDataFromFileName - should have 4 parts");
      return null;
    }

    let [timestamp, uuid, type, extension] = parts;
    if (extension != "json" && extension != "jsonlz4") {
      this._log.trace(
        "_getArchivedPingDataFromFileName - should have 'json' or 'jsonlz4' extension"
      );
      return null;
    }

    // Check for a valid timestamp.
    timestamp = parseInt(timestamp);
    if (Number.isNaN(timestamp)) {
      this._log.trace(
        "_getArchivedPingDataFromFileName - should have a valid timestamp"
      );
      return null;
    }

    // Check for a valid UUID.
    if (!UUID_REGEX.test(uuid)) {
      this._log.trace(
        "_getArchivedPingDataFromFileName - should have a valid id"
      );
      return null;
    }

    // Check for a valid type string.
    const typeRegex = /^[a-z0-9][a-z0-9-]+[a-z0-9]$/i;
    if (!typeRegex.test(type)) {
      this._log.trace(
        "_getArchivedPingDataFromFileName - should have a valid type"
      );
      return null;
    }

    return {
      timestamp,
      id: uuid,
      type,
    };
  },

  async saveAbortedSessionPing(ping) {
    this._log.trace(
      "saveAbortedSessionPing - ping path: " + gAbortedSessionFilePath
    );
    await OS.File.makeDir(gDataReportingDir, { ignoreExisting: true });

    return this._abortedSessionSerializer.enqueueTask(() =>
      this.savePingToFile(ping, gAbortedSessionFilePath, true)
    );
  },

  async loadAbortedSessionPing() {
    let ping = null;
    try {
      ping = await this.loadPingFile(gAbortedSessionFilePath);
    } catch (ex) {
      if (ex.becauseNoSuchFile) {
        this._log.trace("loadAbortedSessionPing - no such file");
      } else {
        this._log.error("loadAbortedSessionPing - error loading ping", ex);
      }
    }
    return ping;
  },

  removeAbortedSessionPing() {
    return this._abortedSessionSerializer.enqueueTask(async () => {
      try {
        await OS.File.remove(gAbortedSessionFilePath, { ignoreAbsent: false });
        this._log.trace("removeAbortedSessionPing - success");
      } catch (ex) {
        if (ex.becauseNoSuchFile) {
          this._log.trace("removeAbortedSessionPing - no such file");
        } else {
          this._log.error("removeAbortedSessionPing - error removing ping", ex);
        }
      }
    });
  },

  async saveUninstallPing(ping) {
    if (AppConstants.platform != "win") {
      return;
    }

    // Remove any old pings from this install first.
    await this.removeUninstallPings();

    let { directory: pingFile, file } = Policy.getUninstallPingPath(ping.id);
    pingFile.append(file);

    await this.savePingToFile(ping, pingFile.path, /* overwrite */ true);
  },

  async removeUninstallPings() {
    if (AppConstants.platform != "win") {
      return;
    }

    const { directory, file } = Policy.getUninstallPingPath("*");

    const iteratorOptions = { winPattern: file };
    const iterator = new OS.File.DirectoryIterator(
      directory.path,
      iteratorOptions
    );

    await iterator.forEach(async entry => {
      this._log.trace("removeUninstallPings - removing", entry.path);
      try {
        await OS.File.remove(entry.path);
        this._log.trace("removeUninstallPings - success");
      } catch (ex) {
        if (ex.becauseNoSuchFile) {
          this._log.trace("removeUninstallPings - no such file");
        } else {
          this._log.error("removeUninstallPings - error removing ping", ex);
        }
      }
    });
    iterator.close();
  },

  /**
   * Remove FHR database files. This is temporary and will be dropped in
   * the future.
   * @return {Promise} Resolved when the database files are deleted.
   */
  async removeFHRDatabase() {
    this._log.trace("removeFHRDatabase");

    // Let's try to remove the FHR DB with the default filename first.
    const FHR_DB_DEFAULT_FILENAME = "healthreport.sqlite";

    // Even if it's uncommon, there may be 2 additional files: - a "write ahead log"
    // (-wal) file and a "shared memory file" (-shm). We need to remove them as well.
    let FILES_TO_REMOVE = [
      OS.Path.join(OS.Constants.Path.profileDir, FHR_DB_DEFAULT_FILENAME),
      OS.Path.join(
        OS.Constants.Path.profileDir,
        FHR_DB_DEFAULT_FILENAME + "-wal"
      ),
      OS.Path.join(
        OS.Constants.Path.profileDir,
        FHR_DB_DEFAULT_FILENAME + "-shm"
      ),
    ];

    // FHR could have used either the default DB file name or a custom one
    // through this preference.
    const FHR_DB_CUSTOM_FILENAME = Preferences.get(
      "datareporting.healthreport.dbName",
      undefined
    );
    if (FHR_DB_CUSTOM_FILENAME) {
      FILES_TO_REMOVE.push(
        OS.Path.join(OS.Constants.Path.profileDir, FHR_DB_CUSTOM_FILENAME),
        OS.Path.join(
          OS.Constants.Path.profileDir,
          FHR_DB_CUSTOM_FILENAME + "-wal"
        ),
        OS.Path.join(
          OS.Constants.Path.profileDir,
          FHR_DB_CUSTOM_FILENAME + "-shm"
        )
      );
    }

    for (let f of FILES_TO_REMOVE) {
      await OS.File.remove(f, { ignoreAbsent: true }).catch(e =>
        this._log.error("removeFHRDatabase - failed to remove " + f, e)
      );
    }
  },
};

// Utility functions

function pingFilePath(ping) {
  // Support legacy ping formats, who don't have an "id" field, but a "slug" field.
  let pingIdentifier = ping.slug ? ping.slug : ping.id;
  return OS.Path.join(TelemetryStorage.pingDirectoryPath, pingIdentifier);
}

function getPingDirectory() {
  return (async function() {
    let directory = TelemetryStorage.pingDirectoryPath;

    if (!(await OS.File.exists(directory))) {
      await OS.File.makeDir(directory, { unixMode: OS.Constants.S_IRWXU });
    }

    return directory;
  })();
}

/**
 * Build the path to the archived ping.
 * @param {String} aPingId The ping id.
 * @param {Object} aDate The ping creation date.
 * @param {String} aType The ping type.
 * @return {String} The full path to the archived ping.
 */
function getArchivedPingPath(aPingId, aDate, aType) {
  // Get the ping creation date and generate the archive directory to hold it. Note
  // that getMonth returns a 0-based month, so we need to add an offset.
  let month = String(aDate.getMonth() + 1);
  let archivedPingDir = OS.Path.join(
    gPingsArchivePath,
    aDate.getFullYear() + "-" + month.padStart(2, "0")
  );
  // Generate the archived ping file path as YYYY-MM/<TIMESTAMP>.UUID.type.json
  let fileName = [aDate.getTime(), aPingId, aType, "json"].join(".");
  return OS.Path.join(archivedPingDir, fileName);
}

/**
 * Get the size of the ping file on the disk.
 * @return {Integer} The file size, in bytes, of the ping file or 0 on errors.
 */
var getArchivedPingSize = async function(aPingId, aDate, aType) {
  const path = getArchivedPingPath(aPingId, aDate, aType);
  let filePaths = [path + "lz4", path];

  for (let path of filePaths) {
    try {
      return (await OS.File.stat(path)).size;
    } catch (e) {}
  }

  // That's odd, this ping doesn't seem to exist.
  return 0;
};

/**
 * Get the size of the pending ping file on the disk.
 * @return {Integer} The file size, in bytes, of the ping file or 0 on errors.
 */
var getPendingPingSize = async function(aPingId) {
  const path = OS.Path.join(TelemetryStorage.pingDirectoryPath, aPingId);
  try {
    return (await OS.File.stat(path)).size;
  } catch (e) {}

  // That's odd, this ping doesn't seem to exist.
  return 0;
};

/**
 * Check if a directory name is in the "YYYY-MM" format.
 * @param {String} aDirName The name of the pings archive directory.
 * @return {Boolean} True if the directory name is in the right format, false otherwise.
 */
function isValidArchiveDir(aDirName) {
  const dirRegEx = /^[0-9]{4}-[0-9]{2}$/;
  return dirRegEx.test(aDirName);
}

/**
 * Gets a date object from an archive directory name.
 * @param {String} aDirName The name of the pings archive directory. Must be in the YYYY-MM
 *        format.
 * @return {Object} A Date object or null if the dir name is not valid.
 */
function getDateFromArchiveDir(aDirName) {
  let [year, month] = aDirName.split("-");
  year = parseInt(year);
  month = parseInt(month);
  // Make sure to have sane numbers.
  if (
    !Number.isFinite(month) ||
    !Number.isFinite(year) ||
    month < 1 ||
    month > 12
  ) {
    return null;
  }
  return new Date(year, month - 1, 1, 0, 0, 0);
}
