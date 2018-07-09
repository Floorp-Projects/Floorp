/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ClientID"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Log.jsm");

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "ClientID::";

ChromeUtils.defineModuleGetter(this, "CommonUtils",
                               "resource://services-common/utils.js");
ChromeUtils.defineModuleGetter(this, "OS",
                               "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyGetter(this, "gDatareportingPath", () => {
  return OS.Path.join(OS.Constants.Path.profileDir, "datareporting");
});

XPCOMUtils.defineLazyGetter(this, "gStateFilePath", () => {
  return OS.Path.join(gDatareportingPath, "state.json");
});

const PREF_CACHED_CLIENTID = "toolkit.telemetry.cachedClientID";

/**
 * Checks if client ID has a valid format.
 *
 * @param {String} id A string containing the client ID.
 * @return {Boolean} True when the client ID has valid format, or False
 * otherwise.
 */
function isValidClientID(id) {
  const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  return UUID_REGEX.test(id);
}

var ClientID = Object.freeze({
  /**
   * This returns a promise resolving to the the stable client ID we use for
   * data reporting (FHR & Telemetry).
   *
   * WARNING: This functionality is duplicated for Android (see GeckoProfile.getClientId
   * for more). There are Java tests (TestGeckoProfile) to ensure the functionality is
   * consistent and Gecko tests to come (bug 1249156). However, THIS IS NOT FOOLPROOF.
   * Be careful when changing this code and, in particular, the underlying file format.
   *
   * @return {Promise<string>} The stable client ID.
   */
  getClientID() {
    return ClientIDImpl.getClientID();
  },

/**
   * Get the client id synchronously without hitting the disk.
   * This returns:
   *  - the current on-disk client id if it was already loaded
   *  - the client id that we cached into preferences (if any)
   *  - null otherwise
   */
  getCachedClientID() {
    return ClientIDImpl.getCachedClientID();
  },

  /**
   * Only used for testing. Invalidates the client ID so that it gets read
   * again from file.
   */
  _reset() {
    return ClientIDImpl._reset();
  },
});

var ClientIDImpl = {
  _clientID: null,
  _loadClientIdTask: null,
  _saveClientIdTask: null,
  _logger: null,

  _loadClientID() {
    if (this._loadClientIdTask) {
      return this._loadClientIdTask;
    }

    this._loadClientIdTask = this._doLoadClientID();
    let clear = () => this._loadClientIdTask = null;
    this._loadClientIdTask.then(clear, clear);
    return this._loadClientIdTask;
  },

  /**
   * Load the Client ID from the DataReporting Service state file.
   * If no Client ID is found, we generate a new one.
   */
  async _doLoadClientID() {
    // Try to load the client id from the DRS state file.
    try {
      let state = await CommonUtils.readJSON(gStateFilePath);
      if (state && this.updateClientID(state.clientID)) {
        return this._clientID;
      }
    } catch (e) {
      // fall through to next option
    }

    // We dont have an id from the DRS state file yet, generate a new ID.
    this.updateClientID(CommonUtils.generateUUID());
    this._saveClientIdTask = this._saveClientID();

    // Wait on persisting the id. Otherwise failure to save the ID would result in
    // the client creating and subsequently sending multiple IDs to the server.
    // This would appear as multiple clients submitting similar data, which would
    // result in orphaning.
    await this._saveClientIdTask;

    return this._clientID;
  },

  /**
   * Save the client ID to the client ID file.
   *
   * @return {Promise} A promise resolved when the client ID is saved to disk.
   */
  async _saveClientID() {
    let obj = { clientID: this._clientID };
    await OS.File.makeDir(gDatareportingPath);
    await CommonUtils.writeJSON(obj, gStateFilePath);
    this._saveClientIdTask = null;
  },

  /**
   * This returns a promise resolving to the the stable client ID we use for
   * data reporting (FHR & Telemetry).
   *
   * @return {Promise<string>} The stable client ID.
   */
  getClientID() {
    if (!this._clientID) {
      return this._loadClientID();
    }

    return Promise.resolve(this._clientID);
  },

  /**
   * Get the client id synchronously without hitting the disk.
   * This returns:
   *  - the current on-disk client id if it was already loaded
   *  - the client id that we cached into preferences (if any)
   *  - null otherwise
   */
  getCachedClientID() {
    if (this._clientID) {
      // Already loaded the client id from disk.
      return this._clientID;
    }

    // If the client id cache contains a value of the wrong type,
    // reset the pref. We need to do this before |getStringPref| since
    // it will just return |null| in that case and we won't be able
    // to distinguish between the missing pref and wrong type cases.
    if (Services.prefs.prefHasUserValue(PREF_CACHED_CLIENTID) &&
        Services.prefs.getPrefType(PREF_CACHED_CLIENTID) != Ci.nsIPrefBranch.PREF_STRING) {
      this._log.error("getCachedClientID - invalid client id type in preferences, resetting");
      Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
    }

    // Not yet loaded, return the cached client id if we have one.
    let id = Services.prefs.getStringPref(PREF_CACHED_CLIENTID, null);
    if (id === null) {
      return null;
    }
    if (!isValidClientID(id)) {
      this._log.error("getCachedClientID - invalid client id in preferences, resetting", id);
      Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
      return null;
    }
    return id;
  },

  /*
   * Resets the provider. This is for testing only.
   */
  async _reset() {
    await this._loadClientIdTask;
    await this._saveClientIdTask;
    this._clientID = null;
  },

  /**
   * Sets the client id to the given value and updates the value cached in
   * preferences only if the given id is a valid.
   *
   * @param {String} id A string containing the client ID.
   * @return {Boolean} True when the client ID has valid format, or False
   * otherwise.
   */
  updateClientID(id) {
    if (!isValidClientID(id)) {
      this._log.error("updateClientID - invalid client ID", id);
      return false;
    }

    this._clientID = id;
    Services.prefs.setStringPref(PREF_CACHED_CLIENTID, this._clientID);
    return true;
  },

  /**
   * A helper for getting access to telemetry logger.
   */
  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    return this._logger;
  },
};
