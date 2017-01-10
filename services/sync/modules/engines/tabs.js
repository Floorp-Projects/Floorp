/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["TabEngine", "TabSetRecord"];

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const TABS_TTL = 604800;           // 7 days.
const TAB_ENTRIES_LIMIT = 25;      // How many URLs to include in tab history.

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
  ttl: TABS_TTL,
};

Utils.deferGetSet(TabSetRecord, "cleartext", ["clientName", "tabs"]);


this.TabEngine = function TabEngine(service) {
  SyncEngine.call(this, "Tabs", service);

  // Reset the client on every startup so that we fetch recent tabs.
  this._resetClient();
}
TabEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: TabStore,
  _trackerObj: TabTracker,
  _recordObj: TabSetRecord,
  // A flag to indicate if we have synced in this session. This is to help
  // consumers of remote tabs that may want to differentiate between "I've an
  // empty tab list as I haven't yet synced" vs "I've an empty tab list
  // as there really are no tabs"
  hasSyncedThisSession: false,

  syncPriority: 3,

  getChangedIDs() {
    // No need for a proper timestamp (no conflict resolution needed).
    let changedIDs = {};
    if (this._tracker.modified)
      changedIDs[this.service.clientsEngine.localID] = 0;
    return changedIDs;
  },

  // API for use by Sync UI code to give user choices of tabs to open.
  getAllClients() {
    return this._store._remoteClients;
  },

  getClientById(id) {
    return this._store._remoteClients[id];
  },

  _resetClient() {
    SyncEngine.prototype._resetClient.call(this);
    this._store.wipe();
    this._tracker.modified = true;
    this.hasSyncedThisSession = false;
  },

  removeClientData() {
    let url = this.engineURL + "/" + this.service.clientsEngine.localID;
    this.service.resource(url).delete();
  },

  /**
   * Return a Set of open URLs.
   */
  getOpenURLs() {
    let urls = new Set();
    for (let entry of this._store.getAllTabs()) {
      urls.add(entry.urlHistory[0]);
    }
    return urls;
  },

  _reconcile(item) {
    // Skip our own record.
    // TabStore.itemExists tests only against our local client ID.
    if (this._store.itemExists(item.id)) {
      this._log.trace("Ignoring incoming tab item because of its id: " + item.id);
      return false;
    }

    return SyncEngine.prototype._reconcile.call(this, item);
  },

  _syncFinish() {
    this.hasSyncedThisSession = true;
    return SyncEngine.prototype._syncFinish.call(this);
  },
};


