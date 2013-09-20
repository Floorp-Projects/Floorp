function run_test()
{
  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test doesn't run when the old cache back end is used since the behavior is different");
    return;
  }

  do_get_profile();

  // Open for write, write
  asyncOpenCacheEntry("http://r200/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "200m1", "200part1a-", function(entry) {
      // Open normally but wait for validation from the server
      asyncOpenCacheEntry("http://r200/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(PARTIAL, "200m1", "200part1a-", function(entry) {
          // emulate 200 from the server, i.e. recreate the entry, resume transaction and
          // write new content to the output stream
          (new OpenCallback(NEW|WAITFORWRITE|RECREATE, "200m2", "200part1b--part2b", function(entry) {
            entry.setValid();
          })).onCacheEntryAvailable(entry, true, null, Cr.NS_OK);
        })
      );

      var mc = new MultipleCallbacks(3, finish_cache2_test);

      asyncOpenCacheEntry("http://r200/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "200m2", "200part1b--part2b", function(entry) {
          mc.fired();
        })
      );
      asyncOpenCacheEntry("http://r200/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "200m2", "200part1b--part2b", function(entry) {
          mc.fired();
        })
      );
      asyncOpenCacheEntry("http://r200/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "200m2", "200part1b--part2b", function(entry) {
          mc.fired();
        })
      );
    })
  );

  do_test_pending();
}
