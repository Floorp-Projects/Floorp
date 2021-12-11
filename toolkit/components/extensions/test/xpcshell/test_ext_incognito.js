/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);
AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("privileged");

// Assert on the expected "addonsManager.action" telemetry events (and optional filter events to verify
// by using a given actionType).
function assertActionAMTelemetryEvent(
  expectedActionEvents,
  assertMessage,
  { actionType } = {}
) {
  const snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  );

  ok(
    snapshot.parent && !!snapshot.parent.length,
    "Got parent telemetry events in the snapshot"
  );

  const events = snapshot.parent
    .filter(([timestamp, category, method, object, value, extra]) => {
      return (
        category === "addonsManager" &&
        method === "action" &&
        (!actionType ? true : extra && extra.action === actionType)
      );
    })
    .map(([timestamp, category, method, object, value, extra]) => {
      return { method, object, value, extra };
    });

  Assert.deepEqual(events, expectedActionEvents, assertMessage);
}

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

// We only test spanning upgrades since that is the only allowed
// incognito type prior to feature being turned on.
add_task(async function test_extension_incognito_spanning_grandfathered() {
  await AddonTestUtils.promiseStartupManager();
  Services.prefs.setBoolPref("extensions.incognito.migrated", false);

  // This extension gets disabled before the "upgrade", it should not
  // get grandfathered permissions.
  const disabledAddonId = "disabled-ext@mozilla.com";
  let disabledWrapper = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: disabledAddonId } },
      incognito: "spanning",
    },
    useAddonManager: "permanent",
  });
  await disabledWrapper.startup();
  let disabledPolicy = WebExtensionPolicy.getByID(disabledAddonId);

  // Verify policy settings.
  equal(
    disabledPolicy.permissions.includes("internal:privateBrowsingAllowed"),
    false,
    "privateBrowsingAllowed is not in permissions for disabled addon"
  );
  equal(
    disabledPolicy.privateBrowsingAllowed,
    false,
    "privateBrowsingAllowed in not allowed disabled addon"
  );

  let disabledAddon = await AddonManager.getAddonByID(disabledAddonId);
  await disabledAddon.disable();

  // This extension gets grandfathered permissions for private browsing.
  let addonId = "grandfathered@mozilla.com";
  let wrapper = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: addonId } },
      incognito: "spanning",
    },
    useAddonManager: "permanent",
  });
  await wrapper.startup();
  let policy = WebExtensionPolicy.getByID(addonId);

  // Verify policy settings.
  equal(
    policy.permissions.includes("internal:privateBrowsingAllowed"),
    false,
    "privateBrowsingAllowed is not in permissions"
  );
  equal(
    policy.privateBrowsingAllowed,
    false,
    "privateBrowsingAllowed is false in extension"
  );

  // Disable the addonsManager telemetry event category, to ensure that it will
  // be enabled automatically during the AddonManager/XPIProvider startup and
  // the telemetry event recorded (See Bug 1540112 for a rationale).
  Services.telemetry.setEventRecordingEnabled("addonsManager", false);
  await AddonTestUtils.promiseRestartManager("2");
  await wrapper.awaitStartup();

  // Did it upgrade?
  ok(
    Services.prefs.getBoolPref("extensions.incognito.migrated", false),
    "pref marked as migrated"
  );

  // Verify policy settings.
  policy = WebExtensionPolicy.getByID(addonId);
  ok(
    policy.permissions.includes("internal:privateBrowsingAllowed"),
    "privateBrowsingAllowed is in permissions"
  );
  equal(
    policy.privateBrowsingAllowed,
    true,
    "privateBrowsingAllowed in extension"
  );

  // Verify the disabled addon did not get permissions.
  disabledAddon = await AddonManager.getAddonByID(disabledAddonId);
  await disabledAddon.enable();
  disabledPolicy = WebExtensionPolicy.getByID(disabledAddonId);

  // Verify policy settings.
  equal(
    disabledPolicy.permissions.includes("internal:privateBrowsingAllowed"),
    false,
    "privateBrowsingAllowed is not in permissions for disabled addon"
  );
  equal(
    disabledPolicy.privateBrowsingAllowed,
    false,
    "privateBrowsingAllowed in disabled addon"
  );

  await wrapper.unload();
  await disabledWrapper.unload();
  Services.prefs.clearUserPref("extensions.incognito.migrated");

  const expectedEvents = [
    {
      method: "action",
      object: "appUpgrade",
      value: "on",
      extra: { addonId, action: "privateBrowsingAllowed" },
    },
  ];

  assertActionAMTelemetryEvent(
    expectedEvents,
    "Got the expected telemetry events for the grandfathered extensions",
    { actionType: "privateBrowsingAllowed" }
  );
});

add_task(async function test_extension_privileged_not_allowed() {
  let addonId = "privileged_not_allowed@mochi.test";
  let extensionData = {
    manifest: {
      version: "1.0",
      applications: { gecko: { id: addonId } },
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
      applications: { gecko: { id: addonId } },
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
