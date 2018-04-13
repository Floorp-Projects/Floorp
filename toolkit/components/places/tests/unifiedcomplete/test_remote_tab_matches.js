/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
*/
"use strict";

ChromeUtils.import("resource://services-sync/main.js");

Services.prefs.setCharPref("services.sync.username", "someone@somewhere.com");
Services.prefs.setCharPref("services.sync.registerEngines", "");

// A mock "Tabs" engine which autocomplete will use instead of the real
// engine. We pass a constructor that Sync creates.
function MockTabsEngine() {
  this.clients = null; // We'll set this dynamically
}

MockTabsEngine.prototype = {
  name: "tabs",

  startTracking() {},
  getAllClients() {
    return this.clients;
  },
};

// A clients engine that doesn't need to be a constructor.
let MockClientsEngine = {
  getClientType(guid) {
    Assert.ok(guid.endsWith("desktop") || guid.endsWith("mobile"));
    return guid.endsWith("mobile") ? "phone" : "desktop";
  }
};

// Tell Sync about the mocks.
Weave.Service.engineManager.register(MockTabsEngine);

// Tell the Sync XPCOM service it is initialized.
let weaveXPCService = Cc["@mozilla.org/weave/service;1"]
                        .getService(Ci.nsISupports)
                        .wrappedJSObject;
weaveXPCService.ready = true;

// Configure the singleton engine for a test.
function configureEngine(clients) {
  // Configure the instance Sync created.
  let engine = Weave.Service.engineManager.get("tabs");
  engine.clients = clients;
  Weave.Service.clientsEngine = MockClientsEngine;
  // Send an observer that pretends the engine just finished a sync.
  Services.obs.notifyObservers(null, "weave:engine:sync:finish", "tabs");
}

// Make a match object suitable for passing to check_autocomplete.
function makeRemoteTabMatch(url, deviceName, extra = {}) {
  return {
    uri: makeActionURI("remotetab", {url, deviceName}),
    title: extra.title || url,
    style: [ "action", "remotetab" ],
    icon: extra.icon,
  };
}

// The tests.
add_task(async function test_nomatch() {
  // Nothing matches.
  configureEngine({
    guid_desktop: {
      clientName: "My Desktop",
      tabs: [{
        urlHistory: ["http://foo.com/"],
      }],
    }
  });

  // No remote tabs match here, so we only expect search results.
  await check_autocomplete({
    search: "ex",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("ex", { heuristic: true }) ],
  });
});

add_task(async function test_minimal() {
  // The minimal client and tabs info we can get away with.
  configureEngine({
    guid_desktop: {
      clientName: "My Desktop",
      tabs: [{
        urlHistory: ["http://example.com/"],
      }],
    }
  });

  await check_autocomplete({
    search: "ex",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("ex", { heuristic: true }),
               makeRemoteTabMatch("http://example.com/", "My Desktop") ],
  });
});

add_task(async function test_maximal() {
  // Every field that could possibly exist on a remote record.
  configureEngine({
    guid_mobile: {
      clientName: "My Phone",
      tabs: [{
        urlHistory: ["http://example.com/"],
        title: "An Example",
        icon: "http://favicon",
      }],
    }
  });

  await check_autocomplete({
    search: "ex",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("ex", { heuristic: true }),
               makeRemoteTabMatch("http://example.com/", "My Phone",
                                  { title: "An Example",
                                    icon: "moz-anno:favicon:http://favicon/"
                                  }),
             ],
  });
});

add_task(async function test_noShowIcons() {
  Services.prefs.setBoolPref("services.sync.syncedTabs.showRemoteIcons", false);
  configureEngine({
    guid_mobile: {
      clientName: "My Phone",
      tabs: [{
        urlHistory: ["http://example.com/"],
        title: "An Example",
        icon: "http://favicon",
      }],
    }
  });

  await check_autocomplete({
    search: "ex",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("ex", { heuristic: true }),
               makeRemoteTabMatch("http://example.com/", "My Phone",
                                  { title: "An Example",
                                    // expecting the default favicon due to that pref.
                                    icon: "",
                                  }),
             ],
  });
  Services.prefs.clearUserPref("services.sync.syncedTabs.showRemoteIcons");
});

add_task(async function test_matches_title() {
  // URL doesn't match search expression, should still match the title.
  configureEngine({
    guid_mobile: {
      clientName: "My Phone",
      tabs: [{
        urlHistory: ["http://foo.com/"],
        title: "An Example",
      }],
    }
  });

  await check_autocomplete({
    search: "ex",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("ex", { heuristic: true }),
               makeRemoteTabMatch("http://foo.com/", "My Phone",
                                  { title: "An Example" }),
             ],
  });
});

add_task(async function test_localtab_matches_override() {
  // We have an open tab to the same page on a remote device, only "switch to
  // tab" should appear as duplicate detection removed the remote one.

  // First setup Sync to have the page as a remote tab.
  configureEngine({
    guid_mobile: {
      clientName: "My Phone",
      tabs: [{
        urlHistory: ["http://foo.com/"],
        title: "An Example",
      }],
    }
  });

  // Setup Places to think the tab is open locally.
  let uri = NetUtil.newURI("http://foo.com/");
  await PlacesTestUtils.addVisits([
    { uri, title: "An Example" },
  ]);
  addOpenPages(uri, 1);

  await check_autocomplete({
    search: "ex",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("ex", { heuristic: true }),
               makeSwitchToTabMatch("http://foo.com/", { title: "An Example" }),
             ],
  });
});

add_task(async function test_remotetab_matches_override() {
  // If We have an history result to the same page, we should only get the
  // remote tab match.
  let url = "http://foo.remote.com/";
  // First setup Sync to have the page as a remote tab.
  configureEngine({
    guid_mobile: {
      clientName: "My Phone",
      tabs: [{
        urlHistory: [url],
        title: "An Example",
      }],
    }
  });

  // Setup Places to think the tab is open locally.
  await PlacesTestUtils.addVisits(url);

  await check_autocomplete({
    search: "rem",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("rem", { heuristic: true }),
               makeRemoteTabMatch("http://foo.remote.com/", "My Phone",
                                  { title: "An Example" }),
             ],
  });
});
