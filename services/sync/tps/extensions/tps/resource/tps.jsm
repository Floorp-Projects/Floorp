/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This is a JavaScript module (JSM) to be imported via
 * Components.utils.import() and acts as a singleton. Only the following
 * listed symbols will exposed on import, and only when and where imported.
 */

var EXPORTED_SYMBOLS = [
  "ACTIONS",
  "Addons",
  "Addresses",
  "Bookmarks",
  "CreditCards",
  "ExtensionStorage",
  "Formdata",
  "History",
  "Passwords",
  "Prefs",
  "Tabs",
  "TPS",
  "Windows",
];

var module = this;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Async: "resource://services-common/async.js",
  BrowserTabs: "resource://tps/modules/tabs.jsm",
  BrowserWindows: "resource://tps/modules/windows.jsm",
  CommonUtils: "resource://services-common/utils.js",
  extensionStorageSync: "resource://gre/modules/ExtensionStorageSync.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  Log: "resource://gre/modules/Log.jsm",
  Logger: "resource://tps/logger.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PromiseUtils: "resource://gre/modules/PromiseUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  Svc: "resource://services-sync/util.js",
  SyncTelemetry: "resource://services-sync/telemetry.js",
  Weave: "resource://services-sync/main.js",
  WEAVE_VERSION: "resource://services-sync/constants.js",

  Addon: "resource://tps/modules/addons.jsm",
  AddonValidator: "resource://services-sync/engines/addons.js",

  FormData: "resource://tps/modules/forms.jsm",
  FormValidator: "resource://services-sync/engines/forms.js",

  Bookmark: "resource://tps/modules/bookmarks.jsm",
  DumpBookmarks: "resource://tps/modules/bookmarks.jsm",
  BookmarkFolder: "resource://tps/modules/bookmarks.jsm",
  Livemark: "resource://tps/modules/bookmarks.jsm",
  Separator: "resource://tps/modules/bookmarks.jsm",
  BookmarkValidator: "resource://services-sync/bookmark_validator.js",

  Address: "resource://tps/modules/formautofill.jsm",
  DumpAddresses: "resource://tps/modules/formautofill.jsm",
  CreditCard: "resource://tps/modules/formautofill.jsm",
  DumpCreditCards: "resource://tps/modules/formautofill.jsm",

  DumpHistory: "resource://tps/modules/history.jsm",
  HistoryEntry: "resource://tps/modules/history.jsm",

  Preference: "resource://tps/modules/prefs.jsm",

  DumpPasswords: "resource://tps/modules/passwords.jsm",
  Password: "resource://tps/modules/passwords.jsm",
  PasswordValidator: "resource://services-sync/engines/passwords.js",
});

XPCOMUtils.defineLazyGetter(this, "fileProtocolHandler", () => {
  let fileHandler = Services.io.getProtocolHandler("file");
  return fileHandler.QueryInterface(Ci.nsIFileProtocolHandler);
});

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => {
  return new TextDecoder();
});

ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);

// Options for wiping data during a sync
const SYNC_RESET_CLIENT = "resetClient";
const SYNC_WIPE_CLIENT = "wipeClient";
const SYNC_WIPE_REMOTE = "wipeRemote";

// Actions a test can perform
const ACTION_ADD = "add";
const ACTION_DELETE = "delete";
const ACTION_MODIFY = "modify";
const ACTION_SET_ENABLED = "set-enabled";
const ACTION_SYNC = "sync";
const ACTION_SYNC_RESET_CLIENT = SYNC_RESET_CLIENT;
const ACTION_SYNC_WIPE_CLIENT = SYNC_WIPE_CLIENT;
const ACTION_SYNC_WIPE_REMOTE = SYNC_WIPE_REMOTE;
const ACTION_VERIFY = "verify";
const ACTION_VERIFY_NOT = "verify-not";

const ACTIONS = [
  ACTION_ADD,
  ACTION_DELETE,
  ACTION_MODIFY,
  ACTION_SET_ENABLED,
  ACTION_SYNC,
  ACTION_SYNC_RESET_CLIENT,
  ACTION_SYNC_WIPE_CLIENT,
  ACTION_SYNC_WIPE_REMOTE,
  ACTION_VERIFY,
  ACTION_VERIFY_NOT,
];

const OBSERVER_TOPICS = [
  "fxaccounts:onlogin",
  "fxaccounts:onlogout",
  "profile-before-change",
  "weave:service:tracking-started",
  "weave:service:tracking-stopped",
  "weave:service:login:error",
  "weave:service:setup-complete",
  "weave:service:sync:finish",
  "weave:service:sync:delayed",
  "weave:service:sync:error",
  "weave:service:sync:start",
  "weave:service:resyncs-finished",
  "places-browser-init-complete",
];

