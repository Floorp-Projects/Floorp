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

const EXPORTED_SYMBOLS = ['Weave'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

// how long we should wait before actually syncing on idle
const IDLE_TIME = 5; // xxxmpc: in seconds, should be preffable

// How long before refreshing the cluster
const CLUSTER_BACKOFF = 5 * 60 * 1000; // 5 minutes

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/ext/Sync.js");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/auth.js");
Cu.import("resource://weave/resource.js");
Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/crypto.js");
Cu.import("resource://weave/base_records/keys.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/identity.js");
Cu.import("resource://weave/status.js");
Cu.import("resource://weave/engines/clientData.js");

// for export
let Weave = {};
Cu.import("resource://weave/constants.js", Weave);
Cu.import("resource://weave/util.js", Weave);
Cu.import("resource://weave/auth.js", Weave);
Cu.import("resource://weave/resource.js", Weave);
Cu.import("resource://weave/base_records/keys.js", Weave);
Cu.import("resource://weave/notifications.js", Weave);
Cu.import("resource://weave/identity.js", Weave);
Cu.import("resource://weave/status.js", Weave);
Cu.import("resource://weave/stores.js", Weave);
Cu.import("resource://weave/engines.js", Weave);

Cu.import("resource://weave/engines/bookmarks.js", Weave);
Cu.import("resource://weave/engines/clientData.js", Weave);
Cu.import("resource://weave/engines/cookies.js", Weave);
Cu.import("resource://weave/engines/extensions.js", Weave);
Cu.import("resource://weave/engines/forms.js", Weave);
Cu.import("resource://weave/engines/history.js", Weave);
Cu.import("resource://weave/engines/input.js", Weave);
Cu.import("resource://weave/engines/prefs.js", Weave);
Cu.import("resource://weave/engines/microformats.js", Weave);
Cu.import("resource://weave/engines/passwords.js", Weave);
Cu.import("resource://weave/engines/plugins.js", Weave);
Cu.import("resource://weave/engines/tabs.js", Weave);
Cu.import("resource://weave/engines/themes.js", Weave);

Utils.lazy(Weave, 'Service', WeaveSvc);

/*
 * Service singleton
 * Main entry point into Weave's sync framework
 */

