/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
add_task(async function setup() {
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  await promiseStartupManager();
});

// Tests that the localized properties are visible before installation
add_task(async function test_1() {
  await restartWithLocales(["fr-FR"]);

  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "__MSG_name__",
      description: "__MSG_description__",
      default_locale: "en",

      browser_specific_settings: {
        gecko: {
          id: "addon1@tests.mozilla.org",
        },
      },
    },

    files: {
      "_locales/en/messages.json": {
        name: {
          message: "Fallback Name",
          description: "name",
        },
        description: {
          message: "Fallback Description",
          description: "description",
        },
      },
      "_locales/fr_FR/messages.json": {
        name: {
          message: "fr-FR Name",
          description: "name",
        },
        description: {
          message: "fr-FR Description",
          description: "description",
        },
      },
      "_locales/de-DE/messages.json": {
        name: {
          message: "de-DE Name",
          description: "name",
        },
      },
      "_locales/es-ES/messages.json": {
        name: {
          // This length is 80, which exceeds ExtensionData.EXT_NAME_MAX_LEN:
          message: "123456789_".repeat(8),
          description: "name with 80 chars, should truncate to 75",
        },
      },
    },
  });

  let install = await AddonManager.getInstallForFile(xpi);
  Assert.equal(install.addon.name, "fr-FR Name");
  Assert.equal(install.addon.description, "fr-FR Description");
  await install.install();
});

// Tests that the localized properties are visible after installation
add_task(async function test_2() {
  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(addon, null);

  Assert.equal(addon.name, "fr-FR Name");
  Assert.equal(addon.description, "fr-FR Description");

  await addon.disable();
});

// Test that the localized properties are still there when disabled.
add_task(async function test_3() {
  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(addon, null);
  Assert.equal(addon.name, "fr-FR Name");
});

// Test that changing locale works
add_task(async function test_5() {
  await restartWithLocales(["de-DE"]);

  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(addon, null);

  Assert.equal(addon.name, "de-DE Name");
  Assert.equal(addon.description, "Fallback Description");
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

add_task(async function test_name_too_long() {
  await restartWithLocales(["es-ES"]);

  let addon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(addon, null);

  Assert.equal(
    addon.name,
    "123456789_123456789_123456789_123456789_123456789_123456789_123456789_12345",
    "Name should be truncated"
  );

  await addon.enable();
});
