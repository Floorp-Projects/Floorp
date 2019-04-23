/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {KeyValueService} = ChromeUtils.import("resource://gre/modules/kvstore.jsm");
const {OS} = ChromeUtils.import("resource://gre/modules/osfile.jsm");

function run_test() {
  do_get_profile();
  run_next_test();
}

async function makeDatabaseDir(name) {
  const databaseDir = OS.Path.join(OS.Constants.Path.profileDir, name);
  await OS.File.makeDir(databaseDir, { from: OS.Constants.Path.profileDir });
  return databaseDir;
}

const gKeyValueService =
  Cc["@mozilla.org/key-value-service;1"].getService(Ci.nsIKeyValueService);

add_task(async function getService() {
  Assert.ok(gKeyValueService);
});

add_task(async function getOrCreate() {
  const databaseDir = await makeDatabaseDir("getOrCreate");
  const database = await KeyValueService.getOrCreate(databaseDir, "db");
  Assert.ok(database);

  // Test creating a database with a nonexistent path.
  const nonexistentDir = OS.Path.join(OS.Constants.Path.profileDir, "nonexistent");
  await Assert.rejects(KeyValueService.getOrCreate(nonexistentDir, "db"), /DirectoryDoesNotExistError/);

  // Test creating a database with a non-normalized but fully-qualified path.
  let nonNormalizedDir = await makeDatabaseDir("non-normalized");
  nonNormalizedDir = OS.Path.join(nonNormalizedDir, "..", ".", "non-normalized");
  Assert.ok(await KeyValueService.getOrCreate(nonNormalizedDir, "db"));
});

add_task(async function putGetHasDelete() {
  const databaseDir = await makeDatabaseDir("putGetHasDelete");
  const database = await KeyValueService.getOrCreate(databaseDir, "db");

  // Getting key/value pairs that don't exist (yet) returns default values
  // or null, depending on whether you specify a default value.
  Assert.strictEqual(await database.get("int-key", 1), 1);
  Assert.strictEqual(await database.get("double-key", 1.1), 1.1);
  Assert.strictEqual(await database.get("string-key", ""), "");
  Assert.strictEqual(await database.get("bool-key", false), false);
  Assert.strictEqual(await database.get("int-key"), null);
  Assert.strictEqual(await database.get("double-key"), null);
  Assert.strictEqual(await database.get("string-key"), null);
  Assert.strictEqual(await database.get("bool-key"), null);

  // The put method succeeds without returning a value.
  Assert.strictEqual(await database.put("int-key", 1234), undefined);
  Assert.strictEqual(await database.put("double-key", 56.78), undefined);
  Assert.strictEqual(await database.put("string-key", "Héllo, wőrld!"), undefined);
  Assert.strictEqual(await database.put("bool-key", true), undefined);

  // Getting key/value pairs that exist returns the expected values.
  Assert.strictEqual(await database.get("int-key", 1), 1234);
  Assert.strictEqual(await database.get("double-key", 1.1), 56.78);
  Assert.strictEqual(await database.get("string-key", ""), "Héllo, wőrld!");
  Assert.strictEqual(await database.get("bool-key", false), true);
  Assert.strictEqual(await database.get("int-key"), 1234);
  Assert.strictEqual(await database.get("double-key"), 56.78);
  Assert.strictEqual(await database.get("string-key"), "Héllo, wőrld!");
  Assert.strictEqual(await database.get("bool-key"), true);

  // The has() method works as expected for both existing and non-existent keys.
  Assert.strictEqual(await database.has("int-key"), true);
  Assert.strictEqual(await database.has("double-key"), true);
  Assert.strictEqual(await database.has("string-key"), true);
  Assert.strictEqual(await database.has("bool-key"), true);
  Assert.strictEqual(await database.has("nonexistent-key"), false);

  // The delete() method succeeds without returning a value.
  Assert.strictEqual(await database.delete("int-key"), undefined);
  Assert.strictEqual(await database.delete("double-key"), undefined);
  Assert.strictEqual(await database.delete("string-key"), undefined);
  Assert.strictEqual(await database.delete("bool-key"), undefined);

  // The has() method works as expected for a deleted key.
  Assert.strictEqual(await database.has("int-key"), false);
  Assert.strictEqual(await database.has("double-key"), false);
  Assert.strictEqual(await database.has("string-key"), false);
  Assert.strictEqual(await database.has("bool-key"), false);

  // Getting key/value pairs that were deleted returns default values.
  Assert.strictEqual(await database.get("int-key", 1), 1);
  Assert.strictEqual(await database.get("double-key", 1.1), 1.1);
  Assert.strictEqual(await database.get("string-key", ""), "");
  Assert.strictEqual(await database.get("bool-key", false), false);
  Assert.strictEqual(await database.get("int-key"), null);
  Assert.strictEqual(await database.get("double-key"), null);
  Assert.strictEqual(await database.get("string-key"), null);
  Assert.strictEqual(await database.get("bool-key"), null);
});

