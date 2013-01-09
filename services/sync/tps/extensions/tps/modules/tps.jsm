/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* This is a JavaScript module (JSM) to be imported via
  * Components.utils.import() and acts as a singleton. Only the following
  * listed symbols will exposed on import, and only when and where imported.
  */

let EXPORTED_SYMBOLS = ["TPS"];

const {classes: CC, interfaces: CI, utils: CU} = Components;

CU.import("resource://gre/modules/XPCOMUtils.jsm");
CU.import("resource://gre/modules/Services.jsm");
CU.import("resource://services-common/async.js");
CU.import("resource://services-sync/constants.js");
CU.import("resource://services-sync/main.js");
CU.import("resource://services-sync/util.js");
CU.import("resource://tps/addons.jsm");
CU.import("resource://tps/bookmarks.jsm");
CU.import("resource://tps/logger.jsm");
CU.import("resource://tps/passwords.jsm");
CU.import("resource://tps/history.jsm");
CU.import("resource://tps/forms.jsm");
CU.import("resource://tps/prefs.jsm");
CU.import("resource://tps/tabs.jsm");
CU.import("resource://tps/windows.jsm");

var hh = CC["@mozilla.org/network/protocol;1?name=http"]
         .getService(CI.nsIHttpProtocolHandler);
var prefs = CC["@mozilla.org/preferences-service;1"]
            .getService(CI.nsIPrefBranch);

var mozmillInit = {};
CU.import('resource://mozmill/modules/init.js', mozmillInit);

const ACTION_ADD              = "add";
const ACTION_VERIFY           = "verify";
const ACTION_VERIFY_NOT       = "verify-not";
const ACTION_MODIFY           = "modify";
const ACTION_SYNC             = "sync";
const ACTION_DELETE           = "delete";
const ACTION_PRIVATE_BROWSING = "private-browsing";
const ACTION_WIPE_REMOTE      = "wipe-remote";
const ACTION_WIPE_SERVER      = "wipe-server";
const ACTION_SET_ENABLED      = "set-enabled";

const ACTIONS = [ACTION_ADD, ACTION_VERIFY, ACTION_VERIFY_NOT,
                 ACTION_MODIFY, ACTION_SYNC, ACTION_DELETE,
                 ACTION_PRIVATE_BROWSING, ACTION_WIPE_REMOTE,
                 ACTION_WIPE_SERVER, ACTION_SET_ENABLED];

const SYNC_WIPE_CLIENT  = "wipe-client";
const SYNC_WIPE_REMOTE  = "wipe-remote";
const SYNC_WIPE_SERVER  = "wipe-server";
const SYNC_RESET_CLIENT = "reset-client";
const SYNC_START_OVER   = "start-over";

const OBSERVER_TOPICS = ["weave:engine:start-tracking",
                         "weave:engine:stop-tracking",
                         "weave:service:sync:finish",
                         "weave:service:sync:error",
                         "sessionstore-windows-restored",
                         "private-browsing"];

