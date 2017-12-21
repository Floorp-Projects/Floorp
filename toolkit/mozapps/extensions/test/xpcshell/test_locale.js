/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  do_test_pending();

  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  Services.locale.setRequestedLocales(["fr-FR"]);

  startupManager();

  run_test_1();
}

// Tests that the localized properties are visible before installation
function run_test_1() {
  AddonManager.getInstallForFile(do_get_addon("test_locale"), function(install) {
    Assert.equal(install.addon.name, "fr-FR Name");
    Assert.equal(install.addon.description, "fr-FR Description");

    prepare_test({
      "addon1@tests.mozilla.org": [
        "onInstalling"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], callback_soon(run_test_2));
    install.install();
  });
}

// Tests that the localized properties are visible after installation
function run_test_2() {
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    Assert.notEqual(addon, null);

    Assert.equal(addon.name, "fr-FR Name");
    Assert.equal(addon.description, "fr-FR Description");

    addon.userDisabled = true;
    executeSoon(run_test_3);
  });
}

// Test that the localized properties are still there when disabled.
function run_test_3() {
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    Assert.notEqual(addon, null);
    Assert.equal(addon.name, "fr-FR Name");

    executeSoon(run_test_4);
  });
}

// Localised preference values should be ignored when the add-on is disabled
function run_test_4() {
  Services.prefs.setCharPref("extensions.addon1@tests.mozilla.org.name", "Name from prefs");
  Services.prefs.setCharPref("extensions.addon1@tests.mozilla.org.contributor.1", "Contributor 1");
  Services.prefs.setCharPref("extensions.addon1@tests.mozilla.org.contributor.2", "Contributor 2");
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    Assert.notEqual(addon, null);
    Assert.equal(addon.name, "fr-FR Name");
    let contributors = addon.contributors;
    Assert.equal(contributors.length, 3);
    Assert.equal(contributors[0], "Fr Contributor 1");
    Assert.equal(contributors[1], "Fr Contributor 2");
    Assert.equal(contributors[2], "Fr Contributor 3");

    executeSoon(run_test_5);
  });
}

// Test that changing locale works
function run_test_5() {
  Services.locale.setRequestedLocales(["de-DE"]);
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    Assert.notEqual(addon, null);

    Assert.equal(addon.name, "de-DE Name");
    Assert.equal(addon.description, null);

    executeSoon(run_test_6);
  });
}

// Test that missing locales use the fallbacks
function run_test_6() {
  Services.locale.setRequestedLocales(["nl-NL"]);
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(function(addon) {
    Assert.notEqual(addon, null);

    Assert.equal(addon.name, "Fallback Name");
    Assert.equal(addon.description, "Fallback Description");

    addon.userDisabled = false;
    executeSoon(run_test_7);
  }));
}

// Test that the prefs will override the fallbacks
function run_test_7() {
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    Assert.notEqual(addon, null);

    Assert.equal(addon.name, "Name from prefs");

    executeSoon(run_test_8);
  });
}

// Test that the prefs will override localized values from the manifest
function run_test_8() {
  Services.locale.setRequestedLocales(["fr-FR"]);
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    Assert.notEqual(addon, null);

    Assert.equal(addon.name, "Name from prefs");
    let contributors = addon.contributors;
    Assert.equal(contributors.length, 2);
    Assert.equal(contributors[0], "Contributor 1");
    Assert.equal(contributors[1], "Contributor 2");

    executeSoon(do_test_finished);
  });
}
