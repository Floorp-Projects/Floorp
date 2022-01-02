"use strict";

function run_test() {
  // Just check the contract ID alias works well.
  try {
    var serviceA = Services.cache2;
    Assert.ok(serviceA);
    var serviceB = Services.cache2;
    Assert.ok(serviceB);

    Assert.equal(serviceA, serviceB);
  } catch (ex) {
    do_throw("Cannot instantiate cache storage service: " + ex);
  }
}
