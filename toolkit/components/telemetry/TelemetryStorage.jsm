/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TelemetryStorage"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, 'Deprecated',
  'resource://gre/modules/Deprecated.jsm');

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryStorage::";

const Telemetry = Services.telemetry;
const Utils = TelemetryUtils;

// Compute the path of the pings archive on the first use.
const DATAREPORTING_DIR = "datareporting";
const PINGS_ARCHIVE_DIR = "archived";
const ABORTED_SESSION_FILE_NAME = "aborted-session-ping";

XPCOMUtils.defineLazyGetter(this, "gDataReportingDir", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, DATAREPORTING_DIR);
});
XPCOMUtils.defineLazyGetter(this, "gPingsArchivePath", function() {
  return OS.Path.join(gDataReportingDir, PINGS_ARCHIVE_DIR);
});
XPCOMUtils.defineLazyGetter(this, "gAbortedSessionFilePath", function() {
  return OS.Path.join(gDataReportingDir, ABORTED_SESSION_FILE_NAME);
});

// Maxmimum time, in milliseconds, archive pings should be retained.
const MAX_ARCHIVED_PINGS_RETENTION_MS = 180 * 24 * 60 * 60 * 1000;  // 180 days

// Maximum space the archive can take on disk (in Bytes).
const ARCHIVE_QUOTA_BYTES = 120 * 1024 * 1024; // 120 MB

// This special value is submitted when the archive is outside of the quota.
const ARCHIVE_SIZE_PROBE_SPECIAL_VALUE = 300;

const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

/**
 * This is a policy object used to override behavior for testing.
 */
let Policy = {
  now: () => new Date(),
  getArchiveQuota: () => ARCHIVE_QUOTA_BYTES,
};

/**
 * Wait for all promises in iterable to resolve or reject. This function
 * always resolves its promise with undefined, and never rejects.
 */
function waitForAll(it) {
  let list = Array.from(it);
  let pending = list.length;
  if (pending == 0) {
    return Promise.resolve();
  }
  return new Promise(function(resolve, reject) {
    let rfunc = () => {
      --pending;
      if (pending == 0) {
        resolve();
      }
    };
    for (let p of list) {
      p.then(rfunc, rfunc);
    }
  });
}