add_task(async function largeNumbers() {
  const databaseDir = await makeDatabaseDir("largeNumbers");
  const database = await KeyValueService.getOrCreate(databaseDir, "db");

  const MAX_INT_VARIANT = Math.pow(2, 31) - 1;
  const MIN_DOUBLE_VARIANT = Math.pow(2, 31);

  await database.put("max-int-variant", MAX_INT_VARIANT);
  await database.put("min-double-variant", MIN_DOUBLE_VARIANT);
  await database.put("max-safe-integer", Number.MAX_SAFE_INTEGER);
  await database.put("min-safe-integer", Number.MIN_SAFE_INTEGER);
  await database.put("max-value", Number.MAX_VALUE);
  await database.put("min-value", Number.MIN_VALUE);

  Assert.strictEqual(await database.get("max-int-variant"), MAX_INT_VARIANT);
  Assert.strictEqual(await database.get("min-double-variant"), MIN_DOUBLE_VARIANT);
  Assert.strictEqual(await database.get("max-safe-integer"), Number.MAX_SAFE_INTEGER);
  Assert.strictEqual(await database.get("min-safe-integer"), Number.MIN_SAFE_INTEGER);
  Assert.strictEqual(await database.get("max-value"), Number.MAX_VALUE);
  Assert.strictEqual(await database.get("min-value"), Number.MIN_VALUE);
});

add_task(async function extendedCharacterKey() {
  const databaseDir = await makeDatabaseDir("extendedCharacterKey");
  const database = await KeyValueService.getOrCreate(databaseDir, "db");

  // Ensure that we can use extended character (i.e. non-ASCII) strings as keys.

  await database.put("Héllo, wőrld!", 1);
  Assert.strictEqual(await database.has("Héllo, wőrld!"), true);
  Assert.strictEqual(await database.get("Héllo, wőrld!"), 1);

  const enumerator = await database.enumerate();
  const { key } = enumerator.getNext();
  Assert.strictEqual(key, "Héllo, wőrld!");

  await database.delete("Héllo, wőrld!");
});

add_task(async function clear() {
  const databaseDir = await makeDatabaseDir("clear");
  const database = await KeyValueService.getOrCreate(databaseDir, "db");

  await database.put("int-key", 1234);
  await database.put("double-key", 56.78);
  await database.put("string-key", "Héllo, wőrld!");
  await database.put("bool-key", true);

  Assert.strictEqual(await database.clear(), undefined);
  Assert.strictEqual(await database.has("int-key"), false);
  Assert.strictEqual(await database.has("double-key"), false);
  Assert.strictEqual(await database.has("string-key"), false);
  Assert.strictEqual(await database.has("bool-key"), false);
});

add_task(async function writeManyFailureCases() {
  const databaseDir = await makeDatabaseDir("writeManyFailureCases");
  const database = await KeyValueService.getOrCreate(databaseDir, "db");

  Assert.throws(() => database.writeMany(), /unexpected argument/);
  Assert.throws(() => database.writeMany("foo"), /unexpected argument/);
  Assert.throws(() => database.writeMany(["foo"]), /unexpected argument/);
});

add_task(async function writeManyPutOnly() {
  const databaseDir = await makeDatabaseDir("writeMany");
  const database = await KeyValueService.getOrCreate(databaseDir, "db");

  async function test_helper(pairs) {
    Assert.strictEqual(await database.writeMany(pairs), undefined);
    Assert.strictEqual(await database.get("int-key"), 1234);
    Assert.strictEqual(await database.get("double-key"), 56.78);
    Assert.strictEqual(await database.get("string-key"), "Héllo, wőrld!");
    Assert.strictEqual(await database.get("bool-key"), true);
    await database.clear();
  }

  // writeMany with an empty object is OK
  Assert.strictEqual(await database.writeMany({}), undefined);

  // writeMany with an object
  const pairs = {
    "int-key": 1234,
    "double-key": 56.78,
    "string-key": "Héllo, wőrld!",
    "bool-key": true,
  };
  await test_helper(pairs);

  // writeMany with an array of pairs
  const arrayPairs = [
    ["int-key", 1234],
    ["double-key", 56.78],
    ["string-key", "Héllo, wőrld!"],
    ["bool-key", true],
  ];
  await test_helper(arrayPairs);

  // writeMany with a key/value generator
  function* pairMaker() {
    yield ["int-key", 1234];
    yield ["double-key", 56.78];
    yield ["string-key", "Héllo, wőrld!"];
    yield ["bool-key", true];
  }
  await test_helper(pairMaker());

  // writeMany with a map
  const mapPairs = new Map(arrayPairs);
  await test_helper(mapPairs);
});

