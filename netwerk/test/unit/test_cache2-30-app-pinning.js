function run_test()
{
  do_get_profile();

  var applci = LoadContextInfo.custom(false, false, 1001, false);

  // Open for write, write
  asyncOpenCacheEntry("http://a/", "pin", Ci.nsICacheStorage.OPEN_NORMALLY, applci,
    new OpenCallback(NEW|WAITFORWRITE, "a1m", "a1d", function(entry) {
      // Open for read and check
      asyncOpenCacheEntry("http://a/", "pin", Ci.nsICacheStorage.OPEN_NORMALLY, applci,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {

          // Now clear the whole cache
          get_cache_service().clear();

          // The pinned entry should be intact
          asyncOpenCacheEntry("http://a/", "pin", Ci.nsICacheStorage.OPEN_NORMALLY, applci,
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
