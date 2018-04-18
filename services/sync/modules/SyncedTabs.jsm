/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SyncedTabs"];


ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://services-sync/main.js");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");

// The Sync XPCOM service
XPCOMUtils.defineLazyGetter(this, "weaveXPCService", function() {
  return Cc["@mozilla.org/weave/service;1"]
           .getService(Ci.nsISupports)
           .wrappedJSObject;
});

ChromeUtils.defineModuleGetter(this, "PlacesUtils",
                               "resource://gre/modules/PlacesUtils.jsm");

// from MDN...
function escapeRegExp(string) {
  return string.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

// A topic we fire whenever we have new tabs available. This might be due
// to a request made by this module to refresh the tab list, or as the result
// of a regularly scheduled sync. The intent is that consumers just listen
// for this notification and update their UI in response.
const TOPIC_TABS_CHANGED = "services.sync.tabs.changed";

// The interval, in seconds, before which we consider the existing list
// of tabs "fresh enough" and don't force a new sync.
const TABS_FRESH_ENOUGH_INTERVAL = 30;

XPCOMUtils.defineLazyGetter(this, "log", function() {
  let log = Log.repository.getLogger("Sync.RemoteTabs");
  log.manageLevelFromPref("services.sync.log.logger.tabs");
  return log;
});

// A private singleton that does the work.
let SyncedTabsInternal = {
  /* Make a "tab" record. Returns a promise */
  async _makeTab(client, tab, url, showRemoteIcons) {
    let icon;
    if (showRemoteIcons) {
      icon = tab.icon;
    }
    if (!icon) {
      try {
        icon = (await PlacesUtils.promiseFaviconLinkUrl(url)).spec;
      } catch (ex) { /* no favicon avaiable */ }
    }
    if (!icon) {
      icon = "";
    }
    return {
      type:  "tab",
      title: tab.title || url,
      url,
      icon,
      client: client.id,
      lastUsed: tab.lastUsed,
    };
  },

  /* Make a "client" record. Returns a promise for consistency with _makeTab */
  async _makeClient(client) {
    return {
      id: client.id,
      type: "client",
      name: Weave.Service.clientsEngine.getClientName(client.id),
      clientType: Weave.Service.clientsEngine.getClientType(client.id),
      lastModified: client.lastModified * 1000, // sec to ms
      tabs: []
    };
  },

  _tabMatchesFilter(tab, filter) {
    let reFilter = new RegExp(escapeRegExp(filter), "i");
    return tab.url.match(reFilter) || tab.title.match(reFilter);
  },

  async getTabClients(filter) {
    log.info("Generating tab list with filter", filter);
    let result = [];

    // If Sync isn't ready, don't try and get anything.
    if (!weaveXPCService.ready) {
      log.debug("Sync isn't yet ready, so returning an empty tab list");
      return result;
    }

    // A boolean that controls whether we should show the icon from the remote tab.
    const showRemoteIcons = Preferences.get("services.sync.syncedTabs.showRemoteIcons", true);

    let engine = Weave.Service.engineManager.get("tabs");

    let ntabs = 0;

    for (let client of Object.values(engine.getAllClients())) {
      if (!Weave.Service.clientsEngine.remoteClientExists(client.id)) {
        continue;
      }
      let clientRepr = await this._makeClient(client);
      log.debug("Processing client", clientRepr);

      for (let tab of client.tabs) {
        let url = tab.urlHistory[0];
        log.trace("remote tab", url);

        if (!url) {
          continue;
        }
        let tabRepr = await this._makeTab(client, tab, url, showRemoteIcons);
        if (filter && !this._tabMatchesFilter(tabRepr, filter)) {
          continue;
        }
        clientRepr.tabs.push(tabRepr);
      }
      // We return all clients, even those without tabs - the consumer should
      // filter it if they care.
      ntabs += clientRepr.tabs.length;
      result.push(clientRepr);
    }
    log.info(`Final tab list has ${result.length} clients with ${ntabs} tabs.`);
    return result;
  },

  async syncTabs(force) {
    if (!force) {
      // Don't bother refetching tabs if we already did so recently
      let lastFetch = Preferences.get("services.sync.lastTabFetch", 0);
      let now = Math.floor(Date.now() / 1000);
      if (now - lastFetch < TABS_FRESH_ENOUGH_INTERVAL) {
        log.info("_refetchTabs was done recently, do not doing it again");
        return false;
      }
    }

    // If Sync isn't configured don't try and sync, else we will get reports
    // of a login failure.
    if (Weave.Status.checkSetup() == Weave.CLIENT_NOT_CONFIGURED) {
      log.info("Sync client is not configured, so not attempting a tab sync");
      return false;
    }
    // Ask Sync to just do the tabs engine if it can.
    try {
      log.info("Doing a tab sync.");
      await Weave.Service.sync({why: "tabs", engines: ["tabs"]});
      return true;
    } catch (ex) {
      log.error("Sync failed", ex);
      throw ex;
    }
  },

  observe(subject, topic, data) {
    log.trace(`observed topic=${topic}, data=${data}, subject=${subject}`);
    switch (topic) {
      case "weave:engine:sync:finish":
        if (data != "tabs") {
          return;
        }
        // The tabs engine just finished syncing
        // Set our lastTabFetch pref here so it tracks both explicit sync calls
        // and normally scheduled ones.
        Preferences.set("services.sync.lastTabFetch", Math.floor(Date.now() / 1000));
        Services.obs.notifyObservers(null, TOPIC_TABS_CHANGED);
        break;
      case "weave:service:start-over":
        // start-over needs to notify so consumers find no tabs.
        Preferences.reset("services.sync.lastTabFetch");
        Services.obs.notifyObservers(null, TOPIC_TABS_CHANGED);
        break;
      case "nsPref:changed":
        Services.obs.notifyObservers(null, TOPIC_TABS_CHANGED);
        break;
      default:
        break;
    }
  },

  get loginFailed() {
    if (!weaveXPCService.ready) {
      log.debug("Sync isn't yet ready; assuming the login didn't fail");
      return false;
    }
    return Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED;
  },

  // Returns true if Sync is configured to Sync tabs, false otherwise
  get isConfiguredToSyncTabs() {
    if (!weaveXPCService.ready) {
      log.debug("Sync isn't yet ready; assuming tab engine is enabled");
      return true;
    }

    let engine = Weave.Service.engineManager.get("tabs");
    return engine && engine.enabled;
  },

  get hasSyncedThisSession() {
    let engine = Weave.Service.engineManager.get("tabs");
    return engine && engine.hasSyncedThisSession;
  },
};

Services.obs.addObserver(SyncedTabsInternal, "weave:engine:sync:finish");
Services.obs.addObserver(SyncedTabsInternal, "weave:service:start-over");
// Observe the pref the indicates the state of the tabs engine has changed.
// This will force consumers to re-evaluate the state of sync and update
// accordingly.
Services.prefs.addObserver("services.sync.engine.tabs", SyncedTabsInternal);

// The public interface.
var SyncedTabs = {
  // A mock-point for tests.
  _internal: SyncedTabsInternal,

  // We make the topic for the observer notification public.
  TOPIC_TABS_CHANGED,

  // Returns true if Sync is configured to Sync tabs, false otherwise
  get isConfiguredToSyncTabs() {
    return this._internal.isConfiguredToSyncTabs;
  },

  // Returns true if a tab sync has completed once this session. If this
  // returns false, then getting back no clients/tabs possibly just means we
  // are waiting for that first sync to complete.
  get hasSyncedThisSession() {
    return this._internal.hasSyncedThisSession;
  },

  // Returns true if Sync is in a "need to reauthenticate" state.
  get loginFailed() {
    return this._internal.loginFailed;
  },

  // Return a promise that resolves with an array of client records, each with
  // a .tabs array. Note that part of the contract for this module is that the
  // returned objects are not shared between invocations, so callers are free
  // to mutate the returned objects (eg, sort, truncate) however they see fit.
  getTabClients(query) {
    return this._internal.getTabClients(query);
  },

  // Starts a background request to start syncing tabs. Returns a promise that
  // resolves when the sync is complete, but there's no resolved value -
  // callers should be listening for TOPIC_TABS_CHANGED.
  // If |force| is true we always sync. If false, we only sync if the most
  // recent sync wasn't "recently".
  syncTabs(force) {
    return this._internal.syncTabs(force);
  },

  sortTabClientsByLastUsed(clients) {
    // First sort the list of tabs for each client. Note that
    // this module promises that the objects it returns are never
    // shared, so we are free to mutate those objects directly.
    for (let client of clients) {
      let tabs = client.tabs;
      tabs.sort((a, b) => b.lastUsed - a.lastUsed);
    }
    // Now sort the clients - the clients are sorted in the order of the
    // most recent tab for that client (ie, it is important the tabs for
    // each client are already sorted.)
    clients.sort((a, b) => {
      if (a.tabs.length == 0) {
        return 1; // b comes first.
      }
      if (b.tabs.length == 0) {
        return -1; // a comes first.
      }
      return b.tabs[0].lastUsed - a.tabs[0].lastUsed;
    });
  },
};
