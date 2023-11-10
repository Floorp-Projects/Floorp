/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { ExtensionStorageIDB } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionStorageIDB.sys.mjs"
);
const { getTrimmedString } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionTelemetry.sys.mjs"
);
const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const HISTOGRAM_JSON_IDS = [
  "WEBEXT_STORAGE_LOCAL_SET_MS",
  "WEBEXT_STORAGE_LOCAL_GET_MS",
];
const KEYED_HISTOGRAM_JSON_IDS = [
  "WEBEXT_STORAGE_LOCAL_SET_MS_BY_ADDONID",
  "WEBEXT_STORAGE_LOCAL_GET_MS_BY_ADDONID",
];

const HISTOGRAM_IDB_IDS = [
  "WEBEXT_STORAGE_LOCAL_IDB_SET_MS",
  "WEBEXT_STORAGE_LOCAL_IDB_GET_MS",
];
const KEYED_HISTOGRAM_IDB_IDS = [
  "WEBEXT_STORAGE_LOCAL_IDB_SET_MS_BY_ADDONID",
  "WEBEXT_STORAGE_LOCAL_IDB_GET_MS_BY_ADDONID",
];

const HISTOGRAM_IDS = [].concat(HISTOGRAM_JSON_IDS, HISTOGRAM_IDB_IDS);
const KEYED_HISTOGRAM_IDS = [].concat(
  KEYED_HISTOGRAM_JSON_IDS,
  KEYED_HISTOGRAM_IDB_IDS
);

const EXTENSION_ID1 = "@test-extension1";
const EXTENSION_ID2 = "@test-extension2";

