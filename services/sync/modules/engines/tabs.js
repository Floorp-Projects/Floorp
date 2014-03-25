/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ['TabEngine', 'TabSetRecord'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const TABS_TTL = 604800; // 7 days

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

this.TabSetRecord = function TabSetRecord(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
TabSetRecord.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.Tabs",
  ttl: TABS_TTL
};

Utils.deferGetSet(TabSetRecord, "cleartext", ["clientName", "tabs"]);


this.TabEngine = function TabEngine(service) {
  SyncEngine.call(this, "Tabs", service);

  // Reset the client on every startup so that we fetch recent tabs
  this._resetClient();
}
TabEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: TabStore,
  _trackerObj: TabTracker,
  _recordObj: TabSetRecord,

  getChangedIDs: function getChangedIDs() {
    // No need for a proper timestamp (no conflict resolution needed).
    let changedIDs = {};
    if (this._tracker.modified)
      changedIDs[this.service.clientsEngine.localID] = 0;
    return changedIDs;
  },

  // API for use by Weave UI code to give user choices of tabs to open:
  getAllClients: function TabEngine_getAllClients() {
    return this._store._remoteClients;
  },

  getClientById: function TabEngine_getClientById(id) {
    return this._store._remoteClients[id];
  },

  _resetClient: function TabEngine__resetClient() {
    SyncEngine.prototype._resetClient.call(this);
    this._store.wipe();
    this._tracker.modified = true;
  },

  removeClientData: function removeClientData() {
    let url = this.engineURL + "/" + this.service.clientsEngine.localID;
    this.service.resource(url).delete();
  },

  /**
   * Return a Set of open URLs.
   */
  getOpenURLs: function () {
    let urls = new Set();
    for (let entry of this._store.getAllTabs()) {
      urls.add(entry.urlHistory[0]);
    }
    return urls;
  }
};


function TabStore(name, engine) {
  Store.call(this, name, engine);
}
TabStore.prototype = {
  __proto__: Store.prototype,

  itemExists: function TabStore_itemExists(id) {
    return id == this.engine.service.clientsEngine.localID;
  },

  getWindowEnumerator: function () {
    return Services.wm.getEnumerator("navigator:browser");
  },

  shouldSkipWindow: function (win) {
    return win.closed ||
           PrivateBrowsingUtils.isWindowPrivate(win);
  },

  getTabState: function (tab) {
    return JSON.parse(Svc.Session.getTabState(tab));
  },

  getAllTabs: function (filter) {
    let filteredUrls = new RegExp(Svc.Prefs.get("engine.tabs.filteredUrls"), "i");

    let allTabs = [];

    let winEnum = this.getWindowEnumerator();
    while (winEnum.hasMoreElements()) {
      let win = winEnum.getNext();
      if (this.shouldSkipWindow(win)) {
        continue;
      }

      dump("WIN IS " + JSON.stringify(win)  + "\n");
      for (let tab of win.gBrowser.tabs) {
        tabState = this.getTabState(tab);

        // Make sure there are history entries to look at.
        if (!tabState || !tabState.entries.length) {
          continue;
        }

        // Until we store full or partial history, just grab the current entry.
        // index is 1 based, so make sure we adjust.
        let entry = tabState.entries[tabState.index - 1];

        // Filter out some urls if necessary. SessionStore can return empty
        // tabs in some cases - easiest thing is to just ignore them for now.
        if (!entry.url || filter && filteredUrls.test(entry.url)) {
          continue;
        }

        // I think it's also possible that attributes[.image] might not be set
        // so handle that as well.
        allTabs.push({
          title: entry.title || "",
          urlHistory: [entry.url],
          icon: tabState.attributes && tabState.attributes.image || "",
          lastUsed: Math.floor((tabState.lastAccessed || 0) / 1000)
        });
      }
    }

    return allTabs;
  },

  createRecord: function createRecord(id, collection) {
    let record = new TabSetRecord(collection, id);
    record.clientName = this.engine.service.clientsEngine.localName;

    // Sort tabs in descending-used order to grab the most recently used
    let tabs = this.getAllTabs(true).sort(function (a, b) {
      return b.lastUsed - a.lastUsed;
    });

    // Figure out how many tabs we can pack into a payload. Starting with a 28KB
    // payload, we can estimate various overheads from encryption/JSON/WBO.
    let size = JSON.stringify(tabs).length;
    let origLength = tabs.length;
    const MAX_TAB_SIZE = 20000;
    if (size > MAX_TAB_SIZE) {
      // Estimate a little more than the direct fraction to maximize packing
      let cutoff = Math.ceil(tabs.length * MAX_TAB_SIZE / size);
      tabs = tabs.slice(0, cutoff + 1);

      // Keep dropping off the last entry until the data fits
      while (JSON.stringify(tabs).length > MAX_TAB_SIZE)
        tabs.pop();
    }

    this._log.trace("Created tabs " + tabs.length + " of " + origLength);
    tabs.forEach(function (tab) {
      this._log.trace("Wrapping tab: " + JSON.stringify(tab));
    }, this);

    record.tabs = tabs;
    return record;
  },

  getAllIDs: function TabStore_getAllIds() {
    // Don't report any tabs if all windows are in private browsing for
    // first syncs.
    let ids = {};
    let allWindowsArePrivate = false;
    let wins = Services.wm.getEnumerator("navigator:browser");
    while (wins.hasMoreElements()) {
      if (PrivateBrowsingUtils.isWindowPrivate(wins.getNext())) {
        // Ensure that at least there is a private window.
        allWindowsArePrivate = true;
      } else {
        // If there is a not private windown then finish and continue.
        allWindowsArePrivate = false;
        break;
      }
    }

    if (allWindowsArePrivate &&
        !PrivateBrowsingUtils.permanentPrivateBrowsing) {
      return ids;
    }

    ids[this.engine.service.clientsEngine.localID] = true;
    return ids;
  },

  wipe: function TabStore_wipe() {
    this._remoteClients = {};
  },

  create: function TabStore_create(record) {
    this._log.debug("Adding remote tabs from " + record.clientName);
    this._remoteClients[record.id] = record.cleartext;

    // Lose some precision, but that's good enough (seconds)
    let roundModify = Math.floor(record.modified / 1000);
    let notifyState = Svc.Prefs.get("notifyTabState");
    // If there's no existing pref, save this first modified time
    if (notifyState == null)
      Svc.Prefs.set("notifyTabState", roundModify);
    // Don't change notifyState if it's already 0 (don't notify)
    else if (notifyState == 0)
      return;
    // We must have gotten a new tab that isn't the same as last time
    else if (notifyState != roundModify)
      Svc.Prefs.set("notifyTabState", 0);
  },

  update: function update(record) {
    this._log.trace("Ignoring tab updates as local ones win");
  }
};


