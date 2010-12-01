/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that localized properties work as expected

const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE = "general.useragent.locale";


function run_test() {
  do_test_pending();

  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  Services.prefs.setBoolPref(PREF_MATCH_OS_LOCALE, false);
  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "fr-FR");

  startupManager();

  run_test_1();
}

// Tests that the localized properties are visible before installation
function run_test_1() {
  AddonManager.getInstallForFile(do_get_addon("test_locale"), function(install) {
    do_check_eq(install.addon.name, "fr-FR Name");
    do_check_eq(install.addon.description, "fr-FR Description");

    prepare_test({
      "addon1@tests.mozilla.org": [
        "onInstalling"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], run_test_2);
    install.install();
  });
}

// Tests that the localized properties are visible after installation
function run_test_2() {
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);

    do_check_eq(addon.name, "fr-FR Name");
    do_check_eq(addon.description, "fr-FR Description");

    addon.userDisabled = true;
    restartManager();

    run_test_3();
  });
}

// Test that the localized properties are still there when disabled.
function run_test_3() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    do_check_eq(addon.name, "fr-FR Name");

    run_test_4();
  });
}

// Localised preference values should be ignored when the add-on is disabled
function run_test_4() {
  Services.prefs.setCharPref("extensions.addon1@tests.mozilla.org.name", "Name from prefs");
  Services.prefs.setCharPref("extensions.addon1@tests.mozilla.org.contributor.1", "Contributor 1");
  Services.prefs.setCharPref("extensions.addon1@tests.mozilla.org.contributor.2", "Contributor 2");
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    do_check_eq(addon.name, "fr-FR Name");
    let contributors = addon.contributors;
    do_check_eq(contributors.length, 3);
    do_check_eq(contributors[0], "Fr Contributor 1");
    do_check_eq(contributors[1], "Fr Contributor 2");
    do_check_eq(contributors[2], "Fr Contributor 3");

    run_test_5();
  });
}

// Test that changing locale works
function run_test_5() {
  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "de-DE");
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);

    do_check_eq(addon.name, "de-DE Name");
    do_check_eq(addon.description, null);

    run_test_6();
  });
}

// Test that missing locales use the fallbacks
function run_test_6() {
  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "nl-NL");
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);

    do_check_eq(addon.name, "Fallback Name");
    do_check_eq(addon.description, "Fallback Description");

    addon.userDisabled = false;
    restartManager();
    run_test_7();
  });
}

// Test that the prefs will override the fallbacks
function run_test_7() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);

    do_check_eq(addon.name, "Name from prefs");

    run_test_8();
  });
}

// Test that the prefs will override localized values from the manifest
function run_test_8() {
  dump("The rest of this test is disabled for now\n");
  do_test_finished();
  return;

  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "fr-FR");
  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);

    do_check_eq(addon.name, "Name from prefs");
    let contributors = addon.contributors;
    do_check_eq(contributors.length, 2);
    do_check_eq(contributors[0], "Contributor 1");
    do_check_eq(contributors[1], "Contributor 2");

    do_test_finished();
  });
}
