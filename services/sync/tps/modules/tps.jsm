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
 * The Original Code is TPS.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Griffin <jgriffin@mozilla.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
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

 /* This is a JavaScript module (JSM) to be imported via 
  * Components.utils.import() and acts as a singleton. Only the following 
  * listed symbols will exposed on import, and only when and where imported. 
  */

var EXPORTED_SYMBOLS = ["TPS"];

const CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;

CU.import("resource://services-sync/service.js");
CU.import("resource://services-sync/constants.js");
CU.import("resource://services-sync/util.js");
CU.import("resource://gre/modules/XPCOMUtils.jsm");
CU.import("resource://gre/modules/Services.jsm");
CU.import("resource://tps/bookmarks.jsm");
CU.import("resource://tps/logger.jsm");
CU.import("resource://tps/passwords.jsm");
CU.import("resource://tps/history.jsm");
CU.import("resource://tps/forms.jsm");
CU.import("resource://tps/prefs.jsm");
CU.import("resource://tps/tabs.jsm");

const ACTION_ADD = "add";
const ACTION_VERIFY = "verify";
const ACTION_VERIFY_NOT = "verify-not";
const ACTION_MODIFY = "modify";
const ACTION_SYNC = "sync";
const ACTION_DELETE = "delete";
const ACTION_PRIVATE_BROWSING = "private-browsing";
const ACTION_WIPE_SERVER = "wipe-server";
const ACTIONS = [ACTION_ADD, ACTION_VERIFY, ACTION_VERIFY_NOT, 
                 ACTION_MODIFY, ACTION_SYNC, ACTION_DELETE,
                 ACTION_PRIVATE_BROWSING, ACTION_WIPE_SERVER];

const SYNC_WIPE_SERVER = "wipe-server";
const SYNC_RESET_CLIENT = "reset-client";
const SYNC_WIPE_CLIENT = "wipe-client";

function GetFileAsText(file)
{
  let channel = Services.io.newChannel(file, null, null);
  let inputStream = channel.open();
  if (channel instanceof CI.nsIHttpChannel && 
      channel.responseStatus != 200) {
    return "";
  }

  let streamBuf = "";
  let sis = CC["@mozilla.org/scriptableinputstream;1"]
            .createInstance(CI.nsIScriptableInputStream);
  sis.init(inputStream);

  let available;
  while ((available = sis.available()) != 0) {
    streamBuf += sis.read(available);
  }

  inputStream.close();
  return streamBuf;
}

