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
Cu.import("resource://weave/crypto.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/oauth.js");
Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/identity.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/clientData.js");
Cu.import("resource://weave/engines/cookies.js");
Cu.import("resource://weave/engines/bookmarks.js");
Cu.import("resource://weave/engines/history.js");
Cu.import("resource://weave/engines/passwords.js");
Cu.import("resource://weave/engines/forms.js");
Cu.import("resource://weave/engines/tabs.js");

Function.prototype.async = Async.sugar;

// for export
let Weave = {};
Cu.import("resource://weave/constants.js", Weave);
Cu.import("resource://weave/util.js", Weave);
Cu.import("resource://weave/async.js", Weave);
Cu.import("resource://weave/faultTolerance.js", Weave);
Cu.import("resource://weave/crypto.js", Weave);
Cu.import("resource://weave/notifications.js", Weave);
Cu.import("resource://weave/identity.js", Weave);
Cu.import("resource://weave/clientData.js", Weave);
Cu.import("resource://weave/dav.js", Weave);
Cu.import("resource://weave/stores.js", Weave);
Cu.import("resource://weave/syncCores.js", Weave);
Cu.import("resource://weave/engines.js", Weave);
Cu.import("resource://weave/oauth.js", Weave);
Cu.import("resource://weave/service.js", Weave);
Cu.import("resource://weave/engines/cookies.js", Weave);
Cu.import("resource://weave/engines/passwords.js", Weave);
Cu.import("resource://weave/engines/bookmarks.js", Weave);
Cu.import("resource://weave/engines/history.js", Weave);
Cu.import("resource://weave/engines/forms.js", Weave);
Cu.import("resource://weave/engines/tabs.js", Weave);

Utils.lazy(Weave, 'Service', WeaveSvc);

/*
 * Service singleton
 * Main entry point into Weave's sync framework
 */

