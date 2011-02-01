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
 *  Richard Newman <rnewman@mozilla.com>
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

// how long we should wait before actually syncing on idle
const IDLE_TIME = 5; // xxxmpc: in seconds, should be preffable

// How long before refreshing the cluster
const CLUSTER_BACKOFF = 5 * 60 * 1000; // 5 minutes

// How long a key to generate from an old passphrase.
const PBKDF2_KEY_BYTES = 16;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/ext/Sync.js");
Cu.import("resource://services-sync/ext/Preferences.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/status.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/main.js");

Utils.lazy(this, 'Service', WeaveSvc);

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
    return misc + "1.0/";
  },

  get userAPI() {
    // Append to the serverURL if it's a relative fragment
    let user = Svc.Prefs.get("userURL");
    if (user.indexOf(":") == -1)
      user = this.serverURL + user;
    return user + "1.0/";
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

  // nextSync and nextHeartbeat are in milliseconds, but prefs can't hold that much
  get nextSync() Svc.Prefs.get("nextSync", 0) * 1000,
  set nextSync(value) Svc.Prefs.set("nextSync", Math.floor(value / 1000)),
  get nextHeartbeat() Svc.Prefs.get("nextHeartbeat", 0) * 1000,
  set nextHeartbeat(value) Svc.Prefs.set("nextHeartbeat", Math.floor(value / 1000)),

  get syncInterval() {
    // If we have a partial download, sync sooner if we're not mobile
    if (Status.partial && Clients.clientType != "mobile")
      return PARTIAL_DATA_SYNC;
    return Svc.Prefs.get("syncInterval", MULTI_MOBILE_SYNC);
  },
  set syncInterval(value) Svc.Prefs.set("syncInterval", value),

  get syncThreshold() Svc.Prefs.get("syncThreshold", SINGLE_USER_THRESHOLD),
  set syncThreshold(value) Svc.Prefs.set("syncThreshold", value),

  get globalScore() Svc.Prefs.get("globalScore", 0),
  set globalScore(value) Svc.Prefs.set("globalScore", value),

  get numClients() Svc.Prefs.get("numClients", 0),
  set numClients(value) Svc.Prefs.set("numClients", value),

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

    let storageAPI = this.clusterURL + Svc.Prefs.get("storageAPI") + "/";
    this.userBaseURL = storageAPI + this.username + "/";
    this._log.debug("Caching URLs under storage user base: " + this.userBaseURL);

    // Generate and cache various URLs under the storage API for this user
    this.infoURL = this.userBaseURL + "info/collections";
    this.storageURL = this.userBaseURL + "storage/";
    this.metaURL = this.storageURL + "meta/global";
    this.cryptoKeysURL = this.storageURL + "crypto/keys";
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
    let cryptoKeys = new CryptoWrapper("crypto", "keys");
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
    this._initLogs();
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
    Svc.Obs.add("network:offline-status-changed", this);
    Svc.Obs.add("weave:service:sync:finish", this);
    Svc.Obs.add("weave:service:sync:error", this);
    Svc.Obs.add("weave:service:backoff:interval", this);
    Svc.Obs.add("weave:engine:score:updated", this);
    Svc.Obs.add("weave:engine:sync:apply-failed", this);
    Svc.Obs.add("weave:resource:status:401", this);
    Svc.Prefs.observe("engine.", this);

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

    // Applications can specify this preference if they want autoconnect
    // to happen after a fixed delay.
    let delay = Svc.Prefs.get("autoconnectDelay");
    if (delay) {
      this.delayedAutoConnect(delay);
    }

    // Send an event now that Weave service is ready.  We don't do this
    // synchronously so that observers will definitely have access to the
    // 'Weave' namespace.
    Utils.delay(function() Svc.Obs.notify("weave:service:ready"), 0);
  },

  _checkSetup: function WeaveSvc__checkSetup() {
    if (!this.enabled)
      return Status.service = STATUS_DISABLED;
    return Status.checkSetup();
  },

  _migratePrefs: function _migratePrefs() {
    // No need to re-migrate
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

  _initLogs: function WeaveSvc__initLogs() {
    this._log = Log4Moz.repository.getLogger("Service.Main");
    this._log.level =
      Log4Moz.Level[Svc.Prefs.get("log.logger.service.main")];

    let formatter = new Log4Moz.BasicFormatter();
    let root = Log4Moz.repository.rootLogger;
    root.level = Log4Moz.Level[Svc.Prefs.get("log.rootLogger")];

    let capp = new Log4Moz.ConsoleAppender(formatter);
    capp.level = Log4Moz.Level[Svc.Prefs.get("log.appender.console")];
    root.addAppender(capp);

    let dapp = new Log4Moz.DumpAppender(formatter);
    dapp.level = Log4Moz.Level[Svc.Prefs.get("log.appender.dump")];
    root.addAppender(dapp);

    let enabled = Svc.Prefs.get("log.appender.debugLog.enabled", false);
    if (enabled) {
      let verbose = Svc.Directory.get("ProfD", Ci.nsIFile);
      verbose.QueryInterface(Ci.nsILocalFile);
      verbose.append("weave");
      verbose.append("logs");
      verbose.append("verbose-log.txt");
      if (!verbose.exists())
        verbose.create(verbose.NORMAL_FILE_TYPE, PERMS_FILE);

      if (Svc.Prefs.get("log.appender.debugLog.rotate", true)) {
        let maxSize = Svc.Prefs.get("log.appender.debugLog.maxSize");
        this._debugApp = new Log4Moz.RotatingFileAppender(verbose, formatter,
                                                          maxSize);
      } else {
        this._debugApp = new Log4Moz.FileAppender(verbose, formatter);
      }
      this._debugApp.level = Log4Moz.Level[Svc.Prefs.get("log.appender.debugLog")];
      root.addAppender(this._debugApp);
    }
  },

  clearLogs: function WeaveSvc_clearLogs() {
    if (this._debugApp)
      this._debugApp.clear();
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
      case "network:offline-status-changed":
        // Whether online or offline, we'll reschedule syncs
        this._log.trace("Network offline status change: " + data);
        this._checkSyncStatus();
        break;
      case "weave:service:sync:error":
        this._handleSyncError();
        if (Status.sync == CREDENTIALS_CHANGED) {
          this.logout();
        }
        break;
      case "weave:service:sync:finish":
        this._scheduleNextSync();
        this._syncErrors = 0;
        break;
      case "weave:service:backoff:interval":
        let interval = (data + Math.random() * data * 0.25) * 1000; // required backoff + up to 25%
        Status.backoffInterval = interval;
        Status.minimumNextSync = Date.now() + data;
        break;
      case "weave:engine:score:updated":
        this._handleScoreUpdate();
        break;
      case "weave:engine:sync:apply-failed":
        // An engine isn't able to apply one or more incoming records.
        // We don't fail hard on this, but it usually indicates a bug,
        // so for now treat it as sync error (c.f. Service._syncEngine())
        Status.engines = [data, ENGINE_APPLY_FAIL];
        this._syncError = true;
        this._log.debug(data + " failed to apply some records.");
        break;
      case "weave:resource:status:401":
        this._handleResource401(subject);
        break;
      case "idle":
        this._log.trace("Idle time hit, trying to sync");
        Svc.Idle.removeIdleObserver(this, this._idleTime);
        this._idleTime = 0;
        Utils.delay(function() this.sync(false), 0, this);
        break;
      case "nsPref:changed":
        if (this._ignorePrefObserver)
          return;
        let engine = data.slice((PREFS_BRANCH + "engine.").length);
        this._handleEngineStatusChanged(engine);
        break;
    }
  },

  _handleScoreUpdate: function WeaveSvc__handleScoreUpdate() {
    const SCORE_UPDATE_DELAY = 3000;
    Utils.delay(this._calculateScore, SCORE_UPDATE_DELAY, this, "_scoreTimer");
  },

  _calculateScore: function WeaveSvc_calculateScoreAndDoStuff() {
    var engines = Engines.getEnabled();
    for (let i = 0;i < engines.length;i++) {
      this._log.trace(engines[i].name + ": score: " + engines[i].score);
      this.globalScore += engines[i].score;
      engines[i]._tracker.resetScore();
    }

    this._log.trace("Global score updated: " + this.globalScore);
    this._checkSyncStatus();
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

  _handleResource401: function _handleResource401(request) {
    // Only handle 401s that are hitting the current cluster
    let spec = request.resource.spec;
    let cluster = this.clusterURL;
    if (spec.indexOf(cluster) != 0)
      return;

    // Nothing to do if the cluster isn't changing
    if (!this._setCluster())
      return;

    // Replace the old cluster with the new one to retry the request
    request.newUri = this.clusterURL + spec.slice(cluster.length);
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
          fail = "Find cluster denied: " + this._errorStr(node);
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
          fail = "Unexpected response code: " + node.status;
          break;
      }
    } catch (e) {
      this._log.debug("Network error on findCluster");
      Status.login = LOGIN_FAILED_NETWORK_ERROR;
      fail = e;
    }
    throw fail;
  },

  // gets cluster from central LDAP server and sets this.clusterURL
  _setCluster: function _setCluster() {
    // Make sure we didn't get some unexpected response for the cluster
    let cluster = this._findCluster();
    this._log.debug("cluster value = " + cluster);
    if (cluster == null)
      return false;

    // Don't update stuff if we already have the right cluster
    if (cluster == this.clusterURL)
      return false;

    this.clusterURL = cluster;
    return true;
  },

  // update cluster if required. returns false if the update was not required
  _updateCluster: function _updateCluster() {
    let cTime = Date.now();
    let lastUp = parseFloat(Svc.Prefs.get("lastClusterUpdate"));
    if (!lastUp || ((cTime - lastUp) >= CLUSTER_BACKOFF)) {
      if (this._setCluster()) {
        Svc.Prefs.set("lastClusterUpdate", cTime.toString());
        return true;
      }
    }
    return false;
  },

  /**
   * Perform the info fetch as part of a login or key fetch.
   */
  _fetchInfo: function _fetchInfo(url, logout) {
    let infoURL = url || this.infoURL;
    
    this._log.trace("In _fetchInfo: " + infoURL);
    let info = new Resource(infoURL).get();
    if (!info.success) {
      if (info.status == 401) {
        if (logout) {
          this.logout();
          Status.login = LOGIN_FAILED_LOGIN_REJECTED;
        }
      }
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
        return false;
      }
      
      let infoCollections = infoResponse.obj;
      
      this._log.info("Testing info/collections: " + JSON.stringify(infoCollections));
      
      if (CollectionKeys.updateNeeded(infoCollections)) {
        this._log.info("CollectionKeys reports that a key update is needed.");
        
        // Don't always set to CREDENTIALS_CHANGED -- we will probably take care of this.
            
        // Fetch storage/crypto/keys.
        let cryptoKeys;
        
        if (infoCollections && ('crypto' in infoCollections)) {
          try {
            cryptoKeys = new CryptoWrapper("crypto", "keys");
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
              this._log.warn("Got status " + cryptoResp.status + " fetching crypto keys.");
              Status.login = LOGIN_FAILED_SERVER_ERROR;
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
          // Must have got a 404, or no reported collection.
          // Better make some and upload them.
          // 
          // Reset the client so we reupload.
          this.resetClient();
          
          // Generate the new keys.
          this.generateNewSymmetricKeys();
          
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
      // Make sure we have a cluster to verify against
      // this is a little weird, if we don't get a node we pretend
      // to succeed, since that probably means we just don't have storage
      if (this.clusterURL == "" && !this._setCluster()) {
        Status.sync = NO_SYNC_NODE_FOUND;
        Svc.Obs.notify("weave:service:sync:delayed");
        return true;
      }

      if (!this.username) {
        this._log.warn("No username in verifyLogin.");
        Status.login = LOGIN_FAILED_NO_USERNAME;
        return false;
      }

      // Unlock master password, or return.
      try {
        // Fetch collection info on every startup.
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
            this._checkServerError(test);
            Status.login = LOGIN_FAILED_SERVER_ERROR;
            return false;
        }
      }
      catch (ex) {
        // Must have failed on some network issue
        this._log.debug("verifyLogin failed: " + Utils.exceptionStr(ex));
        Status.login = LOGIN_FAILED_NETWORK_ERROR;
        return false;
      }
    })(),

  generateNewSymmetricKeys:
  function WeaveSvc_generateNewSymmetricKeys() {
    this._log.info("Generating new keys....");
    CollectionKeys.generateNewKeys();
    let wbo = CollectionKeys.asWBO("crypto", "keys");
    this._log.info("Encrypting new key bundle. Modified time is " + wbo.modified);
    wbo.encrypt(this.syncKeyBundle);
    
    this._log.info("Uploading...");
    let uploadRes = wbo.upload(this.cryptoKeysURL);
    if (uploadRes.status >= 400) {
      this._log.warn("Got status " + uploadRes.status + " uploading new keys. What to do? Throw!");
      throw new Error("Unable to upload symmetric keys.");
    }
    this._log.info("Got status " + uploadRes.status);
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
      this.sync(true);
      return true;
    }))(),

  startOver: function() {
    // Set a username error so the status message shows "set up..."
    Status.login = LOGIN_FAILED_NO_USERNAME;
    this.logout();
    
    // Reset all engines and clear keys.
    this.resetClient();
    CollectionKeys.clear();
    
    // Reset Weave prefs.
    this._ignorePrefObserver = true;
    Svc.Prefs.resetBranch("");
    this._ignorePrefObserver = false;
    
    Svc.Prefs.set("lastversion", WEAVE_VERSION);
    // Find weave logins and remove them.
    this.password = "";
    this.passphrase = "";
    Svc.Login.findLogins({}, PWDMGR_HOST, "", "").map(function(login) {
      Svc.Login.removeLogin(login);
    });
    Svc.Obs.notify("weave:service:start-over");
    Svc.Obs.notify("weave:engine:stop-tracking");
  },

  delayedAutoConnect: function delayedAutoConnect(delay) {
    if (this._loggedIn)
      return;

    if (this._checkSetup() == STATUS_OK && Svc.Prefs.get("autoconnect")) {
      Utils.delay(this._autoConnect, delay * 1000, this, "_autoTimer");
    }
  },

  _autoConnect: let (attempts = 0) function _autoConnect() {
    let isLocked = Utils.mpLocked();
    if (isLocked) {
      // There's no reason to back off if we're locked: we'll just try to login
      // during sync. Clear our timer, see if we should go ahead and sync, then
      // just return.
      this._log.trace("Autoconnect skipped: master password still locked.");
      
      if (this._autoTimer)
        this._autoTimer.clear();
      
      this._checkSyncStatus();
      Svc.Prefs.set("autoconnect", true);
      
      return;
    }
    
    let reason = this._checkSync([kSyncNotLoggedIn, kFirstSyncChoiceNotMade]);

    // Can't autoconnect if we're missing these values.
    if (!reason) {
      if (!this.username || !this.password || !this.passphrase)
        return;

      // Nothing more to do on a successful login.
      if (this.login())
        return;
    }

    // Something failed, so try again some time later.
    let interval = this._calculateBackoff(++attempts, 60 * 1000);
    this._log.debug("Autoconnect failed: " + (reason || Status.login) +
      "; retry in " + Math.ceil(interval / 1000) + " sec.");
    Utils.delay(function() this._autoConnect(), interval, this, "_autoTimer");
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
      if (Svc.IO.offline)
        throw "Application is offline, login should not be called";

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
        if (Status.login == MASTER_PASSWORD_LOCKED) {
          // Drat.
          this._log.debug("Login failed: " + Status.login);
          return false;
        }
        // verifyLogin sets the failure states here.
        throw "Login failed: " + Status.login;
      }

      // No need to try automatically connecting after a successful login.
      if (this._autoTimer)
        this._autoTimer.clear();

      this._loggedIn = true;
      // Try starting the sync timer now that we're logged in.
      this._checkSyncStatus();
      Svc.Prefs.set("autoconnect", true);

      return true;
    })))(),

  logout: function WeaveSvc_logout() {
    // No need to do anything if we're already logged out.
    if (!this._loggedIn)
      return;

    this._log.info("Logging out");
    this._loggedIn = false;

    // Cancel the sync timer now that we're logged out.
    this._checkSyncStatus();
    Svc.Prefs.set("autoconnect", false);

    Svc.Obs.notify("weave:service:logout:finish");
  },

  _errorStr: function WeaveSvc__errorStr(code) {
    switch (code.toString()) {
    case "1":
      return "illegal-method";
    case "2":
      return "invalid-captcha";
    case "3":
      return "invalid-username";
    case "4":
      return "cannot-overwrite-resource";
    case "5":
      return "userid-mismatch";
    case "6":
      return "json-parse-failure";
    case "7":
      return "invalid-password";
    case "8":
      return "invalid-record";
    case "9":
      return "weak-password";
    default:
      return "generic-server-error";
    }
  },

  checkAccount: function checkAccount(account) {
    let username = this._usernameFromAccount(account);
    return this.checkUsername(username);
  },

  // Backwards compat with the Firefox UI. Fold into checkAccount() once
  // bug 595066 has landed.
  checkUsername: function checkUsername(username) {
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
    return this._errorStr(data);
  },

  createAccount: function createAccount() {
    // Backwards compat with the Firefox UI. Change to signature to
    // (email, password, captchaChallenge, captchaResponse) once
    // bug 595066 has landed.
    let username, email, password, captchaChallenge, captchaResponse;
    if (arguments.length == 4) {
      [email, password, captchaChallenge, captchaResponse] = arguments;
      username = this._usernameFromAccount(email);
    } else {
      [username, password, email, captchaChallenge, captchaResponse] = arguments;
    }

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
        error = this._errorStr(register);
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
        this._checkServerError(Records.response);
        Status.sync = METARECORD_DOWNLOAD_FAIL;
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
           !Svc.IO.offline &&
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
    else if (Svc.IO.offline)
      reason = kSyncNetworkOffline;
    else if (Status.minimumNextSync > Date.now())
      reason = kSyncBackoffNotMet;
    else if ((Status.login == MASTER_PASSWORD_LOCKED) &&
             Utils.mpLocked())
      reason = kSyncMasterPasswordLocked;
    else if (!this._loggedIn)
      reason = kSyncNotLoggedIn;
    else if (Svc.Prefs.get("firstSync") == "notReady")
      reason = kFirstSyncChoiceNotMade;

    if (ignore && ignore.indexOf(reason) != -1)
      return "";

    return reason;
  },

  /**
   * Remove any timers/observers that might trigger a sync
   */
  _clearSyncTriggers: function _clearSyncTriggers() {
    this._log.debug("Clearing sync triggers.");
    
    // Clear out any scheduled syncs
    if (this._syncTimer)
      this._syncTimer.clear();
    if (this._heartbeatTimer)
      this._heartbeatTimer.clear();

    // Clear out a sync that's just waiting for idle if we happen to have one
    try {
      Svc.Idle.removeIdleObserver(this, this._idleTime);
      this._idleTime = 0;
    }
    catch(ex) {}
  },

  /**
   * Check if we should be syncing and schedule the next sync, if it's not scheduled
   */
  _checkSyncStatus: function WeaveSvc__checkSyncStatus() {
    // Should we be syncing now, if not, cancel any sync timers and return
    // if we're in backoff, we'll schedule the next sync
    let ignore = [kSyncBackoffNotMet];
    
    // We're ready to sync even if we're not logged in... so long as the 
    // master password isn't locked.
    if (Utils.mpLocked()) {
      ignore.push(kSyncNotLoggedIn);
      ignore.push(kSyncMasterPasswordLocked);
    }
    
    let skip = this._checkSync(ignore);
    this._log.trace("_checkSync returned \"" + skip + "\".");
    if (skip) {
      this._clearSyncTriggers();
      return;
    }

    // Only set the wait time to 0 if we need to sync right away
    let wait;
    if (this.globalScore > this.syncThreshold) {
      this._log.debug("Global Score threshold hit, triggering sync.");
      wait = 0;
    }
    this._scheduleNextSync(wait);
  },

  /**
   * Call sync() on an idle timer
   *
   * delay is optional
   */
  syncOnIdle: function WeaveSvc_syncOnIdle(delay) {
    // No need to add a duplicate idle observer, and no point if we got kicked
    // out by the master password dialog.
    if (Status.login == MASTER_PASSWORD_LOCKED &&
        Utils.mpLocked()) {
      this._log.debug("Not syncing on idle: Login status is " + Status.login);
      
      // If we're not syncing now, we need to schedule the next one.
      this._scheduleAtInterval(MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
      return false;
    }

    if (this._idleTime)
      return false;

    this._idleTime = delay || IDLE_TIME;
    this._log.debug("Idle timer created for sync, will sync after " +
                    this._idleTime + " seconds of inactivity.");
    Svc.Idle.addIdleObserver(this, this._idleTime);
  },

  /**
   * Set a timer for the next sync
   */
  _scheduleNextSync: function WeaveSvc__scheduleNextSync(interval) {
    // Figure out when to sync next if not given a interval to wait
    if (interval == null) {
      // Check if we had a pending sync from last time
      if (this.nextSync != 0)
        interval = this.nextSync - Date.now();
      // Use the bigger of default sync interval and backoff
      else
        interval = Math.max(this.syncInterval, Status.backoffInterval);
    }

    // Start the sync right away if we're already late
    if (interval <= 0) {
      if (this.syncOnIdle())
        this._log.debug("Syncing as soon as we're idle.");
      return;
    }

    this._log.trace("Next sync in " + Math.ceil(interval / 1000) + " sec.");
    Utils.delay(function() this.syncOnIdle(), interval, this, "_syncTimer");

    // Save the next sync time in-case sync is disabled (logout/offline/etc.)
    this.nextSync = Date.now() + interval;

    // if we're a single client, set up a heartbeat to detect new clients sooner
    if (this.numClients == 1)
      this._scheduleHeartbeat();
  },

  /**
   * Hits info/collections on the server to see if there are new clients.
   * This is only called when the account has one active client, and if more
   * are found will trigger a sync to change client sync frequency and update data.
   */

  _doHeartbeat: function WeaveSvc__doHeartbeat() {
    if (this._heartbeatTimer)
      this._heartbeatTimer.clear();

    this.nextHeartbeat = 0;
    let info = null;
    try {
      info = new Resource(this.infoURL).get();
      if (info && info.success) {
        // if clients.lastModified doesn't match what the server has,
        // we have another client in play
        this._log.trace("Remote timestamp:" + info.obj["clients"] +
                        " Local timestamp: " + Clients.lastSync);
        if (info.obj["clients"] > Clients.lastSync) {
          this._log.debug("New clients detected, triggering a full sync");
          this.syncOnIdle();
          return;
        }
      }
      else {
        this._checkServerError(info);
        this._log.debug("Heartbeat failed. HTTP Error: " + info.status);
      }
    } catch(ex) {
      // if something throws unexpectedly, not a big deal
      this._log.debug("Heartbeat failed unexpectedly: " + ex);
    }

    // no joy, schedule the next heartbeat
    this._scheduleHeartbeat();
  },

  /**
   * Sets up a heartbeat ping to check for new clients.  This is not a critical
   * behaviour for the client, so if we hit server/network issues, we'll just drop
   * this until the next sync.
   */
  _scheduleHeartbeat: function WeaveSvc__scheduleNextHeartbeat() {
    if (this._heartbeatTimer)
      return;

    let now = Date.now();
    if (this.nextHeartbeat && this.nextHeartbeat < now) {
      this._doHeartbeat();
      return;
    }

    // if the next sync is in less than an hour, don't bother
    let interval = MULTI_DESKTOP_SYNC;
    if (this.nextSync < Date.now() + interval ||
        Status.enforceBackoff)
      return;

    if (this.nextHeartbeat)
      interval = this.nextHeartbeat - now;
    else
      this.nextHeartbeat = now + interval;

    this._log.trace("Setting up heartbeat, next ping in " +
                    Math.ceil(interval / 1000) + " sec.");
    Utils.delay(function() this._doHeartbeat(), interval, this, "_heartbeatTimer");
  },

  /**
   * Incorporates the backoff/retry logic used in error handling and elective
   * non-syncing.
   */
  _scheduleAtInterval: function _scheduleAtInterval(minimumInterval) {
    const MINIMUM_BACKOFF_INTERVAL = 15 * 60 * 1000;     // 15 minutes
    let interval = this._calculateBackoff(this._syncErrors, MINIMUM_BACKOFF_INTERVAL);
    if (minimumInterval)
      interval = Math.max(minimumInterval, interval);

    let d = new Date(Date.now() + interval);
    this._log.config("Starting backoff, next sync at:" + d.toString());

    this._scheduleNextSync(interval);
  },
  
  _syncErrors: 0,
  /**
   * Deal with sync errors appropriately
   */
  _handleSyncError: function WeaveSvc__handleSyncError() {
    this._syncErrors++;

    // do nothing on the first couple of failures, if we're not in backoff due to 5xx errors
    if (!Status.enforceBackoff) {
      if (this._syncErrors < 3) {
        this._scheduleNextSync();
        return;
      }
      Status.enforceBackoff = true;
    }
    
    this._scheduleAtInterval();
  },

  _skipScheduledRetry: function _skipScheduledRetry() {
    return [LOGIN_FAILED_INVALID_PASSPHRASE,
            LOGIN_FAILED_LOGIN_REJECTED].indexOf(Status.login) == -1;
  },
  
  sync: function sync() {
    this._log.debug("In wrapping sync().");
    this._catch(function () {
      // Make sure we're logged in.
      if (this._shouldLogin()) {
        this._log.debug("In sync: should login.");
        if (!this.login()) {
          this._log.debug("Not syncing: login returned false.");
          this._clearSyncTriggers();    // No more pending syncs, please.
          
          // Try again later, just as if we threw an error... only without the
          // error count.
          if (!this._skipScheduledRetry())
            this._scheduleAtInterval(MASTER_PASSWORD_LOCKED_RETRY_INTERVAL);
    
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
      // this is a purposeful abort rather than a failure, so don't set
      // any status bits
      reason = "Can't sync: " + reason;
      throw reason;
    }

    // if we don't have a node, get one.  if that fails, retry in 10 minutes
    if (this.clusterURL == "" && !this._setCluster()) {
      Status.sync = NO_SYNC_NODE_FOUND;
      this._scheduleNextSync(10 * 60 * 1000);
      return;
    }

    // Clear out any potentially pending syncs now that we're syncing
    this._clearSyncTriggers();
    this.nextSync = 0;
    this.nextHeartbeat = 0;

    // reset backoff info, if the server tells us to continue backing off,
    // we'll handle that later
    Status.resetBackoff();

    // Ping the server with a special info request once a day.
    let infoURL = this.infoURL;
    let now = Math.floor(Date.now() / 1000);
    let lastPing = Svc.Prefs.get("lastPing", 0);
    if (now - lastPing > 86400) { // 60 * 60 * 24
      infoURL += "?v=" + WEAVE_VERSION;
      Svc.Prefs.set("lastPing", now);
    }

    // Figure out what the last modified time is for each collection
    let info = this._fetchInfo(infoURL, true);
    this.globalScore = 0;

    // Convert the response to an object and read out the modified times
    for each (let engine in [Clients].concat(Engines.getAll()))
      engine.lastModified = info.obj[engine.name] || 0;

    if (!(this._remoteSetup(info)))
      throw "aborting sync, remote setup failed";

    // Make sure we have an up-to-date list of clients before sending commands
    this._log.trace("Refreshing client list");
    this._syncEngine(Clients);

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

    // Process the incoming commands if we have any
    if (Clients.localCommands) {
      try {
        if (!(this.processCommands())) {
          Status.sync = ABORT_SYNC_COMMAND;
          throw "aborting sync, process commands said so";
        }

        // Repeat remoteSetup in-case the commands forced us to reset
        if (!(this._remoteSetup(info)))
          throw "aborting sync, remote setup failed after processing commands";
      }
      finally {
        // Always immediately push back the local client (now without commands)
        this._syncEngine(Clients);
      }
    }

    // Update the client mode and engines because it might change what we sync.
    this._updateClientMode();
    this._updateEnabledEngines();

    try {
      for each (let engine in Engines.getEnabled()) {
        // If there's any problems with syncing the engine, report the failure
        if (!(this._syncEngine(engine)) || Status.enforceBackoff) {
          this._log.info("Aborting sync");
          break;
        }
      }

      // Upload meta/global if any engines changed anything
      let meta = Records.get(this.metaURL);
      if (meta.isNew || meta.changed) {
        new Resource(this.metaURL).put(meta);
        delete meta.isNew;
        delete meta.changed;
      }

      if (this._syncError)
        throw "Some engines did not sync correctly";
      else {
        Svc.Prefs.set("lastSync", new Date().toString());
        Status.sync = SYNC_SUCCEEDED;
        let syncTime = ((Date.now() - syncStartTime) / 1000).toFixed(2);
        this._log.info("Sync completed successfully in " + syncTime + " secs.");
      }
    } finally {
      this._syncError = false;
      Svc.Prefs.reset("firstSync");
    }
  }))(),

  /**
   * Process the locally stored clients list to figure out what mode to be in
   */
  _updateClientMode: function _updateClientMode() {
    // Nothing to do if it's the same amount
    let {numClients, hasMobile} = Clients.stats;
    if (this.numClients == numClients)
      return;

    this._log.debug("Client count: " + this.numClients + " -> " + numClients);
    this.numClients = numClients;

    if (numClients == 1) {
      this.syncInterval = SINGLE_USER_SYNC;
      this.syncThreshold = SINGLE_USER_THRESHOLD;
    }
    else {
      this.syncInterval = hasMobile ? MULTI_MOBILE_SYNC : MULTI_DESKTOP_SYNC;
      this.syncThreshold = hasMobile ? MULTI_MOBILE_THRESHOLD : MULTI_DESKTOP_THRESHOLD;
    }
  },

  _updateEnabledEngines: function _updateEnabledEngines() {
    this._log.info("Updating enabled engines: " + this.numClients + " clients.");
    let meta = Records.get(this.metaURL);
    if (meta.isNew || !meta.payload.engines)
      return;
    
    // If we're the only client, and no engines are marked as enabled,
    // thumb our noses at the server data: it can't be right.
    // Belt-and-suspenders approach to Bug 615926.
    if ((this.numClients <= 1) &&
        ([e for (e in meta.payload.engines) if (e != "clients")].length == 0)) {
      this._log.info("One client and no enabled engines: not touching local engine status.");
      return;
    }

    this._ignorePrefObserver = true;

    let enabled = [eng.name for each (eng in Engines.getEnabled())];
    for (let engineName in meta.payload.engines) {
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
    for each (engineName in enabled) {
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

  // returns true if sync should proceed
  // false / no return value means sync should be aborted
  _syncEngine: function WeaveSvc__syncEngine(engine) {
    try {
      engine.sync();
      return true;
    }
    catch(e) {
      // maybe a 401, cluster update needed?
      if (e.status == 401 && this._updateCluster())
        return this._syncEngine(engine);

      this._checkServerError(e);

      Status.engines = [engine.name, e.failureCode || ENGINE_UNKNOWN_FAIL];

      this._syncError = true;
      this._log.debug(engine.name + " failed: " + Utils.exceptionStr(e));
      return true;
    }
    finally {
      // If this engine has more to fetch, remember that globally
      if (engine.toFetch != null && engine.toFetch.length > 0)
        Status.partial = true;
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
    
    // Generate and upload new keys. Do this last so we don't wipe them...
    this.generateNewSymmetricKeys();
  },

  /**
   * Check to see if this is a failure
   *
   */
  _checkServerError: function WeaveSvc__checkServerError(resp) {
    if (Utils.checkStatus(resp.status, null, [500, [502, 504]])) {
      Status.enforceBackoff = true;
      if (resp.status == 503 && resp.headers["retry-after"])
        Svc.Obs.notify("weave:service:backoff:interval",
                       parseInt(resp.headers["retry-after"], 10));
    }
    if (resp.status == 400 && resp == RESPONSE_OVER_QUOTA)
      Status.sync = OVER_QUOTA;
  },
  /**
   * Return a value for a backoff interval.  Maximum is eight hours, unless
   * Status.backoffInterval is higher.
   *
   */
  _calculateBackoff: function WeaveSvc__calculateBackoff(attempts, base_interval) {
    const MAXIMUM_BACKOFF_INTERVAL = 8 * 60 * 60 * 1000; // 8 hours
    let backoffInterval = attempts *
                          (Math.floor(Math.random() * base_interval) +
                           base_interval);
    return Math.max(Math.min(backoffInterval, MAXIMUM_BACKOFF_INTERVAL), Status.backoffInterval);
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
      // Make sure stuff gets uploaded
      this.resetClient(engines);

      // Clear out any server data
      this.wipeServer(engines);

      // Only wipe the engines provided
      if (engines)
        engines.forEach(function(e) this.prepCommand("wipeEngine", [e]), this);
      // Tell the remote machines to wipe themselves
      else
        this.prepCommand("wipeAll", []);

      // Make sure the changed clients get updated
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
   * A hash of valid commands that the client knows about. The key is a command
   * and the value is a hash containing information about the command such as
   * number of arguments and description.
   */
  _commands: [
    ["resetAll", 0, "Clear temporary local data for all engines"],
    ["resetEngine", 1, "Clear temporary local data for engine"],
    ["wipeAll", 0, "Delete all client data for all engines"],
    ["wipeEngine", 1, "Delete all client data for engine"],
    ["logout", 0, "Log out client"],
  ].reduce(function WeaveSvc__commands(commands, entry) {
    commands[entry[0]] = {};
    for (let [i, attr] in Iterator(["args", "desc"]))
      commands[entry[0]][attr] = entry[i + 1];
    return commands;
  }, {}),

  /**
   * Check if the local client has any remote commands and perform them.
   *
   * @return False to abort sync
   */
  processCommands: function WeaveSvc_processCommands()
    this._notify("process-commands", "", function() {
      // Immediately clear out the commands as we've got them locally
      let commands = Clients.localCommands;
      Clients.clearCommands();

      // Process each command in order
      for each ({command: command, args: args} in commands) {
        this._log.debug("Processing command: " + command + "(" + args + ")");

        let engines = [args[0]];
        switch (command) {
          case "resetAll":
            engines = null;
            // Fallthrough
          case "resetEngine":
            this.resetClient(engines);
            break;
          case "wipeAll":
            engines = null;
            // Fallthrough
          case "wipeEngine":
            this.wipeClient(engines);
            break;
          case "logout":
            this.logout();
            return false;
          default:
            this._log.debug("Received an unknown command: " + command);
            break;
        }
      }

      return true;
    })(),

  /**
   * Prepare to send a command to each remote client. Calling this doesn't
   * actually sync the command data to the server. If the client already has
   * the command/args pair, it won't get a duplicate action.
   *
   * @param command
   *        Command to invoke on remote clients
   * @param args
   *        Array of arguments to give to the command
   */
  prepCommand: function WeaveSvc_prepCommand(command, args) {
    let commandData = this._commands[command];
    // Don't send commands that we don't know about
    if (commandData == null) {
      this._log.error("Unknown command to send: " + command);
      return;
    }
    // Don't send a command with the wrong number of arguments
    else if (args == null || args.length != commandData.args) {
      this._log.error("Expected " + commandData.args + " args for '" +
                      command + "', but got " + args);
      return;
    }

    // Send the command to all remote clients
    this._log.debug("Sending clients: " + [command, args, commandData.desc]);
    Clients.sendCommand(command, args);
  },

  _getInfo: function _getInfo(what)
    this._catch(this._notify(what, "", function() {
      let url = this.userBaseURL + "info/" + what;
      let response = new Resource(url).get();
      if (response.status != 200)
        return null;
      return response.obj;
    }))(),

  getCollectionUsage: function getCollectionUsage()
    this._getInfo("collection_usage"),

  getQuota: function getQuota() this._getInfo("quota")

};

// Load Weave on the first time this file is loaded
Service.onStartup();
