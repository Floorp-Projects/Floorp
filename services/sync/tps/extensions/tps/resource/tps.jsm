/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* This is a JavaScript module (JSM) to be imported via
  * Components.utils.import() and acts as a singleton. Only the following
  * listed symbols will exposed on import, and only when and where imported.
  */

var EXPORTED_SYMBOLS = ["ACTIONS", "TPS"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

var module = this;

// Global modules
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/bookmark_validator.js");
// TPS modules
Cu.import("resource://tps/logger.jsm");

// Module wrappers for tests
Cu.import("resource://tps/modules/addons.jsm");
Cu.import("resource://tps/modules/bookmarks.jsm");
Cu.import("resource://tps/modules/forms.jsm");
Cu.import("resource://tps/modules/history.jsm");
Cu.import("resource://tps/modules/passwords.jsm");
Cu.import("resource://tps/modules/prefs.jsm");
Cu.import("resource://tps/modules/tabs.jsm");
Cu.import("resource://tps/modules/windows.jsm");

var hh = Cc["@mozilla.org/network/protocol;1?name=http"]
         .getService(Ci.nsIHttpProtocolHandler);
var prefs = Cc["@mozilla.org/preferences-service;1"]
            .getService(Ci.nsIPrefBranch);

var mozmillInit = {};
Cu.import('resource://mozmill/driver/mozmill.js', mozmillInit);

// Options for wiping data during a sync
const SYNC_RESET_CLIENT = "resetClient";
const SYNC_WIPE_CLIENT  = "wipeClient";
const SYNC_WIPE_REMOTE  = "wipeRemote";

// Actions a test can perform
const ACTION_ADD                = "add";
const ACTION_DELETE             = "delete";
const ACTION_MODIFY             = "modify";
const ACTION_PRIVATE_BROWSING   = "private-browsing";
const ACTION_SET_ENABLED        = "set-enabled";
const ACTION_SYNC               = "sync";
const ACTION_SYNC_RESET_CLIENT  = SYNC_RESET_CLIENT;
const ACTION_SYNC_WIPE_CLIENT   = SYNC_WIPE_CLIENT;
const ACTION_SYNC_WIPE_REMOTE   = SYNC_WIPE_REMOTE;
const ACTION_VERIFY             = "verify";
const ACTION_VERIFY_NOT         = "verify-not";

const ACTIONS = [
  ACTION_ADD,
  ACTION_DELETE,
  ACTION_MODIFY,
  ACTION_PRIVATE_BROWSING,
  ACTION_SET_ENABLED,
  ACTION_SYNC,
  ACTION_SYNC_RESET_CLIENT,
  ACTION_SYNC_WIPE_CLIENT,
  ACTION_SYNC_WIPE_REMOTE,
  ACTION_VERIFY,
  ACTION_VERIFY_NOT,
];

const OBSERVER_TOPICS = ["fxaccounts:onlogin",
                         "fxaccounts:onlogout",
                         "private-browsing",
                         "quit-application-requested",
                         "sessionstore-windows-restored",
                         "weave:engine:start-tracking",
                         "weave:engine:stop-tracking",
                         "weave:service:login:error",
                         "weave:service:setup-complete",
                         "weave:service:sync:finish",
                         "weave:service:sync:delayed",
                         "weave:service:sync:error",
                         "weave:service:sync:start"
                        ];

var TPS = {
  _currentAction: -1,
  _currentPhase: -1,
  _enabledEngines: null,
  _errors: 0,
  _isTracking: false,
  _operations_pending: 0,
  _phaseFinished: false,
  _phaselist: {},
  _setupComplete: false,
  _syncActive: false,
  _syncErrors: 0,
  _syncWipeAction: null,
  _tabsAdded: 0,
  _tabsFinished: 0,
  _test: null,
  _triggeredSync: false,
  _usSinceEpoch: 0,
  _requestedQuit: false,
  shouldValidateBookmarks: false,

  _init: function TPS__init() {
    // Check if Firefox Accounts is enabled
    let service = Cc["@mozilla.org/weave/service;1"]
                  .getService(Components.interfaces.nsISupports)
                  .wrappedJSObject;
    this.fxaccounts_enabled = service.fxAccountsEnabled;

    this.delayAutoSync();

    OBSERVER_TOPICS.forEach(function (aTopic) {
      Services.obs.addObserver(this, aTopic, true);
    }, this);

    // Configure some logging prefs for Sync itself.
    Weave.Svc.Prefs.set("log.appender.dump", "Debug");
    // Import the appropriate authentication module
    if (this.fxaccounts_enabled) {
      Cu.import("resource://tps/auth/fxaccounts.jsm", module);
    }
    else {
      Cu.import("resource://tps/auth/sync.jsm", module);
    }
  },

  DumpError(msg, exc = null) {
    this._errors++;
    let errInfo;
    if (exc) {
      errInfo = Log.exceptionStr(exc); // includes details and stack-trace.
    } else {
      // always write a stack even if no error passed.
      errInfo = Log.stackTrace(new Error());
    }
    Logger.logError(`[phase ${this._currentPhase}] ${msg} - ${errInfo}`);
    this.quit();
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function TPS__observe(subject, topic, data) {
    try {
      Logger.logInfo("----------event observed: " + topic);

      switch(topic) {
        case "private-browsing":
          Logger.logInfo("private browsing " + data);
          break;

        case "quit-application-requested":
          OBSERVER_TOPICS.forEach(function(topic) {
            Services.obs.removeObserver(this, topic);
          }, this);

          Logger.close();

          break;

        case "sessionstore-windows-restored":
          Utils.nextTick(this.RunNextTestAction, this);
          break;

        case "weave:service:setup-complete":
          this._setupComplete = true;

          if (this._syncWipeAction) {
            Weave.Svc.Prefs.set("firstSync", this._syncWipeAction);
            this._syncWipeAction = null;
          }

          break;

        case "weave:service:sync:error":
          this._syncActive = false;

          this.delayAutoSync();

          // If this is the first sync error, retry...
          if (this._syncErrors === 0) {
            Logger.logInfo("Sync error; retrying...");
            this._syncErrors++;
            Utils.nextTick(this.RunNextTestAction, this);
          }
          else {
            this._triggeredSync = false;
            this.DumpError("Sync error; aborting test");
            return;
          }

          break;

        case "weave:service:sync:finish":
          this._syncActive = false;
          this._syncErrors = 0;
          this._triggeredSync = false;

          this.delayAutoSync();

          // Wait a second before continuing, otherwise we can get
          // 'sync not complete' errors.
          Utils.namedTimer(function () {
            this.FinishAsyncOperation();
          }, 1000, this, "postsync");

          break;

        case "weave:service:sync:start":
          // Ensure that the sync operation has been started by TPS
          if (!this._triggeredSync) {
            this.DumpError("Automatic sync got triggered, which is not allowed.")
          }

          this._syncActive = true;
          break;

        case "weave:engine:start-tracking":
          this._isTracking = true;
          break;

        case "weave:engine:stop-tracking":
          this._isTracking = false;
          break;
      }
    }
    catch (e) {
      this.DumpError("Observer failed", e);
      return;
    }
  },

  /**
   * Given that we cannot complely disable the automatic sync operations, we
   * massively delay the next sync. Sync operations have to only happen when
   * directly called via TPS.Sync()!
   */
  delayAutoSync: function TPS_delayAutoSync() {
    Weave.Svc.Prefs.set("scheduler.eolInterval", 7200);
    Weave.Svc.Prefs.set("scheduler.immediateInterval", 7200);
    Weave.Svc.Prefs.set("scheduler.idleInterval", 7200);
    Weave.Svc.Prefs.set("scheduler.activeInterval", 7200);
    Weave.Svc.Prefs.set("syncThreshold", 10000000);
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

  quit: function TPS__quit() {
    this._requestedQuit = true;
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
    for (let tab of tabs) {
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

              // Wait a second before continuing to be sure tabs can be synced,
              // otherwise we can get 'error locating tab'
              Utils.namedTimer(function () {
                that.FinishAsyncOperation();
              }, 1000, this, "postTabsOpening");
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
    for (let pref of prefs) {
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
    for (let datum of data) {
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
      for (let entry of entries) {
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
      for (let password of passwords) {
        let password_id = -1;
        Logger.logInfo("executing action " + action.toUpperCase() +
                      " on password " + JSON.stringify(password));
        let passwordOb = new Password(password);
        switch (action) {
          case ACTION_ADD:
            Logger.AssertTrue(passwordOb.Create() > -1, "error adding password");
            break;
          case ACTION_VERIFY:
            Logger.AssertTrue(passwordOb.Find() != -1, "password not found");
            break;
          case ACTION_VERIFY_NOT:
            Logger.AssertTrue(passwordOb.Find() == -1,
              "password found, but it shouldn't exist");
            break;
          case ACTION_DELETE:
            Logger.AssertTrue(passwordOb.Find() != -1, "password not found");
            passwordOb.Remove();
            break;
          case ACTION_MODIFY:
            if (passwordOb.updateProps != null) {
              Logger.AssertTrue(passwordOb.Find() != -1, "password not found");
              passwordOb.Update();
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
    for (let entry of addons) {
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
    this.shouldValidateBookmarks = true;
    try {
      let items = [];
      for (let folder in bookmarks) {
        let last_item_pos = -1;
        for (let bookmark of bookmarks[folder]) {
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
        for (let item of items) {
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
    catch (e) {
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

  Cleanup() {
    try {
      this.WipeServer();
    } catch (ex) {
      Logger.logError("Failed to wipe server: " + Log.exceptionStr(ex));
    }
    try {
      Authentication.signOut();
    } catch (e) {
      Logger.logError("Failed to sign out: " + Log.exceptionStr(e));
    }
  },

  /**
   * Use Sync's bookmark validation code to see if we've corrupted the tree.
   */
  ValidateBookmarks() {

    let getServerBookmarkState = () => {
      let bookmarkEngine = Weave.Service.engineManager.get('bookmarks');
      let collection = bookmarkEngine._itemSource();
      let collectionKey = bookmarkEngine.service.collectionKeys.keyForCollection(bookmarkEngine.name);
      collection.full = true;
      let items = [];
      collection.recordHandler = function(item) {
        item.decrypt(collectionKey);
        items.push(item.cleartext);
      };
      collection.get();
      return items;
    };
    let serverRecordDumpStr;
    try {
      Logger.logInfo("About to perform bookmark validation");
      let clientTree = Async.promiseSpinningly(PlacesUtils.promiseBookmarksTree("", {
        includeItemIds: true
      }));
      let serverRecords = getServerBookmarkState();
      // We can't wait until catch to stringify this, since at that point it will have cycles.
      serverRecordDumpStr = JSON.stringify(serverRecords);

      let validator = new BookmarkValidator();
      let {problemData} = validator.compareServerWithClient(serverRecords, clientTree);

      for (let {name, count} of problemData.getSummary()) {
        // Exclude mobile showing up on the server hackily so that we don't
        // report it every time, see bug 1273234 and 1274394 for more information.
        if (name === "serverUnexpected" && problemData.serverUnexpected.indexOf("mobile") >= 0) {
          --count;
        } else if (name === "differences") {
          // Also exclude errors in parentName/wrongParentName (bug 1276969) for
          // the same reason.
          let newCount = problemData.differences.filter(diffInfo =>
            !diffInfo.differences.every(diff =>
              diff === "parentName")).length
          count = newCount;
        } else if (name === "wrongParentName") {
          continue;
        }
        if (count) {
          // Log this out before we assert. This is useful in the context of TPS logs, since we
          // can see the IDs in the test files.
          Logger.logInfo(`Validation problem: "${name}": ${JSON.stringify(problemData[name])}`);
        }
        Logger.AssertEqual(count, 0, `Bookmark validation error of type ${name}`);
      }
    } catch (e) {
      // Dump the client records (should always be doable)
      DumpBookmarks();
      // Dump the server records if gotten them already.
      if (serverRecordDumpStr) {
        Logger.logInfo("Server bookmark records:\n" + serverRecordDumpStr + "\n");
      }
      this.DumpError("Bookmark validation failed", e);
    }
    Logger.logInfo("Bookmark validation finished");
  },

  RunNextTestAction: function() {
    try {
      if (this._currentAction >=
          this._phaselist[this._currentPhase].length) {
        if (this.shouldValidateBookmarks) {
          // Run bookmark validation and then finish up
          this.ValidateBookmarks();
        }
        // we're all done
        Logger.logInfo("test phase " + this._currentPhase + ": " +
                       (this._errors ? "FAIL" : "PASS"));
        this._phaseFinished = true;
        this.quit();
        return;
      }

      if (this.seconds_since_epoch)
        this._usSinceEpoch = this.seconds_since_epoch * 1000 * 1000;
      else {
        this.DumpError("seconds-since-epoch not set");
        return;
      }

      let phase = this._phaselist[this._currentPhase];
      let action = phase[this._currentAction];
      Logger.logInfo("starting action: " + action[0].name);
      action[0].apply(this, action.slice(1));

      // if we're in an async operation, don't continue on to the next action
      if (this._operations_pending)
        return;

      this._currentAction++;
    }
    catch(e) {
      if (Async.isShutdownException(e)) {
        if (this._requestedQuit) {
          Logger.logInfo("Sync aborted due to requested shutdown");
        } else {
          this.DumpError("Sync aborted due to shutdown, but we didn't request it");
        }
      } else {
        this.DumpError("RunNextTestAction failed", e);
      }
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
      Logger.logInfo("Firefox buildid: " + Services.appinfo.appBuildID);
      Logger.logInfo("Firefox version: " + Services.appinfo.version);
      Logger.logInfo("Firefox source revision: " + (AppConstants.SOURCE_REVISION_URL || "unknown"));
      Logger.logInfo("Firefox platform: " + AppConstants.platform);
      Logger.logInfo('Firefox Accounts enabled: ' + this.fxaccounts_enabled);

      // do some sync housekeeping
      if (Weave.Service.isLoggedIn) {
        this.DumpError("Sync logged in on startup...profile may be dirty");
        return;
      }

      // Wait for Sync service to become ready.
      if (!Weave.Status.ready) {
        this.waitForEvent("weave:service:ready");
      }

      // We only want to do this if we modified the bookmarks this phase.
      this.shouldValidateBookmarks = false;

      // Always give Sync an extra tick to initialize. If we waited for the
      // service:ready event, this is required to ensure all handlers have
      // executed.
      Utils.nextTick(this._executeTestPhase.bind(this, file, phase, settings));
    } catch(e) {
      this.DumpError("RunTestPhase failed", e);
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
      // parse the test file
      Services.scriptloader.loadSubScript(file, this);
      this._currentPhase = phase;
      if (this._currentPhase.startsWith("cleanup-")) {
        let profileToClean = Cc["@mozilla.org/toolkit/profile-service;1"]
                             .getService(Ci.nsIToolkitProfileService)
                             .selectedProfile.name;
        this.phases[this._currentPhase] = profileToClean;
        this.Phase(this._currentPhase, [[this.Cleanup]]);
      }
      let this_phase = this._phaselist[this._currentPhase];

      if (this_phase == undefined) {
        this.DumpError("invalid phase " + this._currentPhase);
        return;
      }

      if (this.phases[this._currentPhase] == undefined) {
        this.DumpError("no profile defined for phase " + this._currentPhase);
        return;
      }

      // If we have restricted the active engines, unregister engines we don't
      // care about.
      if (settings.ignoreUnusedEngines && Array.isArray(this._enabledEngines)) {
        let names = {};
        for (let name of this._enabledEngines) {
          names[name] = true;
        }

        for (let engine of Weave.Service.engineManager.getEnabled()) {
          if (!(engine.name in names)) {
            Logger.logInfo("Unregistering unused engine: " + engine.name);
            Weave.Service.engineManager.unregister(engine);
          }
        }
      }
      Logger.logInfo("Starting phase " + this._currentPhase);

      Logger.logInfo("setting client.name to " + this.phases[this._currentPhase]);
      Weave.Svc.Prefs.set("client.name", this.phases[this._currentPhase]);

      // If a custom server was specified, set it now
      if (this.config["serverURL"]) {
        Weave.Service.serverURL = this.config.serverURL;
        prefs.setCharPref('tps.serverURL', this.config.serverURL);
      }

      // Store account details as prefs so they're accessible to the Mozmill
      // framework.
      if (this.fxaccounts_enabled) {
        prefs.setCharPref('tps.account.username', this.config.fx_account.username);
        prefs.setCharPref('tps.account.password', this.config.fx_account.password);
      }
      else {
        prefs.setCharPref('tps.account.username', this.config.sync_account.username);
        prefs.setCharPref('tps.account.password', this.config.sync_account.password);
        prefs.setCharPref('tps.account.passphrase', this.config.sync_account.passphrase);
      }

      // start processing the test actions
      this._currentAction = 0;
    }
    catch(e) {
      this.DumpError("_executeTestPhase failed", e);
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
    if (Object.keys(this._phaselist).length === 0) {
      // This is the first phase, add that we need to login.
      fnlist.unshift([this.Login]);
    }
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
    var mozmillfile = Cc["@mozilla.org/file/local;1"]
                      .createInstance(Ci.nsILocalFile);
    if (hh.oscpu.toLowerCase().indexOf('windows') > -1) {
      let re = /\/(\w)\/(.*)/;
      this.config.testdir = this.config.testdir.replace(re, "$1://$2").replace(/\//g, "\\");
    }
    mozmillfile.initWithPath(this.config.testdir);
    mozmillfile.appendRelativePath(testfile);
    Logger.logInfo("Running mozmill test " + mozmillfile.path);

    var frame = {};
    Cu.import('resource://mozmill/modules/frame.js', frame);
    frame.events.addListener('setTest', this.MozmillSetTestListener.bind(this));
    frame.events.addListener('endTest', this.MozmillEndTestListener.bind(this));
    this.StartAsyncOperation();
    frame.runTestFile(mozmillfile.path, null);
  },

  /**
   * Synchronously wait for the named event to be observed.
   *
   * When the event is observed, the function will wait an extra tick before
   * returning.
   *
   * @param aEventName
   *        String event to wait for.
   */
  waitForEvent: function waitForEvent(aEventName) {
    Logger.logInfo("Waiting for " + aEventName + "...");
    let cb = Async.makeSpinningCallback();
    Svc.Obs.add(aEventName, cb);
    cb.wait();
    Svc.Obs.remove(aEventName, cb);
    Logger.logInfo(aEventName + " observed!");
  },


  /**
   * Waits for Sync to logged in before returning
   */
  waitForSetupComplete: function waitForSetup() {
    if (!this._setupComplete) {
      this.waitForEvent("weave:service:setup-complete");
    }
  },

  /**
   * Waits for Sync to be finished before returning
   */
  waitForSyncFinished: function TPS__waitForSyncFinished() {
    if (this._syncActive) {
      this.waitForEvent("weave:service:sync:finished");
    }
  },

  /**
   * Waits for Sync to start tracking before returning.
   */
  waitForTracking: function waitForTracking() {
    if (!this._isTracking) {
      this.waitForEvent("weave:engine:start-tracking");
    }
  },

  /**
   * Login on the server
   */
  Login: function Login(force) {
    if (Authentication.isLoggedIn && !force) {
      return;
    }

    Logger.logInfo("Setting client credentials and login.");
    let account = this.fxaccounts_enabled ? this.config.fx_account
                                          : this.config.sync_account;
    Authentication.signIn(account);
    this.waitForSetupComplete();
    Logger.AssertEqual(Weave.Status.service, Weave.STATUS_OK, "Weave status OK");
    this.waitForTracking();
    // If fxaccounts is enabled we get an initial sync at login time - let
    // that complete.
    if (this.fxaccounts_enabled) {
      this._triggeredSync = true;
      this.waitForSyncFinished();
    }
  },

  /**
   * Triggers a sync operation
   *
   * @param {String} [wipeAction]
   *        Type of wipe to perform (resetClient, wipeClient, wipeRemote)
   *
   */
  Sync: function TPS__Sync(wipeAction) {
    Logger.logInfo("Executing Sync" + (wipeAction ? ": " + wipeAction : ""));

    // Force a wipe action if requested. In case of an initial sync the pref
    // will be overwritten by Sync itself (see bug 992198), so ensure that we
    // also handle it via the "weave:service:setup-complete" notification.
    if (wipeAction) {
      this._syncWipeAction = wipeAction;
      Weave.Svc.Prefs.set("firstSync", wipeAction);
    }
    else {
      Weave.Svc.Prefs.reset("firstSync");
    }

    this.Login(false);

    this._triggeredSync = true;
    this.StartAsyncOperation();
    Weave.Service.sync();
    Logger.logInfo("Sync is complete");
  },

  WipeServer: function TPS__WipeServer() {
    Logger.logInfo("Wiping data from server.");

    this.Login(false);
    Weave.Service.login();
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
  },
  skipValidation() {
    TPS.shouldValidateBookmarks = false;
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

// Initialize TPS
TPS._init();
