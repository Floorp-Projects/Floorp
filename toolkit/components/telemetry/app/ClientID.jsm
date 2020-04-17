/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ClientID"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "ClientID::";
// Must match ID in TelemetryUtils
const CANARY_CLIENT_ID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0";

ChromeUtils.defineModuleGetter(
  this,
  "CommonUtils",
  "resource://services-common/utils.js"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyGetter(this, "CryptoHash", () => {
  return Components.Constructor(
    "@mozilla.org/security/hash;1",
    "nsICryptoHash",
    "initWithString"
  );
});

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
   * This returns true if the client ID prior to the last client ID reset was a canary client ID.
   * Android only. Always returns null on Desktop.
   */
  wasCanaryClientID() {
    if (AppConstants.platform == "android") {
      return ClientIDImpl.wasCanaryClientID();
    }

    return null;
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

  async getClientIdHash() {
    return ClientIDImpl.getClientIdHash();
  },

  /**
   * Set a specific client id asynchronously, writing it to disk
   * and updating the cached version.
   *
   * Should only ever be used when a known client ID value should be set.
   * Use `resetClientID` to generate a new random one if required.
   *
   * @return {Promise<string>} The stable client ID.
   */
  setClientID(id) {
    return ClientIDImpl.setClientID(id);
  },

  /**
   * Reset the client id asynchronously, writing it to disk
   * and updating the cached version.
   *
   * Should only be used if a reset is explicitely requested by the user.
   *
   * @return {Promise<string>} A new stable client ID.
   */
  resetClientID() {
    return ClientIDImpl.resetClientID();
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
  _clientIDHash: null,
  _loadClientIdTask: null,
  _saveClientIdTask: null,
  _removeClientIdTask: null,
  _logger: null,
  _wasCanary: null,

  _loadClientID() {
    if (this._loadClientIdTask) {
      return this._loadClientIdTask;
    }

    this._loadClientIdTask = this._doLoadClientID();
    let clear = () => (this._loadClientIdTask = null);
    this._loadClientIdTask.then(clear, clear);
    return this._loadClientIdTask;
  },

  /**
   * Load the Client ID from the DataReporting Service state file.
   * If no Client ID is found, we generate a new one.
   */
  async _doLoadClientID() {
    this._log.trace(`_doLoadClientID`);
    // If there's a removal in progress, let's wait for it
    await this._removeClientIdTask;

    // Try to load the client id from the DRS state file.
    try {
      let state = await CommonUtils.readJSON(gStateFilePath);
      if (AppConstants.platform == "android" && state && "wasCanary" in state) {
        this._wasCanary = state.wasCanary;
      }
      if (state && this.updateClientID(state.clientID)) {
        this._log.trace(`_doLoadClientID: Client id loaded from state.`);
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

    this._log.trace("_doLoadClientID: New client id loaded and persisted.");
    return this._clientID;
  },

  /**
   * Save the client ID to the client ID file.
   *
   * @return {Promise} A promise resolved when the client ID is saved to disk.
   */
  async _saveClientID() {
    this._log.trace(`_saveClientID`);
    let obj = { clientID: this._clientID };
    // We detected a canary client ID when resetting, storing this as a flag
    if (AppConstants.platform == "android" && this._wasCanary) {
      obj.wasCanary = true;
    }
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
   * This returns true if the client ID prior to the last client ID reset was a canary client ID.
   * Android only. Always returns null on Desktop.
   */
  wasCanaryClientID() {
    return this._wasCanary;
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
    if (
      Services.prefs.prefHasUserValue(PREF_CACHED_CLIENTID) &&
      Services.prefs.getPrefType(PREF_CACHED_CLIENTID) !=
        Ci.nsIPrefBranch.PREF_STRING
    ) {
      this._log.error(
        "getCachedClientID - invalid client id type in preferences, resetting"
      );
      Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
    }

    // Not yet loaded, return the cached client id if we have one.
    let id = Services.prefs.getStringPref(PREF_CACHED_CLIENTID, null);
    if (id === null) {
      return null;
    }
    if (!isValidClientID(id)) {
      this._log.error(
        "getCachedClientID - invalid client id in preferences, resetting",
        id
      );
      Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);
      return null;
    }
    return id;
  },

  async getClientIdHash() {
    if (!this._clientIDHash) {
      let byteArr = new TextEncoder().encode(await this.getClientID());
      let hash = new CryptoHash("sha256");
      hash.update(byteArr, byteArr.length);
      this._clientIDHash = CommonUtils.bytesAsHex(hash.finish(false));
    }
    return this._clientIDHash;
  },

  /*
   * Resets the provider. This is for testing only.
   */
  async _reset() {
    await this._loadClientIdTask;
    await this._saveClientIdTask;
    this._clientID = null;
    this._clientIDHash = null;
  },

  async setClientID(id) {
    this._log.trace("setClientID");
    if (!this.updateClientID(id)) {
      throw new Error("Invalid client ID: " + id);
    }

    this._saveClientIdTask = this._saveClientID();
    await this._saveClientIdTask;
    return this._clientID;
  },

  async _doRemoveClientID() {
    this._log.trace("_doRemoveClientID");

    // Reset stored id.
    this._clientID = null;
    this._clientIDHash = null;

    // Clear the client id from the preference cache.
    Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);

    // If there is a save in progress, wait for it to complete.
    await this._saveClientIdTask;

    // Remove the client id from disk
    await OS.File.remove(gStateFilePath, { ignoreAbsent: true });
  },

  async resetClientID() {
    this._log.trace("resetClientID");
    let oldClientId = this._clientID;

    // Wait for the removal.
    // Asynchronous calls to getClientID will also be blocked on this.
    this._removeClientIdTask = this._doRemoveClientID();
    let clear = () => (this._removeClientIdTask = null);
    this._removeClientIdTask.then(clear, clear);

    await this._removeClientIdTask;

    // On Android we detect resets after a canary client ID.
    if (AppConstants.platform == "android") {
      this._wasCanary = oldClientId == CANARY_CLIENT_ID;
    }

    // Generate a new id.
    return this.getClientID();
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
    this._clientIDHash = null;
    Services.prefs.setStringPref(PREF_CACHED_CLIENTID, this._clientID);
    return true;
  },

  /**
   * A helper for getting access to telemetry logger.
   */
  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX
      );
    }

    return this._logger;
  },
};
