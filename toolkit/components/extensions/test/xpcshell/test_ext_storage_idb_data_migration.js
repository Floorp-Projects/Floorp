/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// This test file verifies various scenarios related to the data migration
// from the JSONFile backend to the IDB backend.

AddonTestUtils.init(this);

// Create appInfo before importing any other jsm file, to prevent
// Services.appinfo to be cached before an appInfo.version is
// actually defined (which prevent failures to be triggered when
// the test run in a non nightly build).
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

const { getTrimmedString } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionTelemetry.sys.mjs"
);
const { ExtensionStorage } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionStorage.sys.mjs"
);
const { ExtensionStorageIDB } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionStorageIDB.sys.mjs"
);
const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const { promiseShutdownManager, promiseStartupManager } = AddonTestUtils;

const { IDB_MIGRATED_PREF_BRANCH, IDB_MIGRATE_RESULT_HISTOGRAM } =
  ExtensionStorageIDB;
const CATEGORIES = ["success", "failure"];
const EVENT_CATEGORY = "extensions.data";
const EVENT_OBJECT = "storageLocal";
const EVENT_METHOD = "migrateResult";
const LEAVE_STORAGE_PREF = "extensions.webextensions.keepStorageOnUninstall";
const LEAVE_UUID_PREF = "extensions.webextensions.keepUuidOnUninstall";
const TELEMETRY_EVENTS_FILTER = {
  category: "extensions.data",
  method: "migrateResult",
  object: "storageLocal",
};

add_setup(async function setup() {
  do_get_profile();
  Services.fog.initializeFOG();
});

async function createExtensionJSONFileWithData(extensionId, data) {
  await ExtensionStorage.set(extensionId, data);
  const jsonFile = await ExtensionStorage.getFile(extensionId);
  await jsonFile._save();
  const oldStorageFilename = ExtensionStorage.getStorageFile(extensionId);
  equal(
    await IOUtils.exists(oldStorageFilename),
    true,
    "The old json file has been created"
  );

  return { jsonFile, oldStorageFilename };
}

function clearMigrationHistogram() {
  const histogram = Services.telemetry.getHistogramById(
    IDB_MIGRATE_RESULT_HISTOGRAM
  );
  histogram.clear();
  equal(
    histogram.snapshot().sum,
    0,
    `No data recorded for histogram ${IDB_MIGRATE_RESULT_HISTOGRAM}`
  );
}

function assertMigrationHistogramCount(category, expectedCount) {
  const histogram = Services.telemetry.getHistogramById(
    IDB_MIGRATE_RESULT_HISTOGRAM
  );

  equal(
    histogram.snapshot().values[CATEGORIES.indexOf(category)],
    expectedCount,
    `Got the expected count on category "${category}" for histogram ${IDB_MIGRATE_RESULT_HISTOGRAM}`
  );
}

// Note: for consistency with telemetry event format, this function also
// expects the addon_id to be passed in the event.value property.
function assertMigrateResultGleanEvents(expectedEvents) {
  let glean = Glean.extensionsData.migrateResult.testGetValue() ?? [];
  equal(glean.length, expectedEvents.length, "Correct number of events.");

  expectedEvents.forEach((expected, i) =>
    Assert.deepEqual(
      glean[i].extra,
      { addon_id: expected.value, ...expected.extra },
      "Correct addon_id and event extra properties."
    )
  );
  Services.fog.testResetFOG();
}

function assertTelemetryEvents(expectedEvents) {
  TelemetryTestUtils.assertEvents(expectedEvents, {
    category: EVENT_CATEGORY,
    method: EVENT_METHOD,
    object: EVENT_OBJECT,
  });
  assertMigrateResultGleanEvents(expectedEvents);
}

add_setup(async function setup() {
  Services.prefs.setBoolPref(ExtensionStorageIDB.BACKEND_ENABLED_PREF, true);

  await promiseStartupManager();

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

  // Clear any telemetry events collected so far.
  Services.telemetry.clearEvents();
});

