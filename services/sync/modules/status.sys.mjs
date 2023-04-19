/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  CLIENT_NOT_CONFIGURED,
  ENGINE_SUCCEEDED,
  LOGIN_FAILED,
  LOGIN_FAILED_NO_PASSPHRASE,
  LOGIN_FAILED_NO_USERNAME,
  LOGIN_SUCCEEDED,
  STATUS_OK,
  SYNC_FAILED,
  SYNC_FAILED_PARTIAL,
  SYNC_SUCCEEDED,
} from "resource://services-sync/constants.sys.mjs";

import { Log } from "resource://gre/modules/Log.sys.mjs";

import { SyncAuthManager } from "resource://services-sync/sync_auth.sys.mjs";

export var Status = {
  _log: Log.repository.getLogger("Sync.Status"),
  __authManager: null,
  ready: false,

  get _authManager() {
    if (this.__authManager) {
      return this.__authManager;
    }
    this.__authManager = new SyncAuthManager();
    return this.__authManager;
  },

  get service() {
    return this._service;
  },

  set service(code) {
    this._log.debug(
      "Status.service: " + (this._service || undefined) + " => " + code
    );
    this._service = code;
  },

  get login() {
    return this._login;
  },

  set login(code) {
    this._log.debug("Status.login: " + this._login + " => " + code);
    this._login = code;

    if (
      code == LOGIN_FAILED_NO_USERNAME ||
      code == LOGIN_FAILED_NO_PASSPHRASE
    ) {
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
    return (
      "<Status" +
      ": login: " +
      Status.login +
      ", service: " +
      Status.service +
      ", sync: " +
      Status.sync +
      ">"
    );
  },

  checkSetup: function checkSetup() {
    if (!this._authManager.username) {
      Status.login = LOGIN_FAILED_NO_USERNAME;
      Status.service = CLIENT_NOT_CONFIGURED;
    } else if (Status.login == STATUS_OK) {
      Status.service = STATUS_OK;
    }
    return Status.service;
  },

  resetBackoff: function resetBackoff() {
    this.enforceBackoff = false;
    this.backoffInterval = 0;
    this.minimumNextSync = 0;
  },

  resetSync: function resetSync() {
    // Logger setup.
    this._log.manageLevelFromPref("services.sync.log.logger.status");

    this._log.info("Resetting Status.");
    this.service = STATUS_OK;
    this._login = LOGIN_SUCCEEDED;
    this._sync = SYNC_SUCCEEDED;
    this._engines = {};
    this.partial = false;
  },
};

// Initialize various status values.
Status.resetBackoff();
Status.resetSync();