function WeaveSvc() {
  this._notify = Utils.notify("weave:service:");
}
WeaveSvc.prototype = {

  _lock: Utils.lock,
  _catch: Utils.catch,
  _isQuitting: false,
  _loggedIn: false,
  _syncInProgress: false,
  _keyGenEnabled: true,

  // object for caching public and private keys
  _keyPair: {},

  get username() {
    return Svc.Prefs.get("username", "");
  },
  set username(value) {
    if (value)
      Svc.Prefs.set("username", value);
    else
      Svc.Prefs.reset("username");

    // fixme - need to loop over all Identity objects - needs some rethinking...
    ID.get('WeaveID').username = value;
    ID.get('WeaveCryptoID').username = value;

    // FIXME: need to also call this whenever the username pref changes
    this._updateCachedURLs();
  },

  get password password() ID.get("WeaveID").password,
  set password password(value) ID.get("WeaveID").password = value,

  get passphrase passphrase() ID.get("WeaveCryptoID").password,
  set passphrase passphrase(value) ID.get("WeaveCryptoID").password = value,

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
    return misc + "1/";
  },

  get userAPI() {
    // Append to the serverURL if it's a relative fragment
    let user = Svc.Prefs.get("userURL");
    if (user.indexOf(":") == -1)
      user = this.serverURL + user;
    return user + "1/";
  },

  get isLoggedIn() { return this._loggedIn; },

  get isQuitting() { return this._isQuitting; },
  set isQuitting(value) { this._isQuitting = value; },

  get keyGenEnabled() { return this._keyGenEnabled; },
  set keyGenEnabled(value) { this._keyGenEnabled = value; },

  // nextSync is in milliseconds, but prefs can't hold that much
  get nextSync() Svc.Prefs.get("nextSync", 0) * 1000,
  set nextSync(value) Svc.Prefs.set("nextSync", Math.floor(value / 1000)),

  get syncInterval() {
    // If we have a partial download, sync sooner if we're not mobile
    if (Status.partial && Clients.clientType != "mobile")
      return PARTIAL_DATA_SYNC;
    return Svc.Prefs.get("syncInterval", MULTI_MOBILE_SYNC);
  },
  set syncInterval(value) Svc.Prefs.set("syncInterval", value),

  get syncThreshold() Svc.Prefs.get("syncThreshold", SINGLE_USER_THRESHOLD),
  set syncThreshold(value) Svc.Prefs.set("nextSync", value),

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

  _updateCachedURLs: function _updateCachedURLs() {
    // Nothing to cache yet if we don't have the building blocks
    if (this.clusterURL == "" || this.username == "")
      return;

    let storageAPI = this.clusterURL + "0.5/";
    let userBase = storageAPI + this.username + "/";
    this._log.debug("Caching URLs under storage user base: " + userBase);

    // Generate and cache various URLs under the storage API for this user
    this.infoURL = userBase + "info/collections";
    this.storageURL = userBase + "storage/";
    this.metaURL = this.storageURL + "meta/global";
    PubKeys.defaultKeyUri = this.storageURL + "keys/pubkey";
    PrivKeys.defaultKeyUri = this.storageURL + "keys/privkey";
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

  onWindowOpened: function WeaveSvc__onWindowOpened() {
  },

  /**
   * Prepare to initialize the rest of Weave after waiting a little bit
   */
  onStartup: function onStartup() {
    Status.service = STATUS_DELAYED;

    // Figure out how many seconds to delay loading Weave based on the app
    let wait = 0;
    switch (Svc.AppInfo.ID) {
      case FIREFOX_ID:
        // Add one second delay for each tab in every window
        let enum = Svc.WinMediator.getEnumerator("navigator:browser");
        while (enum.hasMoreElements())
          wait += enum.getNext().gBrowser.mTabs.length;
    }

    // Make sure we wait a little but but not too long in the worst case
    wait = Math.ceil(Math.max(5, Math.min(20, wait)));

    this._initLogs();
    this._log.info("Loading Weave " + WEAVE_VERSION + " in " + wait + " sec.");
    Utils.delay(this._onStartup, wait * 1000, this, "_startupTimer");
  },

  // one-time initialization like setting up observers and the like
  // xxx we might need to split some of this out into something we can call
  //     again when username/server/etc changes
  _onStartup: function _onStartup() {
    Status.service = STATUS_OK;
    this.enabled = true;

    this._registerEngines();

    // Reset our sync id if we're upgrading, so sync knows to reset local data
    if (WEAVE_VERSION != Svc.Prefs.get("lastversion")) {
      this._log.info("Resetting client syncID from _onStartup.");
      Clients.resetSyncID();
    }

    let ua = Cc["@mozilla.org/network/protocol;1?name=http"].
      getService(Ci.nsIHttpProtocolHandler).userAgent;
    this._log.info(ua);

    if (!this._checkCrypto()) {
      this.enabled = false;
      this._log.error("Could not load the Weave crypto component. Disabling " +
                      "Weave, since it will not work correctly.");
    }

    Svc.Observer.addObserver(this, "network:offline-status-changed", true);
    Svc.Observer.addObserver(this, "private-browsing", true);
    Svc.Observer.addObserver(this, "quit-application", true);
    Svc.Observer.addObserver(this, "weave:service:sync:finish", true);
    Svc.Observer.addObserver(this, "weave:service:sync:error", true);
    Svc.Observer.addObserver(this, "weave:service:backoff:interval", true);
    Svc.Observer.addObserver(this, "weave:engine:score:updated", true);

    if (!this.enabled)
      this._log.info("Weave Sync disabled");

    // Create Weave identities (for logging in, and for encryption)
    ID.set('WeaveID', new Identity('Mozilla Services Password', this.username));
    Auth.defaultAuthenticator = new BasicAuthenticator(ID.get('WeaveID'));

    ID.set('WeaveCryptoID',
           new Identity('Mozilla Services Encryption Passphrase', this.username));

    this._updateCachedURLs();

    if (Svc.Prefs.get("autoconnect"))
      this._autoConnect();
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

    let verbose = Svc.Directory.get("ProfD", Ci.nsIFile);
    verbose.QueryInterface(Ci.nsILocalFile);
    verbose.append("weave");
    verbose.append("logs");
    verbose.append("verbose-log.txt");
    if (!verbose.exists())
      verbose.create(verbose.NORMAL_FILE_TYPE, PERMS_FILE);

    let maxSize = 65536; // 64 * 1024 (64KB)
    this._debugApp = new Log4Moz.RotatingFileAppender(verbose, formatter, maxSize);
    this._debugApp.level = Log4Moz.Level[Svc.Prefs.get("log.appender.debugLog")];
    root.addAppender(this._debugApp);
  },

  clearLogs: function WeaveSvc_clearLogs() {
    this._debugApp.clear();
  },

  /**
   * Register the built-in engines for certain applications
   */
  _registerEngines: function WeaveSvc__registerEngines() {
    let engines = [];
    switch (Svc.AppInfo.ID) {
      case FENNEC_ID:
        engines = ["Bookmarks", "History", "Password", "Tab"];
        break;

      case FIREFOX_ID:
        engines = ["Bookmarks", "Form", "History", "Password", "Prefs", "Tab"];
        break;

      case SEAMONKEY_ID:
        engines = ["Form", "History", "Password"];
        break;

      case THUNDERBIRD_ID:
        engines = ["Cookie", "Password"];
        break;
    }

    // Grab the actual engine and register them
    Engines.register(engines.map(function(name) Weave[name + "Engine"]));
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  // nsIObserver

  observe: function WeaveSvc__observe(subject, topic, data) {
    switch (topic) {
      case "network:offline-status-changed":
        // Whether online or offline, we'll reschedule syncs
        this._log.debug("Network offline status change: " + data);
        this._checkSyncStatus();
        break;
      case "private-browsing":
        // Entering or exiting private browsing? Reschedule syncs
        this._log.debug("Private browsing change: " + data);
        this._checkSyncStatus();
        break;
      case "quit-application":
        this._onQuitApplication();
        break;
      case "weave:service:sync:error":
        this._handleSyncError();
        break;
      case "weave:service:sync:finish":
        this._scheduleNextSync();
        this._syncErrors = 0;
        break;
      case "weave:service:backoff:interval":
        let interval = data + Math.random() * data * 0.25; // required backoff + up to 25%
        Status.backoffInterval = interval;
        Status.minimumNextSync = Date.now() + data;
        break;
      case "weave:engine:score:updated":
        this._handleScoreUpdate();
        break;
      case "idle":
        this._log.trace("Idle time hit, trying to sync");
        Svc.Idle.removeIdleObserver(this, IDLE_TIME);
        Utils.delay(function() this.sync(false), 0, this);
        break;
    }
  },

  _doGS: function () {
    this._scoreTimer = null;
    this._calculateScore();
  },

  _handleScoreUpdate: function WeaveSvc__handleScoreUpdate() {
    const SCORE_UPDATE_DELAY = 3000;
    Utils.delay(function() this._doGS(), SCORE_UPDATE_DELAY, this, "_scoreTimer");
  },

  _calculateScore: function WeaveSvc_calculateScoreAndDoStuff() {
    var engines = Engines.getEnabled();
    for (let i = 0;i < engines.length;i++) {
      this._log.trace(engines[i].name + ": score: " + engines[i].score);
      this.globalScore += engines[i].score;
      engines[i].resetScore();
    }

    this._log.trace("Global score updated: " + this.globalScore);

    if (this.globalScore > this.syncThreshold) {
      this._log.debug("Global Score threshold hit, triggering sync.");
      this.syncOnIdle();
    }
    else if (!this._syncTimer) // start the clock if it isn't already
      this._scheduleNextSync();
  },

  // These are global (for all engines)

  // gets cluster from central LDAP server and returns it, or null on error
  _findCluster: function _findCluster() {
    this._log.debug("Finding cluster for user " + this.username);

    let res = new Resource(this.userAPI + this.username + "/node/weave");
    try {
      let node = res.get();
      switch (node.status) {
        case 404:
          this._log.debug("Using serverURL as data cluster (multi-cluster support disabled)");
          return this.serverURL;
        case 0:
        case 200:
          if (node == "null")
            node = null;
          return node;
        default:
          this._log.debug("Unexpected response code: " + node.status);
          break;
      }
    } catch (e) {
      this._log.debug("Network error on findCluster");
      Status.login = LOGIN_FAILED_NETWORK_ERROR;
      throw e;
    }
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

  _verifyLogin: function _verifyLogin()
    this._notify("verify-login", "", function() {
      // Make sure we have a cluster to verify against
      // this is a little weird, if we don't get a node we pretend 
      // to succeed, since that probably means we just don't have storage
      if (this.clusterURL == "" && !this._setCluster()) {
        Status.sync = NO_SYNC_NODE_FOUND;
        Svc.Observer.notifyObservers(null, "weave:service:sync:delayed", "");
        return true;
      }

      try {
        let test = new Resource(this.infoURL).get();
        switch (test.status) {
          case 200:
            // The user is authenticated, so check the passphrase now
            if (!this._verifyPassphrase()) {
              Status.login = LOGIN_FAILED_INVALID_PASSPHRASE;
              return false;
            }

            // Username/password and passphrase all verified
            Status.login = LOGIN_SUCCEEDED;
            return true;

          case 401:
          case 404:
            // Check that we're verifying with the correct cluster
            if (this._setCluster())
              return this._verifyLogin();

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

  _verifyPassphrase: function _verifyPassphrase()
    this._catch(this._notify("verify-passphrase", "", function() {
      try {
        let pubkey = PubKeys.getDefaultKey();
        let privkey = PrivKeys.get(pubkey.privateKeyUri);
        return Svc.Crypto.verifyPassphrase(
          privkey.payload.keyData, this.passphrase,
          privkey.payload.salt, privkey.payload.iv
        );
      } catch (e) {
        // this means no keys are present (or there's a network error)
        return true;
      }
    }))(),

  changePassphrase: function WeaveSvc_changePassphrase(newphrase)
    this._catch(this._notify("changepph", "", function() {
      let pubkey = PubKeys.getDefaultKey();
      let privkey = PrivKeys.get(pubkey.privateKeyUri);

      /* Re-encrypt with new passphrase.
       * FIXME: verifyPassphrase first!
       */
      let newkey = Svc.Crypto.rewrapPrivateKey(privkey.payload.keyData,
          this.passphrase, privkey.payload.salt,
          privkey.payload.iv, newphrase);
      privkey.payload.keyData = newkey;

      let resp = new Resource(privkey.uri).put(privkey);
      if (!resp.success)
        throw resp;

      // Save the new passphrase to the login manager for it to sync
      this.passphrase = newphrase;
      this.persistLogin();
      return true;
    }))(),

  changePassword: function WeaveSvc_changePassword(newpass)
    this._notify("changepwd", "", function() {
      let url = this.userAPI + this.username + "/password";
      try {
        let resp = new Resource(url).post(newpass);
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

      // Save the new password for requests and login manager
      this.password = newpass;
      this.persistLogin();
      return true;
    })(),

  resetPassphrase: function WeaveSvc_resetPassphrase(newphrase)
    this._catch(this._notify("resetpph", "", function() {
      /* Make remote commands ready so we have a list of clients beforehand */
      this.prepCommand("logout", []);
      let clientsBackup = Clients._store.clients;

      /* Wipe */
      this.wipeServer();
      PubKeys.clearCache();
      PrivKeys.clearCache();

      /* Set remote commands before syncing */
      Clients._store.clients = clientsBackup;
      let username = this.username;
      let password = this.password;
      this.logout();

      /* Set this so UI is updated on next run */
      this.passphrase = newphrase;

      /* Login in sync: this also generates new keys */
      this.login(username, password, newphrase);
      this.sync(true);
      return true;
    }))(),

  requestPasswordReset: function WeaveSvc_requestPasswordReset(username) {
    let res = new Resource(Svc.Prefs.get("pwChangeURL"));
    res.authenticator = new NoOpAuthenticator();
    res.headers['Content-Type'] = 'application/x-www-form-urlencoded';
    let ret = res.post('uid=' + username);
    if (ret.indexOf("Further instructions have been sent") >= 0)
      return true;
    return false;
  },

  _autoConnect: let (attempts = 0) function _autoConnect() {
    // Can't autoconnect if we're missing these values
    if (!this.username || !this.password || !this.passphrase)
      return;

    // Nothing more to do on a successful login
    if (this.login())
      return;

    // Something failed, so try again some time later
    let interval = this._calculateBackoff(++attempts, 60 * 1000);
    this._log.debug("Autoconnect failed: " + Status.login + "; retry in " +
      Math.ceil(interval / 1000) + " sec.");
    Utils.delay(function() this._autoConnect(), interval, this, "_autoTimer");
  },

  persistLogin: function persistLogin() {
    // Canceled master password prompt can prevent these from succeeding
    try {
      ID.get("WeaveID").persist();
      ID.get("WeaveCryptoID").persist();
    }
    catch(ex) {}
  },

  login: function WeaveSvc_login(username, password, passphrase)
    this._catch(this._lock(this._notify("login", "", function() {
      this._loggedIn = false;
      if (Svc.IO.offline)
        throw "Application is offline, login should not be called";

      if (username)
        this.username = username;
      if (password)
        this.password = password;
      if (passphrase)
        this.passphrase = passphrase;

      if (!this.username) {
        Status.login = LOGIN_FAILED_NO_USERNAME;
        throw "No username set, login failed";
      }
      if (!this.password) {
        Status.login = LOGIN_FAILED_NO_PASSWORD;
        throw "No password given or found in password manager";
      }
      this._log.info("Logging in user " + this.username);

      if (!this._verifyLogin()) {
        // verifyLogin sets the failure states here
        throw "Login failed: " + Status.login;
      }

      // No need to try automatically connecting after a successful login
      if (this._autoTimer)
        this._autoTimer.clear();

      this._loggedIn = true;
      // Try starting the sync timer now that we're logged in
      this._checkSyncStatus();

      return true;
    })))(),

  logout: function WeaveSvc_logout() {
    // No need to do anything if we're already logged out
    if (!this._loggedIn)
      return;

    this._log.info("Logging out");
    this._loggedIn = false;
    this._keyPair = {};

    // Cancel the sync timer now that we're logged out
    this._checkSyncStatus();

    Svc.Observer.notifyObservers(null, "weave:service:logout:finish", "");
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

  checkUsername: function WeaveSvc_checkUsername(username) {
    let url = this.userAPI + username;
    let res = new Resource(url);
    res.authenticator = new NoOpAuthenticator();

    let data = "";
    try {
      data = res.get();
      if (data.status == 200 && data == "0")
        return "available";
    }
    catch(ex) {}

    // Convert to the error string, or default to generic on exception
    return this._errorStr(data);
  },

  createAccount: function WeaveSvc_createAccount(username, password, email,
                                            captchaChallenge, captchaResponse)
  {
    let payload = JSON.stringify({
      "password": password, "email": email,
      "captcha-challenge": captchaChallenge,
      "captcha-response": captchaResponse
    });

    let url = this.userAPI + username;
    let res = new Resource(url);
    res.authenticator = new Weave.NoOpAuthenticator();

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

  // stuff we need to to after login, before we can really do
  // anything (e.g. key setup)
  _remoteSetup: function WeaveSvc__remoteSetup() {
    let reset = false;

    this._log.debug("Fetching global metadata record");
    let meta = Records.import(this.metaURL);

    let remoteVersion = (meta && meta.payload.storageVersion)?
      meta.payload.storageVersion : "";

    this._log.debug(["Weave Version:", WEAVE_VERSION, "Compatible:",
      COMPATIBLE_VERSION, "Remote:", remoteVersion].join(" "));

    if (!meta || !meta.payload.storageVersion || !meta.payload.syncID ||
        Svc.Version.compare(COMPATIBLE_VERSION, remoteVersion) > 0) {

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
      if (Svc.Version.compare(COMPATIBLE_VERSION, remoteVersion) > 0)
        this._log.info("Server data is older than what Weave supports, server wipe needed");

      if (!this._keyGenEnabled) {
        this._log.info("...and key generation is disabled.  Not wiping. " +
                       "Aborting sync.");
        Status.sync = DESKTOP_VERSION_OUT_OF_DATE;
        return false;
      }
      reset = true;
      this._log.info("Wiping server data");
      this._freshStart();

      if (status == 404)
        this._log.info("Metadata record not found, server wiped to ensure " +
                       "consistency.");
      else // 200
        this._log.info("Server data wiped to ensure consistency after client " +
                       "upgrade (" + remoteVersion + " -> " + WEAVE_VERSION + ")");

    } else if (Svc.Version.compare(remoteVersion, WEAVE_VERSION) > 0) {
      Status.sync = VERSION_OUT_OF_DATE;
      this._log.warn("Server data is of a newer Weave version, this client " +
                     "needs to be upgraded.  Aborting sync.");
      return false;

    } else if (meta.payload.syncID != Clients.syncID) {
      this._log.warn("Meta.payload.syncID is " + meta.payload.syncID +
                     ", Clients.syncID is " + Clients.syncID);
      this.resetClient();
      this._log.info("Reset client because of syncID mismatch.");
      Clients.syncID = meta.payload.syncID;
      this._log.info("Reset the client after a server/client sync ID mismatch");
      this._updateRemoteVersion(meta);
    }
    // We didn't wipe the server and we're not out of date, so update remote
    else
      this._updateRemoteVersion(meta);

    let needKeys = true;
    let pubkey = PubKeys.getDefaultKey();
    if (!pubkey)
      this._log.debug("Could not get public key");
    else if (pubkey.keyData == null)
      this._log.debug("Public key has no key data");
    else {
      // make sure we have a matching privkey
      let privkey = PrivKeys.get(pubkey.privateKeyUri);
      if (!privkey)
        this._log.debug("Could not get private key");
      else if (privkey.keyData == null)
        this._log.debug("Private key has no key data");
      else
        return true;
    }

    if (needKeys) {
      if (PubKeys.response.status != 404 && PrivKeys.response.status != 404) {
        this._log.warn("Couldn't download keys from server, aborting sync");
        this._log.debug("PubKey HTTP status: " + PubKeys.response.status);
        this._log.debug("PrivKey HTTP status: " + PrivKeys.response.status);
        this._checkServerError(PubKeys.response);
        this._checkServerError(PrivKeys.response);
        Status.sync = KEYS_DOWNLOAD_FAIL;
        return false;
      }

      if (!this._keyGenEnabled) {
        this._log.warn("Couldn't download keys from server, and key generation" +
                       "is disabled.  Aborting sync");
        Status.sync = NO_KEYS_NO_KEYGEN;
        return false;
      }

      if (!reset) {
        this._log.warn("Calling freshStart from !reset case.");
        this._freshStart();
        this._log.info("Server data wiped to ensure consistency due to missing keys");
      }

      let passphrase = ID.get("WeaveCryptoID");
      if (passphrase.password) {
        let keys = PubKeys.createKeypair(passphrase, PubKeys.defaultKeyUri,
                                         PrivKeys.defaultKeyUri);
        try {
          // Upload and cache the keypair
          PubKeys.uploadKeypair(keys);
          PubKeys.set(keys.pubkey.uri, keys.pubkey);
          PrivKeys.set(keys.privkey.uri, keys.privkey);
          return true;
        } catch (e) {
          Status.sync = KEYS_UPLOAD_FAIL;
          this._log.error("Could not upload keys: " + Utils.exceptionStr(e));
        }
      } else {
        Status.sync = SETUP_FAILED_NO_PASSPHRASE;
        this._log.warn("Could not get encryption passphrase");
      }
    }

    return false;
  },

  /**
   * Determine if a sync should run.
   *
   * @return Reason for not syncing; not-truthy if sync should run
   */
  _checkSync: function WeaveSvc__checkSync() {
    let reason = "";
    if (!this.enabled)
      reason = kSyncWeaveDisabled;
    else if (!this._loggedIn)
      reason = kSyncNotLoggedIn;
    else if (Svc.IO.offline)
      reason = kSyncNetworkOffline;
    else if (Svc.Private && Svc.Private.privateBrowsingEnabled)
      // Svc.Private doesn't exist on Fennec -- don't assume it's there.
      reason = kSyncInPrivateBrowsing;
    else if (Status.minimumNextSync > Date.now())
      reason = kSyncBackoffNotMet;

    return reason;
  },

  /**
   * Remove any timers/observers that might trigger a sync
   */
  _clearSyncTriggers: function _clearSyncTriggers() {
    // Clear out any scheduled syncs
    if (this._syncTimer)
      this._syncTimer.clear();

    // Clear out a sync that's just waiting for idle if we happen to have one
    try {
      Svc.Idle.removeIdleObserver(this, IDLE_TIME);
    }
    catch(ex) {}
  },

  /**
   * Check if we should be syncing and schedule the next sync, if it's not scheduled
   */
  _checkSyncStatus: function WeaveSvc__checkSyncStatus() {
    // Should we be syncing now, if not, cancel any sync timers and return
    // if we're in backoff, we'll schedule the next sync
    let reason = this._checkSync();
    if (reason && reason != kSyncBackoffNotMet) {
      this._clearSyncTriggers();
      Status.service = STATUS_DISABLED;
      return;
    }

    // otherwise, schedule the sync
    this._scheduleNextSync();
  },

  /**
   * Call sync() on an idle timer
   *
   */
  syncOnIdle: function WeaveSvc_syncOnIdle() {
    this._log.debug("Idle timer created for sync, will sync after " +
                    IDLE_TIME + " seconds of inactivity.");
    Svc.Idle.addIdleObserver(this, IDLE_TIME);
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
      this.syncOnIdle();
      return;
    }

    this._log.debug("Next sync in " + Math.ceil(interval / 1000) + " sec.");
    Utils.delay(function() this.syncOnIdle(), interval, this, "_syncTimer");

    // Save the next sync time in-case sync is disabled (logout/offline/etc.)
    this.nextSync = Date.now() + interval;
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

    const MINIMUM_BACKOFF_INTERVAL = 15 * 60 * 1000;     // 15 minutes
    let interval = this._calculateBackoff(this._syncErrors, MINIMUM_BACKOFF_INTERVAL);

    this._scheduleNextSync(interval);

    let d = new Date(Date.now() + interval);
    this._log.config("Starting backoff, next sync at:" + d.toString());
  },

  /**
   * Sync up engines with the server.
   */
  sync: function sync()
    this._catch(this._lock(this._notify("sync", "", function() {
      Status.resetSync();
    
    // if we don't have a node, get one.  if that fails, retry in 10 minutes
    if (this.clusterURL == "" && !this._setCluster()) {
      this._scheduleNextSync(10 * 60 * 1000);
      return;
    }

    // Make sure we should sync or record why we shouldn't
    let reason = this._checkSync();
    if (reason) {
      // this is a purposeful abort rather than a failure, so don't set
      // any status bits
      reason = "Can't sync: " + reason;
      throw reason;
    }

    // Clear out any potentially pending syncs now that we're syncing
    this._clearSyncTriggers();
    this.nextSync = 0;

    this.globalScore = 0;

    if (!(this._remoteSetup()))
      throw "aborting sync, remote setup failed";

    // Figure out what the last modified time is for each collection
    let info = new Resource(this.infoURL).get();
    if (!info.success)
      throw "aborting sync, failed to get collections";

    // Convert the response to an object and read out the modified times
    for each (let engine in [Clients].concat(Engines.getEnabled()))
      engine.lastModified = info.obj[engine.name] || 0;

    this._log.trace("Refreshing client list");
    Clients.sync();

    // Process the incoming commands if we have any
    if (Clients.getClients()[Clients.clientID].commands) {
      try {
        if (!(this.processCommands())) {
          Status.sync = ABORT_SYNC_COMMAND;
          throw "aborting sync, process commands said so";
        }

        // Repeat remoteSetup in-case the commands forced us to reset
        if (!(this._remoteSetup()))
          throw "aborting sync, remote setup failed after processing commands";
      }
      finally {
        // Always immediately push back the local client (now without commands)
        Clients.sync();
      }
    }

    // Update the client mode now because it might change what we sync
    this._updateClientMode();

    try {
      for each (let engine in Engines.getEnabled()) {
        // If there's any problems with syncing the engine, report the failure
        if (!(this._syncEngine(engine)) || Status.enforceBackoff) {
          this._log.info("Aborting sync");
          break;
        }
      }

      if (this._syncError)
        throw "Some engines did not sync correctly";
      else {
        Svc.Prefs.set("lastSync", new Date().toString());
        Status.sync = SYNC_SUCCEEDED;
        this._log.info("Sync completed successfully");
      }
    } finally {
      this._syncError = false;
    }
  })))(),

  /**
   * Process the locally stored clients list to figure out what mode to be in
   */
  _updateClientMode: function _updateClientMode() {
    let numClients = 0;
    let hasMobile = false;

    // Check how many and what type of clients we have
    for each (let {type} in Clients.getClients()) {
      numClients++;
      hasMobile = hasMobile || type == "mobile";
    }

    // Nothing to do if it's the same amount
    if (this.numClients == numClients)
      return;

    this._log.debug("Client count: " + this.numClients + " -> " + numClients);
    this.numClients = numClients;

    let tabEngine = Engines.get("tabs");
    if (numClients == 1) {
      this.syncInterval = SINGLE_USER_SYNC;
      this.syncThreshold = SINGLE_USER_THRESHOLD;

      // Disable tabs sync for single client, but store the original value
      Svc.Prefs.set("engine.tabs.backup", tabEngine.enabled);
      tabEngine.enabled = false;
    }
    else {
      this.syncInterval = hasMobile ? MULTI_MOBILE_SYNC : MULTI_DESKTOP_SYNC;
      this.syncThreshold = hasMobile ? MULTI_MOBILE_THRESHOLD : MULTI_DESKTOP_THRESHOLD;

      // Restore the original tab enabled value
      tabEngine.enabled = Svc.Prefs.get("engine.tabs.backup", true);
      Svc.Prefs.reset("engine.tabs.backup");
    }
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
      this._log.debug(Utils.exceptionStr(e));
      return true;
    }
    finally {
      // If this engine has more to fetch, remember that globally
      if (engine.toFetch != null && engine.toFetch.length > 0)
        Status.partial = true;
    }
  },

  _freshStart: function WeaveSvc__freshStart() {
    this.resetClient();
    this._log.info("Reset client data from freshStart.");
    this._log.info("Client metadata wiped, deleting server data");
    this.wipeServer();

    // XXX Bug 504125 Wait a while after wiping so that the DELETEs replicate
    Sync.sleep(2000);

    this._log.debug("Uploading new metadata record");
    let meta = new WBORecord(this.metaURL);
    meta.payload.syncID = Clients.syncID;
    this._updateRemoteVersion(meta);
  },

  _updateRemoteVersion: function WeaveSvc__updateRemoteVersion(meta) {
    // Don't update if the remote version is already newer
    if (Svc.Version.compare(meta.payload.storageVersion, WEAVE_VERSION) >= 0)
      return;

    this._log.debug("Setting meta payload storage version to " + WEAVE_VERSION);
    meta.payload.storageVersion = WEAVE_VERSION;
    let resp = new Resource(meta.uri).put(meta);
    if (!resp.success)
      throw resp;
  },


  /**
   * Check to see if this is a failure
   *
   */
  _checkServerError: function WeaveSvc__checkServerError(resp) {
    if (Utils.checkStatus(resp.status, null, [500, [502, 504]])) {
      Status.enforceBackoff = true;
      if (resp.status == 503 && resp.headers["Retry-After"])
        Observers.notify("weave:service:backoff:interval", parseInt(resp.headers["Retry-After"], 10));
    }
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
   * Wipe all user data from the server.
   *
   * @param engines [optional]
   *        Array of engine names to wipe. If not given, all engines are used.
   */
  wipeServer: function WeaveSvc_wipeServer(engines)
    this._catch(this._notify("wipe-server", "", function() {
      // Grab all the collections for the user and delete each one
      let info = new Resource(this.infoURL).get();
      for (let name in info.obj) {
        try {
          // If we have a list of engines, make sure it's one we want
          if (engines && engines.indexOf(name) == -1)
            continue;

          new Resource(this.storageURL + name).delete();
        }
        catch(ex) {
          this._log.debug("Exception on wipe of '" + name + "': " + Utils.exceptionStr(ex));
        }
      }
    }))(),

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
        if (engine._testDecrypt())
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
      // Clear out any server data
      //this.wipeServer(engines);

      // Only wipe the engines provided
      if (engines) {
        engines.forEach(function(e) this.prepCommand("wipeEngine", [e]), this);
        return;
      }

      // Tell the remote machines to wipe themselves
      this.prepCommand("wipeAll", []);
    }))(),

  /**
   * Reset local service information like logs, sync times, caches.
   */
  resetService: function WeaveSvc_resetService()
    this._catch(this._notify("reset-service", "", function() {
      // First drop old logs to track client resetting behavior
      this.clearLogs();
      this._log.info("Logs reinitialized for service reset");

      // Pretend we've never synced to the server and drop cached data
      Clients.resetSyncID();
      Svc.Prefs.reset("lastSync");
      for each (let cache in [PubKeys, PrivKeys, CryptoMetas, Records])
        cache.clearCache();
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

      // XXX Bug 480448: Delete any snapshots from old code
      try {
        let cruft = Svc.Directory.get("ProfD", Ci.nsIFile);
        cruft.QueryInterface(Ci.nsILocalFile);
        cruft.append("weave");
        cruft.append("snapshots");
        if (cruft.exists())
          cruft.remove(true);
      } catch (e) {
        this._log.debug("Could not remove old snapshots: " + Utils.exceptionStr(e));
      }
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
      let info = Clients.getInfo(Clients.clientID);
      let commands = info.commands;

      // Immediately clear out the commands as we've got them locally
      delete info.commands;
      Clients.setInfo(Clients.clientID, info);

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

    // Package the command/args pair into an object
    let action = {
      command: command,
      args: args,
    };
    let actionStr = command + "(" + args + ")";

    // Convert args into a string to simplify array comparisons
    let jsonArgs = JSON.stringify(args);
    let notDupe = function(action) action.command != command ||
      JSON.stringify(action.args) != jsonArgs;

    this._log.info("Sending clients: " + actionStr + "; " + commandData.desc);

    // Add the action to each remote client
    for (let guid in Clients.getClients()) {
      // Don't send commands to the local client
      if (guid == Clients.clientID)
        continue;

      let info = Clients.getInfo(guid);
      // Set the action to be a new commands array if none exists
      if (info.commands == null)
        info.commands = [action];
      // Add the new action if there are no duplicates
      else if (info.commands.every(notDupe))
        info.commands.push(action);
      // Must have been a dupe.. skip!
      else
        continue;

      Clients.setInfo(guid, info);
      this._log.trace("Client " + guid + " got a new action: " + actionStr);
    }
  },
};