function TabStore(name, engine) {
  Store.call(this, name, engine);
}
TabStore.prototype = {
  __proto__: Store.prototype,

  itemExists(id) {
    return id == this.engine.service.clientsEngine.localID;
  },

  getWindowEnumerator() {
    return Services.wm.getEnumerator("navigator:browser");
  },

  shouldSkipWindow(win) {
    return win.closed ||
           PrivateBrowsingUtils.isWindowPrivate(win);
  },

  getTabState(tab) {
    return JSON.parse(Svc.Session.getTabState(tab));
  },

  getAllTabs(filter) {
    let filteredUrls = new RegExp(Svc.Prefs.get("engine.tabs.filteredUrls"), "i");

    let allTabs = [];

    let winEnum = this.getWindowEnumerator();
    while (winEnum.hasMoreElements()) {
      let win = winEnum.getNext();
      if (this.shouldSkipWindow(win)) {
        continue;
      }

      for (let tab of win.gBrowser.tabs) {
        let tabState = this.getTabState(tab);

        // Make sure there are history entries to look at.
        if (!tabState || !tabState.entries.length) {
          continue;
        }

        let acceptable = !filter ? (url) => url :
                                   (url) => url && !filteredUrls.test(url);

        let entries = tabState.entries;
        let index = tabState.index;
        let current = entries[index - 1];

        // We ignore the tab completely if the current entry url is
        // not acceptable (we need something accurate to open).
        if (!acceptable(current.url)) {
          continue;
        }

        if (current.url.length >= (MAX_UPLOAD_BYTES - 1000)) {
          this._log.trace("Skipping over-long URL.");
          continue;
        }

        // The element at `index` is the current page. Previous URLs were
        // previously visited URLs; subsequent URLs are in the 'forward' stack,
        // which we can't represent in Sync, so we truncate here.
        let candidates = (entries.length == index) ?
                         entries :
                         entries.slice(0, index);

        let urls = candidates.map((entry) => entry.url)
                             .filter(acceptable)
                             .reverse();                       // Because Sync puts current at index 0, and history after.

        // Truncate if necessary.
        if (urls.length > TAB_ENTRIES_LIMIT) {
          urls.length = TAB_ENTRIES_LIMIT;
        }

        allTabs.push({
          title: current.title || "",
          urlHistory: urls,
          icon: tabState.image ||
                (tabState.attributes && tabState.attributes.image) ||
                "",
          lastUsed: Math.floor((tabState.lastAccessed || 0) / 1000),
        });
      }
    }

    return allTabs;
  },

  createRecord(id, collection) {
    let record = new TabSetRecord(collection, id);
    record.clientName = this.engine.service.clientsEngine.localName;

    // Sort tabs in descending-used order to grab the most recently used
    let tabs = this.getAllTabs(true).sort(function(a, b) {
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
    tabs.forEach(function(tab) {
      this._log.trace("Wrapping tab: " + JSON.stringify(tab));
    }, this);

    record.tabs = tabs;
    return record;
  },

  getAllIDs() {
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

  wipe() {
    this._remoteClients = {};
  },

  create(record) {
    this._log.debug("Adding remote tabs from " + record.clientName);
    this._remoteClients[record.id] = Object.assign({}, record.cleartext, {
      lastModified: record.modified
    });
  },

  update(record) {
    this._log.trace("Ignoring tab updates as local ones win");
  },
};


function TabTracker(name, engine) {
  Tracker.call(this, name, engine);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);

  // Make sure "this" pointer is always set correctly for event listeners.
  this.onTab = Utils.bind2(this, this.onTab);
  this._unregisterListeners = Utils.bind2(this, this._unregisterListeners);
}
TabTracker.prototype = {
  __proto__: Tracker.prototype,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  clearChangedIDs() {
    this.modified = false;
  },

  _topics: ["pageshow", "TabOpen", "TabClose", "TabSelect"],

  _registerListenersForWindow(window) {
    this._log.trace("Registering tab listeners in window");
    for (let topic of this._topics) {
      window.addEventListener(topic, this.onTab, false);
    }
    window.addEventListener("unload", this._unregisterListeners, false);
    // If it's got a tab browser we can listen for things like navigation.
    if (window.gBrowser) {
      window.gBrowser.addProgressListener(this);
    }
  },

  _unregisterListeners(event) {
    this._unregisterListenersForWindow(event.target);
  },

  _unregisterListenersForWindow(window) {
    this._log.trace("Removing tab listeners in window");
    window.removeEventListener("unload", this._unregisterListeners, false);
    for (let topic of this._topics) {
      window.removeEventListener(topic, this.onTab, false);
    }
    if (window.gBrowser) {
      window.gBrowser.removeProgressListener(this);
    }
  },

  startTracking() {
    Svc.Obs.add("domwindowopened", this);
    let wins = Services.wm.getEnumerator("navigator:browser");
    while (wins.hasMoreElements()) {
      this._registerListenersForWindow(wins.getNext());
    }
  },

  stopTracking() {
    Svc.Obs.remove("domwindowopened", this);
    let wins = Services.wm.getEnumerator("navigator:browser");
    while (wins.hasMoreElements()) {
      this._unregisterListenersForWindow(wins.getNext());
    }
  },

  observe(subject, topic, data) {
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

  onTab(event) {
    if (event.originalTarget.linkedBrowser) {
      let browser = event.originalTarget.linkedBrowser;
      if (PrivateBrowsingUtils.isBrowserPrivate(browser) &&
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
    if (event.type != "pageshow" || Math.random() < .1) {
      this.score += SCORE_INCREMENT_SMALL;
    }
  },

  // web progress listeners.
  onLocationChange(webProgress, request, location, flags) {
    // We only care about top-level location changes which are not in the same
    // document.
    if (webProgress.isTopLevel &&
        ((flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) == 0)) {
      this.modified = true;
    }
  },
};