async function test_telemetry_background() {
  const { GleanTimingDistribution } = globalThis;
  const expectedEmptyGleanMetrics = ExtensionStorageIDB.isBackendEnabled
    ? ["storageLocalGetJson", "storageLocalSetJson"]
    : ["storageLocalGetIdb", "storageLocalSetIdb"];
  const expectedNonEmptyGleanMetrics = ExtensionStorageIDB.isBackendEnabled
    ? ["storageLocalGetIdb", "storageLocalSetIdb"]
    : ["storageLocalGetJson", "storageLocalSetJson"];

  const expectedEmptyHistograms = ExtensionStorageIDB.isBackendEnabled
    ? HISTOGRAM_JSON_IDS
    : HISTOGRAM_IDB_IDS;
  const expectedEmptyKeyedHistograms = ExtensionStorageIDB.isBackendEnabled
    ? KEYED_HISTOGRAM_JSON_IDS
    : KEYED_HISTOGRAM_IDB_IDS;

  const expectedNonEmptyHistograms = ExtensionStorageIDB.isBackendEnabled
    ? HISTOGRAM_IDB_IDS
    : HISTOGRAM_JSON_IDS;
  const expectedNonEmptyKeyedHistograms = ExtensionStorageIDB.isBackendEnabled
    ? KEYED_HISTOGRAM_IDB_IDS
    : KEYED_HISTOGRAM_JSON_IDS;

  const server = createHttpServer();
  server.registerDirectory("/data/", do_get_file("data"));

  const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

  async function contentScript() {
    await browser.storage.local.set({ a: "b" });
    await browser.storage.local.get("a");
    browser.test.sendMessage("contentDone");
  }

  let baseManifest = {
    permissions: ["storage"],
    content_scripts: [
      {
        matches: ["http://*/*/file_sample.html"],
        js: ["content_script.js"],
      },
    ],
  };

  let baseExtInfo = {
    async background() {
      await browser.storage.local.set({ a: "b" });
      await browser.storage.local.get("a");
      browser.test.sendMessage("backgroundDone");
    },
    files: {
      "content_script.js": contentScript,
    },
  };

  let extension1 = ExtensionTestUtils.loadExtension({
    ...baseExtInfo,
    manifest: {
      ...baseManifest,
      browser_specific_settings: {
        gecko: { id: EXTENSION_ID1 },
      },
    },
  });
  let extension2 = ExtensionTestUtils.loadExtension({
    ...baseExtInfo,
    manifest: {
      ...baseManifest,
      browser_specific_settings: {
        gecko: { id: EXTENSION_ID2 },
      },
    },
  });

  // Make sure to force flushing glean fog data from child processes before
  // resetting the already collected data.
  await Services.fog.testFlushAllChildren();
  resetTelemetryData();

  // Verify the telemetry data has been cleared.

  // Assert glean telemetry data.
  for (let metricId of expectedNonEmptyGleanMetrics) {
    assertGleanMetricsNoSamples({
      metricId,
      gleanMetric: Glean.extensionsTiming[metricId],
      gleanMetricConstructor: GleanTimingDistribution,
    });
  }

  // Assert unified telemetry data.
  let process = IS_OOP ? "extension" : "parent";
  let snapshots = getSnapshots(process);
  let keyedSnapshots = getKeyedSnapshots(process);

  for (let id of HISTOGRAM_IDS) {
    ok(!(id in snapshots), `No data recorded for histogram: ${id}.`);
  }

  for (let id of KEYED_HISTOGRAM_IDS) {
    Assert.deepEqual(
      Object.keys(keyedSnapshots[id] || {}),
      [],
      `No data recorded for histogram: ${id}.`
    );
  }

  await extension1.startup();
  await extension1.awaitMessage("backgroundDone");

  // Assert glean telemetry data.
  await Services.fog.testFlushAllChildren();
  for (let metricId of expectedNonEmptyGleanMetrics) {
    assertGleanMetricsSamplesCount({
      metricId,
      gleanMetric: Glean.extensionsTiming[metricId],
      gleanMetricConstructor: GleanTimingDistribution,
      expectedSamplesCount: 1,
    });
  }

  // Assert unified telemetry data.
  if (AppConstants.platform != "android") {
    for (let id of expectedNonEmptyHistograms) {
      await promiseTelemetryRecorded(id, process, 1);
    }
    for (let id of expectedNonEmptyKeyedHistograms) {
      await promiseKeyedTelemetryRecorded(id, process, EXTENSION_ID1, 1);
    }

    // Telemetry from extension1's background page should be recorded.
    snapshots = getSnapshots(process);
    keyedSnapshots = getKeyedSnapshots(process);

    for (let id of expectedNonEmptyHistograms) {
      equal(
        valueSum(snapshots[id].values),
        1,
        `Data recorded for histogram: ${id}.`
      );
    }

    for (let id of expectedNonEmptyKeyedHistograms) {
      Assert.deepEqual(
        Object.keys(keyedSnapshots[id]),
        [EXTENSION_ID1],
        `Data recorded for histogram: ${id}.`
      );
      equal(
        valueSum(keyedSnapshots[id][EXTENSION_ID1].values),
        1,
        `Data recorded for histogram: ${id}.`
      );
    }
  }

  await extension2.startup();
  await extension2.awaitMessage("backgroundDone");

  // Assert glean telemetry data.
  await Services.fog.testFlushAllChildren();
  for (let metricId of expectedNonEmptyGleanMetrics) {
    assertGleanMetricsSamplesCount({
      metricId,
      gleanMetric: Glean.extensionsTiming[metricId],
      gleanMetricConstructor: GleanTimingDistribution,
      expectedSamplesCount: 2,
    });
  }

  // Assert unified telemetry data.
  if (AppConstants.platform != "android") {
    for (let id of expectedNonEmptyHistograms) {
      await promiseTelemetryRecorded(id, process, 2);
    }
    for (let id of expectedNonEmptyKeyedHistograms) {
      await promiseKeyedTelemetryRecorded(id, process, EXTENSION_ID2, 1);
    }

    // Telemetry from extension2's background page should be recorded.
    snapshots = getSnapshots(process);
    keyedSnapshots = getKeyedSnapshots(process);

    for (let id of expectedNonEmptyHistograms) {
      equal(
        valueSum(snapshots[id].values),
        2,
        `Additional data recorded for histogram: ${id}.`
      );
    }

    for (let id of expectedNonEmptyKeyedHistograms) {
      Assert.deepEqual(
        Object.keys(keyedSnapshots[id]).sort(),
        [EXTENSION_ID1, EXTENSION_ID2],
        `Additional data recorded for histogram: ${id}.`
      );
      equal(
        valueSum(keyedSnapshots[id][EXTENSION_ID2].values),
        1,
        `Additional data recorded for histogram: ${id}.`
      );
    }
  }

  await extension2.unload();

  await Services.fog.testFlushAllChildren();
  resetTelemetryData();

  // Run a content script.
  process = "content";
  // Expect only telemetry for the single extension content script
  // that should be executed when loading the test webpage.
  let expectedCount = 1;
  let expectedKeyedCount = 1;

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );
  await extension1.awaitMessage("contentDone");

  // Assert glean telemetry data.
  await Services.fog.testFlushAllChildren();
  for (let metricId of expectedNonEmptyGleanMetrics) {
    assertGleanMetricsSamplesCount({
      metricId,
      gleanMetric: Glean.extensionsTiming[metricId],
      gleanMetricConstructor: GleanTimingDistribution,
      expectedSamplesCount: expectedCount,
    });
  }

  // Assert unified telemetry data.
  if (AppConstants.platform != "android") {
    for (let id of expectedNonEmptyHistograms) {
      await promiseTelemetryRecorded(id, process, expectedCount);
    }
    for (let id of expectedNonEmptyKeyedHistograms) {
      await promiseKeyedTelemetryRecorded(
        id,
        process,
        EXTENSION_ID1,
        expectedKeyedCount
      );
    }

    // Telemetry from extension1's content script should be recorded.
    snapshots = getSnapshots(process);
    keyedSnapshots = getKeyedSnapshots(process);

    for (let id of expectedNonEmptyHistograms) {
      equal(
        valueSum(snapshots[id].values),
        expectedCount,
        `Data recorded in content script for histogram: ${id}.`
      );
    }

    for (let id of expectedNonEmptyKeyedHistograms) {
      Assert.deepEqual(
        Object.keys(keyedSnapshots[id]).sort(),
        [EXTENSION_ID1],
        `Additional data recorded for histogram: ${id}.`
      );
      equal(
        valueSum(keyedSnapshots[id][EXTENSION_ID1].values),
        expectedKeyedCount,
        `Additional data recorded for histogram: ${id}.`
      );
    }
  }

  await extension1.unload();

  // Telemetry that we expect to be empty.

  // Assert glean telemetry data.
  await Services.fog.testFlushAllChildren();
  for (let metricId of expectedEmptyGleanMetrics) {
    assertGleanMetricsNoSamples({
      metricId,
      gleanMetric: Glean.extensionsTiming[metricId],
      gleanMetricConstructor: GleanTimingDistribution,
    });
  }

  // Assert unified telemetry data.
  if (AppConstants.platform != "android") {
    for (let id of expectedEmptyHistograms) {
      ok(!(id in snapshots), `No data recorded for histogram: ${id}.`);
    }

    for (let id of expectedEmptyKeyedHistograms) {
      Assert.deepEqual(
        Object.keys(keyedSnapshots[id] || {}),
        [],
        `No data recorded for histogram: ${id}.`
      );
    }
  }

  await contentPage.close();
}

