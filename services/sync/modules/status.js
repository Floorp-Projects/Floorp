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

let Status = {
  get login() this._login,
  set login(code) {
    this._login = code;

    if (code == LOGIN_FAILED_NO_USERNAME || 
        code == LOGIN_FAILED_NO_PASSWORD || 
        code == LOGIN_FAILED_NO_PASSPHRASE)
      this.service = CLIENT_NOT_CONFIGURED;      
    else if (code != LOGIN_SUCCEEDED)
      this.service = LOGIN_FAILED;
    else
      this.service = STATUS_OK;
  },

  get sync() this._sync,
  set sync(code) {
    this._sync = code;
    this.service = code == SYNC_SUCCEEDED ? STATUS_OK : SYNC_FAILED;
  },

  get engines() this._engines,
  set engines([name, code]) {
    this._engines[name] = code;

    if (code != ENGINE_SUCCEEDED)
      this.service = SYNC_FAILED_PARTIAL;
  },

  checkSetup: function checkSetup() {
    // Check whether we have a username without importing The World(tm).
    let prefs = Cc["@mozilla.org/preferences-service;1"]
                .getService(Ci.nsIPrefService)
                .getBranch(PREFS_BRANCH);
    let username;
    try {
      username = prefs.getCharPref("username");
    } catch(ex) {}
    if (!username) {
      Status.login = LOGIN_FAILED_NO_USERNAME;
      return Status.service;
    }

    Cu.import("resource://services-sync/util.js");
    Cu.import("resource://services-sync/identity.js");
    if (!Utils.mpLocked()) {
      let id = ID.get("WeaveID");
      if (!id)
        id = ID.set("WeaveID", new Identity(PWDMGR_PASSWORD_REALM, username));

      if (!id.password) {
        Status.login = LOGIN_FAILED_NO_PASSWORD;
        return Status.service;
      }

      id = ID.get("WeaveCryptoID");
      if (!id)
        id = ID.set("WeaveCryptoID",
                    new Identity(PWDMGR_PASSPHRASE_REALM, username));

      if (!id.password) {
        Status.login = LOGIN_FAILED_NO_PASSPHRASE;
        return Status.service;
      }
    }
    return Status.service = STATUS_OK;
  },

  resetBackoff: function resetBackoff() {
    this.enforceBackoff = false;
    this.backoffInterval = 0;
    this.minimumNextSync = 0;
  },
  resetSync: function resetSync() {
    this.service = STATUS_OK;
    this._login = LOGIN_SUCCEEDED;
    this._sync = SYNC_SUCCEEDED;
    this._engines = {};
    this.partial = false;
  },
};

// Initialize various status values
Status.resetBackoff();
Status.resetSync();
