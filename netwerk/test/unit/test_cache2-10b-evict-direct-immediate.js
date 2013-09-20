function run_test()
{
  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test doesn't run when the old cache back end is used since the behavior is different");
    return;
  }

  do_get_profile();

  asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW|DOOMED, "b1m", "b1d", function(entry) {
      entry.asyncDoom(
        new EvictionCallback(true, function() {
          finish_cache2_test();
        })
      );
    })
  );

  do_test_pending();
}
