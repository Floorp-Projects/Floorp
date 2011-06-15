/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const EXPORTED_SYMBOLS = ['Identity', 'ID'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/util.js");

// Avoid circular import.
__defineGetter__("Service", function() {
  delete this.Service;
  Cu.import("resource://services-sync/service.js", this);
  return this.Service;
});

XPCOMUtils.defineLazyGetter(this, "ID", function () {
  return new IDManager();
});

// For storing identities we'll use throughout Weave
function IDManager() {
  this._ids = {};
}
IDManager.prototype = {
  get: function IDMgr_get(name) {
    if (name in this._ids)
      return this._ids[name];
    return null;
  },
  set: function IDMgr_set(name, id) {
    this._ids[name] = id;
    return id;
  },
  del: function IDMgr_del(name) {
    delete this._ids[name];
  }
};

/*
 * Identity
 * These objects hold a realm, username, and password
 * They can hold a password in memory, but will try to fetch it from
 * the password manager if it's not set.
 * FIXME: need to rethink this stuff as part of a bigger identity mgmt framework
 */

function Identity(realm, username, password) {
  this.realm = realm;
  this.username = username;
  this._password = password;
  if (password)
    this._passwordUTF8 = Utils.encodeUTF8(password);
}
Identity.prototype = {
  get password() {
    // Look up the password then cache it
    if (this._password == null)
      for each (let login in this._logins)
        if (login.username.toLowerCase() == this.username) {
          this._password = login.password;
          this._passwordUTF8 = Utils.encodeUTF8(login.password);
        }
    return this._password;
  },

  set password(value) {
    this._password = value;
    this._passwordUTF8 = Utils.encodeUTF8(value);
  },

  get passwordUTF8() {
    if (!this._passwordUTF8)
      this.password; // invoke password getter
    return this._passwordUTF8;
  },

  persist: function persist() {
    // Clean up any existing passwords unless it's what we're persisting
    let exists = false;
    for each (let login in this._logins) {
      if (login.username == this.username && login.password == this._password)
        exists = true;
      else
        Services.logins.removeLogin(login);
    }

    // No need to create the login after clearing out the other ones
    let log = Log4Moz.repository.getLogger("Sync.Identity");
    if (exists) {
      log.trace("Skipping persist: " + this.realm + " for " + this.username);
      return;
    }

    // Add the new username/password
    log.trace("Persisting " + this.realm + " for " + this.username);
    let nsLoginInfo = new Components.Constructor(
      "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
    let newLogin = new nsLoginInfo(PWDMGR_HOST, null, this.realm,
      this.username, this.password, "", "");
    Services.logins.addLogin(newLogin);
  },

  get _logins() Services.logins.findLogins({}, PWDMGR_HOST, null, this.realm)
};
