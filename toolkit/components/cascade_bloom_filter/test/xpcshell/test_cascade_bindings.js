"use strict";

const CASCADE_CID = "@mozilla.org/cascade-filter;1";
const CASCADE_IID = Ci.nsICascadeFilter;
const CascadeFilter = Components.Constructor(CASCADE_CID, CASCADE_IID);

add_task(function CascadeFilter_uninitialized() {
  let filter = new CascadeFilter();
  Assert.throws(
    () => filter.has(""),
    e => e.result === Cr.NS_ERROR_NOT_INITIALIZED,
    "Cannot use has() if the filter is not initialized"
  );
});

add_task(function CascadeFilter_with_setFilterData() {
  let filter = new CascadeFilter();
  Assert.throws(
    () => filter.setFilterData(),
    e => e.result === Cr.NS_ERROR_XPC_NOT_ENOUGH_ARGS,
    "setFilterData without parameters should throw"
  );
  Assert.throws(
    () => filter.setFilterData(null),
    e => e.result === Cr.NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY,
    "setFilterData with null parameter is invalid"
  );
  Assert.throws(
    () => filter.setFilterData(new Uint8Array()),
    e => e.result === Cr.NS_ERROR_INVALID_ARG,
    "setFilterData with empty array is invalid"
  );

  // Test data based on rust_cascade's unit tests (bloom_v1_test_from_bytes),
  // with two bytes in front to have a valid format.
  const TEST_DATA = [1, 0, 1, 9, 0, 0, 0, 1, 0, 0, 0, 1, 0x41, 0];
  Assert.throws(
    () => filter.setFilterData(new Uint8Array(TEST_DATA.slice(1))),
    e => e.result === Cr.NS_ERROR_INVALID_ARG,
    "setFilterData with invalid data (missing head) is invalid"
  );
  Assert.throws(
    () => filter.setFilterData(new Uint8Array(TEST_DATA.slice(0, -1))),
    e => e.result === Cr.NS_ERROR_INVALID_ARG,
    "setFilterData with invalid data (missing tail) is invalid"
  );
  filter.setFilterData(new Uint8Array(TEST_DATA));
  Assert.equal(filter.has("this"), true, "has(this) should be true");
  Assert.equal(filter.has("that"), true, "has(that) should be true");
  Assert.equal(filter.has("other"), false, "has(other) should be false");
});
