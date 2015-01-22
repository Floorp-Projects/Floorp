/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the compatibility dialog that displays during startup when the browser
// version changes.

const URI_EXTENSION_UPDATE_DIALOG = "chrome://mozapps/content/extensions/update.xul";

const PREF_GETADDONS_BYIDS            = "extensions.getAddons.get.url";
const PREF_MIN_PLATFORM_COMPAT        = "extensions.minCompatiblePlatformVersion";

Services.prefs.setBoolPref(PREF_STRICT_COMPAT, true);
// avoid the 'leaked window property' check
let scope = {};
Components.utils.import("resource://gre/modules/TelemetrySession.jsm", scope);
let TelemetrySession = scope.TelemetrySession;

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
 */

function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();

  run_next_test();
}

function end_test() {
  // Test generates a lot of available installs so just cancel them all
  AddonManager.getAllInstalls(function(aInstalls) {
    for (let install of aInstalls)
      install.cancel();

    finish();
  });
}

function install_test_addons(aCallback) {
  var installs = [];

  // Use a blank update URL
  Services.prefs.setCharPref(PREF_UPDATEURL, TESTROOT + "missing.rdf");

  let names = ["browser_bug557956_1",
               "browser_bug557956_2",
               "browser_bug557956_3",
               "browser_bug557956_4",
               "browser_bug557956_5",
               "browser_bug557956_6",
               "browser_bug557956_7",
               "browser_bug557956_8_1",
               "browser_bug557956_9_1",
               "browser_bug557956_10"];
  for (let name of names) {
    AddonManager.getInstallForURL(TESTROOT + "addons/" + name + ".xpi", function(aInstall) {
      installs.push(aInstall);
    }, "application/x-xpinstall");
  }

  var listener = {
    installCount: 0,

    onInstallEnded: function() {
      this.installCount++;
      if (this.installCount == installs.length) {
        // Switch to the test update URL
        Services.prefs.setCharPref(PREF_UPDATEURL, TESTROOT + "browser_bug557956.rdf");

        executeSoon(aCallback);
      }
    }
  };

  for (let install of installs) {
    install.addListener(listener);
    install.install();
  }
}

function uninstall_test_addons(aCallback) {
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org",
                               "addon8@tests.mozilla.org",
                               "addon9@tests.mozilla.org",
                               "addon10@tests.mozilla.org"],
                               function(aAddons) {
    for (let addon of aAddons) {
      if (addon)
        addon.uninstall();
    }
    aCallback();
  });
}

// Open the compatibility dialog, with the list of addon IDs
// that were disabled by this "update"
function open_compatibility_window(aDisabledAddons, aCallback) {
  // This will reset the longer timeout multiplier to 2 which will give each
  // test that calls open_compatibility_window a minimum of 60 seconds to
  // complete.
  requestLongerTimeout(2);

  var variant = Cc["@mozilla.org/variant;1"].
                createInstance(Ci.nsIWritableVariant);
  variant.setFromVariant(aDisabledAddons);

  // Cannot be modal as we want to interact with it, shouldn't cause problems
  // with testing though.
  var features = "chrome,centerscreen,dialog,titlebar";
  var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  var win = ww.openWindow(null, URI_EXTENSION_UPDATE_DIALOG, "", features, variant);

  win.addEventListener("load", function() {
    win.removeEventListener("load", arguments.callee, false);

    info("Compatibility dialog opened");

    function page_shown(aEvent) {
      if (aEvent.target.pageid)
        info("Page " + aEvent.target.pageid + " shown");
    }

    win.addEventListener("pageshow", page_shown, false);
    win.addEventListener("unload", function() {
      win.removeEventListener("unload", arguments.callee, false);
      win.removeEventListener("pageshow", page_shown, false);
      info("Compatibility dialog closed");
    }, false);

    aCallback(win);
  }, false);
}

function wait_for_window_close(aWindow, aCallback) {
  aWindow.addEventListener("unload", function() {
    aWindow.removeEventListener("unload", arguments.callee, false);
    aCallback();
  }, false);
}

function wait_for_page(aWindow, aPageId, aCallback) {
  var page = aWindow.document.getElementById(aPageId);
  page.addEventListener("pageshow", function() {
    page.removeEventListener("pageshow", arguments.callee, false);
    executeSoon(function() {
      aCallback(aWindow);
    });
  }, false);
}

