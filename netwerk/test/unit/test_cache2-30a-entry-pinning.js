function run_test()
{
  do_get_profile();
  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test checks only cache2 specific behavior.");
    return;
  }

  // Open for write, write
  asyncOpenCacheEntry("http://a/", "pin", Ci.nsICacheStorage.OPEN_TRUNCATE, LoadContextInfo.default,
    new OpenCallback(NEW|WAITFORWRITE, "a1m", "a1d", function(entry) {
      // Open for read and check
      asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.default,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {

          // Now clear the whole cache
          get_cache_service().clear();

          // The pinned entry should be intact
          asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.default,
            new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
              finish_cache2_test();
            })
          );

        })
      );
    })
  );

  do_test_pending();
}
