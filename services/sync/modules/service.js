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
 *  Myk Melez <myk@mozilla.org>
 *  Anant Narayanan <anant@kix.in>
 *  Philipp von Weitershausen <philipp@weitershausen.de>
 *  Richard Newman <rnewman@mozilla.com>
 *  Marina Samuel <msamuel@mozilla.com>
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

// 'Weave' continues to be exported for backwards compatibility.
const EXPORTED_SYMBOLS = ["Service", "Weave"];

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

const LOG_DATE_FORMAT = "%Y-%m-%d %H:%M:%S";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/ext/Preferences.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/rest.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/main.js");

const STORAGE_INFO_TYPES = [INFO_COLLECTIONS,
                            INFO_COLLECTION_USAGE,
                            INFO_COLLECTION_COUNTS,
                            INFO_QUOTA];

/*
 * Service singleton
 * Main entry point into Weave's sync framework
 */

function WeaveSvc() {
  this._notify = Utils.notify("weave:service:");
}
WeaveSvc.prototype = {

  _lock: Utils.lock,
  _locked: false,
  _loggedIn: false,

  get account() Svc.Prefs.get("account", this.username),
  set account(value) {
    if (value) {
      value = value.toLowerCase();
      Svc.Prefs.set("account", value);
    } else {
      Svc.Prefs.reset("account");
    }
    this.username = this._usernameFromAccount(value);
  },

  _usernameFromAccount: function _usernameFromAccount(value) {
    // If we encounter characters not allowed by the API (as found for
    // instance in an email address), hash the value.
    if (value && value.match(/[^A-Z0-9._-]/i))
      return Utils.sha1Base32(value.toLowerCase()).toLowerCase();
    return value;
  },

  get username() {
    return Svc.Prefs.get("username", "").toLowerCase();
  },
  set username(value) {
    if (value) {
      // Make sure all uses of this new username is lowercase
      value = value.toLowerCase();
      Svc.Prefs.set("username", value);
    }
    else
      Svc.Prefs.reset("username");

    // fixme - need to loop over all Identity objects - needs some rethinking...
    ID.get('WeaveID').username = value;
    ID.get('WeaveCryptoID').username = value;

    // FIXME: need to also call this whenever the username pref changes
    this._updateCachedURLs();
  },

  get password() ID.get("WeaveID").password,
  set password(value) ID.get("WeaveID").password = value,

  get passphrase() ID.get("WeaveCryptoID").keyStr,
  set passphrase(value) ID.get("WeaveCryptoID").keyStr = value,

  get syncKeyBundle() ID.get("WeaveCryptoID"),

  get serverURL() Svc.Prefs.get("serverURL"),
  set serverURL(value) {
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

  get userAPI() {
    // Append to the serverURL if it's a relative fragment
    let user = Svc.Prefs.get("userURL");
    if (user.indexOf(":") == -1)
      user = this.serverURL + user;
    return user + USER_API_VERSION + "/";
  },

  get pwResetURL() {
    return this.serverURL + "weave-password-reset";
  },

  get updatedURL() {
    return WEAVE_CHANNEL == "dev" ? UPDATED_DEV_URL : UPDATED_REL_URL;
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
  lock: function Svc_lock() {
    if (this._locked)
      return false;
    this._locked = true;
    return true;
  },
  unlock: function Svc_unlock() {
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
    if (this.clusterURL == "" || this.username == "")
      return;

    let storageAPI = this.clusterURL + SYNC_API_VERSION + "/";
    this.userBaseURL = storageAPI + this.username + "/";
    this._log.debug("Caching URLs under storage user base: " + this.userBaseURL);

    // Generate and cache various URLs under the storage API for this user
    this.infoURL = this.userBaseURL + "info/collections";
    this.storageURL = this.userBaseURL + "storage/";
    this.metaURL = this.storageURL + "meta/global";
    this.cryptoKeysURL = this.storageURL + CRYPTO_COLLECTION + "/" + KEYS_WBO;
  },

  _checkCrypto: function WeaveSvc__checkCrypto() {
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
      let cryptoResp = cryptoKeys.fetch(this.cryptoKeysURL).response;

      // Save out the ciphertext for when we reupload. If there's a bug in
      // CollectionKeys, this will prevent us from uploading junk.
      let cipherText = cryptoKeys.ciphertext;

      if (!cryptoResp.success) {
        this._log.warn("Failed to download keys.");
        return false;
      }

      let keysChanged = this.handleFetchedKeys(this.syncKeyBundle,
                                               cryptoKeys, true);
      if (keysChanged) {
        // Did they change? If so, carry on.
        this._log.info("Suggesting retry.");
        return true;              // Try again.
      }

      // If not, reupload them and continue the current sync.
      cryptoKeys.ciphertext = cipherText;
      cryptoKeys.cleartext  = null;

      let uploadResp = cryptoKeys.upload(this.cryptoKeysURL);
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
    // This is largely relevant because we don't persist
    // CollectionKeys yet: Bug 610913.
    let wasBlank = CollectionKeys.isClear;
    let keysChanged = CollectionKeys.updateContents(syncKey, cryptoKeys);

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
    ErrorHandler.init();

    this._log = Log4Moz.repository.getLogger("Sync.Service");
    this._log.level =
      Log4Moz.Level[Svc.Prefs.get("log.logger.service.main")];

    this._log.info("Loading Weave " + WEAVE_VERSION);

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

    SyncScheduler.init();

    if (!this.enabled)
      this._log.info("Weave Sync disabled");

    // Create Weave identities (for logging in, and for encryption)
    let id = ID.get("WeaveID");
    if (!id)
      id = ID.set("WeaveID", new Identity(PWDMGR_PASSWORD_REALM, this.username));
    Auth.defaultAuthenticator = new BasicAuthenticator(id);

    if (!ID.get("WeaveCryptoID"))
      ID.set("WeaveCryptoID",
             new SyncKeyBundle(PWDMGR_PASSPHRASE_REALM, this.username));

    this._updateCachedURLs();

    let status = this._checkSetup();
    if (status != STATUS_DISABLED && status != CLIENT_NOT_CONFIGURED)
      Svc.Obs.notify("weave:engine:start-tracking");

    // Send an event now that Weave service is ready.  We don't do this
    // synchronously so that observers can import this module before
    // registering an observer.
    Utils.nextTick(function() {
      Status.ready = true;
      Svc.Obs.notify("weave:service:ready");
    });
  },

  _checkSetup: function WeaveSvc__checkSetup() {
    if (!this.enabled)
      return Status.service = STATUS_DISABLED;
    return Status.checkSetup();
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
  _registerEngines: function WeaveSvc__registerEngines() {
    let engines = [];
    // Applications can provide this preference (comma-separated list)
    // to specify which engines should be registered on startup.
    let pref = Svc.Prefs.get("registerEngines");
    if (pref) {
      engines = pref.split(",");
    }

    // Grab the actual engines and register them
    Engines.register(engines.map(function(name) Weave[name + "Engine"]));
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  // nsIObserver

  observe: function WeaveSvc__observe(subject, topic, data) {
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

  // gets cluster from central LDAP server and returns it, or null on error
  _findCluster: function _findCluster() {
    this._log.debug("Finding cluster for user " + this.username);

    let fail;
    let res = new Resource(this.userAPI + this.username + "/node/weave");
    try {
      let node = res.get();
      switch (node.status) {
        case 400:
          Status.login = LOGIN_FAILED_LOGIN_REJECTED;
          fail = "Find cluster denied: " + ErrorHandler.errorStr(node);
          break;
        case 404:
          this._log.debug("Using serverURL as data cluster (multi-cluster support disabled)");
          return this.serverURL;
        case 0:
        case 200:
          if (node == "null")
            node = null;
          return node;
        default:
          ErrorHandler.checkServerError(node);
          fail = "Unexpected response code: " + node.status;
          break;
      }
    } catch (e) {
      this._log.debug("Network error on findCluster");
      Status.login = LOGIN_FAILED_NETWORK_ERROR;
      ErrorHandler.checkServerError(e);
      fail = e;
    }
    throw fail;
  },

  // gets cluster from central LDAP server and sets this.clusterURL
  _setCluster: function _setCluster() {
    // Make sure we didn't get some unexpected response for the cluster
    let cluster = this._findCluster();
    this._log.debug("Cluster value = " + cluster);
    if (cluster == null)
      return false;

    // Don't update stuff if we already have the right cluster
    if (cluster == this.clusterURL)
      return false;

    this._log.debug("Setting cluster to " + cluster);
    this.clusterURL = cluster;
    Svc.Prefs.set("lastClusterUpdate", Date.now().toString());
    return true;
  },

  // Update cluster if required.
  // Returns false if the update was not required.
  _updateCluster: function _updateCluster() {
    this._log.info("Updating cluster.");
    let cTime = Date.now();
    let lastUp = parseFloat(Svc.Prefs.get("lastClusterUpdate"));
    if (!lastUp || ((cTime - lastUp) >= CLUSTER_BACKOFF)) {
      return this._setCluster();
    }
    return false;
  },

  /**
   * Perform the info fetch as part of a login or key fetch.
   */
  _fetchInfo: function _fetchInfo(url) {
    let infoURL = url || this.infoURL;

    this._log.trace("In _fetchInfo: " + infoURL);
    let info;
    try {
      info = new Resource(infoURL).get();
    } catch (ex) {
      ErrorHandler.checkServerError(ex);
      throw ex;
    }
    if (!info.success) {
      ErrorHandler.checkServerError(info);
      throw "aborting sync, failed to get collections";
    }
    return info;
  },

  verifyAndFetchSymmetricKeys: function verifyAndFetchSymmetricKeys(infoResponse) {

    this._log.debug("Fetching and verifying -- or generating -- symmetric keys.");

    // Don't allow empty/missing passphrase.
    // Furthermore, we assume that our sync key is already upgraded,
    // and fail if that assumption is invalidated.

    let syncKey = this.syncKeyBundle;
    if (!syncKey) {
      this._log.error("No sync key: cannot fetch symmetric keys.");
      Status.login = LOGIN_FAILED_NO_PASSPHRASE;
      Status.sync = CREDENTIALS_CHANGED;             // For want of a better option.
      return false;
    }

    // Not sure this validation is necessary now.
    if (!Utils.isPassphrase(syncKey.keyStr)) {
      this._log.warn("Sync key input is invalid: cannot fetch symmetric keys.");
      Status.login = LOGIN_FAILED_INVALID_PASSPHRASE;
      Status.sync = CREDENTIALS_CHANGED;
      return false;
    }

    try {
      if (!infoResponse)
        infoResponse = this._fetchInfo();    // Will throw an exception on failure.

      // This only applies when the server is already at version 4.
      if (infoResponse.status != 200) {
        this._log.warn("info/collections returned non-200 response. Failing key fetch.");
        Status.login = LOGIN_FAILED_SERVER_ERROR;
        ErrorHandler.checkServerError(infoResponse);
        return false;
      }

      let infoCollections = infoResponse.obj;

      this._log.info("Testing info/collections: " + JSON.stringify(infoCollections));

      if (CollectionKeys.updateNeeded(infoCollections)) {
        this._log.info("CollectionKeys reports that a key update is needed.");

        // Don't always set to CREDENTIALS_CHANGED -- we will probably take care of this.

        // Fetch storage/crypto/keys.
        let cryptoKeys;

        if (infoCollections && (CRYPTO_COLLECTION in infoCollections)) {
          try {
            cryptoKeys = new CryptoWrapper(CRYPTO_COLLECTION, KEYS_WBO);
            let cryptoResp = cryptoKeys.fetch(this.cryptoKeysURL).response;

            if (cryptoResp.success) {
              let keysChanged = this.handleFetchedKeys(syncKey, cryptoKeys);
              return true;
            }
            else if (cryptoResp.status == 404) {
              // On failure, ask CollectionKeys to generate new keys and upload them.
              // Fall through to the behavior below.
              this._log.warn("Got 404 for crypto/keys, but 'crypto' in info/collections. Regenerating.");
              cryptoKeys = null;
            }
            else {
              // Some other problem.
              Status.login = LOGIN_FAILED_SERVER_ERROR;
              ErrorHandler.checkServerError(cryptoResp);
              this._log.warn("Got status " + cryptoResp.status + " fetching crypto keys.");
              return false;
            }
          }
          catch (ex) {
            this._log.warn("Got exception \"" + ex + "\" fetching cryptoKeys.");
            // TODO: Um, what exceptions might we get here? Should we re-throw any?

            // One kind of exception: HMAC failure.
            if (Utils.isHMACMismatch(ex)) {
              Status.login = LOGIN_FAILED_INVALID_PASSPHRASE;
              Status.sync = CREDENTIALS_CHANGED;
            }
            else {
              // In the absence of further disambiguation or more precise
              // failure constants, just report failure.
              Status.login = LOGIN_FAILED;
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

    } catch (e) {
      // This means no keys are present, or there's a network error.
      return false;
    }
  },

  verifyLogin: function verifyLogin()
    this._notify("verify-login", "", function() {
      if (!this.username) {
        this._log.warn("No username in verifyLogin.");
        Status.login = LOGIN_FAILED_NO_USERNAME;
        return false;
      }

      // Unlock master password, or return.
      // Attaching auth credentials to a request requires access to
      // passwords, which means that Resource.get can throw MP-related
      // exceptions!
      // Try to fetch the passphrase first, while we still have control.
      try {
        this.passphrase;
      } catch (ex) {
        this._log.debug("Fetching passphrase threw " + ex +
                        "; assuming master password locked.");
        Status.login = MASTER_PASSWORD_LOCKED;
        return false;
      }

      try {
        // Make sure we have a cluster to verify against
        // this is a little weird, if we don't get a node we pretend
        // to succeed, since that probably means we just don't have storage
        if (this.clusterURL == "" && !this._setCluster()) {
          Status.sync = NO_SYNC_NODE_FOUND;
          Svc.Obs.notify("weave:service:sync:delayed");
          return true;
        }

        // Fetch collection info on every startup.
        let test = new Resource(this.infoURL).get();

        switch (test.status) {
          case 200:
            // The user is authenticated.

            // We have no way of verifying the passphrase right now,
            // so wait until remoteSetup to do so.
            // Just make the most trivial checks.
            if (!this.passphrase) {
              this._log.warn("No passphrase in verifyLogin.");
              Status.login = LOGIN_FAILED_NO_PASSPHRASE;
              return false;
            }

            // Go ahead and do remote setup, so that we can determine
            // conclusively that our passphrase is correct.
            if (this._remoteSetup()) {
              // Username/password verified.
              Status.login = LOGIN_SUCCEEDED;
              return true;
            }

            this._log.warn("Remote setup failed.");
            // Remote setup must have failed.
            return false;

          case 401:
            this._log.warn("401: login failed.");
            // Login failed.  If the password contains non-ASCII characters,
            // perhaps the server password is an old low-byte only one?
            let id = ID.get('WeaveID');
            if (id.password != id.passwordUTF8) {
              let res = new Resource(this.infoURL);
              let auth = new BrokenBasicAuthenticator(id);
              res.authenticator = auth;
              test = res.get();
              if (test.status == 200) {
                this._log.debug("Non-ASCII password detected. "
                                + "Changing to UTF-8 version.");
                // Let's change the password on the server to the UTF8 version.
                let url = this.userAPI + this.username + "/password";
                res = new Resource(url);
                res.authenticator = auth;
                res.post(id.passwordUTF8);
                return this.verifyLogin();
              }
            }
            // Yes, we want to fall through to the 404 case.
          case 404:
            // Check that we're verifying with the correct cluster
            if (this._setCluster())
              return this.verifyLogin();

            // We must have the right cluster, but the server doesn't expect us
            Status.login = LOGIN_FAILED_LOGIN_REJECTED;
            return false;

          default:
            // Server didn't respond with something that we expected
            Status.login = LOGIN_FAILED_SERVER_ERROR;
            ErrorHandler.checkServerError(test);
            return false;
        }
      }
      catch (ex) {
        // Must have failed on some network issue
        this._log.debug("verifyLogin failed: " + Utils.exceptionStr(ex));
        Status.login = LOGIN_FAILED_NETWORK_ERROR;
        ErrorHandler.checkServerError(ex);
        return false;
      }
    })(),

  generateNewSymmetricKeys:
  function WeaveSvc_generateNewSymmetricKeys() {
    this._log.info("Generating new keys WBO...");
    let wbo = CollectionKeys.generateNewKeysWBO();
    this._log.info("Encrypting new key bundle.");
    wbo.encrypt(this.syncKeyBundle);

    this._log.info("Uploading...");
    let uploadRes = wbo.upload(this.cryptoKeysURL);
    if (uploadRes.status != 200) {
      this._log.warn("Got status " + uploadRes.status + " uploading new keys. What to do? Throw!");
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
    let cryptoResp = cryptoKeys.fetch(this.cryptoKeysURL).response;
    if (cryptoResp.status != 200) {
      this._log.warn("Failed to download keys.");
      throw new Error("Symmetric key download failed.");
    }
    let keysChanged = this.handleFetchedKeys(this.syncKeyBundle,
                                             cryptoKeys, true);
    if (keysChanged) {
      this._log.info("Downloaded keys differed, as expected.");
    }
  },

  changePassword: function WeaveSvc_changePassword(newpass)
    this._notify("changepwd", "", function() {
      let url = this.userAPI + this.username + "/password";
      try {
        let resp = new Resource(url).post(Utils.encodeUTF8(newpass));
        if (resp.status != 200) {
          this._log.debug("Password change failed: " + resp);
          return false;
        }
      }
      catch(ex) {
        // Must have failed on some network issue
        this._log.debug("changePassword failed: " + Utils.exceptionStr(ex));
        return false;
      }

      // Save the new password for requests and login manager.
      this.password = newpass;
      this.persistLogin();
      return true;
    })(),

  changePassphrase: function WeaveSvc_changePassphrase(newphrase)
    this._catch(this._notify("changepph", "", function() {
      /* Wipe. */
      this.wipeServer();

      this.logout();

      /* Set this so UI is updated on next run. */
      this.passphrase = newphrase;
      this.persistLogin();

      /* We need to re-encrypt everything, so reset. */
      this.resetClient();
      CollectionKeys.clear();

      /* Login and sync. This also generates new keys. */
      this.sync();
      return true;
    }))(),

  startOver: function() {
    Svc.Obs.notify("weave:engine:stop-tracking");

    // We want let UI consumers of the following notification know as soon as
    // possible, so let's fake for the CLIENT_NOT_CONFIGURED status for now
    // by emptying the passphrase (we still need the password).
    Service.passphrase = "";
    Status.login = LOGIN_FAILED_NO_PASSPHRASE;
    this.logout();
    Svc.Obs.notify("weave:service:start-over");

    // Deletion doesn't make sense if we aren't set up yet!
    if (this.clusterURL != "") {
      // Clear client-specific data from the server, including disabled engines.
      for each (let engine in [Clients].concat(Engines.getAll())) {
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
    CollectionKeys.clear();
    Status.resetBackoff();
    Status.resetSync();

    // Reset Weave prefs.
    this._ignorePrefObserver = true;
    Svc.Prefs.resetBranch("");
    this._ignorePrefObserver = false;

    Svc.Prefs.set("lastversion", WEAVE_VERSION);
    // Find weave logins and remove them.
    this.password = "";
    this.passphrase = "";
    Services.logins.findLogins({}, PWDMGR_HOST, "", "").map(function(login) {
      Services.logins.removeLogin(login);
    });
  },

  persistLogin: function persistLogin() {
    // Canceled master password prompt can prevent these from succeeding.
    try {
      ID.get("WeaveID").persist();
      ID.get("WeaveCryptoID").persist();
    }
    catch(ex) {}
  },

  login: function WeaveSvc_login(username, password, passphrase)
    this._catch(this._lock("service.js: login",
          this._notify("login", "", function() {
      this._loggedIn = false;
      if (Services.io.offline) {
        Status.login = LOGIN_FAILED_NETWORK_ERROR;
        throw "Application is offline, login should not be called";
      }

      let initialStatus = this._checkSetup();
      if (username)
        this.username = username;
      if (password)
        this.password = password;
      if (passphrase)
        this.passphrase = passphrase;

      if (this._checkSetup() == CLIENT_NOT_CONFIGURED)
        throw "aborting login, client not configured";

      // Calling login() with parameters when the client was
      // previously not configured means setup was completed.
      if (initialStatus == CLIENT_NOT_CONFIGURED
          && (username || password || passphrase))
        Svc.Obs.notify("weave:service:setup-complete");

      this._log.info("Logging in user " + this.username);

      if (!this.verifyLogin()) {
        // verifyLogin sets the failure states here.
        throw "Login failed: " + Status.login;
      }

      this._loggedIn = true;

      return true;
    })))(),

  logout: function WeaveSvc_logout() {
    // No need to do anything if we're already logged out.
    if (!this._loggedIn)
      return;

    this._log.info("Logging out");
    this._loggedIn = false;

    Svc.Obs.notify("weave:service:logout:finish");
  },

  checkAccount: function checkAccount(account) {
    let username = this._usernameFromAccount(account);
    let url = this.userAPI + username;
    let res = new Resource(url);
    res.authenticator = new NoOpAuthenticator();

    let data = "";
    try {
      data = res.get();
      if (data.status == 200) {
        if (data == "0")
          return "available";
        else if (data == "1")
          return "notAvailable";
      }

    }
    catch(ex) {}

    // Convert to the error string, or default to generic on exception.
    return ErrorHandler.errorStr(data);
  },

  createAccount: function createAccount(email, password,
                                        captchaChallenge, captchaResponse) {
    let username = this._usernameFromAccount(email);
    let payload = JSON.stringify({
      "password": Utils.encodeUTF8(password),
      "email": email,
      "captcha-challenge": captchaChallenge,
      "captcha-response": captchaResponse
    });

    let url = this.userAPI + username;
    let res = new Resource(url);
    res.authenticator = new NoOpAuthenticator();

    // Hint to server to allow scripted user creation or otherwise
    // ignore captcha.
    if (Svc.Prefs.isSet("admin-secret"))
      res.setHeader("X-Weave-Secret", Svc.Prefs.get("admin-secret", ""));

    let error = "generic-server-error";
    try {
      let register = res.put(payload);
      if (register.success) {
        this._log.info("Account created: " + register);
        return null;
      }

      // Must have failed, so figure out the reason
      if (register.status == 400)
        error = ErrorHandler.errorStr(register);
    }
    catch(ex) {
      this._log.warn("Failed to create account: " + ex);
    }

    return error;
  },

  // Stuff we need to do after login, before we can really do
  // anything (e.g. key setup).
  _remoteSetup: function WeaveSvc__remoteSetup(infoResponse) {
    let reset = false;

    this._log.debug("Fetching global metadata record");
    let meta = Records.get(this.metaURL);

    // Checking modified time of the meta record.
    if (infoResponse &&
        (infoResponse.obj.meta != this.metaModified) &&
        (!meta || !meta.isNew)) {

      // Delete the cached meta record...
      this._log.debug("Clearing cached meta record. metaModified is " +
          JSON.stringify(this.metaModified) + ", setting to " +
          JSON.stringify(infoResponse.obj.meta));

      Records.del(this.metaURL);

      // ... fetch the current record from the server, and COPY THE FLAGS.
      let newMeta       = Records.get(this.metaURL);

      if (!Records.response.success || !newMeta) {
        this._log.debug("No meta/global record on the server. Creating one.");
        newMeta = new WBORecord("meta", "global");
        newMeta.payload.syncID = this.syncID;
        newMeta.payload.storageVersion = STORAGE_VERSION;

        newMeta.isNew = true;

        Records.set(this.metaURL, newMeta);
        if (!newMeta.upload(this.metaURL).success) {
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
      let status = Records.response.status;
      if (status != 200 && status != 404) {
        Status.sync = METARECORD_DOWNLOAD_FAIL;
        ErrorHandler.checkServerError(Records.response);
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
      Status.sync = VERSION_OUT_OF_DATE;
      this._log.warn("Upgrade required to access newer storage version.");
      return false;
    }
    else if (meta.payload.syncID != this.syncID) {

      this._log.info("Sync IDs differ. Local is " + this.syncID + ", remote is " + meta.payload.syncID);
      this.resetClient();
      CollectionKeys.clear();
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
        Status.sync = CREDENTIALS_CHANGED;
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
  _checkSync: function WeaveSvc__checkSync(ignore) {
    let reason = "";
    if (!this.enabled)
      reason = kSyncWeaveDisabled;
    else if (Services.io.offline)
      reason = kSyncNetworkOffline;
    else if (Status.minimumNextSync > Date.now())
      reason = kSyncBackoffNotMet;
    else if ((Status.login == MASTER_PASSWORD_LOCKED) &&
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
  _lockedSync: function _lockedSync()
    this._lock("service.js: sync",
               this._notify("sync", "", function() {

    this._log.info("In sync().");

    let syncStartTime = Date.now();

    Status.resetSync();

    // Make sure we should sync or record why we shouldn't
    let reason = this._checkSync();
    if (reason) {
      if (reason == kSyncNetworkOffline) {
        Status.sync = LOGIN_FAILED_NETWORK_ERROR;
      }
      // this is a purposeful abort rather than a failure, so don't set
      // any status bits
      reason = "Can't sync: " + reason;
      throw reason;
    }

    // if we don't have a node, get one.  if that fails, retry in 10 minutes
    if (this.clusterURL == "" && !this._setCluster()) {
      Status.sync = NO_SYNC_NODE_FOUND;
      return;
    }

    // Ping the server with a special info request once a day.
    let infoURL = this.infoURL;
    let now = Math.floor(Date.now() / 1000);
    let lastPing = Svc.Prefs.get("lastPing", 0);
    if (now - lastPing > 86400) { // 60 * 60 * 24
      infoURL += "?v=" + WEAVE_VERSION;
      Svc.Prefs.set("lastPing", now);
    }

    // Figure out what the last modified time is for each collection
    let info = this._fetchInfo(infoURL);

    // Convert the response to an object and read out the modified times
    for each (let engine in [Clients].concat(Engines.getAll()))
      engine.lastModified = info.obj[engine.name] || 0;

    if (!(this._remoteSetup(info)))
      throw "aborting sync, remote setup failed";

    // Make sure we have an up-to-date list of clients before sending commands
    this._log.debug("Refreshing client list.");
    if (!this._syncEngine(Clients)) {
      // Clients is an engine like any other; it can fail with a 401,
      // and we can elect to abort the sync.
      this._log.warn("Client engine sync failed. Aborting.");
      return;
    }

    // Wipe data in the desired direction if necessary
    switch (Svc.Prefs.get("firstSync")) {
      case "resetClient":
        this.resetClient(Engines.getEnabled().map(function(e) e.name));
        break;
      case "wipeClient":
        this.wipeClient(Engines.getEnabled().map(function(e) e.name));
        break;
      case "wipeRemote":
        this.wipeRemote(Engines.getEnabled().map(function(e) e.name));
        break;
    }

    if (Clients.localCommands) {
      try {
        if (!(Clients.processIncomingCommands())) {
          Status.sync = ABORT_SYNC_COMMAND;
          throw "aborting sync, process commands said so";
        }

        // Repeat remoteSetup in-case the commands forced us to reset
        if (!(this._remoteSetup(info)))
          throw "aborting sync, remote setup failed after processing commands";
      }
      finally {
        // Always immediately attempt to push back the local client (now
        // without commands).
        // Note that we don't abort here; if there's a 401 because we've
        // been reassigned, we'll handle it around another engine.
        this._syncEngine(Clients);
      }
    }

    // Update engines because it might change what we sync.
    this._updateEnabledEngines();

    try {
      for each (let engine in Engines.getEnabled()) {
        // If there's any problems with syncing the engine, report the failure
        if (!(this._syncEngine(engine)) || Status.enforceBackoff) {
          this._log.info("Aborting sync");
          break;
        }
      }

      // If _syncEngine fails for a 401, we might not have a cluster URL here.
      // If that's the case, break out of this immediately, rather than
      // throwing an exception when trying to fetch metaURL.
      if (!this.clusterURL) {
        this._log.debug("Aborting sync, no cluster URL: " +
                        "not uploading new meta/global.");
        return;
      }

      // Upload meta/global if any engines changed anything
      let meta = Records.get(this.metaURL);
      if (meta.isNew || meta.changed) {
        new Resource(this.metaURL).put(meta);
        delete meta.isNew;
        delete meta.changed;
      }

      // If there were no sync engine failures
      if (Status.service != SYNC_FAILED_PARTIAL) {
        Svc.Prefs.set("lastSync", new Date().toString());
        Status.sync = SYNC_SUCCEEDED;
      }
    } finally {
      Svc.Prefs.reset("firstSync");

      let syncTime = ((Date.now() - syncStartTime) / 1000).toFixed(2);
      let dateStr = new Date().toLocaleFormat(LOG_DATE_FORMAT);
      this._log.info("Sync completed at " + dateStr
                     + " after " + syncTime + " secs.");
    }
  }))(),


  _updateEnabledEngines: function _updateEnabledEngines() {
    this._log.info("Updating enabled engines: " + SyncScheduler.numClients + " clients.");
    let meta = Records.get(this.metaURL);
    if (meta.isNew || !meta.payload.engines)
      return;

    // If we're the only client, and no engines are marked as enabled,
    // thumb our noses at the server data: it can't be right.
    // Belt-and-suspenders approach to Bug 615926.
    if ((SyncScheduler.numClients <= 1) &&
        ([e for (e in meta.payload.engines) if (e != "clients")].length == 0)) {
      this._log.info("One client and no enabled engines: not touching local engine status.");
      return;
    }

    this._ignorePrefObserver = true;

    let enabled = [eng.name for each (eng in Engines.getEnabled())];
    for (let engineName in meta.payload.engines) {
      if (engineName == "clients") {
        // Clients is special.
        continue;
      }
      let index = enabled.indexOf(engineName);
      if (index != -1) {
        // The engine is enabled locally. Nothing to do.
        enabled.splice(index, 1);
        continue;
      }
      let engine = Engines.get(engineName);
      if (!engine) {
        // The engine doesn't exist locally. Nothing to do.
        continue;
      }

      if (Svc.Prefs.get("engineStatusChanged." + engine.prefName, false)) {
        // The engine was disabled locally. Wipe server data and
        // disable it everywhere.
        this._log.trace("Wiping data for " + engineName + " engine.");
        engine.wipeServer();
        delete meta.payload.engines[engineName];
        meta.changed = true;
      } else {
        // The engine was enabled remotely. Enable it locally.
        this._log.trace(engineName + " engine was enabled remotely.");
        engine.enabled = true;
      }
    }

    // Any remaining engines were either enabled locally or disabled remotely.
    for each (let engineName in enabled) {
      let engine = Engines.get(engineName);
      if (Svc.Prefs.get("engineStatusChanged." + engine.prefName, false)) {
        this._log.trace("The " + engineName + " engine was enabled locally.");
      } else {
        this._log.trace("The " + engineName + " engine was disabled remotely.");
        engine.enabled = false;
      }
    }

    Svc.Prefs.resetBranch("engineStatusChanged.");
    this._ignorePrefObserver = false;
  },

  // Returns true if sync should proceed.
  // false / no return value means sync should be aborted.
  _syncEngine: function WeaveSvc__syncEngine(engine) {
    try {
      engine.sync();
      return true;
    }
    catch(e) {
      // Maybe a 401, cluster update perhaps needed?
      if (e.status == 401) {
        // Log out and clear the cluster URL pref. That will make us perform
        // cluster detection and password check on next sync, which handles
        // both causes of 401s; in either case, we won't proceed with this
        // sync so return false, but kick off a sync for next time.
        this.logout();
        Svc.Prefs.reset("clusterURL");
        Utils.nextTick(this.sync, this);
        return false;
      }
      return true;
    }
  },

  /**
   * Silently fixes case issues.
   */
  syncKeyNeedsUpgrade: function syncKeyNeedsUpgrade() {
    let p = this.passphrase;

    // Check whether it's already a key that we generated.
    if (Utils.isPassphrase(p)) {
      this._log.info("Sync key is up-to-date: no need to upgrade.");
      return false;
    }

    return true;
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
    let p = this.passphrase;

    // Check whether it's already a key that we generated.
    if (!this.syncKeyNeedsUpgrade(p))
      return true;

    // Otherwise, let's upgrade it.
    // N.B., we persist the sync key without testing it first...

    let s = btoa(syncID);        // It's what WeaveCrypto expects. *sigh*
    let k = Utils.derivePresentableKeyFromPassphrase(p, s, PBKDF2_KEY_BYTES);   // Base 32.

    if (!k) {
      this._log.error("No key resulted from derivePresentableKeyFromPassphrase. Failing upgrade.");
      return false;
    }

    this._log.info("Upgrading sync key...");
    this.passphrase = k;
    this._log.info("Saving upgraded sync key...");
    this.persistLogin();
    this._log.info("Done saving.");
    return true;
  },

  _freshStart: function WeaveSvc__freshStart() {
    this._log.info("Fresh start. Resetting client and considering key upgrade.");
    this.resetClient();
    CollectionKeys.clear();
    this.upgradeSyncKey(this.syncID);

    let meta = new WBORecord("meta", "global");
    meta.payload.syncID = this.syncID;
    meta.payload.storageVersion = STORAGE_VERSION;
    meta.isNew = true;

    this._log.debug("New metadata record: " + JSON.stringify(meta.payload));
    let resp = new Resource(this.metaURL).put(meta);
    if (!resp.success)
      throw resp;
    Records.set(this.metaURL, meta);

    // Wipe everything we know about except meta because we just uploaded it
    let collections = [Clients].concat(Engines.getAll()).map(function(engine) {
      return engine.name;
    });
    this.wipeServer(collections);

    // Generate, upload, and download new keys. Do this last so we don't wipe
    // them...
    this.generateNewSymmetricKeys();
  },

  /**
   * Wipe user data from the server.
   *
   * @param collections [optional]
   *        Array of collections to wipe. If not given, all collections are wiped.
   *
   * @param includeKeys [optional]
   *        If true, keys/pubkey and keys/privkey are deleted from the server.
   *        This is false by default, which will cause the usual upgrade paths
   *        to leave those keys on the server. This is to solve Bug 614737: old
   *        clients check for keys *before* checking storage versions.
   *
   *        Note that this parameter only has an effect if `collections` is not
   *        passed. If you explicitly pass a list of collections, they will be
   *        processed regardless of the value of `includeKeys`.
   */
  wipeServer: function wipeServer(collections, includeKeyPairs)
    this._notify("wipe-server", "", function() {
      if (!collections) {
        collections = [];
        let info = new Resource(this.infoURL).get();
        for (let name in info.obj) {
          if (includeKeyPairs || (name != "keys"))
            collections.push(name);
        }
      }
      for each (let name in collections) {
        let url = this.storageURL + name;
        let response = new Resource(url).delete();
        if (response.status != 200 && response.status != 404) {
          throw "Aborting wipeServer. Server responded with "
                + response.status + " response for " + url;
        }
      }
    })(),

  /**
   * Wipe all local user data.
   *
   * @param engines [optional]
   *        Array of engine names to wipe. If not given, all engines are used.
   */
  wipeClient: function WeaveSvc_wipeClient(engines)
    this._catch(this._notify("wipe-client", "", function() {
      // If we don't have any engines, reset the service and wipe all engines
      if (!engines) {
        // Clear out any service data
        this.resetService();

        engines = [Clients].concat(Engines.getAll());
      }
      // Convert the array of names into engines
      else
        engines = Engines.get(engines);

      // Fully wipe each engine if it's able to decrypt data
      for each (let engine in engines)
        if (engine.canDecrypt())
          engine.wipeClient();

      // Save the password/passphrase just in-case they aren't restored by sync
      this.persistLogin();
    }))(),

  /**
   * Wipe all remote user data by wiping the server then telling each remote
   * client to wipe itself.
   *
   * @param engines [optional]
   *        Array of engine names to wipe. If not given, all engines are used.
   */
  wipeRemote: function WeaveSvc_wipeRemote(engines)
    this._catch(this._notify("wipe-remote", "", function() {
      // Make sure stuff gets uploaded.
      this.resetClient(engines);

      // Clear out any server data.
      this.wipeServer(engines);

      // Only wipe the engines provided.
      if (engines) {
        engines.forEach(function(e) Clients.sendCommand("wipeEngine", [e]), this);
      }
      // Tell the remote machines to wipe themselves.
      else {
        Clients.sendCommand("wipeAll", []);
      }

      // Make sure the changed clients get updated.
      Clients.sync();
    }))(),

  /**
   * Reset local service information like logs, sync times, caches.
   */
  resetService: function WeaveSvc_resetService()
    this._catch(this._notify("reset-service", "", function() {
      this._log.info("Service reset.");

      // Pretend we've never synced to the server and drop cached data
      this.syncID = "";
      Svc.Prefs.reset("lastSync");
      Records.clearCache();
    }))(),

  /**
   * Reset the client by getting rid of any local server data and client data.
   *
   * @param engines [optional]
   *        Array of engine names to reset. If not given, all engines are used.
   */
  resetClient: function WeaveSvc_resetClient(engines)
    this._catch(this._notify("reset-client", "", function() {
      // If we don't have any engines, reset everything including the service
      if (!engines) {
        // Clear out any service data
        this.resetService();

        engines = [Clients].concat(Engines.getAll());
      }
      // Convert the array of names into engines
      else
        engines = Engines.get(engines);

      // Have each engine drop any temporary meta data
      for each (let engine in engines)
        engine.resetClient();
    }))(),

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
    return new SyncStorageRequest(url).get(function onComplete(error) {
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
  }

};

// Load Weave on the first time this file is loaded
let Service = new WeaveSvc();
Service.onStartup();
