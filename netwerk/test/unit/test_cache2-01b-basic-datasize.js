function run_test()
{
  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW|WAITFORWRITE, "a1m", "a1d", function(entry) {
      // Open for read and check
      Assert.equal(entry.dataSize, 3);
      asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          // Open for rewrite (truncate), write different meta and data
          Assert.equal(entry.dataSize, 3);
          asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_TRUNCATE, null,
            new OpenCallback(NEW|WAITFORWRITE, "a2m", "a2d", function(entry) {
              // Open for read and check
              Assert.equal(entry.dataSize, 3);
              asyncOpenCacheEntry("http://a/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                new OpenCallback(NORMAL, "a2m", "a2d", function(entry) {
                  Assert.equal(entry.dataSize, 3);
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