function TabTracker(name, engine) {
  Tracker.call(this, name, engine);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);

  // Make sure "this" pointer is always set correctly for event listeners
  this.onTab = Utils.bind2(this, this.onTab);
  this._unregisterListeners = Utils.bind2(this, this._unregisterListeners);
}
TabTracker.prototype = {
  __proto__: Tracker.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  loadChangedIDs: function loadChangedIDs() {
    // Don't read changed IDs from disk at start up.
  },

  clearChangedIDs: function clearChangedIDs() {
    this.modified = false;
  },

  _topics: ["pageshow", "TabOpen", "TabClose", "TabSelect"],
  _registerListenersForWindow: function registerListenersFW(window) {
    this._log.trace("Registering tab listeners in window");
    for each (let topic in this._topics) {
      window.addEventListener(topic, this.onTab, false);
    }
    window.addEventListener("unload", this._unregisterListeners, false);
  },

  _unregisterListeners: function unregisterListeners(event) {
    this._unregisterListenersForWindow(event.target);
  },

  _unregisterListenersForWindow: function unregisterListenersFW(window) {
    this._log.trace("Removing tab listeners in window");
    window.removeEventListener("unload", this._unregisterListeners, false);
    for each (let topic in this._topics) {
      window.removeEventListener(topic, this.onTab, false);
    }
  },

  startTracking: function () {
    Svc.Obs.add("domwindowopened", this);
    let wins = Services.wm.getEnumerator("navigator:browser");
    while (wins.hasMoreElements()) {
      this._registerListenersForWindow(wins.getNext());
    }
  },

  stopTracking: function () {
    Svc.Obs.remove("domwindowopened", this);
    let wins = Services.wm.getEnumerator("navigator:browser");
    while (wins.hasMoreElements()) {
      this._unregisterListenersForWindow(wins.getNext());
    }
  },

  observe: function (subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);

    switch (topic) {
      case "domwindowopened":
        let onLoad = () => {
          subject.removeEventListener("load", onLoad, false);
          // Only register after the window is done loading to avoid unloads.
          this._registerListenersForWindow(subject);
        };

        // Add tab listeners now that a window has opened.
        subject.addEventListener("load", onLoad, false);
        break;
    }
  },

  onTab: function onTab(event) {
    if (event.originalTarget.linkedBrowser) {
      let win = event.originalTarget.linkedBrowser.contentWindow;
      if (PrivateBrowsingUtils.isWindowPrivate(win) &&
          !PrivateBrowsingUtils.permanentPrivateBrowsing) {
        this._log.trace("Ignoring tab event from private browsing.");
        return;
      }
    }

    this._log.trace("onTab event: " + event.type);
    this.modified = true;

    // For page shows, bump the score 10% of the time, emulating a partial
    // score. We don't want to sync too frequently. For all other page
    // events, always bump the score.
    if (event.type != "pageshow" || Math.random() < .1)
      this.score += SCORE_INCREMENT_SMALL;
  },
}
