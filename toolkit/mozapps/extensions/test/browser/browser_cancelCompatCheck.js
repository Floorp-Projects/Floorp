/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that we can cancel the add-on compatibility check while it is
// in progress (bug 772484).
// Test framework copied from browser_bug557956.js

const URI_EXTENSION_UPDATE_DIALOG = "chrome://mozapps/content/extensions/update.xul";

const PREF_GETADDONS_BYIDS            = "extensions.getAddons.get.url";
const PREF_MIN_PLATFORM_COMPAT        = "extensions.minCompatiblePlatformVersion";
const PREF_METADATA_LASTUPDATE        = "extensions.getAddons.cache.lastUpdate";

let repo = {};
Components.utils.import("resource://gre/modules/addons/AddonRepository.jsm", repo);
Components.utils.import("resource://gre/modules/Promise.jsm", this);

/**
 * Test add-ons:
 *
 * Addon    minVersion   maxVersion   Notes
 * addon1   0            *
 * addon2   0            0
 * addon3   0            0
 * addon4   1            *
 * addon5   0            0            Made compatible by update check
 * addon6   0            0            Made compatible by update check
 * addon7   0            0            Has a broken update available
 * addon8   0            0            Has an update available
 * addon9   0            0            Has an update available
 * addon10  0            0            Made incompatible by override check
 */

// describe the addons
let ao1 = { file: "browser_bug557956_1", id: "addon1@tests.mozilla.org"};
let ao2 = { file: "browser_bug557956_2", id: "addon2@tests.mozilla.org"};
let ao3 = { file: "browser_bug557956_3", id: "addon3@tests.mozilla.org"};
let ao4 = { file: "browser_bug557956_4", id: "addon4@tests.mozilla.org"};
let ao5 = { file: "browser_bug557956_5", id: "addon5@tests.mozilla.org"};
let ao6 = { file: "browser_bug557956_6", id: "addon6@tests.mozilla.org"};
let ao7 = { file: "browser_bug557956_7", id: "addon7@tests.mozilla.org"};
let ao8 = { file: "browser_bug557956_8_1", id: "addon8@tests.mozilla.org"};
let ao9 = { file: "browser_bug557956_9_1", id: "addon9@tests.mozilla.org"};
let ao10 = { file: "browser_bug557956_10", id: "addon10@tests.mozilla.org"};

// Return a promise that resolves after the specified delay in MS
function delayMS(aDelay) {
  let deferred = Promise.defer();
  setTimeout(deferred.resolve, aDelay);
  return deferred.promise;
}

// Return a promise that resolves when the specified observer topic is notified
function promise_observer(aTopic) {
  let deferred = Promise.defer();
  Services.obs.addObserver(function observe(aSubject, aObsTopic, aData) {
    Services.obs.removeObserver(arguments.callee, aObsTopic);
    deferred.resolve([aSubject, aData]);
  }, aTopic, false);
  return deferred.promise;
}

// Install a set of addons using a bogus update URL so that we can force
// the compatibility update to happen later
// @param aUpdateURL The real update URL to use after the add-ons are installed
function promise_install_test_addons(aAddonList, aUpdateURL) {
  info("Starting add-on installs");
  var installs = [];
  let deferred = Promise.defer();

  // Use a blank update URL
  Services.prefs.setCharPref(PREF_UPDATEURL, TESTROOT + "missing.rdf");

  for (let addon of aAddonList) {
    AddonManager.getInstallForURL(TESTROOT + "addons/" + addon.file + ".xpi", function(aInstall) {
      installs.push(aInstall);
    }, "application/x-xpinstall");
  }

  var listener = {
    installCount: 0,

    onInstallEnded: function() {
      this.installCount++;
      if (this.installCount == installs.length) {
        info("Done add-on installs");
        // Switch to the test update URL
        Services.prefs.setCharPref(PREF_UPDATEURL, aUpdateURL);
        deferred.resolve();
      }
    }
  };

  for (let install of installs) {
    install.addListener(listener);
    install.install();
  }

  return deferred.promise;
}

function promise_addons_by_ids(aAddonIDs) {
  info("promise_addons_by_ids " + aAddonIDs.toSource());
  let deferred = Promise.defer();
  AddonManager.getAddonsByIDs(aAddonIDs, deferred.resolve);
  return deferred.promise;
}

