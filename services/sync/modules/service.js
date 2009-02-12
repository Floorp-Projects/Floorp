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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/wrap.js");
Cu.import("resource://weave/faultTolerance.js");
Cu.import("resource://weave/auth.js");
Cu.import("resource://weave/resource.js");
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

function WeaveSvc() {}
WeaveSvc.prototype = {

  _notify: Wrap.notify,
  _localLock: Wrap.localLock,
  _catchAll: Wrap.catchAll,
  _osPrefix: "weave:service:",
  _isQuitting: false,
  _loggedIn: false,
  _syncInProgress: false,
  _keyGenEnabled: true,

  __os: null,
  get _os() {
    if (!this.__os)
      this.__os = Cc["@mozilla.org/observer-service;1"]
        .getService(Ci.nsIObserverService);
    return this.__os;
  },

  __dirSvc: null,
  get _dirSvc() {
    if (!this.__dirSvc)
      this.__dirSvc = Cc["@mozilla.org/file/directory_service;1"].
        getService(Ci.nsIProperties);
    return this.__dirSvc;
  },

  __json: null,
  get _json() {
    if (!this.__json)
      this.__json = Cc["@mozilla.org/dom/json;1"].
        createInstance(Ci.nsIJSON);
    return this.__json;
  },

  // object for caching public and private keys
  _keyPair: {},

  // Timer object for automagically syncing
  _scheduleTimer: null,

  get username() {
    return Svc.Prefs.get("username");
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

  get schedule() {
    if (!this.enabled)
      return 0; // manual/off
    return Svc.Prefs.get("schedule");
  },

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

  _setSchedule: function Weave__setSchedule(schedule) {
    switch (this.schedule) {
    case 0:
      this._disableSchedule();
      break;
    case 1:
      this._enableSchedule();
      break;
    default:
      this._log.warn("Invalid Weave scheduler setting: " + schedule);
      break;
    }
  },

  _enableSchedule: function WeaveSvc__enableSchedule() {
    if (this._scheduleTimer) {
      this._scheduleTimer.cancel();
      this._scheduleTimer = null;
    }
    this._scheduleTimer = Cc["@mozilla.org/timer;1"].
      createInstance(Ci.nsITimer);
    let listener = new Utils.EventListener(Utils.bind2(this, this._onSchedule));
    this._scheduleTimer.initWithCallback(listener, SCHEDULED_SYNC_INTERVAL,
                                         this._scheduleTimer.TYPE_REPEATING_SLACK);
    this._log.config("Weave scheduler enabled");
  },

  _disableSchedule: function WeaveSvc__disableSchedule() {
    if (this._scheduleTimer) {
      this._scheduleTimer.cancel();
      this._scheduleTimer = null;
    }
    this._log.config("Weave scheduler disabled");
  },

  _onSchedule: function WeaveSvc__onSchedule() {
    if (this.enabled) {
      if (this.locked) {
        this._log.trace("Skipping scheduled sync; local operation in progress");
      } else {
        this._log.info("Running scheduled sync");
        this._catchAll(
	  this._notify("sync", "",
		       this._localLock(this._sync))).async(this);
      }
    }
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

    let ua = Cc["@mozilla.org/network/protocol;1?name=http"].
      getService(Ci.nsIHttpProtocolHandler).userAgent;
    this._log.info(ua);

    if (!this._checkCrypto()) {
      this.enabled = false;
      this._log.error("Could not load the Weave crypto component. Disabling " +
		      "Weave, since it will not work correctly.");
    }

    Utils.prefs.addObserver("", this, false);
    this._os.addObserver(this, "quit-application", true);
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
	  yield this.sync(self.cb);
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

    let brief = this._dirSvc.get("ProfD", Ci.nsIFile);
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
          case "enabled": // this works because this.schedule is 0 when disabled
          case "schedule":
            this._setSchedule(this.schedule);
            break;
        }
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
        this.clusterURL = 'https://' + res.data + '/';
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

      try {
	if (typeof(user) != 'undefined')
          this.username = user;
	if (typeof(pass) != 'undefined')
          ID.get('WeaveID').setTempPassword(pass);
	if (typeof(passp) != 'undefined')
          ID.get('WeaveCryptoID').setTempPassword(passp);

	if (!this.username)
          throw "No username set, login failed";
	if (!this.password)
          throw "No password given or found in password manager";

	this._log.debug("Logging in user " + this.username);

	if (!(yield this.verifyLogin(self.cb, this.username, this.password, true)))
	  throw "Login failed";
	this._loggedIn = true;
	this._setSchedule(this.schedule);
	self.done(true);

      } catch (e) {
	this._disableSchedule();
	throw e;
      }
    };
    this._catchAll(
      this._localLock(
	this._notify("login", "", fn))).async(this, onComplete);
  },

  logout: function WeaveSvc_logout() {
    this._log.info("Logging out");
    this._disableSchedule();
    this._loggedIn = false;
    this._keyPair = {};
    ID.get('WeaveID').setTempPassword(null); // clear cached password
    ID.get('WeaveCryptoID').setTempPassword(null); // and passphrase
    this._os.notifyObservers(null, "weave:service:logout:success", "");
  },

  serverWipe: function WeaveSvc_serverWipe(onComplete) {
    let cb = function WeaveSvc_serverWipeCb() {
      let self = yield;
      this._log.error("Server wipe not supported");
      this.logout();
    };
    this._catchAll(
      this._notify("server-wipe", "", this._localLock(cb))).async(this, onComplete);
  },

  // stuff we need to to after login, before we can really do
  // anything (e.g. key setup)
  _remoteSetup: function WeaveSvc__remoteSetup() {
    let self = yield;
    let ret = false; // false to abort sync

    // FIXME: too easy to generate a keypair.  we should be absolutely certain
    // that the keys are not there (404) and that it's not some other
    // transient network problem instead
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

    if (needKeys && this._keyGenEnabled) {
      let pass = yield ID.get('WeaveCryptoID').getPassword(self.cb);
      if (pass) {
        let keys = PubKeys.createKeypair(pass, PubKeys.defaultKeyUri,
                                         PrivKeys.defaultKeyUri);
        try {
	  yield PubKeys.uploadKeypair(self.cb, keys);
          ret = true;
        } catch (e) {
          this._log.error("Could not upload keys: " + Utils.exceptionStr(e));
	  // FIXME no lastRequest anymore
          //this._log.error(keys.pubkey.lastRequest.responseText);
        }
      } else {
        this._log.warn("Could not get encryption passphrase");
      }
    }

    if (needKeys && !this._keyGenEnabled) {
      this._log.warn("Can't get keys from server and local keygen disabled.");
    }

    self.done(ret);
  },

  // These are per-engine

  _sync: function WeaveSvc__sync() {
    let self = yield;

    if (!this.enabled)
      return;

    if (!this._loggedIn) {
      this._disableSchedule();
      throw "aborting sync, not logged in";
    }

    if (!(yield this._remoteSetup.async(this, self.cb))) {
      throw "aborting sync, remote setup failed";
    }

    this._log.debug("Refreshing client list");
    yield Clients.sync(self.cb);

    try {
      let engines = Engines.getAll();
      for each (let engine in engines) {
        if (!engine.enabled)
          continue;

	if (!(yield this._syncEngine.async(this, self.cb, engine))) {
	  this._log.info("Aborting sync");
	  break;
	}
      }

      if (this._syncError)
        this._log.warn("Some engines did not sync correctly");
      else
        this._log.info("Sync completed successfully");

    } finally {
      this.cancelRequested = false;
      this._syncError = false;
    }
  },
  sync: function WeaveSvc_sync(onComplete) {
    this._catchAll(
      this._notify("sync", "",
		   this._localLock(this._sync))).async(this, onComplete);
  },

  // The values that engine scores must meet or exceed before we sync them
  // as needed.  These are engine-specific, as different kinds of data change
  // at different rates, so we store them in a hash indexed by engine name.
  _syncThresholds: {},

  _syncAsNeeded: function WeaveSvc__syncAsNeeded() {
    let self = yield;

    if (!this.enabled)
      return;

    try {

      if (!this._loggedIn) {
	this._disableSchedule();
	throw "aborting sync, not logged in";
      }

      let engines = Engines.getAll();
      for each (let engine in engines) {
        if (!engine.enabled)
          continue;

        if (!(engine.name in this._syncThresholds))
          this._syncThresholds[engine.name] = INITIAL_THRESHOLD;

        let score = engine.score;
        if (score >= this._syncThresholds[engine.name]) {
          this._log.debug(engine.name + " score " + score +
                          " reaches threshold " +
                          this._syncThresholds[engine.name] + "; syncing");
          if (!(yield this._syncEngine.async(this, self.cb, engine))) {
	    this._log.info("Aborting sync");
	    break;
	  }

          // Reset the engine's threshold to the initial value.
          // Note: we do this after syncing the engine so that we'll try again
          // next time around if syncing fails for some reason.  The upside
          // of this approach is that we'll sync again as soon as possible;
          // but the downside is that if the error is caused by the server being
          // overloaded, we'll contribute to the problem by trying to sync
          // repeatedly at the maximum rate.
          this._syncThresholds[engine.name] = INITIAL_THRESHOLD;
        }
        else {
          this._log.debug(engine.name + " score " + score +
                          " does not reach threshold " +
                          this._syncThresholds[engine.name] + "; not syncing");

          // Decrement the threshold by the standard amount, and if this puts it
          // at or below zero, then set it to 1, the lowest possible value, where
          // it'll stay until there's something to sync (whereupon we'll sync it,
          // reset the threshold to the initial value, and start over again).
          this._syncThresholds[engine.name] -= THRESHOLD_DECREMENT_STEP;
          if (this._syncThresholds[engine.name] <= 0)
            this._syncThresholds[engine.name] = 1;
        }
      }

      if (this._syncError)
        this._log.warn("Some engines did not sync correctly");
      else
        this._log.info("Sync completed successfully");

    } finally {
      this._cancelRequested = false;
      this._syncError = false;
    }
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

  _wipeServer: function WeaveSvc__wipeServer() {
    let self = yield;

    let engines = Engines.getAll();
    for (let i = 0; i < engines.length; i++) {
      if (!engines[i].enabled)
        continue;
      engines[i].wipeServer(self.cb);
      yield;
    }
  },
  wipeServer: function WeaveSvc_wipeServer(onComplete) {
    this._catchAll(
      this._notify("wipe-server", "",
                   this._localLock(this._wipeServer))).async(this, onComplete);
  }
};
