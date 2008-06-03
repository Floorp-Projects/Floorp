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

const EXPORTED_SYMBOLS = ['Store', 'SnapshotStore',
			  'FormStore',
			  'TabStore'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

/*
 * Data Stores
 * These can wrap, serialize items and apply commands
 */

function Store() {
  this._init();
}
Store.prototype = {
  _logName: "Store",
  _yieldDuringApply: true,

  __json: null,
  get _json() {
    if (!this.__json)
      this.__json = Cc["@mozilla.org/dom/json;1"].
        createInstance(Ci.nsIJSON);
    return this.__json;
  },

  _init: function Store__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
  },

  applyCommands: function Store_applyCommands(commandList) {
    let self = yield;
    let timer, listener;

    if (this._yieldDuringApply) {
      listener = new Utils.EventListener(self.cb);
      timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }

    for (var i = 0; i < commandList.length; i++) {
      if (this._yieldDuringApply) {
        timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
        yield; // Yield to main loop
      }
      var command = commandList[i];
      this._log.trace("Processing command: " + this._json.encode(command));
      switch (command["action"]) {
      case "create":
        this._createCommand(command);
        break;
      case "remove":
        this._removeCommand(command);
        break;
      case "edit":
        this._editCommand(command);
        break;
      default:
        this._log.error("unknown action in command: " + command["action"]);
        break;
      }
    }
    self.done();
  },

  // override these in derived objects
  wrap: function Store_wrap() {},
  wipe: function Store_wipe() {},
  resetGUIDs: function Store_resetGUIDs() {}
};

function SnapshotStore(name) {
  this._init(name);
}
SnapshotStore.prototype = {
  _logName: "SnapStore",

  _filename: null,
  get filename() {
    if (this._filename === null)
      throw "filename is null";
    return this._filename;
  },
  set filename(value) {
    this._filename = value + ".json";
  },

  __dirSvc: null,
  get _dirSvc() {
    if (!this.__dirSvc)
      this.__dirSvc = Cc["@mozilla.org/file/directory_service;1"].
        getService(Ci.nsIProperties);
    return this.__dirSvc;
  },

  // Last synced tree, version, and GUID (to detect if the store has
  // been completely replaced and invalidate the snapshot)

  _data: {},
  get data() { return this._data; },
  set data(value) { this._data = value; },

  _version: 0,
  get version() { return this._version; },
  set version(value) { this._version = value; },

  _GUID: null,
  get GUID() {
    if (!this._GUID) {
      let uuidgen = Cc["@mozilla.org/uuid-generator;1"].
        getService(Ci.nsIUUIDGenerator);
      this._GUID = uuidgen.generateUUID().toString().replace(/[{}]/g, '');
    }
    return this._GUID;
  },
  set GUID(GUID) {
    this._GUID = GUID;
  },

  _init: function SStore__init(name) {
    this.filename = name;
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
  },

  _createCommand: function SStore__createCommand(command) {
    this._data[command.GUID] = Utils.deepCopy(command.data);
  },

  _removeCommand: function SStore__removeCommand(command) {
    delete this._data[command.GUID];
  },

  _editCommand: function SStore__editCommand(command) {
    if ("GUID" in command.data) {
      // special-case guid changes
      let newGUID = command.data.GUID,
      oldGUID = command.GUID;

      this._data[newGUID] = this._data[oldGUID];
      delete this._data[oldGUID]

      for (let GUID in this._data) {
        if (this._data[GUID].parentGUID == oldGUID)
          this._data[GUID].parentGUID = newGUID;
      }
    }
    for (let prop in command.data) {
      if (prop == "GUID")
        continue;
      this._data[command.GUID][prop] = command.data[prop];
    }
  },

  save: function SStore_save() {
    this._log.info("Saving snapshot to disk");

    let file = this._dirSvc.get("ProfD", Ci.nsIFile);
    file.QueryInterface(Ci.nsILocalFile);

    file.append("weave");
    file.append("snapshots");
    file.append(this.filename);
    if (!file.exists())
      file.create(file.NORMAL_FILE_TYPE, PERMS_FILE);

    let out = {version: this.version,
               GUID: this.GUID,
               snapshot: this.data};
    out = this._json.encode(out);

    let [fos] = Utils.open(file, ">");
    fos.writeString(out);
    fos.close();
  },

  load: function SStore_load() {
    let file = this._dirSvc.get("ProfD", Ci.nsIFile);
    file.append("weave");
    file.append("snapshots");
    file.append(this.filename);

    if (!file.exists())
      return;

    try {
      let [is] = Utils.open(file, "<");
      let json = Utils.readStream(is);
      is.close();
      json = this._json.decode(json);

      if (json && 'snapshot' in json && 'version' in json && 'GUID' in json) {
        this._log.info("Read saved snapshot from disk");
        this.data = json.snapshot;
        this.version = json.version;
        this.GUID = json.GUID;
      }
    } catch (e) {
      this._log.warn("Could not parse saved snapshot; reverting to initial-sync state");
      this._log.debug("Exception: " + e);
    }
  },

  serialize: function SStore_serialize() {
    let json = this._json.encode(this.data);
    json = json.replace(/:{type/g, ":\n\t{type");
    json = json.replace(/}, /g, "},\n  ");
    json = json.replace(/, parentGUID/g, ",\n\t parentGUID");
    json = json.replace(/, index/g, ",\n\t index");
    json = json.replace(/, title/g, ",\n\t title");
    json = json.replace(/, URI/g, ",\n\t URI");
    json = json.replace(/, tags/g, ",\n\t tags");
    json = json.replace(/, keyword/g, ",\n\t keyword");
    return json;
  },

  wrap: function SStore_wrap() {
    return this.data;
  },

  wipe: function SStore_wipe() {
    this.data = {};
    this.version = -1;
    this.GUID = null;
    this.save();
  }
};
SnapshotStore.prototype.__proto__ = new Store();

