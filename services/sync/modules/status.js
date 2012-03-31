/***************************** BEGIN LICENSE BLOCK *****************************
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with the
* License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
* the specific language governing rights and limitations under the License.
*
* The Original Code is Weave Status.
*
* The Initial Developer of the Original Code is Mozilla Corporation.
* Portions created by the Initial Developer are Copyright (C) 2009 the Initial
* Developer. All Rights Reserved.
*
* Contributor(s):
*  Edward Lee <edilee@mozilla.com> (original author)
*  Richard Newman <rnewman@mozilla.com>
*
* Alternatively, the contents of this file may be used under the terms of either
* the GNU General Public License Version 2 or later (the "GPL"), or the GNU
* Lesser General Public License Version 2.1 or later (the "LGPL"), in which case
* the provisions of the GPL or the LGPL are applicable instead of those above.
* If you wish to allow use of your version of this file only under the terms of
* either the GPL or the LGPL, and not to allow others to use your version of
* this file under the terms of the MPL, indicate your decision by deleting the
* provisions above and replace them with the notice and other provisions
* required by the GPL or the LGPL. If you do not delete the provisions above, a
* recipient may use your version of this file under the terms of any one of the
* MPL, the GPL or the LGPL.
*
****************************** END LICENSE BLOCK ******************************/

const EXPORTED_SYMBOLS = ["Status"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://gre/modules/Services.jsm");

let Status = {
  _log: Log4Moz.repository.getLogger("Sync.Status"),
  _authManager: Identity,
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
    this._log.level = Log4Moz.Level[logLevel];

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
