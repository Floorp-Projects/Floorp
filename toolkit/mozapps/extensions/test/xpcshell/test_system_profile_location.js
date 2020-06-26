"use strict";

/* globals browser */
let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);
Services.prefs.setBoolPref(
  "extensions.webextensions.background-delayed-startup",
  false
);

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "1"
);

AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("system");

async function promiseInstallSystemProfileExtension(id, hidden) {
  let xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id } },
      hidden,
    },
    background() {
      browser.test.sendMessage("started");
    },
  });
  let wrapper = ExtensionTestUtils.expectExtension(id);

  const install = await AddonManager.getInstallForURL(`file://${xpi.path}`, {
    useSystemLocation: true, // KEY_APP_SYSTEM_PROFILE
  });

  install.install();

  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");
  return wrapper;
}

async function promiseUninstall(id) {
  await AddonManager.uninstallSystemProfileAddon(id);

  let addon = await promiseAddonByID(id);
  equal(addon, null, "Addon is gone after uninstall");
}

// Tests installing an extension into the app-system-profile location.
add_task(async function test_system_profile_location() {
  let id = "system@tests.mozilla.org";
  await AddonTestUtils.promiseStartupManager();
  let wrapper = await promiseInstallSystemProfileExtension(id);

  let addon = await promiseAddonByID(id);
  notEqual(addon, null, "Addon is installed");
  ok(addon.isActive, "Addon is active");
  ok(addon.isPrivileged, "Addon is privileged");
  ok(wrapper.extension.isAppProvided, "Addon is app provided");
  ok(!addon.hidden, "Addon is not hidden");
  equal(
    addon.signedState,
    AddonManager.SIGNEDSTATE_PRIVILEGED,
    "Addon is system signed"
  );

  // After a restart, the extension should start up normally.
  await promiseRestartManager();
  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");
  ok(true, "Extension in app-system-profile location ran after restart");

  addon = await promiseAddonByID(id);
  notEqual(addon, null, "Addon is installed");
  ok(addon.isActive, "Addon is active");

  // After a restart that causes a database rebuild, it should still work
  await promiseRestartManager("2");
  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");
  ok(true, "Extension in app-system-profile location ran after restart");

  addon = await promiseAddonByID(id);
  notEqual(addon, null, "Addon is installed");
  ok(addon.isActive, "Addon is active");

  // After a restart that changes the schema version, it should still work
  await promiseShutdownManager();
  Services.prefs.setIntPref("extensions.databaseSchema", 0);
  await promiseStartupManager();

  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");
  ok(true, "Extension in app-system-profile location ran after restart");

  addon = await promiseAddonByID(id);
  notEqual(addon, null, "Addon is installed");
  ok(addon.isActive, "Addon is active");

  await promiseUninstall(id);

  await AddonTestUtils.promiseShutdownManager();
});

// Tests installing a hidden extension in the app-system-profile location.
add_task(async function test_system_profile_location_hidden() {
  let id = "system-hidden@tests.mozilla.org";
  await AddonTestUtils.promiseStartupManager();
  await promiseInstallSystemProfileExtension(id, true);

  let addon = await promiseAddonByID(id);
  notEqual(addon, null, "Addon is installed");
  ok(addon.isActive, "Addon is active");
  ok(addon.isPrivileged, "Addon is privileged");
  ok(addon.hidden, "Addon is hidden");

  await promiseUninstall(id);

  await AddonTestUtils.promiseShutdownManager();
});

add_task(async function test_system_profile_location_installFile() {
  await AddonTestUtils.promiseStartupManager();
  let id = "system-fileinstall@test";
  let xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id } },
    },
    background() {
      browser.test.sendMessage("started");
    },
  });
  let wrapper = ExtensionTestUtils.expectExtension(id);

  const install = await AddonManager.getInstallForFile(xpi, null, null, true);
  install.install();

  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");

  await promiseUninstall(id);

  await AddonTestUtils.promiseShutdownManager();
});

add_task(async function test_system_profile_location_overridden() {
  await AddonTestUtils.promiseStartupManager();
  let id = "system-fileinstall@test";
  let xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      applications: { gecko: { id } },
    },
  });

  let install = await AddonManager.getInstallForFile(xpi, null, null, true);
  await install.install();

  let addon = await promiseAddonByID(id);
  equal(addon.version, "1.0", "Addon is installed");

  // Install a user profile version on top of the system profile location.
  xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: { gecko: { id } },
    },
  });

  install = await AddonManager.getInstallForFile(xpi);
  await install.install();

  addon = await promiseAddonByID(id);
  equal(addon.version, "2.0", "Addon is upgraded");

  // Uninstall the system profile addon.
  await AddonManager.uninstallSystemProfileAddon(id);

  addon = await promiseAddonByID(id);
  equal(addon.version, "2.0", "Addon is still active");

  await addon.uninstall();
  await AddonTestUtils.promiseShutdownManager();
});

add_task(async function test_system_profile_location_require_system_cert() {
  await AddonTestUtils.promiseStartupManager();
  let id = "fail@test";
  let xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id } },
    },
  });
  const install = await AddonManager.getInstallForURL(`file://${xpi.path}`, {
    useSystemLocation: true, // KEY_APP_SYSTEM_PROFILE
  });

  await install.install();

  let addon = await promiseAddonByID(id);
  ok(!addon.isPrivileged, "Addon is not privileged");
  ok(!addon.isActive, "Addon is not active");
  equal(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED, "Addon is signed");

  await addon.uninstall();
  await AddonTestUtils.promiseShutdownManager();
});
