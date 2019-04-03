/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// This test file verifies various scenarios related to the data migration
// from the JSONFile backend to the IDB backend.

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

const {ExtensionStorage} = ChromeUtils.import("resource://gre/modules/ExtensionStorage.jsm");
const {TelemetryController} = ChromeUtils.import("resource://gre/modules/TelemetryController.jsm");

const {
  ExtensionStorageIDB,
} = ChromeUtils.import("resource://gre/modules/ExtensionStorageIDB.jsm");

const {
  getTrimmedString,
} = ChromeUtils.import("resource://gre/modules/ExtensionTelemetry.jsm", null);

XPCOMUtils.defineLazyModuleGetters(this, {
  OS: "resource://gre/modules/osfile.jsm",
});

const {
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

const {
  IDB_MIGRATED_PREF_BRANCH,
  IDB_MIGRATE_RESULT_HISTOGRAM,
} = ExtensionStorageIDB;
const CATEGORIES = ["success", "failure"];
const EVENT_CATEGORY = "extensions.data";
const EVENT_OBJECT = "storageLocal";
const EVENT_METHODS = ["migrateResult"];
const LEAVE_STORAGE_PREF = "extensions.webextensions.keepStorageOnUninstall";
const LEAVE_UUID_PREF = "extensions.webextensions.keepUuidOnUninstall";

async function createExtensionJSONFileWithData(extensionId, data) {
  await ExtensionStorage.set(extensionId, data);
  const jsonFile = await ExtensionStorage.getFile(extensionId);
  await jsonFile._save();
  const oldStorageFilename = ExtensionStorage.getStorageFile(extensionId);
  equal(await OS.File.exists(oldStorageFilename), true, "The old json file has been created");

  return {jsonFile, oldStorageFilename};
}

function clearMigrationHistogram() {
  const histogram = Services.telemetry.getHistogramById(IDB_MIGRATE_RESULT_HISTOGRAM);
  histogram.clear();
  equal(histogram.snapshot().sum, 0,
        `No data recorded for histogram ${IDB_MIGRATE_RESULT_HISTOGRAM}`);
}

function assertMigrationHistogramCount(category, expectedCount) {
  const histogram = Services.telemetry.getHistogramById(IDB_MIGRATE_RESULT_HISTOGRAM);

  equal(histogram.snapshot().values[CATEGORIES.indexOf(category)], expectedCount,
        `Got the expected count on category "${category}" for histogram ${IDB_MIGRATE_RESULT_HISTOGRAM}`);
}

function assertTelemetryEvents(extensionId, expectedEvents) {
  const snapshot = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true);

  ok(snapshot.parent && snapshot.parent.length > 0, "Got parent telemetry events in the snapshot");

  const migrateEvents = snapshot.parent.filter(([timestamp, category, method, object, value]) => {
    return category === EVENT_CATEGORY &&
      EVENT_METHODS.includes(method) &&
      object === EVENT_OBJECT &&
      value === extensionId;
  }).map(event => {
    return {method: event[2], extra: event[5]};
  });

  Assert.deepEqual(migrateEvents, expectedEvents, "Got the expected telemetry events");
}

