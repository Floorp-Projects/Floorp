/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the openSpecialDatabase function of mozIStorageService.

add_task(async function test_invalid_storage_key() {
  try {
    Services.storage.openSpecialDatabase("abcd");
    do_throw("We should not get here!");
  } catch (e) {
    print(e);
    print("e.result is " + e.result);
    Assert.equal(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
});

add_task(async function test_memdb_close_clone_fails() {
  for (const name of [null, "foo"]) {
    const db = Services.storage.openSpecialDatabase("memory", name);
    db.close();
    expectError(Cr.NS_ERROR_NOT_INITIALIZED, () => db.clone());
  }
});

add_task(async function test_memdb_no_file_on_disk() {
  for (const name of [null, "foo"]) {
    const db = Services.storage.openSpecialDatabase("memory", name);
    db.close();
    for (const dirKey of ["CurWorkD", "ProfD"]) {
      const dir = Services.dirsvc.get(dirKey, Ci.nsIFile);
      const file = dir.clone();
      file.append(!name ? ":memory:" : "file:foo?mode=memory&cache=shared");
      Assert.ok(!file.exists());
    }
  }
});

add_task(async function test_memdb_sharing() {
  for (const name of [null, "foo"]) {
    const db = Services.storage.openSpecialDatabase("memory", name);
    db.executeSimpleSQL("CREATE TABLE test(name TEXT)");
    const db2 = Services.storage.openSpecialDatabase("memory", name);
    Assert.ok(!!name == db2.tableExists("test"));
    db.close();
    db2.close();
  }
});
