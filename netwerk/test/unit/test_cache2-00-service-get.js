function run_test()
{
  // Just check the contract ID alias works well.
  try {
    var serviceA = Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                             .getService(Components.interfaces.nsICacheStorageService);
    Assert.ok(serviceA);
    var serviceB = Components.classes["@mozilla.org/network/cache-storage-service;1"]
                             .getService(Components.interfaces.nsICacheStorageService);
    Assert.ok(serviceB);

    Assert.equal(serviceA, serviceB);
  } catch (ex) {
    do_throw("Cannot instantiate cache storage service: " + ex);
  }
}
