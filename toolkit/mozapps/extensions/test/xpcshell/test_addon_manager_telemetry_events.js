/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const {TelemetryController} = ChromeUtils.import("resource://gre/modules/TelemetryController.jsm");

// We don't have an easy way to serve update manifests from a secure URL.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const EVENT_CATEGORY = "addonsManager";
const EVENT_METHODS_INSTALL = ["install", "update"];
const EVENT_METHODS_MANAGE = [
  "disable", "enable", "uninstall",
];
const EVENT_METHODS = [...EVENT_METHODS_INSTALL, ...EVENT_METHODS_MANAGE];

const FAKE_INSTALL_TELEMETRY_INFO = {
  source: "fake-install-source",
  method: "fake-install-method",
};

function getTelemetryEvents(includeMethods = EVENT_METHODS) {
  const snapshot = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true);

  ok(snapshot.parent && snapshot.parent.length > 0, "Got parent telemetry events in the snapshot");

  return snapshot.parent.filter(([timestamp, category, method]) => {
    const includeMethod = includeMethods ?
          includeMethods.includes(method) : true;

    return category === EVENT_CATEGORY && includeMethod;
  }).map(event => {
    return {method: event[2], object: event[3], value: event[4], extra: event[5]};
  });
}

function assertNoTelemetryEvents() {
  const snapshot = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true);

  if (!snapshot.parent || snapshot.parent.length === 0) {
    ok(true, "Got no parent telemetry events as expected");
    return;
  }

  let filteredEvents = snapshot.parent.filter(([timestamp, category, method]) => {
    return category === EVENT_CATEGORY;
  });

  Assert.deepEqual(filteredEvents, [], "Got no AMTelemetry events as expected");
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Telemetry test setup needed to ensure that the builtin events are defined
  // and they can be collected and verified.
  await TelemetryController.testSetup();

  await promiseStartupManager();
});

// Test the basic install and management flows.
add_task(async function test_basic_telemetry_events() {
  const EXTENSION_ID = "basic@test.extension";

  const manifest = {
    applications: {gecko: {id: EXTENSION_ID}},
  };

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "permanent",
    amInstallTelemetryInfo: FAKE_INSTALL_TELEMETRY_INFO,
  });

  await extension.startup();

  const addon = await promiseAddonByID(EXTENSION_ID);

  info("Disabling the extension");
  await addon.disable();

  info("Set pending uninstall on the extension");
  const onceAddonUninstalling = promiseAddonEvent("onUninstalling");
  addon.uninstall(true);
  await onceAddonUninstalling;

  info("Cancel pending uninstall");
  const oncePendingUninstallCancelled = promiseAddonEvent("onOperationCancelled");
  addon.cancelUninstall();
  await oncePendingUninstallCancelled;

  info("Re-enabling the extension");
  const onceAddonStarted = promiseWebExtensionStartup(EXTENSION_ID);
  const onceAddonEnabled = promiseAddonEvent("onEnabled");
  addon.enable();
  await Promise.all([onceAddonEnabled, onceAddonStarted]);

  await extension.unload();

  let amEvents = getTelemetryEvents();

  const amMethods = amEvents.map(evt => evt.method);
  const expectedMethods = [
    // These two install methods are related to the steps "started" and "completed".
    "install", "install",
    // Sequence of disable and enable (pending uninstall and undo uninstall are not going to
    // record any telemetry events).
    "disable", "enable",
    // The final "uninstall" when the test extension is unloaded.
    "uninstall",
  ];
  Assert.deepEqual(amMethods, expectedMethods, "Got the addonsManager telemetry events in the expected order");

  const installEvents = amEvents.filter(evt => evt.method === "install");
  const expectedInstallEvents = [
    {
      method: "install", object: "extension", value: "1",
      extra: {
        addon_id: "basic@test.extension",
        step: "started",
        ...FAKE_INSTALL_TELEMETRY_INFO,
      },
    },
    {
      method: "install", object: "extension", value: "1",
      extra: {
        addon_id: "basic@test.extension",
        step: "completed",
        ...FAKE_INSTALL_TELEMETRY_INFO,
      },
    },
  ];
  Assert.deepEqual(installEvents, expectedInstallEvents, "Got the expected addonsManager.install events");

  const manageEvents = amEvents.filter(evt => EVENT_METHODS_MANAGE.includes(evt.method));
  const expectedExtra = FAKE_INSTALL_TELEMETRY_INFO;
  const expectedManageEvents = [
    {
      method: "disable", object: "extension", value: "basic@test.extension", extra: expectedExtra,
    },
    {
      method: "enable", object: "extension", value: "basic@test.extension", extra: expectedExtra,
    },
    {
      method: "uninstall", object: "extension", value: "basic@test.extension", extra: expectedExtra,
    },
  ];
  Assert.deepEqual(manageEvents, expectedManageEvents, "Got the expected addonsManager.manage events");

  // Verify that on every install flow, the value of the addonsManager.install Telemetry events
  // is being incremented.

  extension = ExtensionTestUtils.loadExtension({
    manifest,
    useAddonManager: "permanent",
    amInstallTelemetryInfo: FAKE_INSTALL_TELEMETRY_INFO,
  });

  await extension.startup();
  await extension.unload();

  const eventsFromNewInstall = getTelemetryEvents();
  equal(eventsFromNewInstall.length, 3, "Got the expected number of addonsManager install events");

  const eventValues = eventsFromNewInstall.filter(evt => evt.method === "install").map(evt => evt.value);
  const expectedValues = ["2", "2"];
  Assert.deepEqual(eventValues, expectedValues, "Got the expected install id");
});