function WeaveSvc() {
  this._startupFinished = false;
  this._initLogs();
  this._log.info("Weave Sync Service Initializing");

  // Create Weave identities (for logging in, and for encryption)
  ID.set('WeaveID', new Identity('Mozilla Services Password', this.username));
  ID.set('WeaveCryptoID',
         new Identity('Mozilla Services Encryption Passphrase', this.username));

  // Set up aliases for other modules to use our IDs
  ID.setAlias('WeaveID', 'DAV:default');
  ID.setAlias('WeaveCryptoID', 'Engine:PBE:default');

  // Other misc startup
  Utils.prefs.addObserver("", this, false);
  this._os.addObserver(this, "quit-application", true);
  FaultTolerance.Service; // initialize FT service

  if (!this.enabled)
    this._log.info("Weave Sync disabled");
}
WeaveSvc.prototype = {

  _notify: Wrap.notify,
  _lock: Wrap.lock,
  _localLock: Wrap.localLock,
  _catchAll: Wrap.catchAll,
  _osPrefix: "weave:service:",
  _cancelRequested: false,
  _isQuitting: false,
  _loggedIn: false,
  _syncInProgress: false,

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
    return Utils.prefs.getCharPref("username");
  },
  set username(value) {
    if (value)
      Utils.prefs.setCharPref("username", value);
    else
      Utils.prefs.clearUserPref("username");

    // fixme - need to loop over all Identity objects - needs some rethinking...
    ID.get('WeaveID').username = value;
    ID.get('WeaveCryptoID').username = value;
  },

  get password() { return ID.get('WeaveID').password; },
  set password(value) { ID.get('WeaveID').password = value; },

  get passphrase() { return ID.get('WeaveCryptoID').password; },
  set passphrase(value) { ID.get('WeaveCryptoID').password = value; },

  get userPath() { return ID.get('WeaveID').username; },

  get isLoggedIn() this._loggedIn,
  get isInitialized() this._initialized,

  get isQuitting() this._isQuitting,
  set isQuitting(value) { this._isQuitting = value; },

  get cancelRequested() this._cancelRequested,
  set cancelRequested(value) { this._cancelRequested = value; },

  get enabled() Utils.prefs.getBoolPref("enabled"),

  get schedule() {
    if (!this.enabled)
      return 0; // manual/off
    return Utils.prefs.getIntPref("schedule");
  },

  onWindowOpened: function Weave__onWindowOpened() {
    if (!this._startupFinished) {
      this._startupFinished = true;
      if (Utils.prefs.getBoolPref("autoconnect") &&
          this.username && this.username != 'nobody')
        this._initialLoginAndSync.async(this);
    }
  },

  _initialLoginAndSync: function Weave__initialLoginAndSync() {
    let self = yield;
    yield this.loginAndInit(self.cb); // will throw if login fails
    yield this.sync(self.cb);
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
      if (DAV.locked) {
        this._log.info("Skipping scheduled sync; local operation in progress")
      } else {
        this._log.info("Running scheduled sync");
        this._notify("sync", "",
                     this._catchAll(this._lock(this._syncAsNeeded))).async(this);
      }
    }
  },

  _initLogs: function WeaveSvc__initLogs() {
    this._log = Log4Moz.Service.getLogger("Service.Main");
    this._log.level =
      Log4Moz.Level[Utils.prefs.getCharPref("log.logger.service.main")];

    let formatter = new Log4Moz.BasicFormatter();
    let root = Log4Moz.Service.rootLogger;
    root.level = Log4Moz.Level[Utils.prefs.getCharPref("log.rootLogger")];

    let capp = new Log4Moz.ConsoleAppender(formatter);
    capp.level = Log4Moz.Level[Utils.prefs.getCharPref("log.appender.console")];
    root.addAppender(capp);

    let dapp = new Log4Moz.DumpAppender(formatter);
    dapp.level = Log4Moz.Level[Utils.prefs.getCharPref("log.appender.dump")];
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
    this._briefApp.level = Log4Moz.Level[Utils.prefs.getCharPref("log.appender.briefLog")];
    root.addAppender(this._briefApp);
    this._debugApp = new Log4Moz.RotatingFileAppender(verbose, formatter);
    this._debugApp.level = Log4Moz.Level[Utils.prefs.getCharPref("log.appender.debugLog")];
    root.addAppender(this._debugApp);
  },

  clearLogs: function WeaveSvc_clearLogs() {
    this._briefApp.clear();
    this._debugApp.clear();
  },

  _uploadVersion: function WeaveSvc__uploadVersion() {
    let self = yield;

    DAV.MKCOL("meta", self.cb);
    let ret = yield;
    if (!ret)
      throw "Could not create meta information directory";

    DAV.PUT("meta/version", STORAGE_FORMAT_VERSION, self.cb);
    ret = yield;
    Utils.ensureStatus(ret.status, "Could not upload server version file");
  },

  // force a server wipe when the version is lower than ours (or there is none)
  _versionCheck: function WeaveSvc__versionCheck() {
    let self = yield;

    let ret = yield DAV.GET("meta/version", self.cb);

    if (ret.status == 404) {
      this._log.info("Could not get version file.  Wiping server data.");
      yield this._serverWipe.async(this, self.cb);
      yield this._uploadVersion.async(this, self.cb);

    } else if (!Utils.checkStatus(ret.status)) {
      this._log.debug("Could not get version file from server");
      self.done(false);
      return;

    } else if (ret.responseText < STORAGE_FORMAT_VERSION) {
      this._log.info("Server version too low.  Wiping server data.");
      yield this._serverWipe.async(this, self.cb);
      yield this._uploadVersion.async(this, self.cb);

    } else if (ret.responseText > STORAGE_FORMAT_VERSION) {
      // XXX should we do something here?
      throw "Server version higher than this client understands.  Aborting."
    }
    self.done(true);
  },

  _checkUserDir: function WeaveSvc__checkUserDir() {
    let self = yield;
    let prefix = DAV.defaultPrefix;

    this._log.trace("Checking user directory exists");

    try {
      DAV.defaultPrefix = '';
      DAV.MKCOL("user/" + this.userPath, self.cb);
      let ret = yield;
      if (!ret)
        throw "Could not create user directory";
    }
    catch (e) { throw e; }
    finally { DAV.defaultPrefix = prefix; }
  },

  // Retrieves the keypair for the given Identity object and inserts
  // its information into the Identity object.  If no Identity object
  // is supplied, the 'WeaveCryptoID' identity is used.
  //
  // This coroutine assumes the DAV singleton's prefix is set to the
  // proper user-specific directory.
  //
  // If the password associated with the Identity cannot be used to
  // decrypt the private key, an exception is raised.
  _getKeypair : function WeaveSvc__getKeypair(id) {
    let self = yield;

    if ("none" == Utils.prefs.getCharPref("encryption"))
      return;

    if (typeof(id) == "undefined")
      id = ID.get('WeaveCryptoID');

    this._log.trace("Retrieving keypair from server");

    if (this._keyPair['private'] && this._keyPair['public'])
      this._log.debug("Using cached keypair");
    else {
      this._log.debug("Fetching keypair from server");

      let privkeyResp = yield DAV.GET("private/privkey", self.cb);
      Utils.ensureStatus(privkeyResp.status, "Could not download private key");

      let pubkeyResp = yield DAV.GET("public/pubkey", self.cb);
      Utils.ensureStatus(pubkeyResp.status, "Could not download public key");

      this._keyPair['private'] = this._json.decode(privkeyResp.responseText);
      this._keyPair['public'] = this._json.decode(pubkeyResp.responseText);
    }

    let privkeyData = this._keyPair['private']
    let pubkeyData  = this._keyPair['public'];

    if (!privkeyData || !pubkeyData)
      throw "Bad keypair JSON";
    if (privkeyData.version != 1 || pubkeyData.version != 1)
      throw "Unexpected keypair data version";
    if (privkeyData.algorithm != "RSA" || pubkeyData.algorithm != "RSA")
      throw "Only RSA keys currently supported";
    if (!privkeyData.privkey)
      throw "Private key does not contain private key data!";
    if (!pubkeyData.pubkey)
      throw "Public key does not contain public key data!";


    id.keypairAlg     = privkeyData.algorithm;
    id.privkey        = privkeyData.privkey;
    id.privkeyWrapIV  = privkeyData.privkeyIV;
    id.passphraseSalt = privkeyData.privkeySalt;

    id.pubkey = pubkeyData.pubkey;

    let isValid = yield Crypto.isPassphraseValid.async(Crypto, self.cb, id);
    if (!isValid)
      throw new Error("Passphrase is not valid.");
  },

  _generateKeys: function WeaveSvc__generateKeys() {
    let self = yield;

    this._log.debug("Generating new RSA key");

    // RSAkeygen will set the needed |id| properties.
    let id = ID.get('WeaveCryptoID');
    Crypto.RSAkeygen.async(Crypto, self.cb, id);
    yield;

    DAV.MKCOL("private/", self.cb);
    let ret = yield;
    if (!ret)
      throw "Could not create private key directory";

    DAV.MKCOL("public/", self.cb);
    ret = yield;
    if (!ret)
      throw "Could not create public key directory";

    let privkeyData = { version     : 1,
                        algorithm   : id.keypairAlg,
                        privkey     : id.privkey,
                        privkeyIV   : id.privkeyWrapIV,
                        privkeySalt : id.passphraseSalt
                      };
    let data = this._json.encode(privkeyData);

    DAV.PUT("private/privkey", data, self.cb);
    ret = yield;
    Utils.ensureStatus(ret.status, "Could not upload private key");


    let pubkeyData = { version   : 1,
                       algorithm : id.keypairAlg,
                       pubkey    : id.pubkey
                     };
    data = this._json.encode(pubkeyData);

    DAV.PUT("public/pubkey", data, self.cb);
    ret = yield;
    Utils.ensureStatus(ret.status, "Could not upload public key");
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
    if (!this.enabled || !this._loggedIn)
      return;

    // Don't quit on exit if this is a forced restart due to application update
    // or extension install.
    var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                                        getService(Ci.nsIPrefBranch);
    // non browser apps may throw
    try {
      if(prefBranch.getBoolPref("browser.sessionstore.resume_session_once"))
        return;
    } catch (ex) {}

    this.isQuitting = true;

    let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
             getService(Ci.nsIWindowWatcher);

    // This window has to be modal to prevent the application from quitting
    // until the sync finishes and the window closes.
    let window = ww.openWindow(null,
                               "chrome://weave/content/status.xul",
                               "Weave:Status",
                               "chrome,centerscreen,modal,close=0",
                               null);
  },

  // These are global (for all engines)

  verifyPassphrase: function WeaveSvc_verifyPassphrase(onComplete, username,
                                                       password, passphrase) {
    this._localLock(this._notify("verify-passphrase", "", this._verifyPassphrase,
                                 username, password, passphrase)).
      async(this, onComplete);
  },

  _verifyPassphrase: function WeaveSvc__verifyPassphrase(username, password,
                                                         passphrase) {
    let self = yield;

    this._log.debug("Verifying passphrase");

    this.username = username;
    ID.get('WeaveID').setTempPassword(password);
    
    let id = new Identity('Passphrase Verification', username);
    id.setTempPassword(passphrase);

    // FIXME: abstract common bits and share code with getKeypair()

    // XXX: We're not checking the version of the server here, in part because
    // we have no idea what to do if the version is different than we expect
    // it to be.
    // XXX check it and ... throw?

    this.username = username;
    ID.get('WeaveID').setTempPassword(password);

    let privkeyResp = yield DAV.GET("private/privkey", self.cb);
    let pubkeyResp = yield DAV.GET("public/pubkey", self.cb);

    // FIXME: this will cause 404's to turn into a 'success'
    // while technically incorrect, this function currently only gets
    // called from the wizard, which will do a loginAndInit, which
    // will create the keys if necessary later
    if (privkeyResp.status == 404 || pubkeyResp.status == 404)
      return;

    Utils.ensureStatus(privkeyResp.status, "Could not download private key");
    Utils.ensureStatus(privkeyResp.status, "Could not download public key");

    let privkey = this._json.decode(privkeyResp.responseText);
    let pubkey = this._json.decode(pubkeyResp.responseText);

    if (!privkey || !pubkey)
      throw "Bad keypair JSON";
    if (privkey.version != 1 || pubkey.version != 1)
      throw "Unexpected keypair data version";
    if (privkey.algorithm != "RSA" || pubkey.algorithm != "RSA")
      throw "Only RSA keys currently supported";
    if (!privkey.privkey)
      throw "Private key does not contain private key data!";
    if (!pubkey.pubkey)
      throw "Public key does not contain public key data!";

    id.keypairAlg = privkey.algorithm;
    id.privkey = privkey.privkey;
    id.privkeyWrapIV = privkey.privkeyIV;
    id.passphraseSalt = privkey.privkeySalt;
    id.pubkey = pubkey.pubkey;

    if (!(yield Crypto.isPassphraseValid.async(Crypto, self.cb, id)))
      throw new Error("Passphrase is not valid.");
  },

  verifyLogin: function WeaveSvc_verifyLogin(onComplete, username, password) {
    this._localLock(this._notify("verify-login", "", this._verifyLogin,
                                 username, password)).async(this, onComplete);
  },

  _verifyLogin: function WeaveSvc__verifyLogin(username, password) {
    let self = yield;

    this._log.debug("Verifying login for user " + username);

    DAV.baseURL = Utils.prefs.getCharPref("serverURL");
    DAV.defaultPrefix = "user/" + username;

    this._log.config("Using server URL: " + DAV.baseURL + DAV.defaultPrefix);

    let status = yield DAV.checkLogin.async(DAV, self.cb, username, password);
    Utils.ensureStatus(status, "Login verification failed");
  },

  loginAndInit: function WeaveSvc_loginAndInit(onComplete,
                                               username, password, passphrase) {
    this._localLock(this._notify("login", "", this._loginAndInit,
                                 username, password, passphrase)).
      async(this, onComplete);
  },
  _loginAndInit: function WeaveSvc__loginAndInit(username, password, passphrase) {
    let self = yield;
    try {
      yield this._login.async(this, self.cb, username, password, passphrase);
    } catch (e) {
      // we might need to initialize before login will work (e.g. to create the
      // user directory), so do this and try again...
      yield this._initialize.async(this, self.cb);
      yield this._login.async(this, self.cb, username, password, passphrase);
    }
    yield this._initialize.async(this, self.cb);
  },

  login: function WeaveSvc_login(onComplete, username, password, passphrase) {
    this._localLock(
      this._notify("login", "", this._login,
                   username, password, passphrase)).async(this, onComplete);
  },
  _login: function WeaveSvc__login(username, password, passphrase) {
    let self = yield;

    this._log.debug("Logging in user " + this.username);

    if (typeof(username) != 'undefined')
      this.username = username;
    if (typeof(password) != 'undefined')
      ID.get('WeaveID').setTempPassword(password);
    if (typeof(passphrase) != 'undefined')
      ID.get('WeaveCryptoID').setTempPassword(passphrase);

    if (!this.username)
      throw "No username set, login failed";
    if (!this.password)
      throw "No password given or found in password manager";

    yield this._verifyLogin.async(this, self.cb, this.username, this.password);

    this._loggedIn = true;
    self.done(true);
  },

  initialize: function WeaveSvc_initialize() {
    this._localLock(
      this._notify("initialize", "", this._initialize)).async(this, onComplete);
  },

  _initialize: function WeaveSvc__initialize() {
    let self = yield;

    this._log.info("Making sure server is initialized...");

    // create user directory (for self-hosted webdav shares) if it doesn't exist
    let status = yield DAV.checkLogin.async(DAV, self.cb,
                                            this.username, this.password);
    if (status == 404) {
      yield this._checkUserDir.async(this, self.cb);
      status = yield DAV.checkLogin.async(DAV, self.cb,
                                          this.username, this.password);
    }
    Utils.ensureStatus(status, "Cannot initialize server");

    // wipe the server if it has any old cruft
    yield this._versionCheck.async(this, self.cb);

    // get info on the clients that are syncing with this store
    yield ClientData.refresh(self.cb);

    // cache keys, create public/private keypair if it doesn't exist
    this._log.debug("Caching keys");
    let privkeyResp = yield DAV.GET("private/privkey", self.cb);
    let pubkeyResp = yield DAV.GET("public/pubkey", self.cb);

    if (privkeyResp.status == 404 || pubkeyResp.status == 404) {
      yield this._generateKeys.async(this, self.cb);
      privkeyResp = yield DAV.GET("private/privkey", self.cb);
      pubkeyResp = yield DAV.GET("public/pubkey", self.cb);
    }

    Utils.ensureStatus(privkeyResp.status, "Cannot initialize privkey");
    Utils.ensureStatus(pubkeyResp.status, "Cannot initialize pubkey");

    this._keyPair['private'] = this._json.decode(privkeyResp.responseText);
    this._keyPair['public'] = this._json.decode(pubkeyResp.responseText);

    yield this._getKeypair.async(this, self.cb); // makes sure passphrase works

    this._setSchedule(this.schedule);

    this._initialized = true;
    self.done(true);
  },

  logout: function WeaveSvc_logout() {
    this._log.info("Logging out");
    this._disableSchedule();
    this._loggedIn = false;
    this._initialized = false;
    this._keyPair = {};
    ID.get('WeaveID').setTempPassword(null); // clear cached password
    ID.get('WeaveCryptoID').setTempPassword(null); // and passphrase
    this._os.notifyObservers(null, "weave:service:logout:success", "");
  },

  resetLock: function WeaveSvc_resetLock(onComplete) {
    this._notify("reset-server-lock", "",
                 this._resetLock).async(this, onComplete);
  },
  _resetLock: function WeaveSvc__resetLock() {
    let self = yield;
    DAV.forceUnlock.async(DAV, self.cb);
    yield;
  },

  serverWipe: function WeaveSvc_serverWipe(onComplete) {
    let cb = function WeaveSvc_serverWipeCb() {
      let self = yield;
      this._serverWipe.async(this, self.cb);
      yield;
      this.logout();
      self.done();
    };
    this._notify("server-wipe", "", this._lock(cb)).async(this, onComplete);
  },
  _serverWipe: function WeaveSvc__serverWipe() {
    let self = yield;

    this._keyPair = {};
    DAV.listFiles.async(DAV, self.cb);
    let names = yield;

    for (let i = 0; i < names.length; i++) {
      if (names[i].match(/\.htaccess$/))
        continue;
      DAV.DELETE(names[i], self.cb);
      let resp = yield;
      this._log.debug(resp.status);
    }
  },

  // These are per-engine

  sync: function WeaveSvc_sync(onComplete) {
    this._notify("sync", "",
                 this._catchAll(this._lock(this._sync))).async(this, onComplete);
  },

  _sync: function WeaveSvc__sync() {
    let self = yield;

    yield ClientData.refresh(self.cb);

    let engines = Engines.getAll();
    for (let i = 0; i < engines.length; i++) {
      if (this.cancelRequested)
        continue;

      if (!engines[i].enabled)
        continue;

      yield this._notify(engines[i].name + "-engine:sync", "",
                         this._syncEngine, engines[i]).async(this, self.cb);
    }

    if (this._syncError) {
      this._syncError = false;
      throw "Some engines did not sync correctly";
    }
  },

  // The values that engine scores must meet or exceed before we sync them
  // as needed.  These are engine-specific, as different kinds of data change
  // at different rates, so we store them in a hash indexed by engine name.
  _syncThresholds: {},

  _syncAsNeeded: function WeaveSvc__syncAsNeeded() {
    let self = yield;

    let engines = Engines.getAll();
    for each (let engine in engines) {
      if (!engine.enabled)
        continue;

      if (!(engine.name in this._syncThresholds))
        this._syncThresholds[engine.name] = INITIAL_THRESHOLD;

      let score = engine._tracker.score;
      if (score >= this._syncThresholds[engine.name]) {
        this._log.debug(engine.name + " score " + score +
                        " reaches threshold " +
                        this._syncThresholds[engine.name] + "; syncing");
        this._notify(engine.name + "-engine:sync", "",
                     this._syncEngine, engine).async(this, self.cb);
        yield;

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

    if (this._syncError) {
      this._syncError = false;
      throw "Some engines did not sync correctly";
    }
  },

  _syncEngine: function WeaveSvc__syncEngine(engine) {
    let self = yield;
    try {
      yield engine.sync(self.cb);
      engine._tracker.resetScore();
    } catch(e) {
      // FIXME: FT module is not printing out exceptions - it should be
      this._log.warn("Engine exception: " + e);
      let ok = FaultTolerance.Service.onException(e);
      if (!ok)
        this._syncError = true;
    }
  },

  resetServer: function WeaveSvc_resetServer(onComplete) {
    this._notify("reset-server", "",
                 this._lock(this._resetServer)).async(this, onComplete);
  },
  _resetServer: function WeaveSvc__resetServer() {
    let self = yield;

    let engines = Engines.getAll();
    for (let i = 0; i < engines.length; i++) {
      if (!engines[i].enabled)
        continue;
      engines[i].resetServer(self.cb);
      yield;
    }
  },

  resetClient: function WeaveSvc_resetClient(onComplete) {
    this._localLock(this._notify("reset-client", "",
                                 this._resetClient)).async(this, onComplete);
  },
  _resetClient: function WeaveSvc__resetClient() {
    let self = yield;
    let engines = Engines.getAll();
    for (let i = 0; i < engines.length; i++) {
      if (!engines[i].enabled)
        continue;
      engines[i].resetClient(self.cb);
      yield;
    }
  },

  shareData: function WeaveSvc_shareData(dataType,
					 isShareEnabled,
                                         onComplete,
                                         guid,
                                         username) {
    /* Shares data of the specified datatype (which must correspond to
       one of the registered engines) with the user specified by username.
       The data node indicated by guid will be shared, along with all its
       children, if it has any.  onComplete is a function that will be called
       when sharing is done; it takes an argument that will be true or false
       to indicate whether sharing succeeded or failed.
       Implementation, as well as the interpretation of what 'guid' means,
       is left up to the engine for the specific dataType.

       isShareEnabled: true to start sharing, false to stop sharing.*/

    let messageName = "share-" + dataType;
    /* so for instance, if dataType is "bookmarks" then a message
     "share-bookmarks" will be sent out to any observers who are listening
     for it.  As far as I know, there aren't currently any listeners for
     "share-bookmarks" but we'll send it out just in case. */

    let self = this;
    let saved_dataType = dataType;
    let saved_onComplete = onComplete;
    let saved_guid = guid;
    let saved_username = username;
    let saved_isShareEnabled = isShareEnabled;
    let successMsg = "weave:service:global:success";
    let errorMsg = "weave:service:global:error";
    let os = Cc["@mozilla.org/observer-service;1"].
                      getService(Ci.nsIObserverService);

    let observer = {
      observe: function(subject, topic, data) {
	if (!Weave.DAV.locked) {
          self._notify(messageName, "", self._lock(self._shareData,
					saved_dataType,
					saved_isShareEnabled,
                                        saved_guid,
                                    saved_username)).async(self,
							   saved_onComplete);
	  os.removeObserver(observer, successMsg);
	  os.removeObserver(observer, errorMsg);
	}
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver])
    };

    if (Weave.DAV.locked) {
      /* then we have to wait until it's not locked. */
      dump( "DAV is locked, gonna set up observer to do it later.\n");
      os.addObserver( observer, successMsg, true );
      os.addObserver( observer, errorMsg, true );
    } else {
      // Just do it right now!
      dump( "DAV not locked, doing it now.\n");
      observer.observe();
    }
  },

  _shareData: function WeaveSvc__shareData(dataType,
					   isShareEnabled,
                                           guid,
                                           username) {
    let self = yield;
    let ret;
    if (Engines.get(dataType).enabled) {
      if (isShareEnabled) {
        Engines.get(dataType).share(self.cb, guid, username);
      } else {
        Engines.get(dataType).stopSharing(self.cb, guid, username);
      }
      ret = yield;
    } else {
      this._log.warn( "Can't share disabled data type: " + dataType );
      ret = false;
    }
    self.done(ret);
  }
};
