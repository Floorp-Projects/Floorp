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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/trackers.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/type_records/tabs.js");
Cu.import("resource://weave/engines/clientData.js");

Function.prototype.async = Async.sugar;

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
    let self = yield;
    this.resetLastSync();
    this._store.wipe();
  }
};


function TabStore() {
  this._TabStore_init();
}
TabStore.prototype = {
  __proto__: Store.prototype,
  _logName: "Tabs.Store",
  _filePath: "weave/meta/tabSets.json",
  _remoteClients: {},

  _TabStore_init: function TabStore__init() {
    // TODO: This gets dumped twice.  Is TabStore initialized twice?
    dump("Initializing TabStore!!\n");
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
    // use JSON service to serialize the records...
    this._log.debug("Writing out to file...");
    let file = Utils.getProfileFile(
      {path: this._filePath, autoCreate: true});
    let jsonObj = {};
    for (let id in this._remoteClients) {
      jsonObj[id] = this._remoteClients[id].toJson();
    }
    let [fos] = Utils.open(file, ">");
    fos.writeString(this._json.encode(jsonObj));
    fos.close();
  },

  _readFromFile: function TabStore_readFromFile() {
    // use JSON service to un-serialize the records...
    // call on initialization.
    // Put stuff into remoteClients.
    this._log.debug("Reading in from file...");
    let file = Utils.getProfileFile(this._filePath);
    if (!file.exists())
      return;
    try {
      let [is] = Utils.open(file, "<");
      let json = Utils.readStream(is);
      is.close();
      let jsonObj = this._json.decode(json);
      for (let id in jsonObj) {
	this._remoteClients[id] = new TabSetRecord();
	this._remoteClients[id].fromJson(jsonObj[id]);
	this._remoteClients[id].id = id;
      }
    } catch (e) {
      this._log.warn("Failed to load saved tabs file" + e);
    }
  },

  get _sessionStore() {
    let sessionStore = Cc["@mozilla.org/browser/sessionstore;1"].
		       getService(Ci.nsISessionStore);
    this.__defineGetter__("_sessionStore", function() { return sessionStore;});
    return this._sessionStore;
  },

  get _json() {
    let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    this.__defineGetter__("_json", function() {return json;});
    return this._json;
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
    let session = this._json.decode(this._sessionStore.getBrowserState());
    for (let i = 0; i < session.windows.length; i++) {
      let window = session.windows[i];
      /* For some reason, session store uses one-based array index references,
        (f.e. in the "selectedWindow" and each tab's "index" properties), so we
        convert them to and from JavaScript's zero-based indexes as needed. */
      let windowID = i + 1;

      for (let j = 0; j < window.tabs.length; j++) {
        let tab = window.tabs[j];
	//this._sessionStore.getTabState(tab));
	// Skip empty (i.e. just-opened, no history yet) tabs:
	if (tab.entries.length == 0)
	  continue;
	let currentPage = tab.entries[tab.entries.length - 1];
	/* TODO not always accurate -- if you've hit Back in this tab, then the current
	 * page might not be the last entry.  Deal with this later.
	 */
        this._log.debug("Wrapping a tab with title " + currentPage.title);
        let urlHistory = [];
	// Include URLs in reverse order; max out at 10, and skip nulls.
	for (let i = tab.entries.length -1; i >= 0; i--) {
          let entry = tab.entries[i];
	  if (entry && entry.url)
	    urlHistory.push(entry.url);
	  if (urlHistory.length >= 10)
	    break;
	}
        record.addTab(currentPage.title, urlHistory);
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
      // TODO how to get older entries in urlHistory?
      dump("Making tab with title = " + title + ", url = " + url + "\n");
      record.addTab(title, urlHistory);
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

  changeItemId: function TabStore_changeItemId(oldId, newId) {
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
    this._log.debug("Create called.  Adding remote client record for ");
    this._log.debug(record.getClientName());
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
  _logName: "TabTracker",
  file: "tab_tracker",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  _TabTracker_init: function TabTracker__init() {
    this._init();

    // Register me as an observer!!  Listen for tabs opening and closing:
    // TODO We need to also register with any windows that are ALREDY
    // open.  On Fennec maybe try to get this from getBrowser(), which is
    // defined differently but should still exist...
    var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"]
	       .getService(Ci.nsIWindowWatcher);
    ww.registerNotification(this);
  },

  observe: function TabTracker_observe(aSubject, aTopic, aData) {
    dump("TabTracker spotted window open/close...\n");
    let window = aSubject.QueryInterface(Ci.nsIDOMWindow);
    // Ignore windows that don't have tabContainers.
    // TODO: Fennec windows don't have tabContainers, but we still want
    // to register an observer in them.
    if (! window.getBrowser)
      return;
    let browser = window.getBrowser();
    if (! browser.tabContainer)
      return;
    let container = browser.tabContainer;
    if (aTopic == "domwindowopened") {
      container.addEventListener("TabOpen", this.onTabChanged, false);
      container.addEventListener("TabClose", this.onTabChanged, false);
    } else if (aTopic == "domwindowclosed") {
      container.removeEventListener("TabOpen", this.onTabChanged, false);
      container.removeEventListener("TabClose", this.onTabChanged, false);
    }
    // TODO
  },

  onTabChanged: function TabTracker_onTabChanged(event) {
    this._score += 10; // meh?  meh.
  },

  get changedIDs() {
    // The record for my own client is always the only changed record.
    let obj = {};
    obj[Clients.clientID] = true;
    return obj;
  },

  // TODO this hard-coded score is a hack; replace with maybe +25 or +35
  // per tab open event.
  get score() {
    return 100;
  }
}