function FormStore() {
  this._init();
}
FormStore.prototype = {
  _logName: "FormStore",

  __formDB: null,
  get _formDB() {
    if (!this.__formDB) {
      var file = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties).
                 get("ProfD", Ci.nsIFile);
      file.append("formhistory.sqlite");
      var stor = Cc["@mozilla.org/storage/service;1"].
                 getService(Ci.mozIStorageService);
      this.__formDB = stor.openDatabase(file);
    }
    return this.__formDB;
  },

  __formHistory: null,
  get _formHistory() {
    if (!this.__formHistory)
      this.__formHistory = Cc["@mozilla.org/satchel/form-history;1"].
                           getService(Ci.nsIFormHistory2);
    return this.__formHistory;
  },

  _createCommand: function FormStore__createCommand(command) {
    this._log.info("FormStore got createCommand: " + command );
    this._formHistory.addEntry(command.data.name, command.data.value);
  },

  _removeCommand: function FormStore__removeCommand(command) {
    this._log.info("FormStore got removeCommand: " + command );
    this._formHistory.removeEntry(command.data.name, command.data.value);
  },

  _editCommand: function FormStore__editCommand(command) {
    this._log.info("FormStore got editCommand: " + command );
    this._log.warn("Form syncs are expected to only be create/remove!");
  },

  wrap: function FormStore_wrap() {
    var items = [];
    var stmnt = this._formDB.createStatement("SELECT * FROM moz_formhistory");

    while (stmnt.executeStep()) {
      var nam = stmnt.getUTF8String(1);
      var val = stmnt.getUTF8String(2);
      var key = Utils.sha1(nam + val);

      items[key] = { name: nam, value: val };
    }

    return items;
  },

  wipe: function FormStore_wipe() {
    this._formHistory.removeAllEntries();
  },

  resetGUIDs: function FormStore_resetGUIDs() {
    // Not needed.
  }
};
FormStore.prototype.__proto__ = new Store();

