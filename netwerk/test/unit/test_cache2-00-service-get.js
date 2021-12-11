"use strict";

function run_test() {
  // Just check the contract ID alias works well.
  try {
    var serviceA = Cc[
      "@mozilla.org/netwerk/cache-storage-service;1"
    ].getService(Ci.nsICacheStorageService);
    Assert.ok(serviceA);
    var serviceB = Cc[
      "@mozilla.org/network/cache-storage-service;1"
    ].getService(Ci.nsICacheStorageService);
    Assert.ok(serviceB);

    Assert.equal(serviceA, serviceB);
  } catch (ex) {
    do_throw("Cannot instantiate cache storage service: " + ex);
  }
}
