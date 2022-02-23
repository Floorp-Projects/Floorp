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

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "ClientID::";
// Must match ID in TelemetryUtils
const CANARY_CLIENT_ID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0";

ChromeUtils.defineModuleGetter(
  this,
  "CommonUtils",
  "resource://services-common/utils.js"
);

XPCOMUtils.defineLazyGetter(this, "CryptoHash", () => {
  return Components.Constructor(
    "@mozilla.org/security/hash;1",
    "nsICryptoHash",
    "initWithString"
  );
});

XPCOMUtils.defineLazyGetter(this, "gDatareportingPath", () => {
  return PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "datareporting"
  );
});

XPCOMUtils.defineLazyGetter(this, "gStateFilePath", () => {
  return PathUtils.join(gDatareportingPath, "state.json");
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

  async getClientIdHash() {
    return ClientIDImpl.getClientIdHash();
  },

  /**
   * Sets the client ID to the canary (known) client ID,
   * writing it to disk and updating the cached version.
   *
   * Use `removeClientID` followed by `getClientID` to clear the
   * existing ID and generate a new, random one if required.
   *
   * @return {Promise<void>}
   */
  setCanaryClientID() {
    return ClientIDImpl.setCanaryClientID();
  },

  /**
   * Clears the client ID asynchronously, removing it
   * from disk. Use `getClientID()` to generate
   * a fresh ID after calling this method.
   *
   * Should only be used if a reset is explicitly requested by the user.
   *
   * @return {Promise<void>}
   */
  removeClientID() {
    return ClientIDImpl.removeClientID();
  },

  /**
   * Only used for testing. Invalidates the cached client ID so that it is
   * read again from file, but doesn't remove the existing ID from disk.
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
   * Load the client ID from the DataReporting Service state file. If it is
   * missing, we generate a new one.
   */
  async _doLoadClientID() {
    this._log.trace(`_doLoadClientID`);
    // If there's a removal in progress, let's wait for it
    await this._removeClientIdTask;

    // Try to load the client id from the DRS state file.
    let hasCurrentClientID = false;
    try {
      let state = await IOUtils.readJSON(gStateFilePath);
      if (state) {
        try {
          if (Services.prefs.prefHasUserValue(PREF_CACHED_CLIENTID)) {
            let cachedID = Services.prefs.getStringPref(
              PREF_CACHED_CLIENTID,
              null
            );
            if (cachedID && cachedID != state.clientID) {
              Services.telemetry.scalarAdd(
                "telemetry.loaded_client_id_doesnt_match_pref",
                1
              );
            }
          }
        } catch (e) {
          // This data collection's not that important.
        }
        hasCurrentClientID = this.updateClientID(state.clientID);
        if (hasCurrentClientID) {
          this._log.trace(`_doLoadClientID: Client IDs loaded from state.`);
          return {
            clientID: this._clientID,
          };
        }
      }
    } catch (e) {
      Services.telemetry.scalarAdd("telemetry.state_file_read_errors", 1);
      // fall through to next option
    }

    // Absent or broken state file? Check prefs as last resort.
    if (!hasCurrentClientID) {
      const cachedID = this.getCachedClientID();
      // Calling `updateClientID` with `null` logs an error, which breaks tests.
      if (cachedID) {
        Services.telemetry.scalarAdd("telemetry.using_pref_client_id", 1);
        hasCurrentClientID = this.updateClientID(cachedID);
      }
    }

    // We're missing the ID from the DRS state file and prefs.
    // Generate a new one.
    if (!hasCurrentClientID) {
      Services.telemetry.scalarSet("telemetry.generated_new_client_id", true);
      this.updateClientID(CommonUtils.generateUUID());
    }
    this._saveClientIdTask = this._saveClientID();

    // Wait on persisting the id. Otherwise failure to save the ID would result in
    // the client creating and subsequently sending multiple IDs to the server.
    // This would appear as multiple clients submitting similar data, which would
    // result in orphaning.
    await this._saveClientIdTask;

    this._log.trace("_doLoadClientID: New client ID loaded and persisted.");
    return {
      clientID: this._clientID,
    };
  },

  /**
   * Save the client ID to the client ID file.
   *
   * @return {Promise} A promise resolved when the client ID is saved to disk.
   */
  async _saveClientID() {
    try {
      this._log.trace(`_saveClientID`);
      let obj = {
        clientID: this._clientID,
      };
      await IOUtils.makeDirectory(gDatareportingPath);
      await IOUtils.writeJSON(gStateFilePath, obj, {
        tmpPath: `${gStateFilePath}.tmp`,
      });
      this._saveClientIdTask = null;
    } catch (ex) {
      Services.telemetry.scalarAdd("telemetry.state_file_save_errors", 1);

      if (!(ex instanceof DOMException) || ex.name !== "AbortError") {
        throw ex;
      }
    }
  },

  /**
   * This returns a promise resolving to the the stable client ID we use for
   * data reporting (FHR & Telemetry).
   *
   * @return {Promise<string>} The stable client ID.
   */
  async getClientID() {
    if (!this._clientID) {
      let { clientID } = await this._loadClientID();
      return clientID;
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
   * Resets the module. This is for testing only.
   */
  async _reset() {
    await this._loadClientIdTask;
    await this._saveClientIdTask;
    this._clientID = null;
    this._clientIDHash = null;
  },

  async setCanaryClientID() {
    this._log.trace("setCanaryClientID");
    this.updateClientID(CANARY_CLIENT_ID);

    this._saveClientIdTask = this._saveClientID();
    await this._saveClientIdTask;
    return this._clientID;
  },

  async _doRemoveClientID() {
    this._log.trace("_doRemoveClientID");

    // Reset the cached client ID.
    this._clientID = null;
    this._clientIDHash = null;

    // Clear the client id from the preference cache.
    Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);

    // If there is a save in progress, wait for it to complete.
    await this._saveClientIdTask;

    // Remove the client-id-containing state file from disk
    await IOUtils.remove(gStateFilePath);
  },

  async removeClientID() {
    this._log.trace("removeClientID");
    Services.telemetry.scalarAdd("telemetry.removed_client_ids", 1);

    // Wait for the removal.
    // Asynchronous calls to getClientID will also be blocked on this.
    this._removeClientIdTask = this._doRemoveClientID();
    let clear = () => (this._removeClientIdTask = null);
    this._removeClientIdTask.then(clear, clear);

    await this._removeClientIdTask;
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
