/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["PasswordEngine", "LoginRec", "PasswordValidator"];

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/collection_validator.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-common/async.js");

const SYNCABLE_LOGIN_FIELDS = [
  // `nsILoginInfo` fields.
  "hostname",
  "formSubmitURL",
  "httpRealm",
  "username",
  "password",
  "usernameField",
  "passwordField",

  // `nsILoginMetaInfo` fields.
  "timeCreated",
  "timePasswordChanged",
];

// Compares two logins to determine if their syncable fields changed. The login
// manager fires `modifyLogin` for changes to all fields, including ones we
// don't sync. In particular, `timeLastUsed` changes shouldn't mark the login
// for upload; otherwise, we might overwrite changed passwords before they're
// downloaded (bug 973166).
function isSyncableChange(oldLogin, newLogin) {
  oldLogin.QueryInterface(Ci.nsILoginMetaInfo).QueryInterface(Ci.nsILoginInfo);
  newLogin.QueryInterface(Ci.nsILoginMetaInfo).QueryInterface(Ci.nsILoginInfo);
  for (let property of SYNCABLE_LOGIN_FIELDS) {
    if (oldLogin[property] != newLogin[property]) {
      return true;
    }
  }
  return false;
}

this.LoginRec = function LoginRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
LoginRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.Login",
};

Utils.deferGetSet(LoginRec, "cleartext", [
    "hostname", "formSubmitURL",
    "httpRealm", "username", "password", "usernameField", "passwordField",
    "timeCreated", "timePasswordChanged",
    ]);


this.PasswordEngine = function PasswordEngine(service) {
  SyncEngine.call(this, "Passwords", service);
}
PasswordEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: PasswordStore,
  _trackerObj: PasswordTracker,
  _recordObj: LoginRec,

  applyIncomingBatchSize: PASSWORDS_STORE_BATCH_SIZE,

  syncPriority: 2,

  async _syncFinish() {
    await SyncEngine.prototype._syncFinish.call(this);

    // Delete the Weave credentials from the server once.
    if (!Svc.Prefs.get("deletePwdFxA", false)) {
      try {
        let ids = [];
        for (let host of Utils.getSyncCredentialsHosts()) {
          for (let info of Services.logins.findLogins({}, host, "", "")) {
            ids.push(info.QueryInterface(Components.interfaces.nsILoginMetaInfo).guid);
          }
        }
        if (ids.length) {
          let coll = new Collection(this.engineURL, null, this.service);
          coll.ids = ids;
          let ret = await coll.delete();
          this._log.debug("Delete result: " + ret);
          if (!ret.success && ret.status != 400) {
            // A non-400 failure means try again next time.
            return;
          }
        } else {
          this._log.debug("Didn't find any passwords to delete");
        }
        // If there were no ids to delete, or we succeeded, or got a 400,
        // record success.
        Svc.Prefs.set("deletePwdFxA", true);
        Svc.Prefs.reset("deletePwd"); // The old prefname we previously used.
      } catch (ex) {
        if (Async.isShutdownException(ex)) {
          throw ex;
        }
        this._log.debug("Password deletes failed", ex);
      }
    }
  },

  async _findDupe(item) {
    let login = this._store._nsLoginInfoFromRecord(item);
    if (!login) {
      return null;
    }

    let logins = Services.logins.findLogins({}, login.hostname, login.formSubmitURL, login.httpRealm);

    await Async.promiseYield(); // Yield back to main thread after synchronous operation.

    // Look for existing logins that match the hostname, but ignore the password.
    for (let local of logins) {
      if (login.matches(local, true) && local instanceof Ci.nsILoginMetaInfo) {
        return local.guid;
      }
    }

    return null;
  },

  async pullAllChanges() {
    let changes = {};
    let ids = await this._store.getAllIDs();
    for (let [id, info] of Object.entries(ids)) {
      changes[id] = info.timePasswordChanged / 1000;
    }
    return changes;
  }
};

