function exitPB()
{
  var obsvc = Cc["@mozilla.org/observer-service;1"].
    getService(Ci.nsIObserverService);
  obsvc.notifyObservers(null, "last-pb-context-exited");
}

function run_test()
{
  do_get_profile();

  // Store PB entry
  asyncOpenCacheEntry("http://p1/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.private,
    new OpenCallback(NEW, "p1m", "p1d", function(entry) {
      asyncOpenCacheEntry("http://p1/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.private,
        new OpenCallback(NORMAL, "p1m", "p1d", function(entry) {
          // Check it's there
          syncWithCacheIOThread(function() {
            var storage = getCacheStorage("disk", LoadContextInfo.private);
            storage.asyncVisitStorage(
              new VisitCallback(1, 12, ["http://p1/"], function() {
                // Simulate PB exit
                exitPB();
                // Check the entry is gone
                storage.asyncVisitStorage(
                  new VisitCallback(0, 0, [], function() {
                    finish_cache2_test();
                  }),
                  true
                );
              }),
              true
            );
          });
        })
      );
    })
  );

  do_test_pending();
}
