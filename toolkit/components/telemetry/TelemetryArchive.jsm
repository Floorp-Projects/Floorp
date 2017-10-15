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
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryArchive::";

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
  promiseArchivedPingList() {
    return TelemetryArchiveImpl.promiseArchivedPingList();
  },

  /**
   * Load an archived ping from disk by id, asynchronously.
   *
   * @param id {String} The pings UUID.
   * @return {Promise<PingData>} A promise resolved with the pings data on success.
   */
  promiseArchivedPingById(id) {
    return TelemetryArchiveImpl.promiseArchivedPingById(id);
  },

  /**
   * Archive a ping and persist it to disk.
   *
   * @param {object} ping The ping data to archive.
   * @return {promise} Promise that is resolved when the ping is successfully archived.
   */
  promiseArchivePing(ping) {
    return TelemetryArchiveImpl.promiseArchivePing(ping);
  },
};

/**
 * Checks if pings can be archived. Some products (e.g. Thunderbird) might not want
 * to do that.
 * @return {Boolean} True if pings should be archived, false otherwise.
 */
function shouldArchivePings() {
  return Preferences.get(TelemetryUtils.Preferences.ArchiveEnabled, false);
}

var TelemetryArchiveImpl = {
  _logger: null,

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    return this._logger;
  },

  promiseArchivePing(ping) {
    if (!shouldArchivePings()) {
      this._log.trace("promiseArchivePing - archiving is disabled");
      return Promise.resolve();
    }

    for (let field of ["creationDate", "id", "type"]) {
      if (!(field in ping)) {
        this._log.warn("promiseArchivePing - missing field " + field);
        return Promise.reject(new Error("missing field " + field));
      }
    }

    return TelemetryStorage.saveArchivedPing(ping);
  },

  _buildArchivedPingList(archivedPingsMap) {
    let list = Array.from(archivedPingsMap, p => ({
      id: p[0],
      timestampCreated: p[1].timestampCreated,
      type: p[1].type,
    }));

    list.sort((a, b) => a.timestampCreated - b.timestampCreated);

    return list;
  },

  promiseArchivedPingList() {
    this._log.trace("promiseArchivedPingList");

    return TelemetryStorage.loadArchivedPingList().then(loadedInfo => {
      return this._buildArchivedPingList(loadedInfo);
    });
  },

  promiseArchivedPingById(id) {
    this._log.trace("promiseArchivedPingById - id: " + id);
    return TelemetryStorage.loadArchivedPing(id);
  },
};
