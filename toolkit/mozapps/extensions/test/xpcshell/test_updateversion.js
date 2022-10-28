/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

let server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });

async function serverRegisterUpdate({ id, version, actualVersion }) {
  let xpi = await createTempWebExtensionFile({
    manifest: {
      version: actualVersion,
      browser_specific_settings: { gecko: { id } },
    },
  });

  server.registerFile("/addon.xpi", xpi);
  AddonTestUtils.registerJSON(server, "/update.json", {
    addons: {
      [id]: {
        updates: [{ version, update_link: "http://example.com/addon.xpi" }],
      },
    },
  });
}

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_update_version_mismatch() {
  const ID = "updateversion@tests.mozilla.org";
  await promiseInstallWebExtension({
    manifest: {
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: ID,
          update_url: "http://example.com/update.json",
        },
      },
    },
  });

  await serverRegisterUpdate({
    id: ID,
    version: "2.0",
    actualVersion: "2.0.0",
  });

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");

  let update = await promiseFindAddonUpdates(
    addon,
    AddonManager.UPDATE_WHEN_USER_REQUESTED
  );
  let install = update.updateAvailable;
  Assert.notEqual(install, false, "Found available update");
  Assert.equal(install.version, "2.0");
  Assert.equal(install.state, AddonManager.STATE_AVAILABLE);
  Assert.equal(install.existingAddon, addon);

  await Assert.rejects(
    install.install(),
    err => install.error == AddonManager.ERROR_UNEXPECTED_ADDON_VERSION,
    "Should refuse installation when downloaded version does not match"
  );

  await addon.uninstall();
});

add_task(async function test_update_version_empty() {
  const ID = "updateversionempty@tests.mozilla.org";
  await serverRegisterUpdate({ id: ID, version: "", actualVersion: "1.0" });

  await promiseInstallWebExtension({
    manifest: {
      version: "0",
      browser_specific_settings: {
        gecko: {
          id: ID,
          update_url: "http://example.com/update.json",
        },
      },
    },
  });
  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "0");
  let update = await promiseFindAddonUpdates(
    addon,
    AddonManager.UPDATE_WHEN_USER_REQUESTED
  );
  // The only item in the updates array has version "" (empty). This should not
  // be offered as an available update because it is certainly not newer.
  Assert.equal(update.updateAvailable, false, "No update found");
  await addon.uninstall();
});