function get_list_names(aList) {
  var items = [];
  for (let listItem of aList.childNodes)
    items.push(listItem.label);
  items.sort();
  return items;
}

function check_telemetry({disabled, metaenabled, metadisabled, upgraded, failed, declined}) {
  let ping = TelemetrySession.getPayload();
  // info(JSON.stringify(ping));
  let am = ping.simpleMeasurements.addonManager;
  if (disabled !== undefined)
    is(am.appUpdate_disabled, disabled, disabled + " add-ons disabled by version change");
  if (metaenabled !== undefined)
    is(am.appUpdate_metadata_enabled, metaenabled, metaenabled + " add-ons enabled by metadata");
  if (metadisabled !== undefined)
    is(am.appUpdate_metadata_disabled, metadisabled, metadisabled + " add-ons disabled by metadata");
  if (upgraded !== undefined)
    is(am.appUpdate_upgraded, upgraded, upgraded + " add-ons upgraded");
  if (failed !== undefined)
    is(am.appUpdate_upgradeFailed, failed, failed + " upgrades failed");
  if (declined !== undefined)
    is(am.appUpdate_upgradeDeclined, declined, declined + " upgrades declined");
}

add_test(function test_setup() {
  TelemetrySession.setup().then(run_next_test);
});

// Tests that the right add-ons show up in the mismatch dialog and updates can
// be installed
add_test(function basic_mismatch() {
  install_test_addons(function() {
    // These add-ons become disabled
    var disabledAddonIds = [
      "addon3@tests.mozilla.org",
      "addon6@tests.mozilla.org",
      "addon7@tests.mozilla.org",
      "addon8@tests.mozilla.org",
      "addon9@tests.mozilla.org"
    ];

    AddonManager.getAddonsByIDs(["addon5@tests.mozilla.org",
                                 "addon6@tests.mozilla.org"],
                                 function([a5, a6]) {
      // Check starting (pre-update) conditions
      ok(!a5.isCompatible, "addon5 should not be compatible");
      ok(!a6.isCompatible, "addon6 should not be compatible");

      open_compatibility_window(disabledAddonIds, function(aWindow) {
        var doc = aWindow.document;
        wait_for_page(aWindow, "mismatch", function(aWindow) {
          var items = get_list_names(doc.getElementById("mismatch.incompatible"));
          // Check that compatibility updates from individual add-on update checks were applied.
          is(items.length, 4, "Should have seen 4 still incompatible items");
          is(items[0], "Addon3 1.0", "Should have seen addon3 still incompatible");
          is(items[1], "Addon7 1.0", "Should have seen addon7 still incompatible");
          is(items[2], "Addon8 1.0", "Should have seen addon8 still incompatible");
          is(items[3], "Addon9 1.0", "Should have seen addon9 still incompatible");

          // If it wasn't disabled by this run, we don't try to enable it
          ok(!a5.isCompatible, "addon5 should not be compatible");
          ok(a6.isCompatible, "addon6 should be compatible");

          var button = doc.documentElement.getButton("next");
          EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);

          wait_for_page(aWindow, "found", function(aWindow) {
            ok(doc.getElementById("xpinstallDisabledAlert").hidden,
               "Install should be allowed");

            var list = doc.getElementById("found.updates");
            var items = get_list_names(list);
            is(items.length, 3, "Should have seen 3 updates available");
            is(items[0], "Addon7 2.0", "Should have seen update for addon7");
            is(items[1], "Addon8 2.0", "Should have seen update for addon8");
            is(items[2], "Addon9 2.0", "Should have seen update for addon9");

            ok(!doc.documentElement.getButton("next").disabled,
               "Next button should be enabled");

            // Uncheck all
            for (let listItem of list.childNodes)
              EventUtils.synthesizeMouse(listItem, 2, 2, { }, aWindow);

            ok(doc.documentElement.getButton("next").disabled,
               "Next button should not be enabled");

            // Check the ones we want to install
            for (let listItem of list.childNodes) {
              if (listItem.label != "Addon7 2.0")
                EventUtils.synthesizeMouse(listItem, 2, 2, { }, aWindow);
            }

            var button = doc.documentElement.getButton("next");
            EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);

            wait_for_page(aWindow, "finished", function(aWindow) {
              var button = doc.documentElement.getButton("finish");
              ok(!button.hidden, "Finish button should not be hidden");
              ok(!button.disabled, "Finish button should not be disabled");
              EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);

              wait_for_window_close(aWindow, function() {
                AddonManager.getAddonsByIDs(["addon8@tests.mozilla.org",
                                             "addon9@tests.mozilla.org"],
                                             function([a8, a9]) {
                  is(a8.version, "2.0", "addon8 should have updated");
                  is(a9.version, "2.0", "addon9 should have updated");
  
                  check_telemetry({disabled: 5, metaenabled: 1, metadisabled: 0,
                                   upgraded: 2, failed: 0, declined: 1});

                  uninstall_test_addons(run_next_test);
                });
              });
            });
          });
        });
      });
    });
  });
});

