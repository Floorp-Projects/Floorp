/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);
AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("privileged");

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

async function runIncognitoTest(extensionData, privateBrowsingAllowed) {
  let wrapper = ExtensionTestUtils.loadExtension(extensionData);
  await wrapper.startup();
  let { extension } = wrapper;

  equal(
    extension.permissions.has("internal:privateBrowsingAllowed"),
    privateBrowsingAllowed,
    "privateBrowsingAllowed in serialized extension"
  );
  equal(
    extension.privateBrowsingAllowed,
    privateBrowsingAllowed,
    "privateBrowsingAllowed in extension"
  );
  equal(
    extension.policy.privateBrowsingAllowed,
    privateBrowsingAllowed,
    "privateBrowsingAllowed on policy"
  );

  await wrapper.unload();
}

add_task(async function test_extension_incognito_spanning() {
  await runIncognitoTest({}, false);
});

// Test that when we are restricted, we can override the restriction for tests.
add_task(async function test_extension_incognito_override_spanning() {
  let extensionData = {
    incognitoOverride: "spanning",
  };
  await runIncognitoTest(extensionData, true);
});

// This tests that a privileged extension will always have private browsing.
add_task(async function test_extension_incognito_privileged() {
  let extensionData = {
    isPrivileged: true,
  };
  await runIncognitoTest(extensionData, true);
});

add_task(async function test_extension_privileged_not_allowed() {
  let addonId = "privileged_not_allowed@mochi.test";
  let extensionData = {
    manifest: {
      version: "1.0",
      browser_specific_settings: { gecko: { id: addonId } },
      incognito: "not_allowed",
    },
    useAddonManager: "permanent",
    isPrivileged: true,
  };
  let wrapper = ExtensionTestUtils.loadExtension(extensionData);
  await wrapper.startup();
  let policy = WebExtensionPolicy.getByID(addonId);
  equal(
    policy.extension.isPrivileged,
    true,
    "The test extension is privileged"
  );
  equal(
    policy.privateBrowsingAllowed,
    false,
    "privateBrowsingAllowed is false"
  );

  await wrapper.unload();
});

// Test that we remove pb permission if an extension is updated to not_allowed.
add_task(async function test_extension_upgrade_not_allowed() {
  let addonId = "upgrade@mochi.test";
  let extensionData = {
    manifest: {
      version: "1.0",
      browser_specific_settings: { gecko: { id: addonId } },
      incognito: "spanning",
    },
    useAddonManager: "permanent",
    incognitoOverride: "spanning",
  };
  let wrapper = ExtensionTestUtils.loadExtension(extensionData);
  await wrapper.startup();

  let policy = WebExtensionPolicy.getByID(addonId);

  equal(
    policy.privateBrowsingAllowed,
    true,
    "privateBrowsingAllowed in extension"
  );

  extensionData.manifest.version = "2.0";
  extensionData.manifest.incognito = "not_allowed";
  await wrapper.upgrade(extensionData);

  equal(wrapper.version, "2.0", "Expected extension version");
  policy = WebExtensionPolicy.getByID(addonId);
  equal(
    policy.privateBrowsingAllowed,
    false,
    "privateBrowsingAllowed is false"
  );

  await wrapper.unload();
});
