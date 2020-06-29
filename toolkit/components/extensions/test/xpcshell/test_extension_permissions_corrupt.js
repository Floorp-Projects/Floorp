"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

const gDatabase = do_get_profile().clone();
gDatabase.append("extension-store");
gDatabase.append("data.mdb");

const gCorrupt = do_get_profile().clone();
gCorrupt.append("extension-store.corrupt");

const TEST_ID = "test@tests.mozilla.org";

async function test_corrupt_permission_database() {
  // This makes sure the db is initialized
  await ExtensionPermissions.add(TEST_ID, {
    permissions: ["test-permission"],
    origins: [],
  });
  await ExtensionPermissions._uninit();

  Assert.ok(await OS.File.exists(gDatabase.path), "Database was created");

  // Write garbage to the db
  let bytes = await OS.File.read(gDatabase.path);
  for (let i = 0; i < 100; i++) {
    bytes[i] = 0;
  }
  await OS.File.writeAtomic(gDatabase.path, bytes, {
    flush: true,
  });

  // Requesting a read will re-initialize the database
  let permissions = await ExtensionPermissions._get(TEST_ID);

  Assert.deepEqual(
    permissions,
    { permissions: [], origins: [] },
    "Database was recreated"
  );
}

// Tests that in the case of a corrupt database we correctly backup the file to
// a folder, create a new one and use it.
add_task(async function test_corrupt_permission_database_recovers() {
  Assert.ok(!(await OS.File.exists(gCorrupt.path)), "No corrupt files");

  await test_corrupt_permission_database();
  Assert.ok(await OS.File.exists(gCorrupt.path), "Database was backed up.");

  // Testing that you can corrupt the database multiple times
  await test_corrupt_permission_database();

  await OS.File.removeDir(gCorrupt.path);
});
