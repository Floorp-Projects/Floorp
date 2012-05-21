/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ['PasswordEngine', 'LoginRec'];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");

function LoginRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
LoginRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.Login",
};

Utils.deferGetSet(LoginRec, "cleartext", ["hostname", "formSubmitURL",
  "httpRealm", "username", "password", "usernameField", "passwordField"]);


function PasswordEngine() {
  SyncEngine.call(this, "Passwords");
}
PasswordEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: PasswordStore,
  _trackerObj: PasswordTracker,
  _recordObj: LoginRec,
  applyIncomingBatchSize: PASSWORDS_STORE_BATCH_SIZE,

  _syncFinish: function _syncFinish() {
    SyncEngine.prototype._syncFinish.call(this);

    // Delete the weave credentials from the server once
    if (!Svc.Prefs.get("deletePwd", false)) {
      try {
        let ids = Services.logins.findLogins({}, PWDMGR_HOST, "", "")
                          .map(function(info) {
          return info.QueryInterface(Components.interfaces.nsILoginMetaInfo).guid;
        });
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
    if (!login)
      return;

    let logins = Services.logins.findLogins(
      {}, login.hostname, login.formSubmitURL, login.httpRealm);
    this._store._sleep(0); // Yield back to main thread after synchronous operation.

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

  XPCOMUtils.defineLazyGetter(this, "DBConnection", function() {
    return Services.logins.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.mozIStorageConnection);
  });
}
PasswordStore.prototype = {
  __proto__: Store.prototype,

  _nsLoginInfoFromRecord: function PasswordStore__nsLoginInfoRec(record) {
    if (record.formSubmitURL &&
        record.httpRealm) {
      this._log.warn("Record " + record.id +
                     " has both formSubmitURL and httpRealm. Skipping.");
      return null;
    }
    
    // Passing in "undefined" results in an empty string, which later
    // counts as a value. Explicitly `|| null` these fields according to JS
    // truthiness. Records with empty strings or null will be unmolested.
    function nullUndefined(x) (x == undefined) ? null : x;
    let info = new this._nsLoginInfo(record.hostname,
                                     nullUndefined(record.formSubmitURL),
                                     nullUndefined(record.httpRealm),
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

    let logins = Services.logins.searchLogins({}, prop);
    this._sleep(0); // Yield back to main thread after synchronous operation.
    if (logins.length > 0) {
      this._log.trace(logins.length + " items matching " + id + " found.");
      return logins[0];
    } else {
      this._log.trace("No items matching " + id + " found. Ignoring");
    }
    return null;
  },

  applyIncomingBatch: function applyIncomingBatch(records) {
    if (!this.DBConnection) {
      return Store.prototype.applyIncomingBatch.call(this, records);
    }

    return Utils.runInTransaction(this.DBConnection, function() {
      return Store.prototype.applyIncomingBatch.call(this, records);
    }, this);
  },

  applyIncoming: function applyIncoming(record) {
    Store.prototype.applyIncoming.call(this, record);
    this._sleep(0); // Yield back to main thread after synchronous operation.
  },

  getAllIDs: function PasswordStore__getAllIDs() {
    let items = {};
    let logins = Services.logins.getAllLogins({});

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

    Services.logins.modifyLogin(oldLogin, prop);
  },

  itemExists: function PasswordStore__itemExists(id) {
    if (this._getLoginFromGUID(id))
      return true;
    return false;
  },

  createRecord: function createRecord(id, collection) {
    let record = new LoginRec(collection, id);
    let login = this._getLoginFromGUID(id);

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
    let login = this._nsLoginInfoFromRecord(record);
    if (!login)
      return;
    this._log.debug("Adding login for " + record.hostname);
    this._log.trace("httpRealm: " + JSON.stringify(login.httpRealm) + "; " +
                    "formSubmitURL: " + JSON.stringify(login.formSubmitURL));
    try {
      Services.logins.addLogin(login);
    } catch(ex) {
      this._log.debug("Adding record " + record.id +
                      " resulted in exception " + Utils.exceptionStr(ex));
    }
  },

  remove: function PasswordStore__remove(record) {
    this._log.trace("Removing login " + record.id);

    let loginItem = this._getLoginFromGUID(record.id);
    if (!loginItem) {
      this._log.trace("Asked to remove record that doesn't exist, ignoring");
      return;
    }

    Services.logins.removeLogin(loginItem);
  },

  update: function PasswordStore__update(record) {
    let loginItem = this._getLoginFromGUID(record.id);
    if (!loginItem) {
      this._log.debug("Skipping update for unknown item: " + record.hostname);
      return;
    }

    this._log.debug("Updating " + record.hostname);
    let newinfo = this._nsLoginInfoFromRecord(record);
    if (!newinfo)
      return;
    try {
      Services.logins.modifyLogin(loginItem, newinfo);
    } catch(ex) {
      this._log.debug("Modifying record " + record.id +
                      " resulted in exception " + Utils.exceptionStr(ex) +
                      ". Not modifying.");
    }
  },

  wipe: function PasswordStore_wipe() {
    Services.logins.removeAllLogins();
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

    // A single add, remove or change or removing all items
    // will trigger a sync for MULTI_DEVICE.
    switch (aData) {
    case 'modifyLogin':
      aSubject = aSubject.QueryInterface(Ci.nsIArray).
        queryElementAt(1, Ci.nsILoginMetaInfo);
      // fallthrough
    case 'addLogin':
    case 'removeLogin':
      // Skip over Weave password/passphrase changes
      aSubject.QueryInterface(Ci.nsILoginMetaInfo).
        QueryInterface(Ci.nsILoginInfo);
      if (aSubject.hostname == PWDMGR_HOST)
        break;

      this.score += SCORE_INCREMENT_XLARGE;
      this._log.trace(aData + ": " + aSubject.guid);
      this.addChangedID(aSubject.guid);
      break;
    case 'removeAllLogins':
      this._log.trace(aData);
      this.score += SCORE_INCREMENT_XLARGE;
      break;
    }
  }
};
