/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TelemetryArchive"
];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryArchive::";

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_ARCHIVE_ENABLED = PREF_BRANCH + "archive.enabled";

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStorage",
                                  "resource://gre/modules/TelemetryStorage.jsm");

this.TelemetryArchive = {
  /**
   * Get a list of the archived pings, sorted by the creation date.
   * Note that scanning the archived pings on disk is delayed on startup,
   * use promizeInitialized() to access this after scanning.
   *
   * @return {Promise<sequence<Object>>}
   *                    A list of the archived ping info in the form:
   *                    { id: <string>,
   *                      timestampCreated: <number>,
   *                      type: <string> }
   */
  promiseArchivedPingList: function() {
    return TelemetryArchiveImpl.promiseArchivedPingList();
  },

  /**
   * Load an archived ping from disk by id, asynchronously.
   *
   * @param id {String} The pings UUID.
   * @return {Promise<PingData>} A promise resolved with the pings data on success.
   */
  promiseArchivedPingById: function(id) {
    return TelemetryArchiveImpl.promiseArchivedPingById(id);
  },

  /**
   * Archive a ping and persist it to disk.
   *
   * @param {object} ping The ping data to archive.
   * @return {promise} Promise that is resolved when the ping is successfully archived.
   */
  promiseArchivePing: function(ping) {
    return TelemetryArchiveImpl.promiseArchivePing(ping);
  },

  /**
   * Used in tests only to fake a restart of the module.
   */
  _testReset: function() {
    TelemetryArchiveImpl._testReset();
  },
};

/**
 * Checks if pings can be archived. Some products (e.g. Thunderbird) might not want
 * to do that.
 * @return {Boolean} True if pings should be archived, false otherwise.
 */
function shouldArchivePings() {
  return Preferences.get(PREF_ARCHIVE_ENABLED, false);
}

let TelemetryArchiveImpl = {
  _logger: null,

  // Tracks the archived pings in a Map of (id -> {timestampCreated, type}).
  // We use this to cache info on archived pings to avoid scanning the disk more than once.
  _archivedPings: new Map(),
  // Whether we already scanned the archived pings on disk.
  _scannedArchiveDirectory: false,

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    return this._logger;
  },

  _testReset: function() {
    this._archivedPings = new Map();
    this._scannedArchiveDirectory = false;
  },

  promiseArchivePing: function(ping) {
    if (!shouldArchivePings()) {
      this._log.trace("promiseArchivePing - archiving is disabled");
      return Promise.resolve();
    }

    for (let field of ["creationDate", "id", "type"]) {
      if (!(field in ping)) {
        this._log.warn("promiseArchivePing - missing field " + field)
        return Promise.reject(new Error("missing field " + field));
      }
    }

    const creationDate = new Date(ping.creationDate);
    if (this._archivedPings.has(ping.id)) {
      const data = this._archivedPings.get(ping.id);
      if (data.timestampCreated > creationDate.getTime()) {
        this._log.error("promiseArchivePing - trying to overwrite newer ping with the same id");
        return Promise.reject(new Error("trying to overwrite newer ping with the same id"));
      } else {
        this._log.warn("promiseArchivePing - overwriting older ping with the same id");
      }
    }

    this._archivedPings.set(ping.id, {
      timestampCreated: creationDate.getTime(),
      type: ping.type,
    });

    return TelemetryStorage.saveArchivedPing(ping);
  },

  _buildArchivedPingList: function() {
    let list = [for (p of this._archivedPings) {
      id: p[0],
      timestampCreated: p[1].timestampCreated,
      type: p[1].type,
    }];

    list.sort((a, b) => a.timestampCreated - b.timestampCreated);

    return list;
  },

  promiseArchivedPingList: function() {
    this._log.trace("promiseArchivedPingList");

    if (this._scannedArchiveDirectory) {
      return Promise.resolve(this._buildArchivedPingList())
    }

    return TelemetryStorage.loadArchivedPingList().then((loadedInfo) => {
      // Add the ping info from scanning to the existing info.
      // We might have pings added before lazily loading this list.
      for (let [id, info] of loadedInfo) {
        this._log.trace("promiseArchivedPingList - id: " + id + ", info: " + info);
        this._archivedPings.set(id, {
          timestampCreated: info.timestampCreated,
          type: info.type,
        });
      }

      this._scannedArchiveDirectory = true;
      return this._buildArchivedPingList();
    });
  },

  promiseArchivedPingById: function(id) {
    this._log.trace("promiseArchivedPingById - id: " + id);
    const data = this._archivedPings.get(id);
    if (!data) {
      this._log.trace("promiseArchivedPingById - no ping with id: " + id);
      return Promise.reject(new Error("TelemetryArchive.promiseArchivedPingById - no ping with id " + id));
    }

    return TelemetryStorage.loadArchivedPing(id, data.timestampCreated, data.type);
  },
};
