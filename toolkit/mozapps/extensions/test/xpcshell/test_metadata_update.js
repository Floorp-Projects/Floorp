/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Test whether we need to block start-up to check add-on compatibility,
 * based on how long since the last succesful metadata ping.
 * All tests depend on having one add-on installed in the profile
 */


const URI_EXTENSION_UPDATE_DIALOG     = "chrome://mozapps/content/extensions/update.xul";
const PREF_EM_SHOW_MISMATCH_UI        = "extensions.showMismatchUI";

// Constants copied from AddonRepository.jsm
const PREF_METADATA_LASTUPDATE           = "extensions.getAddons.cache.lastUpdate";
const PREF_METADATA_UPDATETHRESHOLD_SEC  = "extensions.getAddons.cache.updateThreshold";
const DEFAULT_METADATA_UPDATETHRESHOLD_SEC = 172800;  // two days

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);
// None of this works without the add-on repository cache
Services.prefs.setBoolPref("extensions.getAddons.cache.enabled", true);

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");
var testserver;

const profileDir = gProfD.clone();
profileDir.append("extensions");

// This will be called to show the compatibility update dialog.
var WindowWatcher = {
  expected: false,
  arguments: null,

  openWindow: function(parent, url, name, features, args) {
    do_check_true(Services.startup.interrupted);
    do_check_eq(url, URI_EXTENSION_UPDATE_DIALOG);
    do_check_true(this.expected);
    this.expected = false;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}

var WindowWatcherFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return WindowWatcher.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                          "Fake Window Watcher",
                          "@mozilla.org/embedcomp/window-watcher;1", WindowWatcherFactory);

// Return Date.now() in seconds, rounded
function now() {
  return Math.round(Date.now() / 1000);
}

// First time with a new profile, so we don't have a cache.lastUpdate pref
add_task(function* checkFirstMetadata() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  Services.prefs.setBoolPref(PREF_EM_SHOW_MISMATCH_UI, true);

  // Create and configure the HTTP server.
  testserver = new HttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.start(-1);
  gPort = testserver.identity.primaryPort;
  const BASE_URL  = "http://localhost:" + gPort;
  const GETADDONS_RESULTS = BASE_URL + "/data/test_AddonRepository_cache.xml";
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, GETADDONS_RESULTS);

  // Put an add-on in our profile so the metadata check will have something to do
  var min1max2 = {
    id: "min1max2@tests.mozilla.org",
    version: "1.0",
    name: "Test addon compatible with v1->v2",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  };
  writeInstallRDFForExtension(min1max2, profileDir);

  startupManager();

  // Make sure that updating metadata for the first time sets the lastUpdate preference
  yield AddonRepository.repopulateCache();
  do_print("Update done, getting last update");
  let lastUpdate = Services.prefs.getIntPref(PREF_METADATA_LASTUPDATE);
  do_check_true(lastUpdate > 0);

  // Make sure updating metadata again updates the preference
  let oldUpdate = lastUpdate - 2 * DEFAULT_METADATA_UPDATETHRESHOLD_SEC;
  Services.prefs.setIntPref(PREF_METADATA_LASTUPDATE, oldUpdate);
  yield AddonRepository.repopulateCache();
  do_check_neq(oldUpdate, Services.prefs.getIntPref(PREF_METADATA_LASTUPDATE));
});

// First upgrade with no lastUpdate pref and no add-ons changing shows UI
add_task(function* upgrade_no_lastupdate() {
  Services.prefs.clearUserPref(PREF_METADATA_LASTUPDATE);

  WindowWatcher.expected = true;
  yield promiseRestartManager("2");
  do_check_false(WindowWatcher.expected);
});

// Upgrade with lastUpdate more than default threshold and no add-ons changing shows UI
add_task(function* upgrade_old_lastupdate() {
  let oldEnough = now() - DEFAULT_METADATA_UPDATETHRESHOLD_SEC - 1000;
  Services.prefs.setIntPref(PREF_METADATA_LASTUPDATE, oldEnough);

  WindowWatcher.expected = true;
  // upgrade, downgrade, it has the same effect on the code path under test
  yield promiseRestartManager("1");
  do_check_false(WindowWatcher.expected);
});

// Upgrade with lastUpdate less than default threshold and no add-ons changing doesn't show
add_task(function* upgrade_young_lastupdate() {
  let notOldEnough = now() - DEFAULT_METADATA_UPDATETHRESHOLD_SEC + 1000;
  Services.prefs.setIntPref(PREF_METADATA_LASTUPDATE, notOldEnough);

  WindowWatcher.expected = false;
  yield promiseRestartManager("2");
  do_check_false(WindowWatcher.expected);
});

// Repeat more-than and less-than but with updateThreshold preference set
// Upgrade with lastUpdate more than pref threshold and no add-ons changing shows UI
const TEST_UPDATETHRESHOLD_SEC = 50000;
add_task(function* upgrade_old_pref_lastupdate() {
  Services.prefs.setIntPref(PREF_METADATA_UPDATETHRESHOLD_SEC, TEST_UPDATETHRESHOLD_SEC);

  let oldEnough = now() - TEST_UPDATETHRESHOLD_SEC - 1000;
  Services.prefs.setIntPref(PREF_METADATA_LASTUPDATE, oldEnough);

  WindowWatcher.expected = true;
  yield promiseRestartManager("1");
  do_check_false(WindowWatcher.expected);
});

// Upgrade with lastUpdate less than pref threshold and no add-ons changing doesn't show
add_task(function* upgrade_young_pref_lastupdate() {
  let notOldEnough = now() - TEST_UPDATETHRESHOLD_SEC + 1000;
  Services.prefs.setIntPref(PREF_METADATA_LASTUPDATE, notOldEnough);

  WindowWatcher.expected = false;
  yield promiseRestartManager("2");
  do_check_false(WindowWatcher.expected);
});



add_task(function* cleanup() {
  return new Promise((resolve, reject) => {
    testserver.stop(resolve);
  });
});

function run_test() {
  run_next_test();
}
