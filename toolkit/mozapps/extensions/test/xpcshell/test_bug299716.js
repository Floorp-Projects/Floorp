/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Disables security checking our updates which haven't been signed
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

// Update check listener.
const checkListener = {
  pendingCount: 0,

  onUpdateAvailable: function onUpdateAvailable(aAddon, aInstall) {
    for (let currentAddon of ADDONS) {
      if (currentAddon.id == aAddon.id) {
       currentAddon.newInstall = aInstall;
       return;
      }
    }
  },

  onUpdateFinished: function onUpdateFinished() {
    if (--this.pendingCount == 0)
      next_test();
  }
}

// Get the HTTP server.
Components.utils.import("resource://testing-common/httpd.js");
var testserver;

var ADDONS = [
  // XPCShell
  {
    id: "bug299716-a@tests.mozilla.org",
    addon: "test_bug299716_a_1",
    installed: true,
    item: null,
    newInstall: null
  },

  // Toolkit
  {
    id: "bug299716-b@tests.mozilla.org",
    addon: "test_bug299716_b_1",
    installed: true,
    item: null,
    newInstall: null
  },

  // XPCShell + Toolkit
  {
    id: "bug299716-c@tests.mozilla.org",
    addon: "test_bug299716_c_1",
    installed: true,
    item: null,
    newInstall: null
  },

  // XPCShell (Toolkit invalid)
  {
    id: "bug299716-d@tests.mozilla.org",
    addon: "test_bug299716_d_1",
    installed: true,
    item: null,
    newInstall: null
  },

  // Toolkit (XPCShell invalid)
  {
    id: "bug299716-e@tests.mozilla.org",
    addon: "test_bug299716_e_1",
    installed: false,
    item: null,
    newInstall: null,
    failedAppName: "XPCShell"
  },

  // None (XPCShell, Toolkit invalid)
  {
    id: "bug299716-f@tests.mozilla.org",
    addon: "test_bug299716_f_1",
    installed: false,
    item: null,
    newInstall: null,
    failedAppName: "XPCShell"
  },

  // None (Toolkit invalid)
  {
    id: "bug299716-g@tests.mozilla.org",
    addon: "test_bug299716_g_1",
    installed: false,
    item: null,
    newInstall: null,
    failedAppName: "Toolkit"
  },
];

var next_test = function() {};

function do_check_item(aItem, aVersion, aAddonsEntry) {
  if (aAddonsEntry.installed) {
    if (aItem == null)
      do_throw("Addon " + aAddonsEntry.id + " wasn't detected");
    if (aItem.version != aVersion)
      do_throw("Addon " + aAddonsEntry.id + " was version " + aItem.version + " instead of " + aVersion);
  } else {
    if (aItem != null)
      do_throw("Addon " + aAddonsEntry.id + " was detected");
  }
}

/**
 * Start the test by installing extensions.
 */
function run_test() {
  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "5", "1.9");

  const dataDir = do_get_file("data");
  const addonsDir = do_get_addon(ADDONS[0].addon).parent;

  // Make sure we can actually get our data files.
  const xpiFile = addonsDir.clone();
  xpiFile.append("test_bug299716_a_2.xpi");
  do_check_true(xpiFile.exists());

  // Create and configure the HTTP server.
  testserver = new HttpServer();
  testserver.registerDirectory("/addons/", addonsDir);
  testserver.registerDirectory("/data/", dataDir);
  testserver.start(4444);

  // Make sure we can fetch the files over HTTP.
  const Ci = Components.interfaces;
  const xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                        .createInstance(Ci.nsIXMLHttpRequest)
  xhr.open("GET", "http://localhost:4444/addons/test_bug299716_a_2.xpi", false);
  xhr.send(null);
  do_check_true(xhr.status == 200);

  xhr.open("GET", "http://localhost:4444/data/test_bug299716.rdf", false);
  xhr.send(null);
  do_check_true(xhr.status == 200);

  // Start the real test.
  startupManager();
  dump("\n\n*** INSTALLING NEW ITEMS\n\n");

  installAllFiles(ADDONS.map(a => do_get_addon(a.addon)), run_test_pt2,
                  true);
}

/**
 * Check the versions of all items, and ask the extension manager to find updates.
 */
function run_test_pt2() {
  dump("\n\n*** DONE INSTALLING NEW ITEMS\n\n");
  dump("\n\n*** RESTARTING EXTENSION MANAGER\n\n");
  restartManager();

  AddonManager.getAddonsByIDs(ADDONS.map(a => a.id), function(items) {
    dump("\n\n*** REQUESTING UPDATE\n\n");
    // checkListener will call run_test_pt3().
    next_test = run_test_pt3;

    // Try to update the items.
    for (var i = 0; i < ADDONS.length; i++) {
      var item = items[i];
      do_check_item(item, "0.1", ADDONS[i]);

      if (item) {
        checkListener.pendingCount++;
        ADDONS[i].item = item;
        item.findUpdates(checkListener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
      }
    }
  });
}

/**
 * Install new items for each enabled extension.
 */
function run_test_pt3() {
  // Install the new items.
  dump("\n\n*** UPDATING ITEMS\n\n");
  completeAllInstalls(ADDONS.filter(a => a.newInstall).map(a => a.newInstall),
                      run_test_pt4);
}

/**
 * Check the final version of each extension.
 */
function run_test_pt4() {
  dump("\n\n*** RESTARTING EXTENSION MANAGER\n\n");
  restartManager();

  dump("\n\n*** FINAL CHECKS\n\n");
  AddonManager.getAddonsByIDs(ADDONS.map(a => a.id), function(items) {
    for (var i = 0; i < ADDONS.length; i++) {
      var item = items[i];
      do_check_item(item, "0.2", ADDONS[i]);
    }

    testserver.stop(do_test_finished);
  });
}
