function run_test()
{
  do_get_profile();

  // Open for write, delay the actual write
  asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW|DONTFILL, "a1m", "a1d", function(entry) {
      var bypassed = false;

      // Open and bypass
      asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_BYPASS_IF_BUSY, null,
        new OpenCallback(NOTFOUND, "", "", function(entry) {
          Assert.ok(!bypassed);
          bypassed = true;
        })
      );

      // do_execute_soon for two reasons:
      // 1. we want finish_cache2_test call for sure after do_test_pending, but all the callbacks here
      //    may invoke synchronously
      // 2. precaution when the OPEN_BYPASS_IF_BUSY invocation become a post one day
      do_execute_soon(function() {
        Assert.ok(bypassed);
        finish_cache2_test();
      });
    })
  );

  do_test_pending();
}
