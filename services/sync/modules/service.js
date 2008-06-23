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
const SCHEDULED_SYNC_INTERVAL = 60 * 1000; // one minute

// INITIAL_THRESHOLD represents the value an engine's score has to exceed
// in order for us to sync it the first time we start up (and the first time
// we do a sync check after having synced the engine or reset the threshold).
const INITIAL_THRESHOLD = 100;

// THRESHOLD_DECREMENT_STEP is the amount by which we decrement an engine's
// threshold each time we do a sync check and don't sync that engine.
const THRESHOLD_DECREMENT_STEP = 5;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/wrap.js");
Cu.import("resource://weave/crypto.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/identity.js");
Cu.import("resource://weave/async.js");
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
Cu.import("resource://weave/crypto.js", Weave);
Cu.import("resource://weave/identity.js", Weave);
Cu.import("resource://weave/dav.js", Weave);
Cu.import("resource://weave/stores.js", Weave);
Cu.import("resource://weave/syncCores.js", Weave);
Cu.import("resource://weave/engines.js", Weave);
Cu.import("resource://weave/service.js", Weave);
Utils.lazy(Weave, 'Service', WeaveSvc);

/*
 * Service singleton
 * Main entry point into Weave's sync framework
 */

function WeaveSvc(engines) {
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

  if (typeof engines == "undefined")
    engines = [
      new BookmarksEngine(),
      new HistoryEngine(),
      new CookieEngine(),
      new PasswordEngine(),
      new FormEngine(),
      new TabEngine()
    ];

  // Register engines
  for (let i = 0; i < engines.length; i++)
    Engines.register(engines[i]);

  // Other misc startup
  Utils.prefs.addObserver("", this, false);
  this._os.addObserver(this, "quit-application", true);

  if (!this.enabled) {
    this._log.info("Weave Sync disabled");
    return;
  }
}
WeaveSvc.prototype = {

  _notify: Wrap.notify,
  _lock: Wrap.lock,
  _localLock: Wrap.localLock,
  _osPrefix: "weave:service:",
  _loggedIn: false,

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

  get currentUser() {
    if (this._loggedIn)
      return this.username;
    return null;
  },

  get enabled() {
    return Utils.prefs.getBoolPref("enabled");
  },

  get schedule() {
    if (!this.enabled)
      return 0; // manual/off
    return Utils.prefs.getIntPref("schedule");
  },

  onWindowOpened: function Weave__onWindowOpened() {
    if (!this._startupFinished) {
      if (Utils.prefs.getBoolPref("autoconnect") &&
          this.username && this.username != 'nobody') {
        // Login, then sync.
        let self = this;
        this.login(function() { self.sync(); });
      }
      this._startupFinished = true;
    }
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
      this._log.info("Invalid Weave scheduler setting: " + schedule);
      break;
    }
  },

  _enableSchedule: function WeaveSync__enableSchedule() {
    if (this._scheduleTimer) {
      this._scheduleTimer.cancel();
      this._scheduleTimer = null;
    }
    this._scheduleTimer = Cc["@mozilla.org/timer;1"].
      createInstance(Ci.nsITimer);
    let listener = new Utils.EventListener(Utils.bind2(this, this._onSchedule));
    this._scheduleTimer.initWithCallback(listener, SCHEDULED_SYNC_INTERVAL,
                                         this._scheduleTimer.TYPE_REPEATING_SLACK);
    this._log.info("Weave scheduler enabled");
  },

  _disableSchedule: function WeaveSync__disableSchedule() {
    if (this._scheduleTimer) {
      this._scheduleTimer.cancel();
      this._scheduleTimer = null;
    }
    this._log.info("Weave scheduler disabled");
  },

  _onSchedule: function WeaveSync__onSchedule() {
    if (this.enabled) {
      this._log.info("Running scheduled sync");
      this._notify("sync", this._lock(this._syncAsNeeded)).async(this);
    }
  },

  _initLogs: function WeaveSync__initLogs() {
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

  _uploadVersion: function WeaveSync__uploadVersion() {
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
  _versionCheck: function WeaveSync__versionCheck() {
    let self = yield;

    DAV.GET("meta/version", self.cb);
    let ret = yield;

    if (!Utils.checkStatus(ret.status)) {
      this._log.info("Server has no version file.  Wiping server data.");
      this._serverWipe.async(this, self.cb);
      yield;
      this._uploadVersion.async(this, self.cb);
      yield;

    } else if (ret.responseText < STORAGE_FORMAT_VERSION) {
      this._log.info("Server version too low.  Wiping server data.");
      this._serverWipe.async(this, self.cb);
      yield;
      this._uploadVersion.async(this, self.cb);
      yield;

    } else if (ret.responseText > STORAGE_FORMAT_VERSION) {
      // FIXME: should we do something here?
    }
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

  _keyCheck: function WeaveSvc__keyCheck() {
    let self = yield;

    if ("none" != Utils.prefs.getCharPref("encryption")) {
      DAV.GET("private/privkey", self.cb);
      let keyResp = yield;
      Utils.ensureStatus(keyResp.status,
                        "Could not get private key from server", [[200,300],404]);

      if (keyResp.status != 404) {
        let id = ID.get('WeaveCryptoID');
        id.privkey = keyResp.responseText;
        Crypto.RSAkeydecrypt.async(Crypto, self.cb, id);
        id.pubkey = yield;
      } else {
        this._generateKeys.async(this, self.cb);
        yield;
      }
    }
  },

  _generateKeys: function WeaveSync__generateKeys() {
    let self = yield;

    this._log.debug("Generating new RSA key");

    let id = ID.get('WeaveCryptoID');
    Crypto.RSAkeygen.async(Crypto, self.cb, id);
    let [privkey, pubkey] = yield;

    id.privkey = privkey;
    id.pubkey = pubkey;

    DAV.MKCOL("private/", self.cb);
    let ret = yield;
    if (!ret)
      throw "Could not create private key directory";

    DAV.MKCOL("public/", self.cb);
    ret = yield;
    if (!ret)
      throw "Could not create public key directory";

    DAV.PUT("private/privkey", privkey, self.cb);
    ret = yield;
    Utils.ensureStatus(ret.status, "Could not upload private key");

    DAV.PUT("public/pubkey", pubkey, self.cb);
    ret = yield;
    Utils.ensureStatus(ret.status, "Could not upload public key");
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  // nsIObserver

  observe: function WeaveSync__observe(subject, topic, data) {
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

  _onQuitApplication: function WeaveSync__onQuitApplication() {
    if (!this.enabled ||
        !Utils.prefs.getBoolPref("syncOnQuit.enabled") ||
        !this._loggedIn)
      return;

    let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
             getService(Ci.nsIWindowWatcher);

    // This window has to be modal to prevent the application from quitting
    // until the sync finishes and the window closes.
    let window = ww.openWindow(null,
                               "chrome://weave/content/status.xul",
                               "Weave:status",
                               "chrome,centerscreen,modal",
                               null);
  },

  // These are global (for all engines)

  login: function WeaveSync_login(onComplete, password, passphrase) {
    this._localLock(this._notify("login", this._login,
                                 password, passphrase)).async(this, onComplete);
  },
  _login: function WeaveSync__login(password, passphrase) {
    let self = yield;

    // cache password & passphrase
    // if null, we'll try to get them from the pw manager below
    ID.get('WeaveID').setTempPassword(password);
    ID.get('WeaveCryptoID').setTempPassword(passphrase);

    this._log.debug("Logging in");

    if (!this.username)
      throw "No username set, login failed";
    if (!this.password)
      throw "No password given or found in password manager";

    DAV.baseURL = Utils.prefs.getCharPref("serverURL");
    DAV.defaultPrefix = "user/" + this.userPath;

    DAV.checkLogin.async(DAV, self.cb, this.username, this.password);
    let success = yield;
    if (!success) {
      try {
        // FIXME: This code may not be needed any more, due to the way
        // that the server is expected to create the user dir for us.
        this._checkUserDir.async(this, self.cb);
        yield;
      } catch (e) { /* FIXME: tmp workaround for services.m.c */ }
      DAV.checkLogin.async(DAV, self.cb, this.username, this.password);
      let success = yield;
      if (!success)
        throw "Login failed";
    }

    this._log.info("Using server URL: " + DAV.baseURL + DAV.defaultPrefix);

    this._versionCheck.async(this, self.cb);
    yield;
    this._keyCheck.async(this, self.cb);
    yield;

    this._loggedIn = true;

    this._setSchedule(this.schedule);

    self.done(true);
  },

  logout: function WeaveSync_logout() {
    this._log.info("Logging out");
    this._disableSchedule();
    this._loggedIn = false;
    ID.get('WeaveID').setTempPassword(null); // clear cached password
    ID.get('WeaveCryptoID').setTempPassword(null); // and passphrase
    this._os.notifyObservers(null, "weave:service:logout:success", "");
  },

  resetLock: function WeaveSvc_resetLock(onComplete) {
    this._notify("reset-server-lock", this._resetLock).async(this, onComplete);
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
    this._notify("server-wipe", this._lock(cb)).async(this, onComplete);
  },
  _serverWipe: function WeaveSvc__serverWipe() {
    let self = yield;

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

  sync: function WeaveSync_sync(onComplete) {
    this._notify("sync", this._lock(this._sync)).async(this, onComplete);
  },

  _sync: function WeaveSync__sync() {
    let self = yield;

    if (!this._loggedIn)
      throw "Can't sync: Not logged in";

    this._versionCheck.async(this, self.cb);
    yield;

    this._keyCheck.async(this, self.cb);
    yield;

    let engines = Engines.getAll();
    for (let i = 0; i < engines.length; i++) {
      if (!engines[i].enabled)
        continue;
      this._notify(engines[i].name + "-engine:sync",
                   this._syncEngine, engines[i]).async(this, self.cb);
      yield;
    }
  },

  // The values that engine scores must meet or exceed before we sync them
  // as needed.  These are engine-specific, as different kinds of data change
  // at different rates, so we store them in a hash indexed by engine name.
  _syncThresholds: {},

  _syncAsNeeded: function WeaveSync__syncAsNeeded() {
    let self = yield;

    if (!this._loggedIn)
      throw "Can't sync: Not logged in";

    this._versionCheck.async(this, self.cb);
    yield;

    this._keyCheck.async(this, self.cb);
    yield;

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
        this._notify(engine.name + "-engine:sync",
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
  },

  _syncEngine: function WeaveSvc__syncEngine(engine) {
    let self = yield;
    try {
      engine.sync(self.cb);
      yield;
      engine._tracker.resetScore();
    } catch(e) {
      this._log.error(Utils.exceptionStr(e));
      if (e.trace)
        this._log.trace(Utils.stackTrace(e.trace));
    }
  },

  resetServer: function WeaveSync_resetServer(onComplete) {
    this._notify("reset-server",
                 this._lock(this._resetServer)).async(this, onComplete);
  },
  _resetServer: function WeaveSync__resetServer() {
    let self = yield;

    if (!this._loggedIn)
      throw "Can't reset server: Not logged in";

    let engines = Engines.getAll();
    for (let i = 0; i < engines.length; i++) {
      if (!engines[i].enabled)
        continue;
      engines[i].resetServer(self.cb);
      yield;
    }
  },

  resetClient: function WeaveSync_resetClient(onComplete) {
    this._localLock(this._notify("reset-client",
                                 this._resetClient)).async(this, onComplete);
  },
  _resetClient: function WeaveSync__resetClient() {
    let self = yield;
    let engines = Engines.getAll();
    for (let i = 0; i < engines.length; i++) {
      if (!engines[i].enabled)
        continue;
      engines[i].resetClient(self.cb);
      yield;
    }
  },

  shareData: function WeaveSync_shareData(dataType,
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
       is left up to the engine for the specific dataType. */

    let messageName = "share-" + dataType;
    /* so for instance, if dataType is "bookmarks" then a message
     "share-bookmarks" will be sent out to any observers who are listening
     for it.  As far as I know, there aren't currently any listeners for
     "share-bookmarks" but we'll send it out just in case. */
    this._notify(messageName, this._lock(this._shareData,
                                         dataType,
                                         guid,
                                         username)).async(this, onComplete);
  },

  _shareData: function WeaveSync__shareData(dataType,
					    guid,
					    username) {
    dump( "in _shareData...\n" );
    let self = yield;
    if (!Engines.get(dataType).enabled) {
      this._log.warn( "Can't share disabled data type: " + dataType );
      return;
    }
    Engines.get(dataType).share(self.cb, guid, username);
    let ret = yield;
    self.done(ret);
  }

};
