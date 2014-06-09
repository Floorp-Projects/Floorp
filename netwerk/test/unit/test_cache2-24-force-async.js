function run_test()
{
  do_get_profile();

  // Open for write, write, and wait for finishing it before notification to avoid concurrent write
  // since we want to get as much as possible the scenario when an entry is left in the pool
  // and a new consumer comes to open it later.
  var outOfAsyncOpen0 = false;
  asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY | Ci.nsICacheStorage.FORCE_ASYNC_CALLBACK, null,
    new OpenCallback(NEW|WAITFORWRITE, "a1m", "a1d", function(entry) {
      do_check_true(outOfAsyncOpen0);
      // Open for read, expect callback happen from inside asyncOpenURI
      var outOfAsyncOpen1 = false;
      asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        function(entry) {
          do_check_false(outOfAsyncOpen1);
          var outOfAsyncOpen2 = false;
          // Open for read, again should be sync
          asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
            function(entry) {
              do_check_false(outOfAsyncOpen2);
              var outOfAsyncOpen3 = false;
              // Open for read, expect callback happen from outside of asyncOpenURI
              asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY | Ci.nsICacheStorage.FORCE_ASYNC_CALLBACK, null,
                function(entry) {
                  do_check_true(outOfAsyncOpen3);
                  finish_cache2_test();
                }
              );
              outOfAsyncOpen3 = true;
            }
          );
          outOfAsyncOpen2 = true;
        }
      );
      outOfAsyncOpen1 = true;
    })
  );
  outOfAsyncOpen0 = true;

  do_test_pending();
}
