function run_test()
{
  do_get_profile();

  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test doesn't run when the old cache back end is used since the behavior is different");
    return;
  }

  // Open for write, write but expect it to fail, since other callback will recreate (and doom)
  // the first entry before it opens output stream (note: in case of problems the DOOMED flag
  // can be removed, it is not the test failure when opening the output stream on recreated entry.
  asyncOpenCacheEntry("http://nv/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW|DOOMED, "v1m", "v1d", function(entry) {
      // Open for rewrite (don't validate), write different meta and data
      asyncOpenCacheEntry("http://nv/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NOTVALID|RECREATE, "v2m", "v2d", function(entry) {
          // And check...
          asyncOpenCacheEntry("http://nv/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
            new OpenCallback(NORMAL, "v2m", "v2d", function(entry) {
              finish_cache2_test();
            })
          );
        })
      );
    })
  );

  do_test_pending();
}
