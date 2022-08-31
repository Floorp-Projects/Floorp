/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Realm } = ChromeUtils.import(
  "chrome://remote/content/webdriver-bidi/Realm.jsm"
);

add_test(function test_id() {
  const realm1 = new Realm();
  const id1 = realm1.id;
  Assert.equal(typeof id1, "string");

  const realm2 = new Realm();
  const id2 = realm2.id;
  Assert.equal(typeof id2, "string");

  Assert.notEqual(id1, id2, "Ids for different realms are different");

  run_next_test();
});

add_test(function test_handleObjectMap() {
  const realm = new Realm();

  // Test an unknown handle.
  Assert.equal(
    realm.getObjectForHandle("unknown"),
    undefined,
    "Unknown handles return undefined"
  );

  // Test creating a simple handle.
  const object = {};
  const handle = realm.getHandleForObject(object);
  Assert.equal(typeof handle, "string", "Created a valid handle");
  Assert.equal(
    realm.getObjectForHandle(handle),
    object,
    "Using the handle returned the original object"
  );

  // Test another handle for the same object.
  const secondHandle = realm.getHandleForObject(object);
  Assert.equal(typeof secondHandle, "string", "Created a valid handle");
  Assert.notEqual(secondHandle, handle, "A different handle was generated");
  Assert.equal(
    realm.getObjectForHandle(secondHandle),
    object,
    "Using the second handle also returned the original object"
  );

  // Test using the handles in another realm.
  const otherRealm = new Realm();
  Assert.equal(
    otherRealm.getObjectForHandle(handle),
    undefined,
    "A realm returns undefined for handles from another realm"
  );

  // Removing an unknown handle should not throw or have any side effect on
  // existing handles.
  realm.removeObjectHandle("unknown");
  Assert.equal(realm.getObjectForHandle(handle), object);
  Assert.equal(realm.getObjectForHandle(secondHandle), object);

  // Remove the second handle
  realm.removeObjectHandle(secondHandle);
  Assert.equal(
    realm.getObjectForHandle(handle),
    object,
    "The first handle is still resolving the object"
  );
  Assert.equal(
    realm.getObjectForHandle(secondHandle),
    undefined,
    "The second handle returns undefined after calling removeObjectHandle"
  );

  // Remove the original handle
  realm.removeObjectHandle(handle);
  Assert.equal(
    realm.getObjectForHandle(handle),
    undefined,
    "The first handle returns undefined as well"
  );

  run_next_test();
});
