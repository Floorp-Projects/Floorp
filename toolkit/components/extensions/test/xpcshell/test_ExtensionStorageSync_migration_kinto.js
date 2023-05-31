/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Import the rust-based and kinto-based implementations
const { extensionStorageSync: rustImpl } = ChromeUtils.import(
  "resource://gre/modules/ExtensionStorageSync.jsm"
);
const { extensionStorageSyncKinto: kintoImpl } = ChromeUtils.import(
  "resource://gre/modules/ExtensionStorageSyncKinto.jsm"
);

Services.prefs.setBoolPref("webextensions.storage.sync.kinto", false);

add_task(async function test_sync_migration() {
  // There's no good reason to perform this test via test extensions - we just
  // call the underlying APIs directly.

  // Set some stuff using the kinto-based impl.
  let e1 = { id: "test@mozilla.com" };
  let c1 = { extension: e1, callOnClose() {} };
  await kintoImpl.set(e1, { foo: "bar" }, c1);

  let e2 = { id: "test-2@mozilla.com" };
  let c2 = { extension: e2, callOnClose() {} };
  await kintoImpl.set(e2, { second: "2nd" }, c2);

  let e3 = { id: "test-3@mozilla.com" };
  let c3 = { extension: e3, callOnClose() {} };

  // And all the data should be magically migrated.
  Assert.deepEqual(await rustImpl.get(e1, "foo", c1), { foo: "bar" });
  Assert.deepEqual(await rustImpl.get(e2, null, c2), { second: "2nd" });

  // Sanity check we really are doing what we think we are - set a value in our
  // new one, it should not be reflected by kinto.
  await rustImpl.set(e3, { third: "3rd" }, c3);
  Assert.deepEqual(await rustImpl.get(e3, null, c3), { third: "3rd" });
  Assert.deepEqual(await kintoImpl.get(e3, null, c3), {});
  // cleanup.
  await kintoImpl.clear(e1, c1);
  await kintoImpl.clear(e2, c2);
  await kintoImpl.clear(e3, c3);
  await rustImpl.clear(e1, c1);
  await rustImpl.clear(e2, c2);
  await rustImpl.clear(e3, c3);
});

// It would be great to have failure tests, but that seems impossible to have
// in automated tests given the conditions under which we migrate - it would
// basically require us to arrange for zero free disk space or to somehow
// arrange for sqlite to see an io error. Specially crafted "corrupt"
// sqlite files doesn't help because that file must not exist for us to even
// attempt migration.
//
// But - what we can test is that if .migratedOk on the new impl ever goes to
// false we delegate correctly.
add_task(async function test_sync_migration_delgates() {
  let e1 = { id: "test@mozilla.com" };
  let c1 = { extension: e1, callOnClose() {} };
  await kintoImpl.set(e1, { foo: "bar" }, c1);

  // We think migration went OK - `get` shouldn't see kinto.
  Assert.deepEqual(rustImpl.get(e1, null, c1), {});

  info(
    "Setting migration failure flag to ensure we delegate to kinto implementation"
  );
  rustImpl.migrationOk = false;
  // get should now be seeing kinto.
  Assert.deepEqual(await rustImpl.get(e1, null, c1), { foo: "bar" });
  // check everything else delegates.

  await rustImpl.set(e1, { foo: "foo" }, c1);
  Assert.deepEqual(await kintoImpl.get(e1, null, c1), { foo: "foo" });

  Assert.equal(await rustImpl.getBytesInUse(e1, null, c1), 8);

  await rustImpl.remove(e1, "foo", c1);
  Assert.deepEqual(await kintoImpl.get(e1, null, c1), {});

  await rustImpl.set(e1, { foo: "foo" }, c1);
  Assert.deepEqual(await kintoImpl.get(e1, null, c1), { foo: "foo" });
  await rustImpl.clear(e1, c1);
  Assert.deepEqual(await kintoImpl.get(e1, null, c1), {});
});
