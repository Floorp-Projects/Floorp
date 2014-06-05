function run_test()
{
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY | Ci.nsICacheStorage.FORCE_ASYNC_CALLBACK, null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      // Open for read and check
      asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY | Ci.nsICacheStorage.FORCE_ASYNC_CALLBACK, null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          // Open for rewrite (truncate), write different meta and data
          asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_TRUNCATE | Ci.nsICacheStorage.FORCE_ASYNC_CALLBACK, null,
            new OpenCallback(NEW, "a2m", "a2d", function(entry) {
              // Open for read and check
              asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY | Ci.nsICacheStorage.FORCE_ASYNC_CALLBACK, null,
                new OpenCallback(NORMAL, "a2m", "a2d", function(entry) {
                  finish_cache2_test();
                })
              );
            })
          );
        })
      );
    })
  );

  do_test_pending();
}
