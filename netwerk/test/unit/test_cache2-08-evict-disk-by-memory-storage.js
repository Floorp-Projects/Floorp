function run_test()
{
  do_get_profile();

  asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      var storage = getCacheStorage("memory");
      // Have to fail
      storage.asyncDoomURI(createURI("http://a/"), "",
        new EvictionCallback(false, function() {
          finish_cache2_test();
        })
      );
    })
  );

  do_test_pending();
}
