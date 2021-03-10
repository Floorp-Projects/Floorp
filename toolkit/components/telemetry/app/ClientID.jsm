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

const SCALAR_DELETION_REQUEST_ECOSYSTEM_CLIENT_ID =
  "deletion.request.ecosystem_client_id";

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
   * Returns a promise resolving to the ecosystem client ID, used in ecosystem
   * pings as a stable identifier for this profile.
   *
   * @return {Promise<string>} The ecosystem client ID.
   */
  getEcosystemClientID() {
    return ClientIDImpl.getEcosystemClientID();
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

  /**
   * Gets the in-memory cached ecosystem client ID if it was already loaded;
   * `null` otherwise.
   */
  getCachedEcosystemClientID() {
    return ClientIDImpl.getCachedEcosystemClientID();
  },

  async getClientIdHash() {
    return ClientIDImpl.getClientIdHash();
  },

  /**
   * Sets the main and ecosystem client IDs to the canary (known) client ID,
   * writing them to disk and updating the cached versions.
   *
   * Use `removeClientIDs` followed by `get{Ecosystem}ClientID` to clear the
   * existing IDs and generate new, random ones if required.
   *
   * @return {Promise<void>}
   */
  setCanaryClientIDs() {
    return ClientIDImpl.setCanaryClientIDs();
  },

  /**
   * Sets the ecosystem client IDs to a new random value while leaving other IDs
   * unchanged, writing the result to disk and updating the cached identifier.
   * This can be used when a user signs out, to avoid linking telemetry between
   * different accounts.
   *
   * Use `removeClientIDs` followed by `get{Ecosystem}ClientID` to reset *all* the
   * identifiers rather than just the ecosystem client id.
   *
   * @return {Promise<void>} Resolves when the change has been saved to disk.
   */
  resetEcosystemClientID() {
    return ClientIDImpl.resetEcosystemClientID();
  },

  /**
   * Clears the main and ecosystem client IDs asynchronously, removing them
   * from disk. Use `getClientID()` and `getEcosystemClientID()` to generate
   * fresh IDs after calling this method.
   *
   * Should only be used if a reset is explicitly requested by the user.
   *
   * @return {Promise<void>}
   */
  removeClientIDs() {
    return ClientIDImpl.removeClientIDs();
  },

  /**
   * Only used for testing. Invalidates the cached client IDs so that they're
   * read again from file, but doesn't remove the existing IDs from disk.
   */
  _reset() {
    return ClientIDImpl._reset();
  },
});

