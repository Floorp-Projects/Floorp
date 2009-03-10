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

// The following constants determine when Weave will automatically sync data.

// An interval of one minute, initial threshold of 100, and step of 5 means
// that we'll try to sync each engine 21 times, once per minute, at
// consecutively lower thresholds (from 100 down to 5 in steps of 5 and then
// one more time with the threshold set to the minimum 1) before resetting
// the engine's threshold to the initial value and repeating the cycle
// until at some point the engine's score exceeds the threshold, at which point
// we'll sync it, reset its threshold to the initial value, rinse, and repeat.

// How long we wait between sync checks.
const SCHEDULED_SYNC_INTERVAL = 60 * 1000 * 5; // five minutes

// INITIAL_THRESHOLD represents the value an engine's score has to exceed
// in order for us to sync it the first time we start up (and the first time
// we do a sync check after having synced the engine or reset the threshold).
const INITIAL_THRESHOLD = 75;

// THRESHOLD_DECREMENT_STEP is the amount by which we decrement an engine's
// threshold each time we do a sync check and don't sync that engine.
const THRESHOLD_DECREMENT_STEP = 25;

// The following are various error messages for not syncing
const kSyncWeaveDisabled = "Weave is disabled";
const kSyncNotLoggedIn = "User is not logged in";
const kSyncNetworkOffline = "Network is offline";
const kSyncInPrivateBrowsing = "Private browsing is enabled";
const kSyncNotScheduled = "Not scheduled to do sync";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/wrap.js");
Cu.import("resource://weave/faultTolerance.js");
Cu.import("resource://weave/auth.js");
Cu.import("resource://weave/resource.js");
Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/crypto.js");
Cu.import("resource://weave/base_records/keys.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/oauth.js");
Cu.import("resource://weave/identity.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/engines/clientData.js");

Function.prototype.async = Async.sugar;

// for export
let Weave = {};
Cu.import("resource://weave/constants.js", Weave);
Cu.import("resource://weave/util.js", Weave);
Cu.import("resource://weave/async.js", Weave);
Cu.import("resource://weave/faultTolerance.js", Weave);
Cu.import("resource://weave/auth.js", Weave);
Cu.import("resource://weave/resource.js", Weave);
Cu.import("resource://weave/base_records/keys.js", Weave);
Cu.import("resource://weave/notifications.js", Weave);
Cu.import("resource://weave/identity.js", Weave);
Cu.import("resource://weave/stores.js", Weave);
Cu.import("resource://weave/engines.js", Weave);
Cu.import("resource://weave/oauth.js", Weave);
Cu.import("resource://weave/service.js", Weave); // ??
Cu.import("resource://weave/engines/clientData.js", Weave);

Utils.lazy(Weave, 'Service', WeaveSvc);

/*
 * Service singleton
 * Main entry point into Weave's sync framework
 */