add_task(async function writeManyDeleteOnly() {
  const databaseDir = await makeDatabaseDir("writeManyDeletesOnly");
  const database = await KeyValueService.getOrCreate(databaseDir, "db");

  // writeMany with an object
  const pairs = {
    "int-key": 1234,
    "double-key": 56.78,
    "string-key": "Héllo, wőrld!",
    "bool-key": true,
  };

  async function test_helper(deletes) {
    Assert.strictEqual(await database.writeMany(pairs), undefined);
    Assert.strictEqual(await database.writeMany(deletes), undefined);
    Assert.strictEqual(await database.get("int-key"), null);
    Assert.strictEqual(await database.get("double-key"), null);
    Assert.strictEqual(await database.get("string-key"), null);
    Assert.strictEqual(await database.get("bool-key"), null);
  }

  // writeMany with an empty object is OK
  Assert.strictEqual(await database.writeMany({}), undefined);

  // writeMany with an object
  await test_helper({
    "int-key": null,
    "double-key": null,
    "string-key": null,
    "bool-key": null,
  });

  // writeMany with an array of pairs
  const arrayPairs = [
    ["int-key", null],
    ["double-key", null],
    ["string-key", null],
    ["bool-key", null],
  ];
  await test_helper(arrayPairs);

  // writeMany with a key/value generator
  function* pairMaker() {
    yield ["int-key", null];
    yield ["double-key", null];
    yield ["string-key", null];
    yield ["bool-key", null];
  }
  await test_helper(pairMaker());

  // writeMany with a map
  const mapPairs = new Map(arrayPairs);
  await test_helper(mapPairs);
});

add_task(async function writeManyPutDelete() {
  const databaseDir = await makeDatabaseDir("writeManyPutDelete");
  const database = await KeyValueService.getOrCreate(databaseDir, "db");

  await database.writeMany([
    ["key1", "val1"],
    ["key3", "val3"],
    ["key4", "val4"],
    ["key5", "val5"],
  ]);

  await database.writeMany([
    ["key2", "val2"],
    ["key4", null],
    ["key5", null],
  ]);

  Assert.strictEqual(await database.get("key1"), "val1");
  Assert.strictEqual(await database.get("key2"), "val2");
  Assert.strictEqual(await database.get("key3"), "val3");
  Assert.strictEqual(await database.get("key4"), null);
  Assert.strictEqual(await database.get("key5"), null);

  await database.clear();

  await database.writeMany([
    ["key1", "val1"],
    ["key1", null],
    ["key1", "val11"],
    ["key1", null],
    ["key2", null],
    ["key2", "val2"],
  ]);

  Assert.strictEqual(await database.get("key1"), null);
  Assert.strictEqual(await database.get("key2"), "val2");
});

add_task(async function getOrCreateNamedDatabases() {
  const databaseDir = await makeDatabaseDir("getOrCreateNamedDatabases");

  let fooDB = await KeyValueService.getOrCreate(databaseDir, "foo");
  Assert.ok(fooDB, "retrieval of first named database works");

  let barDB = await KeyValueService.getOrCreate(databaseDir, "bar");
  Assert.ok(barDB, "retrieval of second named database works");

  let bazDB = await KeyValueService.getOrCreate(databaseDir, "baz");
  Assert.ok(bazDB, "retrieval of third named database works");

  // Key/value pairs that are put into a database don't exist in others.
  await bazDB.put("key", 1);
  Assert.ok(!(await fooDB.has("key")), "the foo DB still doesn't have the key");
  await fooDB.put("key", 2);
  Assert.ok(!(await barDB.has("key")), "the bar DB still doesn't have the key");
  await barDB.put("key", 3);
  Assert.strictEqual(await bazDB.get("key", 0), 1, "the baz DB has its KV pair");
  Assert.strictEqual(await fooDB.get("key", 0), 2, "the foo DB has its KV pair");
  Assert.strictEqual(await barDB.get("key", 0), 3, "the bar DB has its KV pair");

  // Key/value pairs that are deleted from a database still exist in other DBs.
  await bazDB.delete("key");
  Assert.strictEqual(await fooDB.get("key", 0), 2, "the foo DB still has its KV pair");
  await fooDB.delete("key");
  Assert.strictEqual(await barDB.get("key", 0), 3, "the bar DB still has its KV pair");
  await barDB.delete("key");
});

