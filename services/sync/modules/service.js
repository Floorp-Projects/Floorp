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


Function.prototype.async = generatorAsync;

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
    let branch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch);
    return branch.getCharPref("extensions.weave.username");
  },
  set username(value) {
    let branch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch);
    if (value)
      branch.setCharPref("extensions.weave.username", value);
    else
      branch.clearUserPref("extensions.weave.username");

    // fixme - need to loop over all Identity objects - needs some rethinking...
    this._mozId.username = value;
    this._cryptoId.username = value;
  },

  get password() { return this._mozId.password; },
  set password(value) { this._mozId.password = value; },

  get passphrase() { return this._cryptoId.password; },
  set passphrase(value) { this._cryptoId.password = value; },

  get userPath() {
    this._log.info("Hashing username " + this.username);

    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
      createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let hasher = Cc["@mozilla.org/security/hash;1"]
      .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA1);

    let data = converter.convertToByteArray(this.username, {});
    hasher.update(data, data.length);
    let rawHash = hasher.finish(false);

    // return the two-digit hexadecimal code for a byte
    function toHexString(charCode) {
      return ("0" + charCode.toString(16)).slice(-2);
    }

    let hash = [toHexString(rawHash.charCodeAt(i)) for (i in rawHash)].join("");
    this._log.debug("Username hashes to " + hash);
    return hash;
  },

  get currentUser() {
    if (this._dav.loggedIn)
      return this.username;
    return null;
  },

  _init: function WeaveSync__init() {
    this._initLogs();
    this._log.info("Weave Sync Service Initializing");

    this._serverURL = 'https://services.mozilla.com/';
    this._user = '';
    let enabled = false;
    let schedule = 0;
    try {
      let branch = Cc["@mozilla.org/preferences-service;1"].
        getService(Ci.nsIPrefBranch2);
      this._serverURL = branch.getCharPref("extensions.weave.serverURL");
      enabled = branch.getBoolPref("extensions.weave.enabled");
      schedule = branch.getIntPref("extensions.weave.schedule");

      branch.addObserver("extensions.weave", this, false);
    }
    catch (ex) { /* use defaults */ }

    if (!enabled) {
      this._log.info("Bookmarks sync disabled");
      return;
    }

    switch (schedule) {
    case 0:
      this._log.info("Bookmarks sync enabled, manual mode");
      break;
    case 1:
      this._log.info("Bookmarks sync enabled, automagic mode");
      this._enableSchedule();
      break;
    default:
      this._log.info("Bookmarks sync enabled");
      this._log.info("Invalid schedule setting: " + schedule);
      break;
    }
  },

  _enableSchedule: function WeaveSync__enableSchedule() {
    this._scheduleTimer = Cc["@mozilla.org/timer;1"].
      createInstance(Ci.nsITimer);
    let listener = new EventListener(bind2(this, this._onSchedule));
    this._scheduleTimer.initWithCallback(listener, 1800000, // 30 min
                                         this._scheduleTimer.TYPE_REPEATING_SLACK);
  },

  _disableSchedule: function WeaveSync__disableSchedule() {
    this._scheduleTimer = null;
  },

  _onSchedule: function WeaveSync__onSchedule() {
    this._log.info("Running scheduled sync");
    this.sync();
  },

  _initLogs: function WeaveSync__initLogs() {
    this._log = Log4Moz.Service.getLogger("Service.Main");

    let formatter = Log4Moz.Service.newFormatter("basic");
    let root = Log4Moz.Service.rootLogger;
    root.level = Log4Moz.Level.Debug;

    let capp = Log4Moz.Service.newAppender("console", formatter);
    capp.level = Log4Moz.Level.Warn;
    root.addAppender(capp);

    let dapp = Log4Moz.Service.newAppender("dump", formatter);
    dapp.level = Log4Moz.Level.All;
    root.addAppender(dapp);

    let logFile = this._dirSvc.get("ProfD", Ci.nsIFile);

    let brief = this._dirSvc.get("ProfD", Ci.nsIFile);
    brief.QueryInterface(Ci.nsILocalFile);

    brief.append("weave");
    if (!brief.exists())
      brief.create(brief.DIRECTORY_TYPE, PERMS_DIRECTORY);

    brief.append("logs");
    if (!brief.exists())
      brief.create(brief.DIRECTORY_TYPE, PERMS_DIRECTORY);

    brief.append("brief-log.txt");
    if (!brief.exists())
      brief.create(brief.NORMAL_FILE_TYPE, PERMS_FILE);

    let verbose = brief.parent.clone();
    verbose.append("verbose-log.txt");
    if (!verbose.exists())
      verbose.create(verbose.NORMAL_FILE_TYPE, PERMS_FILE);

    let fapp = Log4Moz.Service.newFileAppender("rotating", brief, formatter);
    fapp.level = Log4Moz.Level.Info;
    root.addAppender(fapp);
    let vapp = Log4Moz.Service.newFileAppender("rotating", verbose, formatter);
    vapp.level = Log4Moz.Level.Debug;
    root.addAppender(vapp);
  },

  _lock: function weaveSync__lock() {
    if (this._locked) {
      this._log.warn("Service lock failed: already locked");
      this._os.notifyObservers(null, "weave:service-lock:error", "");
      return false;
    }
    this._locked = true;
    this._log.debug("Service lock acquired");
    this._os.notifyObservers(null, "weave:service-lock:success", "");
    return true;
  },

  _unlock: function WeaveSync__unlock() {
    this._locked = false;
    this._log.debug("Service lock released");
    this._os.notifyObservers(null, "weave:service-unlock:success", "");
  },

  // IBookmarksSyncService internal implementation

  _login: function WeaveSync__login(onComplete) {
    let [self, cont] = yield;
    let success = false;

    try {
      this._log.debug("Logging in");
      this._os.notifyObservers(null, "weave:service-login:start", "");

      if (!this.username) {
        this._log.warn("No username set, login failed");
	return;
      }
      if (!this.password) {
        this._log.warn("No password given or found in password manager");
	return;
      }

      this._log.debug("USERNAME: " + this.username);
      this._log.debug("PASSWORD: " + this.password);
      this._log.debug("PASSPHRASE: " + this.passphrase);

      this._dav.baseURL = this._serverURL + "user/" + this.userPath + "/";
      this._log.info("Using server URL: " + this._dav.baseURL);

      this._dav.login.async(this._dav, cont, this.username, this.password);
      success = yield;

    } catch (e) {
      this._log.error("Exception caught: " + e.message);

    } finally {
      this._passphrase = null;
      if (success) {
        //this._log.debug("Login successful"); // chrome prints this too, hm
        this._os.notifyObservers(null, "weave:service-login:success", "");
      } else {
        //this._log.debug("Login error");
        this._os.notifyObservers(null, "weave:service-login:error", "");
      }
      generatorDone(this, self, onComplete, success);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _resetLock: function WeaveSync__resetLock(onComplete) {
    let [self, cont] = yield;
    let success = false;

    try {
      this._log.debug("Resetting server lock");
      this._os.notifyObservers(null, "weave:server-lock-reset:start", "");

      this._dav.forceUnlock.async(this._dav, cont);
      success = yield;

    } catch (e) {
      this._log.error("Exception caught: " + (e.message? e.message : e));

    } finally {
      if (success) {
        this._log.debug("Server lock reset successful");
        this._os.notifyObservers(null, "weave:server-lock-reset:success", "");
      } else {
        this._log.debug("Server lock reset failed");
        this._os.notifyObservers(null, "weave:server-lock-reset:error", "");
      }
      generatorDone(this, self, onComplete, success);
      yield; // generatorDone is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _sync: function WeaveSync__sync() {
    let [self, cont] = yield;
    let success = false;

    try {
      if (!this._lock())
	return;

      this._os.notifyObservers(null, "weave:service:sync:start", "");

      this._bmkEngine.sync(cont);
      yield;
      this._histEngine.sync(cont);
      yield;

      success = true;
      this._unlock();

    } catch (e) {
      this._log.error("Exception caught: " + (e.message? e.message : e));

    } finally {
      if (success)
        this._os.notifyObservers(null, "weave:service:sync:success", "");
      else
        this._os.notifyObservers(null, "weave:service:sync:error", "");
      generatorDone(this, self);
      yield; // generatorDone is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _resetServer: function WeaveSync__resetServer() {
    let [self, cont] = yield;

    try {
      if (!this._lock())
	return;

      this._bmkEngine.resetServer(cont);
      this._histEngine.resetServer(cont);

      this._unlock();

    } catch (e) {
      this._log.error("Exception caught: " + (e.message? e.message : e));

    } finally {
      generatorDone(this, self);
      yield; // generatorDone is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  _resetClient: function WeaveSync__resetClient() {
    let [self, cont] = yield;

    try {
      if (!this._lock())
	return;

      this._bmkEngine.resetClient(cont);
      this._histEngine.resetClient(cont);

      this._unlock();

    } catch (e) {
      this._log.error("Exception caught: " + (e.message? e.message : e));

    } finally {
      generatorDone(this, self);
      yield; // generatorDone is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupports]),

  // nsIObserver

  observe: function WeaveSync__observe(subject, topic, data) {
    switch (topic) {
    case "extensions.weave.enabled":
      switch (data) {
      case false:
        this._log.info("Disabling automagic bookmarks sync");
        this._disableSchedule();
        break;
      case true:
        this._log.info("Enabling automagic bookmarks sync");
        this._enableSchedule();
        break;
      }
      break;
    case "extensions.weave.schedule":
      switch (data) {
      case 0:
        this._log.info("Disabling automagic bookmarks sync");
        this._disableSchedule();
        break;
      case 1:
        this._log.info("Enabling automagic bookmarks sync");
        this._enableSchedule();
        break;
      default:
        this._log.warn("Unknown schedule value set");
        break;
      }
      break;
    default:
      // ignore, there are prefs we observe but don't care about
    }
  },

  // IBookmarksSyncService public methods

  // These are global (for all engines)

  login: function WeaveSync_login(password, passphrase) {
    if (!this._lock())
      return;
    // cache password & passphrase
    // if null, _login() will try to get them from the pw manager
    this._mozId.setTempPassword(password);
    this._cryptoId.setTempPassword(passphrase);
    let self = this;
    this._login.async(this, function() {self._unlock()});
  },

  logout: function WeaveSync_logout() {
    this._log.info("Logging out");
    this._dav.logout();
    this._mozId.setTempPassword(null); // clear cached password
    this._cryptoId.setTempPassword(null); // and passphrase
    this._os.notifyObservers(null, "weave:service-logout:success", "");
  },

  resetLock: function WeaveSync_resetLock() {
    if (!this._lock())
      return;
    let self = this;
    this._resetLock.async(this, function() {self._unlock()});
  },

  // These are per-engine

  sync: function WeaveSync_sync() { this._sync.async(this); },
  resetServer: function WeaveSync_resetServer() { this._resetServer.async(this); },
  resetClient: function WeaveSync_resetClient() { this._resetClient.async(this); }
};