function TabStore() {
  this._virtualTabs = {};
  this._init();
}
TabStore.prototype = {
  __proto__: new Store(),

  _logName: "TabStore",

  get _sessionStore() {
    let sessionStore = Cc["@mozilla.org/browser/sessionstore;1"].
		       getService(Ci.nsISessionStore);
    this.__defineGetter__("_sessionStore", function() sessionStore);
    return this._sessionStore;
  },

  get _windowMediator() {
    let windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].
			 getService(Ci.nsIWindowMediator);
    this.__defineGetter__("_windowMediator", function() windowMediator);
    return this._windowMediator;
  },

  get _os() {
    let os = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);
    this.__defineGetter__("_os", function() os);
    return this._os;
  },

  get _dirSvc() {
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties);
    this.__defineGetter__("_dirSvc", function() dirSvc);
    return this._dirSvc;
  },

  /**
   * A cache of "virtual" tabs from other devices synced to the server
   * that the user hasn't opened locally.  Unlike other stores, we don't
   * immediately apply create commands, which would be jarring to users.
   * Instead, we store them in this cache and prompt the user to pick
   * which ones she wants to open.
   *
   * We also persist this cache on disk and include it in the list of tabs
   * we generate in this.wrap to reduce ping-pong updates between clients
   * running simultaneously and to maintain a consistent state across restarts.
   */
  _virtualTabs: null,

  get virtualTabs() {
    // Make sure the list of virtual tabs is completely up-to-date (the user
    // might have independently opened some of these virtual tabs since the last
    // time we synced).
    let realTabs = this._wrapRealTabs();
    let virtualTabsChanged = false;
    for (let id in this._virtualTabs) {
      if (id in realTabs) {
        this._log.warn("get virtualTabs: both real and virtual tabs exist for "
                       + id + "; removing virtual one");
        delete this._virtualTabs[id];
        virtualTabsChanged = true;
      }
    }
    if (virtualTabsChanged)
      this._saveVirtualTabs();

    return this._virtualTabs;
  },

  set virtualTabs(newValue) {
    this._virtualTabs = newValue;
    this._saveVirtualTabs();
  },

  // The file in which we store the state of virtual tabs.
  get _file() {
    let file = this._dirSvc.get("ProfD", Ci.nsILocalFile);
    file.append("weave");
    file.append("store");
    file.append("tabs");
    file.append("virtual.json");
    this.__defineGetter__("_file", function() file);
    return this._file;
  },

  _saveVirtualTabs: function TabStore__saveVirtualTabs() {
    try {
      if (!this._file.exists())
        this._file.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
      let out = this._json.encode(this._virtualTabs);
      let [fos] = Utils.open(this._file, ">");
      fos.writeString(out);
      fos.close();
    }
    catch(ex) {
      this._log.warn("could not serialize virtual tabs to disk: " + ex);
    }
  },

  _restoreVirtualTabs: function TabStore__restoreVirtualTabs() {
    try {
      if (this._file.exists()) {
        let [is] = Utils.open(this._file, "<");
        let json = Utils.readStream(is);
        is.close();
        this._virtualTabs = this._json.decode(json);
      }
    }
    catch (ex) {
      this._log.warn("could not parse virtual tabs from disk: " + ex);
    }
  },

  _init: function TabStore__init() {
    this._restoreVirtualTabs();

    this.__proto__.__proto__._init();
  },

  /**
   * Apply commands generated by a diff during a sync operation.  This method
   * overrides the one in its superclass so it can save a copy of the latest set
   * of virtual tabs to disk so they can be restored on startup.
   */
  applyCommands: function TabStore_applyCommands(commandList) {
    let self = yield;

    this.__proto__.__proto__.applyCommands.async(this, self.cb, commandList);
    yield;

    this._saveVirtualTabs();

    self.done();
  },

  _createCommand: function TabStore__createCommand(command) {
    this._log.debug("_createCommand: " + command.GUID);

    if (command.GUID in this._virtualTabs || command.GUID in this._wrapRealTabs())
      throw "trying to create a tab that already exists; id: " + command.GUID;

    // Cache the tab and notify the UI to prompt the user to open it.
    this._virtualTabs[command.GUID] = command.data;
    this._os.notifyObservers(null, "weave:store:tabs:virtual:created", null);
  },

  _removeCommand: function TabStore__removeCommand(command) {
    this._log.debug("_removeCommand: " + command.GUID);

    // If this is a virtual tab, it's ok to remove it, since it was never really
    // added to this session in the first place.  But we don't remove it if it's
    // a real tab, since that would be unexpected, unpleasant, and unwanted.
    if (command.GUID in this._virtualTabs) {
      delete this._virtualTabs[command.GUID];
      this._os.notifyObservers(null, "weave:store:tabs:virtual:removed", null);
    }
  },

  _editCommand: function TabStore__editCommand(command) {
    this._log.debug("_editCommand: " + command.GUID);

    // We don't edit real tabs, because that isn't what the user would expect,
    // but it's ok to edit virtual tabs, so that if users do open them, they get
    // the most up-to-date version of them (and also to reduce sync churn).

    if (this._virtualTabs[command.GUID])
      this._virtualTabs[command.GUID] = command.data;
  },

  /**
   * Serialize the current state of tabs.
   *
   * Note: the state includes both tabs on this device and those on others.
   * We get the former from the session store.  The latter we retrieved from
   * the Weave server and stored in this._virtualTabs.  Including virtual tabs
   * in the serialized state prevents ping-pong deletes between two clients
   * running at the same time.
   */
  wrap: function TabStore_wrap() {
    let items;

    let virtualTabs = this._wrapVirtualTabs();
    let realTabs = this._wrapRealTabs();

    // Real tabs override virtual ones, which means ping-pong edits when two
    // clients have the same URL loaded with different history/attributes.
    // We could fix that by overriding real tabs with virtual ones, but then
    // we'd have stale tab metadata in same cases.
    items = virtualTabs;
    let virtualTabsChanged = false;
    for (let id in realTabs) {
      // Since virtual tabs can sometimes get out of sync with real tabs
      // (the user could have independently opened a new tab that exists
      // in the virtual tabs cache since the last time we updated the cache),
      // we sync them up in the process of merging them here.
      if (this._virtualTabs[id]) {
        this._log.warn("wrap: both real and virtual tabs exist for " + id +
                       "; removing virtual one");
        delete this._virtualTabs[id];
        virtualTabsChanged = true;
      }

      items[id] = realTabs[id];
    }
    if (virtualTabsChanged)
      this._saveVirtualTabs();

    return items;
  },

  _wrapVirtualTabs: function TabStore__wrapVirtualTabs() {
    let items = {};

    for (let id in this._virtualTabs) {
      let virtualTab = this._virtualTabs[id];

      // Copy the virtual tab without private properties (those that begin
      // with an underscore character) so that we don't sync data private to
      // this particular Weave client (like the _disposed flag).
      let item = {};
      for (let property in virtualTab)
        if (property[0] != "_")
          item[property] = virtualTab[property];

      items[id] = item;
    }

    return items;
  },

  _wrapRealTabs: function TabStore__wrapRealTabs() {
    let items = {};

    let session = this._json.decode(this._sessionStore.getBrowserState());

    for (let i = 0; i < session.windows.length; i++) {
      let window = session.windows[i];
      // For some reason, session store uses one-based array index references,
      // (f.e. in the "selectedWindow" and each tab's "index" properties), so we
      // convert them to and from JavaScript's zero-based indexes as needed.
      let windowID = i + 1;
      this._log.debug("_wrapRealTabs: window " + windowID);
      for (let j = 0; j < window.tabs.length; j++) {
        let tab = window.tabs[j];

	// The session history entry for the page currently loaded in the tab.
	// We use the URL of the current page as the ID for the tab.
	let currentEntry = tab.entries[tab.index - 1];

	if (!currentEntry || !currentEntry.url) {
	  this._log.warn("_wrapRealTabs: no current entry or no URL, can't " +
                         "identify " + this._json.encode(tab));
	  continue;
	}

	let tabID = currentEntry.url;
        this._log.debug("_wrapRealTabs: tab " + tabID);

	items[tabID] = {
          // Identify this item as a tab in case we start serializing windows
          // in the future.
	  type: "tab",

          // The position of this tab relative to other tabs in the window.
          // For consistency with session store data, we make this one-based.
          position: j + 1,

	  windowID: windowID,

	  state: tab
	};
      }
    }

    return items;
  },

  wipe: function TabStore_wipe() {
    // We're not going to close tabs, since that's probably not what
    // the user wants, but we'll clear the cache of virtual tabs.
    this._virtualTabs = {};
    this._saveVirtualTabs();
  },

  resetGUIDs: function TabStore_resetGUIDs() {
    // Not needed.
  }

};