// Test that for newly installed extension the IDB backend is enabled without
// any data migration.
add_task(async function test_no_migration_for_newly_installed_extensions() {
  const EXTENSION_ID = "test-no-data-migration@mochi.test";

  await createExtensionJSONFileWithData(EXTENSION_ID, {
    test_old_data: "test_old_value",
  });

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      permissions: ["storage"],
      browser_specific_settings: { gecko: { id: EXTENSION_ID } },
    },
    async background() {
      const data = await browser.storage.local.get();
      browser.test.assertEq(
        Object.keys(data).length,
        0,
        "Expect the storage.local store to be empty"
      );
      browser.test.sendMessage("test-stored-data:done");
    },
  });

  await extension.startup();
  equal(
    ExtensionStorageIDB.isMigratedExtension(extension),
    true,
    "The newly installed test extension is marked as migrated"
  );
  await extension.awaitMessage("test-stored-data:done");
  await extension.unload();

  // Verify that no data migration have been needed on the newly installed
  // extension, by asserting that no telemetry events has been collected.
  await TelemetryTestUtils.assertEvents([], TELEMETRY_EVENTS_FILTER);
  assertMigrateResultGleanEvents([]);
});

// Test that the data migration is still running for a newly installed extension
// if keepStorageOnUninstall is true.
add_task(async function test_data_migration_on_keep_storage_on_uninstall() {
  Services.prefs.setBoolPref(LEAVE_STORAGE_PREF, true);

  // Store some fake data in the storage.local file backend before starting the extension.
  const EXTENSION_ID = "new-extension-on-keep-storage-on-uninstall@mochi.test";
  await createExtensionJSONFileWithData(EXTENSION_ID, {
    test_key_string: "test_value",
  });

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      permissions: ["storage"],
      browser_specific_settings: { gecko: { id: EXTENSION_ID } },
    },
    async background() {
      const storedData = await browser.storage.local.get();
      browser.test.assertEq(
        "test_value",
        storedData.test_key_string,
        "Got the expected data after the storage.local data migration"
      );
      browser.test.sendMessage("storage-local-data-migrated");
    },
  });

  await extension.startup();
  await extension.awaitMessage("storage-local-data-migrated");
  equal(
    ExtensionStorageIDB.isMigratedExtension(extension),
    true,
    "The newly installed test extension is marked as migrated"
  );
  await extension.unload();

  // Verify that the expected telemetry has been recorded.
  let expected = {
    method: "migrateResult",
    value: EXTENSION_ID,
    extra: {
      backend: "IndexedDB",
      data_migrated: "y",
      has_jsonfile: "y",
      has_olddata: "y",
    },
  };

  await TelemetryTestUtils.assertEvents([expected], TELEMETRY_EVENTS_FILTER);
  assertMigrateResultGleanEvents([expected]);

  Services.prefs.clearUserPref(LEAVE_STORAGE_PREF);
});

