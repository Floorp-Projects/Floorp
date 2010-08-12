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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Dolske <dolske@mozilla.com>
 *  Anant Narayanan <anant@kix.in>
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

const EXPORTED_SYMBOLS = ['PasswordEngine'];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://services-sync/base_records/collection.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/stores.js");
Cu.import("resource://services-sync/trackers.js");
Cu.import("resource://services-sync/type_records/passwords.js");
Cu.import("resource://services-sync/util.js");

function PasswordEngine() {
  SyncEngine.call(this, "Passwords");
}
PasswordEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: PasswordStore,
  _trackerObj: PasswordTracker,
  _recordObj: LoginRec,

  _syncFinish: function _syncFinish() {
    SyncEngine.prototype._syncFinish.call(this);

    // Delete the weave credentials from the server once
    if (!Svc.Prefs.get("deletePwd", false)) {
      try {
        let ids = Svc.Login.findLogins({}, PWDMGR_HOST, "", "").map(function(info)
          info.QueryInterface(Components.interfaces.nsILoginMetaInfo).guid);
        let coll = new Collection(this.engineURL);
        coll.ids = ids;
        let ret = coll.delete();
        this._log.debug("Delete result: " + ret);

        Svc.Prefs.set("deletePwd", true);
      }
      catch(ex) {
        this._log.debug("Password deletes failed: " + Utils.exceptionStr(ex));
      }
    }
  },

  _findDupe: function _findDupe(item) {
    let login = this._store._nsLoginInfoFromRecord(item);
    let logins = Svc.Login.findLogins({}, login.hostname, login.formSubmitURL,
      login.httpRealm);

    // Look for existing logins that match the hostname but ignore the password
    for each (let local in logins)
      if (login.matches(local, true) && local instanceof Ci.nsILoginMetaInfo)
        return local.guid;
  }
};

function PasswordStore(name) {
  Store.call(this, name);
  this._nsLoginInfo = new Components.Constructor(
    "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
}
PasswordStore.prototype = {
  __proto__: Store.prototype,

  _nsLoginInfoFromRecord: function PasswordStore__nsLoginInfoRec(record) {
    let info = new this._nsLoginInfo(record.hostname,
                                     record.formSubmitURL,
                                     record.httpRealm,
                                     record.username,
                                     record.password,
                                     record.usernameField,
                                     record.passwordField);
    info.QueryInterface(Ci.nsILoginMetaInfo);
    info.guid = record.id;
    return info;
  },

  _getLoginFromGUID: function PasswordStore__getLoginFromGUID(id) {
    let prop = Cc["@mozilla.org/hash-property-bag;1"].
      createInstance(Ci.nsIWritablePropertyBag2);
    prop.setPropertyAsAUTF8String("guid", id);

    let logins = Svc.Login.searchLogins({}, prop);
    if (logins.length > 0) {
      this._log.trace(logins.length + " items matching " + id + " found.");
      return logins[0];
    } else {
      this._log.trace("No items matching " + id + " found. Ignoring");
    }
    return false;
  },

  getAllIDs: function PasswordStore__getAllIDs() {
    let items = {};
    let logins = Svc.Login.getAllLogins({});

    for (let i = 0; i < logins.length; i++) {
      // Skip over Weave password/passphrase entries
      let metaInfo = logins[i].QueryInterface(Ci.nsILoginMetaInfo);
      if (metaInfo.hostname == PWDMGR_HOST)
        continue;

      items[metaInfo.guid] = metaInfo;
    }

    return items;
  },

  changeItemID: function PasswordStore__changeItemID(oldID, newID) {
    this._log.trace("Changing item ID: " + oldID + " to " + newID);

    let oldLogin = this._getLoginFromGUID(oldID);
    if (!oldLogin) {
      this._log.trace("Can't change item ID: item doesn't exist");
      return;
    }
    if (this._getLoginFromGUID(newID)) {
      this._log.trace("Can't change item ID: new ID already in use");
      return;
    }

    let prop = Cc["@mozilla.org/hash-property-bag;1"].
      createInstance(Ci.nsIWritablePropertyBag2);
    prop.setPropertyAsAUTF8String("guid", newID);

    Svc.Login.modifyLogin(oldLogin, prop);
  },

  itemExists: function PasswordStore__itemExists(id) {
    if (this._getLoginFromGUID(id))
      return true;
    return false;
  },

  createRecord: function createRecord(guid, uri) {
    let record = new LoginRec(uri);
    let login = this._getLoginFromGUID(guid);

    if (login) {
      record.hostname = login.hostname;
      record.formSubmitURL = login.formSubmitURL;
      record.httpRealm = login.httpRealm;
      record.username = login.username;
      record.password = login.password;
      record.usernameField = login.usernameField;
      record.passwordField = login.passwordField;
    }
    else
      record.deleted = true;
    return record;
  },

  create: function PasswordStore__create(record) {
    this._log.debug("Adding login for " + record.hostname);
    Svc.Login.addLogin(this._nsLoginInfoFromRecord(record));
  },

  remove: function PasswordStore__remove(record) {
    this._log.trace("Removing login " + record.id);

    let loginItem = this._getLoginFromGUID(record.id);
    if (!loginItem) {
      this._log.trace("Asked to remove record that doesn't exist, ignoring");
      return;
    }

    Svc.Login.removeLogin(loginItem);
  },

  update: function PasswordStore__update(record) {
    let loginItem = this._getLoginFromGUID(record.id);
    if (!loginItem) {
      this._log.debug("Skipping update for unknown item: " + record.hostname);
      return;
    }

    this._log.debug("Updating " + record.hostname);
    let newinfo = this._nsLoginInfoFromRecord(record);
    Svc.Login.modifyLogin(loginItem, newinfo);
  },

  wipe: function PasswordStore_wipe() {
    Svc.Login.removeAllLogins();
  }
};

function PasswordTracker(name) {
  Tracker.call(this, name);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}
PasswordTracker.prototype = {
  __proto__: Tracker.prototype,

  _enabled: false,
  observe: function PasswordTracker_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "weave:engine:start-tracking":
        if (!this._enabled) {
          Svc.Obs.add("passwordmgr-storage-changed", this);
          this._enabled = true;
        }
        return;
      case "weave:engine:stop-tracking":
        if (this._enabled) {
          Svc.Obs.remove("passwordmgr-storage-changed", this);
          this._enabled = false;
        }
        return;
    }

    if (this.ignoreAll)
      return;

    // A single add, remove or change is 15 points, all items removed is 50
    switch (aData) {
    case 'modifyLogin':
      aSubject = aSubject.QueryInterface(Ci.nsIArray).
        queryElementAt(1, Ci.nsILoginMetaInfo);
    case 'addLogin':
    case 'removeLogin':
      // Skip over Weave password/passphrase changes
      aSubject.QueryInterface(Ci.nsILoginMetaInfo).
        QueryInterface(Ci.nsILoginInfo);
      if (aSubject.hostname == PWDMGR_HOST)
        break;

      this.score += 15;
      this._log.trace(aData + ": " + aSubject.guid);
      this.addChangedID(aSubject.guid);
      break;
    case 'removeAllLogins':
      this._log.trace(aData);
      this.score += 500;
      break;
    }
  }
};
