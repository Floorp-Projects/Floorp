"use strict";

/* globals browser */

// Tests installing an extension from the built-in location.
add_task(async function test_builtin_location() {
  let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_SYSTEM;
  Services.prefs.setIntPref("extensions.enabledScopes", scopes);
  Services.prefs.setBoolPref("extensions.webextensions.background-delayed-startup", false);

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  await promiseStartupManager();

  const ID = "builtin@tests.mozilla.org";
  let xpi = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: {gecko: {id: ID}},
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

  let wrapper = ExtensionTestUtils.expectExtension(ID);

  await AddonManager.installBuiltinAddon("resource://ext-test/");

  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");
  ok(true, "Extension was installed successfully in built-in location");

  let addon = await promiseAddonByID(ID);
  notEqual(addon, null, "Addon is installed");
  equal(addon.isActive, true, "Addon is active");

  // After a restart, the extension should start up normally.
  await promiseRestartManager();
  await wrapper.awaitStartup();
  await wrapper.awaitMessage("started");
  ok(true, "Extension in built-in location ran after restart");

  addon = await promiseAddonByID(ID);
  notEqual(addon, null, "Addon is installed");
  equal(addon.isActive, true, "Addon is active");

  await wrapper.unload();

  addon = await promiseAddonByID(ID);
  equal(addon, null, "Addon is gone after uninstall");

  await promiseShutdownManager();
});
