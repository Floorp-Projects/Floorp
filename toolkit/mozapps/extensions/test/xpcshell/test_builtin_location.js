"use strict";

/* globals browser */
let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);
Services.prefs.setBoolPref("extensions.webextensions.background-delayed-startup", false);

AddonTestUtils.usePrivilegedSignatures = false;
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

async function getWrapper(id, hidden) {
  let xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: {gecko: {id}},
      hidden,
    },
    background() {
      browser.test.sendMessage("started");
    },
  });

  // The built-in location requires a resource: URL that maps to a
  // jar: or file: URL.  This would typically be something bundled
  // into omni.ja but for testing we just use a temp file.
  let base = Services.io.newURI(`jar:file:${xpi.path}!/`);
  let resProto = Services.io.getProtocolHandler("resource")
                         .QueryInterface(Ci.nsIResProtocolHandler);
  resProto.setSubstitution("ext-test", base);

  let wrapper = ExtensionTestUtils.expectExtension(id);
  await AddonManager.installBuiltinAddon("resource://ext-test/");
  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");
  ok(true, "Extension was installed successfully in built-in location");

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
  ok(!addon.hidden, "Addon is not hidden");

  // After a restart, the extension should start up normally.
  await promiseRestartManager();
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
  ok(addon.hidden, "Addon is hidden");

  await wrapper.unload();
  await AddonTestUtils.promiseShutdownManager();
});