// Test that the old data is migrated successfully to the new storage backend
// and that the original JSONFile has been renamed.
add_task(async function test_storage_local_data_migration() {
  const EXTENSION_ID = "extension-to-be-migrated@mozilla.org";

  // Keep the extension storage and the uuid on uninstall, to verify that no telemetry events
  // are being sent for an already migrated extension.
  Services.prefs.setBoolPref(LEAVE_STORAGE_PREF, true);
  Services.prefs.setBoolPref(LEAVE_UUID_PREF, true);

  const data = {
    test_key_string: "test_value1",
    test_key_number: 1000,
    test_nested_data: {
      nested_key: true,
    },
  };

  // Store some fake data in the storage.local file backend before starting the extension.
  const { oldStorageFilename } = await createExtensionJSONFileWithData(
    EXTENSION_ID,
    data
  );

  async function background() {
    const storedData = await browser.storage.local.get();

    browser.test.assertEq(
      "test_value1",
      storedData.test_key_string,
      "Got the expected data after the storage.local data migration"
    );
    browser.test.assertEq(
      1000,
      storedData.test_key_number,
      "Got the expected data after the storage.local data migration"
    );
    browser.test.assertEq(
      true,
      storedData.test_nested_data.nested_key,
      "Got the expected data after the storage.local data migration"
    );

    browser.test.sendMessage("storage-local-data-migrated");
  }

  clearMigrationHistogram();

  let extensionDefinition = {
    useAddonManager: "temporary",
    manifest: {
      permissions: ["storage"],
      browser_specific_settings: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionDefinition);

  // Install the extension while the storage.local IDB backend is disabled.
  Services.prefs.setBoolPref(ExtensionStorageIDB.BACKEND_ENABLED_PREF, false);
  await extension.startup();

  ok(
    !ExtensionStorageIDB.isMigratedExtension(extension),
    "The test extension should be using the JSONFile backend"
  );

  // Enabled the storage.local IDB backend and upgrade the extension.
  Services.prefs.setBoolPref(ExtensionStorageIDB.BACKEND_ENABLED_PREF, true);
  await extension.upgrade({
    ...extensionDefinition,
    background,
  });

  await extension.awaitMessage("storage-local-data-migrated");

  ok(
    ExtensionStorageIDB.isMigratedExtension(extension),
    "The test extension should be using the IndexedDB backend"
  );

  const storagePrincipal = ExtensionStorageIDB.getStoragePrincipal(
    extension.extension
  );

  const idbConn = await ExtensionStorageIDB.open(storagePrincipal);

  equal(
    await idbConn.isEmpty(extension.extension),
    false,
    "Data stored in the ExtensionStorageIDB backend as expected"
  );

  equal(
    await IOUtils.exists(oldStorageFilename),
    false,
    "The old json storage file name should not exist anymore"
  );

  equal(
    await IOUtils.exists(`${oldStorageFilename}.migrated`),
    true,
    "The old json storage file name should have been renamed as .migrated"
  );

  equal(
    Services.prefs.getBoolPref(
      `${IDB_MIGRATED_PREF_BRANCH}.${EXTENSION_ID}`,
      false
    ),
    true,
    `Got the ${IDB_MIGRATED_PREF_BRANCH} preference set to true as expected`
  );

  assertMigrationHistogramCount("success", 1);
  assertMigrationHistogramCount("failure", 0);

  assertTelemetryEvents([
    {
      method: "migrateResult",
      value: EXTENSION_ID,
      extra: {
        backend: "IndexedDB",
        data_migrated: "y",
        has_jsonfile: "y",
        has_olddata: "y",
      },
    },
  ]);

  equal(
    Services.prefs.getBoolPref(
      `${IDB_MIGRATED_PREF_BRANCH}.${EXTENSION_ID}`,
      false
    ),
    true,
    `${IDB_MIGRATED_PREF_BRANCH} should still be true on keepStorageOnUninstall=true`
  );

  // Upgrade the extension and check that no telemetry events are being sent
  // for an already migrated extension.
  await extension.upgrade({
    ...extensionDefinition,
    background,
  });

  await extension.awaitMessage("storage-local-data-migrated");

  // The histogram values are unmodified.
  assertMigrationHistogramCount("success", 1);
  assertMigrationHistogramCount("failure", 0);

  // No new telemetry events recorded for the extension.
  const snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  );
  const filterByCategory = ([timestamp, category]) =>
    category === EVENT_CATEGORY;

  ok(
    !snapshot.parent || snapshot.parent.filter(filterByCategory).length === 0,
    "No telemetry events should be recorded for an already migrated extension"
  );

  Services.prefs.setBoolPref(LEAVE_STORAGE_PREF, false);
  Services.prefs.setBoolPref(LEAVE_UUID_PREF, false);

  await extension.unload();

  equal(
    Services.prefs.getPrefType(`${IDB_MIGRATED_PREF_BRANCH}.${EXTENSION_ID}`),
    Services.prefs.PREF_INVALID,
    `Got the ${IDB_MIGRATED_PREF_BRANCH} preference has been cleared on addon uninstall`
  );
});

// Test that the extensionId included in the telemetry event is being trimmed down to 80 chars
// as expected.
add_task(async function test_extensionId_trimmed_in_telemetry_event() {
  // Generated extensionId in email-like format, longer than 80 chars.
  const EXTENSION_ID = `long.extension.id@${Array(80).fill("a").join("")}`;

  const data = { test_key_string: "test_value" };

  // Store some fake data in the storage.local file backend before starting the extension.
  await createExtensionJSONFileWithData(EXTENSION_ID, data);

  async function background() {
    const storedData = await browser.storage.local.get("test_key_string");

    browser.test.assertEq(
      "test_value",
      storedData.test_key_string,
      "Got the expected data after the storage.local data migration"
    );

    browser.test.sendMessage("storage-local-data-migrated");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
      browser_specific_settings: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
    background,
    // We don't want the (default) startupReason ADDON_INSTALL because
    // that automatically sets the migrated pref and skips migration.
    startupReason: "APP_STARTUP",
  });

  await extension.startup();

  await extension.awaitMessage("storage-local-data-migrated");

  const expectedTrimmedExtensionId = getTrimmedString(EXTENSION_ID);

  equal(
    expectedTrimmedExtensionId.length,
    80,
    "The trimmed version of the extensionId should be 80 chars long"
  );

  assertTelemetryEvents([
    {
      method: "migrateResult",
      value: expectedTrimmedExtensionId,
      extra: {
        backend: "IndexedDB",
        data_migrated: "y",
        has_jsonfile: "y",
        has_olddata: "y",
      },
    },
  ]);

  await extension.unload();
});

