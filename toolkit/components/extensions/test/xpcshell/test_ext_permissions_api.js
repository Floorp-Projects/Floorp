"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_task(async function setup() {
  Services.prefs.setBoolPref(
    "extensions.webextOptionalPermissionPrompts",
    false
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("extensions.webextOptionalPermissionPrompts");
  });
  await AddonTestUtils.promiseStartupManager();
  AddonTestUtils.usePrivilegedSignatures = false;
});

add_task(async function test_api_on_permissions_changed() {
  async function background() {
    browser.test.onMessage.addListener(async opts => {
      for (let perm of opts.optional_permissions) {
        let permObj = { permissions: [perm] };
        browser.test.assertFalse(
          browser[perm],
          `${perm} API is not available at start`
        );
        await browser.permissions.request(permObj);
        browser.test.assertTrue(
          browser[perm],
          `${perm} API is available after permission request`
        );
        await browser.permissions.remove(permObj);
        browser.test.assertFalse(
          browser[perm],
          `${perm} API is not available after permission remove`
        );
      }
      browser.test.notifyPass("done");
    });
  }

  let optional_permissions = [
    "downloads",
    "cookies",
    "webRequest",
    "webNavigation",
    "browserSettings",
    "idle",
    "notifications",
  ];

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      optional_permissions,
    },
    useAddonManager: "permanent",
  });
  await extension.startup();

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage({ optional_permissions });
    await extension.awaitFinish("done");
  });
  await extension.unload();
});
