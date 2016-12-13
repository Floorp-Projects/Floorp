/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = [
  "FxAccountsStorageManagerCanStoreField",
  "FxAccountsStorageManager",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://services-common/utils.js");

// A helper function so code can check what fields are able to be stored by
// the storage manager without having a reference to a manager instance.
function FxAccountsStorageManagerCanStoreField(fieldName) {
  return FXA_PWDMGR_MEMORY_FIELDS.has(fieldName) ||
         FXA_PWDMGR_PLAINTEXT_FIELDS.has(fieldName) ||
         FXA_PWDMGR_SECURE_FIELDS.has(fieldName);
}

// The storage manager object.
this.FxAccountsStorageManager = function(options = {}) {
  this.options = {
    filename: options.filename || DEFAULT_STORAGE_FILENAME,
    baseDir: options.baseDir || OS.Constants.Path.profileDir,
  }
  this.plainStorage = new JSONStorage(this.options);
  // On b2g we have no loginManager for secure storage, and tests may want
  // to pretend secure storage isn't available.
  let useSecure = 'useSecure' in options ? options.useSecure : haveLoginManager;
  if (useSecure) {
    this.secureStorage = new LoginManagerStorage();
  } else {
    this.secureStorage = null;
  }
  this._clearCachedData();
  // See .initialize() below - this protects against it not being called.
  this._promiseInitialized = Promise.reject("initialize not called");
  // A promise to avoid storage races - see _queueStorageOperation
  this._promiseStorageComplete = Promise.resolve();
}

this.FxAccountsStorageManager.prototype = {
  _initialized: false,
  _needToReadSecure: true,

  // An initialization routine that *looks* synchronous to the callers, but
  // is actually async as everything else waits for it to complete.
  initialize(accountData) {
    if (this._initialized) {
      throw new Error("already initialized");
    }
    this._initialized = true;
    // If we just throw away our pre-rejected promise it is reported as an
    // unhandled exception when it is GCd - so add an empty .catch handler here
    // to prevent this.
    this._promiseInitialized.catch(() => {});
    this._promiseInitialized = this._initialize(accountData);
  },

  _initialize: Task.async(function* (accountData) {
    log.trace("initializing new storage manager");
    try {
      if (accountData) {
        // If accountData is passed we don't need to read any storage.
        this._needToReadSecure = false;
        // split it into the 2 parts, write it and we are done.
        for (let [name, val] of Object.entries(accountData)) {
          if (FXA_PWDMGR_PLAINTEXT_FIELDS.has(name)) {
            this.cachedPlain[name] = val;
          } else if (FXA_PWDMGR_SECURE_FIELDS.has(name)) {
            this.cachedSecure[name] = val;
          } else {
            // Hopefully it's an "in memory" field. If it's not we log a warning
            // but still treat it as such (so it will still be available in this
            // session but isn't persisted anywhere.)
            if (!FXA_PWDMGR_MEMORY_FIELDS.has(name)) {
              log.warn("Unknown FxA field name in user data, treating as in-memory", name);
            }
            this.cachedMemory[name] = val;
          }
        }
        // write it out and we are done.
        yield this._write();
        return;
      }
      // So we were initialized without account data - that means we need to
      // read the state from storage. We try and read plain storage first and
      // only attempt to read secure storage if the plain storage had a user.
      this._needToReadSecure = yield this._readPlainStorage();
      if (this._needToReadSecure && this.secureStorage) {
        yield this._doReadAndUpdateSecure();
      }
    } finally {
      log.trace("initializing of new storage manager done");
    }
  }),

  finalize() {
    // We can't throw this instance away while it is still writing or we may
    // end up racing with the newly created one.
    log.trace("StorageManager finalizing");
    return this._promiseInitialized.then(() => {
      return this._promiseStorageComplete;
    }).then(() => {
      this._promiseStorageComplete = null;
      this._promiseInitialized = null;
      this._clearCachedData();
      log.trace("StorageManager finalized");
    })
  },

  // We want to make sure we don't end up doing multiple storage requests
  // concurrently - which has a small window for reads if the master-password
  // is locked at initialization time and becomes unlocked later, and always
  // has an opportunity for updates.
  // We also want to make sure we finished writing when finalizing, so we
  // can't accidentally end up with the previous user's write finishing after
  // a signOut attempts to clear it.
  // So all such operations "queue" themselves via this.
  _queueStorageOperation(func) {
    // |result| is the promise we return - it has no .catch handler, so callers
    // of the storage operation still see failure as a normal rejection.
    let result = this._promiseStorageComplete.then(func);
    // But the promise we assign to _promiseStorageComplete *does* have a catch
    // handler so that rejections in one storage operation does not prevent
    // future operations from starting (ie, _promiseStorageComplete must never
    // be in a rejected state)
    this._promiseStorageComplete = result.catch(err => {
      log.error("${func} failed: ${err}", {func, err});
    });
    return result;
  },

  // Get the account data by combining the plain and secure storage.
  // If fieldNames is specified, it may be a string or an array of strings,
  // and only those fields are returned. If not specified the entire account
  // data is returned except for "in memory" fields. Note that not specifying
  // field names will soon be deprecated/removed - we want all callers to
  // specify the fields they care about.
  getAccountData: Task.async(function* (fieldNames = null) {
    yield this._promiseInitialized;
    // We know we are initialized - this means our .cachedPlain is accurate
    // and doesn't need to be read (it was read if necessary by initialize).
    // So if there's no uid, there's no user signed in.
    if (!('uid' in this.cachedPlain)) {
      return null;
    }
    let result = {};
    if (fieldNames === null) {
      // The "old" deprecated way of fetching a logged in user.
      for (let [name, value] of Object.entries(this.cachedPlain)) {
        result[name] = value;
      }
      // But the secure data may not have been read, so try that now.
      yield this._maybeReadAndUpdateSecure();
      // .cachedSecure now has as much as it possibly can (which is possibly
      // nothing if (a) secure storage remains locked and (b) we've never updated
      // a field to be stored in secure storage.)
      for (let [name, value] of Object.entries(this.cachedSecure)) {
        result[name] = value;
      }
      // Note we don't return cachedMemory fields here - they must be explicitly
      // requested.
      return result;
    }
    // The new explicit way of getting attributes.
    if (!Array.isArray(fieldNames)) {
      fieldNames = [fieldNames];
    }
    let checkedSecure = false;
    for (let fieldName of fieldNames) {
      if (FXA_PWDMGR_MEMORY_FIELDS.has(fieldName)) {
        if (this.cachedMemory[fieldName] !== undefined) {
          result[fieldName] = this.cachedMemory[fieldName];
        }
      } else if (FXA_PWDMGR_PLAINTEXT_FIELDS.has(fieldName)) {
        if (this.cachedPlain[fieldName] !== undefined) {
          result[fieldName] = this.cachedPlain[fieldName];
        }
      } else if (FXA_PWDMGR_SECURE_FIELDS.has(fieldName)) {
        // We may not have read secure storage yet.
        if (!checkedSecure) {
          yield this._maybeReadAndUpdateSecure();
          checkedSecure = true;
        }
        if (this.cachedSecure[fieldName] !== undefined) {
          result[fieldName] = this.cachedSecure[fieldName];
        }
      } else {
        throw new Error("unexpected field '" + name + "'");
      }
    }
    return result;
  }),

  // Update just the specified fields. This DOES NOT allow you to change to
  // a different user, nor to set the user as signed-out.
  updateAccountData: Task.async(function* (newFields) {
    yield this._promiseInitialized;
    if (!('uid' in this.cachedPlain)) {
      // If this storage instance shows no logged in user, then you can't
      // update fields.
      throw new Error("No user is logged in");
    }
    if (!newFields || 'uid' in newFields || 'email' in newFields) {
      // Once we support
      // user changing email address this may need to change, but it's not
      // clear how we would be told of such a change anyway...
      throw new Error("Can't change uid or email address");
    }
    log.debug("_updateAccountData with items", Object.keys(newFields));
    // work out what bucket.
    for (let [name, value] of Object.entries(newFields)) {
      if (FXA_PWDMGR_MEMORY_FIELDS.has(name)) {
        if (value == null) {
          delete this.cachedMemory[name];
        } else {
          this.cachedMemory[name] = value;
        }
      } else if (FXA_PWDMGR_PLAINTEXT_FIELDS.has(name)) {
        if (value == null) {
          delete this.cachedPlain[name];
        } else {
          this.cachedPlain[name] = value;
        }
      } else if (FXA_PWDMGR_SECURE_FIELDS.has(name)) {
        // don't do the "delete on null" thing here - we need to keep it until
        // we have managed to read so we can nuke it on write.
        this.cachedSecure[name] = value;
      } else {
        // Throwing seems reasonable here as some client code has explicitly
        // specified the field name, so it's either confused or needs to update
        // how this field is to be treated.
        throw new Error("unexpected field '" + name + "'");
      }
    }
    // If we haven't yet read the secure data, do so now, else we may write
    // out partial data.
    yield this._maybeReadAndUpdateSecure();
    // Now save it - but don't wait on the _write promise - it's queued up as
    // a storage operation, so .finalize() will wait for completion, but no need
    // for us to.
    this._write();
  }),

  _clearCachedData() {
    this.cachedMemory = {};
    this.cachedPlain = {};
    // If we don't have secure storage available we have cachedPlain and
    // cachedSecure be the same object.
    this.cachedSecure = this.secureStorage == null ? this.cachedPlain : {};
  },

  /* Reads the plain storage and caches the read values in this.cachedPlain.
     Only ever called once and unlike the "secure" storage, is expected to never
     fail (ie, plain storage is considered always available, whereas secure
     storage may be unavailable if it is locked).

     Returns a promise that resolves with true if valid account data was found,
     false otherwise.

     Note: _readPlainStorage is only called during initialize, so isn't
     protected via _queueStorageOperation() nor _promiseInitialized.
  */
  _readPlainStorage: Task.async(function* () {
    let got;
    try {
      got = yield this.plainStorage.get();
    } catch(err) {
      // File hasn't been created yet.  That will be done
      // when write is called.
      if (!(err instanceof OS.File.Error) || !err.becauseNoSuchFile) {
        log.error("Failed to read plain storage", err);
      }
      // either way, we return null.
      got = null;
    }
    if (!got || !got.accountData || !got.accountData.uid ||
        got.version != DATA_FORMAT_VERSION) {
      return false;
    }
    // We need to update our .cachedPlain, but can't just assign to it as
    // it may need to be the exact same object as .cachedSecure
    // As a sanity check, .cachedPlain must be empty (as we are called by init)
    // XXX - this would be a good use-case for a RuntimeAssert or similar, as
    // being added in bug 1080457.
    if (Object.keys(this.cachedPlain).length != 0) {
      throw new Error("should be impossible to have cached data already.")
    }
    for (let [name, value] of Object.entries(got.accountData)) {
      this.cachedPlain[name] = value;
    }
    return true;
  }),

  /* If we haven't managed to read the secure storage, try now, so
     we can merge our cached data with the data that's already been set.
  */
  _maybeReadAndUpdateSecure: Task.async(function* () {
    if (this.secureStorage == null || !this._needToReadSecure) {
      return;
    }
    return this._queueStorageOperation(() => {
      if (this._needToReadSecure) { // we might have read it by now!
        return this._doReadAndUpdateSecure();
      }
    });
  }),

  /* Unconditionally read the secure storage and merge our cached data (ie, data
     which has already been set while the secure storage was locked) with
     the read data
  */
  _doReadAndUpdateSecure: Task.async(function* () {
    let { uid, email } = this.cachedPlain;
    try {
      log.debug("reading secure storage with existing", Object.keys(this.cachedSecure));
      // If we already have anything in .cachedSecure it means something has
      // updated cachedSecure before we've read it. That means that after we do
      // manage to read we must write back the merged data.
      let needWrite = Object.keys(this.cachedSecure).length != 0;
      let readSecure = yield this.secureStorage.get(uid, email);
      // and update our cached data with it - anything already in .cachedSecure
      // wins (including the fact it may be null or undefined, the latter
      // which means it will be removed from storage.
      if (readSecure && readSecure.version != DATA_FORMAT_VERSION) {
        log.warn("got secure data but the data format version doesn't match");
        readSecure = null;
      }
      if (readSecure && readSecure.accountData) {
        log.debug("secure read fetched items", Object.keys(readSecure.accountData));
        for (let [name, value] of Object.entries(readSecure.accountData)) {
          if (!(name in this.cachedSecure)) {
            this.cachedSecure[name] = value;
          }
        }
        if (needWrite) {
          log.debug("successfully read secure data; writing updated data back")
          yield this._doWriteSecure();
        }
      }
      this._needToReadSecure = false;
    } catch (ex) {
      if (ex instanceof this.secureStorage.STORAGE_LOCKED) {
        log.debug("setAccountData: secure storage is locked trying to read");
      } else {
        log.error("failed to read secure storage", ex);
        throw ex;
      }
    }
  }),

  _write() {
    // We don't want multiple writes happening concurrently, and we also need to
    // know when an "old" storage manager is done (this.finalize() waits for this)
    return this._queueStorageOperation(() => this.__write());
  },

  __write: Task.async(function* () {
    // Write everything back - later we could track what's actually dirty,
    // but for now we write it all.
    log.debug("writing plain storage", Object.keys(this.cachedPlain));
    let toWritePlain = {
      version: DATA_FORMAT_VERSION,
      accountData: this.cachedPlain,
    }
    yield this.plainStorage.set(toWritePlain);

    // If we have no secure storage manager we are done.
    if (this.secureStorage == null) {
      return;
    }
    // and only attempt to write to secure storage if we've managed to read it,
    // otherwise we might clobber data that's already there.
    if (!this._needToReadSecure) {
      yield this._doWriteSecure();
    }
  }),

  /* Do the actual write of secure data. Caller is expected to check if we actually
     need to write and to ensure we are in a queued storage operation.
  */
  _doWriteSecure: Task.async(function* () {
    // We need to remove null items here.
    for (let [name, value] of Object.entries(this.cachedSecure)) {
      if (value == null) {
        delete this.cachedSecure[name];
      }
    }
    log.debug("writing secure storage", Object.keys(this.cachedSecure));
    let toWriteSecure = {
      version: DATA_FORMAT_VERSION,
      accountData: this.cachedSecure,
    }
    try {
      yield this.secureStorage.set(this.cachedPlain.uid, toWriteSecure);
    } catch (ex) {
      if (!(ex instanceof this.secureStorage.STORAGE_LOCKED)) {
        throw ex;
      }
      // This shouldn't be possible as once it is unlocked it can't be
      // re-locked, and we can only be here if we've previously managed to
      // read.
      log.error("setAccountData: secure storage is locked trying to write");
    }
  }),

  // Delete the data for an account - ie, called on "sign out".
  deleteAccountData() {
    return this._queueStorageOperation(() => this._deleteAccountData());
  },

  _deleteAccountData: Task.async(function* () {
    log.debug("removing account data");
    yield this._promiseInitialized;
    yield this.plainStorage.set(null);
    if (this.secureStorage) {
      yield this.secureStorage.set(null);
    }
    this._clearCachedData();
    log.debug("account data reset");
  }),
}

/**
 * JSONStorage constructor that creates instances that may set/get
 * to a specified file, in a directory that will be created if it
 * doesn't exist.
 *
 * @param options {
 *                  filename: of the file to write to
 *                  baseDir: directory where the file resides
 *                }
 * @return instance
 */
function JSONStorage(options) {
  this.baseDir = options.baseDir;
  this.path = OS.Path.join(options.baseDir, options.filename);
};

JSONStorage.prototype = {
  set: function(contents) {
    log.trace("starting write of json user data", contents ? Object.keys(contents.accountData) : "null");
    let start = Date.now();
    return OS.File.makeDir(this.baseDir, {ignoreExisting: true})
      .then(CommonUtils.writeJSON.bind(null, contents, this.path))
      .then(result => {
        log.trace("finished write of json user data - took", Date.now()-start);
        return result;
      });
  },

  get: function() {
    log.trace("starting fetch of json user data");
    let start = Date.now();
    return CommonUtils.readJSON(this.path).then(result => {
      log.trace("finished fetch of json user data - took", Date.now()-start);
      return result;
    });
  },
};

function StorageLockedError() {
}
/**
 * LoginManagerStorage constructor that creates instances that set/get
 * data stored securely in the nsILoginManager.
 *
 * @return instance
 */

function LoginManagerStorage() {
}

LoginManagerStorage.prototype = {
  STORAGE_LOCKED: StorageLockedError,
  // The fields in the credentials JSON object that are stored in plain-text
  // in the profile directory.  All other fields are stored in the login manager,
  // and thus are only available when the master-password is unlocked.

  // a hook point for testing.
  get _isLoggedIn() {
    return Services.logins.isLoggedIn;
  },

  // Clear any data from the login manager.  Returns true if the login manager
  // was unlocked (even if no existing logins existed) or false if it was
  // locked (meaning we don't even know if it existed or not.)
  _clearLoginMgrData: Task.async(function* () {
    try { // Services.logins might be third-party and broken...
      yield Services.logins.initializationPromise;
      if (!this._isLoggedIn) {
        return false;
      }
      let logins = Services.logins.findLogins({}, FXA_PWDMGR_HOST, null, FXA_PWDMGR_REALM);
      for (let login of logins) {
        Services.logins.removeLogin(login);
      }
      return true;
    } catch (ex) {
      log.error("Failed to clear login data: ${}", ex);
      return false;
    }
  }),

  set: Task.async(function* (uid, contents) {
    if (!contents) {
      // Nuke it from the login manager.
      let cleared = yield this._clearLoginMgrData();
      if (!cleared) {
        // just log a message - we verify that the uid matches when
        // we reload it, so having a stale entry doesn't really hurt.
        log.info("not removing credentials from login manager - not logged in");
      }
      log.trace("storage set finished clearing account data");
      return;
    }

    // We are saving actual data.
    log.trace("starting write of user data to the login manager");
    try { // Services.logins might be third-party and broken...
      // and the stuff into the login manager.
      yield Services.logins.initializationPromise;
      // If MP is locked we silently fail - the user may need to re-auth
      // next startup.
      if (!this._isLoggedIn) {
        log.info("not saving credentials to login manager - not logged in");
        throw new this.STORAGE_LOCKED();
      }
      // write the data to the login manager.
      let loginInfo = new Components.Constructor(
         "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
      let login = new loginInfo(FXA_PWDMGR_HOST,
                                null, // aFormSubmitURL,
                                FXA_PWDMGR_REALM, // aHttpRealm,
                                uid, // aUsername
                                JSON.stringify(contents), // aPassword
                                "", // aUsernameField
                                "");// aPasswordField

      let existingLogins = Services.logins.findLogins({}, FXA_PWDMGR_HOST, null,
                                                      FXA_PWDMGR_REALM);
      if (existingLogins.length) {
        Services.logins.modifyLogin(existingLogins[0], login);
      } else {
        Services.logins.addLogin(login);
      }
      log.trace("finished write of user data to the login manager");
    } catch (ex) {
      if (ex instanceof this.STORAGE_LOCKED) {
        throw ex;
      }
      // just log and consume the error here - it may be a 3rd party login
      // manager replacement that's simply broken.
      log.error("Failed to save data to the login manager", ex);
    }
  }),

  get: Task.async(function* (uid, email) {
    log.trace("starting fetch of user data from the login manager");

    try { // Services.logins might be third-party and broken...
      // read the data from the login manager and merge it for return.
      yield Services.logins.initializationPromise;

      if (!this._isLoggedIn) {
        log.info("returning partial account data as the login manager is locked.");
        throw new this.STORAGE_LOCKED();
      }

      let logins = Services.logins.findLogins({}, FXA_PWDMGR_HOST, null, FXA_PWDMGR_REALM);
      if (logins.length == 0) {
        // This could happen if the MP was locked when we wrote the data.
        log.info("Can't find any credentials in the login manager");
        return null;
      }
      let login = logins[0];
      // Support either the uid or the email as the username - as of bug 1183951
      // we store the uid, but we support having either for b/w compat.
      if (login.username == uid || login.username == email) {
        return JSON.parse(login.password);
      }
      log.info("username in the login manager doesn't match - ignoring it");
      yield this._clearLoginMgrData();
    } catch (ex) {
      if (ex instanceof this.STORAGE_LOCKED) {
        throw ex;
      }
      // just log and consume the error here - it may be a 3rd party login
      // manager replacement that's simply broken.
      log.error("Failed to get data from the login manager", ex);
    }
    return null;
  }),
}

// A global variable to indicate if the login manager is available - it doesn't
// exist on b2g. Defined here as the use of preprocessor directives skews line
// numbers in the runtime, meaning stack-traces etc end up off by a few lines.
// Doing it at the end of the file makes that less of a pita.
var haveLoginManager = !AppConstants.MOZ_B2G;