var TPS = {
  _currentAction: -1,
  _currentPhase: -1,
  _enabledEngines: null,
  _errors: 0,
  _isTracking: false,
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
  _msSinceEpoch: 0,
  _requestedQuit: false,
  shouldValidateAddons: false,
  shouldValidateBookmarks: false,
  shouldValidatePasswords: false,
  shouldValidateForms: false,
  _placesInitDeferred: PromiseUtils.defer(),

  _init: function TPS__init() {
    this.delayAutoSync();

    OBSERVER_TOPICS.forEach(function(aTopic) {
      Services.obs.addObserver(this, aTopic, true);
    }, this);

    /* global Authentication */
    ChromeUtils.import("resource://tps/auth/fxaccounts.jsm", module);

    // Some engines bump their score during their sync, which then causes
    // another sync immediately (notably, prefs and addons). We don't want
    // this to happen, and there's no obvious preference to kill it - so
    // we do this nasty hack to ensure the global score is always zero.
    Services.prefs.addObserver("services.sync.globalScore", () => {
      if (Weave.Service.scheduler.globalScore != 0) {
        Weave.Service.scheduler.globalScore = 0;
      }
    });
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

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  observe: function TPS__observe(subject, topic, data) {
    try {
      Logger.logInfo("----------event observed: " + topic);

      switch (topic) {
        case "profile-before-change":
          OBSERVER_TOPICS.forEach(function(topic) {
            Services.obs.removeObserver(this, topic);
          }, this);

          Logger.close();

          break;

        case "places-browser-init-complete":
          this._placesInitDeferred.resolve();
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
            CommonUtils.nextTick(() => {
              this.RunNextTestAction().catch(err => {
                this.DumpError("RunNextTestActionFailed", err);
              });
            });
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
          break;

        case "weave:service:sync:start":
          // Ensure that the sync operation has been started by TPS
          if (!this._triggeredSync) {
            this.DumpError(
              "Automatic sync got triggered, which is not allowed."
            );
          }

          this._syncActive = true;
          break;

        case "weave:service:tracking-started":
          this._isTracking = true;
          break;

        case "weave:service:tracking-stopped":
          this._isTracking = false;
          break;

        case "fxaccounts:onlogin":
          // A user signed in - for TPS that always means sync - so configure
          // that.
          Weave.Service.configure().catch(e => {
            this.DumpError("Configuring sync failed.", e);
          });
          break;

        default:
          Logger.logInfo(`unhandled event: ${topic}`);
      }
    } catch (e) {
      this.DumpError("Observer failed", e);
    }
  },

  /**
   * Given that we cannot completely disable the automatic sync operations, we
   * massively delay the next sync. Sync operations have to only happen when
   * directly called via TPS.Sync()!
   */
  delayAutoSync: function TPS_delayAutoSync() {
    Weave.Svc.Prefs.set("scheduler.immediateInterval", 7200);
    Weave.Svc.Prefs.set("scheduler.idleInterval", 7200);
    Weave.Svc.Prefs.set("scheduler.activeInterval", 7200);
    Weave.Svc.Prefs.set("syncThreshold", 10000000);
  },

  quit: function TPS__quit() {
    Logger.logInfo("quitting");
    this._requestedQuit = true;
    this.goQuitApplication();
  },

  async HandleWindows(aWindow, action) {
    Logger.logInfo(
      "executing action " +
        action.toUpperCase() +
        " on window " +
        JSON.stringify(aWindow)
    );
    switch (action) {
      case ACTION_ADD:
        await BrowserWindows.Add(aWindow.private);
        break;
    }
    Logger.logPass("executing action " + action.toUpperCase() + " on windows");
  },

  async HandleTabs(tabs, action) {
    for (let tab of tabs) {
      Logger.logInfo(
        "executing action " +
          action.toUpperCase() +
          " on tab " +
          JSON.stringify(tab)
      );
      switch (action) {
        case ACTION_ADD:
          await BrowserTabs.Add(tab.uri);
          break;
        case ACTION_VERIFY:
          Logger.AssertTrue(
            typeof tab.profile != "undefined",
            "profile must be defined when verifying tabs"
          );
          Logger.AssertTrue(
            BrowserTabs.Find(tab.uri, tab.title, tab.profile),
            "error locating tab"
          );
          break;
        case ACTION_VERIFY_NOT:
          Logger.AssertTrue(
            typeof tab.profile != "undefined",
            "profile must be defined when verifying tabs"
          );
          Logger.AssertTrue(
            !BrowserTabs.Find(tab.uri, tab.title, tab.profile),
            "tab found which was expected to be absent"
          );
          break;
        default:
          Logger.AssertTrue(false, "invalid action: " + action);
      }
    }
    if (action === ACTION_ADD) {
      // Ideally we'd do the right thing (probably resolving bug 1383832, or
      // waiting for sessionstore events in TPS), but waiting enough time to
      // be reasonably confident the sessionstore time has fired is simpler.
      // Without this timeout, we are likely to see "error locating tab"
      // on subsequent syncs.
      await new Promise(resolve => setTimeout(resolve, 2500));
    }
    Logger.logPass("executing action " + action.toUpperCase() + " on tabs");
  },

  async HandlePrefs(prefs, action) {
    for (let pref of prefs) {
      Logger.logInfo(
        "executing action " +
          action.toUpperCase() +
          " on pref " +
          JSON.stringify(pref)
      );
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

  async HandleForms(data, action) {
    this.shouldValidateForms = true;
    for (let datum of data) {
      Logger.logInfo(
        "executing action " +
          action.toUpperCase() +
          " on form entry " +
          JSON.stringify(datum)
      );
      let formdata = new FormData(datum, this._msSinceEpoch);
      switch (action) {
        case ACTION_ADD:
          await formdata.Create();
          break;
        case ACTION_DELETE:
          await formdata.Remove();
          break;
        case ACTION_VERIFY:
          Logger.AssertTrue(await formdata.Find(), "form data not found");
          break;
        case ACTION_VERIFY_NOT:
          Logger.AssertTrue(
            !(await formdata.Find()),
            "form data found, but it shouldn't be present"
          );
          break;
        default:
          Logger.AssertTrue(false, "invalid action: " + action);
      }
    }
    Logger.logPass("executing action " + action.toUpperCase() + " on formdata");
  },

  async HandleHistory(entries, action) {
    try {
      for (let entry of entries) {
        const entryString = JSON.stringify(entry);
        Logger.logInfo(
          "executing action " +
            action.toUpperCase() +
            " on history entry " +
            entryString
        );
        switch (action) {
          case ACTION_ADD:
            await HistoryEntry.Add(entry, this._msSinceEpoch);
            break;
          case ACTION_DELETE:
            await HistoryEntry.Delete(entry, this._msSinceEpoch);
            break;
          case ACTION_VERIFY:
            Logger.AssertTrue(
              await HistoryEntry.Find(entry, this._msSinceEpoch),
              "Uri visits not found in history database: " + entryString
            );
            break;
          case ACTION_VERIFY_NOT:
            Logger.AssertTrue(
              !(await HistoryEntry.Find(entry, this._msSinceEpoch)),
              "Uri visits found in history database, but they shouldn't be: " +
                entryString
            );
            break;
          default:
            Logger.AssertTrue(false, "invalid action: " + action);
        }
      }
      Logger.logPass(
        "executing action " + action.toUpperCase() + " on history"
      );
    } catch (e) {
      await DumpHistory();
      throw e;
    }
  },

  async HandlePasswords(passwords, action) {
    this.shouldValidatePasswords = true;
    try {
      for (let password of passwords) {
        Logger.logInfo(
          "executing action " +
            action.toUpperCase() +
            " on password " +
            JSON.stringify(password)
        );
        let passwordOb = new Password(password);
        switch (action) {
          case ACTION_ADD:
            Logger.AssertTrue(
              passwordOb.Create() > -1,
              "error adding password"
            );
            break;
          case ACTION_VERIFY:
            Logger.AssertTrue(passwordOb.Find() != -1, "password not found");
            break;
          case ACTION_VERIFY_NOT:
            Logger.AssertTrue(
              passwordOb.Find() == -1,
              "password found, but it shouldn't exist"
            );
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
      Logger.logPass(
        "executing action " + action.toUpperCase() + " on passwords"
      );
    } catch (e) {
      DumpPasswords();
      throw e;
    }
  },

  async HandleAddons(addons, action, state) {
    this.shouldValidateAddons = true;
    for (let entry of addons) {
      Logger.logInfo(
        "executing action " +
          action.toUpperCase() +
          " on addon " +
          JSON.stringify(entry)
      );
      let addon = new Addon(this, entry);
      switch (action) {
        case ACTION_ADD:
          await addon.install();
          break;
        case ACTION_DELETE:
          await addon.uninstall();
          break;
        case ACTION_VERIFY:
          Logger.AssertTrue(
            await addon.find(state),
            "addon " + addon.id + " not found"
          );
          break;
        case ACTION_VERIFY_NOT:
          Logger.AssertFalse(
            await addon.find(state),
            "addon " + addon.id + " is present, but it shouldn't be"
          );
          break;
        case ACTION_SET_ENABLED:
          Logger.AssertTrue(
            await addon.setEnabled(state),
            "addon " + addon.id + " not found"
          );
          break;
        default:
          throw new Error("Unknown action for add-on: " + action);
      }
    }
    Logger.logPass("executing action " + action.toUpperCase() + " on addons");
  },

  async HandleBookmarks(bookmarks, action) {
    // wait for default bookmarks to be created.
    await this._placesInitDeferred.promise;
    this.shouldValidateBookmarks = true;
    try {
      let items = [];
      for (let folder in bookmarks) {
        let last_item_pos = -1;
        for (let bookmark of bookmarks[folder]) {
          Logger.clearPotentialError();
          let placesItem;
          bookmark.location = folder;

          if (last_item_pos != -1) {
            bookmark.last_item_pos = last_item_pos;
          }
          let itemGuid = null;

          if (action != ACTION_MODIFY && action != ACTION_DELETE) {
            Logger.logInfo(
              "executing action " +
                action.toUpperCase() +
                " on bookmark " +
                JSON.stringify(bookmark)
            );
          }

          if ("uri" in bookmark) {
            placesItem = new Bookmark(bookmark);
          } else if ("folder" in bookmark) {
            placesItem = new BookmarkFolder(bookmark);
          } else if ("livemark" in bookmark) {
            placesItem = new Livemark(bookmark);
          } else if ("separator" in bookmark) {
            placesItem = new Separator(bookmark);
          }

          if (action == ACTION_ADD) {
            itemGuid = await placesItem.Create();
          } else {
            itemGuid = await placesItem.Find();
            if (action == ACTION_VERIFY_NOT) {
              Logger.AssertTrue(
                itemGuid == null,
                "places item exists but it shouldn't: " +
                  JSON.stringify(bookmark)
              );
            } else {
              Logger.AssertTrue(itemGuid, "places item not found", true);
            }
          }

          last_item_pos = await placesItem.GetItemIndex();
          items.push(placesItem);
        }
      }

      if (action == ACTION_DELETE || action == ACTION_MODIFY) {
        for (let item of items) {
          Logger.logInfo(
            "executing action " +
              action.toUpperCase() +
              " on bookmark " +
              JSON.stringify(item)
          );
          switch (action) {
            case ACTION_DELETE:
              await item.Remove();
              break;
            case ACTION_MODIFY:
              if (item.updateProps != null) {
                await item.Update();
              }
              break;
          }
        }
      }

      Logger.logPass(
        "executing action " + action.toUpperCase() + " on bookmarks"
      );
    } catch (e) {
      await DumpBookmarks();
      throw e;
    }
  },

  async HandleAddresses(addresses, action) {
    try {
      for (let address of addresses) {
        Logger.logInfo(
          "executing action " +
            action.toUpperCase() +
            " on address " +
            JSON.stringify(address)
        );
        let addressOb = new Address(address);
        switch (action) {
          case ACTION_ADD:
            await addressOb.Create();
            break;
          case ACTION_MODIFY:
            await addressOb.Update();
            break;
          case ACTION_VERIFY:
            Logger.AssertTrue(await addressOb.Find(), "address not found");
            break;
          case ACTION_VERIFY_NOT:
            Logger.AssertTrue(
              !(await addressOb.Find()),
              "address found, but it shouldn't exist"
            );
            break;
          case ACTION_DELETE:
            Logger.AssertTrue(await addressOb.Find(), "address not found");
            await addressOb.Remove();
            break;
          default:
            Logger.AssertTrue(false, "invalid action: " + action);
        }
      }
      Logger.logPass(
        "executing action " + action.toUpperCase() + " on addresses"
      );
    } catch (e) {
      await DumpAddresses();
      throw e;
    }
  },

  async HandleCreditCards(creditCards, action) {
    try {
      for (let creditCard of creditCards) {
        Logger.logInfo(
          "executing action " +
            action.toUpperCase() +
            " on creditCard " +
            JSON.stringify(creditCard)
        );
        let creditCardOb = new CreditCard(creditCard);
        switch (action) {
          case ACTION_ADD:
            await creditCardOb.Create();
            break;
          case ACTION_MODIFY:
            await creditCardOb.Update();
            break;
          case ACTION_VERIFY:
            Logger.AssertTrue(
              await creditCardOb.Find(),
              "creditCard not found"
            );
            break;
          case ACTION_VERIFY_NOT:
            Logger.AssertTrue(
              !(await creditCardOb.Find()),
              "creditCard found, but it shouldn't exist"
            );
            break;
          case ACTION_DELETE:
            Logger.AssertTrue(
              await creditCardOb.Find(),
              "creditCard not found"
            );
            await creditCardOb.Remove();
            break;
          default:
            Logger.AssertTrue(false, "invalid action: " + action);
        }
      }
      Logger.logPass(
        "executing action " + action.toUpperCase() + " on creditCards"
      );
    } catch (e) {
      await DumpCreditCards();
      throw e;
    }
  },

  async Cleanup() {
    try {
      await this.WipeServer();
    } catch (ex) {
      Logger.logError("Failed to wipe server: " + Log.exceptionStr(ex));
    }
    try {
      if (await Authentication.isLoggedIn()) {
        // signout and wait for Sync to completely reset itself.
        Logger.logInfo("signing out");
        let waiter = this.promiseObserver("weave:service:start-over:finish");
        await Authentication.signOut();
        await waiter;
        Logger.logInfo("signout complete");
      }
      await Authentication.deleteEmail(this.config.fx_account.username);
    } catch (e) {
      Logger.logError("Failed to sign out: " + Log.exceptionStr(e));
    }
  },

  /**
   * Use Sync's bookmark validation code to see if we've corrupted the tree.
   */
  async ValidateBookmarks() {
    let getServerBookmarkState = async () => {
      let bookmarkEngine = Weave.Service.engineManager.get("bookmarks");
      let collection = bookmarkEngine.itemSource();
      let collectionKey = bookmarkEngine.service.collectionKeys.keyForCollection(
        bookmarkEngine.name
      );
      collection.full = true;
      let items = [];
      let resp = await collection.get();
      for (let json of resp.obj) {
        let record = new collection._recordObj();
        record.deserialize(json);
        await record.decrypt(collectionKey);
        items.push(record.cleartext);
      }
      return items;
    };
    let serverRecordDumpStr;
    try {
      Logger.logInfo("About to perform bookmark validation");
      let clientTree = await PlacesUtils.promiseBookmarksTree("", {
        includeItemIds: true,
      });
      let serverRecords = await getServerBookmarkState();
      // We can't wait until catch to stringify this, since at that point it will have cycles.
      serverRecordDumpStr = JSON.stringify(serverRecords);

      let validator = new BookmarkValidator();
      let { problemData } = await validator.compareServerWithClient(
        serverRecords,
        clientTree
      );

      for (let { name, count } of problemData.getSummary()) {
        // Exclude mobile showing up on the server hackily so that we don't
        // report it every time, see bug 1273234 and 1274394 for more information.
        if (
          name === "serverUnexpected" &&
          problemData.serverUnexpected.includes("mobile")
        ) {
          --count;
        }
        if (count) {
          // Log this out before we assert. This is useful in the context of TPS logs, since we
          // can see the IDs in the test files.
          Logger.logInfo(
            `Validation problem: "${name}": ${JSON.stringify(
              problemData[name]
            )}`
          );
        }
        Logger.AssertEqual(
          count,
          0,
          `Bookmark validation error of type ${name}`
        );
      }
    } catch (e) {
      // Dump the client records (should always be doable)
      DumpBookmarks();
      // Dump the server records if gotten them already.
      if (serverRecordDumpStr) {
        Logger.logInfo(
          "Server bookmark records:\n" + serverRecordDumpStr + "\n"
        );
      }
      this.DumpError("Bookmark validation failed", e);
    }
    Logger.logInfo("Bookmark validation finished");
  },

  async ValidateCollection(engineName, ValidatorType) {
    let serverRecordDumpStr;
    let clientRecordDumpStr;
    try {
      Logger.logInfo(`About to perform validation for "${engineName}"`);
      let engine = Weave.Service.engineManager.get(engineName);
      let validator = new ValidatorType(engine);
      let serverRecords = await validator.getServerItems(engine);
      let clientRecords = await validator.getClientItems();
      try {
        // This substantially improves the logs for addons while not making a
        // substantial difference for the other two
        clientRecordDumpStr = JSON.stringify(
          clientRecords.map(r => {
            let res = validator.normalizeClientItem(r);
            delete res.original; // Try and prevent cyclic references
            return res;
          })
        );
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
      let { problemData } = await validator.compareClientWithServer(
        clientRecords,
        serverRecords
      );
      for (let { name, count } of problemData.getSummary()) {
        if (count) {
          Logger.logInfo(
            `Validation problem: "${name}": ${JSON.stringify(
              problemData[name]
            )}`
          );
        }
        Logger.AssertEqual(
          count,
          0,
          `Validation error for "${engineName}" of type "${name}"`
        );
      }
    } catch (e) {
      // Dump the client records if possible
      if (clientRecordDumpStr) {
        Logger.logInfo(
          `Client state for ${engineName}:\n${clientRecordDumpStr}\n`
        );
      }
      // Dump the server records if gotten them already.
      if (serverRecordDumpStr) {
        Logger.logInfo(
          `Server state for ${engineName}:\n${serverRecordDumpStr}\n`
        );
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

  async RunNextTestAction() {
    Logger.logInfo("Running next test action");
    try {
      if (this._currentAction >= this._phaselist[this._currentPhase].length) {
        // Run necessary validations and then finish up
        Logger.logInfo("No more actions - running validations...");
        if (this.shouldValidateBookmarks) {
          await this.ValidateBookmarks();
        }
        if (this.shouldValidatePasswords) {
          await this.ValidatePasswords();
        }
        if (this.shouldValidateForms) {
          await this.ValidateForms();
        }
        if (this.shouldValidateAddons) {
          await this.ValidateAddons();
        }
        // Force this early so that we run the validation and detect missing pings
        // *before* we start shutting down, since if we do it after, the python
        // code won't notice the failure.
        SyncTelemetry.shutdown();
        // we're all done
        Logger.logInfo(
          "test phase " +
            this._currentPhase +
            ": " +
            (this._errors ? "FAIL" : "PASS")
        );
        this._phaseFinished = true;
        this.quit();
        return;
      }
      this.seconds_since_epoch = Services.prefs.getIntPref(
        "tps.seconds_since_epoch"
      );
      if (this.seconds_since_epoch) {
        // Places dislikes it if we add visits in the future. We pretend the
        // real time is 1 minute ago to avoid issues caused by places using a
        // different clock than the one that set the seconds_since_epoch pref.
        this._msSinceEpoch = (this.seconds_since_epoch - 60) * 1000;
      } else {
        this.DumpError("seconds-since-epoch not set");
        return;
      }

      let phase = this._phaselist[this._currentPhase];
      let action = phase[this._currentAction];
      Logger.logInfo("starting action: " + action[0].name);
      await action[0].apply(this, action.slice(1));

      this._currentAction++;
    } catch (e) {
      if (Async.isShutdownException(e)) {
        if (this._requestedQuit) {
          Logger.logInfo("Sync aborted due to requested shutdown");
        } else {
          this.DumpError(
            "Sync aborted due to shutdown, but we didn't request it"
          );
        }
      } else {
        this.DumpError("RunNextTestAction failed", e);
      }
      return;
    }
    await this.RunNextTestAction();
  },

  _getFileRelativeToSourceRoot(testFileURL, relativePath) {
    let file = fileProtocolHandler.getFileFromURLSpec(testFileURL);
    let root = file.parent.parent.parent.parent.parent; // <root>/services/sync/tests/tps/test_foo.js // <root>/services/sync/tests/tps // <root>/services/sync/tests // <root>/services/sync // <root>/services // <root>
    root.appendRelativePath(OS.Path.normalize(relativePath));
    return root;
  },

  // Default ping validator that always says the ping passes. This should be
  // overridden unless the `testing.tps.skipPingValidation` pref is true.
  pingValidator(ping) {
    Logger.logInfo("Not validating ping -- disabled by pref");
    return true;
  },

  // Attempt to load the sync_ping_schema.json and initialize `this.pingValidator`
  // based on the source of the tps file. Assumes that it's at "../unit/sync_ping_schema.json"
  // relative to the directory the tps test file (testFile) is contained in.
  _tryLoadPingSchema(testFile) {
    if (Services.prefs.getBoolPref("testing.tps.skipPingValidation", false)) {
      return;
    }
    try {
      let schemaFile = this._getFileRelativeToSourceRoot(
        testFile,
        "services/sync/tests/unit/sync_ping_schema.json"
      );

      let stream = Cc[
        "@mozilla.org/network/file-input-stream;1"
      ].createInstance(Ci.nsIFileInputStream);

      stream.init(schemaFile, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);

      let bytes = NetUtil.readInputStream(stream, stream.available());
      let schema = JSON.parse(gTextDecoder.decode(bytes));
      Logger.logInfo("Successfully loaded schema");

      // Importing resource://testing-common/* isn't possible from within TPS,
      // so we load Ajv manually.
      let ajvFile = this._getFileRelativeToSourceRoot(
        testFile,
        "testing/modules/ajv-6.12.6.js"
      );
      let ajvURL = fileProtocolHandler.getURLSpecFromActualFile(ajvFile);
      let ns = {};
      ChromeUtils.import(ajvURL, ns);
      let ajv = new ns.Ajv({ async: "co*" });
      this.pingValidator = ajv.compile(schema);
    } catch (e) {
      this.DumpError(
        `Failed to load ping schema and AJV relative to "${testFile}".`,
        e
      );
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
  async RunTestPhase(file, phase, logpath, options) {
    try {
      let settings = options || {};

      Logger.init(logpath);
      Logger.logInfo("Sync version: " + WEAVE_VERSION);
      Logger.logInfo("Firefox buildid: " + Services.appinfo.appBuildID);
      Logger.logInfo("Firefox version: " + Services.appinfo.version);
      Logger.logInfo(
        "Firefox source revision: " +
          (AppConstants.SOURCE_REVISION_URL || "unknown")
      );
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

      await Weave.Service.promiseInitialized;

      // We only want to do this if we modified the bookmarks this phase.
      this.shouldValidateBookmarks = false;

      // Always give Sync an extra tick to initialize. If we waited for the
      // service:ready event, this is required to ensure all handlers have
      // executed.
      await Async.promiseYield();
      await this._executeTestPhase(file, phase, settings);
    } catch (e) {
      this.DumpError("RunTestPhase failed", e);
    }
  },

  /**
   * Executes a single test phase.
   *
   * This is called by RunTestPhase() after the environment is validated.
   */
  async _executeTestPhase(file, phase, settings) {
    try {
      this.config = JSON.parse(Services.prefs.getCharPref("tps.config"));
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
            await Weave.Service.engineManager.unregister(engine);
          }
        }
      }
      Logger.logInfo("Starting phase " + this._currentPhase);

      Logger.logInfo(
        "setting client.name to " + this.phases[this._currentPhase]
      );
      Weave.Svc.Prefs.set("client.name", this.phases[this._currentPhase]);

      this._interceptSyncTelemetry();

      // start processing the test actions
      this._currentAction = 0;
      await SessionStore.promiseAllWindowsRestored;
      await this.RunNextTestAction();
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
      Logger.logInfo(
        "Intercepted sync telemetry submission: " + JSON.stringify(record)
      );
      this._syncsReportedViaTelemetry +=
        record.syncs.length + (record.discarded || 0);
      if (record.discarded) {
        if (record.syncs.length != SyncTelemetry.maxPayloadCount) {
          this.DumpError(
            "Syncs discarded from ping before maximum payload count reached"
          );
        }
      }
      // If this is the shutdown ping, check and see that the telemetry saw all the syncs.
      if (record.why === "shutdown") {
        // If we happen to sync outside of tps manually causing it, its not an
        // error in the telemetry, so we only complain if we didn't see all of them.
        if (this._syncsReportedViaTelemetry < this._syncCount) {
          this.DumpError(
            `Telemetry missed syncs: Saw ${this._syncsReportedViaTelemetry}, should have >= ${this._syncCount}.`
          );
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
        this.DumpError(
          "Sync ping validation failed with errors: " +
            JSON.stringify(this.pingValidator.errors)
        );
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
      // This is the first phase we should force a log in
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
      throw new Error(
        "Argument to RestrictEngines() is not an array: " + typeof names
      );
    }

    this._enabledEngines = names;
  },

  /**
   * Returns a promise that resolves when a specific observer notification is
   * resolved. This is similar to the various waitFor* functions, although is
   * typically safer if you need to do some other work that may make the event
   * fire.
   *
   * eg:
   *    doSomething(); // causes the event to be fired.
   *    await promiseObserver("something");
   * is risky as the call to doSomething may trigger the event before the
   * promiseObserver call is made. Contrast with:
   *
   *   let waiter = promiseObserver("something");
   *   doSomething(); // causes the event to be fired.
   *   await waiter;  // will return as soon as the event fires, even if it fires
   *                  // before this function is called.
   *
   * @param aEventName
   *        String event to wait for.
   */
  promiseObserver(aEventName) {
    return new Promise(resolve => {
      Logger.logInfo("Setting up wait for " + aEventName + "...");
      let handler = () => {
        Logger.logInfo("Observed " + aEventName);
        Svc.Obs.remove(aEventName, handler);
        resolve();
      };
      Svc.Obs.add(aEventName, handler);
    });
  },

  /**
   * Wait for the named event to be observed.
   *
   * Note that in general, you should probably use promiseObserver unless you
   * are 100% sure that the event being waited on can only be sent after this
   * call adds the listener.
   *
   * @param aEventName
   *        String event to wait for.
   */
  async waitForEvent(aEventName) {
    await this.promiseObserver(aEventName);
  },

  /**
   * Waits for Sync to logged in before returning
   */
  async waitForSetupComplete() {
    if (!this._setupComplete) {
      await this.waitForEvent("weave:service:setup-complete");
    }
  },

  /**
   * Waits for Sync to be finished before returning
   */
  async waitForSyncFinished() {
    if (Weave.Service.locked) {
      await this.waitForEvent("weave:service:resyncs-finished");
    }
  },

  /**
   * Waits for Sync to start tracking before returning.
   */
  async waitForTracking() {
    if (!this._isTracking) {
      await this.waitForEvent("weave:service:tracking-started");
    }
  },

  /**
   * Login on the server
   */
  async Login() {
    if (await Authentication.isReady()) {
      return;
    }

    Logger.logInfo("Setting client credentials and login.");
    await Authentication.signIn(this.config.fx_account);
    await this.waitForSetupComplete();
    Logger.AssertEqual(
      Weave.Status.service,
      Weave.STATUS_OK,
      "Weave status OK"
    );
    await this.waitForTracking();
  },

  /**
   * Triggers a sync operation
   *
   * @param {String} [wipeAction]
   *        Type of wipe to perform (resetClient, wipeClient, wipeRemote)
   *
   */
  async Sync(wipeAction) {
    if (this._syncActive) {
      this.DumpError("Sync currently active which should be impossible");
      return;
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
    if (!(await Weave.Service.login())) {
      // We need to complete verification.
      Logger.logInfo("Logging in before performing sync");
      await this.Login();
    }
    ++this._syncCount;

    Logger.logInfo("Executing Sync" + (wipeAction ? ": " + wipeAction : ""));
    this._triggeredSync = true;
    await Weave.Service.sync();
    Logger.logInfo("Sync is complete");
    // wait a second for things to settle...
    await new Promise(resolve => {
      CommonUtils.namedTimer(resolve, 1000, this, "postsync");
    });
  },

  async WipeServer() {
    Logger.logInfo("Wiping data from server.");

    await this.Login();
    await Weave.Service.login();
    await Weave.Service.wipeServer();
  },

  /**
   * Action which ensures changes are being tracked before returning.
   */
  async EnsureTracking() {
    await this.Login();
    await this.waitForTracking();
  },
};

var Addons = {
  async install(addons) {
    await TPS.HandleAddons(addons, ACTION_ADD);
  },
  async setEnabled(addons, state) {
    await TPS.HandleAddons(addons, ACTION_SET_ENABLED, state);
  },
  async uninstall(addons) {
    await TPS.HandleAddons(addons, ACTION_DELETE);
  },
  async verify(addons, state) {
    await TPS.HandleAddons(addons, ACTION_VERIFY, state);
  },
  async verifyNot(addons) {
    await TPS.HandleAddons(addons, ACTION_VERIFY_NOT);
  },
  skipValidation() {
    TPS.shouldValidateAddons = false;
  },
};

var Addresses = {
  async add(addresses) {
    await this.HandleAddresses(addresses, ACTION_ADD);
  },
  async modify(addresses) {
    await this.HandleAddresses(addresses, ACTION_MODIFY);
  },
  async delete(addresses) {
    await this.HandleAddresses(addresses, ACTION_DELETE);
  },
  async verify(addresses) {
    await this.HandleAddresses(addresses, ACTION_VERIFY);
  },
  async verifyNot(addresses) {
    await this.HandleAddresses(addresses, ACTION_VERIFY_NOT);
  },
};

var Bookmarks = {
  async add(bookmarks) {
    await TPS.HandleBookmarks(bookmarks, ACTION_ADD);
  },
  async modify(bookmarks) {
    await TPS.HandleBookmarks(bookmarks, ACTION_MODIFY);
  },
  async delete(bookmarks) {
    await TPS.HandleBookmarks(bookmarks, ACTION_DELETE);
  },
  async verify(bookmarks) {
    await TPS.HandleBookmarks(bookmarks, ACTION_VERIFY);
  },
  async verifyNot(bookmarks) {
    await TPS.HandleBookmarks(bookmarks, ACTION_VERIFY_NOT);
  },
  skipValidation() {
    TPS.shouldValidateBookmarks = false;
  },
};

var CreditCards = {
  async add(creditCards) {
    await this.HandleCreditCards(creditCards, ACTION_ADD);
  },
  async modify(creditCards) {
    await this.HandleCreditCards(creditCards, ACTION_MODIFY);
  },
  async delete(creditCards) {
    await this.HandleCreditCards(creditCards, ACTION_DELETE);
  },
  async verify(creditCards) {
    await this.HandleCreditCards(creditCards, ACTION_VERIFY);
  },
  async verifyNot(creditCards) {
    await this.HandleCreditCards(creditCards, ACTION_VERIFY_NOT);
  },
};

var Formdata = {
  async add(formdata) {
    await this.HandleForms(formdata, ACTION_ADD);
  },
  async delete(formdata) {
    await this.HandleForms(formdata, ACTION_DELETE);
  },
  async verify(formdata) {
    await this.HandleForms(formdata, ACTION_VERIFY);
  },
  async verifyNot(formdata) {
    await this.HandleForms(formdata, ACTION_VERIFY_NOT);
  },
};

var History = {
  async add(history) {
    await this.HandleHistory(history, ACTION_ADD);
  },
  async delete(history) {
    await this.HandleHistory(history, ACTION_DELETE);
  },
  async verify(history) {
    await this.HandleHistory(history, ACTION_VERIFY);
  },
  async verifyNot(history) {
    await this.HandleHistory(history, ACTION_VERIFY_NOT);
  },
};

var Passwords = {
  async add(passwords) {
    await this.HandlePasswords(passwords, ACTION_ADD);
  },
  async modify(passwords) {
    await this.HandlePasswords(passwords, ACTION_MODIFY);
  },
  async delete(passwords) {
    await this.HandlePasswords(passwords, ACTION_DELETE);
  },
  async verify(passwords) {
    await this.HandlePasswords(passwords, ACTION_VERIFY);
  },
  async verifyNot(passwords) {
    await this.HandlePasswords(passwords, ACTION_VERIFY_NOT);
  },
  skipValidation() {
    TPS.shouldValidatePasswords = false;
  },
};

var Prefs = {
  async modify(prefs) {
    await TPS.HandlePrefs(prefs, ACTION_MODIFY);
  },
  async verify(prefs) {
    await TPS.HandlePrefs(prefs, ACTION_VERIFY);
  },
};

var Tabs = {
  async add(tabs) {
    await TPS.HandleTabs(tabs, ACTION_ADD);
  },
  async verify(tabs) {
    await TPS.HandleTabs(tabs, ACTION_VERIFY);
  },
  async verifyNot(tabs) {
    await TPS.HandleTabs(tabs, ACTION_VERIFY_NOT);
  },
};

var Windows = {
  async add(aWindow) {
    await TPS.HandleWindows(aWindow, ACTION_ADD);
  },
};

// Jumping through loads of hoops via calling back into a "HandleXXX" method
// and adding an ACTION_XXX indirection adds no value - let's KISS!
// eslint-disable-next-line no-unused-vars
var ExtStorage = {
  async set(id, data) {
    Logger.logInfo(`setting data for '${id}': ${data}`);
    await extensionStorageSync.set({ id }, data);
  },
  async verify(id, keys, data) {
    let got = await extensionStorageSync.get({ id }, keys);
    Logger.AssertEqual(got, data, `data for '${id}'/${keys}`);
  },
};

// Initialize TPS
TPS._init();
