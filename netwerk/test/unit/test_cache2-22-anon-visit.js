Components.utils.import('resource://gre/modules/LoadContextInfo.jsm');

function run_test()
{
  do_get_profile();

  function checkNewBackEnd()
  {
    var storage = getCacheStorage("disk", LoadContextInfo.default);
    storage.asyncVisitStorage(
      new VisitCallback(1, 12, ["http://an2/"], function() {
        storage = getCacheStorage("disk", LoadContextInfo.anonymous);
        storage.asyncVisitStorage(
          new VisitCallback(1, 12, ["http://an2/"], function() {
            finish_cache2_test();
          }),
          true
        );
      }),
      true
    );
  }

  function checkOldBackEnd()
  {
    syncWithCacheIOThread(function() {
      var storage = getCacheStorage("disk", LoadContextInfo.default);
      storage.asyncVisitStorage(
        new VisitCallback(2, 24, ["http://an2/", "anon&uri=http://an2/"], function() {
          storage = getCacheStorage("disk", LoadContextInfo.anonymous);
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
  }

  var mc = new MultipleCallbacks(2, newCacheBackEndUsed() ? checkNewBackEnd : checkOldBackEnd, !newCacheBackEndUsed());

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