add_task(async function setup() {
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
    "test_key_string": "test_value1",
    "test_key_number": 1000,
    "test_nested_data": {
      "nested_key": true,
    },
  };

  // Store some fake data in the storage.local file backend before starting the extension.
  const {oldStorageFilename} = await createExtensionJSONFileWithData(EXTENSION_ID, data);

  async function background() {
    const storedData = await browser.storage.local.get();

    browser.test.assertEq("test_value1", storedData.test_key_string,
                          "Got the expected data after the storage.local data migration");
    browser.test.assertEq(1000, storedData.test_key_number,
                          "Got the expected data after the storage.local data migration");
    browser.test.assertEq(true, storedData.test_nested_data.nested_key,
                          "Got the expected data after the storage.local data migration");

    browser.test.sendMessage("storage-local-data-migrated");
  }

  clearMigrationHistogram();

  let extensionDefinition = {
    useAddonManager: "temporary",
    manifest: {
      permissions: ["storage"],
      applications: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionDefinition);

  await extension.startup();

  await extension.awaitMessage("storage-local-data-migrated");

  const storagePrincipal = ExtensionStorageIDB.getStoragePrincipal(extension.extension);

  const idbConn = await ExtensionStorageIDB.open(storagePrincipal);

  equal(await idbConn.isEmpty(extension.extension), false,
        "Data stored in the ExtensionStorageIDB backend as expected");

  equal(await OS.File.exists(oldStorageFilename), false,
        "The old json storage file name should not exist anymore");

  equal(await OS.File.exists(`${oldStorageFilename}.migrated`), true,
        "The old json storage file name should have been renamed as .migrated");

  equal(Services.prefs.getBoolPref(`${IDB_MIGRATED_PREF_BRANCH}.${EXTENSION_ID}`, false),
        true, `Got the ${IDB_MIGRATED_PREF_BRANCH} preference set to true as expected`);

  assertMigrationHistogramCount("success", 1);
  assertMigrationHistogramCount("failure", 0);

  assertTelemetryEvents(EXTENSION_ID, [
    {
      method: "migrateResult",
      extra: {
        backend: "IndexedDB",
        data_migrated: "y",
        has_jsonfile: "y",
        has_olddata: "y",
      },
    },
  ]);

  await extension.unload();

  equal(Services.prefs.getBoolPref(`${IDB_MIGRATED_PREF_BRANCH}.${EXTENSION_ID}`, false),
        true, `${IDB_MIGRATED_PREF_BRANCH} should still be true on keepStorageOnUninstall=true`);

  // Re-install the extension and check that no telemetry events are being sent
  // for an already migrated extension.
  extension = ExtensionTestUtils.loadExtension(extensionDefinition);

  await extension.startup();

  await extension.awaitMessage("storage-local-data-migrated");

  // The histogram values are unmodified.
  assertMigrationHistogramCount("success", 1);
  assertMigrationHistogramCount("failure", 0);

  // No new telemetry events recorded for the extension.
  const snapshot = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, true);
  const filterByCategory = ([timestamp, category]) => category === EVENT_CATEGORY;

  ok(!snapshot.parent || snapshot.parent.filter(filterByCategory).length === 0,
     "No telemetry events should be recorded for an already migrated extension");

  Services.prefs.setBoolPref(LEAVE_STORAGE_PREF, false);
  Services.prefs.setBoolPref(LEAVE_UUID_PREF, false);

  await extension.unload();

  equal(Services.prefs.getPrefType(`${IDB_MIGRATED_PREF_BRANCH}.${EXTENSION_ID}`),
        Services.prefs.PREF_INVALID,
        `Got the ${IDB_MIGRATED_PREF_BRANCH} preference has been cleared on addon uninstall`);
});

