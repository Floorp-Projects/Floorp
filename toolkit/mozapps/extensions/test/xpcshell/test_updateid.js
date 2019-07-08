/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that updating an add-on to a new ID does not work.

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

let testserver = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });

const ID = "updateid@tests.mozilla.org";

// Verify that an update to an add-on with a new ID fails
add_task(async function test_update_new_id() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  await promiseStartupManager();

  await promiseInstallWebExtension({
    manifest: {
      version: "1.0",
      applications: {
        gecko: {
          id: ID,
          update_url: "http://example.com/update.json",
        },
      },
    },
  });

  let xpi = await createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: { gecko: { id: "differentid@tests.mozilla.org" } },
    },
  });

  testserver.registerFile("/addon.xpi", xpi);
  AddonTestUtils.registerJSON(testserver, "/update.json", {
    addons: {
      [ID]: {
        updates: [
          {
            version: "2.0",
            update_link: "http://example.com/addon.xpi",
            applications: {
              gecko: {
                strict_min_version: "1",
                strict_max_version: "10",
              },
            },
          },
        ],
      },
    },
  });

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");

  let update = await promiseFindAddonUpdates(
    addon,
    AddonManager.UPDATE_WHEN_USER_REQUESTED
  );
  let install = update.updateAvailable;
  Assert.notEqual(install, null, "Found available update");
  Assert.equal(install.name, addon.name);
  Assert.equal(install.version, "2.0");
  Assert.equal(install.state, AddonManager.STATE_AVAILABLE);
  Assert.equal(install.existingAddon, addon);

  await Assert.rejects(
    install.install(),
    err => install.error == AddonManager.ERROR_INCORRECT_ID,
    "Upgrade to a different ID fails"
  );

  await addon.uninstall();
});
