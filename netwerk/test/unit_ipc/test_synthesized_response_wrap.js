function run_test() {
  // ensure the cache service is prepped when running the test
  Cc["@mozilla.org/netwerk/cache-storage-service;1"].getService(
    Ci.nsICacheStorageService
  );

  run_test_in_child("../unit/test_synthesized_response.js");
}
