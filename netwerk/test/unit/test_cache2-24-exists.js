Components.utils.import('resource://gre/modules/LoadContextInfo.jsm');

function run_test()
{
  do_get_profile();

  var mc = new MultipleCallbacks(2, function() {
    var mem = getCacheStorage("memory");
    var disk = getCacheStorage("disk");

    Assert.ok(disk.exists(createURI("http://m1/"), ""));
    Assert.ok(mem.exists(createURI("http://m1/"), ""));
    Assert.ok(!mem.exists(createURI("http://m2/"), ""));
    Assert.ok(disk.exists(createURI("http://d1/"), ""));
    do_check_throws_nsIException(() => disk.exists(createURI("http://d2/"), ""), 'NS_ERROR_NOT_AVAILABLE');

    finish_cache2_test();
  });

  asyncOpenCacheEntry("http://d1/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.default,
    new OpenCallback(NEW | WAITFORWRITE, "meta", "data", function(entry) {
      mc.fired();
    })
  );

  asyncOpenCacheEntry("http://m1/", "memory", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.default,
    new OpenCallback(NEW | WAITFORWRITE, "meta", "data", function(entry) {
      mc.fired();
    })
  );

  do_test_pending();
}
