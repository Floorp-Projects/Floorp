"use strict";

// Enable SCOPE_APPLICATION for builtin testing.  Default in tests is only SCOPE_PROFILE.
let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);
BootstrapMonitor.init();

// A test directory for default/builtin system addons.
const systemDefaults = FileUtils.getDir(
  "ProfD",
  ["app-system-defaults", "features"],
  true
);
registerDirectory("XREAppFeat", systemDefaults);

AddonTestUtils.usePrivilegedSignatures = id => "system";

const ADDON_ID = "updates@test";

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

let server = createHttpServer();

server.registerPathHandler("/upgrade.json", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "ok");
  response.write(
    JSON.stringify({
      addons: {
        [ADDON_ID]: {
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

async function promiseInstallDefaultSystemAddon(id, version) {
  let xpi = createWebExtensionFile(id, version);
  await AddonTestUtils.manuallyInstall(xpi, systemDefaults);
  return xpi;
}

async function promiseInstallProfileExtension(id, version, update_url) {
  return promiseInstallWebExtension({
    manifest: {
      version,
      applications: {
        gecko: { id, update_url },
      },
    },
  });
}

async function promiseInstallSystemProfileAddon(id, version) {
  let xpi = createWebExtensionFile(id, version);
  const install = await AddonManager.getInstallForURL(`file://${xpi.path}`, {
    useSystemLocation: true, // KEY_APP_SYSTEM_PROFILE
  });

  return install.install();
}

async function promiseUpdateSystemAddon(id, version, waitForStartup = true) {
  let xpi = createWebExtensionFile(id, version);
  let xml = buildSystemAddonUpdates([
    {
      id: ADDON_ID,
      version,
      path: xpi.leafName,
      xpi,
    },
  ]);
  // If we're not expecting a startup we need to wait for install to end.
  let promises = [];
  if (!waitForStartup) {
    promises.push(AddonTestUtils.promiseAddonEvent("onInstalled"));
  }
  promises.push(installSystemAddons(xml, waitForStartup ? [ADDON_ID] : []));
  return Promise.all(promises);
}

async function promiseClearSystemAddons() {
  let xml = buildSystemAddonUpdates([]);
  return installSystemAddons(xml, []);
}

const builtInOverride = { system: [ADDON_ID] };

async function checkAddon(version, reason, startReason = reason) {
  let addons = await AddonManager.getAddonsByTypes(["extension"]);
  Assert.equal(addons.length, 1, "one addon expected to be installed");
  Assert.equal(addons[0].version, version, `addon ${version} is installed`);
  Assert.ok(addons[0].isActive, `addon ${version} is active`);
  Assert.ok(!addons[0].disabled, `addon ${version} is enabled`);

  let installInfo = BootstrapMonitor.checkInstalled(ADDON_ID, version);
  equal(
    installInfo.reason,
    reason,
    `bootstrap monitor verified install reason for ${version}`
  );
  let started = BootstrapMonitor.checkStarted(ADDON_ID, version);
  equal(
    started.reason,
    startReason,
    `bootstrap monitor verified started reason for ${version}`
  );

  return addons[0];
}

/**
 * This test function starts after a 1.0 version of an addon is installed
 * either as a builtin ("app-builtin") or as a builtin system addon ("app-system-defaults").
 *
 * This tests the precedence chain works as expected through upgrades and
 * downgrades.
 *
 * Upgrade to a system addon in the profile location, "app-system-addons"
 * Upgrade to a temporary addon
 * Uninstalling the temporary addon downgrades to the system addon in "app-system-addons".
 * Upgrade to a system addon in the "app-system-profile" location.
 * Uninstalling the "app-system-profile" addon downgrades to the one in "app-system-addons".
 * Upgrade to a user-installed addon
 * Upgrade the addon in "app-system-addons", verify user-install is still active
 * Uninstall the addon in "app-system-addons", verify user-install is active
 * Test that the update_url upgrades the user-install and becomes active
 * Disable the user-install, verify the disabled addon retains precedence
 * Uninstall the disabled user-install, verify system addon in "app-system-defaults" is active and enabled
 * Upgrade the system addon again, then user-install a lower version, verify downgrade happens.
 * Uninstall user-install, verify upgrade happens when the system addon in "app-system-addons" is activated.
 */
async function _test_builtin_addon_override() {
  /////
  // Upgrade to a system addon in the profile location, "app-system-addons"
  /////
  await promiseUpdateSystemAddon(ADDON_ID, "2.0");
  await checkAddon("2.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  /////
  // Upgrade to a temporary addon
  /////
  let tmpAddon = createWebExtensionFile(ADDON_ID, "2.1");
  await Promise.all([
    AddonManager.installTemporaryAddon(tmpAddon),
    AddonTestUtils.promiseWebExtensionStartup(ADDON_ID),
  ]);
  let addon = await checkAddon("2.1", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  /////
  // Downgrade back to the system addon
  /////
  await addon.uninstall();
  await checkAddon("2.0", BOOTSTRAP_REASONS.ADDON_DOWNGRADE);

  /////
  // Install then uninstall an system profile addon
  /////
  info("Install an System Profile Addon, then uninstall it.");
  await Promise.all([
    promiseInstallSystemProfileAddon(ADDON_ID, "2.2"),
    AddonTestUtils.promiseWebExtensionStartup(ADDON_ID),
  ]);
  addon = await checkAddon("2.2", BOOTSTRAP_REASONS.ADDON_UPGRADE);
  await addon.uninstall();
  await checkAddon("2.0", BOOTSTRAP_REASONS.ADDON_DOWNGRADE);

  /////
  // Upgrade to a user-installed addon
  /////
  await Promise.all([
    promiseInstallProfileExtension(
      ADDON_ID,
      "3.0",
      `http://localhost:${server.identity.primaryPort}/upgrade.json`
    ),
    AddonTestUtils.promiseWebExtensionStartup(ADDON_ID),
  ]);
  await checkAddon("3.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  /////
  // Upgrade the system addon, verify user-install is still active
  /////
  await promiseUpdateSystemAddon(ADDON_ID, "2.2", false);
  await checkAddon("3.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  /////
  // Uninstall the system addon, verify user-install is active
  /////
  await Promise.all([
    promiseClearSystemAddons(),
    AddonTestUtils.promiseAddonEvent("onUninstalled"),
  ]);
  addon = await checkAddon("3.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  /////
  // Test that the update_url upgrades the user-install and becomes active
  /////
  let update = await promiseFindAddonUpdates(addon);
  await promiseCompleteAllInstalls([update.updateAvailable]);
  await AddonTestUtils.promiseWebExtensionStartup(ADDON_ID);
  addon = await checkAddon("4.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  /////
  // Disable the user-install, verify the disabled addon retains precedence
  /////
  await addon.disable();

  await AddonManager.getAddonByID(ADDON_ID);
  Assert.ok(!addon.isActive, "4.0 is disabled");
  Assert.equal(
    addon.version,
    "4.0",
    "version 4.0 is still the visible version"
  );

  /////
  // Uninstall the disabled user-install, verify system addon is active and enabled
  /////
  await Promise.all([
    addon.uninstall(),
    AddonTestUtils.promiseWebExtensionStartup(ADDON_ID),
  ]);
  // We've downgraded all the way to 1.0 from a user-installed addon
  addon = await checkAddon("1.0", BOOTSTRAP_REASONS.ADDON_DOWNGRADE);

  /////
  // Upgrade the system addon again, then user-install a lower version, verify downgrade happens.
  /////
  await promiseUpdateSystemAddon(ADDON_ID, "5.1");
  addon = await checkAddon("5.1", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  // user-install a lower version, downgrade happens
  await Promise.all([
    promiseInstallProfileExtension(ADDON_ID, "5.0"),
    AddonTestUtils.promiseWebExtensionStartup(ADDON_ID),
  ]);
  addon = await checkAddon("5.0", BOOTSTRAP_REASONS.ADDON_DOWNGRADE);

  /////
  // Uninstall user-install, verify upgrade happens when system addon is activated.
  /////
  await Promise.all([
    addon.uninstall(),
    AddonTestUtils.promiseWebExtensionStartup(ADDON_ID),
  ]);
  // the "system add-on upgrade" is now revealed
  addon = await checkAddon("5.1", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  await Promise.all([
    addon.uninstall(),
    AddonTestUtils.promiseWebExtensionStartup(ADDON_ID),
  ]);
  await checkAddon("1.0", BOOTSTRAP_REASONS.ADDON_DOWNGRADE);

  // Downgrading from an installed system addon to a default system
  // addon also requires the removal of the file on disk, and removing
  // the addon from the pref.
  Services.prefs.clearUserPref(PREF_SYSTEM_ADDON_SET);
}

add_task(async function test_system_addon_upgrades() {
  await AddonTestUtils.overrideBuiltIns(builtInOverride);
  await promiseInstallDefaultSystemAddon(ADDON_ID, "1.0");
  await AddonTestUtils.promiseStartupManager();
  await checkAddon("1.0", BOOTSTRAP_REASONS.ADDON_INSTALL);

  await _test_builtin_addon_override();

  // cleanup the system addon in the default location
  await AddonTestUtils.manuallyUninstall(systemDefaults, ADDON_ID);
  // If we don't restart (to clean up the uninstall above) and we
  // clear BootstrapMonitor, BootstrapMonitor asserts on the next AOM startup.
  await AddonTestUtils.promiseRestartManager();

  await AddonTestUtils.promiseShutdownManager();
  BootstrapMonitor.clear(ADDON_ID);
});

// Run the test again, but starting from a "builtin" addon location
add_task(async function test_builtin_addon_upgrades() {
  builtInOverride.system = [];

  await AddonTestUtils.promiseStartupManager();
  await Promise.all([
    installBuiltinExtension({
      manifest: {
        version: "1.0",
        applications: {
          gecko: { id: ADDON_ID },
        },
      },
    }),
    AddonTestUtils.promiseWebExtensionStartup(ADDON_ID),
  ]);
  await checkAddon("1.0", BOOTSTRAP_REASONS.ADDON_INSTALL);

  await _test_builtin_addon_override();

  let addon = await AddonManager.getAddonByID(ADDON_ID);
  await addon.uninstall();
  await AddonTestUtils.promiseShutdownManager();
  BootstrapMonitor.clear(ADDON_ID);
});

add_task(async function test_system_addon_precedence() {
  builtInOverride.system = [ADDON_ID];
  await AddonTestUtils.overrideBuiltIns(builtInOverride);
  await promiseInstallDefaultSystemAddon(ADDON_ID, "1.0");
  await AddonTestUtils.promiseStartupManager();
  await checkAddon("1.0", BOOTSTRAP_REASONS.ADDON_INSTALL);

  /////
  // Upgrade to a system addon in the profile location, "app-system-addons"
  /////
  await promiseUpdateSystemAddon(ADDON_ID, "2.0");
  await checkAddon("2.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  /////
  // Builtin system addon is changed, it has precedence because when this
  // happens we remove all prior system addon upgrades.
  /////
  await AddonTestUtils.promiseShutdownManager();
  await AddonTestUtils.overrideBuiltIns(builtInOverride);
  await promiseInstallDefaultSystemAddon(ADDON_ID, "1.5");
  await AddonTestUtils.promiseStartupManager(2);
  await checkAddon(
    "1.5",
    BOOTSTRAP_REASONS.ADDON_DOWNGRADE,
    BOOTSTRAP_REASONS.APP_STARTUP
  );

  // cleanup the system addon in the default location
  await AddonTestUtils.manuallyUninstall(systemDefaults, ADDON_ID);
  await AddonTestUtils.promiseRestartManager();

  await AddonTestUtils.promiseShutdownManager();
  BootstrapMonitor.clear(ADDON_ID);
});

add_task(async function test_builtin_addon_version_precedence() {
  builtInOverride.system = [];

  await AddonTestUtils.promiseStartupManager();
  await Promise.all([
    installBuiltinExtension({
      manifest: {
        version: "1.0",
        applications: {
          gecko: { id: ADDON_ID },
        },
      },
    }),
    AddonTestUtils.promiseWebExtensionStartup(ADDON_ID),
  ]);
  await checkAddon("1.0", BOOTSTRAP_REASONS.ADDON_INSTALL);

  /////
  // Upgrade to a system addon in the profile location, "app-system-addons"
  /////
  await promiseUpdateSystemAddon(ADDON_ID, "2.0");
  await checkAddon("2.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  /////
  // Builtin addon is changed, the system addon should still have precedence.
  /////
  await Promise.all([
    installBuiltinExtension(
      {
        manifest: {
          version: "1.5",
          applications: {
            gecko: { id: ADDON_ID },
          },
        },
      },
      false
    ),
    AddonTestUtils.promiseAddonEvent("onInstalled"),
  ]);
  await checkAddon("2.0", BOOTSTRAP_REASONS.ADDON_UPGRADE);

  let addon = await AddonManager.getAddonByID(ADDON_ID);
  await addon.uninstall();
  await AddonTestUtils.promiseShutdownManager();
  BootstrapMonitor.clear(ADDON_ID);
});