this.TelemetryStorage = {
  get pingDirectoryPath() {
    return OS.Path.join(OS.Constants.Path.profileDir, "saved-telemetry-pings");
  },

  /**
   * Shutdown & block on any outstanding async activity in this module.
   *
   * @return {Promise} Promise that is resolved when shutdown is complete.
   */
  shutdown: function() {
    return TelemetryStorageImpl.shutdown();
  },

  /**
   * Save an archived ping to disk.
   *
   * @param {object} ping The ping data to archive.
   * @return {promise} Promise that is resolved when the ping is successfully archived.
   */
  saveArchivedPing: function(ping) {
    return TelemetryStorageImpl.saveArchivedPing(ping);
  },

  /**
   * Load an archived ping from disk.
   *
   * @param {string} id The pings id.
   * @return {promise<object>} Promise that is resolved with the ping data.
   */
  loadArchivedPing: function(id) {
    return TelemetryStorageImpl.loadArchivedPing(id);
  },

  /**
   * Get a list of info on the archived pings.
   * This will scan the archive directory and grab basic data about the existing
   * pings out of their filename.
   *
   * @return {promise<sequence<object>>}
   */
  loadArchivedPingList: function() {
    return TelemetryStorageImpl.loadArchivedPingList();
  },

  /**
   * Clean the pings archive by removing old pings.
   * This will scan the archive directory.
   *
   * @return {Promise} Resolved when the cleanup task completes.
   */
  runCleanPingArchiveTask: function() {
    return TelemetryStorageImpl.runCleanPingArchiveTask();
  },

  /**
   * Reset the storage state in tests.
   */
  reset: function() {
    return TelemetryStorageImpl.reset();
  },

  /**
   * Test method that allows waiting on the archive clean task to finish.
   */
  testCleanupTaskPromise: function() {
    return (TelemetryStorageImpl._cleanArchiveTask || Promise.resolve());
  },

  /**
   * Save a pending - outgoing - ping to disk and track it.
   *
   * @param {Object} ping The ping data.
   * @return {Promise} Resolved when the ping was saved.
   */
  savePendingPing: function(ping) {
    return TelemetryStorageImpl.savePendingPing(ping);
  },

  /**
   * Load a pending ping from disk by id.
   *
   * @param {String} id The pings id.
   * @return {Promise} Resolved with the loaded ping data.
   */
  loadPendingPing: function(id) {
    return TelemetryStorageImpl.loadPendingPing(id);
  },

  /**
   * Remove a pending ping from disk by id.
   *
   * @param {String} id The pings id.
   * @return {Promise} Resolved when the ping was removed.
   */
  removePendingPing: function(id) {
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
  loadPendingPingList: function() {
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
  getPendingPingList: function() {
    return TelemetryStorageImpl.getPendingPingList();
   },

  /**
   * Save an aborted-session ping to disk. This goes to a special location so
   * it is not picked up as a pending ping.
   *
   * @param {object} ping The ping data to save.
   * @return {promise} Promise that is resolved when the ping is successfully saved.
   */
  saveAbortedSessionPing: function(ping) {
    return TelemetryStorageImpl.saveAbortedSessionPing(ping);
  },

  /**
   * Load the aborted-session ping from disk if present.
   *
   * @return {promise<object>} Promise that is resolved with the ping data if found.
   *                           Otherwise returns null.
   */
  loadAbortedSessionPing: function() {
    return TelemetryStorageImpl.loadAbortedSessionPing();
  },

  /**
   * Remove the aborted-session ping if present.
   *
   * @return {promise} Promise that is resolved once the ping is removed.
   */
  removeAbortedSessionPing: function() {
    return TelemetryStorageImpl.removeAbortedSessionPing();
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
  savePingToFile: function(ping, file, overwrite) {
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
  savePing: function(ping, overwrite) {
    return TelemetryStorageImpl.savePing(ping, overwrite);
  },

  /**
   * Add a ping to the saved pings directory so that it gets saved
   * and sent along with other pings.
   *
   * @param {Object} pingData The ping object.
   * @return {Promise} A promise resolved when the ping is saved to the pings directory.
   */
  addPendingPing: function(pingData) {
    return TelemetryStorageImpl.addPendingPing(pingData);
  },

  /**
   * Add a ping from an existing file to the saved pings directory so that it gets saved
   * and sent along with other pings.
   * Note: that the original ping file will not be modified.
   *
   * @param {String} pingPath The path to the ping file that needs to be added to the
   *                           saved pings directory.
   * @return {Promise} A promise resolved when the ping is saved to the pings directory.
   */
  addPendingPingFromFile: function(pingPath) {
    return TelemetryStorageImpl.addPendingPingFromFile(pingPath);
  },

  /**
   * Remove the file for a ping
   *
   * @param {object} ping The ping.
   * @returns {promise}
   */
  cleanupPingFile: function(ping) {
    return TelemetryStorageImpl.cleanupPingFile(ping);
  },

  /**
   * Load the histograms from a file.
   *
   * @param {string} file The file to load.
   * @returns {promise}
   */
  loadHistograms: function loadHistograms(file) {
    return TelemetryStorageImpl.loadHistograms(file);
  },

  /**
   * The number of pending pings on disk.
   */
  get pendingPingCount() {
    return TelemetryStorageImpl.pendingPingCount;
  },

  testLoadHistograms: function(file) {
    return TelemetryStorageImpl.testLoadHistograms(file);
  },

  /**
   * Loads a ping file.
   * @param {String} aFilePath The path of the ping file.
   * @return {Promise<Object>} A promise resolved with the ping content or rejected if the
   *                           ping contains invalid data.
   */
  loadPingFile: Task.async(function* (aFilePath) {
    return TelemetryStorageImpl.loadPingFile(aFilePath);
  }),

  /**
   * Only used in tests, builds an archived ping path from the ping metadata.
   * @param {String} aPingId The ping id.
   * @param {Object} aDate The ping creation date.
   * @param {String} aType The ping type.
   * @return {String} The full path to the archived ping.
   */
  _testGetArchivedPingPath: function(aPingId, aDate, aType) {
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
  _testGetArchivedPingDataFromFileName: function(aFileName) {
    return TelemetryStorageImpl._getArchivedPingDataFromFileName(aFileName);
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
  this._log = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
}

SaveSerializer.prototype = {
  /**
   * Enqueues an operation to a list to serialise their execution in order to prevent race
   * conditions. Useful to serialise access to disk.
   *
   * @param {Function} aFunction The task function to enqueue. It must return a promise.
   * @return {Promise} A promise resolved when the enqueued task completes.
   */
  enqueueTask: function (aFunction) {
    let promise = new Promise((resolve, reject) =>
      this._queuedOperations.push([aFunction, resolve, reject]));

    if (this._queuedOperations.length == 1) {
      this._popAndPerformQueuedOperation();
    }
    return promise;
  },

  /**
   * Make sure to flush all the pending operations.
   * @return {Promise} A promise resolved when all the pending operations have completed.
   */
  flushTasks: function () {
    let dummyTask = () => new Promise(resolve => resolve());
    return this.enqueueTask(dummyTask);
  },

  /**
   * Pop a task from the queue, executes it and continue to the next one.
   * This function recursively pops all the tasks.
   */
  _popAndPerformQueuedOperation: function () {
    if (!this._queuedOperations.length || this._queuedInProgress) {
      return;
    }

    this._log.trace("_popAndPerformQueuedOperation - Performing queued operation.");
    let [func, resolve, reject] = this._queuedOperations.shift();
    let promise;

    try {
      this._queuedInProgress = true;
      promise = func();
    } catch (ex) {
      this._log.warn("_popAndPerformQueuedOperation - Queued operation threw during execution. ",
                     ex);
      this._queuedInProgress = false;
      reject(ex);
      this._popAndPerformQueuedOperation();
      return;
    }

    if (!promise || typeof(promise.then) != "function") {
      let msg = "Queued operation did not return a promise: " + func;
      this._log.warn("_popAndPerformQueuedOperation - " + msg);

      this._queuedInProgress = false;
      reject(new Error(msg));
      this._popAndPerformQueuedOperation();
      return;
    }

    promise.then(result => {
        this._queuedInProgress = false;
        resolve(result);
        this._popAndPerformQueuedOperation();
      },
      error => {
        this._log.warn("_popAndPerformQueuedOperation - Failure when performing queued operation.",
                       error);
        this._queuedInProgress = false;
        reject(error);
        this._popAndPerformQueuedOperation();
      });
  },
};

let TelemetryStorageImpl = {
  _logger: null,
  // Used to serialize aborted session ping writes to disk.
  _abortedSessionSerializer: new SaveSerializer(),

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

  // Tracks the pending pings in a Map of (id -> {timestampCreated, type}).
  // We use this to cache info on pending pings to avoid scanning the disk more than once.
  _pendingPings: new Map(),

  // Track the shutdown process to bail out of the clean up task quickly.
  _shutdown: false,

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    return this._logger;
  },

  /**
   * Shutdown & block on any outstanding async activity in this module.
   *
   * @return {Promise} Promise that is resolved when shutdown is complete.
   */
  shutdown: Task.async(function*() {
    this._shutdown = true;
    yield this._abortedSessionSerializer.flushTasks();
    // If the archive cleaning task is running, block on it. It should bail out as soon
    // as possible.
    yield this._cleanArchiveTask;
  }),

  /**
   * Save an archived ping to disk.
   *
   * @param {object} ping The ping data to archive.
   * @return {promise} Promise that is resolved when the ping is successfully archived.
   */
  saveArchivedPing: function(ping) {
    let promise = this._saveArchivedPingTask(ping);
    this._activelyArchiving.add(promise);
    promise.then((r) => { this._activelyArchiving.delete(promise); },
                 (e) => { this._activelyArchiving.delete(promise); });
    return promise;
  },

  _saveArchivedPingTask: Task.async(function*(ping) {
    const creationDate = new Date(ping.creationDate);
    if (this._archivedPings.has(ping.id)) {
      const data = this._archivedPings.get(ping.id);
      if (data.timestampCreated > creationDate.getTime()) {
        this._log.error("saveArchivedPing - trying to overwrite newer ping with the same id");
        return Promise.reject(new Error("trying to overwrite newer ping with the same id"));
      } else {
        this._log.warn("saveArchivedPing - overwriting older ping with the same id");
      }
    }

    // Get the archived ping path and append the lz4 suffix to it (so we have 'jsonlz4').
    const filePath = getArchivedPingPath(ping.id, creationDate, ping.type) + "lz4";
    yield OS.File.makeDir(OS.Path.dirname(filePath), { ignoreExisting: true,
                                                       from: OS.Constants.Path.profileDir });
    yield this.savePingToFile(ping, filePath, /*overwrite*/ true, /*compressed*/ true);

    this._archivedPings.set(ping.id, {
      timestampCreated: creationDate.getTime(),
      type: ping.type,
    });

    Telemetry.getHistogramById("TELEMETRY_ARCHIVE_SESSION_PING_COUNT").add();
  }),

  /**
   * Load an archived ping from disk.
   *
   * @param {string} id The pings id.
   * @return {promise<object>} Promise that is resolved with the ping data.
   */
  loadArchivedPing: Task.async(function*(id) {
    this._log.trace("loadArchivedPing - id: " + id);

    const data = this._archivedPings.get(id);
    if (!data) {
      this._log.trace("loadArchivedPing - no ping with id: " + id);
      return Promise.reject(new Error("TelemetryStorage.loadArchivedPing - no ping with id " + id));
    }

    const path = getArchivedPingPath(id, new Date(data.timestampCreated), data.type);
    const pathCompressed = path + "lz4";

    try {
      // Try to load a compressed version of the archived ping first.
      this._log.trace("loadArchivedPing - loading ping from: " + pathCompressed);
      return yield this.loadPingFile(pathCompressed, /*compressed*/ true);
    } catch (ex if ex.becauseNoSuchFile) {
      // If that fails, look for the uncompressed version.
      this._log.trace("loadArchivedPing - compressed ping not found, loading: " + path);
      return yield this.loadPingFile(path, /*compressed*/ false);
    }
  }),

  /**
   * Remove an archived ping from disk.
   *
   * @param {string} id The pings id.
   * @param {number} timestampCreated The pings creation timestamp.
   * @param {string} type The pings type.
   * @return {promise<object>} Promise that is resolved when the pings is removed.
   */
  _removeArchivedPing: Task.async(function*(id, timestampCreated, type) {
    this._log.trace("_removeArchivedPing - id: " + id + ", timestampCreated: " + timestampCreated + ", type: " + type);
    const path = getArchivedPingPath(id, new Date(timestampCreated), type);
    const pathCompressed = path + "lz4";

    this._log.trace("_removeArchivedPing - removing ping from: " + path);
    yield OS.File.remove(path, {ignoreAbsent: true});
    yield OS.File.remove(pathCompressed, {ignoreAbsent: true});
  }),

  /**
   * Clean the pings archive by removing old pings.
   *
   * @return {Promise} Resolved when the cleanup task completes.
   */
  runCleanPingArchiveTask: function() {
    // If there's an archive cleaning task already running, return it.
    if (this._cleanArchiveTask) {
      return this._cleanArchiveTask;
    }

    // Make sure to clear |_cleanArchiveTask| once done.
    let clear = () => this._cleanArchiveTask = null;
    // Since there's no archive cleaning task running, start it.
    this._cleanArchiveTask = this._cleanArchive().then(clear, clear);
    return this._cleanArchiveTask;
  },

  /**
   * Removes pings which are too old from the pings archive.
   * @return {Promise} Resolved when the ping age check is complete.
   */
  _purgeOldPings: Task.async(function*() {
    this._log.trace("_purgeOldPings");

    const nowDate = Policy.now();
    const startTimeStamp = nowDate.getTime();
    let dirIterator = new OS.File.DirectoryIterator(gPingsArchivePath);
    let subdirs = (yield dirIterator.nextBatch()).filter(e => e.isDir);

    // Keep track of the newest removed month to update the cache, if needed.
    let newestRemovedMonthTimestamp = null;
    let evictedDirsCount = 0;
    let maxDirAgeInMonths = 0;

    // Walk through the monthly subdirs of the form <YYYY-MM>/
    for (let dir of subdirs) {
      if (this._shutdown) {
        this._log.trace("_purgeOldPings - Terminating the clean up task due to shutdown");
        return;
      }

      if (!isValidArchiveDir(dir.name)) {
        this._log.warn("_purgeOldPings - skipping invalidly named subdirectory " + dir.path);
        continue;
      }

      const archiveDate = getDateFromArchiveDir(dir.name);
      if (!archiveDate) {
        this._log.warn("_purgeOldPings - skipping invalid subdirectory date " + dir.path);
        continue;
      }

      // If this archive directory is older than 180 days, remove it.
      if ((startTimeStamp - archiveDate.getTime()) > MAX_ARCHIVED_PINGS_RETENTION_MS) {
        try {
          yield OS.File.removeDir(dir.path);
          evictedDirsCount++;

          // Update the newest removed month.
          newestRemovedMonthTimestamp = Math.max(archiveDate, newestRemovedMonthTimestamp);
        } catch (ex) {
          this._log.error("_purgeOldPings - Unable to remove " + dir.path, ex);
        }
      } else {
        // We're not removing this directory, so record the age for the oldest directory.
        const dirAgeInMonths = Utils.getElapsedTimeInMonths(archiveDate, nowDate);
        maxDirAgeInMonths = Math.max(dirAgeInMonths, maxDirAgeInMonths);
      }
    }

    // Trigger scanning of the archived pings.
    yield this.loadArchivedPingList();

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
    Telemetry.getHistogramById("TELEMETRY_ARCHIVE_EVICTED_OLD_DIRS")
             .add(evictedDirsCount);
    Telemetry.getHistogramById("TELEMETRY_ARCHIVE_EVICTING_DIRS_MS")
             .add(Math.ceil(endTimeStamp - startTimeStamp));
    Telemetry.getHistogramById("TELEMETRY_ARCHIVE_OLDEST_DIRECTORY_AGE")
             .add(maxDirAgeInMonths);
  }),

  /**
   * Enforce a disk quota for the pings archive.
   * @return {Promise} Resolved when the quota check is complete.
   */
  _enforceArchiveQuota: Task.async(function*() {
    this._log.trace("_enforceArchiveQuota");
    const startTimeStamp = Policy.now().getTime();

    // Build an ordered list, from newer to older, of archived pings.
    let pingList = [for (p of this._archivedPings) {
      id: p[0],
      timestampCreated: p[1].timestampCreated,
      type: p[1].type,
    }];

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
        this._log.trace("_enforceArchiveQuota - Terminating the clean up task due to shutdown");
        return;
      }

      let ping = pingList[i];

      // Get the size for this ping.
      const fileSize =
        yield getArchivedPingSize(ping.id, new Date(ping.timestampCreated), ping.type);
      if (!fileSize) {
        this._log.warn("_enforceArchiveQuota - Unable to find the size of ping " + ping.id);
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
    Telemetry.getHistogramById("TELEMETRY_ARCHIVE_CHECKING_OVER_QUOTA_MS")
             .add(Math.round(Policy.now().getTime() - startTimeStamp));

    let submitProbes = (sizeInMB, evictedPings, elapsedMs) => {
      Telemetry.getHistogramById("TELEMETRY_ARCHIVE_SIZE_MB").add(sizeInMB);
      Telemetry.getHistogramById("TELEMETRY_ARCHIVE_EVICTED_OVER_QUOTA").add(evictedPings);
      Telemetry.getHistogramById("TELEMETRY_ARCHIVE_EVICTING_OVER_QUOTA_MS").add(elapsedMs);
    };

    // Check if we're using too much space. If not, submit the archive size and bail out.
    if (archiveSizeInBytes < Policy.getArchiveQuota()) {
      submitProbes(Math.round(archiveSizeInBytes / 1024 / 1024), 0, 0);
      return;
    }

    this._log.info("_enforceArchiveQuota - archive size: " + archiveSizeInBytes + "bytes"
                   + ", safety quota: " + SAFE_QUOTA + "bytes");

    let pingsToPurge = pingList.slice(lastPingIndexToKeep + 1);

    // Remove all the pings older than the last one which we are safe to keep.
    for (let ping of pingsToPurge) {
      if (this._shutdown) {
        this._log.trace("_enforceArchiveQuota - Terminating the clean up task due to shutdown");
        return;
      }

      // This list is guaranteed to be in order, so remove the pings at its
      // beginning (oldest).
      yield this._removeArchivedPing(ping.id, ping.timestampCreated, ping.type);

      // Remove outdated pings from the cache.
      this._archivedPings.delete(ping.id);
    }

    const endTimeStamp = Policy.now().getTime();
    submitProbes(ARCHIVE_SIZE_PROBE_SPECIAL_VALUE, pingsToPurge.length,
                 Math.ceil(endTimeStamp - startTimeStamp));
  }),

  _cleanArchive: Task.async(function*() {
    this._log.trace("cleanArchiveTask");

    if (!(yield OS.File.exists(gPingsArchivePath))) {
      return;
    }

    // Remove pings older than 180 days.
    try {
      yield this._purgeOldPings();
    } catch (ex) {
      this._log.error("_cleanArchive - There was an error removing old directories", ex);
    }

    // Make sure we respect the archive disk quota.
    yield this._enforceArchiveQuota();
  }),

  /**
   * Reset the storage state in tests.
   */
  reset: function() {
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
  loadArchivedPingList: Task.async(function*() {
    // If there's an archive loading task already running, return it.
    if (this._scanArchiveTask) {
      return this._scanArchiveTask;
    }

    yield waitForAll(this._activelyArchiving);

    if (this._scannedArchiveDirectory) {
      this._log.trace("loadArchivedPingList - Archive already scanned, hitting cache.");
      return this._archivedPings;
    }

    // Since there's no archive loading task running, start it.
    let result;
    try {
      this._scanArchiveTask = this._scanArchive();
      result = yield this._scanArchiveTask;
    } finally {
      this._scanArchiveTask = null;
    }
    return result;
  }),

  _scanArchive: Task.async(function*() {
    this._log.trace("_scanArchive");

    let submitProbes = (pingCount, dirCount) => {
      Telemetry.getHistogramById("TELEMETRY_ARCHIVE_SCAN_PING_COUNT")
               .add(pingCount);
      Telemetry.getHistogramById("TELEMETRY_ARCHIVE_DIRECTORIES_COUNT")
               .add(dirCount);
    };

    if (!(yield OS.File.exists(gPingsArchivePath))) {
      submitProbes(0, 0);
      return new Map();
    }

    let dirIterator = new OS.File.DirectoryIterator(gPingsArchivePath);
    let subdirs =
      (yield dirIterator.nextBatch()).filter(e => e.isDir).filter(e => isValidArchiveDir(e.name));

    // Walk through the monthly subdirs of the form <YYYY-MM>/
    for (let dir of subdirs) {
      this._log.trace("_scanArchive - checking in subdir: " + dir.path);
      let pingIterator = new OS.File.DirectoryIterator(dir.path);
      let pings = (yield pingIterator.nextBatch()).filter(e => !e.isDir);

      // Now process any ping files of the form "<timestamp>.<uuid>.<type>.[json|jsonlz4]".
      for (let p of pings) {
        // data may be null if the filename doesn't match the above format.
        let data = this._getArchivedPingDataFromFileName(p.name);
        if (!data) {
          continue;
        }

        // In case of conflicts, overwrite only with newer pings.
        if (this._archivedPings.has(data.id)) {
          const overwrite = data.timestamp > this._archivedPings.get(data.id).timestampCreated;
          this._log.warn("_scanArchive - have seen this id before: " + data.id +
                         ", overwrite: " + overwrite);
          if (!overwrite) {
            continue;
          }

          yield this._removeArchivedPing(data.id, data.timestampCreated, data.type)
                    .catch((e) => this._log.warn("_scanArchive - failed to remove ping", e));
        }

        this._archivedPings.set(data.id, {
          timestampCreated: data.timestamp,
          type: data.type,
        });
      }
    }

    // Mark the archive as scanned, so we no longer hit the disk.
    this._scannedArchiveDirectory = true;
    // Update the ping and directories count histograms.
    submitProbes(this._archivedPings.size, subdirs.length);
    return this._archivedPings;
  }),

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
  savePingToFile: Task.async(function*(ping, filePath, overwrite, compress = false) {
    try {
      this._log.trace("savePingToFile - path: " + filePath);
      let pingString = JSON.stringify(ping);
      let options = { tmpPath: filePath + ".tmp", noOverwrite: !overwrite };
      if (compress) {
        options.compression = "lz4";
      }
      yield OS.File.writeAtomic(filePath, pingString, options);
    } catch(e if e.becauseExists) {
    }
  }),

  /**
   * Save a ping to its file.
   *
   * @param {object} ping The content of the ping to save.
   * @param {bool} overwrite If |true|, the file will be overwritten
   * if it exists.
   * @returns {promise}
   */
  savePing: Task.async(function*(ping, overwrite) {
    yield getPingDirectory();
    let file = pingFilePath(ping);
    yield this.savePingToFile(ping, file, overwrite);
    return file;
  }),

  /**
   * Add a ping from an existing file to the saved pings directory so that it gets saved
   * and sent along with other pings.
   * Note: that the original ping file will not be modified.
   *
   * @param {String} pingPath The path to the ping file that needs to be added to the
   *                           saved pings directory.
   * @return {Promise} A promise resolved when the ping is saved to the pings directory.
   */
  addPendingPingFromFile: function(pingPath) {
    // Pings in the saved ping directory need to have the ping id or slug (old format) as
    // the file name. We load the ping content, check that it is valid, and use it to save
    // the ping file with the correct file name.
    return this.loadPingFile(pingPath).then(ping => {
      // Since we read a ping successfully, update the related histogram.
      Telemetry.getHistogramById("READ_SAVED_PING_SUCCESS").add(1);
      return this.addPendingPing(ping);
    });
  },

  /**
   * Add a ping to the saved pings directory so that it gets saved
   * and sent along with other pings.
   * Note: that the original ping file will not be modified.
   *
   * @param {Object} ping The ping object.
   * @return {Promise} A promise resolved when the ping is saved to the pings directory.
   */
  addPendingPing: function(ping) {
    return this.savePendingPing(ping);
  },

  /**
   * Remove the file for a ping
   *
   * @param {object} ping The ping.
   * @returns {promise}
   */
  cleanupPingFile: function(ping) {
    return OS.File.remove(pingFilePath(ping));
  },

  savePendingPing: function(ping) {
    return this.savePing(ping, true).then((path) => {
      this._pendingPings.set(ping.id, {
        path: path,
        lastModificationDate: Policy.now().getTime(),
      });
      this._log.trace("savePendingPing - saved ping with id " + ping.id);
    });
  },

  loadPendingPing: function(id) {
    this._log.trace("loadPendingPing - id: " + id);
    let info = this._pendingPings.get(id);
    if (!info) {
      return;
    }

    return this.loadPingFile(info.path, false);
  },

  removePendingPing: function(id) {
    let info = this._pendingPings.get(id);
    if (!info) {
      this._log.trace("removePendingPing - unknown id " + id);
      return Promise.resolve();
    }

    this._log.trace("removePendingPing - deleting ping with id: " + id +
                    ", path: " + info.path);
    this._pendingPings.delete(id);
    return OS.File.remove(info.path).catch((ex) =>
      this._log.error("removePendingPing - failed to remove ping", ex));
  },

  loadPendingPingList: function() {
    // If we already have a pending scanning task active, return that.
    if (this._scanPendingPingsTask) {
      return this._scanPendingPingsTask;
    }

    if (this._scannedPendingDirectory) {
      this._log.trace("loadPendingPingList - Pending already scanned, hitting cache.");
      return Promise.resolve(this._buildPingList());
    }

    // Since there's no pending pings scan task running, start it.
    // Also make sure to clear the task once done.
    this._scanPendingPingsTask = this._scanPendingPings().then(pings => {
      this._scanPendingPingsTask = null;
      return pings;
    }, ex => {
      this._scanPendingPingsTask = null;
      throw ex;
    });
    return this._scanPendingPingsTask;
  },

  getPendingPingList: function() {
    return this._buildPingList();
  },

  _scanPendingPings: Task.async(function*() {
    this._log.trace("_scanPendingPings");

    let directory = TelemetryStorage.pingDirectoryPath;
    let iter = new OS.File.DirectoryIterator(directory);
    let exists = yield iter.exists();

    if (!exists) {
      yield iter.close();
      return [];
    }

    let files = (yield iter.nextBatch()).filter(e => !e.isDir);

    for (let file of files) {
      if (this._shutdown) {
        yield iter.close();
        return [];
      }

      let info;
      try {
        info = yield OS.File.stat(file.path);
      } catch (ex) {
        this._log.error("_scanPendingPings - failed to stat file " + file.path, ex);
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

    yield iter.close();
    this._scannedPendingDirectory = true;
    return this._buildPingList();
  }),

  _buildPingList: function() {
    const list = [for (p of this._pendingPings) {
      id: p[0],
      lastModificationDate: p[1].lastModificationDate,
    }];

    list.sort((a, b) => b.lastModificationDate - a.lastModificationDate);
    return list;
  },

  /**
   * Load the histograms from a file.
   *
   * Once loaded, the saved pings can be accessed (destructively only)
   * through |popPendingPings|.
   *
   * @param {string} file The file to load.
   * @returns {promise}
   */
  loadHistograms: Task.async(function*(file) {
    let success = true;
    try {
      const ping = yield this.loadPingfile(file);
      return ping;
    } catch (ex) {
      success = false;
      yield OS.File.remove(file);
    } finally {
      const success_histogram = Telemetry.getHistogramById("READ_SAVED_PING_SUCCESS");
      success_histogram.add(success);
    }
  }),

  get pendingPingCount() {
    return this._pendingPings.size;
  },

  testLoadHistograms: function(file) {
    return this.loadHistograms(file.path);
  },

  /**
   * Loads a ping file.
   * @param {String} aFilePath The path of the ping file.
   * @param {Boolean} [aCompressed=false] If |true|, expects the file to be compressed using lz4.
   * @return {Promise<Object>} A promise resolved with the ping content or rejected if the
   *                           ping contains invalid data.
   */
  loadPingFile: Task.async(function* (aFilePath, aCompressed = false) {
    let options = {};
    if (aCompressed) {
      options.compression = "lz4";
    }
    let array = yield OS.File.read(aFilePath, options);
    let decoder = new TextDecoder();
    let string = decoder.decode(array);

    let ping = JSON.parse(string);
    // The ping's payload used to be stringified JSON.  Deal with that.
    if (typeof(ping.payload) == "string") {
      ping.payload = JSON.parse(ping.payload);
    }
    return ping;
  }),

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
  _getArchivedPingDataFromFileName: function(fileName) {
    // Extract the parts.
    let parts = fileName.split(".");
    if (parts.length != 4) {
      this._log.trace("_getArchivedPingDataFromFileName - should have 4 parts");
      return null;
    }

    let [timestamp, uuid, type, extension] = parts;
    if (extension != "json" && extension != "jsonlz4") {
      this._log.trace("_getArchivedPingDataFromFileName - should have 'json' or 'jsonlz4' extension");
      return null;
    }

    // Check for a valid timestamp.
    timestamp = parseInt(timestamp);
    if (Number.isNaN(timestamp)) {
      this._log.trace("_getArchivedPingDataFromFileName - should have a valid timestamp");
      return null;
    }

    // Check for a valid UUID.
    if (!UUID_REGEX.test(uuid)) {
      this._log.trace("_getArchivedPingDataFromFileName - should have a valid id");
      return null;
    }

    // Check for a valid type string.
    const typeRegex = /^[a-z0-9][a-z0-9-]+[a-z0-9]$/i;
    if (!typeRegex.test(type)) {
      this._log.trace("_getArchivedPingDataFromFileName - should have a valid type");
      return null;
    }

    return {
      timestamp: timestamp,
      id: uuid,
      type: type,
    };
  },

  saveAbortedSessionPing: Task.async(function*(ping) {
    this._log.trace("saveAbortedSessionPing - ping path: " + gAbortedSessionFilePath);
    yield OS.File.makeDir(gDataReportingDir, { ignoreExisting: true });

    return this._abortedSessionSerializer.enqueueTask(() =>
      this.savePingToFile(ping, gAbortedSessionFilePath, true));
  }),

  loadAbortedSessionPing: Task.async(function*() {
    let ping = null;
    try {
      ping = yield this.loadPingFile(gAbortedSessionFilePath);
    } catch (ex if ex.becauseNoSuchFile) {
      this._log.trace("loadAbortedSessionPing - no such file");
    } catch (ex) {
      this._log.error("loadAbortedSessionPing - error removing ping", ex)
    }
    return ping;
  }),

  removeAbortedSessionPing: function() {
    return this._abortedSessionSerializer.enqueueTask(Task.async(function*() {
      try {
        yield OS.File.remove(gAbortedSessionFilePath, { ignoreAbsent: false });
        this._log.trace("removeAbortedSessionPing - success");
      } catch (ex if ex.becauseNoSuchFile) {
        this._log.trace("removeAbortedSessionPing - no such file");
      } catch (ex) {
        this._log.error("removeAbortedSessionPing - error removing ping", ex)
      }
    }.bind(this)));
  },
};

///// Utility functions

function pingFilePath(ping) {
  // Support legacy ping formats, who don't have an "id" field, but a "slug" field.
  let pingIdentifier = (ping.slug) ? ping.slug : ping.id;
  return OS.Path.join(TelemetryStorage.pingDirectoryPath, pingIdentifier);
}

function getPingDirectory() {
  return Task.spawn(function*() {
    let directory = TelemetryStorage.pingDirectoryPath;

    if (!(yield OS.File.exists(directory))) {
      yield OS.File.makeDir(directory, { unixMode: OS.Constants.S_IRWXU });
    }

    return directory;
  });
}

/**
 * Build the path to the archived ping.
 * @param {String} aPingId The ping id.
 * @param {Object} aDate The ping creation date.
 * @param {String} aType The ping type.
 * @return {String} The full path to the archived ping.
 */
function getArchivedPingPath(aPingId, aDate, aType) {
  // Helper to pad the month to 2 digits, if needed (e.g. "1" -> "01").
  let addLeftPadding = value => (value < 10) ? ("0" + value) : value;
  // Get the ping creation date and generate the archive directory to hold it. Note
  // that getMonth returns a 0-based month, so we need to add an offset.
  let archivedPingDir = OS.Path.join(gPingsArchivePath,
    aDate.getFullYear() + '-' + addLeftPadding(aDate.getMonth() + 1));
  // Generate the archived ping file path as YYYY-MM/<TIMESTAMP>.UUID.type.json
  let fileName = [aDate.getTime(), aPingId, aType, "json"].join(".");
  return OS.Path.join(archivedPingDir, fileName);
}

/**
 * Get the size of the ping file on the disk.
 * @return {Integer} The file size, in bytes, of the ping file or 0 on errors.
 */
let getArchivedPingSize = Task.async(function*(aPingId, aDate, aType) {
  const path = getArchivedPingPath(aPingId, aDate, aType);
  let filePaths = [ path + "lz4", path ];

  for (let path of filePaths) {
    try {
      return (yield OS.File.stat(path)).size;
    } catch (e) {}
  }

  // That's odd, this ping doesn't seem to exist.
  return 0;
});

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
  if (!Number.isFinite(month) || !Number.isFinite(year) || month < 1 || month > 12) {
    return null;
  }
  return new Date(year, month - 1, 1, 0, 0, 0);
}