add_task(async function enumeration() {
  const databaseDir = await makeDatabaseDir("enumeration");
  const database = await KeyValueService.getOrCreate(databaseDir, "db");

  await database.put("int-key", 1234);
  await database.put("double-key", 56.78);
  await database.put("string-key", "Héllo, wőrld!");
  await database.put("bool-key", true);

  async function test(fromKey, toKey, pairs) {
    const enumerator = await database.enumerate(fromKey, toKey);

    for (const pair of pairs) {
      Assert.strictEqual(enumerator.hasMoreElements(), true);
      const element = enumerator.getNext();
      Assert.ok(element);
      Assert.strictEqual(element.key, pair[0]);
      Assert.strictEqual(element.value, pair[1]);
    }

    Assert.strictEqual(enumerator.hasMoreElements(), false);
    Assert.throws(() => enumerator.getNext(), /NS_ERROR_FAILURE/);
  }

  // Test enumeration without specifying "from" and "to" keys, which should
  // enumerate all of the pairs in the database.  This test does so explicitly
  // by passing "null", "undefined" or "" (empty string) arguments
  // for those parameters. The iterator test below also tests this implicitly
  // by not specifying arguments for those parameters.
  await test(null, null, [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);
  await test(undefined, undefined, [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // The implementation doesn't distinguish between a null/undefined value
  // and an empty string, so enumerating pairs from "" to "" has the same effect
  // as enumerating pairs without specifying from/to keys: it enumerates
  // all of the pairs in the database.
  await test("", "", [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // Test enumeration from a key that doesn't exist and is lexicographically
  // less than the least key in the database, which should enumerate
  // all of the pairs in the database.
  await test("aaaaa", null, [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // Test enumeration from a key that doesn't exist and is lexicographically
  // greater than the first key in the database, which should enumerate pairs
  // whose key is greater than or equal to the specified key.
  await test("ccccc", null, [
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // Test enumeration from a key that does exist, which should enumerate pairs
  // whose key is greater than or equal to that key.
  await test("int-key", null, [
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // Test enumeration from a key that doesn't exist and is lexicographically
  // greater than the greatest test key in the database, which should enumerate
  // none of the pairs in the database.
  await test("zzzzz", null, []);

  // Test enumeration to a key that doesn't exist and is lexicographically
  // greater than the greatest test key in the database, which should enumerate
  // all of the pairs in the database.
  await test(null, "zzzzz", [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
    ["string-key", "Héllo, wőrld!"],
  ]);

  // Test enumeration to a key that doesn't exist and is lexicographically
  // less than the greatest test key in the database, which should enumerate
  // pairs whose key is less than the specified key.
  await test(null, "ppppp", [
    ["bool-key", true],
    ["double-key", 56.78],
    ["int-key", 1234],
  ]);

  // Test enumeration to a key that does exist, which should enumerate pairs
  // whose key is less than that key.
  await test(null, "int-key", [
    ["bool-key", true],
    ["double-key", 56.78],
  ]);

  // Test enumeration to a key that doesn't exist and is lexicographically
  // less than the least key in the database, which should enumerate
  // none of the pairs in the database.
  await test(null, "aaaaa", []);

  // Test enumeration between intermediate keys that don't exist, which should
  // enumerate the pairs whose keys lie in between them.
  await test("ggggg", "ppppp", [
    ["int-key", 1234],
  ]);

  // Test enumeration from a key that exists to the same key, which shouldn't
  // enumerate any pairs, because the "to" key is exclusive.
  await test("int-key", "int-key", []);

  // Test enumeration from a greater key to a lesser one, which should
  // enumerate none of the pairs in the database, even if the reverse ordering
  // would enumerate some pairs.  Consumers are responsible for ordering
  // the "from" and "to" keys such that "from" is less than or equal to "to".
  await test("ppppp", "ccccc", []);
  await test("int-key", "ccccc", []);
  await test("ppppp", "int-key", []);

  const actual = {};
  for (const { key, value } of await database.enumerate()) {
    actual[key] = value;
  }
  Assert.deepEqual(actual, {
    "bool-key": true,
    "double-key": 56.78,
    "int-key": 1234,
    "string-key": "Héllo, wőrld!",
  });

  await database.delete("int-key");
  await database.delete("double-key");
  await database.delete("string-key");
  await database.delete("bool-key");
});
