const EXPORTED_SYMBOLS = ['TabEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/syncCores.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/constants.js");

Function.prototype.async = Async.sugar;

function TabEngine(pbeId) {
  this._init(pbeId);
}

TabEngine.prototype = {
  __proto__: new Engine(),

  get name() "tabs",
  get logName() "TabEngine",
  get serverPrefix() "user-data/tabs/",
  get store() this._store,

  get _core() {
    let core = new TabSyncCore(this);
    this.__defineGetter__("_core", function() core);
    return this._core;
  },

  get _store() {
    let store = new TabStore();
    this.__defineGetter__("_store", function() store);
    return this._store;
  },

  get _tracker() {
    let tracker = new TabTracker(this);
    this.__defineGetter__("_tracker", function() tracker);
    return this._tracker;
  }

};

function TabSyncCore(engine) {
  this._engine = engine;
  this._init();
}
TabSyncCore.prototype = {
  __proto__: new SyncCore(),

  _logName: "TabSync",

  _engine: null,

  get _sessionStore() {
    let sessionStore = Cc["@mozilla.org/browser/sessionstore;1"].
		       getService(Ci.nsISessionStore);
    this.__defineGetter__("_sessionStore", function() sessionStore);
    return this._sessionStore;
  },

  _itemExists: function TSC__itemExists(GUID) {
    // Note: this method returns true if the tab exists in any window, not just
    // the window from which the tab came.  In the future, if we care about
    // windows, we might need to make this more specific, although in that case
    // we'll have to identify tabs by something other than URL, since even
    // window-specific tabs look the same when identified by URL.

    // Get the set of all real and virtual tabs.
    let tabs = this._engine.store.wrap();

    // XXX Should we convert both to nsIURIs and then use nsIURI::equals
    // to compare them?
    if (GUID in tabs) {
      this._log.debug("_itemExists: " + GUID + " exists");
      return true;
    }

    this._log.debug("_itemExists: " + GUID + " doesn't exist");
    return false;
  },

  _commandLike: function TSC_commandLike(a, b) {
    // Not implemented.
    return false;
  }
};

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

    this.__proto__.__proto__._init.call(this);
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

        // The ID property of each entry in the tab, which I think contains
        // nsISHEntry::ID, changes every time session store restores the tab,
        // so we can't sync them, or we would generate edit commands on every
        // restart (even though nothing has actually changed).
        for each (let entry in tab.entries)
          delete entry.ID;

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

function TabTracker(engine) {
  this._engine = engine;
  this._init();
}
TabTracker.prototype = {
  __proto__: new Tracker(),

  _logName: "TabTracker",

  _engine: null,

  get _json() {
    let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    this.__defineGetter__("_json", function() json);
    return this._json;
  },

  /**
   * There are two ways we could calculate the score.  We could calculate it
   * incrementally by using the window mediator to watch for windows opening/
   * closing and FUEL (or some other API) to watch for tabs opening/closing
   * and changing location.
   *
   * Or we could calculate it on demand by comparing the state of tabs
   * according to the session store with the state according to the snapshot.
   *
   * It's hard to say which is better.  The incremental approach is less
   * accurate if it simply increments the score whenever there's a change,
   * but it might be more performant.  The on-demand approach is more accurate,
   * but it might be less performant depending on how often it's called.
   *
   * In this case we've decided to go with the on-demand approach, and we
   * calculate the score as the percent difference between the snapshot set
   * and the current tab set, where tabs that only exist in one set are
   * completely different, while tabs that exist in both sets but whose data
   * doesn't match (f.e. because of variations in history) are considered
   * "half different".
   *
   * So if the sets don't match at all, we return 100;
   * if they completely match, we return 0;
   * if half the tabs match, and their data is the same, we return 50;
   * and if half the tabs match, but their data is all different, we return 75.
   */
  get score() {
    // The snapshot data is a singleton that we can't modify, so we have to
    // copy its unique items to a new hash.
    let snapshotData = this._engine.snapshot.data;
    let a = {};

    // The wrapped current state is a unique instance we can munge all we want.
    let b = this._engine.store.wrap();

    // An array that counts the number of intersecting IDs between a and b
    // (represented as the length of c) and whether or not their values match
    // (represented by the boolean value of each item in c).
    let c = [];

    // Generate c and update a and b to contain only unique items.
    for (id in snapshotData) {
      if (id in b) {
        c.push(this._json.encode(snapshotData[id]) == this._json.encode(b[id]));
        delete b[id];
      }
      else {
        a[id] = snapshotData[id];
      }
    }

    let numShared = c.length;
    let numUnique = [true for (id in a)].length + [true for (id in b)].length;
    let numTotal = numShared + numUnique;

    // We're going to divide by the total later, so make sure we don't try
    // to divide by zero, even though we should never be in a state where there
    // are no tabs in either set.
    if (numTotal == 0)
      return 0;

    // The number of shared items whose data is different.
    let numChanged = c.filter(function(v) !v).length;

    let fractionSimilar = (numShared - (numChanged / 2)) / numTotal;
    let fractionDissimilar = 1 - fractionSimilar;
    let percentDissimilar = Math.round(fractionDissimilar * 100);

    return percentDissimilar;
  },

  resetScore: function FormsTracker_resetScore() {
    // Not implemented, since we calculate the score on demand.
  }
}
