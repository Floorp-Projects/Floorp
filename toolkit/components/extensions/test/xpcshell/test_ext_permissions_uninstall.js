"use strict";

const {ExtensionPermissions} = ChromeUtils.import("resource://gre/modules/ExtensionPermissions.jsm");

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

Services.prefs.setBoolPref("extensions.webextensions.background-delayed-startup", false);

const observer = {
  observe(subject, topic, data) {
    if (topic == "webextension-optional-permission-prompt") {
      let {resolve} = subject.wrappedJSObject;
      resolve(true);
    }
  },
};

add_task(async function setup() {
  Services.prefs.setBoolPref("extensions.webextOptionalPermissionPrompts", true);
  Services.obs.addObserver(observer, "webextension-optional-permission-prompt");
  await AddonTestUtils.promiseStartupManager();
  registerCleanupFunction(async () => {
    await AddonTestUtils.promiseShutdownManager();
    Services.obs.removeObserver(observer, "webextension-optional-permission-prompt");
    Services.prefs.clearUserPref("extensions.webextOptionalPermissionPrompts");
  });
});

// This test must run before any restart of the addonmanager so the
// uninstallObserver works.
add_task(async function test_permissions_removed() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "optional_permissions": ["idle"],
    },
    background() {
      browser.test.onMessage.addListener(async (msg, arg) => {
        if (msg == "request") {
          try {
            let result = await browser.permissions.request(arg);
            browser.test.sendMessage("request.result", result);
          } catch (err) {
            browser.test.sendMessage("request.result", err.message);
          }
        }
      });
    },
    useAddonManager: "temporary",
  });

  await extension.startup();

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request", {permissions: ["idle"], origins: []});
    let result = await extension.awaitMessage("request.result");
    equal(result, true, "request() for optional permissions succeeded");
  });

  let id = extension.id;
  let perms = await ExtensionPermissions.get(id);
  equal(perms.permissions.length, 1, "optional permission added");

  await extension.unload();

  perms = await ExtensionPermissions.get(id);
  equal(perms.permissions.length, 0, "no permissions after uninstall");
  equal(perms.origins.length, 0, "no origin permissions after uninstall");
});
