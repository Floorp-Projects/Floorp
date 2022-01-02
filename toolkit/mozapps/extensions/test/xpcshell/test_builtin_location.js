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

async function getWrapper(id, hidden) {
  let wrapper = await installBuiltinExtension({
    manifest: {
      applications: { gecko: { id } },
      hidden,
    },
    background() {
      browser.test.sendMessage("started");
    },
  });
  await wrapper.awaitMessage("started");
  return wrapper;
}

// Tests installing an extension from the built-in location.
add_task(async function test_builtin_location() {
  let id = "builtin@tests.mozilla.org";
  await AddonTestUtils.promiseStartupManager();
  let wrapper = await getWrapper(id);

  let addon = await promiseAddonByID(id);
  notEqual(addon, null, "Addon is installed");
  ok(addon.isActive, "Addon is active");
  ok(addon.isPrivileged, "Addon is privileged");
  ok(wrapper.extension.isAppProvided, "Addon is app provided");
  ok(!addon.hidden, "Addon is not hidden");

  // Built-in extensions are not checked against the blocklist,
  // so we shouldn't have loaded it.
  ok(!Services.blocklist.isLoaded, "Blocklist hasn't been loaded");

  // After a restart, the extension should start up normally.
  await promiseRestartManager();
  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");
  ok(true, "Extension in built-in location ran after restart");

  addon = await promiseAddonByID(id);
  notEqual(addon, null, "Addon is installed");
  ok(addon.isActive, "Addon is active");

  // After a restart that causes a database rebuild, it should still work
  await promiseRestartManager("2");
  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");
  ok(true, "Extension in built-in location ran after restart");

  addon = await promiseAddonByID(id);
  notEqual(addon, null, "Addon is installed");
  ok(addon.isActive, "Addon is active");

  // After a restart that changes the schema version, it should still work
  await promiseShutdownManager();
  Services.prefs.setIntPref("extensions.databaseSchema", 0);
  await promiseStartupManager();

  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");
  ok(true, "Extension in built-in location ran after restart");

  addon = await promiseAddonByID(id);
  notEqual(addon, null, "Addon is installed");
  ok(addon.isActive, "Addon is active");

  await wrapper.unload();

  addon = await promiseAddonByID(id);
  equal(addon, null, "Addon is gone after uninstall");
  await AddonTestUtils.promiseShutdownManager();
});

// Tests installing a hidden extension from the built-in location.
add_task(async function test_builtin_location_hidden() {
  let id = "hidden@tests.mozilla.org";
  await AddonTestUtils.promiseStartupManager();
  let wrapper = await getWrapper(id, true);

  let addon = await promiseAddonByID(id);
  notEqual(addon, null, "Addon is installed");
  ok(addon.isActive, "Addon is active");
  ok(addon.isPrivileged, "Addon is privileged");
  ok(wrapper.extension.isAppProvided, "Addon is app provided");
  ok(addon.hidden, "Addon is hidden");

  await wrapper.unload();
  await AddonTestUtils.promiseShutdownManager();
});

// Tests updates to builtin extensions
add_task(async function test_builtin_update() {
  let id = "bleah@tests.mozilla.org";
  await AddonTestUtils.promiseStartupManager();

  let wrapper = await installBuiltinExtension({
    manifest: {
      applications: { gecko: { id } },
      version: "1.0",
    },
    background() {
      browser.test.sendMessage("started");
    },
  });
  await wrapper.awaitMessage("started");

  await AddonTestUtils.promiseShutdownManager();

  // Change the built-in
  await setupBuiltinExtension({
    manifest: {
      applications: { gecko: { id } },
      version: "2.0",
    },
    background() {
      browser.test.sendMessage("started");
    },
  });

  let updateReason;
  AddonTestUtils.on("bootstrap-method", (method, params, reason) => {
    updateReason = reason;
  });

  // Re-start, with a new app version
  await AddonTestUtils.promiseStartupManager("3");

  await wrapper.awaitMessage("started");

  equal(
    updateReason,
    BOOTSTRAP_REASONS.ADODN_UPGRADE,
    "Builtin addon's bootstrap update() method was called at startup"
  );

  await wrapper.unload();
  await AddonTestUtils.promiseShutdownManager();
});