let TPS = {
  _waitingForSync: false,
  _isTracking: false,
  _test: null,
  _currentAction: -1,
  _currentPhase: -1,
  _errors: 0,
  _syncErrors: 0,
  _usSinceEpoch: 0,
  _tabsAdded: 0,
  _tabsFinished: 0,
  _phaselist: {},
  _operations_pending: 0,
  _loggedIn: false,
  _enabledEngines: null,

  DumpError: function (msg) {
    this._errors++;
    Logger.logError("[phase" + this._currentPhase + "] " + msg);
    this.quit();
  },

  QueryInterface: XPCOMUtils.generateQI([CI.nsIObserver,
                                         CI.nsISupportsWeakReference]),

  observe: function TPS__observe(subject, topic, data) {
    try {
      Logger.logInfo("----------event observed: " + topic);
      switch(topic) {
        case "private-browsing":
          Logger.logInfo("private browsing " + data);
          break;
        case "weave:service:sync:error":
          if (this._waitingForSync && this._syncErrors == 0) {
            // if this is the first sync error, retry...
            Logger.logInfo("sync error; retrying...");
            this._syncErrors++;
            this._waitingForSync = false;
            Utils.nextTick(this.RunNextTestAction, this);
          }
          else if (this._waitingForSync) {
            // ...otherwise abort the test
            this.DumpError("sync error; aborting test");
            return;
          }
          break;

        case "weave:service:sync:finish":
          if (this._waitingForSync) {
            this._syncErrors = 0;
            this._waitingForSync = false;
            // Wait a second before continuing, otherwise we can get
            // 'sync not complete' errors.
            Utils.namedTimer(function() {
              this.FinishAsyncOperation();
            }, 1000, this, "postsync");
          }
          break;

        case "weave:engine:start-tracking":
          this._isTracking = true;
          break;

        case "weave:engine:stop-tracking":
          this._isTracking = false;
          break;

        case "sessionstore-windows-restored":
          Utils.nextTick(this.RunNextTestAction, this);
          break;
      }
    }
    catch(e) {
      this.DumpError("Exception caught: " + Utils.exceptionStr(e));
      return;
    }
  },

  StartAsyncOperation: function TPS__StartAsyncOperation() {
    this._operations_pending++;
  },

  FinishAsyncOperation: function TPS__FinishAsyncOperation() {
    this._operations_pending--;
    if (!this.operations_pending) {
      this._currentAction++;
      Utils.nextTick(function() {
        this.RunNextTestAction();
      }, this);
    }
  },

  quit: function () {
    OBSERVER_TOPICS.forEach(function(topic) {
      Services.obs.removeObserver(this, topic);
    }, this);
    Logger.close();
    this.goQuitApplication();
  },

  HandleWindows: function (aWindow, action) {
    Logger.logInfo("executing action " + action.toUpperCase() +
                   " on window " + JSON.stringify(aWindow));
    switch(action) {
      case ACTION_ADD:
        BrowserWindows.Add(aWindow.private, function(win) {
          Logger.logInfo("window finished loading");
          this.FinishAsyncOperation();
        }.bind(this));
        break;
    }
    Logger.logPass("executing action " + action.toUpperCase() + " on windows");
  },

  HandleTabs: function (tabs, action) {
    this._tabsAdded = tabs.length;
    this._tabsFinished = 0;
    for each (let tab in tabs) {
      Logger.logInfo("executing action " + action.toUpperCase() +
                     " on tab " + JSON.stringify(tab));
      switch(action) {
        case ACTION_ADD:
          // When adding tabs, we keep track of how many tabs we're adding,
          // and wait until we've received that many onload events from our
          // new tabs before continuing
          let that = this;
          let taburi = tab.uri;
          BrowserTabs.Add(tab.uri, function() {
            that._tabsFinished++;
            Logger.logInfo("tab for " + taburi + " finished loading");
            if (that._tabsFinished == that._tabsAdded) {
              Logger.logInfo("all tabs loaded, continuing...");
              that.FinishAsyncOperation();
            }
          });
          break;
        case ACTION_VERIFY:
          Logger.AssertTrue(typeof(tab.profile) != "undefined",
            "profile must be defined when verifying tabs");
          Logger.AssertTrue(
            BrowserTabs.Find(tab.uri, tab.title, tab.profile), "error locating tab");
          break;
        case ACTION_VERIFY_NOT:
          Logger.AssertTrue(typeof(tab.profile) != "undefined",
            "profile must be defined when verifying tabs");
          Logger.AssertTrue(
            !BrowserTabs.Find(tab.uri, tab.title, tab.profile),
            "tab found which was expected to be absent");
          break;
        default:
          Logger.AssertTrue(false, "invalid action: " + action);
      }
    }
    Logger.logPass("executing action " + action.toUpperCase() + " on tabs");
  },

  HandlePrefs: function (prefs, action) {
    for each (pref in prefs) {
      Logger.logInfo("executing action " + action.toUpperCase() +
                     " on pref " + JSON.stringify(pref));
      let preference = new Preference(pref);
      switch(action) {
        case ACTION_MODIFY:
          preference.Modify();
          break;
        case ACTION_VERIFY:
          preference.Find();
          break;
        default:
          Logger.AssertTrue(false, "invalid action: " + action);
      }
    }
    Logger.logPass("executing action " + action.toUpperCase() + " on pref");
  },

  HandleForms: function (data, action) {
    for each (datum in data) {
      Logger.logInfo("executing action " + action.toUpperCase() +
                     " on form entry " + JSON.stringify(datum));
      let formdata = new FormData(datum, this._usSinceEpoch);
      switch(action) {
        case ACTION_ADD:
          formdata.Create();
          break;
        case ACTION_DELETE:
          formdata.Remove();
          break;
        case ACTION_VERIFY:
          Logger.AssertTrue(formdata.Find(), "form data not found");
          break;
        case ACTION_VERIFY_NOT:
          Logger.AssertTrue(!formdata.Find(),
            "form data found, but it shouldn't be present");
          break;
        default:
          Logger.AssertTrue(false, "invalid action: " + action);
      }
    }
    Logger.logPass("executing action " + action.toUpperCase() +
                   " on formdata");
  },

  HandleHistory: function (entries, action) {
    try {
      for each (entry in entries) {
        Logger.logInfo("executing action " + action.toUpperCase() +
                       " on history entry " + JSON.stringify(entry));
        switch(action) {
          case ACTION_ADD:
            HistoryEntry.Add(entry, this._usSinceEpoch);
            break;
          case ACTION_DELETE:
            HistoryEntry.Delete(entry, this._usSinceEpoch);
            break;
          case ACTION_VERIFY:
            Logger.AssertTrue(HistoryEntry.Find(entry, this._usSinceEpoch),
              "Uri visits not found in history database");
            break;
          case ACTION_VERIFY_NOT:
            Logger.AssertTrue(!HistoryEntry.Find(entry, this._usSinceEpoch),
              "Uri visits found in history database, but they shouldn't be");
            break;
          default:
            Logger.AssertTrue(false, "invalid action: " + action);
        }
      }
      Logger.logPass("executing action " + action.toUpperCase() +
                     " on history");
    }
    catch(e) {
      DumpHistory();
      throw(e);
    }
  },

  HandlePasswords: function (passwords, action) {
    try {
      for each (password in passwords) {
        let password_id = -1;
        Logger.logInfo("executing action " + action.toUpperCase() +
                      " on password " + JSON.stringify(password));
        var password = new Password(password);
        switch (action) {
          case ACTION_ADD:
            Logger.AssertTrue(password.Create() > -1, "error adding password");
            break;
          case ACTION_VERIFY:
            Logger.AssertTrue(password.Find() != -1, "password not found");
            break;
          case ACTION_VERIFY_NOT:
            Logger.AssertTrue(password.Find() == -1,
              "password found, but it shouldn't exist");
            break;
          case ACTION_DELETE:
            Logger.AssertTrue(password.Find() != -1, "password not found");
            password.Remove();
            break;
          case ACTION_MODIFY:
            if (password.updateProps != null) {
              Logger.AssertTrue(password.Find() != -1, "password not found");
              password.Update();
            }
            break;
          default:
            Logger.AssertTrue(false, "invalid action: " + action);
        }
      }
      Logger.logPass("executing action " + action.toUpperCase() +
                     " on passwords");
    }
    catch(e) {
      DumpPasswords();
      throw(e);
    }
  },

  HandleAddons: function (addons, action, state) {
    for each (let entry in addons) {
      Logger.logInfo("executing action " + action.toUpperCase() +
                     " on addon " + JSON.stringify(entry));
      let addon = new Addon(this, entry);
      switch(action) {
        case ACTION_ADD:
          addon.install();
          break;
        case ACTION_DELETE:
          addon.uninstall();
          break;
        case ACTION_VERIFY:
          Logger.AssertTrue(addon.find(state), 'addon ' + addon.id + ' not found');
          break;
        case ACTION_VERIFY_NOT:
          Logger.AssertFalse(addon.find(state), 'addon ' + addon.id + " is present, but it shouldn't be");
          break;
        case ACTION_SET_ENABLED:
          Logger.AssertTrue(addon.setEnabled(state), 'addon ' + addon.id + ' not found');
          break;
        default:
          throw new Error("Unknown action for add-on: " + action);
      }
    }
    Logger.logPass("executing action " + action.toUpperCase() +
                   " on addons");
  },

  HandleBookmarks: function (bookmarks, action) {
    try {
      let items = [];
      for (folder in bookmarks) {
        let last_item_pos = -1;
        for each (bookmark in bookmarks[folder]) {
          Logger.clearPotentialError();
          let placesItem;
          bookmark['location'] = folder;
          if (last_item_pos != -1)
            bookmark['last_item_pos'] = last_item_pos;
          let item_id = -1;
          if (action != ACTION_MODIFY && action != ACTION_DELETE)
            Logger.logInfo("executing action " + action.toUpperCase() +
                           " on bookmark " + JSON.stringify(bookmark));
          if ("uri" in bookmark)
            placesItem = new Bookmark(bookmark);
          else if ("folder" in bookmark)
            placesItem = new BookmarkFolder(bookmark);
          else if ("livemark" in bookmark)
            placesItem = new Livemark(bookmark);
          else if ("separator" in bookmark)
            placesItem = new Separator(bookmark);
          if (action == ACTION_ADD) {
            item_id = placesItem.Create();
          }
          else {
            item_id = placesItem.Find();
            if (action == ACTION_VERIFY_NOT) {
              Logger.AssertTrue(item_id == -1,
                "places item exists but it shouldn't: " +
                JSON.stringify(bookmark));
            }
            else
              Logger.AssertTrue(item_id != -1, "places item not found", true);
          }

          last_item_pos = placesItem.GetItemIndex();
          items.push(placesItem);
        }
      }

      if (action == ACTION_DELETE || action == ACTION_MODIFY) {
        for each (item in items) {
          Logger.logInfo("executing action " + action.toUpperCase() +
                         " on bookmark " + JSON.stringify(item));
          switch(action) {
            case ACTION_DELETE:
              item.Remove();
              break;
            case ACTION_MODIFY:
              if (item.updateProps != null)
                item.Update();
              break;
          }
        }
      }

      Logger.logPass("executing action " + action.toUpperCase() +
        " on bookmarks");
    }
    catch(e) {
      DumpBookmarks();
      throw(e);
    }
  },

  MozmillEndTestListener: function TPS__MozmillEndTestListener(obj) {
    Logger.logInfo("mozmill endTest: " + JSON.stringify(obj));
    if (obj.failed > 0) {
      this.DumpError('mozmill test failed, name: ' + obj.name + ', reason: ' + JSON.stringify(obj.fails));
      return;
    }
    else if ('skipped' in obj && obj.skipped) {
      this.DumpError('mozmill test failed, name: ' + obj.name + ', reason: ' + obj.skipped_reason);
      return;
    }
    else {
      Utils.namedTimer(function() {
        this.FinishAsyncOperation();
      }, 2000, this, "postmozmilltest");
    }
  },

  MozmillSetTestListener: function TPS__MozmillSetTestListener(obj) {
    Logger.logInfo("mozmill setTest: " + obj.name);
  },

  RunNextTestAction: function() {
    try {
      if (this._currentAction >=
          this._phaselist["phase" + this._currentPhase].length) {
        // we're all done
        Logger.logInfo("test phase " + this._currentPhase + ": " +
          (this._errors ? "FAIL" : "PASS"));
        this.quit();
        return;
      }

      if (this.seconds_since_epoch)
        this._usSinceEpoch = this.seconds_since_epoch * 1000 * 1000;
      else {
        this.DumpError("seconds-since-epoch not set");
        return;
      }

      let phase = this._phaselist["phase" + this._currentPhase];
      let action = phase[this._currentAction];
      Logger.logInfo("starting action: " + JSON.stringify(action));
      action[0].apply(this, action.slice(1));

      // if we're in an async operation, don't continue on to the next action
      if (this._operations_pending)
        return;

      this._currentAction++;
    }
    catch(e) {
      this.DumpError("Exception caught: " + Utils.exceptionStr(e));
      return;
    }
    this.RunNextTestAction();
  },

  /**
   * Runs a single test phase.
   *
   * This is the main entry point for each phase of a test. The TPS command
   * line driver loads this module and calls into the function with the
   * arguments from the command line.
   *
   * When a phase is executed, the file is loaded as JavaScript into the
   * current object.
   *
   * The following keys in the options argument have meaning:
   *
   *   - ignoreUnusedEngines  If true, unused engines will be unloaded from
   *                          Sync. This makes output easier to parse and is
   *                          useful for debugging test failures.
   *
   * @param  file
   *         String URI of the file to open.
   * @param  phase
   *         String name of the phase to run.
   * @param  logpath
   *         String path of the log file to write to.
   * @param  options
   *         Object defining addition run-time options.
   */
  RunTestPhase: function (file, phase, logpath, options) {
    try {
      let settings = options || {};

      Logger.init(logpath);
      Logger.logInfo("Sync version: " + WEAVE_VERSION);
      Logger.logInfo("Firefox builddate: " + Services.appinfo.appBuildID);
      Logger.logInfo("Firefox version: " + Services.appinfo.version);

      // do some sync housekeeping
      if (Weave.Service.isLoggedIn) {
        this.DumpError("Sync logged in on startup...profile may be dirty");
        return;
      }

      // Wait for Sync service to become ready.
      if (!Weave.Status.ready) {
        this.waitForEvent("weave:service:ready");
      }

      // Always give Sync an extra tick to initialize. If we waited for the
      // service:ready event, this is required to ensure all handlers have
      // executed.
      Utils.nextTick(this._executeTestPhase.bind(this, file, phase, settings));
    } catch(e) {
      this.DumpError("Exception caught: " + Utils.exceptionStr(e));
      return;
    }
  },

  /**
   * Executes a single test phase.
   *
   * This is called by RunTestPhase() after the environment is validated.
   */
  _executeTestPhase: function _executeTestPhase(file, phase, settings) {
    try {
      OBSERVER_TOPICS.forEach(function(topic) {
        Services.obs.addObserver(this, topic, true);
      }, this);

      // parse the test file
      Services.scriptloader.loadSubScript(file, this);
      this._currentPhase = phase;
      let this_phase = this._phaselist["phase" + this._currentPhase];

      if (this_phase == undefined) {
        this.DumpError("invalid phase " + this._currentPhase);
        return;
      }

      if (this.phases["phase" + this._currentPhase] == undefined) {
        this.DumpError("no profile defined for phase " + this._currentPhase);
        return;
      }

      // If we have restricted the active engines, unregister engines we don't
      // care about.
      if (settings.ignoreUnusedEngines && Array.isArray(this._enabledEngines)) {
        let names = {};
        for each (let name in this._enabledEngines) {
          names[name] = true;
        }

        for (let engine of Weave.Service.engineManager.getEnabled()) {
          if (!(engine.name in names)) {
            Logger.logInfo("Unregistering unused engine: " + engine.name);
            Weave.Service.engineManager.unregister(engine);
          }
        }
      }

      Logger.logInfo("Starting phase " + parseInt(phase, 10) + "/" +
                     Object.keys(this._phaselist).length);

      Logger.logInfo("setting client.name to " + this.phases["phase" + this._currentPhase]);
      Weave.Svc.Prefs.set("client.name", this.phases["phase" + this._currentPhase]);

      // TODO Phases should be defined in a data type that has strong
      // ordering, not by lexical sorting.
      let currentPhase = parseInt(this._currentPhase, 10);
      // Reset everything at the beginning of the test.
      if (currentPhase <= 1) {
        this_phase.unshift([this.ResetData]);
      }

      // Wipe the server at the end of the final test phase.
      if (currentPhase >= Object.keys(this.phases).length) {
        this_phase.push([this.WipeServer]);
      }

      // Store account details as prefs so they're accessible to the mozmill
      // framework.
      prefs.setCharPref('tps.account.username', this.config.account.username);
      prefs.setCharPref('tps.account.password', this.config.account.password);
      prefs.setCharPref('tps.account.passphrase', this.config.account.passphrase);
      if (this.config.account['serverURL']) {
        prefs.setCharPref('tps.account.serverURL', this.config.account.serverURL);
      }

      // start processing the test actions
      this._currentAction = 0;
    }
    catch(e) {
      this.DumpError("Exception caught: " + Utils.exceptionStr(e));
      return;
    }
  },

  /**
   * Register a single phase with the test harness.
   *
   * This is called when loading individual test files.
   *
   * @param  phasename
   *         String name of the phase being loaded.
   * @param  fnlist
   *         Array of functions/actions to perform.
   */
  Phase: function Test__Phase(phasename, fnlist) {
    this._phaselist[phasename] = fnlist;
  },

  /**
   * Restrict enabled Sync engines to a specified set.
   *
   * This can be called by a test to limit what engines are enabled. It is
   * recommended to call it to reduce the overhead and log clutter for the
   * test.
   *
   * The "clients" engine is special and is always enabled, so there is no
   * need to specify it.
   *
   * @param  names
   *         Array of Strings for engines to make active during the test.
   */
  EnableEngines: function EnableEngines(names) {
    if (!Array.isArray(names)) {
      throw new Error("Argument to RestrictEngines() is not an array: "
                      + typeof(names));
    }

    this._enabledEngines = names;
  },

  RunMozmillTest: function TPS__RunMozmillTest(testfile) {
    var mozmillfile = CC["@mozilla.org/file/local;1"]
                      .createInstance(CI.nsILocalFile);
    if (hh.oscpu.toLowerCase().indexOf('windows') > -1) {
      let re = /\/(\w)\/(.*)/;
      this.config.testdir = this.config.testdir.replace(re, "$1://$2").replace(/\//g, "\\");
    }
    mozmillfile.initWithPath(this.config.testdir);
    mozmillfile.appendRelativePath(testfile);
    Logger.logInfo("Running mozmill test " + mozmillfile.path);

    var frame = {};
    CU.import('resource://mozmill/modules/frame.js', frame);
    frame.events.addListener('setTest', this.MozmillSetTestListener.bind(this));
    frame.events.addListener('endTest', this.MozmillEndTestListener.bind(this));
    this.StartAsyncOperation();
    frame.runTestFile(mozmillfile.path, false);
  },

  /**
   * Synchronously wait for the named event to be observed.
   *
   * When the event is observed, the function will wait an extra tick before
   * returning.
   *
   * @param name
   *        String event to wait for.
   */
  waitForEvent:function waitForEvent(name) {
    Logger.logInfo("Waiting for " + name + "...");
    let cb = Async.makeSpinningCallback();
    Svc.Obs.add(name, cb);
    cb.wait();
    Svc.Obs.remove(name, cb);
    Logger.logInfo(name + " observed!");

    let cb = Async.makeSpinningCallback();
    Utils.nextTick(cb);
    cb.wait();
  },

  /**
   * Waits for Sync to start tracking before returning.
   */
  waitForTracking: function waitForTracking() {
    if (!this._isTracking) {
      this.waitForEvent("weave:engine:start-tracking");
    }

    let cb = Async.makeSyncCallback();
    Utils.nextTick(cb);
    Async.waitForSyncCallback(cb);
  },

  /**
   * Reset the client and server to an empty/pure state.
   *
   * All data on the server is wiped and replaced with new keys and local
   * client data. The local client is configured such that it is in sync
   * with the server and ready to handle changes.
   *
   * This is typically called at the beginning of every test to set up a clean
   * slate.
   *
   * This executes synchronously and doesn't return until things are in a good
   * state.
   */
  ResetData: function ResetData() {
    this.Login(true);

    Weave.Service.login();
    Weave.Service.wipeServer();
    Weave.Service.resetClient();
    Weave.Service.login();

    this.waitForTracking();
  },

  Login: function Login(force) {
    if (this._loggedIn && !force) {
      return;
    }

    let account = this.config.account;
    if (!account) {
      this.DumperError("No account information found! Did you use a valid " +
                       "config file?");
      return;
    }

    if (account["serverURL"]) {
      Weave.Service.serverURL = account["serverURL"];
    }

    Logger.logInfo("Setting client credentials.");
    if (account["admin-secret"]) {
      // if admin-secret is specified, we'll dynamically create
      // a new sync account
      Weave.Svc.Prefs.set("admin-secret", account["admin-secret"]);
      let suffix = account["account-suffix"];
      Weave.Service.identity.account = "tps" + suffix + "@mozilla.com";
      Weave.Service.identity.basicPassword = "tps" + suffix + "tps" + suffix;
      Weave.Service.identity.syncKey = Weave.Utils.generatePassphrase();
      Weave.Service.createAccount(Weave.Service.identity.account,
                            Weave.Service.identity.basicPassword,
                            "dummy1", "dummy2");
    } else if (account["username"] && account["password"] &&
               account["passphrase"]) {
      Weave.Service.identity.account = account["username"];
      Weave.Service.identity.basicPassword = account["password"];
      Weave.Service.identity.syncKey = account["passphrase"];
    } else {
      this.DumpError("Must specify admin-secret, or " +
                     "username/password/passphrase in the config file");
      return;
    }

    Weave.Service.login();
    Logger.AssertEqual(Weave.Status.service, Weave.STATUS_OK, "Weave status not OK");
    Weave.Svc.Obs.notify("weave:service:setup-complete");
    this._loggedIn = true;

    this.waitForTracking();
  },

  Sync: function TPS__Sync(options) {
    Logger.logInfo("executing Sync " + (options ? options : ""));

    if (options == SYNC_WIPE_REMOTE) {
      Weave.Svc.Prefs.set("firstSync", "wipeRemote");
    }
    else if (options == SYNC_WIPE_CLIENT) {
      Weave.Svc.Prefs.set("firstSync", "wipeClient");
    }
    else if (options == SYNC_RESET_CLIENT) {
      Weave.Svc.Prefs.set("firstSync", "resetClient");
    }
    else if (options) {
      throw new Error("Unhandled options to Sync(): " + options);
    } else {
      Weave.Svc.Prefs.reset("firstSync");
    }

    this.Login(false);

    this._waitingForSync = true;
    this.StartAsyncOperation();

    Weave.Service.sync();
  },

  WipeServer: function TPS__WipeServer() {
    Logger.logInfo("WipeServer()");
    this.Login();
    Weave.Service.wipeServer();
  },

  /**
   * Action which ensures changes are being tracked before returning.
   */
  EnsureTracking: function EnsureTracking() {
    this.Login(false);
    this.waitForTracking();
  }
};

var Addons = {
  install: function Addons__install(addons) {
    TPS.HandleAddons(addons, ACTION_ADD);
  },
  setEnabled: function Addons__setEnabled(addons, state) {
    TPS.HandleAddons(addons, ACTION_SET_ENABLED, state);
  },
  uninstall: function Addons__uninstall(addons) {
    TPS.HandleAddons(addons, ACTION_DELETE);
  },
  verify: function Addons__verify(addons, state) {
    TPS.HandleAddons(addons, ACTION_VERIFY, state);
  },
  verifyNot: function Addons__verifyNot(addons) {
    TPS.HandleAddons(addons, ACTION_VERIFY_NOT);
  },
};

var Bookmarks = {
  add: function Bookmarks__add(bookmarks) {
    TPS.HandleBookmarks(bookmarks, ACTION_ADD);
  },
  modify: function Bookmarks__modify(bookmarks) {
    TPS.HandleBookmarks(bookmarks, ACTION_MODIFY);
  },
  delete: function Bookmarks__delete(bookmarks) {
    TPS.HandleBookmarks(bookmarks, ACTION_DELETE);
  },
  verify: function Bookmarks__verify(bookmarks) {
    TPS.HandleBookmarks(bookmarks, ACTION_VERIFY);
  },
  verifyNot: function Bookmarks__verifyNot(bookmarks) {
    TPS.HandleBookmarks(bookmarks, ACTION_VERIFY_NOT);
  }
};

var Formdata = {
  add: function Formdata__add(formdata) {
    this.HandleForms(formdata, ACTION_ADD);
  },
  delete: function Formdata__delete(formdata) {
    this.HandleForms(formdata, ACTION_DELETE);
  },
  verify: function Formdata__verify(formdata) {
    this.HandleForms(formdata, ACTION_VERIFY);
  },
  verifyNot: function Formdata__verifyNot(formdata) {
    this.HandleForms(formdata, ACTION_VERIFY_NOT);
  }
};

var History = {
  add: function History__add(history) {
    this.HandleHistory(history, ACTION_ADD);
  },
  delete: function History__delete(history) {
    this.HandleHistory(history, ACTION_DELETE);
  },
  verify: function History__verify(history) {
    this.HandleHistory(history, ACTION_VERIFY);
  },
  verifyNot: function History__verifyNot(history) {
    this.HandleHistory(history, ACTION_VERIFY_NOT);
  }
};

var Passwords = {
  add: function Passwords__add(passwords) {
    this.HandlePasswords(passwords, ACTION_ADD);
  },
  modify: function Passwords__modify(passwords) {
    this.HandlePasswords(passwords, ACTION_MODIFY);
  },
  delete: function Passwords__delete(passwords) {
    this.HandlePasswords(passwords, ACTION_DELETE);
  },
  verify: function Passwords__verify(passwords) {
    this.HandlePasswords(passwords, ACTION_VERIFY);
  },
  verifyNot: function Passwords__verifyNot(passwords) {
    this.HandlePasswords(passwords, ACTION_VERIFY_NOT);
  }
};

var Prefs = {
  modify: function Prefs__modify(prefs) {
    TPS.HandlePrefs(prefs, ACTION_MODIFY);
  },
  verify: function Prefs__verify(prefs) {
    TPS.HandlePrefs(prefs, ACTION_VERIFY);
  }
};

var Tabs = {
  add: function Tabs__add(tabs) {
    TPS.StartAsyncOperation();
    TPS.HandleTabs(tabs, ACTION_ADD);
  },
  verify: function Tabs__verify(tabs) {
    TPS.HandleTabs(tabs, ACTION_VERIFY);
  },
  verifyNot: function Tabs__verifyNot(tabs) {
    TPS.HandleTabs(tabs, ACTION_VERIFY_NOT);
  }
};

var Windows = {
  add: function Window__add(aWindow) {
    TPS.StartAsyncOperation();
    TPS.HandleWindows(aWindow, ACTION_ADD);
  },
};
