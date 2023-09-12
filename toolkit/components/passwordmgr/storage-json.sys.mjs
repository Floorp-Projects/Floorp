/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * LoginManagerStorage implementation for the JSON back-end.
 */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FXA_PWDMGR_HOST: "resource://gre/modules/FxAccountsCommon.sys.mjs",
  FXA_PWDMGR_REALM: "resource://gre/modules/FxAccountsCommon.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  LoginStore: "resource://gre/modules/LoginStore.sys.mjs",
});

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
  return SYNCABLE_LOGIN_FIELDS.some(prop => oldLogin[prop] != newLogin[prop]);
}

// Returns true if the argument is for the FxA login.
function isFXAHost(login) {
  return login.hostname == lazy.FXA_PWDMGR_HOST;
}

export class LoginManagerStorage_json {
  constructor() {
    this.__crypto = null; // nsILoginManagerCrypto service
    this.__decryptedPotentiallyVulnerablePasswords = null;
  }

  get _crypto() {
    if (!this.__crypto) {
      this.__crypto = Cc["@mozilla.org/login-manager/crypto/SDR;1"].getService(
        Ci.nsILoginManagerCrypto
      );
    }
    return this.__crypto;
  }

  get _decryptedPotentiallyVulnerablePasswords() {
    if (!this.__decryptedPotentiallyVulnerablePasswords) {
      this._store.ensureDataReady();
      this.__decryptedPotentiallyVulnerablePasswords = [];
      for (const potentiallyVulnerablePassword of this._store.data
        .potentiallyVulnerablePasswords) {
        const decryptedPotentiallyVulnerablePassword = this._crypto.decrypt(
          potentiallyVulnerablePassword.encryptedPassword
        );
        this.__decryptedPotentiallyVulnerablePasswords.push(
          decryptedPotentiallyVulnerablePassword
        );
      }
    }
    return this.__decryptedPotentiallyVulnerablePasswords;
  }

  initialize() {
    try {
      // Force initialization of the crypto module.
      // See bug 717490 comment 17.
      this._crypto;

      let profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;

      // Set the reference to LoginStore synchronously.
      let jsonPath = PathUtils.join(profileDir, "logins.json");
      let backupPath = "";
      let loginsBackupEnabled = Services.prefs.getBoolPref(
        "signon.backup.enabled"
      );
      if (loginsBackupEnabled) {
        backupPath = PathUtils.join(profileDir, "logins-backup.json");
      }
      this._store = new lazy.LoginStore(jsonPath, backupPath);

      return (async () => {
        // Load the data asynchronously.
        this.log(`Opening database at ${this._store.path}.`);
        await this._store.load();
      })().catch(console.error);
    } catch (e) {
      this.log(`Initialization failed ${e.name}.`);
      throw new Error("Initialization failed");
    }
  }

  /**
   * Internal method used by regression tests only.  It is called before
   * replacing this storage module with a new instance.
   */
  terminate() {
    this._store._saver.disarm();
    return this._store._save();
  }

  /**
   * Returns the "sync id" used by Sync to know whether the store is current with
   * respect to the sync servers. It is stored encrypted, but only so we
   * can detect failure to decrypt (for example, a "reset" of the primary
   * password will leave all logins alone, but they will fail to decrypt. We
   * also want this metadata to be unavailable in that scenario)
   *
   * Returns null if the data doesn't exist or if the data can't be
   * decrypted (including if the primary-password prompt is cancelled). This is
   * OK for Sync as it can't even begin syncing if the primary-password is
   * locked as the sync encrytion keys are stored in this login manager.
   */
  async getSyncID() {
    await this._store.load();
    if (!this._store.data.sync) {
      return null;
    }
    let raw = this._store.data.sync.syncID;
    try {
      return raw ? this._crypto.decrypt(raw) : null;
    } catch (e) {
      if (e.result == Cr.NS_ERROR_FAILURE) {
        this.log("Could not decrypt the syncID - returning null.");
        return null;
      }
      // any other errors get re-thrown.
      throw e;
    }
  }

