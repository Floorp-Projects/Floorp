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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Myk Melez <myk@mozilla.org>
 *  Jono DiCarlo <jdicarlo@mozilla.com>
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

const EXPORTED_SYMBOLS = ['TabEngine'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const TAB_TIME_ATTR = "weave.tabEngine.lastUsed.timeStamp";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/type_records/tabs.js");
Cu.import("resource://weave/engines/clientData.js");

function TabEngine() {
  this._init();
}
TabEngine.prototype = {
  __proto__: SyncEngine.prototype,
  name: "tabs",
  displayName: "Tabs",
  logName: "Tabs",
  _storeObj: TabStore,
  _trackerObj: TabTracker,
  _recordObj: TabSetRecord,

  // API for use by Weave UI code to give user choices of tabs to open:
  getAllClients: function TabEngine_getAllClients() {
    return this._store._remoteClients;
  },

  getClientById: function TabEngine_getClientById(id) {
    return this._store._remoteClients[id];
  },

  _resetClient: function TabEngine__resetClient() {
    this.resetLastSync();
    this._store.wipe();
  },

  /* The intent is not to show tabs in the menu if they're already
   * open locally.  There are a couple ways to interpret this: for
   * instance, we could do it by removing a tab from the list when
   * you open it -- but then if you close it, you can't get back to
   * it.  So the way I'm doing it here is to not show a tab in the menu
   * if you have a tab open to the same URL, even though this means
   * that as soon as you navigate anywhere, the original tab will
   * reappear in the menu.
   */
  locallyOpenTabMatchesURL: function TabEngine_localTabMatches(url) {
    // url should be string, not object
    /* Some code duplication from _addFirefoxTabsToRecord and
     * _addFennecTabsToRecord.  Unify? */
    if (Cc["@mozilla.org/browser/sessionstore;1"])  {
      let state = this._store._sessionStore.getBrowserState();
      let session = JSON.parse(state);
      for (let i = 0; i < session.windows.length; i++) {
        let window = session.windows[i];
        for (let j = 0; j < window.tabs.length; j++) {
          let tab = window.tabs[j];
          if (tab.entries.length > 0) {
            let tabUrl = tab.entries[tab.entries.length-1].url;
            if (tabUrl == url) {
              return true;
            }
          }
        }
      }
    } else {
      let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
	.getService(Ci.nsIWindowMediator);
      let browserWindow = wm.getMostRecentWindow("navigator:browser");
      for each (let tab in browserWindow.Browser._tabs ) {
        let tabUrl = tab.browser.contentWindow.location.toString();
        if (tabUrl == url) {
          return true;
        }
      }
    }
    return false;
  }
};


function TabStore() {
  this._TabStore_init();
}
TabStore.prototype = {
  __proto__: Store.prototype,
  name: "tabs",
  _logName: "Tabs.Store",
  _filePath: "meta/tabSets",
  _remoteClients: {},

  _TabStore_init: function TabStore__init() {
    this._init();
    this._readFromFile();
  },

  get _localClientGUID() {
    return Clients.clientID;
  },

  get _localClientName() {
    return Clients.clientName;
  },

  _writeToFile: function TabStore_writeToFile() {
    let json = {};
    for (let [id, val] in Iterator(this._remoteClients))
      json[id] = val.toJson();

    Utils.jsonSave(this._filePath, this, json);
  },

  _readFromFile: function TabStore_readFromFile() {
    Utils.jsonLoad(this._filePath, this, function(json) {
      for (let [id, val] in Iterator(json)) {
	this._remoteClients[id] = new TabSetRecord();
	this._remoteClients[id].fromJson(val);
	this._remoteClients[id].id = id;
      }
    });
  },

  get _sessionStore() {
    let sessionStore = Cc["@mozilla.org/browser/sessionstore;1"].
		       getService(Ci.nsISessionStore);
    this.__defineGetter__("_sessionStore", function() { return sessionStore;});
    return this._sessionStore;
  },

  get _windowMediator() {
    let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
                 .getService(Ci.nsIWindowMediator);
    this.__defineGetter__("_windowMediator", function() { return wm;});
    return this._windowMediator;
  },

  _createLocalClientTabSetRecord: function TabStore__createLocalTabSet() {
    // Test for existence of sessionStore.  If it doesn't exist, then
    // use get _fennecTabs instead.
    let record = new TabSetRecord();
    record.setClientName( this._localClientName );

    if (Cc["@mozilla.org/browser/sessionstore;1"])  {
      this._addFirefoxTabsToRecord(record);
    } else {
      this._addFennecTabsToRecord(record);
    }
    return record;
  },

  _addFirefoxTabsToRecord: function TabStore__addFirefoxTabs(record) {
    // Iterate through each tab of each window
    let enumerator = this._windowMediator.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      let window = enumerator.getNext();
      let tabContainer = window.getBrowser().tabContainer;

      // Grab each tab child from the array-like child NodeList
      for each (let tab in Array.slice(tabContainer.childNodes)) {
        if (!(tab instanceof Ci.nsIDOMNode))
          continue;

        let tabState = JSON.parse(this._sessionStore.getTabState(tab));
	// Skip empty (i.e. just-opened, no history yet) tabs:
	if (tabState.entries.length == 0)
	  continue;

        // Get the time the tab was last used
        let lastUsedTimestamp = tab.getAttribute(TAB_TIME_ATTR);

        // Get title of current page
	let currentPage = tabState.entries[tabState.entries.length - 1];
	/* TODO not always accurate -- if you've hit Back in this tab,
         * then the current page might not be the last entry. Deal
         * with this later. */

        // Get url history
        let urlHistory = [];
	// Include URLs in reverse order; max out at 10, and skip nulls.
	for (let i = tabState.entries.length -1; i >= 0; i--) {
          let entry = tabState.entries[i];
	  if (entry && entry.url)
	    urlHistory.push(entry.url);
	  if (urlHistory.length >= 10)
	    break;
	}

        // add tab to record
        this._log.debug("Wrapping a tab with title " + currentPage.title);
        this._log.trace("And timestamp " + lastUsedTimestamp);
        record.addTab(currentPage.title, urlHistory, lastUsedTimestamp);
      }
    }
  },

  _addFennecTabsToRecord: function TabStore__addFennecTabs(record) {
    let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
	       .getService(Ci.nsIWindowMediator);
    let browserWindow = wm.getMostRecentWindow("navigator:browser");
    for each (let tab in browserWindow.Browser._tabs ) {
      let title = tab.browser.contentDocument.title;
      let url = tab.browser.contentWindow.location.toString();
      let urlHistory = [url];

      // TODO how to get older entries in urlHistory? without session store?
      // can we use BrowserUI._getHistory somehow?

      // Get the time the tab was last used
      /* let lastUsedTimestamp = tab.getAttribute(TAB_TIME_ATTR);
      if (!lastUsedTimestamp) */
      // TODO that doesn't work: tab.getAttribute is not a function on Fennec.
      let lastUsedTimestamp = "0";

      this._log.debug("Wrapping a tab with title " + title);
      this._log.trace("And timestamp " + lastUsedTimestamp);
      record.addTab(title, urlHistory, lastUsedTimestamp);
      // TODO add last-visited date for this tab... but how?
    }
  },

  itemExists: function TabStore_itemExists(id) {
    if (id == this._localClientGUID) {
      return true;
    } else if (this._remoteClients[id]) {
      return true;
    } else {
      return false;
    }
  },

  createRecord: function TabStore_createRecord(id, cryptoMetaURL) {
    let record;
    if (id == this._localClientGUID) {
      record = this._createLocalClientTabSetRecord();
    } else {
      record = this._remoteClients[id];
    }
    record.id = id;
    record.encryption = cryptoMetaURL;
    return record;
  },

  changeItemID: function TabStore_changeItemId(oldId, newId) {
    if (this._remoteClients[oldId]) {
      let record = this._remoteClients[oldId];
      record.id = newId;
      delete this._remoteClients[oldId];
      this._remoteClients[newId] = record;
    }
  },

  getAllIDs: function TabStore_getAllIds() {
    let items = {};
    items[ this._localClientGUID ] = true;
    for (let id in this._remoteClients) {
      items[id] = true;
    }
    return items;
  },

  wipe: function TabStore_wipe() {
    this._log.debug("Wipe called.  Clearing cache of remote client tabs.");
    this._remoteClients = {};
  },

  create: function TabStore_create(record) {
    if (record.id == this._localClientGUID)
      return; // can't happen?
    this._log.debug("Adding remote tabs from " + record.getClientName());
    this._remoteClients[record.id] = record;
    this._writeToFile();
    // TODO writing to file after every change is inefficient.  How do we
    // make sure to do it (or at least flush it) only after sync is done?
    // override syncFinished
  },

  update: function TabStore_update(record) {
    if (record.id == this._localClientGUID)
      return; // can't happen?
    this._log.debug("Update called.  Updating remote client record for");
    this._log.debug(record.getClientName());
    this._remoteClients[record.id] = record;
    this._writeToFile();
  },

  remove: function TabStore_remove(record) {
    if (record.id == this._localClientGUID)
      return; // can't happen?
    this._log.debug("Remove called.  Deleting record with id " + record.id);
    delete this._remoteClients[record.id];
    this._writeToFile();
  }

};


