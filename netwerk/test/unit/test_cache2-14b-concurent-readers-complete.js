function run_test()
{
  do_get_profile();

  asyncOpenCacheEntry("http://x/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "x1m", "x1d", function(entry) {
      // nothing to do here, we expect concurent callbacks to get
      // all notified, then the test finishes
    })
  );

  var mc = new MultipleCallbacks(3, finish_cache2_test);

  var order = 0;

  asyncOpenCacheEntry("http://x/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NORMAL|COMPLETE, "x1m", "x1d", function(entry) {
      ++order;
      do_check_eq(order, newCacheBackEndUsed() ? 3 : 1);
      mc.fired();
    })
  );
  asyncOpenCacheEntry("http://x/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NORMAL, "x1m", "x1d", function(entry) {
      ++order;
      do_check_eq(order, newCacheBackEndUsed() ? 1 : 2);
      mc.fired();
    })
  );
  asyncOpenCacheEntry("http://x/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NORMAL, "x1m", "x1d", function(entry) {
      ++order;
      do_check_eq(order, newCacheBackEndUsed() ? 2 : 3);
      mc.fired();
    })
  );

  do_test_pending();
}
