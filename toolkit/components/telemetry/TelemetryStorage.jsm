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
Cu.import("resource://gre/modules/Promise.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, 'Deprecated',
  'resource://gre/modules/Deprecated.jsm');

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryStorage::";

const Telemetry = Services.telemetry;

// Compute the path of the pings archive on the first use.
const DATAREPORTING_DIR = "datareporting";
const PINGS_ARCHIVE_DIR = "archived";
XPCOMUtils.defineLazyGetter(this, "gPingsArchivePath", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, DATAREPORTING_DIR, PINGS_ARCHIVE_DIR);
});

// Files that have been lying around for longer than MAX_PING_FILE_AGE are
// deleted without being loaded.
const MAX_PING_FILE_AGE = 14 * 24 * 60 * 60 * 1000; // 2 weeks

// Files that are older than OVERDUE_PING_FILE_AGE, but younger than
// MAX_PING_FILE_AGE indicate that we need to send all of our pings ASAP.
const OVERDUE_PING_FILE_AGE = 7 * 24 * 60 * 60 * 1000; // 1 week

// Maximum number of pings to save.
const MAX_LRU_PINGS = 50;

// The number of outstanding saved pings that we have issued loading
// requests for.
let pingsLoaded = 0;

// The number of pings that we have destroyed due to being older
// than MAX_PING_FILE_AGE.
let pingsDiscarded = 0;

// The number of pings that are older than OVERDUE_PING_FILE_AGE
// but younger than MAX_PING_FILE_AGE.
let pingsOverdue = 0;

// Data that has neither been saved nor sent by ping
let pendingPings = [];

let isPingDirectoryCreated = false;

