/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["Status"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://gre/modules/Services.jsm");

this.Status = {
  _log: Log.repository.getLogger("Sync.Status"),
  _authManager: new IdentityManager(),
  ready: false,

  get service() {
    return this._service;
  },

  set service(code) {
    this._log.debug("Status.service: " + this._service + " => " + code);
    this._service = code;
  },

  get login() {
    return this._login;
  },

  set login(code) {
    this._log.debug("Status.login: " + this._login + " => " + code);
    this._login = code;

    if (code == LOGIN_FAILED_NO_USERNAME ||
        code == LOGIN_FAILED_NO_PASSWORD ||
        code == LOGIN_FAILED_NO_PASSPHRASE) {
      this.service = CLIENT_NOT_CONFIGURED;
    } else if (code != LOGIN_SUCCEEDED) {
      this.service = LOGIN_FAILED;
    } else {
      this.service = STATUS_OK;
    }
  },

  get sync() {
    return this._sync;
  },

  set sync(code) {
    this._log.debug("Status.sync: " + this._sync + " => " + code);
    this._sync = code;
    this.service = code == SYNC_SUCCEEDED ? STATUS_OK : SYNC_FAILED;
  },

  get eol() {
    let modePref = PREFS_BRANCH + "errorhandler.alert.mode";
    try {
      return Services.prefs.getCharPref(modePref) == "hard-eol";
    } catch (ex) {
      return false;
    }
  },

  get engines() {
    return this._engines;
  },

  set engines([name, code]) {
    this._log.debug("Status for engine " + name + ": " + code);
    this._engines[name] = code;

    if (code != ENGINE_SUCCEEDED) {
      this.service = SYNC_FAILED_PARTIAL;
    }
  },

  // Implement toString because adding a logger introduces a cyclic object
  // value, so we can't trivially debug-print Status as JSON.
  toString: function toString() {
    return "<Status" +
           ": login: "   + Status.login +
           ", service: " + Status.service +
           ", sync: "    + Status.sync + ">";
  },

  checkSetup: function checkSetup() {
    let result = this._authManager.currentAuthState;
    if (result == STATUS_OK) {
      Status.service = result;
      return result;
    }

    Status.login = result;
    return Status.service;
  },

  resetBackoff: function resetBackoff() {
    this.enforceBackoff = false;
    this.backoffInterval = 0;
    this.minimumNextSync = 0;
  },

  resetSync: function resetSync() {
    // Logger setup.
    let logPref = PREFS_BRANCH + "log.logger.status";
    let logLevel = "Trace";
    try {
      logLevel = Services.prefs.getCharPref(logPref);
    } catch (ex) {
      // Use default.
    }
    this._log.level = Log.Level[logLevel];

    this._log.info("Resetting Status.");
    this.service = STATUS_OK;
    this._login = LOGIN_SUCCEEDED;
    this._sync = SYNC_SUCCEEDED;
    this._engines = {};
    this.partial = false;
  }
};

// Initialize various status values.
Status.resetBackoff();
Status.resetSync();
