function run_test()
{
  // Just check the contract ID alias works well.
  try {
    var serviceA = Components.classes["@mozilla.org/netwerk/cache-storage-service;1"]
                             .getService(Components.interfaces.nsICacheStorageService);
    do_check_true(serviceA);
    var serviceB = Components.classes["@mozilla.org/network/cache-storage-service;1"]
                             .getService(Components.interfaces.nsICacheStorageService);
    do_check_true(serviceB);

    do_check_eq(serviceA, serviceB);
  } catch (ex) {
    do_throw("Cannot instantiate cache storage service: " + ex);
  }
}
