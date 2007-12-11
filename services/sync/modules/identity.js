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

const EXPORTED_SYMBOLS = ['Identity'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");

/*
 * Identity
 * These objects hold a realm, username, and password
 * They can hold a password in memory, but will try to fetch it from
 * the password manager if it's not set.
 * FIXME: need to rethink this stuff as part of a bigger identity mgmt framework
 */

function Identity(realm, username, password) {
  this._realm = realm;
  this._username = username;
  this._password = password;
}
Identity.prototype = {
  get realm() { return this._realm; },
  set realm(value) { this._realm = value; },

  get username() { return this._username; },
  set username(value) { this._username = value; },

  _password: null,
  get password() {
    if (this._password === null)
      return findPassword(this.realm, this.username);
    return this._password;
  },
  set password(value) {
    setPassword(this.realm, this.username, value);
  },

  setTempPassword: function Id_setTempPassword(value) {
    this._password = value;
  }
};

// fixme: move these to util.js?
function findPassword(realm, username) {
  // fixme: make a request and get the realm ?
  let password;
  let lm = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
  let logins = lm.findLogins({}, 'chrome://sync', null, realm);

  for (let i = 0; i < logins.length; i++) {
    if (logins[i].username == username) {
      password = logins[i].password;
      break;
    }
  }
  return password;
}

function setPassword(realm, username, password) {
  // cleanup any existing passwords
  let lm = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
  let logins = lm.findLogins({}, 'chrome://sync', null, realm);
  for(let i = 0; i < logins.length; i++) {
    lm.removeLogin(logins[i]);
  }

  if (!password)
    return;

  // save the new one
  let nsLoginInfo = new Components.Constructor(
    "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
  let login = new nsLoginInfo('chrome://sync', null, realm,
                              username, password, null, null);
  lm.addLogin(login);
}
