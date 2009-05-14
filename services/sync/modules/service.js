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
Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/crypto.js");
Cu.import("resource://weave/base_records/keys.js");
Cu.import("resource://weave/engines.js");
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

Cu.import("resource://weave/engines/bookmarks.js", Weave);
Cu.import("resource://weave/engines/clientData.js", Weave);
Cu.import("resource://weave/engines/cookies.js", Weave);
Cu.import("resource://weave/engines/extensions.js", Weave);
Cu.import("resource://weave/engines/forms.js", Weave);
Cu.import("resource://weave/engines/history.js", Weave);
Cu.import("resource://weave/engines/input.js", Weave);
Cu.import("resource://weave/engines/microformats.js", Weave);
Cu.import("resource://weave/engines/passwords.js", Weave);
Cu.import("resource://weave/engines/plugins.js", Weave);
Cu.import("resource://weave/engines/tabs.js", Weave);
Cu.import("resource://weave/engines/themes.js", Weave);

Utils.lazy(Weave, 'Service', WeaveSvc);

/*
 * Service status query system.  See constants defined in constants.js.
 */

function StatusRecord() {
  this._init();
}
StatusRecord.prototype = {
  _init: function() {
    this.server = [];
    this.sync = null;
    this.engines = {};
  },

  addServerStatus: function(statusCode) {
    this.server.push(statusCode);
  },

  setSyncStatus: function(statusCode) {
    this.sync = statusCode;
  },

  setEngineStatus: function(engineName, statusCode) {
    this.engines[engineName] = statusCode;
  }
};



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

  // WEAVE_STATUS_OK, WEAVE_STATUS_FAILED, or WEAVE_STATUS_PARTIAL
  _weaveStatusCode: null,
  // More detailed status info:
  _detailedStatus: null,

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

  get statusCode() { return this._weaveStatusCode; },
  get detailedStatus() { return this._detailedStatus; },

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

  _setSyncFailure: function WeavSvc__setSyncFailure(code) {
    this._weaveStatusCode = WEAVE_STATUS_FAILED;
    this._detailedStatus.setSyncStatus(code);
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

  onWindowOpened: function WeaveSvc__onWindowOpened() {
  },

  // one-time initialization like setting up observers and the like
  // xxx we might need to split some of this out into something we can call
  //     again when username/server/etc changes
  _onStartup: function WeaveSvc__onStartup() {
    let self = yield;
    this._initLogs();
    this._log.info("Weave " + WEAVE_VERSION + " initializing");
    this._registerEngines();
    this._detailedStatus = new StatusRecord();

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

  /**
   * Register the built-in engines for certain applications
   */
  _registerEngines: function WeaveSvc__registerEngines() {
    let engines = [];
    switch (Svc.AppInfo.name) {
      case "Fennec":
        engines = ["Bookmarks", "History", "Password", "Tab"];
        break;

      case "Firefox":
        engines = ["Bookmarks", "Cookie", "Extension", "Form", "History", "Input",
          "MicroFormat", "Password", "Plugin", "Tab", "Theme"];
        break;

      case "SeaMonkey":
        engines = ["Form", "History", "Password"];
        break;

      case "Thunderbird":
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

  // gets cluster from central LDAP server and returns it, or null on error
  findCluster: function WeaveSvc_findCluster(onComplete, username) {
    let fn = function WeaveSvc__findCluster() {
      let self = yield;
      let ret = null;

      this._log.debug("Finding cluster for user " + username);

      let res = new Resource(this.baseURL + "api/register/chknode/" + username);
      try {
        yield res.get(self.cb);
      } catch (e) { /* we check status below */ }

      if (res.lastChannel.responseStatus == 404) {
        this._log.debug("Using serverURL as data cluster (multi-cluster support disabled)");
        ret = Svc.Prefs.get("serverURL");
      } else if (res.lastChannel.responseStatus == 200) {
        // XXX Bug 480480 Work around the server sending a trailing newline
        ret = 'https://' + res.data.trim() + '/';
      }

      self.done(ret);
    };
    fn.async(this, onComplete);
  },

  // gets cluster from central LDAP server and sets this.clusterURL
  setCluster: function WeaveSvc_setCluster(onComplete, username) {
    let fn = function WeaveSvc__setCluster() {
      let self = yield;
      let ret = false;

      let cluster = yield this.findCluster(self.cb, username);
      if (cluster) {
        this._log.debug("Saving cluster setting");
        this.clusterURL = cluster;
        ret = true;
      } else
        this._log.debug("Error setting cluster for user " + username);

      self.done(ret);
    };
    fn.async(this, onComplete);
  },

  verifyLogin: function WeaveSvc_verifyLogin(onComplete, username, password, isLogin) {
    let fn = function WeaveSvc__verifyLogin() {
      let self = yield;
      this._log.debug("Verifying login for user " + username);

      let url = yield this.findCluster(self.cb, username);
      if (isLogin)
        this.clusterURL = url;

      if (url[url.length-1] != '/')
        url += '/';
      url += "0.3/user/";

      let res = new Resource(url + username);
      res.authenticator = {
        onRequest: function(headers) {
          headers['Authorization'] = 'Basic ' + btoa(username + ':' + password);
          return headers;
        }
      };
      yield res.get(self.cb);

      //JSON.parse(res.data); // throws if not json
      self.done(true);
    };
    this._catchAll(this._notify("verify-login", "", fn)).async(this, onComplete);
  },

  verifyPassphrase: function WeaveSvc_verifyPassphrase(onComplete, username,
                                                       password, passphrase) {
    let fn = function WeaveSvc__verifyPassphrase() {
      let self = yield;

      this._log.debug("Verifying passphrase");

      this.username = username;
      ID.get('WeaveID').setTempPassword(password);

      let id = new Identity('Passphrase Verification', username);
      id.setTempPassword(passphrase);

      let pubkey = yield PubKeys.getDefaultKey(self.cb);
      let privkey = yield PrivKeys.get(self.cb, pubkey.PrivKeyUri);

      // fixme: decrypt something here
    };
    this._catchAll(
      this._localLock(
        this._notify("verify-passphrase", "", fn))).async(this, onComplete);
  },

  login: function WeaveSvc_login(onComplete, username, password, passphrase) {
    let user = username, pass = password, passp = passphrase;

    let fn = function WeaveSvc__login() {
      let self = yield;

      this._loggedIn = false;
      this._detailedStatus = new StatusRecord();

      if (typeof(user) != 'undefined')
        this.username = user;
      if (typeof(pass) != 'undefined')
        ID.get('WeaveID').setTempPassword(pass);
      if (typeof(passp) != 'undefined')
        ID.get('WeaveCryptoID').setTempPassword(passp);

      if (!this.username) {
        this._setSyncFailure(LOGIN_FAILED_NO_USERNAME);
        throw "No username set, login failed";
      }

      if (!this.password) {
        this._setSyncFailure(LOGIN_FAILED_NO_PASSWORD);
        throw "No password given or found in password manager";
      }

      this._log.debug("Logging in user " + this.username);

      if (!(yield this.verifyLogin(self.cb, this.username, this.password, true))) {
        this._setSyncFailure(LOGIN_FAILED_REJECTED);
        throw "Login failed";
      }

      // Try starting the sync timer now that we're logged in
      this._loggedIn = true;
      this._checkSync();

      self.done(true);
    };
    this._catchAll(this._localLock(this._notify("login", "", fn))).
      async(this, onComplete);
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

  createAccount: function WeaveSvc_createAccount(onComplete, username, password, email,
                                                 captchaChallenge, captchaResponse) {
    let fn = function WeaveSvc__createAccount() {
      let self = yield;

      function enc(x) encodeURIComponent(x);
      let message = "uid=" + enc(username) + "&password=" + enc(password) +
        "&mail=" + enc(email) +
        "&recaptcha_challenge_field=" + enc(captchaChallenge) +
        "&recaptcha_response_field=" + enc(captchaResponse);

      let url = Svc.Prefs.get('tmpServerURL') + '0.3/api/register/new';
      let res = new Weave.Resource(url);
      res.authenticator = new Weave.NoOpAuthenticator();
      res.setHeader("Content-Type", "application/x-www-form-urlencoded",
                    "Content-Length", message.length);

      // fixme: Resource throws on error - it really shouldn't :-/
      let resp;
      try {
        resp = yield res.post(self.cb, message);
      } catch (e) {
        this._log.trace("Create account error: " + e);
      }

      if (res.lastChannel.responseStatus != 200 &&
          res.lastChannel.responseStatus != 201)
        this._log.info("Failed to create account. " +
                       "status: " + res.lastChannel.responseStatus + ", " +
                       "response: " + resp);
      else
	this._log.info("Account created: " + resp);

      self.done(res.lastChannel.responseStatus);
    };
    fn.async(this, onComplete);
  },

  // stuff we need to to after login, before we can really do
  // anything (e.g. key setup)
  _remoteSetup: function WeaveSvc__remoteSetup() {
    let self = yield;
    let ret = false; // false to abort sync
    let reset = false;

    this._log.debug("Fetching global metadata record");
    let meta = yield Records.import(self.cb, this.clusterURL + this.username +
                                             "/meta/global");

    let remoteVersion = (meta && meta.payload.storageVersion)?
      meta.payload.storageVersion : "";

    this._log.debug("Min supported storage version is " + MIN_SERVER_STORAGE_VERSION);
    this._log.debug("Remote storage version is " + remoteVersion);

    if (!meta || !meta.payload.storageVersion || !meta.payload.syncID ||
        Svc.Version.compare(MIN_SERVER_STORAGE_VERSION, remoteVersion) > 0) {

      // abort the server wipe if the GET status was anything other than 404 or 200
      let status = Records.lastResource.lastChannel.responseStatus;
      if (status != 200 && status != 404) {
        this._setSyncFailure(METARECORD_DOWNLOAD_FAIL);
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

      if (!this._keyGenEnabled) {
        this._log.info("...and key generation is disabled.  Not wiping. " +
                       "Aborting sync.");
        this._setSyncFailure(DESKTOP_VERSION_OUT_OF_DATE);
        self.done(false);
        return;
      }
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
      this._setSyncFailure(VERSION_OUT_OF_DATE);
      this._log.warn("Server data is of a newer Weave version, this client " +
                     "needs to be upgraded.  Aborting sync.");
      self.done(false);
      return;

    } else if (meta.payload.syncID != Clients.syncID) {
      this._log.warn("Meta.payload.syncID is " + meta.payload.syncID +
                     ", Clients.syncID is " + Clients.syncID);
      yield this.resetClient(self.cb);
      this._log.info("Reset client because of syncID mismatch.");
      Clients.syncID = meta.payload.syncID;
      this._log.info("Reset the client after a server/client sync ID mismatch");
    }

    let needKeys = true;
    let pubkey = yield PubKeys.getDefaultKey(self.cb);
    if (!pubkey)
      this._log.debug("Could not get public key");
    else if (pubkey.keyData == null)
      this._log.debug("Public key has no key data");
    else {
      // make sure we have a matching privkey
      let privkey = yield PrivKeys.get(self.cb, pubkey.privateKeyUri);
      if (!privkey)
        this._log.debug("Could not get private key");
      else if (privkey.keyData == null)
        this._log.debug("Private key has no key data");
      else {
        needKeys = false;
        ret = true;
      }
    }

    if (needKeys) {
      if (PubKeys.lastResource != null && PrivKeys.lastResource != null &&
          PubKeys.lastResource.lastChannel.responseStatus != 404 &&
          PrivKeys.lastResource.lastChannel.responseStatus != 404) {
        this._log.warn("Couldn't download keys from server, aborting sync");
        this._log.debug("PubKey HTTP response status: " +
                        PubKeys.lastResource.lastChannel.responseStatus);
        this._log.debug("PrivKey HTTP response status: " +
                        PrivKeys.lastResource.lastChannel.responseStatus);
        this._setSyncFailure(KEYS_DOWNLOAD_FAIL);
        self.done(false);
        return;
      }

      if (!this._keyGenEnabled) {
        this._log.warn("Couldn't download keys from server, and key generation" +
                       "is disabled.  Aborting sync");
        this._setSyncFailure(NO_KEYS_NO_KEYGEN);
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
          this._setSyncFailure(KEYS_UPLOAD_FAIL);
          this._log.error("Could not upload keys: " + Utils.exceptionStr(e));
          // FIXME no lastRequest anymore
          //this._log.error(keys.pubkey.lastRequest.responseText);
        }
      } else {
        this._setSyncFailure(SETUP_FAILED_NO_PASSPHRASE);
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
      let listener = new Utils.EventListener(Utils.bind2(this,
        function WeaveSvc__checkSyncCallback(timer) {
          if (this.locked)
            this._log.debug("Skipping scheduled sync: already locked for sync");
          else
            this.sync(null, false);
        }));
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
  _sync: function WeaveSvc__sync(fullSync) {
    let self = yield;

    // Use thresholds to determine what to sync only if it's not a full sync
    let useThresh = !fullSync;

    // Make sure we should sync or record why we shouldn't. We always obey the
    // reason if we're using thresholds (not a full sync); otherwise, allow
    // "not scheduled" as future syncs have already been canceled by checkSync.
    let reason = this._checkSync();
    if (reason && (useThresh || reason != kSyncNotScheduled)) {
      // this is a purposeful abort rather than a failure, so don't set
      // WEAVE_STATUS_FAILED; instead, leave it as it was.
      this._detailedStatus.setSyncStatus(reason);
      reason = "Can't sync: " + reason;
      throw reason;
    }

    if (!(yield this._remoteSetup.async(this, self.cb))) {
      throw "aborting sync, remote setup failed";
    }

    this._log.debug("Refreshing client list");
    yield Clients.sync(self.cb);

    // Process the incoming commands if we have any
    if (Clients.getClients()[Clients.clientID].commands) {
      try {
        if (!(yield this.processCommands(self.cb))) {
          this._detailedStatus.setSyncStatus(ABORT_SYNC_COMMAND);
          throw "aborting sync, process commands said so";
        }

        // Repeat remoteSetup in-case the commands forced us to reset
        if (!(yield this._remoteSetup.async(this, self.cb)))
          throw "aborting sync, remote setup failed after processing commands";
      }
      finally {
        // Always immediately push back the local client (now without commands)
        yield Clients.sync(self.cb);
      }
    }

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
        this._weaveStatusCode = WEAVE_STATUS_OK;
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
      this._weaveStatusCode = WEAVE_STATUS_PARTIAL;
      this._detailedStatus.setEngineStatus(engine.name, e);
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
   *
   * @param onComplete
   *        Callback when this method completes
   * @param engines [optional]
   *        Array of engine names to wipe. If not given, all engines are used.
   */
  wipeServer: function WeaveSvc_wipeServer(onComplete, engines) {
    let fn = function WeaveSvc__wipeServer() {
      let self = yield;

      // Grab all the collections for the user
      let userURL = this.clusterURL + this.username + "/";
      let res = new Resource(userURL);
      yield res.get(self.cb);

      // Get the array of collections and delete each one
      let allCollections = JSON.parse(res.data);
      for each (let name in allCollections) {
        try {
          // If we have a list of engines, make sure it's one we want
          if (engines && engines.indexOf(name) == -1)
            continue;

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
   * Wipe all local user data.
   *
   * @param onComplete
   *        Callback when this method completes
   * @param engines [optional]
   *        Array of engine names to wipe. If not given, all engines are used.
   */
  wipeClient: function WeaveSvc_wipeClient(onComplete, engines) {
    let fn = function WeaveSvc__wipeClient() {
      let self = yield;

      // If we don't have any engines, reset the service and wipe all engines
      if (!engines) {
        // Clear out any service data
        yield this.resetService(self.cb);

        engines = [Clients].concat(Engines.getAll());
      }
      // Convert the array of names into engines
      else
        engines = Engines.get(engines);

      // Fully wipe each engine
      for each (let engine in engines)
        yield engine.wipeClient(self.cb);
    };
    this._catchAll(this._notify("wipe-client", "", fn)).async(this, onComplete);
  },

  /**
   * Wipe all remote user data by wiping the server then telling each remote
   * client to wipe itself.
   *
   * @param onComplete
   *        Callback when this method completes
   * @param engines [optional]
   *        Array of engine names to wipe. If not given, all engines are used.
   */
  wipeRemote: function WeaveSvc_wipeRemote(onComplete, engines) {
    let fn = function WeaveSvc__wipeRemote() {
      let self = yield;

      // Clear out any server data
      //yield this.wipeServer(self.cb, engines);

      // Only wipe the engines provided
      if (engines) {
        engines.forEach(function(e) this.prepCommand("wipeEngine", [e]), this);
        return;
      }

      // Tell the remote machines to wipe themselves
      this.prepCommand("wipeAll", []);
    };
    this._catchAll(this._notify("wipe-remote", "", fn)).async(this, onComplete);
  },

  /**
   * Reset local service information like logs, sync times, caches.
   *
   * @param onComplete
   *        Callback when this method completes
   */
  resetService: function WeaveSvc__resetService(onComplete) {
    let fn = function WeaveSvc__resetService() {
      let self = yield;

      // First drop old logs to track client resetting behavior
      this.clearLogs();
      this._log.info("Logs reinitialized for service reset");

      // Pretend we've never synced to the server and drop cached data
      Clients.resetSyncID();
      Svc.Prefs.reset("lastSync");
      for each (let cache in [PubKeys, PrivKeys, CryptoMetas, Records])
        cache.clearCache();
    };
    this._catchAll(this._notify("reset-service", "", fn)).async(this, onComplete);
  },

  /**
   * Reset the client by getting rid of any local server data and client data.
   *
   * @param onComplete
   *        Callback when this method completes
   * @param engines [optional]
   *        Array of engine names to reset. If not given, all engines are used.
   */
  resetClient: function WeaveSvc_resetClient(onComplete, engines) {
    let fn = function WeaveSvc__resetClient() {
      let self = yield;

      // If we don't have any engines, reset everything including the service
      if (!engines) {
        // Clear out any service data
        yield this.resetService(self.cb);

        engines = [Clients].concat(Engines.getAll());
      }
      // Convert the array of names into engines
      else
        engines = Engines.get(engines);

      // Have each engine drop any temporary meta data
      for each (let engine in engines)
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
    };
    this._catchAll(this._notify("reset-client", "", fn)).async(this, onComplete);
  },

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
  ].reduce(function WeaveSvc__commands(commands, entry) {
    commands[entry[0]] = {};
    for (let [i, attr] in Iterator(["args", "desc"]))
      commands[entry[0]][attr] = entry[i + 1];
    return commands;
  }, {}),

  /**
   * Check if the local client has any remote commands and perform them.
   *
   * @param onComplete
   *        Callback when this method completes
   * @return False to abort sync
   */
  processCommands: function WeaveSvc_processCommands(onComplete) {
    let fn = function WeaveSvc__processCommands() {
      let self = yield;
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
            yield this.resetClient(self.cb, engines);
            break;

          case "wipeAll":
            engines = null;
            // Fallthrough
          case "wipeEngine":
            yield this.wipeClient(self.cb, engines);
            break;

          default:
            this._log.debug("Received an unknown command: " + command);
            break;
        }
      }

      self.done(true);
    };
    this._notify("process-commands", "", fn).async(this, onComplete);
  },

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
