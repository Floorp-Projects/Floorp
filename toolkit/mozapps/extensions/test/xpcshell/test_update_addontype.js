/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

let server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_update_theme_to_extension() {
  const THEME_ID = "theme@tests.mozilla.org";
  await promiseInstallWebExtension({
    manifest: {
      version: "1.0",
      theme: {},
      browser_specific_settings: {
        gecko: {
          id: THEME_ID,
          update_url: "http://example.com/update.json",
        },
      },
    },
  });

  let xpi = await createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      browser_specific_settings: { gecko: { id: THEME_ID } },
    },
  });

  server.registerFile("/addon.xpi", xpi);
  AddonTestUtils.registerJSON(server, "/update.json", {
    addons: {
      [THEME_ID]: {
        updates: [
          {
            version: "2.0",
            update_link: "http://example.com/addon.xpi",
          },
        ],
      },
    },
  });

  let addon = await promiseAddonByID(THEME_ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.type, "theme");
  Assert.equal(addon.version, "1.0");

  let update = await promiseFindAddonUpdates(
    addon,
    AddonManager.UPDATE_WHEN_USER_REQUESTED
  );
  let install = update.updateAvailable;
  Assert.notEqual(install, null, "Found available update");
  // Although the downloaded xpi is an "extension", install.type is "theme"
  // because install.type reflects the type of the add-on that is being updated.
  Assert.equal(install.type, "theme");
  Assert.equal(install.version, "2.0");
  Assert.equal(install.state, AddonManager.STATE_AVAILABLE);
  Assert.equal(install.existingAddon, addon);

  await Assert.rejects(
    install.install(),
    () => install.error == AddonManager.ERROR_UNEXPECTED_ADDON_TYPE,
    "Refusing to change addon type from theme to extension"
  );

  await addon.uninstall();
});
