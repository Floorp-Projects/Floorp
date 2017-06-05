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
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/telemetry.js");
Cu.import("resource://services-sync/bookmark_validator.js");
Cu.import("resource://services-sync/engines/passwords.js");
Cu.import("resource://services-sync/engines/forms.js");
Cu.import("resource://services-sync/engines/addons.js");
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
Cu.import("resource://mozmill/driver/mozmill.js", mozmillInit);

XPCOMUtils.defineLazyGetter(this, "fileProtocolHandler", () => {
  let fileHandler = Services.io.getProtocolHandler("file");
  return fileHandler.QueryInterface(Ci.nsIFileProtocolHandler);
});

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
                         "profile-before-change",
                         "sessionstore-windows-restored",
                         "weave:engine:start-tracking",
                         "weave:engine:stop-tracking",
                         "weave:service:login:error",
                         "weave:service:setup-complete",
                         "weave:service:sync:finish",
                         "weave:service:sync:delayed",
                         "weave:service:sync:error",
                         "weave:service:sync:start",
                         "weave:service:resyncs-finished",
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
  _syncCount: 0,
  _syncsReportedViaTelemetry: 0,
  _syncErrors: 0,
  _syncWipeAction: null,
  _tabsAdded: 0,
  _tabsFinished: 0,
  _test: null,
  _triggeredSync: false,
  _usSinceEpoch: 0,
  _requestedQuit: false,
  shouldValidateAddons: false,
  shouldValidateBookmarks: false,
  shouldValidatePasswords: false,
  shouldValidateForms: false,

  _init: function TPS__init() {
    this.delayAutoSync();

    OBSERVER_TOPICS.forEach(function(aTopic) {
      Services.obs.addObserver(this, aTopic, true);
    }, this);

    /* global Authentication */
    Cu.import("resource://tps/auth/fxaccounts.jsm", module);
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

      switch (topic) {
        case "private-browsing":
          Logger.logInfo("private browsing " + data);
          break;

        case "profile-before-change":
          OBSERVER_TOPICS.forEach(function(topic) {
            Services.obs.removeObserver(this, topic);
          }, this);

          Logger.close();

          break;

        case "sessionstore-windows-restored":
          // This is a terrible hack, but fixes cases where tps (usually cleanup)
          // fails because of sessionstore restoring windows before tps is able
          // to initialize. This used to take only 1 tick, but at some point
          // session store started restoring windows sooner, or tps started
          // initializing later.
          setTimeout(() => {
            this.RunNextTestAction();
          }, 1000);
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
          } else {
            this._triggeredSync = false;
            this.DumpError("Sync error; aborting test");
            return;
          }

          break;

        case "weave:service:resyncs-finished":
          this._syncActive = false;
          this._syncErrors = 0;
          this._triggeredSync = false;

          this.delayAutoSync();

          // Wait a second before continuing, otherwise we can get
          // 'sync not complete' errors.
          Utils.namedTimer(function() {
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
    } catch (e) {
      this.DumpError("Observer failed", e);

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
    // We fire a FinishAsyncOperation at the end of a sync finish (somewhat
    // dubious, but we're consistent about it), but a sync may or may not be
    // triggered during login, and it's hard for us to know (without e.g.
    // auth/fxaccounts.jsm calling back into this module). So we just assume
    // the FinishAsyncOperation without a StartAsyncOperation is fine, and works
    // as if a StartAsyncOperation had been called.
    if (this._operations_pending) {
      this._operations_pending--;
    }
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

  HandleWindows(aWindow, action) {
    Logger.logInfo("executing action " + action.toUpperCase() +
                   " on window " + JSON.stringify(aWindow));
    switch (action) {
      case ACTION_ADD:
        BrowserWindows.Add(aWindow.private, win => {
          Logger.logInfo("window finished loading");
          this.FinishAsyncOperation();
        });
        break;
    }
    Logger.logPass("executing action " + action.toUpperCase() + " on windows");
  },

  HandleTabs(tabs, action) {
    this._tabsAdded = tabs.length;
    this._tabsFinished = 0;
    for (let tab of tabs) {
      Logger.logInfo("executing action " + action.toUpperCase() +
                     " on tab " + JSON.stringify(tab));
      switch (action) {
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
              Utils.namedTimer(function() {
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

  HandlePrefs(prefs, action) {
    for (let pref of prefs) {
      Logger.logInfo("executing action " + action.toUpperCase() +
                     " on pref " + JSON.stringify(pref));
      let preference = new Preference(pref);
      switch (action) {
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

  HandleForms(data, action) {
    this.shouldValidateForms = true;
    for (let datum of data) {
      Logger.logInfo("executing action " + action.toUpperCase() +
                     " on form entry " + JSON.stringify(datum));
      let formdata = new FormData(datum, this._usSinceEpoch);
      switch (action) {
        case ACTION_ADD:
          Async.promiseSpinningly(formdata.Create());
          break;
        case ACTION_DELETE:
          Async.promiseSpinningly(formdata.Remove());
          break;
        case ACTION_VERIFY:
          Logger.AssertTrue(Async.promiseSpinningly(formdata.Find()),
                            "form data not found");
          break;
        case ACTION_VERIFY_NOT:
          Logger.AssertTrue(!Async.promiseSpinningly(formdata.Find()),
                            "form data found, but it shouldn't be present");
          break;
        default:
          Logger.AssertTrue(false, "invalid action: " + action);
      }
    }
    Logger.logPass("executing action " + action.toUpperCase() +
                   " on formdata");
  },

  HandleHistory(entries, action) {
    try {
      for (let entry of entries) {
        Logger.logInfo("executing action " + action.toUpperCase() +
                       " on history entry " + JSON.stringify(entry));
        switch (action) {
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
    } catch (e) {
      DumpHistory();
      throw (e);
    }
  },

  HandlePasswords(passwords, action) {
    this.shouldValidatePasswords = true;
    try {
      for (let password of passwords) {
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
    } catch (e) {
      DumpPasswords();
      throw (e);
    }
  },

  HandleAddons(addons, action, state) {
    this.shouldValidateAddons = true;
    for (let entry of addons) {
      Logger.logInfo("executing action " + action.toUpperCase() +
                     " on addon " + JSON.stringify(entry));
      let addon = new Addon(this, entry);
      switch (action) {
        case ACTION_ADD:
          addon.install();
          break;
        case ACTION_DELETE:
          addon.uninstall();
          break;
        case ACTION_VERIFY:
          Logger.AssertTrue(addon.find(state), "addon " + addon.id + " not found");
          break;
        case ACTION_VERIFY_NOT:
          Logger.AssertFalse(addon.find(state), "addon " + addon.id + " is present, but it shouldn't be");
          break;
        case ACTION_SET_ENABLED:
          Logger.AssertTrue(addon.setEnabled(state), "addon " + addon.id + " not found");
          break;
        default:
          throw new Error("Unknown action for add-on: " + action);
      }
    }
    Logger.logPass("executing action " + action.toUpperCase() +
                   " on addons");
  },

  HandleBookmarks(bookmarks, action) {
    this.shouldValidateBookmarks = true;
    try {
      let items = [];
      for (let folder in bookmarks) {
        let last_item_pos = -1;
        for (let bookmark of bookmarks[folder]) {
          Logger.clearPotentialError();
          let placesItem;
          bookmark["location"] = folder;

          if (last_item_pos != -1)
            bookmark["last_item_pos"] = last_item_pos;
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
          } else {
            item_id = placesItem.Find();
            if (action == ACTION_VERIFY_NOT) {
              Logger.AssertTrue(item_id == -1,
                "places item exists but it shouldn't: " +
                JSON.stringify(bookmark));
            } else
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
          switch (action) {
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
    } catch (e) {
      DumpBookmarks();
      throw (e);
    }
  },

  MozmillEndTestListener: function TPS__MozmillEndTestListener(obj) {
    Logger.logInfo("mozmill endTest: " + JSON.stringify(obj));
    if (obj.failed > 0) {
      this.DumpError("mozmill test failed, name: " + obj.name + ", reason: " + JSON.stringify(obj.fails));
    } else if ("skipped" in obj && obj.skipped) {
      this.DumpError("mozmill test failed, name: " + obj.name + ", reason: " + obj.skipped_reason);
    } else {
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
      if (Authentication.isLoggedIn) {
        // signout and wait for Sync to completely reset itself.
        Logger.logInfo("signing out");
        let waiter = this.createEventWaiter("weave:service:start-over:finish");
        Authentication.signOut();
        waiter();
        Logger.logInfo("signout complete");
      }
      Authentication.deleteEmail(this.config.fx_account.username);
    } catch (e) {
      Logger.logError("Failed to sign out: " + Log.exceptionStr(e));
    }
  },

  /**
   * Use Sync's bookmark validation code to see if we've corrupted the tree.
   */
  ValidateBookmarks() {

    let getServerBookmarkState = async () => {
      let bookmarkEngine = Weave.Service.engineManager.get("bookmarks");
      let collection = bookmarkEngine.itemSource();
      let collectionKey = bookmarkEngine.service.collectionKeys.keyForCollection(bookmarkEngine.name);
      collection.full = true;
      let items = [];
      let resp = await collection.get();
      for (let json of resp.obj) {
        let record = new collection._recordObj();
        record.deserialize(json);
        record.decrypt(collectionKey);
        items.push(record.cleartext);
      }
      return items;
    };
    let serverRecordDumpStr;
    try {
      Logger.logInfo("About to perform bookmark validation");
      let clientTree = Async.promiseSpinningly(PlacesUtils.promiseBookmarksTree("", {
        includeItemIds: true
      }));
      let serverRecords = Async.promiseSpinningly(getServerBookmarkState());
      // We can't wait until catch to stringify this, since at that point it will have cycles.
      serverRecordDumpStr = JSON.stringify(serverRecords);

      let validator = new BookmarkValidator();
      let {problemData} = Async.promiseSpinningly(validator.compareServerWithClient(serverRecords, clientTree));

      for (let {name, count} of problemData.getSummary()) {
        // Exclude mobile showing up on the server hackily so that we don't
        // report it every time, see bug 1273234 and 1274394 for more information.
        if (name === "serverUnexpected" && problemData.serverUnexpected.indexOf("mobile") >= 0) {
          --count;
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

  ValidateCollection(engineName, ValidatorType) {
    let serverRecordDumpStr;
    let clientRecordDumpStr;
    try {
      Logger.logInfo(`About to perform validation for "${engineName}"`);
      let engine = Weave.Service.engineManager.get(engineName);
      let validator = new ValidatorType(engine);
      let serverRecords = Async.promiseSpinningly(validator.getServerItems(engine));
      let clientRecords = Async.promiseSpinningly(validator.getClientItems());
      try {
        // This substantially improves the logs for addons while not making a
        // substantial difference for the other two
        clientRecordDumpStr = JSON.stringify(clientRecords.map(r => {
          let res = validator.normalizeClientItem(r);
          delete res.original; // Try and prevent cyclic references
          return res;
        }));
      } catch (e) {
        // ignore the error, the dump string is just here to make debugging easier.
        clientRecordDumpStr = "<Cyclic value>";
      }
      try {
        serverRecordDumpStr = JSON.stringify(serverRecords);
      } catch (e) {
        // as above
        serverRecordDumpStr = "<Cyclic value>";
      }
      let { problemData } = Async.promiseSpinningly(validator.compareClientWithServer(clientRecords, serverRecords));
      for (let { name, count } of problemData.getSummary()) {
        if (count) {
          Logger.logInfo(`Validation problem: "${name}": ${JSON.stringify(problemData[name])}`);
        }
        Logger.AssertEqual(count, 0, `Validation error for "${engineName}" of type "${name}"`);
      }
    } catch (e) {
      // Dump the client records if possible
      if (clientRecordDumpStr) {
        Logger.logInfo(`Client state for ${engineName}:\n${clientRecordDumpStr}\n`);
      }
      // Dump the server records if gotten them already.
      if (serverRecordDumpStr) {
        Logger.logInfo(`Server state for ${engineName}:\n${serverRecordDumpStr}\n`);
      }
      this.DumpError(`Validation failed for ${engineName}`, e);
    }
    Logger.logInfo(`Validation finished for ${engineName}`);
  },

  ValidatePasswords() {
    return this.ValidateCollection("passwords", PasswordValidator);
  },

  ValidateForms() {
    return this.ValidateCollection("forms", FormValidator);
  },

  ValidateAddons() {
    return this.ValidateCollection("addons", AddonValidator);
  },

  RunNextTestAction() {
    try {
      if (this._currentAction >=
          this._phaselist[this._currentPhase].length) {
        // Run necessary validations and then finish up
        if (this.shouldValidateBookmarks) {
          this.ValidateBookmarks();
        }
        if (this.shouldValidatePasswords) {
          this.ValidatePasswords();
        }
        if (this.shouldValidateForms) {
          this.ValidateForms();
        }
        if (this.shouldValidateAddons) {
          this.ValidateAddons();
        }
        // Force this early so that we run the validation and detect missing pings
        // *before* we start shutting down, since if we do it after, the python
        // code won't notice the failure.
        SyncTelemetry.shutdown();
        // we're all done
        Logger.logInfo("test phase " + this._currentPhase + ": " +
                       (this._errors ? "FAIL" : "PASS"));
        this._phaseFinished = true;
        this.quit();
        return;
      }
      this.seconds_since_epoch = prefs.getIntPref("tps.seconds_since_epoch");
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
    } catch (e) {
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

  _getFileRelativeToSourceRoot(testFileURL, relativePath) {
    let file = fileProtocolHandler.getFileFromURLSpec(testFileURL);
    let root = file // <root>/services/sync/tests/tps/test_foo.js
      .parent // <root>/services/sync/tests/tps
      .parent // <root>/services/sync/tests
      .parent // <root>/services/sync
      .parent // <root>/services
      .parent // <root>
      ;
    root.appendRelativePath(relativePath);
    return root;
  },

  // Attempt to load the sync_ping_schema.json and initialize `this.pingValidator`
  // based on the source of the tps file. Assumes that it's at "../unit/sync_ping_schema.json"
  // relative to the directory the tps test file (testFile) is contained in.
  _tryLoadPingSchema(testFile) {
    try {
      let schemaFile = this._getFileRelativeToSourceRoot(testFile,
        "services/sync/tests/unit/sync_ping_schema.json");

      let stream = Cc["@mozilla.org/network/file-input-stream;1"]
                   .createInstance(Ci.nsIFileInputStream);

      let jsonReader = Cc["@mozilla.org/dom/json;1"]
                       .createInstance(Components.interfaces.nsIJSON);

      stream.init(schemaFile, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
      let schema = jsonReader.decodeFromStream(stream, stream.available());
      Logger.logInfo("Successfully loaded schema")

      // Importing resource://testing-common/* isn't possible from within TPS,
      // so we load Ajv manually.
      let ajvFile = this._getFileRelativeToSourceRoot(testFile, "testing/modules/ajv-4.1.1.js");
      let ajvURL = fileProtocolHandler.getURLSpecFromFile(ajvFile);
      let ns = {};
      Cu.import(ajvURL, ns);
      let ajv = new ns.Ajv({ async: "co*" });
      this.pingValidator = ajv.compile(schema);
    } catch (e) {
      this.DumpError(`Failed to load ping schema and AJV relative to "${testFile}".`, e);
    }
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
  RunTestPhase(file, phase, logpath, options) {
    try {
      let settings = options || {};

      Logger.init(logpath);
      Logger.logInfo("Sync version: " + WEAVE_VERSION);
      Logger.logInfo("Firefox buildid: " + Services.appinfo.appBuildID);
      Logger.logInfo("Firefox version: " + Services.appinfo.version);
      Logger.logInfo("Firefox source revision: " + (AppConstants.SOURCE_REVISION_URL || "unknown"));
      Logger.logInfo("Firefox platform: " + AppConstants.platform);

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
    } catch (e) {
      this.DumpError("RunTestPhase failed", e);
    }
  },

  /**
   * Executes a single test phase.
   *
   * This is called by RunTestPhase() after the environment is validated.
   */
  _executeTestPhase: function _executeTestPhase(file, phase, settings) {
    try {
      this.config = JSON.parse(prefs.getCharPref("tps.config"));
      // parse the test file
      Services.scriptloader.loadSubScript(file, this);
      this._currentPhase = phase;
      // cleanup phases are in the format `cleanup-${profileName}`.
      if (this._currentPhase.startsWith("cleanup-")) {
        let profileToClean = this._currentPhase.slice("cleanup-".length);
        this.phases[this._currentPhase] = profileToClean;
        this.Phase(this._currentPhase, [[this.Cleanup]]);
      } else {
        // Don't bother doing this for cleanup phases.
        this._tryLoadPingSchema(file);
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

      this._interceptSyncTelemetry();

      // start processing the test actions
      this._currentAction = 0;
    } catch (e) {
      this.DumpError("_executeTestPhase failed", e);
    }
  },

  /**
   * Override sync telemetry functions so that we can detect errors generating
   * the sync ping, and count how many pings we report.
   */
  _interceptSyncTelemetry() {
    let originalObserve = SyncTelemetry.observe;
    let self = this;
    SyncTelemetry.observe = function() {
      try {
        originalObserve.apply(this, arguments);
      } catch (e) {
        self.DumpError("Error when generating sync telemetry", e);
      }
    };
    SyncTelemetry.submit = record => {
      Logger.logInfo("Intercepted sync telemetry submission: " + JSON.stringify(record));
      this._syncsReportedViaTelemetry += record.syncs.length + (record.discarded || 0);
      if (record.discarded) {
        if (record.syncs.length != SyncTelemetry.maxPayloadCount) {
          this.DumpError("Syncs discarded from ping before maximum payload count reached");
        }
      }
      // If this is the shutdown ping, check and see that the telemetry saw all the syncs.
      if (record.why === "shutdown") {
        // If we happen to sync outside of tps manually causing it, its not an
        // error in the telemetry, so we only complain if we didn't see all of them.
        if (this._syncsReportedViaTelemetry < this._syncCount) {
          this.DumpError(`Telemetry missed syncs: Saw ${this._syncsReportedViaTelemetry}, should have >= ${this._syncCount}.`);
        }
      }
      if (!record.syncs.length) {
        // Note: we're overwriting submit, so this is called even for pings that
        // may have no data (which wouldn't be submitted to telemetry and would
        // fail validation).
        return;
      }
      if (!this.pingValidator(record)) {
        // Note that we already logged the record.
        this.DumpError("Sync ping validation failed with errors: " + JSON.stringify(this.pingValidator.errors));
      }
    };
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
    if (hh.oscpu.toLowerCase().indexOf("windows") > -1) {
      let re = /\/(\w)\/(.*)/;
      this.config.testdir = this.config.testdir.replace(re, "$1://$2").replace(/\//g, "\\");
    }
    mozmillfile.initWithPath(this.config.testdir);
    mozmillfile.appendRelativePath(testfile);
    Logger.logInfo("Running mozmill test " + mozmillfile.path);

    var frame = {};
    Cu.import("resource://mozmill/modules/frame.js", frame);
    frame.events.addListener("setTest", this.MozmillSetTestListener.bind(this));
    frame.events.addListener("endTest", this.MozmillEndTestListener.bind(this));
    this.StartAsyncOperation();
    frame.runTestFile(mozmillfile.path, null);
  },

  /**
   * Return an object that when called, will block until the named event
   * is observed. This is similar to waitForEvent, although is typically safer
   * if you need to do some other work that may make the event fire.
   *
   * eg:
   *    doSomething(); // causes the event to be fired.
   *    waitForEvent("something");
   * is risky as the call to doSomething may trigger the event before the
   * waitForEvent call is made. Contrast with:
   *
   *   let waiter = createEventWaiter("something"); // does *not* block.
   *   doSomething(); // causes the event to be fired.
   *   waiter(); // will return as soon as the event fires, even if it fires
   *             // before this function is called.
   *
   * @param aEventName
   *        String event to wait for.
   */
  createEventWaiter(aEventName) {
    Logger.logInfo("Setting up wait for " + aEventName + "...");
    let cb = Async.makeSpinningCallback();
    Svc.Obs.add(aEventName, cb);
    return function() {
      try {
        cb.wait();
      } finally {
        Svc.Obs.remove(aEventName, cb);
        Logger.logInfo(aEventName + " observed!");
      }
    }
  },


  /**
   * Synchronously wait for the named event to be observed.
   *
   * When the event is observed, the function will wait an extra tick before
   * returning.
   *
   * Note that in general, you should probably use createEventWaiter unless you
   * are 100% sure that the event being waited on can only be sent after this
   * call adds the listener.
   *
   * @param aEventName
   *        String event to wait for.
   */
  waitForEvent: function waitForEvent(aEventName) {
    this.createEventWaiter(aEventName)();
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
      this.waitForEvent("weave:service:resyncs-finished");
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

    // This might come during Authentication.signIn
    this._triggeredSync = true;
    Logger.logInfo("Setting client credentials and login.");
    Authentication.signIn(this.config.fx_account);
    this.waitForSetupComplete();
    Logger.AssertEqual(Weave.Status.service, Weave.STATUS_OK, "Weave status OK");
    this.waitForTracking();
    // We might get an initial sync at login time - let that complete.
    this.waitForSyncFinished();
  },

  /**
   * Triggers a sync operation
   *
   * @param {String} [wipeAction]
   *        Type of wipe to perform (resetClient, wipeClient, wipeRemote)
   *
   */
  Sync: function TPS__Sync(wipeAction) {
    if (this._syncActive) {
      Logger.logInfo("WARNING: Sync currently active! Waiting, before triggering another");
      this.waitForSyncFinished();
    }
    Logger.logInfo("Executing Sync" + (wipeAction ? ": " + wipeAction : ""));

    // Force a wipe action if requested. In case of an initial sync the pref
    // will be overwritten by Sync itself (see bug 992198), so ensure that we
    // also handle it via the "weave:service:setup-complete" notification.
    if (wipeAction) {
      this._syncWipeAction = wipeAction;
      Weave.Svc.Prefs.set("firstSync", wipeAction);
    } else {
      Weave.Svc.Prefs.reset("firstSync");
    }

    this.Login(false);
    ++this._syncCount;

    this._triggeredSync = true;
    this.StartAsyncOperation();
    Async.promiseSpinningly(Weave.Service.sync());
    Logger.logInfo("Sync is complete");
  },

  WipeServer: function TPS__WipeServer() {
    Logger.logInfo("Wiping data from server.");

    this.Login(false);
    Async.promiseSpinningly(Weave.Service.login());
    Async.promiseSpinningly(Weave.Service.wipeServer());
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
  skipValidation() {
    TPS.shouldValidateAddons = false;
  }
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
  },
  skipValidation() {
    TPS.shouldValidatePasswords = false;
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
