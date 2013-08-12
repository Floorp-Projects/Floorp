/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE = "general.useragent.locale";

// Disables security checking our updates which haven't been signed
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://testing-common/httpd.js");

// This is the data we expect to see sent as part of the update url.
var EXPECTED = [
  {
    id: "bug335238_1@tests.mozilla.org",
    version: "1.3.4",
    maxAppVersion: "5",
    status: "userEnabled",
    appId: "xpcshell@tests.mozilla.org",
    appVersion: "1",
    appOs: "XPCShell",
    appAbi: "noarch-spidermonkey",
    locale: "en-US",
    reqVersion: "2"
  },
  {
    id: "bug335238_2@tests.mozilla.org",
    version: "28at",
    maxAppVersion: "7",
    status: "userDisabled",
    appId: "xpcshell@tests.mozilla.org",
    appVersion: "1",
    appOs: "XPCShell",
    appAbi: "noarch-spidermonkey",
    locale: "en-US",
    reqVersion: "2"
  },
  {
    id: "bug335238_3@tests.mozilla.org",
    version: "58",
    maxAppVersion: "*",
    status: "userDisabled,softblocked",
    appId: "xpcshell@tests.mozilla.org",
    appVersion: "1",
    appOs: "XPCShell",
    appAbi: "noarch-spidermonkey",
    locale: "en-US",
    reqVersion: "2"
  },
  {
    id: "bug335238_4@tests.mozilla.org",
    version: "4",
    maxAppVersion: "2+",
    status: "userEnabled,blocklisted",
    appId: "xpcshell@tests.mozilla.org",
    appVersion: "1",
    appOs: "XPCShell",
    appAbi: "noarch-spidermonkey",
    locale: "en-US",
    reqVersion: "2"
  }
];

var ADDONS = [
  {id: "bug335238_1@tests.mozilla.org",
   addon: "test_bug335238_1"},
  {id: "bug335238_2@tests.mozilla.org",
   addon: "test_bug335238_2"},
  {id: "bug335238_3@tests.mozilla.org",
   addon: "test_bug335238_3"},
  {id: "bug335238_4@tests.mozilla.org",
   addon: "test_bug335238_4"}
];

// This is a replacement for the blocklist service
var BlocklistService = {
  getAddonBlocklistState: function(aId, aVersion, aAppVersion, aToolkitVersion) {
    if (aId == "bug335238_3@tests.mozilla.org")
      return Ci.nsIBlocklistService.STATE_SOFTBLOCKED;
    if (aId == "bug335238_4@tests.mozilla.org")
      return Ci.nsIBlocklistService.STATE_BLOCKED;
    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  getPluginBlocklistState: function(aPlugin, aVersion, aAppVersion, aToolkitVersion) {
    return Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
  },

  isAddonBlocklisted: function(aId, aVersion, aAppVersion, aToolkitVersion) {
    return this.getAddonBlocklistState(aId, aVersion, aAppVersion, aToolkitVersion) ==
           Ci.nsIBlocklistService.STATE_BLOCKED;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIBlocklistService)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

var BlocklistServiceFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return BlocklistService.QueryInterface(iid);
  }
};
var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{61189e7a-6b1b-44b8-ac81-f180a6105085}"), "BlocklistService",
                          "@mozilla.org/extensions/blocklist;1", BlocklistServiceFactory);

var server;

var updateListener = {
  pendingCount: 0,

  onUpdateAvailable: function(aAddon) {
    do_throw("Should not have seen an update for " + aAddon.id);
  },

  onUpdateFinished: function() {
    if (--this.pendingCount == 0)
      server.stop(do_test_finished);
  }
}

var requestHandler = {
  handle: function(metadata, response)
  {
    var expected = EXPECTED[metadata.path.substring(1)];
    var params = metadata.queryString.split("&");
    do_check_eq(params.length, 10);
    for (var k in params) {
      var pair = params[k].split("=");
      var name = decodeURIComponent(pair[0]);
      var value = decodeURIComponent(pair[1]);
      do_check_eq(expected[name], value);
    }
    response.setStatusLine(metadata.httpVersion, 404, "Not Found");
  }
}

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  server = new HttpServer();
  server.registerPathHandler("/0", requestHandler);
  server.registerPathHandler("/1", requestHandler);
  server.registerPathHandler("/2", requestHandler);
  server.registerPathHandler("/3", requestHandler);
  server.start(4444);

  Services.prefs.setBoolPref(PREF_MATCH_OS_LOCALE, false);
  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "en-US");

  startupManager();
  installAllFiles([do_get_addon(a.addon) for each (a in ADDONS)], function() {

    restartManager();
    AddonManager.getAddonByID(ADDONS[1].id, callback_soon(function(addon) {
      do_check_true(!(!addon));
      addon.userDisabled = true;
      restartManager();

      AddonManager.getAddonsByIDs([a.id for each (a in ADDONS)], function(installedItems) {
        installedItems.forEach(function(item) {
          updateListener.pendingCount++;
          item.findUpdates(updateListener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
        });
      });
    }));
  });
}
