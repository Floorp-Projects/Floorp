/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);
const { AMTelemetry } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs"
);

// We don't have an easy way to serve update manifests from a secure URL.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

const EVENT_CATEGORY = "addonsManager";
const EVENT_METHODS_INSTALL = ["install", "update"];
const EVENT_METHODS_MANAGE = ["disable", "enable", "uninstall"];
const EVENT_METHODS = [...EVENT_METHODS_INSTALL, ...EVENT_METHODS_MANAGE];
const GLEAN_EVENT_NAMES = ["install", "update", "manage"];

const FAKE_INSTALL_TELEMETRY_INFO = {
  source: "fake-install-source",
  method: "fake-install-method",
};

add_setup(() => {
  do_get_profile();
  Services.fog.initializeFOG();
});

function getTelemetryEvents(includeMethods = EVENT_METHODS) {
  const snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  );

  ok(
    snapshot.parent && !!snapshot.parent.length,
    "Got parent telemetry events in the snapshot"
  );

  return snapshot.parent
    .filter(([timestamp, category, method]) => {
      const includeMethod = includeMethods
        ? includeMethods.includes(method)
        : true;

      return category === EVENT_CATEGORY && includeMethod;
    })
    .map(event => {
      return {
        method: event[2],
        object: event[3],
        value: event[4],
        extra: event[5],
      };
    });
}

function assertNoTelemetryEvents() {
  const snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  );

  if (!snapshot.parent || snapshot.parent.length === 0) {
    ok(true, "Got no parent telemetry events as expected");
    return;
  }

  let filteredEvents = snapshot.parent.filter(
    ([timestamp, category, method]) => {
      return category === EVENT_CATEGORY;
    }
  );

  Assert.deepEqual(filteredEvents, [], "Got no AMTelemetry events as expected");
}

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Thunderbird doesn't have one or more of the probes used in this test.
  // Ensure the data is collected anyway.
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  // Telemetry test setup needed to ensure that the builtin events are defined
  // and they can be collected and verified.
  await TelemetryController.testSetup();

  await promiseStartupManager();
});

