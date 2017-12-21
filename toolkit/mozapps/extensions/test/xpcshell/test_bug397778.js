/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const ADDON = "test_bug397778";
const ID = "bug397778@tests.mozilla.org";

function run_test() {
  // Setup for test
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");
  Services.locale.setRequestedLocales(["fr-FR"]);

  // Install test add-on
  startupManager();
  installAllFiles([do_get_addon(ADDON)], function() {
    restartManager();

    run_test_1();
  });
}

function run_test_1() {
  AddonManager.getAddonByID(ID, callback_soon(function(addon) {
    Assert.notEqual(addon, null);
    Assert.equal(addon.name, "fr Name");
    Assert.equal(addon.description, "fr Description");

    // Disable item
    addon.userDisabled = true;
    restartManager();

    AddonManager.getAddonByID(ID, function(newAddon) {
      Assert.notEqual(newAddon, null);
      Assert.equal(newAddon.name, "fr Name");

      do_execute_soon(run_test_2);
    });
  }));
}

function run_test_2() {
  // Change locale. The more specific de-DE is the best match
  Services.locale.setRequestedLocales(["de"]);
  restartManager();

  AddonManager.getAddonByID(ID, function(addon) {
    Assert.notEqual(addon, null);
    Assert.equal(addon.name, "de-DE Name");
    Assert.equal(addon.description, null);

    do_execute_soon(run_test_3);
  });
}

function run_test_3() {
  // Change locale. Locale case should have no effect
  Services.locale.setRequestedLocales(["DE-de"]);
  restartManager();

  AddonManager.getAddonByID(ID, function(addon) {
    Assert.notEqual(addon, null);
    Assert.equal(addon.name, "de-DE Name");
    Assert.equal(addon.description, null);

    do_execute_soon(run_test_4);
  });
}

function run_test_4() {
  // Change locale. es-ES should closely match
  Services.locale.setRequestedLocales(["es-AR"]);
  restartManager();

  AddonManager.getAddonByID(ID, function(addon) {
    Assert.notEqual(addon, null);
    Assert.equal(addon.name, "es-ES Name");
    Assert.equal(addon.description, "es-ES Description");

    do_execute_soon(run_test_5);
  });
}

function run_test_5() {
  // Change locale. Either zh-CN or zh-TW could match
  Services.locale.setRequestedLocales(["zh"]);
  restartManager();

  AddonManager.getAddonByID(ID, function(addon) {
    Assert.notEqual(addon, null);
    if (addon.name != "zh-TW Name" && addon.name != "zh-CN Name")
      do_throw("zh matched to " + addon.name);

    do_execute_soon(run_test_6);
  });
}

function run_test_6() {
  // Unknown locale should try to match against en-US as well. Of en,en-GB
  // en should match as being less specific
  Services.locale.setRequestedLocales(["nl-NL"]);
  restartManager();

  AddonManager.getAddonByID(ID, function(addon) {
    Assert.notEqual(addon, null);
    Assert.equal(addon.name, "en Name");
    Assert.equal(addon.description, "en Description");

    do_execute_soon(do_test_finished);
  });
}