function TabTracker() {
  this._TabTracker_init();
}
TabTracker.prototype = {
  __proto__: Tracker.prototype,
  name: "tabs",
  _logName: "TabTracker",
  file: "tab_tracker",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  _TabTracker_init: function TabTracker__init() {
    this._init();

    // Make sure "this" pointer is always set correctly for event listeners
    this.onTabOpened = Utils.bind2(this, this.onTabOpened);
    this.onTabClosed = Utils.bind2(this, this.onTabClosed);
    this.onTabSelected = Utils.bind2(this, this.onTabSelected);

    // TODO Figure out how this will work on Fennec.

    // Register as an observer so we can catch windows opening and closing:
    var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"]
	       .getService(Ci.nsIWindowWatcher);
    ww.registerNotification(this);

    /* Also directly register the listeners for any browser window alread
     * open: */
    let wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                   .getService(Components.interfaces.nsIWindowMediator);
    let enumerator = wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
      this._registerListenersForWindow(enumerator.getNext());
    }
  },

  _getBrowser: function TabTracker__getBrowser(window) {
    // Make sure the window is browser-like
    if (typeof window.getBrowser != "function")
      return null;

    // Make sure it's a tabbrowser-like window
    let browser = window.getBrowser();
    if (browser == null || typeof browser.tabContainer != "object")
      return null;

    return browser;
  },

  _registerListenersForWindow: function TabTracker__registerListen(window) {
    let browser = this._getBrowser(window);
    if (browser == null)
      return;

    //this._log.trace("Registering tab listeners in new window.\n");
    //dump("Tab listeners registered!\n");
    let container = browser.tabContainer;
    container.addEventListener("TabOpen", this.onTabOpened, false);
    container.addEventListener("TabClose", this.onTabClosed, false);
    container.addEventListener("TabSelect", this.onTabSelected, false);
  },

  _unRegisterListenersForWindow: function TabTracker__unregister(window) {
    let browser = this._getBrowser(window);
    if (browser == null)
      return;

    let container = browser.tabContainer;
    container.removeEventListener("TabOpen", this.onTabOpened, false);
    container.removeEventListener("TabClose", this.onTabClosed, false);
    container.removeEventListener("TabSelect", this.onTabSelected, false);
  },

  observe: function TabTracker_observe(aSubject, aTopic, aData) {
    /* Called when a window opens or closes.  Make sure that every
     * window has the appropriate listeners registered. */
    let window = aSubject.QueryInterface(Ci.nsIDOMWindow);
    // TODO figure out how this will work in Fennec.
    if (aTopic == "domwindowopened") {
      this._registerListenersForWindow(window);
    } else if (aTopic == "domwindowclosed") {
      this._unRegisterListenersForWindow(window);
    }
  },

  onTabOpened: function TabTracker_onTabOpened(event) {
    // Store a timestamp in the tab to track when it was last used
    this._log.trace("Tab opened.");
    event.target.setAttribute(TAB_TIME_ATTR, event.timeStamp);
    //this._log.debug("Tab timestamp set to " + event.target.getAttribute(TAB_TIME_ATTR) + "\n");
    this._score += 50;
  },

  onTabClosed: function TabTracker_onTabSelected(event) {
    //this._log.trace("Tab closed.\n");
    this._score += 10;
  },

  onTabSelected: function TabTracker_onTabSelected(event) {
    // Update the tab's timestamp
    this._log.trace("Tab selected.");
    //this._log.trace("Tab selected.\n");
    event.target.setAttribute(TAB_TIME_ATTR, event.timeStamp);
    //this._log.debug("Tab timestamp set to " + event.target.getAttribute(TAB_TIME_ATTR) + "\n");
    this._score += 10;
  },
  // TODO: Also listen for tabs loading new content?

  get changedIDs() {
    // The record for my own client is always the only changed record.
    let obj = {};
    obj[Clients.clientID] = true;
    return obj;
  },

  /* Score is pegged to 100, which means tabs are always synced.
   * Is this the right thing to do?  Or should we be using the score
   * calculated from tab open/close/select events (see above)?  Note that
   * we should definitely listen for tabs loading new content if we want to
   * go that way.  But tabs loading new content happens so often that it
   * might be easier to just always sync.
   */
  get score() {
    return 100;
  }
}
