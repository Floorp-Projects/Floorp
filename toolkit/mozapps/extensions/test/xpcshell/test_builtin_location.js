"use strict";

/* globals browser */
let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "1"
);

async function getWrapper(id, hidden, { manifest } = {}) {
  let wrapper = await installBuiltinExtension({
    manifest: {
      ...manifest,
      browser_specific_settings: { gecko: { id } },
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
      browser_specific_settings: { gecko: { id } },
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
      browser_specific_settings: { gecko: { id } },
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

// Tests private_browsing policy extension settings doesn't apply
// to built-in extensions.
add_task(
  // This test is skipped on builds that don't support enterprise policies
  // (e.g. android builds).
  { skip_if: () => !Services.policies },
  async function test_builtin_private_browsing_policy_setting() {
    await AddonTestUtils.promiseStartupManager();

    const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
      "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
    );

    let id = "builtin-addon@tests.mozilla.org";
    let idNotAllowed = "builtin-not-allowed@tests.mozilla.org";

    await EnterprisePolicyTesting.setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          [id]: {
            private_browsing: false,
          },
          [idNotAllowed]: {
            private_browsing: true,
          },
        },
      },
    });

    // Sanity check the policy data has been loaded.
    Assert.equal(
      Services.policies.getExtensionSettings(id)?.private_browsing,
      false,
      `Got expected policies setting private_browsing for ${id}`
    );
    Assert.equal(
      Services.policies.getExtensionSettings(idNotAllowed)?.private_browsing,
      true,
      `Got expected policies setting private_browsing for ${idNotAllowed}`
    );

    async function assertPrivateBrowsingAllowed({
      addonId,
      manifest,
      expectedPrivateBrowsingAllowed,
    }) {
      let wrapper = await getWrapper(addonId, undefined, { manifest });
      let addon = await promiseAddonByID(addonId);
      notEqual(addon, null, `${addonId} is installed`);
      ok(addon.isActive, `${addonId} is active`);
      ok(addon.isPrivileged, `${addonId} is privileged`);
      ok(wrapper.extension.isAppProvided, `${addonId} is app provided`);
      equal(
        wrapper.extension.privateBrowsingAllowed,
        expectedPrivateBrowsingAllowed,
        `Builtin Addon ${addonId} should ${expectedPrivateBrowsingAllowed ? "" : "NOT"} have access private browsing access`
      );
      await wrapper.unload();
    }

    await assertPrivateBrowsingAllowed({
      addonId: id,
      expectedPrivateBrowsingAllowed: true,
    });
    await assertPrivateBrowsingAllowed({
      addonId: idNotAllowed,
      manifest: {
        incognito: "not_allowed",
      },
      expectedPrivateBrowsingAllowed: false,
    });

    await AddonTestUtils.promiseShutdownManager();
  }
);
