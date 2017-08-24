Components.utils.import('resource://gre/modules/LoadContextInfo.jsm');

function run_test()
{
  do_get_profile();

  var mc = new MultipleCallbacks(2, function() {
    var storage = getCacheStorage("disk", LoadContextInfo.default);
    storage.asyncVisitStorage(
      new VisitCallback(1, 1024, ["http://an2/"], function() {
        storage = getCacheStorage("disk", LoadContextInfo.anonymous);
        storage.asyncVisitStorage(
          new VisitCallback(1, 1024, ["http://an2/"], function() {
            finish_cache2_test();
          }),
          true
        );
      }),
      true
    );
  });


  asyncOpenCacheEntry("http://an2/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.default,
    new OpenCallback(NEW|WAITFORWRITE, "an2", "an2", function(entry) {
      mc.fired();
    })
  );

  asyncOpenCacheEntry("http://an2/", "disk", Ci.nsICacheStorage.OPEN_NORMALLY, LoadContextInfo.anonymous,
    new OpenCallback(NEW|WAITFORWRITE, "an2", "an2", function(entry) {
      mc.fired();
    })
  );

  do_test_pending();
}