function* promise_uninstall_test_addons() {
  info("Starting add-on uninstalls");
  let addons = yield promise_addons_by_ids([ao1.id, ao2.id, ao3.id, ao4.id, ao5.id,
                                            ao6.id, ao7.id, ao8.id, ao9.id, ao10.id]);
  let deferred = Promise.defer();
  let uninstallCount = addons.length;
  let listener = {
    onUninstalled: function(aAddon) {
      if (aAddon) {
        info("Finished uninstalling " + aAddon.id);
      }
      if (--uninstallCount == 0) {
        info("Done add-on uninstalls");
        AddonManager.removeAddonListener(listener);
        deferred.resolve();
      }
    }};
  AddonManager.addAddonListener(listener);
  for (let addon of addons) {
    if (addon)
      addon.uninstall();
    else
      listener.onUninstalled(null);
  }
  yield deferred.promise;
}

// Returns promise{window}, resolves with a handle to the compatibility
// check window
function promise_open_compatibility_window(aInactiveAddonIds) {
  let deferred = Promise.defer();
  // This will reset the longer timeout multiplier to 2 which will give each
  // test that calls open_compatibility_window a minimum of 60 seconds to
  // complete.
  requestLongerTimeout(2);

  var variant = Cc["@mozilla.org/variant;1"].
                createInstance(Ci.nsIWritableVariant);
  variant.setFromVariant(aInactiveAddonIds);

  // Cannot be modal as we want to interract with it, shouldn't cause problems
  // with testing though.
  var features = "chrome,centerscreen,dialog,titlebar";
  var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  var win = ww.openWindow(null, URI_EXTENSION_UPDATE_DIALOG, "", features, variant);

  win.addEventListener("load", function() {
    function page_shown(aEvent) {
      if (aEvent.target.pageid)
        info("Page " + aEvent.target.pageid + " shown");
    }

    win.removeEventListener("load", arguments.callee, false);

    info("Compatibility dialog opened");

    win.addEventListener("pageshow", page_shown, false);
    win.addEventListener("unload", function() {
      win.removeEventListener("unload", arguments.callee, false);
      win.removeEventListener("pageshow", page_shown, false);
      dump("Compatibility dialog closed\n");
    }, false);

    deferred.resolve(win);
  }, false);
  return deferred.promise;
}

function promise_window_close(aWindow) {
  let deferred = Promise.defer();
  aWindow.addEventListener("unload", function() {
    aWindow.removeEventListener("unload", arguments.callee, false);
    deferred.resolve(aWindow);
  }, false);
  return deferred.promise;
}

function promise_page(aWindow, aPageId) {
  let deferred = Promise.defer();
  var page = aWindow.document.getElementById(aPageId);
  if (aWindow.document.getElementById("updateWizard").currentPage === page) {
    deferred.resolve(aWindow);
  } else {
    page.addEventListener("pageshow", function() {
      page.removeEventListener("pageshow", arguments.callee, false);
      executeSoon(function() {
        deferred.resolve(aWindow);
      });
    }, false);
  }
  return deferred.promise;
}

function get_list_names(aList) {
  var items = [];
  for (let listItem of aList.childNodes)
    items.push(listItem.label);
  items.sort();
  return items;
}

// These add-ons became inactive during the upgrade
let inactiveAddonIds = [
  ao5.id,
  ao6.id,
  ao7.id,
  ao8.id,
  ao9.id
];

// Make sure the addons in the list are not installed
function* check_addons_uninstalled(aAddonList) {
  let foundList = yield promise_addons_by_ids([addon.id for (addon of aAddonList)]);
  for (let i = 0; i < aAddonList.length; i++) {
    ok(!foundList[i], "Addon " + aAddonList[i].id + " is not installed");
  }
  info("Add-on uninstall check complete");
  yield true;
}