add_task(async function test_update_telemetry_events() {
  const EXTENSION_ID = "basic@test.extension";

  const testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});

  const updateUrl = `http://example.com/updates.json`;

  const testAddon = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      applications: {
        gecko: {
          id: EXTENSION_ID,
          update_url: updateUrl,
        },
      },
    },
  });

  const testUserRequestedUpdate = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: {
        gecko: {
          id: EXTENSION_ID,
          update_url: updateUrl,
        },
      },
    },
  });
  const testAppRequestedUpdate = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "2.1",
      applications: {
        gecko: {
          id: EXTENSION_ID,
          update_url: updateUrl,
        },
      },
    },
  });

  testserver.registerFile(`/addons/${EXTENSION_ID}-2.0.xpi`, testUserRequestedUpdate);
  testserver.registerFile(`/addons/${EXTENSION_ID}-2.1.xpi`, testAppRequestedUpdate);

  let updates = [
    {
      "version": "2.0",
      "update_link": `http://example.com/addons/${EXTENSION_ID}-2.0.xpi`,
      "applications": {
        "gecko": {
          "strict_min_version": "1",
        },
      },
    },
  ];

  testserver.registerPathHandler("/updates.json", (request, response) => {
    response.write(`{
      "addons": {
        "${EXTENSION_ID}": {
          "updates": ${JSON.stringify(updates)}
        }
      }
    }`);
  });

  await promiseInstallFile(testAddon, false, FAKE_INSTALL_TELEMETRY_INFO);

  let addon = await AddonManager.getAddonByID(EXTENSION_ID);

  // User requested update.
  await promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  let installs = await AddonManager.getAllInstalls();
  await promiseCompleteAllInstalls(installs);

  updates = [
    {
      "version": "2.1",
      "update_link": `http://example.com/addons/${EXTENSION_ID}-2.1.xpi`,
      "applications": {
        "gecko": {
          "strict_min_version": "1",
        },
      },
    },
  ];

  // App requested update.
  await promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
  let installs2 = await AddonManager.getAllInstalls();
  await promiseCompleteAllInstalls(installs2);

  updates = [
    {
      "version": "2.1.1",
      "update_link": `http://example.com/addons/${EXTENSION_ID}-2.1.1-network-failure.xpi`,
      "applications": {
        "gecko": {
          "strict_min_version": "1",
        },
      },
    },
  ];

  // Update which fails to download.
  await promiseFindAddonUpdates(addon, AddonManager.UPDATE_WHEN_PERIODIC_UPDATE);
  let installs3 = await AddonManager.getAllInstalls();
  await promiseCompleteAllInstalls(installs3);

  let amEvents = getTelemetryEvents();

  const installEvents = amEvents.filter(evt => evt.method === "install").map(evt => {
    delete evt.value;
    return evt;
  });

  const addon_id = "basic@test.extension";
  const object = "extension";

  Assert.deepEqual(installEvents, [
    {method: "install", object, extra: {addon_id, step: "started", ...FAKE_INSTALL_TELEMETRY_INFO}},
    {method: "install", object, extra: {addon_id, step: "completed", ...FAKE_INSTALL_TELEMETRY_INFO}},
  ], "Got the expected addonsManager.install events");

  const updateEvents = amEvents.filter(evt => evt.method === "update").map(evt => {
    delete evt.value;
    return evt;
  });

  const method = "update";
  const baseExtra = FAKE_INSTALL_TELEMETRY_INFO;

  const expectedUpdateEvents = [
    // User-requested update to the 2.1 version.
    {
      method, object, extra: {...baseExtra, addon_id, step: "started", updated_from: "user"},
    },
    {
      method, object, extra: {...baseExtra, addon_id, step: "download_started", updated_from: "user"},
    },
    {
      method, object, extra: {...baseExtra, addon_id, step: "download_completed", updated_from: "user"},
    },
    {
      method, object, extra: {...baseExtra, addon_id, step: "completed", updated_from: "user"},
    },
    // App-requested update to the 2.1 version.
    {
      method, object, extra: {...baseExtra, addon_id, step: "started", updated_from: "app"},
    },
    {
      method, object, extra: {...baseExtra, addon_id, step: "download_started", updated_from: "app"},
    },
    {
      method, object, extra: {...baseExtra, addon_id, step: "download_completed", updated_from: "app"},
    },
    {
      method, object, extra: {...baseExtra, addon_id, step: "completed", updated_from: "app"},
    },
    // Broken update to the 2.1.1 version (on ERROR_NETWORK_FAILURE).
    {
      method, object, extra: {...baseExtra, addon_id, step: "started", updated_from: "app"},
    },
    {
      method, object, extra: {...baseExtra, addon_id, step: "download_started", updated_from: "app"},
    },
    {
      method, object, extra: {
        ...baseExtra,
        addon_id,
        error: "ERROR_NETWORK_FAILURE",
        step: "download_failed",
        updated_from: "app",
      },
    },
  ];

  for (let i = 0; i < updateEvents.length; i++) {
    if (["download_completed", "download_failed"].includes(updateEvents[i].extra.step)) {
      const download_time = parseInt(updateEvents[i].extra.download_time, 10);
      ok(!isNaN(download_time) && download_time > 0,
         `Got a download_time extra in ${updateEvents[i].extra.step} events: ${download_time}`);

      delete updateEvents[i].extra.download_time;
    }

    Assert.deepEqual(updateEvents[i], expectedUpdateEvents[i], "Got the expected addonsManager.update events");
  }

  equal(updateEvents.length, expectedUpdateEvents.length,
        "Got the expected number of addonsManager.update events");

  await addon.uninstall();

  // Clear any AMTelemetry events related to the uninstalled extensions.
  getTelemetryEvents();
});

add_task(async function test_no_telemetry_events_on_internal_sources() {
  assertNoTelemetryEvents();

  const INTERNAL_EXTENSION_ID = "internal@test.extension";

  // Install an extension which has internal as its installation source,
  // and expect it to do not appear in the collected telemetry events.
  let internalExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id: INTERNAL_EXTENSION_ID}},
    },
    useAddonManager: "permanent",
    amInstallTelemetryInfo: {source: "internal"},
  });

  await internalExtension.startup();

  const internalAddon = await promiseAddonByID(INTERNAL_EXTENSION_ID);

  info("Disabling the internal extension");
  const onceInternalAddonDisabled = promiseAddonEvent("onDisabled");
  internalAddon.disable();
  await onceInternalAddonDisabled;

  info("Re-enabling the internal extension");
  const onceInternalAddonStarted = promiseWebExtensionStartup(INTERNAL_EXTENSION_ID);
  const onceInternalAddonEnabled = promiseAddonEvent("onEnabled");
  internalAddon.enable();
  await Promise.all([onceInternalAddonEnabled, onceInternalAddonStarted]);

  await internalExtension.unload();

  assertNoTelemetryEvents();
});

add_task(async function teardown() {
  await TelemetryController.testShutdown();
});
