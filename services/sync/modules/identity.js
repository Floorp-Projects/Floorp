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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");

Utils.lazy(this, 'ID', IDManager);

// For storing identities we'll use throughout Weave
function IDManager() {
  this._ids = {};
  this._aliases = {};
}
IDManager.prototype = {
  get: function IDMgr_get(name) {
    if (name in this._aliases)
      return this._ids[this._aliases[name]];
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
  },
  getAlias: function IDMgr_getAlias(alias) {
    return this._aliases[alias];
  },
  setAlias: function IDMgr_setAlias(name, alias) {
    this._aliases[alias] = name;
  },
  delAlias: function IDMgr_delAlias(alias) {
    delete this._aliases[alias];
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
  this.realm     = realm;
  this.username  = username;
  this._password = password;
}
Identity.prototype = {
  realm   : null,

  // Only the "WeaveCryptoID" realm uses these:
  privkey        : null,
  pubkey         : null,
  passphraseSalt : null,
  privkeyWrapIV  : null,

  // Only the per-engine identity uses these:
  bulkKey : null,
  bulkIV  : null,

  username : null,
  get userHash() { return Utils.sha1(this.username); },

  _password: null,
  get password() {
    if (!this._password)
      return Utils.findPassword(this.realm, this.username);
    return this._password;
  },
  set password(value) {
    Utils.setPassword(this.realm, this.username, value);
  },

  setTempPassword: function Id_setTempPassword(value) {
    this._password = value;
  }
};
