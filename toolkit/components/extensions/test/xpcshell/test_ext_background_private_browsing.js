/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {AddonManager} = ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
const {ExtensionPermissions} = ChromeUtils.import("resource://gre/modules/ExtensionPermissions.jsm");

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
AddonTestUtils.usePrivilegedSignatures = false;

add_task(async function test_background_incognito() {
  info("Test background page incognito value with permanent private browsing enabled");
  await AddonTestUtils.promiseStartupManager();

  Services.prefs.setBoolPref("extensions.allowPrivateBrowsingByDefault", false);
  Services.prefs.setBoolPref("browser.privatebrowsing.autostart", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.privatebrowsing.autostart");
    Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");
  });

  let extensionId = "@permTest";
  // We do not need to override incognito here, an extension installed during
  // permanent private browsing gets the permission during install.
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      applications: {
        gecko: {id: extensionId},
      },
    },
    async background() {
      browser.test.assertEq(window, browser.extension.getBackgroundPage(),
                            "Caller should be able to access itself as a background page");
      browser.test.assertEq(window, await browser.runtime.getBackgroundPage(),
                            "Caller should be able to access itself as a background page");

      browser.test.assertEq(browser.extension.inIncognitoContext, true,
                            "inIncognitoContext is true for permanent private browsing");

      browser.test.sendMessage("incognito");
    },
  });

  // Startup reason is ADDON_INSTALL
  await extension.startup();

  await extension.awaitMessage("incognito");

  let addon = await AddonManager.getAddonByID(extensionId);
  await addon.disable();

  // Permission remains when an extension is disabled.
  let perms = await ExtensionPermissions.get(extensionId);
  equal(perms.permissions.length, 1, "one permission");
  equal(perms.permissions[0], "internal:privateBrowsingAllowed", "internal permission present");

  // Startup reason is ADDON_ENABLE, the permission is
  // not granted in this case, but the extension should
  // still have permission.
  await addon.enable();
  await Promise.all([
    extension.awaitStartup(),
    extension.awaitMessage("incognito"),
  ]);

  // ExtensionPermissions should still have it.
  perms = await ExtensionPermissions.get(extensionId);
  equal(perms.permissions.length, 1, "one permission");
  equal(perms.permissions[0], "internal:privateBrowsingAllowed", "internal permission present");

  // This is the same as uninstall, no permissions after.
  await extension.unload();

  perms = await ExtensionPermissions.get(extensionId);
  equal(perms.permissions.length, 0, "no permission");

  await AddonTestUtils.promiseShutdownManager();
});
