/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the functions of mozIStorageValueArray

add_task(async function setup() {
  getOpenedDatabase().createTable(
    "test",
    "id INTEGER PRIMARY KEY, name TEXT," +
      "number REAL, nuller NULL, blobber BLOB"
  );

  var stmt = createStatement(
    "INSERT INTO test (name, number, blobber) VALUES (?1, ?2, ?3)"
  );
  stmt.bindByIndex(0, "foo");
  stmt.bindByIndex(1, 2.34);
  stmt.bindBlobByIndex(2, [], 0);
  stmt.execute();

  stmt.bindByIndex(0, "");
  stmt.bindByIndex(1, 1.23);
  stmt.bindBlobByIndex(2, [1, 2], 2);
  stmt.execute();

  stmt.reset();
  stmt.finalize();

  registerCleanupFunction(cleanup);
});

add_task(async function test_getIsNull_for_null() {
  var stmt = createStatement("SELECT nuller, blobber FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 1);
  Assert.ok(stmt.executeStep());

  Assert.ok(stmt.getIsNull(0)); // null field
  Assert.ok(stmt.getIsNull(1)); // data is null if size is 0
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_getIsNull_for_non_null() {
  var stmt = createStatement("SELECT name, blobber FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 2);
  Assert.ok(stmt.executeStep());

  Assert.ok(!stmt.getIsNull(0));
  Assert.ok(!stmt.getIsNull(1));
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_value_type_null() {
  var stmt = createStatement("SELECT nuller FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 1);
  Assert.ok(stmt.executeStep());

  Assert.equal(
    Ci.mozIStorageValueArray.VALUE_TYPE_NULL,
    stmt.getTypeOfIndex(0)
  );
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_value_type_integer() {
  var stmt = createStatement("SELECT id FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 1);
  Assert.ok(stmt.executeStep());

  Assert.equal(
    Ci.mozIStorageValueArray.VALUE_TYPE_INTEGER,
    stmt.getTypeOfIndex(0)
  );
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_value_type_float() {
  var stmt = createStatement("SELECT number FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 1);
  Assert.ok(stmt.executeStep());

  Assert.equal(
    Ci.mozIStorageValueArray.VALUE_TYPE_FLOAT,
    stmt.getTypeOfIndex(0)
  );
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_value_type_text() {
  var stmt = createStatement("SELECT name FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 1);
  Assert.ok(stmt.executeStep());

  Assert.equal(
    Ci.mozIStorageValueArray.VALUE_TYPE_TEXT,
    stmt.getTypeOfIndex(0)
  );
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_value_type_blob() {
  var stmt = createStatement("SELECT blobber FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 2);
  Assert.ok(stmt.executeStep());

  Assert.equal(
    Ci.mozIStorageValueArray.VALUE_TYPE_BLOB,
    stmt.getTypeOfIndex(0)
  );
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_numEntries_one() {
  var stmt = createStatement("SELECT blobber FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 2);
  Assert.ok(stmt.executeStep());

  Assert.equal(1, stmt.numEntries);
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_numEntries_all() {
  var stmt = createStatement("SELECT * FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 2);
  Assert.ok(stmt.executeStep());

  Assert.equal(5, stmt.numEntries);
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_getInt() {
  var stmt = createStatement("SELECT id FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 2);
  Assert.ok(stmt.executeStep());

  Assert.equal(2, stmt.getInt32(0));
  Assert.equal(2, stmt.getInt64(0));
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_getDouble() {
  var stmt = createStatement("SELECT number FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 2);
  Assert.ok(stmt.executeStep());

  Assert.equal(1.23, stmt.getDouble(0));
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_getUTF8String() {
  var stmt = createStatement("SELECT name FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 1);
  Assert.ok(stmt.executeStep());

  Assert.equal("foo", stmt.getUTF8String(0));
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_getString() {
  var stmt = createStatement("SELECT name FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 2);
  Assert.ok(stmt.executeStep());

  Assert.equal("", stmt.getString(0));
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_getBlob() {
  var stmt = createStatement("SELECT blobber FROM test WHERE id = ?1");
  stmt.bindByIndex(0, 2);
  Assert.ok(stmt.executeStep());

  var count = { value: 0 };
  var arr = { value: null };
  stmt.getBlob(0, count, arr);
  Assert.equal(2, count.value);
  Assert.equal(1, arr.value[0]);
  Assert.equal(2, arr.value[1]);
  stmt.reset();
  stmt.finalize();
});