// Tests that the install failures show the install failed page and disabling
// xpinstall shows the right UI.
add_test(function failure_page() {
  install_test_addons(function() {
    // These add-ons become disabled
    var disabledAddonIds = [
      "addon3@tests.mozilla.org",
      "addon6@tests.mozilla.org",
      "addon7@tests.mozilla.org",
      "addon8@tests.mozilla.org",
      "addon9@tests.mozilla.org"
    ];

    Services.prefs.setBoolPref("xpinstall.enabled", false);

    open_compatibility_window(disabledAddonIds, function(aWindow) {
      var doc = aWindow.document;
      wait_for_page(aWindow, "mismatch", function(aWindow) {
        var items = get_list_names(doc.getElementById("mismatch.incompatible"));
        is(items.length, 4, "Should have seen 4 still incompatible items");
        is(items[0], "Addon3 1.0", "Should have seen addon3 still incompatible");
        is(items[1], "Addon7 1.0", "Should have seen addon7 still incompatible");
        is(items[2], "Addon8 1.0", "Should have seen addon8 still incompatible");
        is(items[3], "Addon9 1.0", "Should have seen addon9 still incompatible");

        // Check that compatibility updates were applied.
        AddonManager.getAddonsByIDs(["addon5@tests.mozilla.org",
                                     "addon6@tests.mozilla.org"],
                                     function([a5, a6]) {
          ok(!a5.isCompatible, "addon5 should not be compatible");
          ok(a6.isCompatible, "addon6 should be compatible");

          var button = doc.documentElement.getButton("next");
          EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);

          wait_for_page(aWindow, "found", function(aWindow) {
            ok(!doc.getElementById("xpinstallDisabledAlert").hidden,
               "Install should not be allowed");

            ok(doc.documentElement.getButton("next").disabled,
               "Next button should be disabled");

            var checkbox = doc.getElementById("enableXPInstall");
            EventUtils.synthesizeMouse(checkbox, 2, 2, { }, aWindow);

            ok(!doc.documentElement.getButton("next").disabled,
               "Next button should be enabled");

            var list = doc.getElementById("found.updates");
            var items = get_list_names(list);
            is(items.length, 3, "Should have seen 3 updates available");
            is(items[0], "Addon7 2.0", "Should have seen update for addon7");
            is(items[1], "Addon8 2.0", "Should have seen update for addon8");
            is(items[2], "Addon9 2.0", "Should have seen update for addon9");

            // Unheck the ones we don't want to install
            for (let listItem of list.childNodes) {
              if (listItem.label != "Addon7 2.0")
                EventUtils.synthesizeMouse(listItem, 2, 2, { }, aWindow);
            }

            var button = doc.documentElement.getButton("next");
            EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);

            wait_for_page(aWindow, "installerrors", function(aWindow) {
              var button = doc.documentElement.getButton("finish");
              ok(!button.hidden, "Finish button should not be hidden");
              ok(!button.disabled, "Finish button should not be disabled");

              wait_for_window_close(aWindow, function() {
                uninstall_test_addons(run_next_test);
              });

              check_telemetry({disabled: 5, metaenabled: 1, metadisabled: 0,
                               upgraded: 0, failed: 1, declined: 2});

              EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);
            });
          });
        });
      });
    });
  });
});