// Test what happens when the user cancels during AddonRepository.repopulateCache()
// Add-ons that have updates available should not update if they were disabled before
// For this test, addon8 became disabled during update and addon9 was previously disabled,
// so addon8 should update and addon9 should not
add_task(function cancel_during_repopulate() {
  let a5, a8, a9, a10;

  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, true);
  Services.prefs.setCharPref(PREF_MIN_PLATFORM_COMPAT, "0");
  Services.prefs.setCharPref(PREF_UPDATEURL, TESTROOT + "missing.rdf");

  let installsDone = promise_observer("TEST:all-updates-done");

  // Don't pull compatibility data during add-on install
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);
  // Set up our test addons so that the server-side JS has a 500ms delay to make
  // sure we cancel the dialog before we get the data we want to refill our
  // AddonRepository cache
  let addonList = [ao5, ao8, ao9, ao10];
  yield promise_install_test_addons(addonList,
                                    TESTROOT + "cancelCompatCheck.sjs?500");

  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, TESTROOT + "browser_bug557956.xml");

  [a5, a8, a9] = yield promise_addons_by_ids([ao5.id, ao8.id, ao9.id]);
  ok(!a5.isCompatible, "addon5 should not be compatible");
  ok(!a8.isCompatible, "addon8 should not be compatible");
  ok(!a9.isCompatible, "addon9 should not be compatible");

  let compatWindow = yield promise_open_compatibility_window([ao5.id, ao8.id]);
  var doc = compatWindow.document;
  yield promise_page(compatWindow, "versioninfo");

  // Brief delay to let the update window finish requesting all add-ons and start
  // reloading the addon repository
  yield delayMS(50);

  info("Cancel the compatibility check dialog");
  var button = doc.documentElement.getButton("cancel");
  EventUtils.synthesizeMouse(button, 2, 2, { }, compatWindow);

  info("Waiting for installs to complete");
  yield installsDone;
  ok(!repo.AddonRepository.isSearching, "Background installs are done");

  // There should be no active updates
  let getInstalls = Promise.defer();
  AddonManager.getAllInstalls(getInstalls.resolve);
  let installs = yield getInstalls.promise;
  is (installs.length, 0, "There should be no active installs after background installs are done");

  // addon8 should have updated in the background,
  // addon9 was listed as previously disabled so it should not have updated
  [a5, a8, a9, a10] = yield promise_addons_by_ids([ao5.id, ao8.id, ao9.id, ao10.id]);
  ok(a5.isCompatible, "addon5 should be compatible");
  ok(a8.isCompatible, "addon8 should have been upgraded");
  ok(!a9.isCompatible, "addon9 should not have been upgraded");
  ok(!a10.isCompatible, "addon10 should not be compatible");

  info("Updates done");
  yield promise_uninstall_test_addons();
  info("done uninstalling add-ons");
});

// User cancels after repopulateCache, while we're waiting for the addon.findUpdates()
// calls in gVersionInfoPage_onPageShow() to complete
// For this test, both addon8 and addon9 were disabled by this update, but addon8
// is set to not auto-update, so only addon9 should update in the background
add_task(function cancel_during_findUpdates() {
  let a5, a8, a9;

  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, true);
  Services.prefs.setCharPref(PREF_MIN_PLATFORM_COMPAT, "0");

  // Clear the AddonRepository-last-updated preference to ensure that it reloads
  Services.prefs.clearUserPref(PREF_METADATA_LASTUPDATE);
  let observeUpdateDone = promise_observer("TEST:addon-repository-data-updated");
  let installsDone = promise_observer("TEST:all-updates-done");

  // Don't pull compatibility data during add-on install
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);
  // No delay on the .sjs this time because we want the cache to repopulate
  let addonList = [ao3, ao5, ao6, ao7, ao8, ao9];
  yield promise_install_test_addons(addonList,
                                    TESTROOT + "cancelCompatCheck.sjs");

  [a8] = yield promise_addons_by_ids([ao8.id]);
  a8.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;

  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  let compatWindow = yield promise_open_compatibility_window(inactiveAddonIds);
  var doc = compatWindow.document;
  yield promise_page(compatWindow, "versioninfo");

  info("Waiting for repository-data-updated");
  yield observeUpdateDone;

  // Quick wait to make sure the findUpdates calls get queued
  yield delayMS(5);

  info("Cancel the compatibility check dialog");
  var button = doc.documentElement.getButton("cancel");
  EventUtils.synthesizeMouse(button, 2, 2, { }, compatWindow);

  info("Waiting for installs to complete 2");
  yield installsDone;
  ok(!repo.AddonRepository.isSearching, "Background installs are done 2");

  // addon8 should have updated in the background,
  // addon9 was listed as previously disabled so it should not have updated
  [a5, a8, a9] = yield promise_addons_by_ids([ao5.id, ao8.id, ao9.id]);
  ok(a5.isCompatible, "addon5 should be compatible");
  ok(!a8.isCompatible, "addon8 should not have been upgraded");
  ok(a9.isCompatible, "addon9 should have been upgraded");

  let getInstalls = Promise.defer();
  AddonManager.getAllInstalls(getInstalls.resolve);
  let installs = yield getInstalls.promise;
  is (installs.length, 0, "There should be no active installs after the dialog is cancelled 2");

  info("findUpdates done");
  yield promise_uninstall_test_addons();
});

