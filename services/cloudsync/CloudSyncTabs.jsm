/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Tabs"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/CloudSyncEventSource.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://services-common/observers.js");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils", "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Session", "@mozilla.org/browser/sessionstore;1", "nsISessionStore");

const DATA_VERSION = 1;

var ClientRecord = function(params) {
  this.id = params.id;
  this.name = params.name || "?";
  this.tabs = new Set();
}

ClientRecord.prototype = {
  version: DATA_VERSION,

  update(params) {
    if (this.id !== params.id) {
      throw new Error("expected " + this.id + " to equal " + params.id);
    }

    this.name = params.name;
  }
};

var TabRecord = function(params) {
  this.url = params.url || "";
  this.update(params);
};

TabRecord.prototype = {
  version: DATA_VERSION,

  update(params) {
    if (this.url && this.url !== params.url) {
      throw new Error("expected " + this.url + " to equal " + params.url);
    }

    if (params.lastUsed && params.lastUsed < this.lastUsed) {
      return;
    }

    this.title = params.title || "";
    this.icon = params.icon || "";
    this.lastUsed = params.lastUsed || 0;
  },
};

var TabCache = function() {
  this.tabs = new Map();
  this.clients = new Map();
};

TabCache.prototype = {
  merge(client, tabs) {
    if (!client || !client.id) {
      return;
    }

    if (!tabs) {
      return;
    }

    let cRecord;
    if (this.clients.has(client.id)) {
      try {
        cRecord = this.clients.get(client.id);
      } catch (e) {
        throw new Error("unable to update client: " + e);
      }
    } else {
      cRecord = new ClientRecord(client);
      this.clients.set(cRecord.id, cRecord);
    }

    for (let tab of tabs) {
      if (!tab || 'object' !== typeof(tab)) {
        continue;
      }

      let tRecord;
      if (this.tabs.has(tab.url)) {
        tRecord = this.tabs.get(tab.url);
        try {
          tRecord.update(tab);
        } catch (e) {
          throw new Error("unable to update tab: " + e);
        }
      } else {
        tRecord = new TabRecord(tab);
        this.tabs.set(tRecord.url, tRecord);
      }

      if (tab.deleted) {
        cRecord.tabs.delete(tRecord);
      } else {
        cRecord.tabs.add(tRecord);
      }
    }
  },

  clear(client) {
    if (client) {
      this.clients.delete(client.id);
    } else {
      this.clients = new Map();
      this.tabs = new Map();
    }
  },

  get() {
    let results = [];
    for (let client of this.clients.values()) {
      results.push(client);
    }
    return results;
  },

  isEmpty() {
    return 0 == this.clients.size;
  },

};

this.Tabs = function() {
  let suspended = true;

  let topics = [
    "pageshow",
    "TabOpen",
    "TabClose",
    "TabSelect",
  ];

  let update = function(event) {
    if (event.originalTarget.linkedBrowser) {
      if (PrivateBrowsingUtils.isBrowserPrivate(event.originalTarget.linkedBrowser) &&
          !PrivateBrowsingUtils.permanentPrivateBrowsing) {
        return;
      }
    }

    eventSource.emit("change");
  };

  let registerListenersForWindow = function(window) {
    for (let topic of topics) {
      window.addEventListener(topic, update, false);
    }
    window.addEventListener("unload", unregisterListeners, false);
  };

  let unregisterListenersForWindow = function(window) {
    window.removeEventListener("unload", unregisterListeners, false);
    for (let topic of topics) {
      window.removeEventListener(topic, update, false);
    }
  };

  let unregisterListeners = function(event) {
    unregisterListenersForWindow(event.target);
  };

  let observer = {
    observe(subject, topic, data) {
      switch (topic) {
        case "domwindowopened":
          let onLoad = () => {
            subject.removeEventListener("load", onLoad, false);
            // Only register after the window is done loading to avoid unloads.
            registerListenersForWindow(subject);
          };

          // Add tab listeners now that a window has opened.
          subject.addEventListener("load", onLoad, false);
          break;
      }
    }
  };

  let resume = function() {
    if (suspended) {
      Observers.add("domwindowopened", observer);
      let wins = Services.wm.getEnumerator("navigator:browser");
      while (wins.hasMoreElements()) {
        registerListenersForWindow(wins.getNext());
      }
    }
  };

  let suspend = function() {
    if (!suspended) {
      Observers.remove("domwindowopened", observer);
      let wins = Services.wm.getEnumerator("navigator:browser");
      while (wins.hasMoreElements()) {
        unregisterListenersForWindow(wins.getNext());
      }
    }
  };

  let eventTypes = [
    "change",
  ];

  let eventSource = new EventSource(eventTypes, suspend, resume);

  let tabCache = new TabCache();

  let getWindowEnumerator = function() {
    return Services.wm.getEnumerator("navigator:browser");
  };

  let shouldSkipWindow = function(win) {
    return win.closed ||
           PrivateBrowsingUtils.isWindowPrivate(win);
  };

  let getTabState = function(tab) {
    return JSON.parse(Session.getTabState(tab));
  };

  let getLocalTabs = function(filter) {
    let deferred = Promise.defer();

    filter = (undefined === filter) ? true : filter;
    let filteredUrls = new RegExp("^(about:.*|chrome://weave/.*|wyciwyg:.*|file:.*)$"); // FIXME: should be a pref (B#1044304)

    let allTabs = [];

    let currentState = JSON.parse(Session.getBrowserState());
    currentState.windows.forEach(function(window) {
      if (window.isPrivate) {
        return;
      }
      window.tabs.forEach(function(tab) {
        if (!tab.entries.length) {
          return;
        }

        // Get only the latest entry
        // FIXME: support full history (B#1044306)
        let entry = tab.entries[tab.index - 1];

        if (!entry.url || filter && filteredUrls.test(entry.url)) {
          return;
        }

        allTabs.push(new TabRecord({
          title: entry.title,
          url: entry.url,
          icon: tab.attributes && tab.attributes.image || "",
          lastUsed: tab.lastAccessed,
        }));
      });
    });

    deferred.resolve(allTabs);

    return deferred.promise;
  };

  let mergeRemoteTabs = function(client, tabs) {
    let deferred = Promise.defer();

    deferred.resolve(tabCache.merge(client, tabs));
    Observers.notify("cloudsync:tabs:update");

    return deferred.promise;
  };

  let clearRemoteTabs = function(client) {
    let deferred = Promise.defer();

    deferred.resolve(tabCache.clear(client));
    Observers.notify("cloudsync:tabs:update");

    return deferred.promise;
  };

  let getRemoteTabs = function() {
    let deferred = Promise.defer();

    deferred.resolve(tabCache.get());

    return deferred.promise;
  };

  let hasRemoteTabs = function() {
    return !tabCache.isEmpty();
  };

  /* PUBLIC API */
  this.addEventListener = eventSource.addEventListener;
  this.removeEventListener = eventSource.removeEventListener;
  this.getLocalTabs = getLocalTabs.bind(this);
  this.mergeRemoteTabs = mergeRemoteTabs.bind(this);
  this.clearRemoteTabs = clearRemoteTabs.bind(this);
  this.getRemoteTabs = getRemoteTabs.bind(this);
  this.hasRemoteTabs = hasRemoteTabs.bind(this);
};

Tabs.prototype = {
};
this.Tabs = Tabs;