function PasswordStore(name, engine) {
  Store.call(this, name, engine);
  this._nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
}
PasswordStore.prototype = {
  __proto__: Store.prototype,

  _newPropertyBag() {
    return Cc["@mozilla.org/hash-property-bag;1"].createInstance(Ci.nsIWritablePropertyBag2);
  },

  /**
   * Return an instance of nsILoginInfo (and, implicitly, nsILoginMetaInfo).
   */
  _nsLoginInfoFromRecord(record) {
    function nullUndefined(x) {
      return (x == undefined) ? null : x;
    }

    if (record.formSubmitURL && record.httpRealm) {
      this._log.warn("Record " + record.id + " has both formSubmitURL and httpRealm. Skipping.");
      return null;
    }

    // Passing in "undefined" results in an empty string, which later
    // counts as a value. Explicitly `|| null` these fields according to JS
    // truthiness. Records with empty strings or null will be unmolested.
    let info = new this._nsLoginInfo(record.hostname,
                                     nullUndefined(record.formSubmitURL),
                                     nullUndefined(record.httpRealm),
                                     record.username,
                                     record.password,
                                     record.usernameField,
                                     record.passwordField);

    info.QueryInterface(Ci.nsILoginMetaInfo);
    info.guid = record.id;
    if (record.timeCreated) {
      info.timeCreated = record.timeCreated;
    }
    if (record.timePasswordChanged) {
      info.timePasswordChanged = record.timePasswordChanged;
    }

    return info;
  },

  async _getLoginFromGUID(id) {
    let prop = this._newPropertyBag();
    prop.setPropertyAsAUTF8String("guid", id);

    let logins = Services.logins.searchLogins({}, prop);
    await Async.promiseYield(); // Yield back to main thread after synchronous operation.

    if (logins.length > 0) {
      this._log.trace(logins.length + " items matching " + id + " found.");
      return logins[0];
    }

    this._log.trace("No items matching " + id + " found. Ignoring");
    return null;
  },

  async getAllIDs() {
    let items = {};
    let logins = Services.logins.getAllLogins({});

    for (let i = 0; i < logins.length; i++) {
      // Skip over Weave password/passphrase entries.
      let metaInfo = logins[i].QueryInterface(Ci.nsILoginMetaInfo);
      if (Utils.getSyncCredentialsHosts().has(metaInfo.hostname)) {
        continue;
      }

      items[metaInfo.guid] = metaInfo;
    }

    return items;
  },

  async changeItemID(oldID, newID) {
    this._log.trace("Changing item ID: " + oldID + " to " + newID);

    let oldLogin = await this._getLoginFromGUID(oldID);
    if (!oldLogin) {
      this._log.trace("Can't change item ID: item doesn't exist");
      return;
    }
    if ((await this._getLoginFromGUID(newID))) {
      this._log.trace("Can't change item ID: new ID already in use");
      return;
    }

    let prop = this._newPropertyBag();
    prop.setPropertyAsAUTF8String("guid", newID);

    Services.logins.modifyLogin(oldLogin, prop);
  },

  async itemExists(id) {
    return !!(await this._getLoginFromGUID(id));
  },

  async createRecord(id, collection) {
    let record = new LoginRec(collection, id);
    let login = await this._getLoginFromGUID(id);

    if (!login) {
      record.deleted = true;
      return record;
    }

    record.hostname = login.hostname;
    record.formSubmitURL = login.formSubmitURL;
    record.httpRealm = login.httpRealm;
    record.username = login.username;
    record.password = login.password;
    record.usernameField = login.usernameField;
    record.passwordField = login.passwordField;

    // Optional fields.
    login.QueryInterface(Ci.nsILoginMetaInfo);
    record.timeCreated = login.timeCreated;
    record.timePasswordChanged = login.timePasswordChanged;

    return record;
  },

  async create(record) {
    let login = this._nsLoginInfoFromRecord(record);
    if (!login) {
      return;
    }

    this._log.debug("Adding login for " + record.hostname);
    this._log.trace("httpRealm: " + JSON.stringify(login.httpRealm) + "; " +
                    "formSubmitURL: " + JSON.stringify(login.formSubmitURL));
    try {
      Services.logins.addLogin(login);
    } catch (ex) {
      this._log.debug(`Adding record ${record.id} resulted in exception`, ex);
    }
  },

  async remove(record) {
    this._log.trace("Removing login " + record.id);

    let loginItem = await this._getLoginFromGUID(record.id);
    if (!loginItem) {
      this._log.trace("Asked to remove record that doesn't exist, ignoring");
      return;
    }

    Services.logins.removeLogin(loginItem);
  },

  async update(record) {
    let loginItem = await this._getLoginFromGUID(record.id);
    if (!loginItem) {
      this._log.debug("Skipping update for unknown item: " + record.hostname);
      return;
    }

    this._log.debug("Updating " + record.hostname);
    let newinfo = this._nsLoginInfoFromRecord(record);
    if (!newinfo) {
      return;
    }

    try {
      Services.logins.modifyLogin(loginItem, newinfo);
    } catch (ex) {
      this._log.debug(`Modifying record ${record.id} resulted in exception; not modifying`, ex);
    }
  },

  async wipe() {
    Services.logins.removeAllLogins();
  },
};

