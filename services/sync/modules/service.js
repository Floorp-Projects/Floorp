/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["Service"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

// How long before refreshing the cluster
const CLUSTER_BACKOFF = 5 * 60 * 1000; // 5 minutes

// How long a key to generate from an old passphrase.
const PBKDF2_KEY_BYTES = 16;

const CRYPTO_COLLECTION = "crypto";
const KEYS_WBO = "keys";

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/rest.js");
Cu.import("resource://services-sync/stages/cluster.js");
Cu.import("resource://services-sync/stages/enginesync.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/userapi.js");
Cu.import("resource://services-sync/util.js");

const ENGINE_MODULES = {
  Addons: "addons.js",
  Apps: "apps.js",
  Bookmarks: "bookmarks.js",
  Form: "forms.js",
  History: "history.js",
  Password: "passwords.js",
  Prefs: "prefs.js",
  Tab: "tabs.js",
};

const STORAGE_INFO_TYPES = [INFO_COLLECTIONS,
                            INFO_COLLECTION_USAGE,
                            INFO_COLLECTION_COUNTS,
                            INFO_QUOTA];


function Sync11Service() {
  this._notify = Utils.notify("weave:service:");
}
Sync11Service.prototype = {

  _lock: Utils.lock,
  _locked: false,
  _loggedIn: false,

  userBaseURL: null,
  infoURL: null,
  storageURL: null,
  metaURL: null,
  cryptoKeyURL: null,

  get enabledEngineNames() {
    return [e.name for each (e in this.engineManager.getEnabled())];
  },

  get serverURL() Svc.Prefs.get("serverURL"),
  set serverURL(value) {
    if (!value.endsWith("/")) {
      value += "/";
    }

    // Only do work if it's actually changing
    if (value == this.serverURL)
      return;

    // A new server most likely uses a different cluster, so clear that
    Svc.Prefs.set("serverURL", value);
    Svc.Prefs.reset("clusterURL");
  },

  get clusterURL() Svc.Prefs.get("clusterURL", ""),
  set clusterURL(value) {
    Svc.Prefs.set("clusterURL", value);
    this._updateCachedURLs();
  },

  get miscAPI() {
    // Append to the serverURL if it's a relative fragment
    let misc = Svc.Prefs.get("miscURL");
    if (misc.indexOf(":") == -1)
      misc = this.serverURL + misc;
    return misc + MISC_API_VERSION + "/";
  },

  /**
   * The URI of the User API service.
   *
   * This is the base URI of the service as applicable to all users up to
   * and including the server version path component, complete with trailing
   * forward slash.
   */
  get userAPIURI() {
    // Append to the serverURL if it's a relative fragment.
    let url = Svc.Prefs.get("userURL");
    if (!url.contains(":")) {
      url = this.serverURL + url;
    }

    return url + USER_API_VERSION + "/";
  },

  get pwResetURL() {
    return this.serverURL + "weave-password-reset";
  },

  get syncID() {
    // Generate a random syncID id we don't have one
    let syncID = Svc.Prefs.get("client.syncID", "");
    return syncID == "" ? this.syncID = Utils.makeGUID() : syncID;
  },
  set syncID(value) {
    Svc.Prefs.set("client.syncID", value);
  },

  get isLoggedIn() { return this._loggedIn; },

  get locked() { return this._locked; },
  lock: function lock() {
    if (this._locked)
      return false;
    this._locked = true;
    return true;
  },
  unlock: function unlock() {
    this._locked = false;
  },

  // A specialized variant of Utils.catch.
  // This provides a more informative error message when we're already syncing:
  // see Bug 616568.
  _catch: function _catch(func) {
    function lockExceptions(ex) {
      if (Utils.isLockException(ex)) {
        // This only happens if we're syncing already.
        this._log.info("Cannot start sync: already syncing?");
      }
    }

    return Utils.catch.call(this, func, lockExceptions);
  },

  _updateCachedURLs: function _updateCachedURLs() {
    // Nothing to cache yet if we don't have the building blocks
    if (this.clusterURL == "" || this.identity.username == "")
      return;

    let storageAPI = this.clusterURL + SYNC_API_VERSION + "/";
    this.userBaseURL = storageAPI + this.identity.username + "/";
    this._log.debug("Caching URLs under storage user base: " + this.userBaseURL);

    // Generate and cache various URLs under the storage API for this user
    this.infoURL = this.userBaseURL + "info/collections";
    this.storageURL = this.userBaseURL + "storage/";
    this.metaURL = this.storageURL + "meta/global";
    this.cryptoKeysURL = this.storageURL + CRYPTO_COLLECTION + "/" + KEYS_WBO;
  },

  _checkCrypto: function _checkCrypto() {
    let ok = false;

    try {
      let iv = Svc.Crypto.generateRandomIV();
      if (iv.length == 24)
        ok = true;

    } catch (e) {
      this._log.debug("Crypto check failed: " + e);
    }

    return ok;
  },

  /**
   * Here is a disgusting yet reasonable way of handling HMAC errors deep in
   * the guts of Sync. The astute reader will note that this is a hacky way of
   * implementing something like continuable conditions.
   *
   * A handler function is glued to each engine. If the engine discovers an
   * HMAC failure, we fetch keys from the server and update our keys, just as
   * we would on startup.
   *
   * If our key collection changed, we signal to the engine (via our return
   * value) that it should retry decryption.
   *
   * If our key collection did not change, it means that we already had the
   * correct keys... and thus a different client has the wrong ones. Reupload
   * the bundle that we fetched, which will bump the modified time on the
   * server and (we hope) prompt a broken client to fix itself.
   *
   * We keep track of the time at which we last applied this reasoning, because
   * thrashing doesn't solve anything. We keep a reasonable interval between
   * these remedial actions.
   */
  lastHMACEvent: 0,

  /*
   * Returns whether to try again.
   */
  handleHMACEvent: function handleHMACEvent() {
    let now = Date.now();

    // Leave a sizable delay between HMAC recovery attempts. This gives us
    // time for another client to fix themselves if we touch the record.
    if ((now - this.lastHMACEvent) < HMAC_EVENT_INTERVAL)
      return false;

    this._log.info("Bad HMAC event detected. Attempting recovery " +
                   "or signaling to other clients.");

    // Set the last handled time so that we don't act again.
    this.lastHMACEvent = now;

    // Fetch keys.
    let cryptoKeys = new CryptoWrapper(CRYPTO_COLLECTION, KEYS_WBO);
    try {
      let cryptoResp = cryptoKeys.fetch(this.resource(this.cryptoKeysURL)).response;

      // Save out the ciphertext for when we reupload. If there's a bug in
      // CollectionKeyManager, this will prevent us from uploading junk.
      let cipherText = cryptoKeys.ciphertext;

      if (!cryptoResp.success) {
        this._log.warn("Failed to download keys.");
        return false;
      }

      let keysChanged = this.handleFetchedKeys(this.identity.syncKeyBundle,
                                               cryptoKeys, true);
      if (keysChanged) {
        // Did they change? If so, carry on.
        this._log.info("Suggesting retry.");
        return true;              // Try again.
      }

      // If not, reupload them and continue the current sync.
      cryptoKeys.ciphertext = cipherText;
      cryptoKeys.cleartext  = null;

      let uploadResp = cryptoKeys.upload(this.resource(this.cryptoKeysURL));
      if (uploadResp.success)
        this._log.info("Successfully re-uploaded keys. Continuing sync.");
      else
        this._log.warn("Got error response re-uploading keys. " +
                       "Continuing sync; let's try again later.");

      return false;            // Don't try again: same keys.

    } catch (ex) {
      this._log.warn("Got exception \"" + ex + "\" fetching and handling " +
                     "crypto keys. Will try again later.");
      return false;
    }
  },

  handleFetchedKeys: function handleFetchedKeys(syncKey, cryptoKeys, skipReset) {
    // Don't want to wipe if we're just starting up!
    let wasBlank = this.collectionKeys.isClear;
    let keysChanged = this.collectionKeys.updateContents(syncKey, cryptoKeys);

    if (keysChanged && !wasBlank) {
      this._log.debug("Keys changed: " + JSON.stringify(keysChanged));

      if (!skipReset) {
        this._log.info("Resetting client to reflect key change.");

        if (keysChanged.length) {
          // Collection keys only. Reset individual engines.
          this.resetClient(keysChanged);
        }
        else {
          // Default key changed: wipe it all.
          this.resetClient();
        }

        this._log.info("Downloaded new keys, client reset. Proceeding.");
      }
      return true;
    }
    return false;
  },

  /**
   * Prepare to initialize the rest of Weave after waiting a little bit
   */
  onStartup: function onStartup() {
    this._migratePrefs();

    // Status is instantiated before us and is the first to grab an instance of
    // the IdentityManager. We use that instance because IdentityManager really
    // needs to be a singleton. Ideally, the longer-lived object would spawn
    // this service instance.
    if (!Status || !Status._authManager) {
      throw new Error("Status or Status._authManager not initialized.");
    }

    this.status = Status;
    this.identity = Status._authManager;
    this.collectionKeys = new CollectionKeyManager();

    this.errorHandler = new ErrorHandler(this);

    this._log = Log4Moz.repository.getLogger("Sync.Service");
    this._log.level =
      Log4Moz.Level[Svc.Prefs.get("log.logger.service.main")];

    this._log.info("Loading Weave " + WEAVE_VERSION);

    this._clusterManager = new ClusterManager(this);
    this.recordManager = new RecordManager(this);

    this.enabled = true;

    this._registerEngines();

    let ua = Cc["@mozilla.org/network/protocol;1?name=http"].
      getService(Ci.nsIHttpProtocolHandler).userAgent;
    this._log.info(ua);

    if (!this._checkCrypto()) {
      this.enabled = false;
      this._log.info("Could not load the Weave crypto component. Disabling " +
                      "Weave, since it will not work correctly.");
    }

    Svc.Obs.add("weave:service:setup-complete", this);
    Svc.Prefs.observe("engine.", this);

    this.scheduler = new SyncScheduler(this);

    if (!this.enabled) {
      this._log.info("Firefox Sync disabled.");
    }

    this._updateCachedURLs();

    let status = this._checkSetup();
    if (status != STATUS_DISABLED && status != CLIENT_NOT_CONFIGURED) {
      Svc.Obs.notify("weave:engine:start-tracking");
    }

    // Send an event now that Weave service is ready.  We don't do this
    // synchronously so that observers can import this module before
    // registering an observer.
    Utils.nextTick(function onNextTick() {
      this.status.ready = true;

      // UI code uses the flag on the XPCOM service so it doesn't have
      // to load a bunch of modules.
      let xps = Cc["@mozilla.org/weave/service;1"]
                  .getService(Ci.nsISupports)
                  .wrappedJSObject;
      xps.ready = true;

      Svc.Obs.notify("weave:service:ready");
    }.bind(this));
  },

  _checkSetup: function _checkSetup() {
    if (!this.enabled) {
      return this.status.service = STATUS_DISABLED;
    }
    return this.status.checkSetup();
  },

  _migratePrefs: function _migratePrefs() {
    // Migrate old debugLog prefs.
    let logLevel = Svc.Prefs.get("log.appender.debugLog");
    if (logLevel) {
      Svc.Prefs.set("log.appender.file.level", logLevel);
      Svc.Prefs.reset("log.appender.debugLog");
    }
    if (Svc.Prefs.get("log.appender.debugLog.enabled")) {
      Svc.Prefs.set("log.appender.file.logOnSuccess", true);
      Svc.Prefs.reset("log.appender.debugLog.enabled");
    }

    // Migrate old extensions.weave.* prefs if we haven't already tried.
    if (Svc.Prefs.get("migrated", false))
      return;

    // Grab the list of old pref names
    let oldPrefBranch = "extensions.weave.";
    let oldPrefNames = Cc["@mozilla.org/preferences-service;1"].
                       getService(Ci.nsIPrefService).
                       getBranch(oldPrefBranch).
                       getChildList("", {});

    // Map each old pref to the current pref branch
    let oldPref = new Preferences(oldPrefBranch);
    for each (let pref in oldPrefNames)
      Svc.Prefs.set(pref, oldPref.get(pref));

    // Remove all the old prefs and remember that we've migrated
    oldPref.resetBranch("");
    Svc.Prefs.set("migrated", true);
  },

  /**
   * Register the built-in engines for certain applications
   */
  _registerEngines: function _registerEngines() {
    this.engineManager = new EngineManager(this);

    let engines = [];
    // Applications can provide this preference (comma-separated list)
    // to specify which engines should be registered on startup.
    let pref = Svc.Prefs.get("registerEngines");
    if (pref) {
      engines = pref.split(",");
    }

    this.clientsEngine = new ClientEngine(this);

    for (let name of engines) {
      if (!name in ENGINE_MODULES) {
        this._log.info("Do not know about engine: " + name);
        continue;
      }

      let ns = {};
      try {
        Cu.import("resource://services-sync/engines/" + ENGINE_MODULES[name], ns);

        let engineName = name + "Engine";
        if (!(engineName in ns)) {
          this._log.warn("Could not find exported engine instance: " + engineName);
          continue;
        }

        this.engineManager.register(ns[engineName], this);
      } catch (ex) {
        this._log.warn("Could not register engine " + name + ": " +
                       CommonUtils.exceptionStr(ex));
      }
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  // nsIObserver

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "weave:service:setup-complete":
        let status = this._checkSetup();
        if (status != STATUS_DISABLED && status != CLIENT_NOT_CONFIGURED)
            Svc.Obs.notify("weave:engine:start-tracking");
        break;
      case "nsPref:changed":
        if (this._ignorePrefObserver)
          return;
        let engine = data.slice((PREFS_BRANCH + "engine.").length);
        this._handleEngineStatusChanged(engine);
        break;
    }
  },

  _handleEngineStatusChanged: function handleEngineDisabled(engine) {
    this._log.trace("Status for " + engine + " engine changed.");
    if (Svc.Prefs.get("engineStatusChanged." + engine, false)) {
      // The enabled status being changed back to what it was before.
      Svc.Prefs.reset("engineStatusChanged." + engine);
    } else {
      // Remember that the engine status changed locally until the next sync.
      Svc.Prefs.set("engineStatusChanged." + engine, true);
    }
  },

  /**
   * Obtain a Resource instance with authentication credentials.
   */
  resource: function resource(url) {
    let res = new Resource(url);
    res.authenticator = this.identity.getResourceAuthenticator();

    return res;
  },

  /**
   * Obtain a SyncStorageRequest instance with authentication credentials.
   */
  getStorageRequest: function getStorageRequest(url) {
    let request = new SyncStorageRequest(url);
    request.authenticator = this.identity.getRESTRequestAuthenticator();

    return request;
  },

  /**
   * Perform the info fetch as part of a login or key fetch.
   */
  _fetchInfo: function _fetchInfo(url) {
    let infoURL = url || this.infoURL;

    this._log.trace("In _fetchInfo: " + infoURL);
    let info;
    try {
      info = this.resource(infoURL).get();
    } catch (ex) {
      this.errorHandler.checkServerError(ex);
      throw ex;
    }
    if (!info.success) {
      this.errorHandler.checkServerError(info);
      throw "aborting sync, failed to get collections";
    }
    return info;
  },

  verifyAndFetchSymmetricKeys: function verifyAndFetchSymmetricKeys(infoResponse) {

    this._log.debug("Fetching and verifying -- or generating -- symmetric keys.");

    // Don't allow empty/missing passphrase.
    // Furthermore, we assume that our sync key is already upgraded,
    // and fail if that assumption is invalidated.

    if (!this.identity.syncKey) {
      this.status.login = LOGIN_FAILED_NO_PASSPHRASE;
      this.status.sync = CREDENTIALS_CHANGED;
      return false;
    }

    let syncKeyBundle = this.identity.syncKeyBundle;
    if (!syncKeyBundle) {
      this._log.error("Sync Key Bundle not set. Invalid Sync Key?");

      this.status.login = LOGIN_FAILED_INVALID_PASSPHRASE;
      this.status.sync = CREDENTIALS_CHANGED;
      return false;
    }

    try {
      if (!infoResponse)
        infoResponse = this._fetchInfo();    // Will throw an exception on failure.

      // This only applies when the server is already at version 4.
      if (infoResponse.status != 200) {
        this._log.warn("info/collections returned non-200 response. Failing key fetch.");
        this.status.login = LOGIN_FAILED_SERVER_ERROR;
        this.errorHandler.checkServerError(infoResponse);
        return false;
      }

      let infoCollections = infoResponse.obj;

      this._log.info("Testing info/collections: " + JSON.stringify(infoCollections));

      if (this.collectionKeys.updateNeeded(infoCollections)) {
        this._log.info("collection keys reports that a key update is needed.");

        // Don't always set to CREDENTIALS_CHANGED -- we will probably take care of this.

        // Fetch storage/crypto/keys.
        let cryptoKeys;

        if (infoCollections && (CRYPTO_COLLECTION in infoCollections)) {
          try {
            cryptoKeys = new CryptoWrapper(CRYPTO_COLLECTION, KEYS_WBO);
            let cryptoResp = cryptoKeys.fetch(this.resource(this.cryptoKeysURL)).response;

            if (cryptoResp.success) {
              let keysChanged = this.handleFetchedKeys(syncKeyBundle, cryptoKeys);
              return true;
            }
            else if (cryptoResp.status == 404) {
              // On failure, ask to generate new keys and upload them.
              // Fall through to the behavior below.
              this._log.warn("Got 404 for crypto/keys, but 'crypto' in info/collections. Regenerating.");
              cryptoKeys = null;
            }
            else {
              // Some other problem.
              this.status.login = LOGIN_FAILED_SERVER_ERROR;
              this.errorHandler.checkServerError(cryptoResp);
              this._log.warn("Got status " + cryptoResp.status + " fetching crypto keys.");
              return false;
            }
          }
          catch (ex) {
            this._log.warn("Got exception \"" + ex + "\" fetching cryptoKeys.");
            // TODO: Um, what exceptions might we get here? Should we re-throw any?

            // One kind of exception: HMAC failure.
            if (Utils.isHMACMismatch(ex)) {
              this.status.login = LOGIN_FAILED_INVALID_PASSPHRASE;
              this.status.sync = CREDENTIALS_CHANGED;
            }
            else {
              // In the absence of further disambiguation or more precise
              // failure constants, just report failure.
              this.status.login = LOGIN_FAILED;
            }
            return false;
          }
        }
        else {
          this._log.info("... 'crypto' is not a reported collection. Generating new keys.");
        }

        if (!cryptoKeys) {
          this._log.info("No keys! Generating new ones.");

          // Better make some and upload them, and wipe the server to ensure
          // consistency. This is all achieved via _freshStart.
          // If _freshStart fails to clear the server or upload keys, it will
          // throw.
          this._freshStart();
          return true;
        }

        // Last-ditch case.
        return false;
      }
      else {
        // No update needed: we're good!
        return true;
      }

    } catch (ex) {
      // This means no keys are present, or there's a network error.
      this._log.debug("Failed to fetch and verify keys: "
                      + Utils.exceptionStr(ex));
      this.errorHandler.checkServerError(ex);
      return false;
    }
  },

  verifyLogin: function verifyLogin() {
    if (!this.identity.username) {
      this._log.warn("No username in verifyLogin.");
      this.status.login = LOGIN_FAILED_NO_USERNAME;
      return false;
    }

    // Unlock master password, or return.
    // Attaching auth credentials to a request requires access to
    // passwords, which means that Resource.get can throw MP-related
    // exceptions!
    // Try to fetch the passphrase first, while we still have control.
    try {
      this.identity.syncKey;
    } catch (ex) {
      this._log.debug("Fetching passphrase threw " + ex +
                      "; assuming master password locked.");
      this.status.login = MASTER_PASSWORD_LOCKED;
      return false;
    }

    try {
      // Make sure we have a cluster to verify against.
      // This is a little weird, if we don't get a node we pretend
      // to succeed, since that probably means we just don't have storage.
      if (this.clusterURL == "" && !this._clusterManager.setCluster()) {
        this.status.sync = NO_SYNC_NODE_FOUND;
        Svc.Obs.notify("weave:service:sync:delayed");
        return true;
      }

      // Fetch collection info on every startup.
      let test = this.resource(this.infoURL).get();

      switch (test.status) {
        case 200:
          // The user is authenticated.

          // We have no way of verifying the passphrase right now,
          // so wait until remoteSetup to do so.
          // Just make the most trivial checks.
          if (!this.identity.syncKey) {
            this._log.warn("No passphrase in verifyLogin.");
            this.status.login = LOGIN_FAILED_NO_PASSPHRASE;
            return false;
          }

          // Go ahead and do remote setup, so that we can determine
          // conclusively that our passphrase is correct.
          if (this._remoteSetup()) {
            // Username/password verified.
            this.status.login = LOGIN_SUCCEEDED;
            return true;
          }

          this._log.warn("Remote setup failed.");
          // Remote setup must have failed.
          return false;

        case 401:
          this._log.warn("401: login failed.");
          // Fall through to the 404 case.

        case 404:
          // Check that we're verifying with the correct cluster
          if (this._clusterManager.setCluster()) {
            return this.verifyLogin();
          }

          // We must have the right cluster, but the server doesn't expect us
          this.status.login = LOGIN_FAILED_LOGIN_REJECTED;
          return false;

        default:
          // Server didn't respond with something that we expected
          this.status.login = LOGIN_FAILED_SERVER_ERROR;
          this.errorHandler.checkServerError(test);
          return false;
      }
    } catch (ex) {
      // Must have failed on some network issue
      this._log.debug("verifyLogin failed: " + Utils.exceptionStr(ex));
      this.status.login = LOGIN_FAILED_NETWORK_ERROR;
      this.errorHandler.checkServerError(ex);
      return false;
    }
  },

  generateNewSymmetricKeys: function generateNewSymmetricKeys() {
    this._log.info("Generating new keys WBO...");
    let wbo = this.collectionKeys.generateNewKeysWBO();
    this._log.info("Encrypting new key bundle.");
    wbo.encrypt(this.identity.syncKeyBundle);

    this._log.info("Uploading...");
    let uploadRes = wbo.upload(this.resource(this.cryptoKeysURL));
    if (uploadRes.status != 200) {
      this._log.warn("Got status " + uploadRes.status + " uploading new keys. What to do? Throw!");
      this.errorHandler.checkServerError(uploadRes);
      throw new Error("Unable to upload symmetric keys.");
    }
    this._log.info("Got status " + uploadRes.status + " uploading keys.");
    let serverModified = uploadRes.obj;   // Modified timestamp according to server.
    this._log.debug("Server reports crypto modified: " + serverModified);

    // Now verify that info/collections shows them!
    this._log.debug("Verifying server collection records.");
    let info = this._fetchInfo();
    this._log.debug("info/collections is: " + info);

    if (info.status != 200) {
      this._log.warn("Non-200 info/collections response. Aborting.");
      throw new Error("Unable to upload symmetric keys.");
    }

    info = info.obj;
    if (!(CRYPTO_COLLECTION in info)) {
      this._log.error("Consistency failure: info/collections excludes " + 
                      "crypto after successful upload.");
      throw new Error("Symmetric key upload failed.");
    }

    // Can't check against local modified: clock drift.
    if (info[CRYPTO_COLLECTION] < serverModified) {
      this._log.error("Consistency failure: info/collections crypto entry " + 
                      "is stale after successful upload.");
      throw new Error("Symmetric key upload failed.");
    }
    
    // Doesn't matter if the timestamp is ahead.
    
    // Download and install them.
    let cryptoKeys = new CryptoWrapper(CRYPTO_COLLECTION, KEYS_WBO);
    let cryptoResp = cryptoKeys.fetch(this.resource(this.cryptoKeysURL)).response;
    if (cryptoResp.status != 200) {
      this._log.warn("Failed to download keys.");
      throw new Error("Symmetric key download failed.");
    }
    let keysChanged = this.handleFetchedKeys(this.identity.syncKeyBundle,
                                             cryptoKeys, true);
    if (keysChanged) {
      this._log.info("Downloaded keys differed, as expected.");
    }
  },

  changePassword: function changePassword(newPassword) {
    let client = new UserAPI10Client(this.userAPIURI);
    let cb = Async.makeSpinningCallback();
    client.changePassword(this.identity.username,
                          this.identity.basicPassword, newPassword, cb);

    try {
      cb.wait();
    } catch (ex) {
      this._log.debug("Password change failed: " +
                      CommonUtils.exceptionStr(ex));
      return false;
    }

    // Save the new password for requests and login manager.
    this.identity.basicPassword = newPassword;
    this.persistLogin();
    return true;
  },

  changePassphrase: function changePassphrase(newphrase) {
    return this._catch(function doChangePasphrase() {
      /* Wipe. */
      this.wipeServer();

      this.logout();

      /* Set this so UI is updated on next run. */
      this.identity.syncKey = newphrase;
      this.persistLogin();

      /* We need to re-encrypt everything, so reset. */
      this.resetClient();
      this.collectionKeys.clear();

      /* Login and sync. This also generates new keys. */
      this.sync();

      Svc.Obs.notify("weave:service:change-passphrase", true);

      return true;
    })();
  },

  startOver: function startOver() {
    this._log.trace("Invoking Service.startOver.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    this.status.resetSync();

    // We want let UI consumers of the following notification know as soon as
    // possible, so let's fake for the CLIENT_NOT_CONFIGURED status for now
    // by emptying the passphrase (we still need the password).
    this.identity.syncKey = null;
    this.status.login = LOGIN_FAILED_NO_PASSPHRASE;
    this.logout();
    Svc.Obs.notify("weave:service:start-over");

    // Deletion doesn't make sense if we aren't set up yet!
    if (this.clusterURL != "") {
      // Clear client-specific data from the server, including disabled engines.
      for each (let engine in [this.clientsEngine].concat(this.engineManager.getAll())) {
        try {
          engine.removeClientData();
        } catch(ex) {
          this._log.warn("Deleting client data for " + engine.name + " failed:"
                         + Utils.exceptionStr(ex));
        }
      }
    } else {
      this._log.debug("Skipping client data removal: no cluster URL.");
    }

    // Reset all engines and clear keys.
    this.resetClient();
    this.collectionKeys.clear();
    this.status.resetBackoff();

    // Reset Weave prefs.
    this._ignorePrefObserver = true;
    Svc.Prefs.resetBranch("");
    this._ignorePrefObserver = false;

    Svc.Prefs.set("lastversion", WEAVE_VERSION);

    this.identity.deleteSyncCredentials();
  },

  persistLogin: function persistLogin() {
    try {
      this.identity.persistCredentials(true);
    } catch (ex) {
      this._log.info("Unable to persist credentials: " + ex);
    }
  },

  login: function login(username, password, passphrase) {
    function onNotify() {
      this._loggedIn = false;
      if (Services.io.offline) {
        this.status.login = LOGIN_FAILED_NETWORK_ERROR;
        throw "Application is offline, login should not be called";
      }

      let initialStatus = this._checkSetup();
      if (username) {
        this.identity.username = username;
      }
      if (password) {
        this.identity.basicPassword = password;
      }
      if (passphrase) {
        this.identity.syncKey = passphrase;
      }

      if (this._checkSetup() == CLIENT_NOT_CONFIGURED) {
        throw "Aborting login, client not configured.";
      }

      // Calling login() with parameters when the client was
      // previously not configured means setup was completed.
      if (initialStatus == CLIENT_NOT_CONFIGURED
          && (username || password || passphrase)) {
        Svc.Obs.notify("weave:service:setup-complete");
      }

      this._log.info("Logging in user " + this.identity.username);
      this._updateCachedURLs();

      if (!this.verifyLogin()) {
        // verifyLogin sets the failure states here.
        throw "Login failed: " + this.status.login;
      }

      this._loggedIn = true;

      return true;
    }

    let notifier = this._notify("login", "", onNotify.bind(this));
    return this._catch(this._lock("service.js: login", notifier))();
  },

  logout: function logout() {
    // No need to do anything if we're already logged out.
    if (!this._loggedIn)
      return;

    this._log.info("Logging out");
    this._loggedIn = false;

    Svc.Obs.notify("weave:service:logout:finish");
  },

  checkAccount: function checkAccount(account) {
    let client = new UserAPI10Client(this.userAPIURI);
    let cb = Async.makeSpinningCallback();

    let username = this.identity.usernameFromAccount(account);
    client.usernameExists(username, cb);

    try {
      let exists = cb.wait();
      return exists ? "notAvailable" : "available";
    } catch (ex) {
      // TODO fix API convention.
      return this.errorHandler.errorStr(ex);
    }
  },

  createAccount: function createAccount(email, password,
                                        captchaChallenge, captchaResponse) {
    let client = new UserAPI10Client(this.userAPIURI);

    // Hint to server to allow scripted user creation or otherwise
    // ignore captcha.
    if (Svc.Prefs.isSet("admin-secret")) {
      client.adminSecret = Svc.Prefs.get("admin-secret", "");
    }

    let cb = Async.makeSpinningCallback();

    client.createAccount(email, password, captchaChallenge, captchaResponse,
                         cb);

    try {
      cb.wait();
      return null;
    } catch (ex) {
      return this.errorHandler.errorStr(ex.body);
    }
  },

  // Stuff we need to do after login, before we can really do
  // anything (e.g. key setup).
  _remoteSetup: function _remoteSetup(infoResponse) {
    let reset = false;

    this._log.debug("Fetching global metadata record");
    let meta = this.recordManager.get(this.metaURL);

    // Checking modified time of the meta record.
    if (infoResponse &&
        (infoResponse.obj.meta != this.metaModified) &&
        (!meta || !meta.isNew)) {

      // Delete the cached meta record...
      this._log.debug("Clearing cached meta record. metaModified is " +
          JSON.stringify(this.metaModified) + ", setting to " +
          JSON.stringify(infoResponse.obj.meta));

      this.recordManager.del(this.metaURL);

      // ... fetch the current record from the server, and COPY THE FLAGS.
      let newMeta = this.recordManager.get(this.metaURL);

      if (!this.recordManager.response.success || !newMeta) {
        this._log.debug("No meta/global record on the server. Creating one.");
        newMeta = new WBORecord("meta", "global");
        newMeta.payload.syncID = this.syncID;
        newMeta.payload.storageVersion = STORAGE_VERSION;

        newMeta.isNew = true;

        this.recordManager.set(this.metaURL, newMeta);
        if (!newMeta.upload(this.resource(this.metaURL)).success) {
          this._log.warn("Unable to upload new meta/global. Failing remote setup.");
          return false;
        }
      } else {
        // If newMeta, then it stands to reason that meta != null.
        newMeta.isNew   = meta.isNew;
        newMeta.changed = meta.changed;
      }

      // Switch in the new meta object and record the new time.
      meta              = newMeta;
      this.metaModified = infoResponse.obj.meta;
    }

    let remoteVersion = (meta && meta.payload.storageVersion)?
      meta.payload.storageVersion : "";

    this._log.debug(["Weave Version:", WEAVE_VERSION, "Local Storage:",
      STORAGE_VERSION, "Remote Storage:", remoteVersion].join(" "));

    // Check for cases that require a fresh start. When comparing remoteVersion,
    // we need to convert it to a number as older clients used it as a string.
    if (!meta || !meta.payload.storageVersion || !meta.payload.syncID ||
        STORAGE_VERSION > parseFloat(remoteVersion)) {

      this._log.info("One of: no meta, no meta storageVersion, or no meta syncID. Fresh start needed.");

      // abort the server wipe if the GET status was anything other than 404 or 200
      let status = this.recordManager.response.status;
      if (status != 200 && status != 404) {
        this.status.sync = METARECORD_DOWNLOAD_FAIL;
        this.errorHandler.checkServerError(this.recordManager.response);
        this._log.warn("Unknown error while downloading metadata record. " +
                       "Aborting sync.");
        return false;
      }

      if (!meta)
        this._log.info("No metadata record, server wipe needed");
      if (meta && !meta.payload.syncID)
        this._log.warn("No sync id, server wipe needed");

      reset = true;

      this._log.info("Wiping server data");
      this._freshStart();

      if (status == 404)
        this._log.info("Metadata record not found, server was wiped to ensure " +
                       "consistency.");
      else // 200
        this._log.info("Wiped server; incompatible metadata: " + remoteVersion);

      return true;
    }
    else if (remoteVersion > STORAGE_VERSION) {
      this.status.sync = VERSION_OUT_OF_DATE;
      this._log.warn("Upgrade required to access newer storage version.");
      return false;
    }
    else if (meta.payload.syncID != this.syncID) {

      this._log.info("Sync IDs differ. Local is " + this.syncID + ", remote is " + meta.payload.syncID);
      this.resetClient();
      this.collectionKeys.clear();
      this.syncID = meta.payload.syncID;
      this._log.debug("Clear cached values and take syncId: " + this.syncID);

      if (!this.upgradeSyncKey(meta.payload.syncID)) {
        this._log.warn("Failed to upgrade sync key. Failing remote setup.");
        return false;
      }

      if (!this.verifyAndFetchSymmetricKeys(infoResponse)) {
        this._log.warn("Failed to fetch symmetric keys. Failing remote setup.");
        return false;
      }

      // bug 545725 - re-verify creds and fail sanely
      if (!this.verifyLogin()) {
        this.status.sync = CREDENTIALS_CHANGED;
        this._log.info("Credentials have changed, aborting sync and forcing re-login.");
        return false;
      }

      return true;
    }
    else {
      if (!this.upgradeSyncKey(meta.payload.syncID)) {
        this._log.warn("Failed to upgrade sync key. Failing remote setup.");
        return false;
      }

      if (!this.verifyAndFetchSymmetricKeys(infoResponse)) {
        this._log.warn("Failed to fetch symmetric keys. Failing remote setup.");
        return false;
      }

      return true;
    }
  },

  /**
   * Return whether we should attempt login at the start of a sync.
   *
   * Note that this function has strong ties to _checkSync: callers
   * of this function should typically use _checkSync to verify that
   * any necessary login took place.
   */
  _shouldLogin: function _shouldLogin() {
    return this.enabled &&
           !Services.io.offline &&
           !this.isLoggedIn;
  },

  /**
   * Determine if a sync should run.
   *
   * @param ignore [optional]
   *        array of reasons to ignore when checking
   *
   * @return Reason for not syncing; not-truthy if sync should run
   */
  _checkSync: function _checkSync(ignore) {
    let reason = "";
    if (!this.enabled)
      reason = kSyncWeaveDisabled;
    else if (Services.io.offline)
      reason = kSyncNetworkOffline;
    else if (this.status.minimumNextSync > Date.now())
      reason = kSyncBackoffNotMet;
    else if ((this.status.login == MASTER_PASSWORD_LOCKED) &&
             Utils.mpLocked())
      reason = kSyncMasterPasswordLocked;
    else if (Svc.Prefs.get("firstSync") == "notReady")
      reason = kFirstSyncChoiceNotMade;

    if (ignore && ignore.indexOf(reason) != -1)
      return "";

    return reason;
  },

  sync: function sync() {
    let dateStr = new Date().toLocaleFormat(LOG_DATE_FORMAT);
    this._log.debug("User-Agent: " + SyncStorageRequest.prototype.userAgent);
    this._log.info("Starting sync at " + dateStr);
    this._catch(function () {
      // Make sure we're logged in.
      if (this._shouldLogin()) {
        this._log.debug("In sync: should login.");
        if (!this.login()) {
          this._log.debug("Not syncing: login returned false.");
          return;
        }
      }
      else {
        this._log.trace("In sync: no need to login.");
      }
      return this._lockedSync.apply(this, arguments);
    })();
  },

  /**
   * Sync up engines with the server.
   */
  _lockedSync: function _lockedSync() {
    return this._lock("service.js: sync",
                      this._notify("sync", "", function onNotify() {

      let synchronizer = new EngineSynchronizer(this);
      let cb = Async.makeSpinningCallback();
      synchronizer.onComplete = cb;

      synchronizer.sync();
      // wait() throws if the first argument is truthy, which is exactly what
      // we want.
      let result = cb.wait();
    }))();
  },

  /**
   * If we have a passphrase, rather than a 25-alphadigit sync key,
   * use the provided sync ID to bootstrap it using PBKDF2.
   *
   * Store the new 'passphrase' back into the identity manager.
   *
   * We can check this as often as we want, because once it's done the
   * check will no longer succeed. It only matters that it happens after
   * we decide to bump the server storage version.
   */
  upgradeSyncKey: function upgradeSyncKey(syncID) {
    let p = this.identity.syncKey;

    if (!p) {
      return false;
    }

    // Check whether it's already a key that we generated.
    if (Utils.isPassphrase(p)) {
      this._log.info("Sync key is up-to-date: no need to upgrade.");
      return true;
    }

    // Otherwise, let's upgrade it.
    // N.B., we persist the sync key without testing it first...

    let s = btoa(syncID);        // It's what WeaveCrypto expects. *sigh*
    let k = Utils.derivePresentableKeyFromPassphrase(p, s, PBKDF2_KEY_BYTES);   // Base 32.

    if (!k) {
      this._log.error("No key resulted from derivePresentableKeyFromPassphrase. Failing upgrade.");
      return false;
    }

    this._log.info("Upgrading sync key...");
    this.identity.syncKey = k;
    this._log.info("Saving upgraded sync key...");
    this.persistLogin();
    this._log.info("Done saving.");
    return true;
  },

  _freshStart: function _freshStart() {
    this._log.info("Fresh start. Resetting client and considering key upgrade.");
    this.resetClient();
    this.collectionKeys.clear();
    this.upgradeSyncKey(this.syncID);

    // Wipe the server.
    let wipeTimestamp = this.wipeServer();

    // Upload a new meta/global record.
    let meta = new WBORecord("meta", "global");
    meta.payload.syncID = this.syncID;
    meta.payload.storageVersion = STORAGE_VERSION;
    meta.isNew = true;

    this._log.debug("New metadata record: " + JSON.stringify(meta.payload));
    let res = this.resource(this.metaURL);
    // It would be good to set the X-If-Unmodified-Since header to `timestamp`
    // for this PUT to ensure at least some level of transactionality.
    // Unfortunately, the servers don't support it after a wipe right now
    // (bug 693893), so we're going to defer this until bug 692700.
    let resp = res.put(meta);
    if (!resp.success) {
      // If we got into a race condition, we'll abort the sync this way, too.
      // That's fine. We'll just wait till the next sync. The client that we're
      // racing is probably busy uploading stuff right now anyway.
      throw resp;
    }
    this.recordManager.set(this.metaURL, meta);

    // Wipe everything we know about except meta because we just uploaded it
    let engines = [this.clientsEngine].concat(this.engineManager.getAll());
    let collections = [engine.name for each (engine in engines)];

    // Generate, upload, and download new keys. Do this last so we don't wipe
    // them...
    this.generateNewSymmetricKeys();
  },

  /**
   * Wipe user data from the server.
   *
   * @param collections [optional]
   *        Array of collections to wipe. If not given, all collections are
   *        wiped by issuing a DELETE request for `storageURL`.
   *
   * @return the server's timestamp of the (last) DELETE.
   */
  wipeServer: function wipeServer(collections) {
    let response;
    if (!collections) {
      // Strip the trailing slash.
      let res = this.resource(this.storageURL.slice(0, -1));
      res.setHeader("X-Confirm-Delete", "1");
      try {
        response = res.delete();
      } catch (ex) {
        this._log.debug("Failed to wipe server: " + CommonUtils.exceptionStr(ex));
        throw ex;
      }
      if (response.status != 200 && response.status != 404) {
        this._log.debug("Aborting wipeServer. Server responded with " +
                        response.status + " response for " + this.storageURL);
        throw response;
      }
      return response.headers["x-weave-timestamp"];
    }

    let timestamp;
    for (let name of collections) {
      let url = this.storageURL + name;
      try {
        response = this.resource(url).delete();
      } catch (ex) {
        this._log.debug("Failed to wipe '" + name + "' collection: " +
                        Utils.exceptionStr(ex));
        throw ex;
      }

      if (response.status != 200 && response.status != 404) {
        this._log.debug("Aborting wipeServer. Server responded with " +
                        response.status + " response for " + url);
        throw response;
      }

      if ("x-weave-timestamp" in response.headers) {
        timestamp = response.headers["x-weave-timestamp"];
      }
    }

    return timestamp;
  },

  /**
   * Wipe all local user data.
   *
   * @param engines [optional]
   *        Array of engine names to wipe. If not given, all engines are used.
   */
  wipeClient: function wipeClient(engines) {
    // If we don't have any engines, reset the service and wipe all engines
    if (!engines) {
      // Clear out any service data
      this.resetService();

      engines = [this.clientsEngine].concat(this.engineManager.getAll());
    }
    // Convert the array of names into engines
    else {
      engines = this.engineManager.get(engines);
    }

    // Fully wipe each engine if it's able to decrypt data
    for each (let engine in engines) {
      if (engine.canDecrypt()) {
        engine.wipeClient();
      }
    }

    // Save the password/passphrase just in-case they aren't restored by sync
    this.persistLogin();
  },

  /**
   * Wipe all remote user data by wiping the server then telling each remote
   * client to wipe itself.
   *
   * @param engines [optional]
   *        Array of engine names to wipe. If not given, all engines are used.
   */
  wipeRemote: function wipeRemote(engines) {
    try {
      // Make sure stuff gets uploaded.
      this.resetClient(engines);

      // Clear out any server data.
      this.wipeServer(engines);

      // Only wipe the engines provided.
      if (engines) {
        engines.forEach(function(e) this.clientsEngine.sendCommand("wipeEngine", [e]), this);
      }
      // Tell the remote machines to wipe themselves.
      else {
        this.clientsEngine.sendCommand("wipeAll", []);
      }

      // Make sure the changed clients get updated.
      this.clientsEngine.sync();
    } catch (ex) {
      this.errorHandler.checkServerError(ex);
      throw ex;
    }
  },

  /**
   * Reset local service information like logs, sync times, caches.
   */
  resetService: function resetService() {
    this._catch(function reset() {
      this._log.info("Service reset.");

      // Pretend we've never synced to the server and drop cached data
      this.syncID = "";
      this.recordManager.clearCache();
    })();
  },

  /**
   * Reset the client by getting rid of any local server data and client data.
   *
   * @param engines [optional]
   *        Array of engine names to reset. If not given, all engines are used.
   */
  resetClient: function resetClient(engines) {
    this._catch(function doResetClient() {
      // If we don't have any engines, reset everything including the service
      if (!engines) {
        // Clear out any service data
        this.resetService();

        engines = [this.clientsEngine].concat(this.engineManager.getAll());
      }
      // Convert the array of names into engines
      else {
        engines = this.engineManager.get(engines);
      }

      // Have each engine drop any temporary meta data
      for each (let engine in engines) {
        engine.resetClient();
      }
    })();
  },

  /**
   * Fetch storage info from the server.
   *
   * @param type
   *        String specifying what info to fetch from the server. Must be one
   *        of the INFO_* values. See Sync Storage Server API spec for details.
   * @param callback
   *        Callback function with signature (error, data) where `data' is
   *        the return value from the server already parsed as JSON.
   *
   * @return RESTRequest instance representing the request, allowing callers
   *         to cancel the request.
   */
  getStorageInfo: function getStorageInfo(type, callback) {
    if (STORAGE_INFO_TYPES.indexOf(type) == -1) {
      throw "Invalid value for 'type': " + type;
    }

    let info_type = "info/" + type;
    this._log.trace("Retrieving '" + info_type + "'...");
    let url = this.userBaseURL + info_type;
    return this.getStorageRequest(url).get(function onComplete(error) {
      // Note: 'this' is the request.
      if (error) {
        this._log.debug("Failed to retrieve '" + info_type + "': " +
                        Utils.exceptionStr(error));
        return callback(error);
      }
      if (this.response.status != 200) {
        this._log.debug("Failed to retrieve '" + info_type +
                        "': server responded with HTTP" +
                        this.response.status);
        return callback(this.response);
      }

      let result;
      try {
        result = JSON.parse(this.response.body);
      } catch (ex) {
        this._log.debug("Server returned invalid JSON for '" + info_type +
                        "': " + this.response.body);
        return callback(ex);
      }
      this._log.trace("Successfully retrieved '" + info_type + "'.");
      return callback(null, result);
    });
  },
};

this.Service = new Sync11Service();
Service.onStartup();
