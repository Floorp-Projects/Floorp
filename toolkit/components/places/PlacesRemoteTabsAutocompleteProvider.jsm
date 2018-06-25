/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Provides functions to handle remote tabs (ie, tabs known by Sync) in
 * the awesomebar.
 */

"use strict";

var EXPORTED_SYMBOLS = ["PlacesRemoteTabsAutocompleteProvider"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "SyncedTabs",
  "resource://services-sync/SyncedTabs.jsm");

XPCOMUtils.defineLazyGetter(this, "weaveXPCService", function() {
  try {
    return Cc["@mozilla.org/weave/service;1"]
             .getService(Ci.nsISupports)
             .wrappedJSObject;
  } catch (ex) {
    // The app didn't build Sync.
  }
  return null;
});

// from MDN...
function escapeRegExp(string) {
  return string.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

// Build the in-memory structure we use.
async function buildItems() {
  // This is sorted by most recent client, most recent tab.
  let tabsData = [];
  // If Sync isn't initialized (either due to lag at startup or due to no user
  // being signed in), don't reach in to Weave.Service as that may initialize
  // Sync unnecessarily - we'll get an observer notification later when it
  // becomes ready and has synced a list of tabs.
  if (weaveXPCService.ready) {
    let clients = await SyncedTabs.getTabClients();
    SyncedTabs.sortTabClientsByLastUsed(clients);
    for (let client of clients) {
      for (let tab of client.tabs) {
        tabsData.push({tab, client});
      }
    }
  }
  return tabsData;
}

// Manage the cache of the items we use.
// The cache itself.
let _items = null;

// Ensure the cache is good.
async function ensureItems() {
  if (!_items) {
    _items = await buildItems();
  }
  return _items;
}

// A preference used to disable the showing of icons in remote tab records.
const PREF_SHOW_REMOTE_ICONS = "services.sync.syncedTabs.showRemoteIcons";
let showRemoteIcons;

// A preference used to disable the synced tabs from showing in awesomebar
// matches.
const PREF_SHOW_REMOTE_TABS = "services.sync.syncedTabs.showRemoteTabs";
let showRemoteTabs;

// An observer to invalidate _items and watch for changed prefs.
function observe(subject, topic, data) {
  switch (topic) {
    case "weave:engine:sync:finish":
      if (data == "tabs") {
        // The tabs engine just finished syncing, so may have a different list
        // of tabs then we previously cached.
        _items = null;
      }
      break;

    case "weave:service:start-over":
      // Sync is being reset due to the user disconnecting - we must invalidate
      // the cache so we don't supply tabs from a different user.
      _items = null;
      break;

    case "nsPref:changed":
      if (data == PREF_SHOW_REMOTE_ICONS) {
        showRemoteIcons = Services.prefs.getBoolPref(PREF_SHOW_REMOTE_ICONS, true);
      } else if (data == PREF_SHOW_REMOTE_TABS) {
        showRemoteTabs = Services.prefs.getBoolPref(PREF_SHOW_REMOTE_TABS, true);
      }
      break;

    default:
      break;
  }
}

Services.obs.addObserver(observe, "weave:engine:sync:finish");
Services.obs.addObserver(observe, "weave:service:start-over");

// Observe the prefs for showing remote icons and tabs and prime
// our bools that reflect their values.
Services.prefs.addObserver(PREF_SHOW_REMOTE_ICONS, observe);
Services.prefs.addObserver(PREF_SHOW_REMOTE_TABS, observe);
observe(null, "nsPref:changed", PREF_SHOW_REMOTE_ICONS);
observe(null, "nsPref:changed", PREF_SHOW_REMOTE_TABS);


// This public object is a static singleton.
var PlacesRemoteTabsAutocompleteProvider = {
  // a promise that resolves with an array of matching remote tabs.
  async getMatches(searchString) {
    // If Sync isn't configured we bail early.
    if (!weaveXPCService || !weaveXPCService.ready || !weaveXPCService.enabled) {
      return [];
    }

    if (!showRemoteTabs) {
      return [];
    }

    let re = new RegExp(escapeRegExp(searchString), "i");
    let matches = [];
    let tabsData = await ensureItems();
    for (let {tab, client} of tabsData) {
      let url = tab.url;
      let title = tab.title;
      if (url.match(re) || (title && title.match(re))) {
        let icon = showRemoteIcons ? tab.icon : null;
        // create the record we return for auto-complete.
        let record = {
          url, title, icon,
          deviceName: client.name,
          lastUsed: tab.lastUsed * 1000
        };
        matches.push(record);
      }
    }

    return matches;
  },
};
