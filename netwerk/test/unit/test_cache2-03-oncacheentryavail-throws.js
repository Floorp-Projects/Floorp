function run_test()
{
  do_get_profile();

  // Open but let OCEA throw
  asyncOpenCacheEntry("http://c/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW|THROWAVAIL, null, null, function(entry) {
      // Try it again, should go
      asyncOpenCacheEntry("http://c/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NEW, "c1m", "c1d", function(entry) {
          // ...and check
          asyncOpenCacheEntry("http://c/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
            new OpenCallback(false, "c1m", "c1d", function(entry) {
              finish_cache2_test();
            })
          );
        })
      );
    })
  );

  do_test_pending();
}