function WeaveSvc() {
  this._notify = Wrap.notify("weave:service:");
}
WeaveSvc.prototype = {

  _localLock: Wrap.localLock,
  _catchAll: Wrap.catchAll,
  _isQuitting: false,
  _loggedIn: false,
  _syncInProgress: false,
  _keyGenEnabled: true,
  _mostRecentError: null,

  // object for caching public and private keys
  _keyPair: {},

  // Timer object for automagically syncing
  _syncTimer: null,

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
    this._genKeyURLs();
  },

  get password() { return ID.get('WeaveID').password; },
  set password(value) { ID.get('WeaveID').password = value; },

  get passphrase() { return ID.get('WeaveCryptoID').password; },
  set passphrase(value) { ID.get('WeaveCryptoID').password = value; },

  // chrome-provided callbacks for when the service needs a password/passphrase
  set onGetPassword(value) {
    ID.get('WeaveID').onGetPassword = value;
  },
  set onGetPassphrase(value) {
    ID.get('WeaveCryptoID').onGetPassword = value;
  },

  get baseURL() {
    let url = Svc.Prefs.get("serverURL");
    if (!url)
      throw "No server URL set";
    if (url[url.length-1] != '/')
      url += '/';
    url += "0.3/";
    return url;
  },
  set baseURL(value) {
    Svc.Prefs.set("serverURL", value);
  },

  get clusterURL() {
    let url = Svc.Prefs.get("clusterURL");
    if (!url)
      return null;
    if (url[url.length-1] != '/')
      url += '/';
    url += "0.3/user/";
    return url;
  },
  set clusterURL(value) {
    Svc.Prefs.set("clusterURL", value);
    this._genKeyURLs();
  },

  get userPath() { return ID.get('WeaveID').username; },

  get isLoggedIn() { return this._loggedIn; },

  get isQuitting() { return this._isQuitting; },
  set isQuitting(value) { this._isQuitting = value; },

  get cancelRequested() { return Engines.cancelRequested; },
  set cancelRequested(value) { Engines.cancelRequested = value; },

  get keyGenEnabled() { return this._keyGenEnabled; },
  set keyGenEnabled(value) { this._keyGenEnabled = value; },

  get enabled() { return Svc.Prefs.get("enabled"); },
  set enabled(value) { Svc.Prefs.set("enabled", value); },

  get mostRecentError() { return this._mostRecentError; },

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

  _genKeyURLs: function WeaveSvc__genKeyURLs() {
    let url = this.clusterURL + this.username;
    PubKeys.defaultKeyUri = url + "/keys/pubkey";
    PrivKeys.defaultKeyUri = url + "/keys/privkey";
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

  onWindowOpened: function Weave__onWindowOpened() {
  },

  // one-time initialization like setting up observers and the like
  // xxx we might need to split some of this out into something we can call
  //     again when username/server/etc changes
  _onStartup: function WeaveSvc__onStartup() {
    let self = yield;
    this._initLogs();
    this._log.info("Weave " + WEAVE_VERSION + " initializing");

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

    Utils.prefs.addObserver("", this, false);
    Svc.Observer.addObserver(this, "network:offline-status-changed", true);
    Svc.Observer.addObserver(this, "private-browsing", true);
    Svc.Observer.addObserver(this, "quit-application", true);
    FaultTolerance.Service; // initialize FT service

    if (!this.enabled)
      this._log.info("Weave Sync disabled");

    // Create Weave identities (for logging in, and for encryption)
    ID.set('WeaveID', new Identity('Mozilla Services Password', this.username));
    Auth.defaultAuthenticator = new BasicAuthenticator(ID.get('WeaveID'));

    ID.set('WeaveCryptoID',
           new Identity('Mozilla Services Encryption Passphrase', this.username));

    this._genKeyURLs();

    if (Svc.Prefs.get("autoconnect") && this.username) {
      try {
	if (yield this.login(self.cb))
	  yield this.sync(self.cb, true);
      } catch (e) {}
    }
    self.done();
  },
  onStartup: function WeaveSvc_onStartup(callback) {
    this._onStartup.async(this, callback);
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

    let brief = Svc.Directory.get("ProfD", Ci.nsIFile);
    brief.QueryInterface(Ci.nsILocalFile);
    brief.append("weave");
    brief.append("logs");
    brief.append("brief-log.txt");
    if (!brief.exists())
      brief.create(brief.NORMAL_FILE_TYPE, PERMS_FILE);

    let verbose = brief.parent.clone();
    verbose.append("verbose-log.txt");
    if (!verbose.exists())
      verbose.create(verbose.NORMAL_FILE_TYPE, PERMS_FILE);

    this._briefApp = new Log4Moz.RotatingFileAppender(brief, formatter);
    this._briefApp.level = Log4Moz.Level[Svc.Prefs.get("log.appender.briefLog")];
    root.addAppender(this._briefApp);
    this._debugApp = new Log4Moz.RotatingFileAppender(verbose, formatter);
    this._debugApp.level = Log4Moz.Level[Svc.Prefs.get("log.appender.debugLog")];
    root.addAppender(this._debugApp);
  },

  clearLogs: function WeaveSvc_clearLogs() {
    this._briefApp.clear();
    this._debugApp.clear();
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  // nsIObserver

  observe: function WeaveSvc__observe(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        switch (data) {
          case "enabled":
          case "schedule":
            // Potentially we'll want to reschedule syncs
            this._checkSync();
            break;
        }
        break;
      case "network:offline-status-changed":
        // Whether online or offline, we'll reschedule syncs
        this._log.debug("Network offline status change: " + data);
        this._checkSync();
        break;
      case "private-browsing":
        // Entering or exiting private browsing? Reschedule syncs
        this._log.debug("Private browsing change: " + data);
        this._checkSync();
        break;
      case "quit-application":
        this._onQuitApplication();
        break;
    }
  },

  _onQuitApplication: function WeaveSvc__onQuitApplication() {
  },

  // These are global (for all engines)

  // gets cluster from central LDAP server and sets this.clusterURL
  findCluster: function WeaveSvc_findCluster(onComplete, username) {
    let fn = function WeaveSvc__findCluster() {
      let self = yield;
      let ret = false;

      this._log.debug("Finding cluster for user " + username);

      let res = new Resource(this.baseURL + "api/register/chknode/" + username);
      try {
	yield res.get(self.cb);
      } catch (e) { /* we check status below */ }

      if (res.lastChannel.responseStatus == 404) {
        this._log.debug("Using serverURL as data cluster (multi-cluster support disabled)");
        this.clusterURL = Svc.Prefs.get("serverURL");
        ret = true;

      } else if (res.lastChannel.responseStatus == 200) {
        // XXX Bug 480480 Work around the server sending a trailing newline
        this.clusterURL = 'https://' + res.data.trim() + '/';
        ret = true;
      }
      self.done(ret);
    };
    fn.async(this, onComplete);
  },

  verifyLogin: function WeaveSvc_verifyLogin(onComplete, username, password, isLogin) {
    let user = username, pass = password;

    let fn = function WeaveSvc__verifyLogin() {
      let self = yield;
      this._log.debug("Verifying login for user " + user);

      let cluster = this.clusterURL;
      yield this.findCluster(self.cb, username);

      let res = new Resource(this.clusterURL + user);
      yield res.get(self.cb);

      if (!isLogin) // restore cluster so verifyLogin has no impact
	this.clusterURL = cluster;

      //Svc.Json.decode(res.data); // throws if not json
      self.done(true);
    };
    this._catchAll(this._notify("verify-login", "", fn)).async(this, onComplete);
  },

  _verifyPassphrase: function WeaveSvc__verifyPassphrase(username, password,
                                                         passphrase) {
    let self = yield;

    this._log.debug("Verifying passphrase");

    this.username = username;
    ID.get('WeaveID').setTempPassword(password);

    let id = new Identity('Passphrase Verification', username);
    id.setTempPassword(passphrase);

    let pubkey = yield PubKeys.getDefaultKey(self.cb);
    let privkey = yield PrivKeys.get(self.cb, pubkey.PrivKeyUri);

    // fixme: decrypt something here
  },
  verifyPassphrase: function WeaveSvc_verifyPassphrase(onComplete, username,
                                                       password, passphrase) {
    this._catchAll(
      this._localLock(
	this._notify("verify-passphrase", "", this._verifyPassphrase,
                     username, password, passphrase))).async(this, onComplete);
  },

  login: function WeaveSvc_login(onComplete, username, password, passphrase) {
    let user = username, pass = password, passp = passphrase;

    let fn = function WeaveSvc__login() {
      let self = yield;

      this._loggedIn = false;

	if (typeof(user) != 'undefined')
          this.username = user;
	if (typeof(pass) != 'undefined')
          ID.get('WeaveID').setTempPassword(pass);
	if (typeof(passp) != 'undefined')
          ID.get('WeaveCryptoID').setTempPassword(passp);

	if (!this.username) {
	  this._mostRecentError = "No username set.";
	  throw "No username set, login failed";
	}

	if (!this.password) {
	  this._mostRecentError = "No password set.";
	  throw "No password given or found in password manager";
	}

	this._log.debug("Logging in user " + this.username);

	if (!(yield this.verifyLogin(self.cb, this.username, this.password, true))) {
	  this._mostRecentError = "Login failed. Check your username/password/phrase.";
	  throw "Login failed";
	}

	this._loggedIn = true;
	self.done(true);
    };
    this._catchAll(
      this._localLock(
	this._notify("login", "", fn))).async(this, onComplete);
  },

  logout: function WeaveSvc_logout() {
    this._log.info("Logging out");
    this._loggedIn = false;
    this._keyPair = {};
    ID.get('WeaveID').setTempPassword(null); // clear cached password
    ID.get('WeaveCryptoID').setTempPassword(null); // and passphrase

    // Cancel the sync timer now that we're logged out
    this._checkSync();

    Svc.Observer.notifyObservers(null, "weave:service:logout:finish", "");
  },

  // stuff we need to to after login, before we can really do
  // anything (e.g. key setup)
  _remoteSetup: function WeaveSvc__remoteSetup() {
    let self = yield;
    let ret = false; // false to abort sync
    let reset = false;

    this._log.debug("Fetching global metadata record");
    let meta = yield Records.import(self.cb, this.clusterURL +
				    this.username + "/meta/global");

    let remoteVersion = (meta && meta.payload.storageVersion)?
      meta.payload.storageVersion : "";

    this._log.debug("Min supported storage version is " + MIN_SERVER_STORAGE_VERSION);
    this._log.debug("Remote storage version is " + remoteVersion);

    if (!meta || !meta.payload.storageVersion || !meta.payload.syncID ||
        Svc.Version.compare(MIN_SERVER_STORAGE_VERSION, remoteVersion) > 0) {

      // abort the server wipe if the GET status was anything other than 404 or 200
      let status = Records.lastResource.lastChannel.responseStatus;
      if (status != 200 && status != 404) {
	this._mostRecentError = "Unknown error when downloading metadata.";
	this._log.warn("Unknown error while downloading metadata record. " +
                       "Aborting sync.");
	self.done(false);
	return;
      }

      if (!meta)
	this._log.info("No metadata record, server wipe needed");
      if (meta && !meta.payload.syncID)
	this._log.warn("No sync id, server wipe needed");
      if (Svc.Version.compare(MIN_SERVER_STORAGE_VERSION, remoteVersion) > 0)
	this._log.info("Server storage version no longer supported, server wipe needed");

      reset = true;
      this._log.info("Wiping server data");
      yield this._freshStart.async(this, self.cb);

      if (status == 404)
        this._log.info("Metadata record not found, server wiped to ensure " +
                       "consistency.");
      else // 200
        this._log.info("Server data wiped to ensure consistency after client " +
                       "upgrade (" + remoteVersion + " -> " + WEAVE_VERSION + ")");

    } else if (Svc.Version.compare(remoteVersion, WEAVE_VERSION) > 0) {
      this._mostRecentError = "Client needs to be upgraded.";
      this._log.warn("Server data is of a newer Weave version, this client " +
                     "needs to be upgraded.  Aborting sync.");
      self.done(false);
      return;

    } else if (meta.payload.syncID != Clients.syncID) {
      this._log.warn("Meta.payload.syncID is " + meta.payload.syncID);
      this._log.warn(", Clients.syncID is " + Clients.syncID);
      yield this.resetClient(self.cb);
      this._log.info("Reset client because of syncID mismatch.");
      Clients.syncID = meta.payload.syncID;
      this._log.info("Reset the client after a server/client sync ID mismatch");
    }

    let needKeys = true;
    let pubkey = yield PubKeys.getDefaultKey(self.cb);
    if (pubkey) {
      // make sure we have a matching privkey
      let privkey = yield PrivKeys.get(self.cb, pubkey.privateKeyUri);
      if (privkey) {
        needKeys = false;
        ret = true;
      }
    }
    if (needKeys) {
      if (PubKeys.lastResource.lastChannel.responseStatus != 404 &&
	  PrivKeys.lastResource.lastChannel.responseStatus != 404) {
	this._log.warn("Couldn't download keys from server, aborting sync");
	this._log.debug("PubKey HTTP response status: " +
			PubKeys.lastResource.lastChannel.responseStatus);
	this._log.debug("PrivKey HTTP response status: " +
			PrivKeys.lastResource.lastChannel.responseStatus);
	this._mostRecentError = "Can't download keys from server.";
	self.done(false);
	return;
      }

      if (!this._keyGenEnabled) {
	this._log.warn("Couldn't download keys from server, and key generation" +
		       "is disabled.  Aborting sync");
	this._mostRecentError = "No keys. Try syncing from desktop first.";
	self.done(false);
	return;
      }

      if (!reset) {
	this._log.warn("Calling freshStart from !reset case.");
	yield this._freshStart.async(this, self.cb);
	this._log.info("Server data wiped to ensure consistency due to missing keys");
      }

      let pass = yield ID.get('WeaveCryptoID').getPassword(self.cb);
      if (pass) {
        let keys = PubKeys.createKeypair(pass, PubKeys.defaultKeyUri,
                                         PrivKeys.defaultKeyUri);
        try {
	  yield PubKeys.uploadKeypair(self.cb, keys);
          ret = true;
        } catch (e) {
	  this._mostRecentError = "Could not upload keys.";
          this._log.error("Could not upload keys: " + Utils.exceptionStr(e));
	  // FIXME no lastRequest anymore
          //this._log.error(keys.pubkey.lastRequest.responseText);
        }
      } else {
	this._mostRecentError = "Could not get encryption passphrase.";
        this._log.warn("Could not get encryption passphrase");
      }
    }

    self.done(ret);
  },

  /**
   * Engine scores must meet or exceed this value before we sync them when
   * using thresholds. These are engine-specific, as different kinds of data
   * change at different rates, so we store them in a hash by engine name.
   */
  _syncThresh: {},

  /**
   * Determine if a sync should run. If so, schedule a repeating sync;
   * otherwise, cancel future syncs and return a reason.
   *
   * @return Reason for not syncing; not-truthy if sync should run
   */
  _checkSync: function Weave__checkSync() {
    let reason = "";
    if (!this.enabled)
      reason = kSyncWeaveDisabled;
    else if (!this._loggedIn)
      reason = kSyncNotLoggedIn;
    else if (Svc.IO.offline)
      reason = kSyncNetworkOffline;
    else if (Svc.Private.privateBrowsingEnabled)
      reason = kSyncInPrivateBrowsing;
    else if (Svc.Prefs.get("schedule", 0) != 1)
      reason = kSyncNotScheduled;

    // A truthy reason means we shouldn't continue to sync
    if (reason) {
      // Cancel any future syncs
      if (this._syncTimer) {
        this._syncTimer.cancel();
        this._syncTimer = null;
      }
      this._log.config("Weave scheduler disabled: " + reason);
    }
    // We're good to sync, so schedule a repeating sync if we haven't yet
    else if (!this._syncTimer) {
      this._syncTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      let listener = new Utils.EventListener(Utils.bind2(this, this.sync));
      this._syncTimer.initWithCallback(listener, SCHEDULED_SYNC_INTERVAL,
                                       Ci.nsITimer.TYPE_REPEATING_SLACK);
      this._log.config("Weave scheduler enabled");
    }

    return reason;
  },

  /**
   * Sync up engines with the server.
   *
   * @param fullSync
   *        True to unconditionally sync all engines
   * @throw Reason for not syncing
   */
  _sync: function WeaveSvc__sync(useThresh) {
    let self = yield;

    // Use thresholds to determine what to sync only if it's not a full sync
    let useThresh = !fullSync;

    // Make sure we should sync or record why we shouldn't. We always obey the
    // reason if we're using thresholds (not a full sync); otherwise, allow
    // "not scheduled" as future syncs have already been canceled by checkSync.
    let reason = this._checkSync();
    if (reason && (useThresh || reason != kSyncNotScheduled)) {
      reason = "Can't sync: " + reason;
      this._mostRecentError = reason;
      throw reason;
    }

    if (!(yield this._remoteSetup.async(this, self.cb))) {
      throw "aborting sync, remote setup failed";
    }

    this._log.debug("Refreshing client list");
    yield Clients.sync(self.cb);

    try {
      for each (let engine in Engines.getAll()) {
        let name = engine.name;

        // Nothing to do for disabled engines
        if (!engine.enabled)
          continue;

        // Conditionally reset the threshold for the current engine
        let resetThresh = Utils.bind2(this, function WeaveSvc__resetThresh(cond)
          cond ? this._syncThresh[name] = INITIAL_THRESHOLD : undefined);

        // Initialize the threshold if it doesn't exist yet
        resetThresh(!(name in this._syncThresh));

        // Determine if we should sync if using thresholds
        if (useThresh) {
          let score = engine.score;
          let thresh = this._syncThresh[name];
          if (score >= thresh)
            this._log.debug("Syncing " + name + "; " +
                            "score " + score + " >= thresh " + thresh);
          else {
            this._log.debug("Not syncing " + name + "; " +
                            "score " + score + " < thresh " + thresh);

            // Decrement the threshold by a standard amount with a lower bound of 1
            this._syncThresh[name] = Math.max(thresh - THRESHOLD_DECREMENT_STEP, 1);

            // No need to sync this engine for now
            continue;
          }
        }

        // If there's any problems with syncing the engine, report the failure
        if (!(yield this._syncEngine.async(this, self.cb, engine))) {
          this._mostRecentError = "Failure in " + engine.displayName;
          this._log.info("Aborting sync");
          break;
        }

        // We've successfully synced, so reset the threshold. We do this after
        // a successful sync so failures can try again on next sync, but this
        // could trigger too many syncs if the server is having problems.
        resetThresh(useThresh);
      }

      if (this._syncError)
        this._log.warn("Some engines did not sync correctly");
      else {
        Svc.Prefs.set("lastSync", new Date().toString());
        this._log.info("Sync completed successfully");
      }
    } finally {
      this.cancelRequested = false;
      this._syncError = false;
    }
  },

  /**
   * Do a synchronized sync (only one sync at a time).
   *
   * @param onComplete
   *        Callback when this method completes
   * @param fullSync
   *        True to unconditionally sync all engines
   */
  sync: function WeaveSvc_sync(onComplete, fullSync) {
    fullSync = true; // not doing thresholds yet
    this._catchAll(this._notify("sync", "", this._localLock(this._sync))).
      async(this, onComplete, fullSync);
  },

  // returns true if sync should proceed
  // false / no return value means sync should be aborted
  _syncEngine: function WeaveSvc__syncEngine(engine) {
    let self = yield;
    try {
      yield engine.sync(self.cb);
      if (!this.cancelRequested)
	self.done(true);
    }
    catch(e) {
      this._syncError = true;
      if (FaultTolerance.Service.onException(e))
	self.done(true);
    }
  },

  _freshStart: function WeaveSvc__freshStart() {
    let self = yield;
    yield this.resetClient(self.cb);
    this._log.info("Reset client data from freshStart.");
    this._log.info("Client metadata wiped, deleting server data");
    yield this.wipeServer(self.cb);

    this._log.debug("Uploading new metadata record");
    meta = new WBORecord(this.clusterURL + this.username + "/meta/global");
    this._log.debug("Setting meta payload storage version to " + WEAVE_VERSION);
    meta.payload.storageVersion = WEAVE_VERSION;
    meta.payload.syncID = Clients.syncID;
    let res = new Resource(meta.uri);
    yield res.put(self.cb, meta.serialize());
  },

  /**
   * Wipe all user data from the server.
   */
  wipeServer: function WeaveSvc_wipeServer(onComplete) {
    let fn = function WeaveSvc__wipeServer() {
      let self = yield;

      // Grab all the collections for the user
      let userURL = this.clusterURL + this.username + "/";
      let res = new Resource(userURL);
      yield res.get(self.cb);

      // Get the array of collections and delete each one
      let allCollections = Svc.Json.decode(res.data);
      for each (let name in allCollections) {
        try {
          yield new Resource(userURL + name).delete(self.cb);
        }
        catch(ex) {
          this._log.debug("Exception on wipe of '" + name + "': " + Utils.exceptionStr(ex));
        }
      }
    };
    this._catchAll(this._notify("wipe-server", "", fn)).async(this, onComplete);
  },

  /**
   * Reset the client by getting rid of any local server data and client data.
   */
  resetClient: function WeaveSvc_resetClient(onComplete) {
    let fn = function WeaveSvc__resetClient() {
      let self = yield;

      // First drop old logs to track client resetting behavior
      this.clearLogs();
      this._log.info("Logs reinitialized for client reset");

      // Pretend we've never synced to the server and drop cached data
      Clients.resetSyncID();
      Svc.Prefs.reset("lastSync");
      for each (let cache in [PubKeys, PrivKeys, CryptoMetas, Records])
        cache.clearCache();

      // Have each engine drop any temporary meta data
      for each (let engine in [Clients].concat(Engines.getAll()))
        yield engine.resetClient(self.cb);

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
    }
    this._catchAll(this._notify("reset-client", "", fn)).async(this, onComplete);
  }
};


