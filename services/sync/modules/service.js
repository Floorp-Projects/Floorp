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

const EXPORTED_SYMBOLS = ['WeaveSyncService'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/crypto.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/dav.js");
Cu.import("resource://weave/identity.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;
let Crypto = new WeaveCrypto();

/*
 * Service singleton
 * Main entry point into Weave's sync framework
 */

function WeaveSyncService() { this._init(); }
WeaveSyncService.prototype = {

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

  __dav: null,
  get _dav() {
    if (!this.__dav)
      this.__dav = new DAVCollection();
    return this.__dav;
  },

  // FIXME: engines should be loaded dynamically somehow / need API to register

  __bmkEngine: null,
  get _bmkEngine() {
    if (!this.__bmkEngine)
      this.__bmkEngine = new BookmarksEngine(this._dav, this._cryptoId);
    return this.__bmkEngine;
  },

  __histEngine: null,
  get _histEngine() {
    if (!this.__histEngine)
      this.__histEngine = new HistoryEngine(this._dav, this._cryptoId);
    return this.__histEngine;
  },

  // Logger object
  _log: null,

  // Timer object for automagically syncing
  _scheduleTimer: null,

  __mozId: null,
  get _mozId() {
    if (this.__mozId === null)
      this.__mozId = new Identity('Mozilla Services Password', this.username);
    return this.__mozId;
  },

  __cryptoId: null,
  get _cryptoId() {
    if (this.__cryptoId === null)
      this.__cryptoId = new Identity('Mozilla Services Encryption Passphrase',
				     this.username);
    return this.__cryptoId;
  },

  get username() {
    return Utils.prefs.getCharPref("username");
  },
  set username(value) {
    if (value)
      Utils.prefs.setCharPref("username", value);
    else
      Utils.prefs.clearUserPref("username");

    // fixme - need to loop over all Identity objects - needs some rethinking...
    this._mozId.username = value;
    this._cryptoId.username = value;
  },

  get password() { return this._mozId.password; },
  set password(value) { this._mozId.password = value; },

  get passphrase() { return this._cryptoId.password; },
  set passphrase(value) { this._cryptoId.password = value; },

  get userPath() { return this._mozId.userHash; },

  get currentUser() {
    if (this._dav.loggedIn)
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

  _init: function WeaveSync__init() {
    this._initLogs();
    this._log.info("Weave Sync Service Initializing");

    Utils.prefs.addObserver("", this, false);

    if (!this.enabled) {
      this._log.info("Weave Sync disabled");
      return;
    }

    this._setSchedule(this.schedule);
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
    this._scheduleTimer.initWithCallback(listener, 1800000, // 30 min
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
      this.sync();
    }
  },

  _initLogs: function WeaveSync__initLogs() {
    this._log = Log4Moz.Service.getLogger("Service.Main");
    this._log.level =
      Log4Moz.Level[Utils.prefs.getCharPref("log.logger.service.main")];

    let formatter = Log4Moz.Service.newFormatter("basic");
    let root = Log4Moz.Service.rootLogger;
    root.level = Log4Moz.Level[Utils.prefs.getCharPref("log.rootLogger")];

    let capp = Log4Moz.Service.newAppender("console", formatter);
    capp.level = Log4Moz.Level[Utils.prefs.getCharPref("log.appender.console")];
    root.addAppender(capp);

    let dapp = Log4Moz.Service.newAppender("dump", formatter);
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

    let fapp = Log4Moz.Service.newFileAppender("rotating", brief, formatter);
    fapp.level = Log4Moz.Level[Utils.prefs.getCharPref("log.appender.briefLog")];
    root.addAppender(fapp);
    let vapp = Log4Moz.Service.newFileAppender("rotating", verbose, formatter);
    vapp.level = Log4Moz.Level[Utils.prefs.getCharPref("log.appender.debugLog")];
    root.addAppender(vapp);
  },

  _lock: function weaveSync__lock() {
    let self = yield;

    this._dav.lock.async(this._dav, self.cb);
    let locked = yield;

    if (!locked) {
      this._log.warn("Service lock failed: already locked");
      this._os.notifyObservers(null, "weave:service-lock:error", "");
      self.done(false);
      return;
    }

    this._log.debug("Service lock acquired");
    this._os.notifyObservers(null, "weave:service-lock:success", "");
    self.done(true);
  },

  _unlock: function WeaveSync__unlock() {
    let self = yield;

    this._dav.unlock.async(this._dav, self.cb);
    let unlocked = yield;

    if (!unlocked) {
      this._log.error("Service unlock failed");
      this._os.notifyObservers(null, "weave:service-unlock:error", "");
      self.done(false);
      return;
    }

    this._log.debug("Service lock released");
    this._os.notifyObservers(null, "weave:service-unlock:success", "");
    self.done(true);
  },

  _login: function WeaveSync__login(password, passphrase) {
    let self = yield;

    try {
      if (this._dav.locked)
        throw "Could not login: could not acquire lock";
      this._dav.allowLock = false;
  
      // cache password & passphrase
      // if null, we'll try to get them from the pw manager below
      this._mozId.setTempPassword(password);
      this._cryptoId.setTempPassword(passphrase);

      this._log.debug("Logging in");
      this._os.notifyObservers(null, "weave:service-login:start", "");

      if (!this.username)
        throw "No username set, login failed";
      if (!this.password)
        throw "No password given or found in password manager";

      let serverURL = Utils.prefs.getCharPref("serverURL");
      this._dav.baseURL = serverURL + "user/" + this.userPath + "/";
      this._log.info("Using server URL: " + this._dav.baseURL);

      this._dav.login.async(this._dav, self.cb, this.username, this.password);
      let success = yield;

      // FIXME: we want to limit this to when we get a 404!
      if (!success) {
        this._log.debug("Attempting to create user directory");

        this._dav.baseURL = serverURL;
        this._dav.MKCOL("user/" + this.userPath, self.cb);
        let ret = yield;
        if (!ret)
          throw "Could not create user directory.  Got status: " + ret.status;

        this._log.debug("Successfully created user directory.  Re-attempting login.");
        this._dav.baseURL = serverURL + "user/" + this.userPath + "/";
        this._dav.login.async(this._dav, self.cb, this.username, this.password);
        success = yield;
        if (!success)
          throw "Created user directory, but login still failed.  Aborting.";
      }

      // FIXME: remove this after services.m.c gets fixed to not
      // return 500 from a GET when parent dirs don't exist
      this._dav.MKCOL("private/", self.cb);
      ret = yield;
      if (!ret)
        throw "Could not create private key directory";

      this._dav.GET("private/privkey", self.cb);
      let keyResp = yield;
      Utils.ensureStatus(keyResp.status,
                         "Could not get private key from server", [[200,300],404]);

      if (keyResp.status != 404) {
        this._cryptoId.privkey = keyResp.responseText;
        Crypto.RSAkeydecrypt.async(Crypto, self.cb, this._cryptoId);
        this._cryptoId.pubkey = yield;

      } else {
        // FIXME: hack to wipe everyone's server data... needs to be removed at some point
        this._serverWipe.async(this, self.cb);
        yield;
        
        // generate a new key
        this._log.debug("Generating new RSA key");
        Crypto.RSAkeygen.async(Crypto, self.cb, this._cryptoId);
        let [privkey, pubkey] = yield;

        this._cryptoId.privkey = privkey;
        this._cryptoId.pubkey = pubkey;

        this._dav.MKCOL("private/", self.cb);
        ret = yield;
        if (!ret)
          throw "Could not create private key directory";

        this._dav.MKCOL("public/", self.cb);
        ret = yield;
        if (!ret)
          throw "Could not create public key directory";

        this._dav.PUT("private/privkey", privkey, self.cb);
        ret = yield;
        Utils.ensureStatus(ret.status, "Could not upload private key");

        this._dav.PUT("public/pubkey", pubkey, self.cb);
        ret = yield;
        Utils.ensureStatus(ret.status, "Could not upload public key");
      }

      this._passphrase = null;
      this._dav.allowLock = true;
      this._os.notifyObservers(null, "weave:service-login:success", "");
      self.done(true);

    } catch (e) {
      this._dav.allowLock = true;
      this._os.notifyObservers(null, "weave:service-login:error", "");
      throw e;
    }
  },

  // NOTE: doesn't lock because it's called from _login() which already holds the lock
  _serverWipe: function WeaveSync__serverWipe() {
    let self = yield;

    this._dav.listFiles.async(this._dav, self.cb);
    let names = yield;

    for (let i = 0; i < names.length; i++) {
      this._dav.DELETE(names[i], self.cb);
      let resp = yield;
      this._log.debug(resp.status);
    }

    self.done();
  },

  // NOTE: can't lock because it assumes the lock is being held ;)
  _resetLock: function WeaveSync__resetLock() {
    let self = yield;
    let success = false;

    try {
      this._log.debug("Resetting server lock");
      this._os.notifyObservers(null, "weave:server-lock-reset:start", "");

      this._dav.forceUnlock.async(this._dav, self.cb);
      success = yield;

    } catch (e) {
      throw e;

    } finally {
      if (success) {
        this._log.debug("Server lock reset successful");
        this._os.notifyObservers(null, "weave:server-lock-reset:success", "");
      } else {
        this._log.debug("Server lock reset failed");
        this._os.notifyObservers(null, "weave:server-lock-reset:error", "");
      }
      self.done(success);
    }
  },

  _syncEngine: function WeaveSync__syncEngine(eng) {
    let self = yield;

    this._os.notifyObservers(null, "weave:" + eng.name + ":sync:start", "");

    let ret;
    try {
      eng.sync(self.cb);
      ret = yield;
    } catch (e) {
      this._log.warn("Engine failed with error: " + Utils.exceptionStr(e));
      if (e.trace)
        this._log.debug("Engine stack trace: " + Utils.stackTrace(e.trace));
    }

    if (ret)
      this._os.notifyObservers(null, "weave:" + eng.name + ":sync:success", "");
    else
      this._os.notifyObservers(null, "weave:" + eng.name + ":sync:error", "");

    self.done();
  },

  _sync: function WeaveSync__sync() {
    let self = yield;

    try {
      this._lock.async(this, self.cb)
      let locked = yield;
      if (!locked)
        return;

      this._os.notifyObservers(null, "weave:service:sync:start", "");

      if (Utils.prefs.getBoolPref("bookmarks")) {
        this._syncEngine.async(this, self.cb, this._bmkEngine);
        yield;
      }
      if (Utils.prefs.getBoolPref("history")) {
        this._syncEngine.async(this, self.cb, this._histEngine);
        yield;
      }

      this._unlock.async(this, self.cb)
      yield;
      this._os.notifyObservers(null, "weave:service:sync:success", "");

    } catch (e) {
      this._unlock.async(this, self.cb)
      yield;
      this._os.notifyObservers(null, "weave:service:sync:error", "");
      throw e;
    }

    self.done();
  },

  _resetServer: function WeaveSync__resetServer() {
    let self = yield;

    this._lock.async(this, self.cb)
    let locked = yield;
    if (!locked)
      return;

    try {
      this._bmkEngine.resetServer(self.cb);
      this._histEngine.resetServer(self.cb);

    } catch (e) {
      throw e;

    } finally {
      this._unlock.async(this, self.cb)
      yield;
    }

    self.done();
  },

  _resetClient: function WeaveSync__resetClient() {
    let self = yield;

    this._lock.async(this, self.cb)
    let locked = yield;
    if (!locked)
      return;

    try {
      this._bmkEngine.resetClient(self.cb);
      this._histEngine.resetClient(self.cb);

    } catch (e) {
      throw e;

    } finally {
      this._unlock.async(this, self.cb)
      yield;
    }

    self.done();
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupports]),

  // nsIObserver

  observe: function WeaveSync__observe(subject, topic, data) {
    if (topic != "nsPref:changed")
      return;

    switch (data) {
    case "enabled": // this works because this.schedule is 0 when disabled
    case "schedule":
      this._setSchedule(this.schedule);
      break;
    }
  },

  // These are global (for all engines)

  login: function WeaveSync_login(password, passphrase) {
    this._login.async(this, null, password, passphrase);
  },

  logout: function WeaveSync_logout() {
    this._log.info("Logging out");
    this._dav.logout();
    this._mozId.setTempPassword(null); // clear cached password
    this._cryptoId.setTempPassword(null); // and passphrase
    this._os.notifyObservers(null, "weave:service-logout:success", "");
  },

  resetLock: function WeaveSync_resetLock() { this._resetLock.async(this); },

  // These are per-engine

  sync: function WeaveSync_sync() { this._sync.async(this); },
  resetServer: function WeaveSync_resetServer() { this._resetServer.async(this); },
  resetClient: function WeaveSync_resetClient() { this._resetClient.async(this); }
};