// Tests that no add-ons show up in the mismatch dialog when they are all disabled
add_test(function all_disabled() {
  install_test_addons(function() {
    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org",
                                 "addon3@tests.mozilla.org",
                                 "addon4@tests.mozilla.org",
                                 "addon5@tests.mozilla.org",
                                 "addon6@tests.mozilla.org",
                                 "addon7@tests.mozilla.org",
                                 "addon8@tests.mozilla.org",
                                 "addon9@tests.mozilla.org",
                                 "addon10@tests.mozilla.org"],
                                 function(aAddons) {
      for (let addon of aAddons)
        addon.userDisabled = true;

      open_compatibility_window([], function(aWindow) {
        // Should close immediately on its own
        wait_for_window_close(aWindow, function() {
          uninstall_test_addons(run_next_test);
        });
      });
    });
  });
});

// Tests that the right UI shows for when no updates are available
add_test(function no_updates() {
  install_test_addons(function() {
    AddonManager.getAddonsByIDs(["addon7@tests.mozilla.org",
                                 "addon8@tests.mozilla.org",
                                 "addon9@tests.mozilla.org",
                                 "addon10@tests.mozilla.org"],
                                 function(aAddons) {
      for (let addon of aAddons)
        addon.uninstall();

      // These add-ons were disabled by the upgrade
      var inactiveAddonIds = [
        "addon3@tests.mozilla.org",
        "addon5@tests.mozilla.org",
        "addon6@tests.mozilla.org"
      ];

      open_compatibility_window(inactiveAddonIds, function(aWindow) {
        var doc = aWindow.document;
        wait_for_page(aWindow, "mismatch", function(aWindow) {
          var items = get_list_names(doc.getElementById("mismatch.incompatible"));
          is(items.length, 1, "Should have seen 1 still incompatible items");
          is(items[0], "Addon3 1.0", "Should have seen addon3 still incompatible");

          var button = doc.documentElement.getButton("next");
          EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);

          wait_for_page(aWindow, "noupdates", function(aWindow) {
            var button = doc.documentElement.getButton("finish");
            ok(!button.hidden, "Finish button should not be hidden");
            ok(!button.disabled, "Finish button should not be disabled");

            wait_for_window_close(aWindow, function() {
              uninstall_test_addons(run_next_test);
            });

            EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);
          });
        });
      });
    });
  });
});

// Tests that compatibility overrides are retrieved and affect addon
// compatibility.
add_test(function overrides_retrieved() {
  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, false);
  Services.prefs.setCharPref(PREF_MIN_PLATFORM_COMPAT, "0");
  is(AddonManager.strictCompatibility, false, "Strict compatibility should be disabled");

  // Use a blank update URL
  Services.prefs.setCharPref(PREF_UPDATEURL, TESTROOT + "missing.rdf");

  install_test_addons(function() {

    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org",
                                 "addon3@tests.mozilla.org",
                                 "addon4@tests.mozilla.org",
                                 "addon5@tests.mozilla.org",
                                 "addon6@tests.mozilla.org",
                                 "addon7@tests.mozilla.org",
                                 "addon8@tests.mozilla.org",
                                 "addon9@tests.mozilla.org",
                                 "addon10@tests.mozilla.org"],
                                 function(aAddons) {

      for (let addon of aAddons) {
        if (addon.id == "addon10@tests.mozilla.org")
          is(addon.isCompatible, true, "Addon10 should be compatible before compat overrides are refreshed");
        else
          addon.uninstall();
      }

      Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, TESTROOT + "browser_bug557956.xml");
      Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

      open_compatibility_window([], function(aWindow) {
        var doc = aWindow.document;
        wait_for_page(aWindow, "mismatch", function(aWindow) {
          var items = get_list_names(doc.getElementById("mismatch.incompatible"));
          is(items.length, 1, "Should have seen 1 incompatible item");
          is(items[0], "Addon10 1.0", "Should have seen addon10 as incompatible");

          var button = doc.documentElement.getButton("next");
          EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);

          wait_for_page(aWindow, "noupdates", function(aWindow) {
            var button = doc.documentElement.getButton("finish");
            ok(!button.hidden, "Finish button should not be hidden");
            ok(!button.disabled, "Finish button should not be disabled");

            wait_for_window_close(aWindow, function() {
              uninstall_test_addons(run_next_test);
            });

            check_telemetry({disabled: 0, metaenabled: 0, metadisabled: 1,
                             upgraded: 0, failed: 0, declined: 0});

            EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);
          });
        });
      });
    });
  });
});

add_test(function test_shutdown() {
  TelemetrySession.shutdown().then(run_next_test);
});
