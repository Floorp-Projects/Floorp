/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ClientID"];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");

XPCOMUtils.defineLazyGetter(this, "gDatareportingPath", () => {
  return OS.Path.join(OS.Constants.Path.profileDir, "datareporting");
});

XPCOMUtils.defineLazyGetter(this, "gStateFilePath", () => {
  return OS.Path.join(gDatareportingPath, "state.json");
});

this.ClientID = Object.freeze({
  /**
   * This returns a promise resolving to the the stable client ID we use for
   * data reporting (FHR & Telemetry). Previously exising FHR client IDs are
   * migrated to this.
   *
   * @return {Promise<string>} The stable client ID.
   */
  getClientID: function() {
    return ClientIDImpl.getClientID();
  },

  /**
   * Only used for testing. Invalidates the client ID so that it gets read
   * again from file.
   */
  _reset: function() {
    return ClientIDImpl._reset();
  },
});

let ClientIDImpl = {
  _clientID: null,
  _loadClientIdTask: null,
  _saveClientIdTask: null,

  _loadClientID: function () {
    if (this._loadClientIdTask) {
      return this._loadClientIdTask;
    }

    this._loadClientIdTask = this._doLoadClientID();
    let clear = () => this._loadClientIdTask = null;
    this._loadClientIdTask.then(clear, clear);
    return this._loadClientIdTask;
  },

  _doLoadClientID: Task.async(function* () {
    // As we want to correlate FHR and telemetry data (and move towards unifying the two),
    // we first moved the ID management from the FHR implementation to the datareporting
    // service, then to a common shared module.
    // Consequently, we try to import an existing FHR ID, so we can keep using it.

    // Try to load the client id from the DRS state file first.
    try {
      let state = yield CommonUtils.readJSON(gStateFilePath);
      if (state && 'clientID' in state && typeof(state.clientID) == 'string') {
        this._clientID = state.clientID;
        return this._clientID;
      }
    } catch (e) {
      // fall through to next option
    }

    // If we dont have DRS state yet, try to import from the FHR state.
    try {
      let fhrStatePath = OS.Path.join(OS.Constants.Path.profileDir, "healthreport", "state.json");
      let state = yield CommonUtils.readJSON(fhrStatePath);
      if (state && 'clientID' in state && typeof(state.clientID) == 'string') {
        this._clientID = state.clientID;
        this._saveClientID();
        return this._clientID;
      }
    } catch (e) {
      // fall through to next option
    }

    // We dont have an id from FHR yet, generate a new ID.
    this._clientID = CommonUtils.generateUUID();
    this._saveClientIdTask = this._saveClientID();

    // Wait on persisting the id. Otherwise failure to save the ID would result in
    // the client creating and subsequently sending multiple IDs to the server.
    // This would appear as multiple clients submitting similar data, which would
    // result in orphaning.
    yield this._saveClientIdTask;

    return this._clientID;
  }),

  /**
   * Save the client ID to the client ID file.
   *
   * @return {Promise} A promise resolved when the client ID is saved to disk.
   */
  _saveClientID: Task.async(function* () {
    let obj = { clientID: this._clientID };
    yield OS.File.makeDir(gDatareportingPath);
    yield CommonUtils.writeJSON(obj, gStateFilePath);
    this._saveClientIdTask = null;
  }),

  /**
   * This returns a promise resolving to the the stable client ID we use for
   * data reporting (FHR & Telemetry). Previously exising FHR client IDs are
   * migrated to this.
   *
   * @return {Promise<string>} The stable client ID.
   */
  getClientID: function() {
    if (!this._clientID) {
      return this._loadClientID();
    }

    return Promise.resolve(this._clientID);
  },

  /*
   * Resets the provider. This is for testing only.
   */
  _reset: Task.async(function* () {
    yield this._loadClientIdTask;
    yield this._saveClientIdTask;
    this._clientID = null;
  }),
};
