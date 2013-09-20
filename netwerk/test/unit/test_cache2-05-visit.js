function run_test()
{
  do_get_profile();

  var storage = getCacheStorage("disk");
  var mc = new MultipleCallbacks(4, function() {
    syncWithCacheIOThread(function() {
      storage.asyncVisitStorage(
        // Test should store 4 entries
        new VisitCallback(4, 48, ["http://a/", "http://b/", "http://c/", "http://d/"], function() {
          storage.asyncVisitStorage(
            // Still 4 entries expected, now don't walk them
            new VisitCallback(4, 48, null, function() {
              finish_cache2_test();
            }),
            false
          );
        }),
        true
      );
    });
  }, !newCacheBackEndUsed());

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
    new OpenCallback(NEW, "b1m", "b1d", function(entry) {
      asyncOpenCacheEntry("http://b/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "b1m", "b1d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  asyncOpenCacheEntry("http://c/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "c1m", "c1d", function(entry) {
      asyncOpenCacheEntry("http://c/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "c1m", "c1d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  asyncOpenCacheEntry("http://d/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "d1m", "d1d", function(entry) {
      asyncOpenCacheEntry("http://d/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "d1m", "d1d", function(entry) {
          mc.fired();
        })
      );
    })
  );

  do_test_pending();
}