// Test that the extensionId included in the telemetry event is being trimmed down to 80 chars
// as expected.
add_task(async function test_extensionId_trimmed_in_telemetry_event() {
  // Generated extensionId in email-like format, longer than 80 chars.
  const EXTENSION_ID = `long.extension.id@${Array(80).fill("a").join("")}`;

  const data = {"test_key_string": "test_value"};

  // Store some fake data in the storage.local file backend before starting the extension.
  await createExtensionJSONFileWithData(EXTENSION_ID, data);

  async function background() {
    const storedData = await browser.storage.local.get("test_key_string");

    browser.test.assertEq("test_value", storedData.test_key_string,
                          "Got the expected data after the storage.local data migration");

    browser.test.sendMessage("storage-local-data-migrated");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
      applications: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
    background,
  });

  await extension.startup();

  await extension.awaitMessage("storage-local-data-migrated");

  const expectedTrimmedExtensionId = getTrimmedString(EXTENSION_ID);

  equal(expectedTrimmedExtensionId.length, 80, "The trimmed version of the extensionId should be 80 chars long");

  assertTelemetryEvents(expectedTrimmedExtensionId, [
    {
      method: "migrateResult",
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
// - the old file is being renamed (with the `.corrupted` suffix that JSONFile.jsm
//   adds when it fails to load the data file) and still available on disk.
add_task(async function test_storage_local_corrupted_data_migration() {
  const EXTENSION_ID = "extension-corrupted-data-migration@mozilla.org";

  const invalidData = `{"test_key_string": "test_value1"`;
  const oldStorageFilename = ExtensionStorage.getStorageFile(EXTENSION_ID);

  const profileDir = OS.Constants.Path.profileDir;
  await OS.File.makeDir(OS.Path.join(profileDir, "browser-extension-data", EXTENSION_ID),
                        {from: profileDir, ignoreExisting: true});

  // Write the json file with some invalid data.
  await OS.File.writeAtomic(oldStorageFilename, invalidData, {flush: true});
  equal(await OS.File.read(oldStorageFilename, {encoding: "utf-8"}),
        invalidData, "The old json file has been overwritten with invalid data");

  async function background() {
    const storedData = await browser.storage.local.get();

    browser.test.assertEq(Object.keys(storedData).length, 0,
                          "No data should be found on invalid data migration");

    await browser.storage.local.set({"test_key_string_on_IDBBackend": "expected-value"});

    browser.test.sendMessage("storage-local-data-migrated-and-set");
  }

  clearMigrationHistogram();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
      applications: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
    background,
  });

  await extension.startup();

  await extension.awaitMessage("storage-local-data-migrated-and-set");

  const storagePrincipal = ExtensionStorageIDB.getStoragePrincipal(extension.extension);

  const idbConn = await ExtensionStorageIDB.open(storagePrincipal);

  equal(await idbConn.isEmpty(extension.extension), false,
        "Data stored in the ExtensionStorageIDB backend as expected");

  equal(await OS.File.exists(`${oldStorageFilename}.corrupt`), true,
        "The old json storage should still be available if failed to be read");

  // The extension is still migrated successfully to the new backend if the file from the
  // original json file was corrupted.

  equal(Services.prefs.getBoolPref(`${IDB_MIGRATED_PREF_BRANCH}.${EXTENSION_ID}`, false),
        true, `Got the ${IDB_MIGRATED_PREF_BRANCH} preference set to true as expected`);

  assertMigrationHistogramCount("success", 1);
  assertMigrationHistogramCount("failure", 0);

  assertTelemetryEvents(EXTENSION_ID, [
    {
      method: "migrateResult",
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
  const {
    jsonFile,
    oldStorageFilename,
  } = await createExtensionJSONFileWithData(EXTENSION_ID, {});

  // Store a fake invalid value which is going to fail to be saved into IndexedDB
  // (because it can't be cloned and it is going to raise a DataCloneError), which
  // will trigger a data migration failure that we expect to increment the related
  // telemetry histogram.
  jsonFile.data.set("fake_invalid_key", new Error());

  async function background() {
    await browser.storage.local.set({"test_key_string_on_JSONFileBackend": "expected-value"});
    browser.test.sendMessage("storage-local-data-migrated-and-set");
  }

  clearMigrationHistogram();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
      applications: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
    },
    background,
  });

  await extension.startup();

  await extension.awaitMessage("storage-local-data-migrated-and-set");

  const storagePrincipal = ExtensionStorageIDB.getStoragePrincipal(extension.extension);

  const idbConn = await ExtensionStorageIDB.open(storagePrincipal);
  equal(await idbConn.isEmpty(extension.extension), true,
        "No data stored in the ExtensionStorageIDB backend as expected");
  equal(await OS.File.exists(oldStorageFilename), true,
        "The old json storage should still be available if failed to be read");

  await extension.unload();

  assertTelemetryEvents(EXTENSION_ID, [
    {
      method: "migrateResult",
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

add_task(async function test_storage_local_data_migration_clear_pref() {
  Services.prefs.clearUserPref(LEAVE_STORAGE_PREF);
  Services.prefs.clearUserPref(LEAVE_UUID_PREF);
  Services.prefs.clearUserPref(ExtensionStorageIDB.BACKEND_ENABLED_PREF);
  await promiseShutdownManager();
  await TelemetryController.testShutdown();
});
