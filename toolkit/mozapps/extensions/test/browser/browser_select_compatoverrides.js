/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that compatibility overrides are refreshed when showing the addon
// selection UI.

const PREF_GETADDONS_BYIDS            = "extensions.getAddons.get.url";
const PREF_MIN_PLATFORM_COMPAT        = "extensions.minCompatiblePlatformVersion";

var gTestAddon = null;
var gWin;

function waitForView(aView, aCallback) {
  var view = gWin.document.getElementById(aView);
  if (view.parentNode.selectedPanel == view) {
    aCallback();
    return;
  }

  view.addEventListener("ViewChanged", function() {
    view.removeEventListener("ViewChanged", arguments.callee, false);
    aCallback();
  }, false);
}

function install_test_addon(aCallback) {
  AddonManager.getInstallForURL(TESTROOT + "addons/browser_select_compatoverrides_1.xpi", function(aInstall) {
    var listener = {
      onInstallEnded: function() {
        AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
          gTestAddon = addon;
          aCallback();
        });
      }
    };
    aInstall.addListener(listener);
    aInstall.install();
  }, "application/x-xpinstall");
}

registerCleanupFunction(function() {
  if (gWin)
    gWin.close();
  if (gTestAddon)
    gTestAddon.uninstall();

  Services.prefs.clearUserPref(PREF_MIN_PLATFORM_COMPAT);
});

function end_test() {
  finish();
}


function test() {
  waitForExplicitFinish();
  Services.prefs.setCharPref(PREF_UPDATEURL, TESTROOT + "missing.rdf");
  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, false);
  Services.prefs.setCharPref(PREF_MIN_PLATFORM_COMPAT, "0");

  install_test_addon(run_next_test);
}

add_test(function() {
  gWin = Services.ww.openWindow(null,
                              "chrome://mozapps/content/extensions/selectAddons.xul",
                              "",
                              "chrome,centerscreen,dialog,titlebar",
                              null);
  waitForFocus(function() {
    waitForView("select", run_next_test);
  }, gWin);
});

add_test(function() {
  for (var row = gWin.document.getElementById("select-rows").firstChild; row; row = row.nextSibling) {
    if (row.localName == "separator")
      continue;
    if (row.id.substr(-18) != "@tests.mozilla.org")
      continue;

    is(row.id, "addon1@tests.mozilla.org", "Should get expected addon");
    isnot(row.action, "incompatible", "Addon should not be incompatible");

    gWin.close();
    gWin = null;
    run_next_test();
  }
});

add_test(function() {
  Services.prefs.setCharPref(PREF_GETADDONS_BYIDS, TESTROOT + "browser_select_compatoverrides.xml");
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

  gWin = Services.ww.openWindow(null,
                              "chrome://mozapps/content/extensions/selectAddons.xul",
                              "",
                              "chrome,centerscreen,dialog,titlebar",
                              null);
  waitForFocus(function() {
    waitForView("select", run_next_test);
  }, gWin);
});

add_test(function() {
  for (var row = gWin.document.getElementById("select-rows").firstChild; row; row = row.nextSibling) {
    if (row.localName == "separator")
      continue;
    if (row.id.substr(-18) != "@tests.mozilla.org")
      continue;
    is(row.id, "addon1@tests.mozilla.org", "Should get expected addon");
    is(row.action, "incompatible", "Addon should be incompatible");
    run_next_test();
  }
});
