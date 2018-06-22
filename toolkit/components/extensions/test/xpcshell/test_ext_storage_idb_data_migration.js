/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// This test file verifies various scenarios related to the data migration
// from the JSONFile backend to the IDB backend.

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionStorage.jsm");
ChromeUtils.import("resource://gre/modules/ExtensionStorageIDB.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  OS: "resource://gre/modules/osfile.jsm",
});

const {IDB_MIGRATE_RESULT_HISTOGRAM} = ExtensionStorageIDB;
const CATEGORIES = ["success", "failure"];

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

  equal(histogram.snapshot().counts[CATEGORIES.indexOf(category)], expectedCount,
        `Got the expected count on category "${category}" for histogram ${IDB_MIGRATE_RESULT_HISTOGRAM}`);
}

add_task(async function setup() {
  Services.prefs.setBoolPref(ExtensionStorageIDB.BACKEND_ENABLED_PREF, true);
});

// Test that the old data is migrated successfully to the new storage backend
// and that the original JSONFile is being removed.
add_task(async function test_storage_local_data_migration() {
  const EXTENSION_ID = "extension-to-be-migrated@mozilla.org";

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

  const storagePrincipal = ExtensionStorageIDB.getStoragePrincipal(extension.extension);

  const idbConn = await ExtensionStorageIDB.open(storagePrincipal);

  equal(await idbConn.isEmpty(extension.extension), false,
        "Data stored in the ExtensionStorageIDB backend as expected");

  equal(await OS.File.exists(oldStorageFilename), false,
        "The old json storage file should have been removed");

  assertMigrationHistogramCount("success", 1);
  assertMigrationHistogramCount("failure", 0);

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
  assertMigrationHistogramCount("success", 1);
  assertMigrationHistogramCount("failure", 0);

  await extension.unload();
});

add_task(function test_storage_local_data_migration_clear_pref() {
  Services.prefs.clearUserPref(ExtensionStorageIDB.BACKEND_ENABLED_PREF);
});
