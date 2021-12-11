"use strict";

function run_test() {
  do_get_profile();

  var svc = get_cache_service();
  svc.clear();

  var storage = getCacheStorage("disk");
  storage.asyncVisitStorage(
    new VisitCallback(0, 0, [], function() {
      finish_cache2_test();
    }),
    true
  );

  do_test_pending();
}