// Cancelling during the 'mismatch' screen allows add-ons that can auto-update
// to continue updating in the background and cancels any other updates
// Same conditions as the previous test - addon8 and addon9 have updates available,
// addon8 is set to not auto-update so only addon9 should become compatible
add_task(function cancel_mismatch() {
  let a3, a5, a7, a8, a9;

  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, true);
  Services.prefs.setCharPref(PREF_MIN_PLATFORM_COMPAT, "0");

  // Clear the AddonRepository-last-updated preference to ensure that it reloads
  Services.prefs.clearUserPref(PREF_METADATA_LASTUPDATE);
  let installsDone = promise_observer("TEST:all-updates-done");

  // Don't pull compatibility data during add-on install
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);
  // No delay on the .sjs this time because we want the cache to repopulate
  let addonList = [ao3, ao5, ao6, ao7, ao8, ao9];
  yield promise_install_test_addons(addonList,
                                    TESTROOT + "cancelCompatCheck.sjs");

  [a8] = yield promise_addons_by_ids([ao8.id]);
  a8.applyBackgroundUpdates = AddonManager.AUTOUPDATE_DISABLE;

  // Check that the addons start out not compatible.
  [a3, a7, a8, a9] = yield promise_addons_by_ids([ao3.id, ao7.id, ao8.id, ao9.id]);
  ok(!a3.isCompatible, "addon3 should not be compatible");
  ok(!a7.isCompatible, "addon7 should not be compatible");
  ok(!a8.isCompatible, "addon8 should not be compatible");
  ok(!a9.isCompatible, "addon9 should not be compatible");

  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  let compatWindow = yield promise_open_compatibility_window(inactiveAddonIds);
  var doc = compatWindow.document;
  info("Wait for mismatch page");
  yield promise_page(compatWindow, "mismatch");
  info("Click the Don't Check button");
  var button = doc.documentElement.getButton("cancel");
  EventUtils.synthesizeMouse(button, 2, 2, { }, compatWindow);

  yield promise_window_close(compatWindow);
  info("Waiting for installs to complete in cancel_mismatch");
  yield installsDone;

  // addon8 should not have updated in the background,
  // addon9 was listed as previously disabled so it should not have updated
  [a5, a8, a9] = yield promise_addons_by_ids([ao5.id, ao8.id, ao9.id]);
  ok(a5.isCompatible, "addon5 should be compatible");
  ok(!a8.isCompatible, "addon8 should not have been upgraded");
  ok(a9.isCompatible, "addon9 should have been upgraded");

  // Make sure there are no pending addon installs
  let pInstalls = Promise.defer();
  AddonManager.getAllInstalls(pInstalls.resolve);
  let installs = yield pInstalls.promise;
  ok(installs.length == 0, "No remaining add-on installs (" + installs.toSource() + ")");

  yield promise_uninstall_test_addons();
  yield check_addons_uninstalled(addonList);
});

// Cancelling during the 'mismatch' screen with only add-ons that have
// no updates available
add_task(function cancel_mismatch_no_updates() {
  let a3, a5, a6

  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, true);
  Services.prefs.setCharPref(PREF_MIN_PLATFORM_COMPAT, "0");

  // Don't pull compatibility data during add-on install
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);
  // No delay on the .sjs this time because we want the cache to repopulate
  let addonList = [ao3, ao5, ao6];
  yield promise_install_test_addons(addonList,
                                    TESTROOT + "cancelCompatCheck.sjs");

  // Check that the addons start out not compatible.
  [a3, a5, a6] = yield promise_addons_by_ids([ao3.id, ao5.id, ao6.id]);
  ok(!a3.isCompatible, "addon3 should not be compatible");
  ok(!a5.isCompatible, "addon5 should not be compatible");
  ok(!a6.isCompatible, "addon6 should not be compatible");

  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);
  let compatWindow = yield promise_open_compatibility_window([ao3.id, ao5.id, ao6.id]);
  var doc = compatWindow.document;
  info("Wait for mismatch page");
  yield promise_page(compatWindow, "mismatch");
  info("Click the Don't Check button");
  var button = doc.documentElement.getButton("cancel");
  EventUtils.synthesizeMouse(button, 2, 2, { }, compatWindow);

  yield promise_window_close(compatWindow);

  [a3, a5, a6] = yield promise_addons_by_ids([ao3.id, ao5.id, ao6.id]);
  ok(!a3.isCompatible, "addon3 should not be compatible");
  ok(a5.isCompatible, "addon5 should have become compatible");
  ok(a6.isCompatible, "addon6 should have become compatible");

  // Make sure there are no pending addon installs
  let pInstalls = Promise.defer();
  AddonManager.getAllInstalls(pInstalls.resolve);
  let installs = yield pInstalls.promise;
  ok(installs.length == 0, "No remaining add-on installs (" + installs.toSource() + ")");

  yield promise_uninstall_test_addons();
  yield check_addons_uninstalled(addonList);
});
