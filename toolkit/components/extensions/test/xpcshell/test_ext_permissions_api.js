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
    "privacy",
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

add_task(async function test_geo_permissions() {
  async function background() {
    const permObj = { permissions: ["geolocation"] };
    browser.test.onMessage.addListener(async msg => {
      if (msg === "request") {
        await browser.permissions.request(permObj);
      } else if (msg === "remove") {
        await browser.permissions.remove(permObj);
      }
      let result = await browser.permissions.contains(permObj);
      browser.test.sendMessage("done", result);
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      applications: { gecko: { id: "geo-test@test" } },
      optional_permissions: ["geolocation"],
    },
    useAddonManager: "permanent",
  });
  await extension.startup();

  let policy = WebExtensionPolicy.getByID(extension.id);
  let principal = policy.extension.principal;
  equal(
    Services.perms.testPermissionFromPrincipal(principal, "geo"),
    Services.perms.UNKNOWN_ACTION,
    "geolocation not allowed on install"
  );

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request");
    ok(await extension.awaitMessage("done"), "permission granted");
    equal(
      Services.perms.testPermissionFromPrincipal(principal, "geo"),
      Services.perms.ALLOW_ACTION,
      "geolocation allowed after requested"
    );

    extension.sendMessage("remove");
    ok(!(await extension.awaitMessage("done")), "permission revoked");

    equal(
      Services.perms.testPermissionFromPrincipal(principal, "geo"),
      Services.perms.UNKNOWN_ACTION,
      "geolocation not allowed after removed"
    );

    // re-grant to test update removal
    extension.sendMessage("request");
    ok(await extension.awaitMessage("done"), "permission granted");
    equal(
      Services.perms.testPermissionFromPrincipal(principal, "geo"),
      Services.perms.ALLOW_ACTION,
      "geolocation allowed after re-requested"
    );
  });

  // We should not have geo permission after this upgrade.
  await extension.upgrade({
    manifest: {
      applications: { gecko: { id: "geo-test@test" } },
    },
    useAddonManager: "permanent",
  });

  equal(
    Services.perms.testPermissionFromPrincipal(principal, "geo"),
    Services.perms.UNKNOWN_ACTION,
    "geolocation not allowed after upgrade"
  );

  await extension.unload();
});

add_task(async function test_browserSetting_permissions() {
  async function background() {
    const permObj = { permissions: ["browserSettings"] };
    browser.test.onMessage.addListener(async msg => {
      if (msg === "request") {
        await browser.permissions.request(permObj);
        await browser.browserSettings.cacheEnabled.set({ value: false });
      } else if (msg === "remove") {
        await browser.permissions.remove(permObj);
      }
      browser.test.sendMessage("done");
    });
  }

  function cacheIsEnabled() {
    return (
      Services.prefs.getBoolPref("browser.cache.disk.enable") &&
      Services.prefs.getBoolPref("browser.cache.memory.enable")
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      optional_permissions: ["browserSettings"],
    },
    useAddonManager: "permanent",
  });
  await extension.startup();
  ok(cacheIsEnabled(), "setting is not set after startup");

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request");
    await extension.awaitMessage("done");
    ok(!cacheIsEnabled(), "setting was set after request");

    extension.sendMessage("remove");
    await extension.awaitMessage("done");
    ok(cacheIsEnabled(), "setting is reset after remove");
  });
  await extension.unload();
});

add_task(async function test_privacy_permissions() {
  async function background() {
    const permObj = { permissions: ["privacy"] };
    browser.test.onMessage.addListener(async msg => {
      if (msg === "request") {
        await browser.permissions.request(permObj);
        await browser.privacy.websites.trackingProtectionMode.set({
          value: "always",
        });
      } else if (msg === "remove") {
        await browser.permissions.remove(permObj);
      }
      browser.test.sendMessage("done");
    });
  }

  function hasSetting() {
    return Services.prefs.getBoolPref("privacy.trackingprotection.enabled");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      optional_permissions: ["privacy"],
    },
    useAddonManager: "permanent",
  });
  await extension.startup();
  ok(!hasSetting(), "setting is not set after startup");

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request");
    await extension.awaitMessage("done");
    ok(hasSetting(), "setting was set after request");

    extension.sendMessage("remove");
    await extension.awaitMessage("done");
    ok(!hasSetting(), "setting is reset after remove");
  });
  await extension.unload();
});
