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

  // Values are untyped; it's OK to store a value of a different type for the
  // same key.
  await PlacesUtils.metadata.set("test/string", 111);
  Assert.strictEqual(await PlacesUtils.metadata.get("test/string"), 111,
    "Should replace string with integer");
  await PlacesUtils.metadata.set("test/string", null);
  Assert.strictEqual(await PlacesUtils.metadata.get("test/string"), null,
    "Should clear value when setting to NULL");

  await PlacesUtils.metadata.delete("test/string", "test/boolean");
  Assert.strictEqual(await PlacesUtils.metadata.get("test/string"), null,
    "Should delete string value");
  Assert.strictEqual(await PlacesUtils.metadata.get("test/boolean"), null,
    "Should delete Boolean value");
  Assert.strictEqual(await PlacesUtils.metadata.get("test/integer"), 123,
    "Should keep undeleted integer value");

  await PlacesTestUtils.clearMetadata();
  Assert.strictEqual(await PlacesUtils.metadata.get("test/integer"), null,
    "Should clear integer value");
  Assert.strictEqual(await PlacesUtils.metadata.get("test/double"), null,
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
  deepEqual(sameBlob, blob,
    "Should store new blob value");

  info("Remove blob from cache");
  await PlacesUtils.metadata.cache.clear();

  let newBlob = await PlacesUtils.metadata.get("test/blob");
  Assert.equal(ChromeUtils.getClassName(newBlob), "Uint8Array",
    "Should inflate blob into typed array");
  deepEqual(newBlob, blob,
    "Should return same blob after clearing cache");

  await PlacesTestUtils.clearMetadata();
});
