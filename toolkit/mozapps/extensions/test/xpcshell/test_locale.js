/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ADDONS = {
  test_locale: {
    "install.rdf": {
      "id": "addon1@tests.mozilla.org",
      "name": "Fallback Name",
      "description": "Fallback Description",
      "localized": [
        {
          "name": "fr-FR Name",
          "description": "fr-FR Description",
          "locale": [
            "fr-FR",
            ""
          ],
          "contributor": [
            "Fr Contributor 1",
            "Fr Contributor 2",
            "Fr Contributor 3"
          ]
        },
        {
          "name": "de-DE Name",
          "locale": [
            "de-DE"
          ]
        },
        {
          "name": "es-ES Name",
          "description": "es-ES Description",
          "locale": [
            "es-ES"
          ]
        },
        {
          "name": "Repeated locale",
          "locale": [
            "fr-FR"
          ]
        },
        {
          "name": "Missing locale"
        }
      ]
    }
  },
};

add_task(async function setup() {
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  await promiseStartupManager();
});

// Tests that the localized properties are visible before installation
add_task(async function test_1() {
  await restartWithLocales(["fr-FR"]);

  let xpi = AddonTestUtils.createTempXPIFile(ADDONS.test_locale);
  let install = await AddonManager.getInstallForFile(xpi);
  Assert.equal(install.addon.name, "fr-FR Name");
  Assert.equal(install.addon.description, "fr-FR Description");

  await new Promise(resolve => {
    prepare_test({
      "addon1@tests.mozilla.org": [
        ["onInstalling", false],
        ["onInstalled", false],
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], resolve);
    install.install();
  });
});

// Tests that the localized properties are visible after installation
add_task(async function test_2() {
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(addon, null);

  Assert.equal(addon.name, "fr-FR Name");
  Assert.equal(addon.description, "fr-FR Description");

  await addon.disable();
});

// Test that the localized properties are still there when disabled.
add_task(async function test_3() {
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(addon, null);
  Assert.equal(addon.name, "fr-FR Name");
});

add_task(async function test_4() {
  await promiseRestartManager();

  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(addon, null);
  Assert.equal(addon.name, "fr-FR Name");
  let contributors = addon.contributors;
  Assert.equal(contributors.length, 3);
  Assert.equal(contributors[0], "Fr Contributor 1");
  Assert.equal(contributors[1], "Fr Contributor 2");
  Assert.equal(contributors[2], "Fr Contributor 3");
});

// Test that changing locale works
add_task(async function test_5() {
  await restartWithLocales(["de-DE"]);

  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(addon, null);

  Assert.equal(addon.name, "de-DE Name");
  Assert.equal(addon.description, null);
});

// Test that missing locales use the fallbacks
add_task(async function test_6() {
  await restartWithLocales(["nl-NL"]);

  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(addon, null);

  Assert.equal(addon.name, "Fallback Name");
  Assert.equal(addon.description, "Fallback Description");

  await addon.enable();
});