var ClientIDImpl = {
  _clientID: null,
  _clientIDHash: null,
  _ecosystemClientID: null,
  _loadClientIdsTask: null,
  _saveClientIdsTask: null,
  _removeClientIdsTask: null,
  _logger: null,
  _wasCanary: null,

  _loadClientIDs() {
    if (this._loadClientIdsTask) {
      return this._loadClientIdsTask;
    }

    this._loadClientIdsTask = this._doLoadClientIDs();
    let clear = () => (this._loadClientIdsTask = null);
    this._loadClientIdsTask.then(clear, clear);
    return this._loadClientIdsTask;
  },

  /**
   * Load the client IDs (Telemetry Client ID and Ecosystem Client ID) from the
   * DataReporting Service state file. If either ID is missing, we generate a
   * new one.
   */
  async _doLoadClientIDs() {
    this._log.trace(`_doLoadClientIDs`);
    // If there's a removal in progress, let's wait for it
    await this._removeClientIdsTask;

    // Try to load the client id from the DRS state file.
    let hasCurrentClientID = false;
    let hasCurrentEcosystemClientID = false;
    try {
      let state = await CommonUtils.readJSON(gStateFilePath);
      if (AppConstants.platform == "android" && state && "wasCanary" in state) {
        this._wasCanary = state.wasCanary;
      }
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
        hasCurrentEcosystemClientID = this.updateEcosystemClientID(
          state.ecosystemClientID
        );
        if (hasCurrentClientID && hasCurrentEcosystemClientID) {
          this._log.trace(`_doLoadClientIDs: Client IDs loaded from state.`);
          return {
            clientID: this._clientID,
            ecosystemClientID: this._ecosystemClientID,
          };
        }
      }
    } catch (e) {
      // fall through to next option
    }

    // We're missing one or both IDs from the DRS state file, generate new ones.
    if (!hasCurrentClientID) {
      Services.telemetry.scalarSet("telemetry.generated_new_client_id", true);
      this.updateClientID(CommonUtils.generateUUID());
    }
    if (!hasCurrentEcosystemClientID) {
      this.updateEcosystemClientID(CommonUtils.generateUUID());
    }
    this._saveClientIdsTask = this._saveClientIDs();

    // Wait on persisting the id. Otherwise failure to save the ID would result in
    // the client creating and subsequently sending multiple IDs to the server.
    // This would appear as multiple clients submitting similar data, which would
    // result in orphaning.
    await this._saveClientIdsTask;

    this._log.trace("_doLoadClientIDs: New client IDs loaded and persisted.");
    return {
      clientID: this._clientID,
      ecosystemClientID: this._ecosystemClientID,
    };
  },

  /**
   * Save the client ID to the client ID file.
   *
   * @return {Promise} A promise resolved when the client ID is saved to disk.
   */
  async _saveClientIDs() {
    try {
      this._log.trace(`_saveClientIDs`);
      let obj = {
        clientID: this._clientID,
        ecosystemClientID: this._ecosystemClientID,
      };
      // We detected a canary client ID when resetting, storing this as a flag
      if (AppConstants.platform == "android" && this._wasCanary) {
        obj.wasCanary = true;
      }
      try {
        await IOUtils.makeDirectory(gDatareportingPath);
      } catch (ex) {
        if (ex.name != "NotAllowedError") {
          throw ex;
        }
      }
      await CommonUtils.writeJSON(obj, gStateFilePath);
      this._saveClientIdsTask = null;
    } catch (ex) {
      Services.telemetry.scalarAdd("telemetry.state_file_save_errors", 1);
      throw ex;
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
      let { clientID } = await this._loadClientIDs();
      return clientID;
    }

    return Promise.resolve(this._clientID);
  },

  async getEcosystemClientID() {
    if (!this._ecosystemClientID) {
      let { ecosystemClientID } = await this._loadClientIDs();
      return ecosystemClientID;
    }

    return Promise.resolve(this._ecosystemClientID);
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

  getCachedEcosystemClientID() {
    return this._ecosystemClientID;
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
    await this._loadClientIdsTask;
    await this._saveClientIdsTask;
    this._clientID = null;
    this._clientIDHash = null;
    this._ecosystemClientID = null;
  },

  async setCanaryClientIDs() {
    this._log.trace("setCanaryClientIDs");
    this.updateClientID(CANARY_CLIENT_ID);
    this.updateEcosystemClientID(CANARY_CLIENT_ID);

    this._saveClientIdsTask = this._saveClientIDs();
    await this._saveClientIdsTask;
    return this._clientID;
  },

  async resetEcosystemClientID() {
    this._log.trace("resetEcosystemClientID");
    this.updateEcosystemClientID(CommonUtils.generateUUID());
    this._saveClientIdsTask = this._saveClientIDs();
    await this._saveClientIdsTask;
    return this._ecosystemClientID;
  },

  async _doRemoveClientIDs() {
    this._log.trace("_doRemoveClientIDs");

    // Reset the cached main and ecosystem client IDs.
    this._clientID = null;
    this._clientIDHash = null;
    this._ecosystemClientID = null;

    // Clear the client id from the preference cache.
    Services.prefs.clearUserPref(PREF_CACHED_CLIENTID);

    // Clear the old ecosystem client ID from the deletion request scalar store.
    Services.telemetry.scalarSet(
      SCALAR_DELETION_REQUEST_ECOSYSTEM_CLIENT_ID,
      ""
    );

    // If there is a save in progress, wait for it to complete.
    await this._saveClientIdsTask;

    // Remove the client id from disk
    await IOUtils.remove(gStateFilePath);
  },

  async removeClientIDs() {
    this._log.trace("removeClientIDs");
    let oldClientId = this._clientID;

    // Wait for the removal.
    // Asynchronous calls to getClientID will also be blocked on this.
    this._removeClientIdsTask = this._doRemoveClientIDs();
    let clear = () => (this._removeClientIdsTask = null);
    this._removeClientIdsTask.then(clear, clear);

    await this._removeClientIdsTask;

    // On Android we detect resets after a canary client ID.
    if (AppConstants.platform == "android") {
      this._wasCanary = oldClientId == CANARY_CLIENT_ID;
    }
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

  updateEcosystemClientID(id) {
    if (!isValidClientID(id)) {
      this._log.error(
        "updateEcosystemClientID - invalid ecosystem client ID",
        id
      );
      return false;
    }
    this._ecosystemClientID = id;
    Services.telemetry.scalarSet(
      SCALAR_DELETION_REQUEST_ECOSYSTEM_CLIENT_ID,
      id
    );
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