// Test that if the old JSONFile data file is corrupted and the old data
// can't be successfully migrated to the new storage backend, then:
// - the new storage backend for that extension is still initialized and enabled
// - any new data is being stored in the new backend
// - the old file is being renamed (with the `.corrupted` suffix that JSONFile.sys.mjs
//   adds when it fails to load the data file) and still available on disk.
add_task(async function test_storage_local_corrupted_data_migration() {
  const EXTENSION_ID = "extension-corrupted-data-migration@mozilla.org";

  const invalidData = `{"test_key_string": "test_value1"`;
  const oldStorageFilename = ExtensionStorage.getStorageFile(EXTENSION_ID);

  await IOUtils.makeDirectory(
    PathUtils.join(PathUtils.profileDir, "browser-extension-data", EXTENSION_ID)
  );

  // Write the json file with some invalid data.
  await IOUtils.writeUTF8(oldStorageFilename, invalidData, { flush: true });
  equal(
    await IOUtils.readUTF8(oldStorageFilename),
    invalidData,
    "The old json file has been overwritten with invalid data"
  );

  async function background() {
    const storedData = await browser.storage.local.get();

    browser.test.assertEq(
      Object.keys(storedData).length,
      0,
      "No data should be found on invalid data migration"
    );

    await browser.storage.local.set({
      test_key_string_on_IDBBackend: "expected-value",
    });

    browser.test.sendMessage("storage-local-data-migrated-and-set");
  }

  clearMigrationHistogram();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
      browser_specific_settings: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
    background,
    // We don't want the (default) startupReason ADDON_INSTALL because
    // that automatically sets the migrated pref and skips migration.
    startupReason: "APP_STARTUP",
  });

  await extension.startup();

  await extension.awaitMessage("storage-local-data-migrated-and-set");

  const storagePrincipal = ExtensionStorageIDB.getStoragePrincipal(
    extension.extension
  );

  const idbConn = await ExtensionStorageIDB.open(storagePrincipal);

  equal(
    await idbConn.isEmpty(extension.extension),
    false,
    "Data stored in the ExtensionStorageIDB backend as expected"
  );

  equal(
    await IOUtils.exists(`${oldStorageFilename}.corrupt`),
    true,
    "The old json storage should still be available if failed to be read"
  );

  // The extension is still migrated successfully to the new backend if the file from the
  // original json file was corrupted.

  equal(
    Services.prefs.getBoolPref(
      `${IDB_MIGRATED_PREF_BRANCH}.${EXTENSION_ID}`,
      false
    ),
    true,
    `Got the ${IDB_MIGRATED_PREF_BRANCH} preference set to true as expected`
  );

  assertMigrationHistogramCount("success", 1);
  assertMigrationHistogramCount("failure", 0);

  assertTelemetryEvents([
    {
      method: "migrateResult",
      value: EXTENSION_ID,
      extra: {
        backend: "IndexedDB",
        data_migrated: "y",
        has_jsonfile: "y",
        has_olddata: "n",
      },
    },
  ]);

  await extension.unload();
});

// Test that if the data migration fails to store the old data into the IndexedDB backend
// then the expected telemetry histogram is being updated.
add_task(async function test_storage_local_data_migration_failure() {
  const EXTENSION_ID = "extension-data-migration-failure@mozilla.org";

  // Create the file under the expected directory tree.
  const { jsonFile, oldStorageFilename } =
    await createExtensionJSONFileWithData(EXTENSION_ID, {});

  // Store a fake invalid value which is going to fail to be saved into IndexedDB
  // (because it can't be cloned and it is going to raise a DataCloneError), which
  // will trigger a data migration failure that we expect to increment the related
  // telemetry histogram.
  jsonFile.data.set("fake_invalid_key", function () {});

  async function background() {
    await browser.storage.local.set({
      test_key_string_on_JSONFileBackend: "expected-value",
    });
    browser.test.sendMessage("storage-local-data-migrated-and-set");
  }

  clearMigrationHistogram();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
      browser_specific_settings: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
    background,
    // We don't want the (default) startupReason ADDON_INSTALL because
    // that automatically sets the migrated pref and skips migration.
    startupReason: "APP_STARTUP",
  });

  await extension.startup();

  await extension.awaitMessage("storage-local-data-migrated-and-set");

  const storagePrincipal = ExtensionStorageIDB.getStoragePrincipal(
    extension.extension
  );

  const idbConn = await ExtensionStorageIDB.open(storagePrincipal);
  equal(
    await idbConn.isEmpty(extension.extension),
    true,
    "No data stored in the ExtensionStorageIDB backend as expected"
  );
  equal(
    await IOUtils.exists(oldStorageFilename),
    true,
    "The old json storage should still be available if failed to be read"
  );

  await extension.unload();

  assertTelemetryEvents([
    {
      method: "migrateResult",
      value: EXTENSION_ID,
      extra: {
        backend: "JSONFile",
        data_migrated: "n",
        error_name: "DataCloneError",
        has_jsonfile: "y",
        has_olddata: "y",
      },
    },
  ]);

  assertMigrationHistogramCount("success", 0);
  assertMigrationHistogramCount("failure", 1);
});