var TPS = 
{
  _waitingForSync: false,
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
            Weave.Service.logout();
            Utils.nextTick(this.RunNextTestAction, this);
          }
          else {
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
              Weave.Service.logout();
              this.FinishAsyncOperation();
            }, 1000, this, "postsync");
          }
          break;
        case "sessionstore-windows-restored":
          Utils.nextTick(this.RunNextTestAction, this);
          break;
      }
    }
    catch(e) {
      this.DumpError("Exception caught: " + e);
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
    Logger.close();
    this.goQuitApplication();
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
  },

  HandlePasswords: function (passwords, action) {
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
  },

  HandleBookmarks: function (bookmarks, action) {
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
      action[0].call(this, action[1]);

      // if we're in an async operation, don't continue on to the next action
      if (this._operations_pending)
        return;

      this._currentAction++;
    }
    catch(e) {
      this.DumpError("Exception caught: " + e);
      return;
    }
    this.RunNextTestAction();
  },

  RunTestPhase: function (file, phase, logpath) {
    try {
      Logger.init(logpath);
      Logger.logInfo("Weave version: " + WEAVE_VERSION);
      Logger.logInfo("Firefox builddate: " + Services.appinfo.appBuildID);
      Logger.logInfo("Firefox version: " + Services.appinfo.version);

      // do some weave housekeeping
      if (Weave.Service.isLoggedIn) {
        this.DumpError("Weave logged in on startup...profile may be dirty");
        return;
      }
      
      // setup observers
      Services.obs.addObserver(this, "weave:service:sync:finish", true);
      Services.obs.addObserver(this, "weave:service:sync:error", true);
      Services.obs.addObserver(this, "sessionstore-windows-restored", true);
      Services.obs.addObserver(this, "private-browsing", true);

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
      Logger.logInfo("setting client.name to " + this.phases["phase" + this._currentPhase]);
      Weave.Svc.Prefs.set("client.name", this.phases["phase" + this._currentPhase]);

      // wipe the server at the end of the final test phase
      if (this.phases["phase" + (parseInt(this._currentPhase) + 1)] == undefined)
        this_phase.push([this.WipeServer]);
      
      // start processing the test actions
      this._currentAction = 0;
    }
    catch(e) {
      this.DumpError("Exception caught: " + e);
      return;
    }
  },

  Phase: function Test__Phase(phasename, fnlist) {
    this._phaselist[phasename] = fnlist;
  },

  SetPrivateBrowsing: function TPS__SetPrivateBrowsing(options) {
    let PBSvc = CC["@mozilla.org/privatebrowsing;1"].
                getService(CI.nsIPrivateBrowsingService);
    PBSvc.privateBrowsingEnabled = options;
    Logger.logInfo("set privateBrowsingEnabled: " + options);
  },

  Sync: function TPS__Sync(options) {
    Logger.logInfo("executing Sync " + (options ? options : ""));
    if (options == SYNC_WIPE_SERVER) {
      Weave.Svc.Prefs.set("firstSync", "wipeRemote");
    }
    else if (options == SYNC_WIPE_CLIENT) {
      Weave.Svc.Prefs.set("firstSync", "wipeClient");
    }
    else if (options == SYNC_RESET_CLIENT) {
      Weave.Svc.Prefs.set("firstSync", "resetClient");
    }
    else {
      Weave.Svc.Prefs.reset("firstSync");
    }
    if (this.config.account) {
      let account = this.config.account;
      if (account["serverURL"]) {
        Weave.Service.serverURL = account["serverURL"];
      }
      if (account["admin-secret"]) {
        // if admin-secret is specified, we'll dynamically create
        // a new sync account
        Weave.Svc.Prefs.set("admin-secret", account["admin-secret"]);
        let suffix = account["account-suffix"];
        Weave.Service.account = "tps" + suffix + "@mozilla.com";
        Weave.Service.password = "tps" + suffix + "tps" + suffix;
        Weave.Service.passphrase = Weave.Utils.generatePassphrase();
        Weave.Service.createAccount(Weave.Service.account, 
                                    Weave.Service.password,
                                    "dummy1", "dummy2");
        Weave.Service.login();
      }
      else if (account["username"] && account["password"] &&
               account["passphrase"]) {
        Weave.Service.account = account["username"];
        Weave.Service.password = account["password"];
        Weave.Service.passphrase = account["passphrase"];
        Weave.Service.login();
      }
      else {
        this.DumpError("Must specify admin-secret, or " +
          "username/password/passphrase in the config file");
        return;
      }
    }
    else {
      this.DumpError("No account information found; did you use " +
        "a valid config file?");
      return;
    }
    Logger.AssertEqual(Weave.Status.service, Weave.STATUS_OK, "Weave status not OK");
    Weave.Svc.Obs.notify("weave:service:setup-complete");
    this._waitingForSync = true;
    this.StartAsyncOperation();
    Weave.Service.sync();
  },

  WipeServer: function TPS__WipeServer() {
    Logger.logInfo("WipeServer()");
    Weave.Service.login();
    Weave.Service.wipeServer();
    Logger.AssertEqual(Weave.Status.service, Weave.STATUS_OK, "Weave status not OK");
    this._waitingForSync = true;
    this.StartAsyncOperation();
    Weave.Service.sync();
    return;
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