function PasswordTracker(name, engine) {
  Tracker.call(this, name, engine);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}
PasswordTracker.prototype = {
  __proto__: Tracker.prototype,

  startTracking() {
    Svc.Obs.add("passwordmgr-storage-changed", this);
  },

  stopTracking() {
    Svc.Obs.remove("passwordmgr-storage-changed", this);
  },

  observe(subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);

    if (this.ignoreAll) {
      return;
    }

    // A single add, remove or change or removing all items
    // will trigger a sync for MULTI_DEVICE.
    switch (data) {
      case "modifyLogin": {
        subject.QueryInterface(Ci.nsIArrayExtensions);
        let oldLogin = subject.GetElementAt(0);
        let newLogin = subject.GetElementAt(1);
        if (!isSyncableChange(oldLogin, newLogin)) {
          this._log.trace(`${data}: Ignoring change for ${newLogin.guid}`);
          break;
        }
        if (this._trackLogin(newLogin)) {
          this._log.trace(`${data}: Tracking change for ${newLogin.guid}`);
        }
        break;
      }

      case "addLogin":
      case "removeLogin":
        subject.QueryInterface(Ci.nsILoginMetaInfo).QueryInterface(Ci.nsILoginInfo);
        if (this._trackLogin(subject)) {
          this._log.trace(data + ": " + subject.guid);
        }
        break;

      case "removeAllLogins":
        this._log.trace(data);
        this.score += SCORE_INCREMENT_XLARGE;
        break;
    }
  },

  _trackLogin(login) {
    if (Utils.getSyncCredentialsHosts().has(login.hostname)) {
      // Skip over Weave password/passphrase changes.
      return false;
    }
    if (!this.addChangedID(login.guid)) {
      return false;
    }
    this.score += SCORE_INCREMENT_XLARGE;
    return true;
  },
};

class PasswordValidator extends CollectionValidator {
  constructor() {
    super("passwords", "id", [
      "hostname",
      "formSubmitURL",
      "httpRealm",
      "password",
      "passwordField",
      "username",
      "usernameField",
    ]);
  }

  getClientItems() {
    let logins = Services.logins.getAllLogins({});
    let syncHosts = Utils.getSyncCredentialsHosts()
    let result = logins.map(l => l.QueryInterface(Ci.nsILoginMetaInfo))
                       .filter(l => !syncHosts.has(l.hostname));
    return Promise.resolve(result);
  }

  normalizeClientItem(item) {
    return {
      id: item.guid,
      guid: item.guid,
      hostname: item.hostname,
      formSubmitURL: item.formSubmitURL,
      httpRealm: item.httpRealm,
      password: item.password,
      passwordField: item.passwordField,
      username: item.username,
      usernameField: item.usernameField,
      original: item,
    }
  }

  async normalizeServerItem(item) {
    return Object.assign({ guid: item.id }, item);
  }
}


