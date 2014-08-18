function run_test()
{
  do_get_profile();

  var storage = getCacheStorage("disk");
  var mc = new MultipleCallbacks(4, function() {
    // Method asyncVisitStorage() gets the data from index on Cache I/O thread
    // with INDEX priority, so it is ensured that index contains information
    // about all pending writes. However, OpenCallback emulates network latency
    // by postponing the writes using do_execute_soon. We must do the same here
    // to make sure that all writes are posted to Cache I/O thread before we
    // visit the storage.
    do_execute_soon(function() {
      syncWithCacheIOThread(function() {

        var expectedConsumption = newCacheBackEndUsed()
          ? 4096
          : 48;

        storage.asyncVisitStorage(
          // Test should store 4 entries
          new VisitCallback(4, expectedConsumption, ["http://a/", "http://b/", "http://c/", "http://d/"], function() {
            storage.asyncVisitStorage(
              // Still 4 entries expected, now don't walk them
              new VisitCallback(4, expectedConsumption, null, function() {
                finish_cache2_test();
              }),
              false
            );
          }),
          true
        );
      });
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