// Test the basic install and management flows.
add_task(
  {
    // We need to enable this pref because some assertions verify that
    // `installOrigins` is collected in some Telemetry events.
    pref_set: [["extensions.install_origins.enabled", true]],
  },
  async function test_basic_telemetry_events() {
    const EXTENSION_ID = "basic@test.extension";

    const manifest = {
      browser_specific_settings: { gecko: { id: EXTENSION_ID } },
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
    const oncePendingUninstallCancelled = promiseAddonEvent(
      "onOperationCancelled"
    );
    addon.cancelUninstall();
    await oncePendingUninstallCancelled;

    info("Re-enabling the extension");
    const onceAddonStarted = promiseWebExtensionStartup(EXTENSION_ID);
    const onceAddonEnabled = promiseAddonEvent("onEnabled");
    addon.enable();
    await Promise.all([onceAddonEnabled, onceAddonStarted]);

    await extension.unload();

    let amEvents = getTelemetryEvents();
    let gleanEvents = AddonTestUtils.getAMGleanEvents(GLEAN_EVENT_NAMES);

    const amMethods = amEvents.map(evt => evt.method);
    const expectedMethods = [
      // These two install methods are related to the steps "started" and "completed".
      "install",
      "install",
      // Sequence of disable and enable (pending uninstall and undo uninstall are not going to
      // record any telemetry events).
      "disable",
      "enable",
      // The final "uninstall" when the test extension is unloaded.
      "uninstall",
    ];
    Assert.deepEqual(
      amMethods,
      expectedMethods,
      "Got the addonsManager telemetry events in the expected order"
    );
    Assert.deepEqual(
      expectedMethods,
      gleanEvents.map(evt => {
        // Install events don't have a method, so use ducktyping to recognize
        // them: they have a step, but unlike update events, no updated_from.
        if (evt.step && !evt.updated_from) {
          return "install";
        }
        return evt.method;
      }),
      "Got the addonsManager Glean events in the expected order."
    );

    const installEvents = amEvents.filter(evt => evt.method === "install");
    const expectedInstallEvents = [
      {
        method: "install",
        object: "extension",
        value: "1",
        extra: {
          addon_id: "basic@test.extension",
          step: "started",
          install_origins: "0",
          ...FAKE_INSTALL_TELEMETRY_INFO,
        },
      },
      {
        method: "install",
        object: "extension",
        value: "1",
        extra: {
          addon_id: "basic@test.extension",
          step: "completed",
          install_origins: "0",
          ...FAKE_INSTALL_TELEMETRY_INFO,
        },
      },
    ];
    Assert.deepEqual(
      installEvents,
      expectedInstallEvents,
      "Got the expected addonsManager.install events"
    );

    let gleanInstall = {
      addon_type: "extension",
      addon_id: "basic@test.extension",
      source: FAKE_INSTALL_TELEMETRY_INFO.source,
      source_method: FAKE_INSTALL_TELEMETRY_INFO.method,
      install_origins: "0",
    };
    Assert.deepEqual(
      AddonTestUtils.getAMGleanEvents("install"),
      [
        { step: "started", ...gleanInstall },
        { step: "completed", ...gleanInstall },
      ],
      "Got the expected addonsManager Glean events."
    );

    let gleanManage = {
      addon_type: "extension",
      addon_id: "basic@test.extension",
      source: FAKE_INSTALL_TELEMETRY_INFO.source,
      source_method: FAKE_INSTALL_TELEMETRY_INFO.method,
    };
    Assert.deepEqual(
      AddonTestUtils.getAMGleanEvents("manage"),
      [
        { method: "disable", ...gleanManage },
        { method: "enable", ...gleanManage },
        { method: "uninstall", ...gleanManage },
      ],
      "Got the expected addonsManager Glean events"
    );

    const manageEvents = amEvents.filter(evt =>
      EVENT_METHODS_MANAGE.includes(evt.method)
    );
    const expectedExtra = FAKE_INSTALL_TELEMETRY_INFO;
    const expectedManageEvents = [
      {
        method: "disable",
        object: "extension",
        value: "basic@test.extension",
        extra: expectedExtra,
      },
      {
        method: "enable",
        object: "extension",
        value: "basic@test.extension",
        extra: expectedExtra,
      },
      {
        method: "uninstall",
        object: "extension",
        value: "basic@test.extension",
        extra: expectedExtra,
      },
    ];
    Assert.deepEqual(
      manageEvents,
      expectedManageEvents,
      "Got the expected addonsManager.manage events"
    );

    Services.fog.testResetFOG();
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
    equal(
      eventsFromNewInstall.length,
      3,
      "Got the expected number of addonsManager install events"
    );

    equal(
      3,
      AddonTestUtils.getAMGleanEvents(GLEAN_EVENT_NAMES).length,
      "Got the expected number of addonsManager Glean events."
    );
    equal(
      2,
      AddonTestUtils.getAMGleanEvents("install", { install_id: "2" }).length,
      "Got the expected install_id for Glean install event."
    );

    const eventValues = eventsFromNewInstall
      .filter(evt => evt.method === "install")
      .map(evt => evt.value);
    const expectedValues = ["2", "2"];
    Assert.deepEqual(
      eventValues,
      expectedValues,
      "Got the expected install id"
    );

    Services.fog.testResetFOG();
  }
);

add_task(
  {
    // We need to enable this pref because some assertions verify that
    // `installOrigins` is collected in some Telemetry events.
    pref_set: [["extensions.install_origins.enabled", true]],
  },
  async function test_update_telemetry_events() {
    const EXTENSION_ID = "basic@test.extension";

    const testserver = AddonTestUtils.createHttpServer({
      hosts: ["example.com"],
    });

    const updateUrl = `http://example.com/updates.json`;

    const testAddon = AddonTestUtils.createTempWebExtensionFile({
      manifest: {
        version: "1.0",
        browser_specific_settings: {
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
        browser_specific_settings: {
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
        browser_specific_settings: {
          gecko: {
            id: EXTENSION_ID,
            update_url: updateUrl,
          },
        },
      },
    });

    testserver.registerFile(
      `/addons/${EXTENSION_ID}-2.0.xpi`,
      testUserRequestedUpdate
    );
    testserver.registerFile(
      `/addons/${EXTENSION_ID}-2.1.xpi`,
      testAppRequestedUpdate
    );

    let updates = [
      {
        version: "2.0",
        update_link: `http://example.com/addons/${EXTENSION_ID}-2.0.xpi`,
        applications: {
          gecko: {
            strict_min_version: "1",
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
    await promiseFindAddonUpdates(
      addon,
      AddonManager.UPDATE_WHEN_USER_REQUESTED
    );
    let installs = await AddonManager.getAllInstalls();
    await promiseCompleteAllInstalls(installs);

    updates = [
      {
        version: "2.1",
        update_link: `http://example.com/addons/${EXTENSION_ID}-2.1.xpi`,
        applications: {
          gecko: {
            strict_min_version: "1",
          },
        },
      },
    ];

    // App requested update.
    await promiseFindAddonUpdates(
      addon,
      AddonManager.UPDATE_WHEN_PERIODIC_UPDATE
    );
    let installs2 = await AddonManager.getAllInstalls();
    await promiseCompleteAllInstalls(installs2);

    updates = [
      {
        version: "2.1.1",
        update_link: `http://example.com/addons/${EXTENSION_ID}-2.1.1-network-failure.xpi`,
        applications: {
          gecko: {
            strict_min_version: "1",
          },
        },
      },
    ];

    // Update which fails to download.
    await promiseFindAddonUpdates(
      addon,
      AddonManager.UPDATE_WHEN_PERIODIC_UPDATE
    );
    let installs3 = await AddonManager.getAllInstalls();
    await promiseCompleteAllInstalls(installs3);

    let amEvents = getTelemetryEvents();

    const installEvents = amEvents
      .filter(evt => evt.method === "install")
      .map(evt => {
        delete evt.value;
        return evt;
      });

    const addon_id = "basic@test.extension";
    const object = "extension";

    let gleanInstall = {
      addon_id,
      addon_type: "extension",
      install_origins: "0",
      source: FAKE_INSTALL_TELEMETRY_INFO.source,
      source_method: FAKE_INSTALL_TELEMETRY_INFO.method,
    };

    Assert.deepEqual(
      AddonTestUtils.getAMGleanEvents("install"),
      [
        { step: "started", ...gleanInstall },
        { step: "completed", ...gleanInstall },
      ],
      "Got the expected install Glean events."
    );

    Assert.deepEqual(
      installEvents,
      [
        {
          method: "install",
          object,
          extra: {
            addon_id,
            step: "started",
            install_origins: "0",
            ...FAKE_INSTALL_TELEMETRY_INFO,
          },
        },
        {
          method: "install",
          object,
          extra: {
            addon_id,
            step: "completed",
            install_origins: "0",
            ...FAKE_INSTALL_TELEMETRY_INFO,
          },
        },
      ],
      "Got the expected addonsManager.install events"
    );

    const updateEvents = amEvents
      .filter(evt => evt.method === "update")
      .map(evt => {
        delete evt.value;
        return evt;
      });

    const method = "update";
    const baseExtra = FAKE_INSTALL_TELEMETRY_INFO;

    let glean = AddonTestUtils.getAMGleanEvents("update");
    glean.forEach(e => delete e.download_time);

    let gleanUpdate = {
      addon_id,
      addon_type: "extension",
      source: FAKE_INSTALL_TELEMETRY_INFO.source,
      source_method: FAKE_INSTALL_TELEMETRY_INFO.method,
    };
    Assert.deepEqual(
      glean,
      [
        { step: "started", updated_from: "user", ...gleanUpdate },
        { step: "download_started", updated_from: "user", ...gleanUpdate },
        { step: "download_completed", updated_from: "user", ...gleanUpdate },
        { step: "completed", updated_from: "user", ...gleanUpdate },
        { step: "started", updated_from: "app", ...gleanUpdate },
        { step: "download_started", updated_from: "app", ...gleanUpdate },
        { step: "download_completed", updated_from: "app", ...gleanUpdate },
        { step: "completed", updated_from: "app", ...gleanUpdate },
        { step: "started", updated_from: "app", ...gleanUpdate },
        { step: "download_started", updated_from: "app", ...gleanUpdate },
        {
          step: "download_failed",
          updated_from: "app",
          error: "ERROR_NETWORK_FAILURE",
          ...gleanUpdate,
        },
      ],
      "Got the expected Glean update events."
    );

    const expectedUpdateEvents = [
      // User-requested update to the 2.1 version.
      {
        method,
        object,
        extra: {
          ...baseExtra,
          addon_id,
          step: "started",
          updated_from: "user",
        },
      },
      {
        method,
        object,
        extra: {
          ...baseExtra,
          addon_id,
          step: "download_started",
          updated_from: "user",
        },
      },
      {
        method,
        object,
        extra: {
          ...baseExtra,
          addon_id,
          step: "download_completed",
          updated_from: "user",
        },
      },
      {
        method,
        object,
        extra: {
          ...baseExtra,
          addon_id,
          step: "completed",
          updated_from: "user",
        },
      },
      // App-requested update to the 2.1 version.
      {
        method,
        object,
        extra: { ...baseExtra, addon_id, step: "started", updated_from: "app" },
      },
      {
        method,
        object,
        extra: {
          ...baseExtra,
          addon_id,
          step: "download_started",
          updated_from: "app",
        },
      },
      {
        method,
        object,
        extra: {
          ...baseExtra,
          addon_id,
          step: "download_completed",
          updated_from: "app",
        },
      },
      {
        method,
        object,
        extra: {
          ...baseExtra,
          addon_id,
          step: "completed",
          updated_from: "app",
        },
      },
      // Broken update to the 2.1.1 version (on ERROR_NETWORK_FAILURE).
      {
        method,
        object,
        extra: { ...baseExtra, addon_id, step: "started", updated_from: "app" },
      },
      {
        method,
        object,
        extra: {
          ...baseExtra,
          addon_id,
          step: "download_started",
          updated_from: "app",
        },
      },
      {
        method,
        object,
        extra: {
          ...baseExtra,
          addon_id,
          error: "ERROR_NETWORK_FAILURE",
          step: "download_failed",
          updated_from: "app",
        },
      },
    ];

    AddonTestUtils.getAMGleanEvents("update")
      .filter(e => ["download_completed", "download_failed"].includes(e.step))
      .forEach(e =>
        ok(
          parseInt(e.download_time, 10) > 0,
          `At step ${e.step} download_time: ${e.download_time}`
        )
      );

    for (let i = 0; i < updateEvents.length; i++) {
      if (
        ["download_completed", "download_failed"].includes(
          updateEvents[i].extra.step
        )
      ) {
        const download_time = parseInt(updateEvents[i].extra.download_time, 10);
        ok(
          !isNaN(download_time) && download_time > 0,
          `Got a download_time extra in ${updateEvents[i].extra.step} events: ${download_time}`
        );

        delete updateEvents[i].extra.download_time;
      }

      Assert.deepEqual(
        updateEvents[i],
        expectedUpdateEvents[i],
        "Got the expected addonsManager.update events"
      );
    }

    equal(
      updateEvents.length,
      expectedUpdateEvents.length,
      "Got the expected number of addonsManager.update events"
    );

    await addon.uninstall();

    // Clear any AMTelemetry events related to the uninstalled extensions.
    getTelemetryEvents();
    Services.fog.testResetFOG();
  }
);

add_task(async function test_no_telemetry_events_on_internal_sources() {
  assertNoTelemetryEvents();

  const INTERNAL_EXTENSION_ID = "internal@test.extension";

  // Install an extension which has internal as its installation source,
  // and expect it to do not appear in the collected telemetry events.
  let internalExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: { gecko: { id: INTERNAL_EXTENSION_ID } },
    },
    useAddonManager: "permanent",
    amInstallTelemetryInfo: { source: "internal" },
  });

  await internalExtension.startup();

  const internalAddon = await promiseAddonByID(INTERNAL_EXTENSION_ID);

  info("Disabling the internal extension");
  const onceInternalAddonDisabled = promiseAddonEvent("onDisabled");
  internalAddon.disable();
  await onceInternalAddonDisabled;

  info("Re-enabling the internal extension");
  const onceInternalAddonStarted = promiseWebExtensionStartup(
    INTERNAL_EXTENSION_ID
  );
  const onceInternalAddonEnabled = promiseAddonEvent("onEnabled");
  internalAddon.enable();
  await Promise.all([onceInternalAddonEnabled, onceInternalAddonStarted]);

  await internalExtension.unload();

  assertNoTelemetryEvents();
});

add_task(async function test_collect_attribution_data_for_amo() {
  assertNoTelemetryEvents();

  // We pass the `source` value to `amInstallTelemetryInfo` in this test so the
  // host could be anything in this variable below. Whether to collect
  // attribution data for AMO is determined by the `source` value, not this
  // host.
  const url = "https://addons.mozilla.org/";
  const addonId = "{28374a9a-676c-5640-bfa7-865cd4686ead}";
  // This is the SHA256 hash of the `addonId` above.
  const expectedHashedAddonId =
    "cf815c9f45c249473d630705f89e64d359737a106a375bbb83be71e6d52dc234";

  for (const { source, sourceURL, expectNoEvent, expectedAmoAttribution } of [
    // Basic test.
    {
      source: "amo",
      sourceURL: `${url}?utm_content=utm-content-value`,
      expectedAmoAttribution: {
        utm_content: "utm-content-value",
      },
    },
    // No UTM parameters will produce an event without any attribution data.
    {
      source: "amo",
      sourceURL: url,
      expectedAmoAttribution: {},
    },
    // Invalid source URLs will produce an event without any attribution data.
    {
      source: "amo",
      sourceURL: "invalid-url",
      expectedAmoAttribution: {},
    },
    // No source URL.
    {
      source: "amo",
      sourceURL: null,
      expectedAmoAttribution: {},
    },
    {
      source: "amo",
      sourceURL: undefined,
      expectedAmoAttribution: {},
    },
    // Ignore unsupported/bogus UTM parameters.
    {
      source: "amo",
      sourceURL: [
        `${url}?utm_content=utm-content-value`,
        "utm_foo=invalid",
        "utm_campaign=some-campaign",
        "utm_term=invalid-too",
      ].join("&"),
      expectedAmoAttribution: {
        utm_campaign: "some-campaign",
        utm_content: "utm-content-value",
      },
    },
    {
      source: "amo",
      sourceURL: `${url}?foo=bar&q=azerty`,
      expectedAmoAttribution: {},
    },
    // Long values are truncated.
    {
      source: "amo",
      sourceURL: `${url}?utm_medium=${"a".repeat(100)}`,
      expectedAmoAttribution: {
        utm_medium: "a".repeat(40),
      },
    },
    // Only collect the first value if the parameter is passed more than once.
    {
      source: "amo",
      sourceURL: `${url}?utm_source=first-source&utm_source=second-source`,
      expectedAmoAttribution: {
        utm_source: "first-source",
      },
    },
    // When source is "disco", we don't collect the UTM parameters.
    {
      source: "disco",
      sourceURL: `${url}?utm_content=utm-content-value`,
      expectedAmoAttribution: {},
    },
    // When source is neither "amo" nor "disco", we don't collect anything.
    {
      source: "link",
      sourceURL: `${url}?utm_content=utm-content-value`,
      expectNoEvent: true,
    },
    {
      source: null,
      sourceURL: `${url}?utm_content=utm-content-value`,
      expectNoEvent: true,
    },
    {
      source: undefined,
      sourceURL: `${url}?utm_content=utm-content-value`,
      expectNoEvent: true,
    },
  ]) {
    const extDefinition = {
      useAddonManager: "permanent",
      manifest: {
        version: "1.0",
        browser_specific_settings: { gecko: { id: addonId } },
      },
      amInstallTelemetryInfo: {
        ...FAKE_INSTALL_TELEMETRY_INFO,
        sourceURL,
        source,
      },
    };
    let extension = ExtensionTestUtils.loadExtension(extDefinition);

    await extension.startup();

    const installStatsEvents = getTelemetryEvents(["install_stats"]);
    let gleanEvents = AddonTestUtils.getAMGleanEvents(["installStats"]);
    Services.fog.testResetFOG();

    if (expectNoEvent === true) {
      Assert.equal(
        installStatsEvents.length,
        0,
        "no install_stats event should be recorded"
      );
      Assert.equal(
        gleanEvents.length,
        0,
        "No install_stats Glean event should be recorded."
      );
    } else {
      Assert.equal(
        installStatsEvents.length,
        1,
        "only one install_stats event should be recorded"
      );
      Assert.equal(
        gleanEvents.length,
        1,
        "Only one install_stats Glean event should be recorded."
      );

      const installStatsEvent = installStatsEvents[0];

      Assert.deepEqual(installStatsEvent, {
        method: "install_stats",
        object: "extension",
        value: expectedHashedAddonId,
        extra: {
          addon_id: addonId,
          ...expectedAmoAttribution,
        },
      });
      Assert.deepEqual(gleanEvents[0], {
        addon_id: addonId,
        addon_type: "extension",
        ...expectedAmoAttribution,
      });
    }

    await extension.upgrade({
      ...extDefinition,
      manifest: {
        ...extDefinition.manifest,
        version: "2.0",
      },
    });

    Assert.deepEqual(
      getTelemetryEvents(["install_stats"]),
      [],
      "no install_stats event should be recorded on addon updates"
    );

    Assert.deepEqual(
      AddonTestUtils.getAMGleanEvents(["installStats"]),
      [],
      "No install_stats Glean event should be recorded on addon updates."
    );

    await extension.unload();
    Services.fog.testResetFOG();
  }

  getTelemetryEvents();
});

add_task(async function test_collect_attribution_data_for_amo_with_long_id() {
  assertNoTelemetryEvents();

  // We pass the `source` value to `installTelemetryInfo` in this test so the
  // host could be anything in this variable below. Whether to collect
  // attribution data for AMO is determined by the `source` value, not this
  // host.
  const url = "https://addons.mozilla.org/";
  const addonId = `@${"a".repeat(90)}`;
  // This is the SHA256 hash of the `addonId` above.
  const expectedHashedAddonId =
    "964d902353fc1c127228b66ec8a174c340cb2e02dbb550c6000fb1cd3ca2f489";

  const installTelemetryInfo = {
    ...FAKE_INSTALL_TELEMETRY_INFO,
    sourceURL: `${url}?utm_content=utm-content-value`,
    source: "amo",
  };

  // We call `recordInstallStatsEvent()` directly because using an add-on ID
  // longer than 64 chars causes signing issues in tests (because of the
  // differences between the fake CertDB injected by
  // `AddonTestUtils.overrideCertDB()` and the real one).
  const fakeAddonInstall = {
    addon: { id: addonId },
    type: "extension",
    installTelemetryInfo,
    hashedAddonId: expectedHashedAddonId,
  };
  AMTelemetry.recordInstallStatsEvent(fakeAddonInstall);

  const installStatsEvents = getTelemetryEvents(["install_stats"]);
  Assert.equal(
    installStatsEvents.length,
    1,
    "only one install_stats event should be recorded"
  );

  const installStatsEvent = installStatsEvents[0];

  Assert.deepEqual(installStatsEvent, {
    method: "install_stats",
    object: "extension",
    value: expectedHashedAddonId,
    extra: {
      addon_id: AMTelemetry.getTrimmedString(addonId),
      utm_content: "utm-content-value",
    },
  });

  Assert.deepEqual(
    AddonTestUtils.getAMGleanEvents(["installStats"]),
    [
      {
        addon_type: "extension",
        addon_id: AMTelemetry.getTrimmedString(addonId),
        utm_content: "utm-content-value",
      },
    ],
    "Got the expected install_stats Glean event."
  );
  Services.fog.testResetFOG();
});

add_task(async function test_collect_attribution_data_for_rtamo() {
  assertNoTelemetryEvents();

  const url = "https://addons.mozilla.org/";
  const addonId = "{28374a9a-676c-5640-bfa7-865cd4686ead}";
  // This is the SHA256 hash of the `addonId` above.
  const expectedHashedAddonId =
    "cf815c9f45c249473d630705f89e64d359737a106a375bbb83be71e6d52dc234";

  // We simulate what is happening in:
  // https://searchfox.org/mozilla-central/rev/d2786d9a6af7507bc3443023f0495b36b7e84c2d/browser/components/newtab/content-src/lib/aboutwelcome-utils.js#91
  const installTelemetryInfo = {
    ...FAKE_INSTALL_TELEMETRY_INFO,
    sourceURL: `${url}?utm_content=utm-content-value`,
    source: "rtamo",
  };

  const fakeAddonInstall = {
    addon: { id: addonId },
    type: "extension",
    installTelemetryInfo,
    hashedAddonId: expectedHashedAddonId,
  };
  AMTelemetry.recordInstallStatsEvent(fakeAddonInstall);

  const installStatsEvents = getTelemetryEvents(["install_stats"]);
  Assert.equal(
    installStatsEvents.length,
    1,
    "only one install_stats event should be recorded"
  );

  const installStatsEvent = installStatsEvents[0];

  Assert.deepEqual(installStatsEvent, {
    method: "install_stats",
    object: "extension",
    value: expectedHashedAddonId,
    extra: {
      addon_id: AMTelemetry.getTrimmedString(addonId),
    },
  });

  Assert.deepEqual(
    AddonTestUtils.getAMGleanEvents(["installStats"]),
    [
      {
        addon_type: "extension",
        addon_id: AMTelemetry.getTrimmedString(addonId),
      },
    ],
    "Got the expected install_stats Glean event."
  );
  Services.fog.testResetFOG();
});

add_task(async function teardown() {
  await TelemetryController.testShutdown();
});
