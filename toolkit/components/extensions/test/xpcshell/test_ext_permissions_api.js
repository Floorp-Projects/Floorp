"use strict";

Services.prefs.setBoolPref(
  "extensions.webextensions.background-delayed-startup",
  false
);

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

let OptionalPermissions;

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

  // We want to get a list of optional permissions prior to loading an extension,
  // so we'll get ExtensionParent to do that for us.
  await ExtensionParent.apiManager.lazyInit();

  // These permissions have special behaviors and/or are not mapped directly to an
  // api namespace.  They will have their own tests for specific behavior.
  let ignore = [
    "activeTab",
    "clipboardRead",
    "clipboardWrite",
    "downloads.open",
    "geolocation",
    "management",
    "menus.overrideContext",
    "search",
    "tabHide",
    "tabs",
    "webRequestBlocking",
  ];
  OptionalPermissions = Schemas.getPermissionNames([
    "OptionalPermission",
    "OptionalPermissionNoPrompt",
  ]).filter(n => !ignore.includes(n));
});

add_task(async function test_api_on_permissions_changed() {
  async function background() {
    let manifest = browser.runtime.getManifest();
    let permObj = { permissions: manifest.optional_permissions, origins: [] };

    function verifyPermissions(enabled) {
      for (let perm of manifest.optional_permissions) {
        browser.test.assertEq(
          enabled,
          !!browser[perm],
          `${perm} API is ${
            enabled ? "injected" : "removed"
          } after permission request`
        );
      }
    }

    browser.permissions.onAdded.addListener(details => {
      browser.test.assertEq(
        JSON.stringify(details.permissions),
        JSON.stringify(manifest.optional_permissions),
        "expected permissions added"
      );
      verifyPermissions(true);
      browser.test.sendMessage("added");
    });

    browser.permissions.onRemoved.addListener(details => {
      browser.test.assertEq(
        JSON.stringify(details.permissions),
        JSON.stringify(manifest.optional_permissions),
        "expected permissions removed"
      );
      verifyPermissions(false);
      browser.test.sendMessage("removed");
    });

    browser.test.onMessage.addListener((msg, enabled) => {
      if (msg === "request") {
        browser.permissions.request(permObj);
      } else if (msg === "verify_access") {
        verifyPermissions(enabled);
        browser.test.sendMessage("verified");
      } else if (msg === "revoke") {
        browser.permissions.remove(permObj);
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      optional_permissions: OptionalPermissions,
    },
    useAddonManager: "permanent",
  });
  await extension.startup();

  function addPermissions() {
    extension.sendMessage("request");
    return extension.awaitMessage("added");
  }

  function removePermissions() {
    extension.sendMessage("revoke");
    return extension.awaitMessage("removed");
  }

  function verifyPermissions(enabled) {
    extension.sendMessage("verify_access", enabled);
    return extension.awaitMessage("verified");
  }

  await withHandlingUserInput(extension, async () => {
    await addPermissions();
    await removePermissions();
    await addPermissions();
  });

  // reset handlingUserInput for the restart
  extensionHandlers.delete(extension);

  // Verify access on restart
  await AddonTestUtils.promiseRestartManager();
  await extension.awaitStartup();
  await verifyPermissions(true);

  await withHandlingUserInput(extension, async () => {
    await removePermissions();
  });

  // Add private browsing to be sure it doesn't come through.
  let permObj = {
    permissions: OptionalPermissions.concat("internal:privateBrowsingAllowed"),
    origins: [],
  };

  // enable the permissions while the addon is running
  await ExtensionPermissions.add(extension.id, permObj, extension.extension);
  await extension.awaitMessage("added");
  await verifyPermissions(true);

  // disable the permissions while the addon is running
  await ExtensionPermissions.remove(extension.id, permObj, extension.extension);
  await extension.awaitMessage("removed");
  await verifyPermissions(false);

  // Add private browsing to test internal permission.  If it slips through,
  // we would get an error for an additional added message.
  await ExtensionPermissions.add(
    extension.id,
    { permissions: ["internal:privateBrowsingAllowed"], origins: [] },
    extension.extension
  );

  // disable the addon and re-test revoking permissions.
  await withHandlingUserInput(extension, async () => {
    await addPermissions();
  });
  let addon = await AddonManager.getAddonByID(extension.id);
  await addon.disable();
  await ExtensionPermissions.remove(extension.id, permObj);
  await addon.enable();
  await extension.awaitStartup();

  await verifyPermissions(false);
  let perms = await ExtensionPermissions.get(extension.id);
  equal(perms.permissions.length, 0, "no permissions on startup");

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

    extension.sendMessage("request");
    await extension.awaitMessage("done");
    ok(!cacheIsEnabled(), "setting was set after request");
  });

  await ExtensionPermissions._uninit();
  extensionHandlers.delete(extension);
  await AddonTestUtils.promiseRestartManager();
  await extension.awaitStartup();

  await withHandlingUserInput(extension, async () => {
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

    extension.sendMessage("request");
    await extension.awaitMessage("done");
    ok(hasSetting(), "setting was set after request");
  });

  await ExtensionPermissions._uninit();
  extensionHandlers.delete(extension);
  await AddonTestUtils.promiseRestartManager();
  await extension.awaitStartup();

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("remove");
    await extension.awaitMessage("done");
    ok(!hasSetting(), "setting is reset after remove");
  });

  await extension.unload();
});