add_task(async function test_migration_aborted_on_shutdown() {
  const EXTENSION_ID = "test-migration-aborted-on-shutdown@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
      browser_specific_settings: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
    // We don't want the (default) startupReason ADDON_INSTALL because
    // that automatically sets the migrated pref and skips migration.
    startupReason: "APP_STARTUP",
  });

  await extension.startup();

  equal(
    extension.extension.hasShutdown,
    false,
    "The extension is still running"
  );

  await extension.unload();
  equal(extension.extension.hasShutdown, true, "The extension has shutdown");

  // Trigger a data migration after the extension has been unloaded.
  const result = await ExtensionStorageIDB.selectBackend({
    extension: extension.extension,
  });
  Assert.deepEqual(
    result,
    { backendEnabled: false },
    "Expect migration to have been aborted"
  );
  let expected = {
    value: EXTENSION_ID,
    extra: {
      backend: "JSONFile",
      error_name: "DataMigrationAbortedError",
    },
  };

  TelemetryTestUtils.assertEvents([expected], TELEMETRY_EVENTS_FILTER);
  assertMigrateResultGleanEvents([expected]);
});

add_task(async function test_storage_local_data_migration_clear_pref() {
  Services.prefs.clearUserPref(LEAVE_STORAGE_PREF);
  Services.prefs.clearUserPref(LEAVE_UUID_PREF);
  Services.prefs.clearUserPref(ExtensionStorageIDB.BACKEND_ENABLED_PREF);
  await promiseShutdownManager();
  await TelemetryController.testShutdown();
});

add_task(async function setup_quota_manager_testing_prefs() {
  Services.prefs.setBoolPref("dom.quotaManager.testing", true);
  Services.prefs.setIntPref(
    "dom.quotaManager.temporaryStorage.fixedLimit",
    100
  );
  await promiseQuotaManagerServiceReset();
});

add_task(
  // TODO: temporarily disabled because it currently perma-fails on
  // android builds (Bug 1564871)
  { skip_if: () => AppConstants.platform === "android" },
  // eslint-disable-next-line no-use-before-define
  test_quota_exceeded_while_migrating_data
);
async function test_quota_exceeded_while_migrating_data() {
  const EXT_ID = "test-data-migration-stuck@mochi.test";
  const dataSize = 1000 * 1024;

  await createExtensionJSONFileWithData(EXT_ID, {
    data: new Array(dataSize).fill("x").join(""),
  });

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
      browser_specific_settings: { gecko: { id: EXT_ID } },
    },
    background() {
      browser.test.onMessage.addListener(async (msg, dataSize) => {
        if (msg !== "verify-stored-data") {
          return;
        }
        const res = await browser.storage.local.get();
        browser.test.assertEq(
          res.data && res.data.length,
          dataSize,
          "Got the expected data"
        );
        browser.test.sendMessage("verify-stored-data:done");
      });

      browser.test.sendMessage("bg-page:ready");
    },
    // We don't want the (default) startupReason ADDON_INSTALL because
    // that automatically sets the migrated pref and skips migration.
    startupReason: "APP_STARTUP",
  });

  await extension.startup();
  await extension.awaitMessage("bg-page:ready");

  extension.sendMessage("verify-stored-data", dataSize);
  await extension.awaitMessage("verify-stored-data:done");

  await ok(
    !ExtensionStorageIDB.isMigratedExtension(extension),
    "The extension falls back to the JSONFile backend because of the migration failure"
  );
  await extension.unload();

  let expected = {
    value: EXT_ID,
    extra: {
      backend: "JSONFile",
      error_name: "QuotaExceededError",
    },
  };
  TelemetryTestUtils.assertEvents([expected], TELEMETRY_EVENTS_FILTER);
  assertMigrateResultGleanEvents([expected]);

  Services.prefs.clearUserPref("dom.quotaManager.temporaryStorage.fixedLimit");
  await promiseQuotaManagerServiceClear();
  Services.prefs.clearUserPref("dom.quotaManager.testing");
}