add_task(async function setup() {
  // Telemetry test setup needed to ensure that the builtin events are defined
  // and they can be collected and verified.
  await TelemetryController.testSetup();

  // This is actually only needed on Android, because it does not properly support unified telemetry
  // and so, if not enabled explicitly here, it would make these tests to fail when running on a
  // non-Nightly build.
  const oldCanRecordBase = Services.telemetry.canRecordBase;
  Services.telemetry.canRecordBase = true;
  registerCleanupFunction(() => {
    Services.telemetry.canRecordBase = oldCanRecordBase;
  });
});

add_task(function test_telemetry_background_file_backend() {
  return runWithPrefs(
    [[ExtensionStorageIDB.BACKEND_ENABLED_PREF, false]],
    test_telemetry_background
  );
});

add_task(function test_telemetry_background_idb_backend() {
  return runWithPrefs(
    [
      [ExtensionStorageIDB.BACKEND_ENABLED_PREF, true],
      // Set the migrated preference for the two test extension, because the
      // first storage.local call fallbacks to run in the parent process when we
      // don't know which is the selected backend during the extension startup
      // and so we can't choose the telemetry histogram to use.
      [
        `${ExtensionStorageIDB.IDB_MIGRATED_PREF_BRANCH}.${EXTENSION_ID1}`,
        true,
      ],
      [
        `${ExtensionStorageIDB.IDB_MIGRATED_PREF_BRANCH}.${EXTENSION_ID2}`,
        true,
      ],
    ],
    test_telemetry_background
  );
});

// This test verifies that we do record the expected telemetry event when we
// normalize the error message for an unexpected error (an error raised internally
// by the QuotaManager and/or IndexedDB, which it is being normalized into the generic
// "An unexpected error occurred" error message).
add_task(async function test_telemetry_storage_local_unexpected_error() {
  // Clear any telemetry events collected so far.
  Services.telemetry.clearEvents();

  const methods = ["clear", "get", "remove", "set"];
  const veryLongErrorName = `VeryLongErrorName${Array(200).fill(0).join("")}`;
  const otherError = new Error("an error recorded as OtherError");

  const recordedErrors = [
    new DOMException("error message", "UnexpectedDOMException"),
    new DOMException("error message", veryLongErrorName),
    otherError,
  ];

  // We expect the following errors to not be recorded in telemetry (because they
  // are raised on scenarios that we already expect).
  const nonRecordedErrors = [
    new DOMException("error message", "QuotaExceededError"),
    new DOMException("error message", "DataCloneError"),
  ];

  const expectedEvents = [];

  const errors = [].concat(recordedErrors, nonRecordedErrors);

  for (let i = 0; i < errors.length; i++) {
    const error = errors[i];
    const storageMethod = methods[i] || "set";
    ExtensionStorageIDB.normalizeStorageError({
      error: errors[i],
      extensionId: EXTENSION_ID1,
      storageMethod,
    });

    if (recordedErrors.includes(error)) {
      let error_name =
        error === otherError ? "OtherError" : getTrimmedString(error.name);

      expectedEvents.push({
        value: EXTENSION_ID1,
        object: storageMethod,
        extra: { error_name },
      });
    }
  }

  await TelemetryTestUtils.assertEvents(expectedEvents, {
    category: "extensions.data",
    method: "storageLocalError",
  });
});
