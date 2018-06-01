/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_metadata() {
  await PlacesUtils.metadata.set("test/integer", 123);
  Assert.strictEqual(await PlacesUtils.metadata.get("test/integer"), 123,
    "Should store new integer value");

  await PlacesUtils.metadata.set("test/double", 123.45);
  Assert.strictEqual(await PlacesUtils.metadata.get("test/double"), 123.45,
    "Should store new double value");
  await PlacesUtils.metadata.set("test/double", 567.89);
  Assert.strictEqual(await PlacesUtils.metadata.get("test/double"), 567.89,
    "Should update existing double value");

  await PlacesUtils.metadata.set("test/boolean", false);
  Assert.strictEqual(await PlacesUtils.metadata.get("test/boolean"), false,
    "Should store new Boolean value");
  await PlacesUtils.metadata.set("test/boolean", true);
  Assert.strictEqual(await PlacesUtils.metadata.get("test/boolean"), true,
    "Should update existing Boolean value");

  await PlacesUtils.metadata.set("test/string", "hi");
  Assert.equal(await PlacesUtils.metadata.get("test/string"), "hi",
    "Should store new string value");
  await PlacesUtils.metadata.cache.clear();
  Assert.equal(await PlacesUtils.metadata.get("test/string"), "hi",
    "Should return string value after clearing cache");

  await Assert.rejects(PlacesUtils.metadata.get("test/nonexistent"),
    /No data stored for key test\/nonexistent/,
    "Should reject for a non-existent key and no default value.");
  Assert.equal(await PlacesUtils.metadata.get("test/nonexistent", "defaultValue"), "defaultValue",
    "Should return the default value for a non-existent key.");

  // Values are untyped; it's OK to store a value of a different type for the
  // same key.
  await PlacesUtils.metadata.set("test/string", 111);
  Assert.strictEqual(await PlacesUtils.metadata.get("test/string"), 111,
    "Should replace string with integer");
  await PlacesUtils.metadata.set("test/string", null);
  await Assert.rejects(PlacesUtils.metadata.get("test/string"),
    /No data stored for key test\/string/,
    "Should clear value when setting to NULL");

  await PlacesUtils.metadata.delete("test/string", "test/boolean");
  await Assert.rejects(PlacesUtils.metadata.get("test/string"),
    /No data stored for key test\/string/,
    "Should delete string value");
  await Assert.rejects(PlacesUtils.metadata.get("test/boolean"),
    /No data stored for key test\/boolean/,
    "Should delete Boolean value");
  Assert.strictEqual(await PlacesUtils.metadata.get("test/integer"), 123,
    "Should keep undeleted integer value");

  await PlacesTestUtils.clearMetadata();
  await Assert.rejects(PlacesUtils.metadata.get("test/integer"),
    /No data stored for key test\/integer/,
    "Should clear integer value");
  await Assert.rejects(PlacesUtils.metadata.get("test/double"),
    /No data stored for key test\/double/,
    "Should clear double value");
});

add_task(async function test_metadata_canonical_keys() {
  await PlacesUtils.metadata.set("Test/Integer", 123);
  Assert.strictEqual(await PlacesUtils.metadata.get("tEsT/integer"), 123,
    "New keys should be case-insensitive");
  await PlacesUtils.metadata.set("test/integer", 456);
  Assert.strictEqual(await PlacesUtils.metadata.get("TEST/INTEGER"), 456,
    "Existing keys should be case-insensitive");

  await Assert.rejects(PlacesUtils.metadata.set("", 123),
    /Invalid metadata key/, "Should reject empty keys");
  await Assert.rejects(PlacesUtils.metadata.get(123),
    /Invalid metadata key/, "Should reject numeric keys");
  await Assert.rejects(PlacesUtils.metadata.delete(true),
    /Invalid metadata key/, "Should reject Boolean keys");
  await Assert.rejects(PlacesUtils.metadata.set({}),
    /Invalid metadata key/, "Should reject object keys");
  await Assert.rejects(PlacesUtils.metadata.get(null),
    /Invalid metadata key/, "Should reject null keys");
  await Assert.rejects(PlacesUtils.metadata.delete("!@#$"),
    /Invalid metadata key/, "Should reject keys with invalid characters");
});

add_task(async function test_metadata_blobs() {
  let blob = new Uint8Array([1, 2, 3]);
  await PlacesUtils.metadata.set("test/blob", blob);

  let sameBlob = await PlacesUtils.metadata.get("test/blob");
  Assert.equal(ChromeUtils.getClassName(sameBlob), "Uint8Array",
    "Should cache typed array for blob value");
  Assert.deepEqual(sameBlob, blob,
    "Should store new blob value");

  info("Remove blob from cache");
  await PlacesUtils.metadata.cache.clear();

  let newBlob = await PlacesUtils.metadata.get("test/blob");
  Assert.equal(ChromeUtils.getClassName(newBlob), "Uint8Array",
    "Should inflate blob into typed array");
  Assert.deepEqual(newBlob, blob,
    "Should return same blob after clearing cache");

  await PlacesTestUtils.clearMetadata();
});

add_task(async function test_metadata_arrays() {
  let array = [1, 2, 3, "\u2713 \u00E0 la mode"];
  await PlacesUtils.metadata.set("test/array", array);

  let sameArray = await PlacesUtils.metadata.get("test/array");
  Assert.ok(Array.isArray(sameArray), "Should cache array for array value");
  Assert.deepEqual(sameArray, array,
    "Should store new array value");

  info("Remove array from cache");
  await PlacesUtils.metadata.cache.clear();

  let newArray = await PlacesUtils.metadata.get("test/array");
  Assert.ok(Array.isArray(newArray), "Should inflate into array");
  Assert.deepEqual(newArray, array,
    "Should return same array after clearing cache");

  await PlacesTestUtils.clearMetadata();
});

add_task(async function test_metadata_objects() {
  let object = {foo: 123, bar: "test", meow: "\u2713 \u00E0 la mode"};
  await PlacesUtils.metadata.set("test/object", object);

  let sameObject = await PlacesUtils.metadata.get("test/object");
  Assert.equal(typeof sameObject, "object", "Should cache object for object value");
  Assert.deepEqual(sameObject, object,
    "Should store new object value");

  info("Remove object from cache");
  await PlacesUtils.metadata.cache.clear();

  let newObject = await PlacesUtils.metadata.get("test/object");
  Assert.equal(typeof newObject, "object", "Should inflate into object");
  Assert.deepEqual(newObject, object,
    "Should return same object after clearing cache");

  await PlacesTestUtils.clearMetadata();
});

add_task(async function test_metadata_unparsable() {
  await PlacesUtils.withConnectionWrapper("test_medata", db => {
    let data = PlacesUtils.metadata._base64Encode("{hjjkhj}");

    return db.execute(`
      INSERT INTO moz_meta (key, value)
      VALUES ("test/unparsable", "data:application/json;base64,${data}")
    `);
  });

  await Assert.rejects(PlacesUtils.metadata.get("test/unparsable"),
    /SyntaxError: JSON.parse/,
    "Should reject for an unparsable value with no default");
  Assert.deepEqual(await PlacesUtils.metadata.get("test/unparsable", {foo: 1}),
    {foo: 1}, "Should return the default when encountering an unparsable value.");

  await PlacesTestUtils.clearMetadata();
});
