/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const ID = "bug397778@tests.mozilla.org";

const ADDON = {
  id: "bug397778@tests.mozilla.org",
  version: "1.0",
  name: "Fallback Name",
  description: "Fallback Description",
  bootstrap: true,

  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"}],

  localized: [
    {
      locale: ["fr"],
      name: "fr Name",
      description: "fr Description",
    },
    {
      locale: ["de-DE"],
      name: "Deutsches W\u00f6rterbuch",
    },
    {
      locale: ["es-ES"],
      name: "es-ES Name",
      description: "es-ES Description",
    },
    {
      locale: ["zh-TW"],
      name: "zh-TW Name",
      description: "zh-TW Description",
    },
    {
      locale: ["zh-CN"],
      name: "zh-CN Name",
      description: "zh-CN Description",
    },
    {
      locale: ["en-GB"],
      name: "en-GB Name",
      description: "en-GB Description",
    },
    {
      locale: ["en"],
      name: "en Name",
      description: "en Description",
    },
    {
      locale: ["en-CA"],
      name: "en-CA Name",
      description: "en-CA Description",
    },
  ],
};

const XPI = createTempXPIFile(ADDON);

function run_test() {
  // Setup for test
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");
  Services.locale.setRequestedLocales(["fr-FR"]);

  // Install test add-on
  startupManager();
  installAllFiles([XPI], function() {
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

      executeSoon(run_test_2);
    });
  }));
}

function run_test_2() {
  // Change locale. The more specific de-DE is the best match
  Services.locale.setRequestedLocales(["de"]);
  restartManager();

  AddonManager.getAddonByID(ID, function(addon) {
    Assert.notEqual(addon, null);
    Assert.equal(addon.name, "Deutsches W\u00f6rterbuch");
    Assert.equal(addon.description, null);

    executeSoon(run_test_3);
  });
}

function run_test_3() {
  // Change locale. Locale case should have no effect
  Services.locale.setRequestedLocales(["DE-de"]);
  restartManager();

  AddonManager.getAddonByID(ID, function(addon) {
    Assert.notEqual(addon, null);
    Assert.equal(addon.name, "Deutsches W\u00f6rterbuch");
    Assert.equal(addon.description, null);

    executeSoon(run_test_4);
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

    executeSoon(run_test_5);
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

    executeSoon(run_test_6);
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

    executeSoon(do_test_finished);
  });
}
