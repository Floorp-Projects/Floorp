function run_test()
{
  do_get_profile();

  if (!newCacheBackEndUsed()) {
    do_check_true(true, "This test doesn't run when the old cache back end is used since the behavior is different");
    return;
  }

  // First check how behaves the memory storage.

  asyncOpenCacheEntry("http://mem-first/", "memory", Ci.nsICacheStorage.OPEN_NORMALLY, null,
    new OpenCallback(NEW, "mem1-meta", "mem1-data", function(entryM1) {
      do_check_false(entryM1.persistent);
      asyncOpenCacheEntry("http://mem-first/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "mem1-meta", "mem1-data", function(entryM2) {
          do_check_false(entryM1.persistent);
          do_check_false(entryM2.persistent);

          // Now check the disk storage behavior.

          asyncOpenCacheEntry("http://disk-first/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
            // Must wait for write, since opening the entry as memory-only before the disk one
            // is written would cause NS_ERROR_NOT_AVAILABLE from openOutputStream when writing
            // this disk entry since it's doomed during opening of the memory-only entry for the same URL.
            new OpenCallback(NEW|WAITFORWRITE, "disk1-meta", "disk1-data", function(entryD1) {
              do_check_true(entryD1.persistent);
              // Now open the same URL as a memory-only entry, the disk entry must be doomed.
              asyncOpenCacheEntry("http://disk-first/", "memory", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                // This must be recreated
                new OpenCallback(NEW, "mem2-meta", "mem2-data", function(entryD2) {
                  do_check_true(entryD1.persistent);
                  do_check_false(entryD2.persistent);
                  // Check we get it back, even when opening via the disk storage
                  asyncOpenCacheEntry("http://disk-first/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                    new OpenCallback(NORMAL, "mem2-meta", "mem2-data", function(entryD3) {
                      do_check_true(entryD1.persistent);
                      do_check_false(entryD2.persistent);
                      do_check_false(entryD3.persistent);
                      finish_cache2_test();
                    })
                  );
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