this.TelemetryStorage = {
  get MAX_PING_FILE_AGE() {
    return MAX_PING_FILE_AGE;
  },

  get OVERDUE_PING_FILE_AGE() {
    return OVERDUE_PING_FILE_AGE;
  },

  get MAX_LRU_PINGS() {
    return MAX_LRU_PINGS;
  },

  get pingDirectoryPath() {
    return OS.Path.join(OS.Constants.Path.profileDir, "saved-telemetry-pings");
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
   * @param {number} timestampCreated The pings creation timestamp.
   * @param {string} type The pings type.
   * @return {promise<object>} Promise that is resolved with the ping data.
   */
  loadArchivedPing: function(id, timestampCreated, type) {
    return TelemetryStorageImpl.loadArchivedPing(id, timestampCreated, type);
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
   * Save all pending pings.
   *
   * @param {object} sessionPing The additional session ping.
   * @returns {promise}
   */
  savePendingPings: function(sessionPing) {
    return TelemetryStorageImpl.savePendingPings(sessionPing);
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
   * Load all saved pings.
   *
   * Once loaded, the saved pings can be accessed (destructively only)
   * through |popPendingPings|.
   *
   * @returns {promise}
   */
  loadSavedPings: function() {
    return TelemetryStorageImpl.loadSavedPings();
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
  loadHistograms: function loadHistograms(file) {
    return TelemetryStorageImpl.loadHistograms(file);
  },

  /**
   * The number of pings loaded since the beginning of time.
   */
  get pingsLoaded() {
    return TelemetryStorageImpl.pingsLoaded;
  },

  /**
   * The number of pings loaded that are older than OVERDUE_PING_FILE_AGE
   * but younger than MAX_PING_FILE_AGE.
   */
  get pingsOverdue() {
    return TelemetryStorageImpl.pingsOverdue;
  },

  /**
   * The number of pings that we just tossed out for being older than
   * MAX_PING_FILE_AGE.
   */
  get pingsDiscarded() {
    return TelemetryStorageImpl.pingsDiscarded;
  },

  /**
   * Iterate destructively through the pending pings.
   *
   * @return {iterator}
   */
  popPendingPings: function*() {
    while (pendingPings.length > 0) {
      let data = pendingPings.pop();
      yield data;
    }
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
};

let TelemetryStorageImpl = {
  _logger: null,

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    return this._logger;
  },

  /**
   * Save an archived ping to disk.
   *
   * @param {object} ping The ping data to archive.
   * @return {promise} Promise that is resolved when the ping is successfully archived.
   */
  saveArchivedPing: Task.async(function*(ping) {
    const creationDate = new Date(ping.creationDate);
    const filePath = getArchivedPingPath(ping.id, creationDate, ping.type);
    yield OS.File.makeDir(OS.Path.dirname(filePath), { ignoreExisting: true,
                                                       from: OS.Constants.Path.profileDir });
    yield TelemetryStorage.savePingToFile(ping, filePath, true);
  }),

  /**
   * Load an archived ping from disk.
   *
   * @param {string} id The pings id.
   * @param {number} timestampCreated The pings creation timestamp.
   * @param {string} type The pings type.
   * @return {promise<object>} Promise that is resolved with the ping data.
   */
  loadArchivedPing: function(id, timestampCreated, type) {
    this._log.trace("loadArchivedPing - id: " + id + ", timestampCreated: " + timestampCreated + ", type: " + type);
    const path = getArchivedPingPath(id, new Date(timestampCreated), type);
    this._log.trace("loadArchivedPing - loading ping from: " + path);
    return this.loadPingFile(path);
  },

  /**
   * Remove an archived ping from disk.
   *
   * @param {string} id The pings id.
   * @param {number} timestampCreated The pings creation timestamp.
   * @param {string} type The pings type.
   * @return {promise<object>} Promise that is resolved when the pings is removed.
   */
  _removeArchivedPing: function(id, timestampCreated, type) {
    this._log.trace("_removeArchivedPing - id: " + id + ", timestampCreated: " + timestampCreated + ", type: " + type);
    const path = getArchivedPingPath(id, new Date(timestampCreated), type);
    this._log.trace("_removeArchivedPing - removing ping from: " + path);
    return OS.File.remove(path);
  },

  /**
   * Get a list of info on the archived pings.
   * This will scan the archive directory and grab basic data about the existing
   * pings out of their filename.
   *
   * @return {promise<sequence<object>>}
   */
  loadArchivedPingList: Task.async(function*() {
    this._log.trace("loadArchivedPingList");

    if (!(yield OS.File.exists(gPingsArchivePath))) {
      return new Map();
    }

    let archivedPings = new Map();
    let dirIterator = new OS.File.DirectoryIterator(gPingsArchivePath);
    let subdirs = (yield dirIterator.nextBatch()).filter(e => e.isDir);

    // Walk through the monthly subdirs of the form <YYYY-MM>/
    for (let dir of subdirs) {
      const dirRegEx = /^[0-9]{4}-[0-9]{2}$/;
      if (!dirRegEx.test(dir.name)) {
        this._log.warn("loadArchivedPingList - skipping invalidly named subdirectory " + dir.path);
        continue;
      }

      this._log.trace("loadArchivedPingList - checking in subdir: " + dir.path);
      let pingIterator = new OS.File.DirectoryIterator(dir.path);
      let pings = (yield pingIterator.nextBatch()).filter(e => !e.isDir);

      // Now process any ping files of the form "<timestamp>.<uuid>.<type>.json"
      for (let p of pings) {
        // data may be null if the filename doesn't match the above format.
        let data = this._getArchivedPingDataFromFileName(p.name);
        if (!data) {
          continue;
        }

        // In case of conflicts, overwrite only with newer pings.
        if (archivedPings.has(data.id)) {
          const overwrite = data.timestamp > archivedPings.get(data.id).timestampCreated;
          this._log.warn("loadArchivedPingList - have seen this id before: " + data.id +
                         ", overwrite: " + overwrite);
          if (!overwrite) {
            continue;
          }

          yield this._removeArchivedPing(data.id, data.timestampCreated, data.type)
                    .catch((e) => this._log.warn("loadArchivedPingList - failed to remove ping", e));
        }

        archivedPings.set(data.id, {
          timestampCreated: data.timestamp,
          type: data.type,
        });
      }
    }

    return archivedPings;
  }),

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
  savePingToFile: function(ping, filePath, overwrite) {
    return Task.spawn(function*() {
      try {
        let pingString = JSON.stringify(ping);
        yield OS.File.writeAtomic(filePath, pingString, {tmpPath: filePath + ".tmp",
                                  noOverwrite: !overwrite});
      } catch(e if e.becauseExists) {
      }
    })
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
    return Task.spawn(function*() {
      yield getPingDirectory();
      let file = pingFilePath(ping);
      yield this.savePingToFile(ping, file, overwrite);
    }.bind(this));
  },

  /**
   * Save all pending pings.
   *
   * @param {object} sessionPing The additional session ping.
   * @returns {promise}
   */
  savePendingPings: function(sessionPing) {
    let p = pendingPings.reduce((p, ping) => {
      // Restore the files with the previous pings if for some reason they have
      // been deleted, don't overwrite them otherwise.
      p.push(this.savePing(ping, false));
      return p;}, [this.savePing(sessionPing, true)]);

    pendingPings = [];
    return Promise.all(p);
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
    // Pings in the saved ping directory need to have the ping id or slug (old format) as
    // the file name. We load the ping content, check that it is valid, and use it to save
    // the ping file with the correct file name.
    return this.loadPingFile(pingPath).then(ping => {
      // Since we read a ping successfully, update the related histogram.
      Telemetry.getHistogramById("READ_SAVED_PING_SUCCESS").add(1);
      this.addPendingPing(ping);
      return this.savePing(ping, false);
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
    // Append the ping to the pending list.
    pendingPings.push(ping);
    // Save the ping to the saved pings directory.
    return this.savePing(ping, false);
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

  /**
   * Load all saved pings.
   *
   * Once loaded, the saved pings can be accessed (destructively only)
   * through |popPendingPings|.
   *
   * @returns {promise}
   */
  loadSavedPings: function() {
    return Task.spawn(function*() {
      let directory = TelemetryStorage.pingDirectoryPath;
      let iter = new OS.File.DirectoryIterator(directory);
      let exists = yield iter.exists();

      if (exists) {
        let entries = yield iter.nextBatch();
        let sortedEntries = [];

        for (let entry of entries) {
          if (entry.isDir) {
            continue;
          }

          let info = yield OS.File.stat(entry.path);
          sortedEntries.push({entry:entry, lastModificationDate: info.lastModificationDate});
        }

        sortedEntries.sort(function compare(a, b) {
          return b.lastModificationDate - a.lastModificationDate;
        });

        let count = 0;
        let result = [];

        // Keep only the last MAX_LRU_PINGS entries to avoid that the backlog overgrows.
        for (let i = 0; i < MAX_LRU_PINGS && i < sortedEntries.length; i++) {
          let entry = sortedEntries[i].entry;
          result.push(this.loadHistograms(entry.path))
        }

        for (let i = MAX_LRU_PINGS; i < sortedEntries.length; i++) {
          let entry = sortedEntries[i].entry;
          OS.File.remove(entry.path);
        }

        yield Promise.all(result);

        Services.telemetry.getHistogramById('TELEMETRY_FILES_EVICTED').
          add(sortedEntries.length - MAX_LRU_PINGS);
      }

      yield iter.close();
    }.bind(this));
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
  loadHistograms: function loadHistograms(file) {
    return OS.File.stat(file).then(function(info){
      let now = Date.now();
      if (now - info.lastModificationDate > MAX_PING_FILE_AGE) {
        // We haven't had much luck in sending this file; delete it.
        pingsDiscarded++;
        return OS.File.remove(file);
      }

      // This file is a bit stale, and overdue for sending.
      if (now - info.lastModificationDate > OVERDUE_PING_FILE_AGE) {
        pingsOverdue++;
      }

      pingsLoaded++;
      return addToPendingPings(file);
    });
  },

  /**
   * The number of pings loaded since the beginning of time.
   */
  get pingsLoaded() {
    return pingsLoaded;
  },

  /**
   * The number of pings loaded that are older than OVERDUE_PING_FILE_AGE
   * but younger than MAX_PING_FILE_AGE.
   */
  get pingsOverdue() {
    return pingsOverdue;
  },

  /**
   * The number of pings that we just tossed out for being older than
   * MAX_PING_FILE_AGE.
   */
  get pingsDiscarded() {
    return pingsDiscarded;
  },

  testLoadHistograms: function(file) {
    pingsLoaded = 0;
    return this.loadHistograms(file.path);
  },

  /**
   * Loads a ping file.
   * @param {String} aFilePath The path of the ping file.
   * @return {Promise<Object>} A promise resolved with the ping content or rejected if the
   *                           ping contains invalid data.
   */
  loadPingFile: Task.async(function* (aFilePath) {
    let array = yield OS.File.read(aFilePath);
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
   * "<timestamp>.<uuid>.<type>.json"
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
    if (extension != "json") {
      this._log.trace("_getArchivedPingDataFromFileName - should have a 'json' extension");
      return null;
    }

    // Check for a valid timestamp.
    timestamp = parseInt(timestamp);
    if (Number.isNaN(timestamp)) {
      this._log.trace("_getArchivedPingDataFromFileName - should have a valid timestamp");
      return null;
    }

    // Check for a valid UUID.
    const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
    if (!uuidRegex.test(uuid)) {
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

    if (!isPingDirectoryCreated) {
      yield OS.File.makeDir(directory, { unixMode: OS.Constants.S_IRWXU });
      isPingDirectoryCreated = true;
    }

    return directory;
  });
}

function addToPendingPings(file) {
  function onLoad(success) {
    let success_histogram = Telemetry.getHistogramById("READ_SAVED_PING_SUCCESS");
    success_histogram.add(success);
  }

  return TelemetryStorage.loadPingFile(file).then(ping => {
      pendingPings.push(ping);
      onLoad(true);
    },
    () => {
      onLoad(false);
      return OS.File.remove(file);
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
