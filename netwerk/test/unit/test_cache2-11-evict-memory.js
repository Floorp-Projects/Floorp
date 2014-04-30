function run_test()
{
  do_get_profile();

  var storage = getCacheStorage("memory");
  var mc = new MultipleCallbacks(3, function() {
    storage.asyncEvictStorage(
      new EvictionCallback(true, function() {
        storage.asyncVisitStorage(
          new VisitCallback(0, 0, [], function() {
            var storage = getCacheStorage("disk");
            storage.asyncVisitStorage(
              new VisitCallback(2, 24, ["http://a/", "http://b/"], function() {
                finish_cache2_test();
              }),
              true
            );
          }),
          true
        );
      })
    );
  }, !newCacheBackEndUsed());

  asyncOpenCacheEntry("http://mem1/", "memory", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "m2m", "m2d", function(entry) {
      asyncOpenCacheEntry("http://mem1/", "memory", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "m2m", "m2d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  do_test_pending();
}
