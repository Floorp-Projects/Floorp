"use strict";

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

// The location for profile installed system addons aka. "app-system-addons".
const systemAddons = FileUtils.getDir("ProfD", ["features", "test"], true);

// A test directory for default/builtin system addons.
const systemDefaults = FileUtils.getDir(
  "ProfD",
  ["app-system-defaults", "features"],
  true
);
registerDirectory("XREAppFeat", systemDefaults);

AddonTestUtils.usePrivilegedSignatures = id => "system";

const PREF_UPDATE_SECURITY = "extensions.checkUpdateSecurity";
const PREF_SYSTEM_ADDON_SET = "extensions.systemAddonSet";
const ADDON_ID = "updates@test";

// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_UPDATE_SECURITY, false);

let server = createHttpServer();

server.registerPathHandler("/upgrade.json", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "ok");
  response.write(
    JSON.stringify({
      addons: {
        "updates@test": {
          updates: [
            {
              version: "4.0",
              update_link: `http://localhost:${server.identity.primaryPort}/${ADDON_ID}.xpi`,
            },
          ],
        },
      },
    })
  );
});

function createWebExtensionFile(id, version, update_url) {
  return AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version,
      applications: {
        gecko: { id, update_url },
      },
    },
  });
}

let xpiUpdate = createWebExtensionFile(ADDON_ID, "4.0");

server.registerFile(`/${ADDON_ID}.xpi`, xpiUpdate);

async function createWebExtension(dir, id, version, update_url) {
  let xpi = createWebExtensionFile(id, version, update_url);
  await AddonTestUtils.manuallyInstall(xpi, dir);
}

async function promiseInstallWebExtension(id, version, update_url) {
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version,
      applications: {
        gecko: { id, update_url },
      },
    },
  });

  let { addon } = await promiseInstallFile(xpi);
  return addon;
}

const builtInOverride = { system: [ADDON_ID] };

add_task(async function test_system_addon_override() {
  await AddonTestUtils.overrideBuiltIns(builtInOverride);
  await createWebExtension(systemDefaults, ADDON_ID, "1.0");

  await AddonTestUtils.promiseStartupManager();

  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 1, "one addon expected to be installed");
  Assert.equal(addons[0].version, "1.0", "version 1.0 is installed");

  await AddonTestUtils.promiseShutdownManager();

  Services.prefs.setCharPref(
    PREF_SYSTEM_ADDON_SET,
    JSON.stringify({
      schema: 1,
      directory: systemAddons.leafName,
      addons: {
        "updates@test": {
          version: "2.0",
        },
      },
    })
  );
  await createWebExtension(systemAddons, ADDON_ID, "2.0");

  await AddonTestUtils.promiseStartupManager();

  addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 1, "one addon expected to be installed");
  Assert.equal(addons[0].version, "2.0", "version 2.0 is installed");

  // Install then uninstall a temporary addon
  let tmpAddon = createWebExtensionFile(ADDON_ID, "2.1");
  await AddonManager.installTemporaryAddon(tmpAddon);
  addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 1, "one addon expected to be installed");
  Assert.equal(addons[0].version, "2.1", "version 2.1 is installed");

  // Uninstall temporary
  await addons[0].uninstall();
  addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 1, "one addon expected to be installed");
  Assert.equal(addons[0].version, "2.0", "version 2.0 is installed");

  // Test a user-installed addon that will update.
  let addon = await promiseInstallWebExtension(
    ADDON_ID,
    "3.0",
    `http://localhost:${server.identity.primaryPort}/upgrade.json`
  );
  addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 1, "one addon expected to be installed");
  Assert.equal(addons[0].version, "3.0", "version 3.0 is installed");

  // Test the update will work.
  let update = await promiseFindAddonUpdates(addon);
  let install = update.updateAvailable;
  await promiseCompleteAllInstalls([install]);
  addon = await promiseAddonByID(ADDON_ID);
  Assert.equal(addon.version, "4.0", "version 4.0 is installed");
  Assert.ok(addon.isActive, "4.0 is enabled");

  // disabling does not reveal underlying add-on, "top" stays disabled
  await addon.disable();
  Assert.ok(!addon.isActive, "4.0 is disabled");
  Assert.equal(
    addon.version,
    "4.0",
    "version 4.0 is still the visible version"
  );

  await addon.uninstall();

  // the "system add-on upgrade" is now revealed
  addon = await promiseAddonByID(ADDON_ID);
  Assert.ok(addon.isActive, "2.0 is active");
  Assert.equal(
    addon.version,
    "2.0",
    "version 2.0 is still the visible version"
  );

  await AddonTestUtils.promiseShutdownManager();
  await AddonTestUtils.manuallyUninstall(systemAddons, ADDON_ID);
  Services.prefs.clearUserPref(PREF_SYSTEM_ADDON_SET);

  await AddonTestUtils.overrideBuiltIns(builtInOverride);
  await AddonTestUtils.promiseStartupManager();

  addon = await promiseAddonByID(ADDON_ID);
  Assert.ok(addon.isActive, "1.0 is active");
  Assert.equal(
    addon.version,
    "1.0",
    "version 1.0 is still the visible version"
  );

  await AddonTestUtils.promiseShutdownManager();
});
