"use strict";

const { ExtensionStorageIDB } = ChromeUtils.import(
  "resource://gre/modules/ExtensionStorageIDB.jsm"
);

PromiseTestUtils.allowMatchingRejectionsGlobally(
  /WebExtension context not found/
);

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

// The storage API in content scripts should behave identical to the storage API
// in background pages.

AddonTestUtils.init(this);

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_contentscript_storage_local_file_backend() {
  return runWithPrefs([[ExtensionStorageIDB.BACKEND_ENABLED_PREF, false]], () =>
    test_contentscript_storage("local")
  );
});

add_task(async function test_contentscript_storage_local_idb_backend() {
  return runWithPrefs([[ExtensionStorageIDB.BACKEND_ENABLED_PREF, true]], () =>
    test_contentscript_storage("local")
  );
});

add_task(async function test_contentscript_storage_local_idb_no_bytes_in_use() {
  return runWithPrefs([[ExtensionStorageIDB.BACKEND_ENABLED_PREF, true]], () =>
    test_contentscript_storage_area_no_bytes_in_use("local")
  );
});
