/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
*/
"use strict";

Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/SyncedTabs.jsm");
Cu.import("resource://gre/modules/Log.jsm");

const faviconService = Cc["@mozilla.org/browser/favicon-service;1"]
                       .getService(Ci.nsIFaviconService);

Log.repository.getLogger("Sync.RemoteTabs").addAppender(new Log.DumpAppender());

// A mock "Tabs" engine which the SyncedTabs module will use instead of the real
// engine. We pass a constructor that Sync creates.
function MockTabsEngine() {
  this.clients = {}; // We'll set this dynamically
}

MockTabsEngine.prototype = {
  name: "tabs",
  enabled: true,

  getAllClients() {
    return this.clients;
  },

  getOpenURLs() {
    return new Set();
  },
};

let tabsEngine;

// A clients engine that doesn't need to be a constructor.
let MockClientsEngine = {
  clientSettings: null, // Set in `configureClients`.

  isMobile(guid) {
    if (!guid.endsWith("desktop") && !guid.endsWith("mobile")) {
      throw new Error("this module expected guids to end with 'desktop' or 'mobile'");
    }
    return guid.endsWith("mobile");
  },
  remoteClientExists(id) {
    return this.clientSettings[id] !== false;
  },
  getClientName(id) {
    if (this.clientSettings[id]) {
      return this.clientSettings[id];
    }
    return tabsEngine.clients[id].clientName;
  },
};

function configureClients(clients, clientSettings = {}) {
  // each client record is expected to have an id.
  for (let [guid, client] of Object.entries(clients)) {
    client.id = guid;
  }
  tabsEngine.clients = clients;
  // Apply clients collection overrides.
  MockClientsEngine.clientSettings = clientSettings;
  // Send an observer that pretends the engine just finished a sync.
  Services.obs.notifyObservers(null, "weave:engine:sync:finish", "tabs");
}

add_task(async function setup() {
  await Weave.Service.promiseInitialized;
  // Configure Sync with our mock tabs engine and force it to become initialized.
  Weave.Service.engineManager.unregister("tabs");
  await Weave.Service.engineManager.register(MockTabsEngine);
  Weave.Service.clientsEngine = MockClientsEngine;
  tabsEngine = Weave.Service.engineManager.get("tabs");

  // Tell the Sync XPCOM service it is initialized.
  let weaveXPCService = Cc["@mozilla.org/weave/service;1"]
                          .getService(Ci.nsISupports)
                          .wrappedJSObject;
  weaveXPCService.ready = true;
});

// The tests.
add_task(async function test_noClients() {
  // no clients, can't be tabs.
  await configureClients({});

  let tabs = await SyncedTabs.getTabClients();
  equal(Object.keys(tabs).length, 0);
});

add_task(async function test_clientWithTabs() {
  await configureClients({
    guid_desktop: {
      clientName: "My Desktop",
      tabs: [
      {
        urlHistory: ["http://foo.com/"],
        icon: "http://foo.com/favicon",
      }],
    },
    guid_mobile: {
      clientName: "My Phone",
      tabs: [],
    }
  });

  let clients = await SyncedTabs.getTabClients();
  equal(clients.length, 2);
  clients.sort((a, b) => { return a.name.localeCompare(b.name); });
  equal(clients[0].tabs.length, 1);
  equal(clients[0].tabs[0].url, "http://foo.com/");
  equal(clients[0].tabs[0].icon, "http://foo.com/favicon");
  // second client has no tabs.
  equal(clients[1].tabs.length, 0);
});

add_task(async function test_staleClientWithTabs() {
  await configureClients({
    guid_desktop: {
      clientName: "My Desktop",
      tabs: [
      {
        urlHistory: ["http://foo.com/"],
        icon: "http://foo.com/favicon",
      }],
    },
    guid_mobile: {
      clientName: "My Phone",
      tabs: [],
    },
    guid_stale_mobile: {
      clientName: "My Deleted Phone",
      tabs: [],
    },
    guid_stale_desktop: {
      clientName: "My Deleted Laptop",
      tabs: [
      {
        urlHistory: ["https://bar.com/"],
        icon: "https://bar.com/favicon",
      }],
    },
    guid_stale_name_desktop: {
      clientName: "My Generic Device",
      tabs: [
      {
        urlHistory: ["https://example.edu/"],
        icon: "https://example.edu/favicon",
      }],
    },
  }, {
    guid_stale_mobile: false,
    guid_stale_desktop: false,
    // We should always use the device name from the clients collection, instead
    // of the possibly stale tabs collection.
    guid_stale_name_desktop: "My Laptop",
  });
  let clients = await SyncedTabs.getTabClients();
  clients.sort((a, b) => { return a.name.localeCompare(b.name); });
  equal(clients.length, 3);
  equal(clients[0].name, "My Desktop");
  equal(clients[0].tabs.length, 1);
  equal(clients[0].tabs[0].url, "http://foo.com/");
  equal(clients[1].name, "My Laptop");
  equal(clients[1].tabs.length, 1);
  equal(clients[1].tabs[0].url, "https://example.edu/");
  equal(clients[2].name, "My Phone");
  equal(clients[2].tabs.length, 0);
});

add_task(async function test_clientWithTabsIconsDisabled() {
  Services.prefs.setBoolPref("services.sync.syncedTabs.showRemoteIcons", false);
  await configureClients({
    guid_desktop: {
      clientName: "My Desktop",
      tabs: [
      {
        urlHistory: ["http://foo.com/"],
        icon: "http://foo.com/favicon",
      }],
    },
  });

  let clients = await SyncedTabs.getTabClients();
  equal(clients.length, 1);
  clients.sort((a, b) => { return a.name.localeCompare(b.name); });
  equal(clients[0].tabs.length, 1);
  equal(clients[0].tabs[0].url, "http://foo.com/");
  // expect the default favicon (empty string) due to the pref being false.
  equal(clients[0].tabs[0].icon, "");
  Services.prefs.clearUserPref("services.sync.syncedTabs.showRemoteIcons");
});

add_task(async function test_filter() {
  // Nothing matches.
  await configureClients({
    guid_desktop: {
      clientName: "My Desktop",
      tabs: [
      {
        urlHistory: ["http://foo.com/"],
        title: "A test page.",
      },
      {
        urlHistory: ["http://bar.com/"],
        title: "Another page.",
      }],
    },
  });

  let clients = await SyncedTabs.getTabClients("foo");
  equal(clients.length, 1);
  equal(clients[0].tabs.length, 1);
  equal(clients[0].tabs[0].url, "http://foo.com/");
  // check it matches the title.
  clients = await SyncedTabs.getTabClients("test");
  equal(clients.length, 1);
  equal(clients[0].tabs.length, 1);
  equal(clients[0].tabs[0].url, "http://foo.com/");
});

add_task(async function test_duplicatesTabsAcrossClients() {

  await configureClients({
    guid_desktop: {
      clientName: "My Desktop",
      tabs: [
      {
        urlHistory: ["http://foo.com/"],
        title: "A test page.",
      }],
    },
    guid_mobile: {
      clientName: "My Phone",
      tabs: [
      {
          urlHistory: ["http://foo.com/"],
          title: "A test page.",
      }],
    },
  });

  let clients = await SyncedTabs.getTabClients();
  equal(clients.length, 2);
  equal(clients[0].tabs.length, 1);
  equal(clients[1].tabs.length, 1);
  equal(clients[0].tabs[0].url, "http://foo.com/");
  equal(clients[1].tabs[0].url, "http://foo.com/");
});
