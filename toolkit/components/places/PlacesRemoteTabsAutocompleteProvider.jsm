/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Provides functions to handle remote tabs (ie, tabs known by Sync) in
 * the awesomebar.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["PlacesRemoteTabsAutocompleteProvider"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "weaveXPCService", function() {
  return Cc["@mozilla.org/weave/service;1"]
           .getService(Ci.nsISupports)
           .wrappedJSObject;
});

XPCOMUtils.defineLazyGetter(this, "Weave", () => {
  try {
    let {Weave} = Cu.import("resource://services-sync/main.js", {});
    return Weave;
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
function buildItems() {
  let clients = new Map(); // keyed by client guid, value is client
  let tabs = new Map(); // keyed by string URL, value is {clientId, tab}

  // If Sync isn't initialized (either due to lag at startup or due to no user
  // being signed in), don't reach in to Weave.Service as that may initialize
  // Sync unnecessarily - we'll get an observer notification later when it
  // becomes ready and has synced a list of tabs.
  if (weaveXPCService.ready) {
    let engine = Weave.Service.engineManager.get("tabs");

    for (let [guid, client] of Object.entries(engine.getAllClients())) {
      clients.set(guid, client);
      for (let tab of client.tabs) {
        let url = tab.urlHistory[0];
        tabs.set(url, { clientId: guid, tab });
      }
    }
  }
  return { clients, tabs };
}

// Manage the cache of the items we use.
// The cache itself.
let _items = null;

// Ensure the cache is good.
function ensureItems() {
  if (!_items) {
    _items = buildItems();
  }
  return _items;
}

// A preference used to disable the showing of icons in remote tab records.
const PREF_SHOW_REMOTE_ICONS = "services.sync.syncedTabs.showRemoteIcons";
let showRemoteIcons;

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
      }
      break;

    default:
      break;
  }
}

Services.obs.addObserver(observe, "weave:engine:sync:finish");
Services.obs.addObserver(observe, "weave:service:start-over");

// Observe the pref for showing remote icons and prime our bool that reflects its value.
Services.prefs.addObserver(PREF_SHOW_REMOTE_ICONS, observe);
observe(null, "nsPref:changed", PREF_SHOW_REMOTE_ICONS);

// This public object is a static singleton.
this.PlacesRemoteTabsAutocompleteProvider = {
  // a promise that resolves with an array of matching remote tabs.
  getMatches(searchString) {
    // If Sync isn't configured we bail early.
    if (Weave === null ||
        !Services.prefs.prefHasUserValue("services.sync.username")) {
      return Promise.resolve([]);
    }

    let re = new RegExp(escapeRegExp(searchString), "i");
    let matches = [];
    let { tabs, clients } = ensureItems();
    for (let [url, { clientId, tab }] of tabs) {
      let title = tab.title;
      if (url.match(re) || (title && title.match(re))) {
        // lookup the client record.
        let client = clients.get(clientId);
        let icon = showRemoteIcons ? tab.icon : null;
        // create the record we return for auto-complete.
        let record = {
          url, title, icon,
          deviceClass: Weave.Service.clientsEngine.isMobile(clientId) ? "mobile" : "desktop",
          deviceName: client.clientName,
        };
        matches.push(record);
      }
    }
    return Promise.resolve(matches);
  },
}
