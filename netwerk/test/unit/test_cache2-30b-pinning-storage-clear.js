function run_test()
{
  do_get_profile();
  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test checks only cache2 specific behavior.");
    return;
  }
  var lci = LoadContextInfo.default;

  // Open a pinned entry for write, write
  asyncOpenCacheEntry("http://a/", "pin", Ci.nsICacheStorage.OPEN_TRUNCATE, lci,
    new OpenCallback(NEW|WAITFORWRITE, "a1m", "a1d", function(entry) {

      // Now clear the disk storage, that should leave the pinned  entry in the cache
      var diskStorage = getCacheStorage("disk", lci);
      diskStorage.asyncEvictStorage(null);

      // Open for read and check, it should still be there
      asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, lci,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {

          // Now clear the pinning storage, entry should be gone
          var pinningStorage = getCacheStorage("pin", lci);
          pinningStorage.asyncEvictStorage(null);

          asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, lci,
            new OpenCallback(NEW, "", "", function(entry) {
              finish_cache2_test();
            })
          );

        })
      );
    })
  );

  do_test_pending();
}