  async setSyncID(syncID) {
    await this._store.load();
    if (!this._store.data.sync) {
      this._store.data.sync = {};
    }
    this._store.data.sync.syncID = syncID ? this._crypto.encrypt(syncID) : null;
    this._store.saveSoon();
  }

  async getLastSync() {
    await this._store.load();
    if (!this._store.data.sync) {
      return 0;
    }
    return this._store.data.sync.lastSync || 0.0;
  }

  async setLastSync(timestamp) {
    await this._store.load();
    if (!this._store.data.sync) {
      this._store.data.sync = {};
    }
    this._store.data.sync.lastSync = timestamp;
    this._store.saveSoon();
  }

  #incrementSyncCounter(login) {
    login.syncCounter++;
  }

  async resetSyncCounter(guid, value) {
    this._store.ensureDataReady();

    // This will also find deleted items.
    let login = this._store.data.logins.find(login => login.guid == guid);
    if (login?.syncCounter > 0) {
      login.syncCounter = Math.max(0, login.syncCounter - value);
      login.everSynced = true;
    }

    this._store.saveSoon();
  }

  // Returns false if the login has marked as deleted or doesn't exist.
  loginIsDeleted(guid) {
    let login = this._store.data.logins.find(l => l.guid == guid);
    return !!login?.deleted;
  }

  // Synrhronuously stores encrypted login, returns login clone with upserted
  // uuid and updated timestamps
  #addLogin(login) {
    this._store.ensureDataReady();

    // Throws if there are bogus values.
    lazy.LoginHelper.checkLoginValues(login);

    // Clone the login, so we don't modify the caller's object.
    let loginClone = login.clone();

    // Initialize the nsILoginMetaInfo fields, unless the caller gave us values
    loginClone.QueryInterface(Ci.nsILoginMetaInfo);
    if (loginClone.guid) {
      let guid = loginClone.guid;
      if (!this._isGuidUnique(guid)) {
        // We have an existing GUID, but it's possible that entry is unable
        // to be decrypted - if that's the case we remove the existing one
        // and allow this one to be added.
        let existing = this._searchLogins({ guid })[0];
        if (this._decryptLogins(existing).length) {
          // Existing item is good, so it's an error to try and re-add it.
          throw new Error("specified GUID already exists");
        }
        // find and remove the existing bad entry.
        let foundIndex = this._store.data.logins.findIndex(l => l.guid == guid);
        if (foundIndex == -1) {
          throw new Error("can't find a matching GUID to remove");
        }
        this._store.data.logins.splice(foundIndex, 1);
      }
    } else {
      loginClone.guid = Services.uuid.generateUUID().toString();
    }

    // Set timestamps
    let currentTime = Date.now();
    if (!loginClone.timeCreated) {
      loginClone.timeCreated = currentTime;
    }
    if (!loginClone.timeLastUsed) {
      loginClone.timeLastUsed = currentTime;
    }
    if (!loginClone.timePasswordChanged) {
      loginClone.timePasswordChanged = currentTime;
    }
    if (!loginClone.timesUsed) {
      loginClone.timesUsed = 1;
    }

    // If the everSynced is already set, then this login is an incoming
    // sync record, so there is no need to mark this as needed to be synced.
    if (!loginClone.everSynced && !isFXAHost(loginClone)) {
      this.#incrementSyncCounter(loginClone);
    }

    this._store.data.logins.push({
      id: this._store.data.nextId++,
      hostname: loginClone.origin,
      httpRealm: loginClone.httpRealm,
      formSubmitURL: loginClone.formActionOrigin,
      usernameField: loginClone.usernameField,
      passwordField: loginClone.passwordField,
      encryptedUsername: loginClone.username,
      encryptedPassword: loginClone.password,
      guid: loginClone.guid,
      encType: this._crypto.defaultEncType,
      timeCreated: loginClone.timeCreated,
      timeLastUsed: loginClone.timeLastUsed,
      timePasswordChanged: loginClone.timePasswordChanged,
      timesUsed: loginClone.timesUsed,
      syncCounter: loginClone.syncCounter,
      everSynced: loginClone.everSynced,
      encryptedUnknownFields: loginClone.unknownFields,
    });
    this._store.saveSoon();

    return loginClone;
  }

  async addLoginsAsync(logins, continueOnDuplicates = false) {
    if (logins.length === 0) {
      return logins;
    }

    const encryptedLogins = await this.#encryptLogins(logins);

    const resultLogins = [];
    for (const [login, encryptedLogin] of encryptedLogins) {
      // check for duplicates
      let loginData = {
        origin: login.origin,
        formActionOrigin: login.formActionOrigin,
        httpRealm: login.httpRealm,
      };
      const existingLogins = await Services.logins.searchLoginsAsync(loginData);

      const matchingLogin = existingLogins.find(l => login.matches(l, true));
      if (matchingLogin) {
        if (continueOnDuplicates) {
          continue;
        } else {
          throw lazy.LoginHelper.createLoginAlreadyExistsError(
            matchingLogin.guid
          );
        }
      }

      const resultLogin = this.#addLogin(encryptedLogin);

      // restore unencrypted username and password for use in `addLogin` event
      // and return value
      resultLogin.username = login.username;
      resultLogin.password = login.password;

      // Send a notification that a login was added.
      lazy.LoginHelper.notifyStorageChanged("addLogin", resultLogin);

      resultLogins.push(resultLogin);
    }

    return resultLogins;
  }

  removeLogin(login, fromSync) {
    this._store.ensureDataReady();

    let [idToDelete, storedLogin] = this._getIdForLogin(login);
    if (!idToDelete) {
      throw new Error("No matching logins");
    }

    let foundIndex = this._store.data.logins.findIndex(l => l.id == idToDelete);
    if (foundIndex != -1) {
      let login = this._store.data.logins[foundIndex];
      if (!login.deleted) {
        if (fromSync) {
          this.#replaceLoginWithTombstone(login);
        } else if (login.everSynced) {
          // The login has been synced, so mark it as deleted.
          this.#incrementSyncCounter(login);
          this.#replaceLoginWithTombstone(login);
        } else {
          // The login was never synced, so just remove it from the data.
          this._store.data.logins.splice(foundIndex, 1);
        }

        this._store.saveSoon();
      }
    }

    lazy.LoginHelper.notifyStorageChanged("removeLogin", storedLogin);
  }

  modifyLogin(oldLogin, newLoginData, fromSync) {
    this._store.ensureDataReady();

    let [idToModify, oldStoredLogin] = this._getIdForLogin(oldLogin);
    if (!idToModify) {
      throw new Error("No matching logins");
    }

    let newLogin = lazy.LoginHelper.buildModifiedLogin(
      oldStoredLogin,
      newLoginData
    );

    // Check if the new GUID is duplicate.
    if (
      newLogin.guid != oldStoredLogin.guid &&
      !this._isGuidUnique(newLogin.guid)
    ) {
      throw new Error("specified GUID already exists");
    }

    // Look for an existing entry in case key properties changed.
    if (!newLogin.matches(oldLogin, true)) {
      let loginData = {
        origin: newLogin.origin,
        formActionOrigin: newLogin.formActionOrigin,
        httpRealm: newLogin.httpRealm,
      };

      let logins = this.searchLogins(
        lazy.LoginHelper.newPropertyBag(loginData)
      );

      let matchingLogin = logins.find(login => newLogin.matches(login, true));
      if (matchingLogin) {
        throw lazy.LoginHelper.createLoginAlreadyExistsError(
          matchingLogin.guid
        );
      }
    }

    // Don't sync changes to the accounts password or when changes were only
    // made to fields that should not be synced.
    if (
      !fromSync &&
      !isFXAHost(newLogin) &&
      isSyncableChange(oldLogin, newLogin)
    ) {
      this.#incrementSyncCounter(newLogin);
    }

    // Get the encrypted value of the username and password.
    let [encUsername, encPassword, encType, encUnknownFields] =
      this._encryptLogin(newLogin);

    for (let loginItem of this._store.data.logins) {
      if (loginItem.id == idToModify && !loginItem.deleted) {
        loginItem.hostname = newLogin.origin;
        loginItem.httpRealm = newLogin.httpRealm;
        loginItem.formSubmitURL = newLogin.formActionOrigin;
        loginItem.usernameField = newLogin.usernameField;
        loginItem.passwordField = newLogin.passwordField;
        loginItem.encryptedUsername = encUsername;
        loginItem.encryptedPassword = encPassword;
        loginItem.guid = newLogin.guid;
        loginItem.encType = encType;
        loginItem.timeCreated = newLogin.timeCreated;
        loginItem.timeLastUsed = newLogin.timeLastUsed;
        loginItem.timePasswordChanged = newLogin.timePasswordChanged;
        loginItem.timesUsed = newLogin.timesUsed;
        loginItem.encryptedUnknownFields = encUnknownFields;
        loginItem.syncCounter = newLogin.syncCounter;
        this._store.saveSoon();
        break;
      }
    }

    lazy.LoginHelper.notifyStorageChanged("modifyLogin", [
      oldStoredLogin,
      newLogin,
    ]);
  }

  // Replace the login with a tombstone. It has a guid and sync-related properties,
  // but does not contain the login or password information.
  #replaceLoginWithTombstone(login) {
    login.deleted = true;

    // Delete all fields except guid, timePasswordChanged, syncCounter
    // and everSynced;
    delete login.hostname;
    delete login.httpRealm;
    delete login.formSubmitURL;
    delete login.usernameField;
    delete login.passwordField;
    delete login.encryptedUsername;
    delete login.encryptedPassword;
    delete login.encType;
    delete login.timeCreated;
    delete login.timeLastUsed;
    delete login.timesUsed;
    delete login.encryptedUnknownFields;
  }

  recordPasswordUse(login) {
    // Update the lastUsed timestamp and increment the use count.
    let propBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag
    );
    propBag.setProperty("timeLastUsed", Date.now());
    propBag.setProperty("timesUsedIncrement", 1);
    this.modifyLogin(login, propBag);
  }

  async recordBreachAlertDismissal(loginGUID) {
    this._store.ensureDataReady();
    const dismissedBreachAlertsByLoginGUID =
      this._store._data.dismissedBreachAlertsByLoginGUID;

    dismissedBreachAlertsByLoginGUID[loginGUID] = {
      timeBreachAlertDismissed: new Date().getTime(),
    };

    return this._store.saveSoon();
  }

  getBreachAlertDismissalsByLoginGUID() {
    this._store.ensureDataReady();
    return this._store._data.dismissedBreachAlertsByLoginGUID;
  }

  /**
   * Returns an array of nsILoginInfo. If decryption of a login
   * fails due to a corrupt entry, the login is not included in
   * the resulting array.
   *
   * @resolve {nsILoginInfo[]}
   */
  async getAllLogins(includeDeleted) {
    this._store.ensureDataReady();

    let [logins] = this._searchLogins({}, includeDeleted);
    if (!logins.length) {
      return [];
    }

    return this.#decryptLogins(logins);
  }

  async searchLoginsAsync(matchData, includeDeleted) {
    this.log(`Searching for matching logins for origin ${matchData.origin}.`);
    let result = this.searchLogins(
      lazy.LoginHelper.newPropertyBag(matchData),
      includeDeleted
    );
    // Emulate being async:
    return Promise.resolve(result);
  }

  /**
   * Public wrapper around _searchLogins to convert the nsIPropertyBag to a
   * JavaScript object and decrypt the results.
   *
   * @return {nsILoginInfo[]} which are decrypted.
   */
  searchLogins(matchData, includeDeleted) {
    this._store.ensureDataReady();

    let realMatchData = {};
    let options = {};

    matchData.QueryInterface(Ci.nsIPropertyBag2);
    if (matchData.hasKey("guid")) {
      // Enforce GUID-based filtering when available, since the origin of the
      // login may not match the origin of the form in the case of scheme
      // upgrades.
      realMatchData = { guid: matchData.getProperty("guid") };
    } else {
      // Convert nsIPropertyBag to normal JS object.
      for (let prop of matchData.enumerator) {
        switch (prop.name) {
          // Some property names aren't field names but are special options to
          // affect the search.
          case "acceptDifferentSubdomains":
          case "schemeUpgrades":
          case "acceptRelatedRealms":
          case "relatedRealms": {
            options[prop.name] = prop.value;
            break;
          }
          default: {
            realMatchData[prop.name] = prop.value;
            break;
          }
        }
      }
    }

    let [logins] = this._searchLogins(realMatchData, includeDeleted, options);

    // Decrypt entries found for the caller.
    logins = this._decryptLogins(logins);

    return logins;
  }

  /**
   * Private method to perform arbitrary searches on any field. Decryption is
   * left to the caller.
   *
   * formActionOrigin is handled specially for compatibility. If a null string
   * is passed and other match fields are present, it is treated as if it was
   * not present.
   *
   * Returns [logins, ids] for logins that match the arguments, where logins
   * is an array of encrypted nsLoginInfo and ids is an array of associated
   * ids in the database.
   */
  _searchLogins(
    matchData,
    includeDeleted = false,
    aOptions = {
      schemeUpgrades: false,
      acceptDifferentSubdomains: false,
      acceptRelatedRealms: false,
      relatedRealms: [],
    },
    candidateLogins = this._store.data.logins
  ) {
    function match(aLoginItem) {
      for (let field in matchData) {
        let wantedValue = matchData[field];

        // Override the storage field name for some fields due to backwards
        // compatibility with Sync/storage.
        let storageFieldName = field;
        switch (field) {
          case "formActionOrigin": {
            storageFieldName = "formSubmitURL";
            break;
          }
          case "origin": {
            storageFieldName = "hostname";
            break;
          }
        }

        switch (field) {
          case "formActionOrigin":
            if (wantedValue != null) {
              // Historical compatibility requires this special case
              if (
                aLoginItem.formSubmitURL == "" ||
                (wantedValue == "" && Object.keys(matchData).length != 1)
              ) {
                break;
              }
              if (
                !lazy.LoginHelper.isOriginMatching(
                  aLoginItem[storageFieldName],
                  wantedValue,
                  aOptions
                )
              ) {
                return false;
              }
              break;
            }
          // fall through
          case "origin":
            if (wantedValue != null) {
              // needed for formActionOrigin fall through
              if (
                !lazy.LoginHelper.isOriginMatching(
                  aLoginItem[storageFieldName],
                  wantedValue,
                  aOptions
                )
              ) {
                return false;
              }
              break;
            }
          // Normal cases.
          // fall through
          case "httpRealm":
          case "id":
          case "usernameField":
          case "passwordField":
          case "encryptedUsername":
          case "encryptedPassword":
          case "guid":
          case "encType":
          case "timeCreated":
          case "timeLastUsed":
          case "timePasswordChanged":
          case "timesUsed":
          case "syncCounter":
          case "everSynced":
            if (wantedValue == null && aLoginItem[storageFieldName]) {
              return false;
            } else if (aLoginItem[storageFieldName] != wantedValue) {
              return false;
            }
            break;
          // Fail if caller requests an unknown property.
          default:
            throw new Error("Unexpected field: " + field);
        }
      }
      return true;
    }

    let foundLogins = [],
      foundIds = [];
    for (let loginItem of candidateLogins) {
      if (loginItem.deleted && !includeDeleted) {
        continue; // skip deleted items
      }

      if (match(loginItem)) {
        // Create the new nsLoginInfo object, push to array
        let login = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(
          Ci.nsILoginInfo
        );
        login.init(
          loginItem.hostname,
          loginItem.formSubmitURL,
          loginItem.httpRealm,
          loginItem.encryptedUsername,
          loginItem.encryptedPassword,
          loginItem.usernameField,
          loginItem.passwordField
        );
        // set nsILoginMetaInfo values
        login.QueryInterface(Ci.nsILoginMetaInfo);
        login.guid = loginItem.guid;
        login.timeCreated = loginItem.timeCreated;
        login.timeLastUsed = loginItem.timeLastUsed;
        login.timePasswordChanged = loginItem.timePasswordChanged;
        login.timesUsed = loginItem.timesUsed;
        login.syncCounter = loginItem.syncCounter;
        login.everSynced = loginItem.everSynced;

        // Any unknown fields along for the ride
        login.unknownFields = loginItem.encryptedUnknownFields;
        foundLogins.push(login);
        foundIds.push(loginItem.id);
      }
    }

    this.log(
      `Returning ${foundLogins.length} logins for specified origin with options ${aOptions}`
    );
    return [foundLogins, foundIds];
  }

  /**
   * Removes all logins from local storage, including FxA Sync key.
   *
   * NOTE: You probably want removeAllUserFacingLogins instead of this function.
   *
   */
  removeAllLogins() {
    this.#removeLogins(false, true);
  }

  /**
   * Removes all user facing logins from storage. e.g. all logins except the FxA Sync key
   *
   * If you need to remove the FxA key, use `removeAllLogins` instead
   *
   * @param fullyRemove remove the logins rather than mark them deleted.
   */
  removeAllUserFacingLogins(fullyRemove) {
    this.#removeLogins(fullyRemove, false);
  }

  /**
   * Removes all logins from storage. If removeFXALogin is true, then the FxA Sync
   * key is also removed.
   *
   * @param fullyRemove remove the logins rather than mark them deleted.
   * @param removeFXALogin also remove the FxA Sync key.
   */
  #removeLogins(fullyRemove, removeFXALogin = false) {
    this._store.ensureDataReady();
    this.log("Removing all logins.");

    let removedLogins = [];
    let remainingLogins = [];
    for (let login of this._store.data.logins) {
      if (
        !removeFXALogin &&
        isFXAHost(login) &&
        login.httpRealm == lazy.FXA_PWDMGR_REALM
      ) {
        remainingLogins.push(login);
      } else {
        removedLogins.push(login);
        if (!fullyRemove && login?.everSynced) {
          // The login has been synced, so mark it as deleted.
          this.#incrementSyncCounter(login);
          this.#replaceLoginWithTombstone(login);
          remainingLogins.push(login);
        }
      }
    }
    this._store.data.logins = remainingLogins;

    this._store.data.potentiallyVulnerablePasswords = [];
    this.__decryptedPotentiallyVulnerablePasswords = null;
    this._store.data.dismissedBreachAlertsByLoginGUID = {};
    this._store.saveSoon();

    lazy.LoginHelper.notifyStorageChanged("removeAllLogins", removedLogins);
  }

  findLogins(origin, formActionOrigin, httpRealm) {
    this._store.ensureDataReady();

    let loginData = {
      origin,
      formActionOrigin,
      httpRealm,
    };
    let matchData = {};
    for (let field of ["origin", "formActionOrigin", "httpRealm"]) {
      if (loginData[field] != "") {
        matchData[field] = loginData[field];
      }
    }
    let [logins] = this._searchLogins(matchData);

    // Decrypt entries found for the caller.
    logins = this._decryptLogins(logins);

    this.log(`Returning ${logins.length} logins.`);
    return logins;
  }

  countLogins(origin, formActionOrigin, httpRealm) {
    this._store.ensureDataReady();

    let loginData = {
      origin,
      formActionOrigin,
      httpRealm,
    };
    let matchData = {};
    for (let field of ["origin", "formActionOrigin", "httpRealm"]) {
      if (loginData[field] != "") {
        matchData[field] = loginData[field];
      }
    }
    let [logins] = this._searchLogins(matchData);

    this.log(`Counted ${logins.length} logins.`);
    return logins.length;
  }

  addPotentiallyVulnerablePassword(login) {
    this._store.ensureDataReady();
    // this breached password is already stored
    if (this.isPotentiallyVulnerablePassword(login)) {
      return;
    }
    this.__decryptedPotentiallyVulnerablePasswords.push(login.password);

    this._store.data.potentiallyVulnerablePasswords.push({
      encryptedPassword: this._crypto.encrypt(login.password),
    });
    this._store.saveSoon();
  }

  isPotentiallyVulnerablePassword(login) {
    return this._decryptedPotentiallyVulnerablePasswords.includes(
      login.password
    );
  }

  clearAllPotentiallyVulnerablePasswords() {
    this._store.ensureDataReady();
    if (!this._store.data.potentiallyVulnerablePasswords.length) {
      // No need to write to disk
      return;
    }
    this._store.data.potentiallyVulnerablePasswords = [];
    this._store.saveSoon();
    this.__decryptedPotentiallyVulnerablePasswords = null;
  }

  get uiBusy() {
    return this._crypto.uiBusy;
  }

  get isLoggedIn() {
    return this._crypto.isLoggedIn;
  }

  /**
   * Returns an array with two items: [id, login]. If the login was not
   * found, both items will be null. The returned login contains the actual
   * stored login (useful for looking at the actual nsILoginMetaInfo values).
   */
  _getIdForLogin(login) {
    this._store.ensureDataReady();

    let matchData = {};
    for (let field of ["origin", "formActionOrigin", "httpRealm"]) {
      if (login[field] != "") {
        matchData[field] = login[field];
      }
    }
    let [logins, ids] = this._searchLogins(matchData);

    let id = null;
    let foundLogin = null;

    // The specified login isn't encrypted, so we need to ensure
    // the logins we're comparing with are decrypted. We decrypt one entry
    // at a time, lest _decryptLogins return fewer entries and screw up
    // indices between the two.
    for (let i = 0; i < logins.length; i++) {
      let [decryptedLogin] = this._decryptLogins([logins[i]]);

      if (!decryptedLogin || !decryptedLogin.equals(login)) {
        continue;
      }

      // We've found a match, set id and break
      foundLogin = decryptedLogin;
      id = ids[i];
      break;
    }

    return [id, foundLogin];
  }

  /**
   * Checks to see if the specified GUID already exists.
   */
  _isGuidUnique(guid) {
    this._store.ensureDataReady();

    return this._store.data.logins.every(l => l.guid != guid);
  }

  /*
   * Asynchronously encrypt multiple logins.
   * Returns a promise resolving to an array of arrays containing two entries:
   * the original login and a clone with encrypted properties.
   */
  async #encryptLogins(logins) {
    if (logins.length === 0) {
      return logins;
    }

    const plaintexts = logins.reduce(
      (memo, { username, password, unknownFields }) =>
        memo.concat([username, password, unknownFields]),
      []
    );
    const ciphertexts = await this._crypto.encryptMany(plaintexts);

    return logins.map((login, i) => {
      const [encryptedUsername, encryptedPassword, encryptedUnknownFields] =
        ciphertexts.slice(3 * i, 3 * i + 3);

      const encryptedLogin = login.clone();
      encryptedLogin.username = encryptedUsername;
      encryptedLogin.password = encryptedPassword;
      encryptedLogin.unknownFields = encryptedUnknownFields;

      return [login, encryptedLogin];
    });
  }

  /*
   * Asynchronously decrypt multiple logins.
   * Returns a promise resolving to an array of clones with decrypted properties.
   */
  async #decryptLogins(logins) {
    if (logins.length === 0) {
      return logins;
    }

    const ciphertexts = logins.reduce(
      (memo, { username, password, unknownFields }) =>
        memo.concat([username, password, unknownFields]),
      []
    );
    const plaintexts = await this._crypto.decryptMany(ciphertexts);

    return logins
      .map((login, i) => {
        // Deleted logins don't have any info to decrypt.
        const decryptedLogin = login.clone();
        if (this.loginIsDeleted(login.guid)) {
          return decryptedLogin;
        }

        const [username, password, unknownFields] = plaintexts.slice(
          3 * i,
          3 * i + 3
        );

        // If the username or password is blank it means that decryption may have
        // failed during decryptMany but we can't differentiate an empty string
        // value from a failure so we attempt to decrypt again and check the
        // result.
        if (!username || !password) {
          try {
            this._crypto.decrypt(login.username);
            this._crypto.decrypt(login.password);
          } catch (e) {
            // If decryption failed (corrupt entry?), just return it as it is.
            // Rethrow other errors (like canceling entry of a primary pw)
            if (e.result == Cr.NS_ERROR_FAILURE) {
              this.log(
                `Could not decrypt login: ${
                  login.QueryInterface(Ci.nsILoginMetaInfo).guid
                }.`
              );
              return null;
            }
            throw e;
          }
        }

        decryptedLogin.username = username;
        decryptedLogin.password = password;
        decryptedLogin.unknownFields = unknownFields;

        return decryptedLogin;
      })
      .filter(Boolean);
  }

  /**
   * Returns the encrypted username, password, and encrypton type for the specified
   * login. Can throw if the user cancels a primary password entry.
   */
  _encryptLogin(login) {
    let encUsername = this._crypto.encrypt(login.username);
    let encPassword = this._crypto.encrypt(login.password);

    // Unknown fields should be encrypted since we can't know whether new fields
    // from other clients will contain sensitive data or not
    let encUnknownFields = null;
    if (login.unknownFields) {
      encUnknownFields = this._crypto.encrypt(login.unknownFields);
    }
    let encType = this._crypto.defaultEncType;

    return [encUsername, encPassword, encType, encUnknownFields];
  }

  /**
   * Decrypts username and password fields in the provided array of
   * logins.
   *
   * The entries specified by the array will be decrypted, if possible.
   * An array of successfully decrypted logins will be returned. The return
   * value should be given to external callers (since still-encrypted
   * entries are useless), whereas internal callers generally don't want
   * to lose unencrypted entries (eg, because the user clicked Cancel
   * instead of entering their primary password)
   */
  _decryptLogins(logins) {
    let result = [];

    for (let login of logins) {
      if (this.loginIsDeleted(login.guid)) {
        result.push(login);
        continue;
      }

      try {
        login.username = this._crypto.decrypt(login.username);
        login.password = this._crypto.decrypt(login.password);
        // Verify unknownFields actually has a value
        if (login.unknownFields) {
          login.unknownFields = this._crypto.decrypt(login.unknownFields);
        }
      } catch (e) {
        // If decryption failed (corrupt entry?), just skip it.
        // Rethrow other errors (like canceling entry of a primary pw)
        if (e.result == Cr.NS_ERROR_FAILURE) {
          continue;
        }
        throw e;
      }
      result.push(login);
    }

    return result;
  }
}

ChromeUtils.defineLazyGetter(LoginManagerStorage_json.prototype, "log", () => {
  let logger = lazy.LoginHelper.createLogger("Login storage");
  return logger.log.bind(logger);
});
