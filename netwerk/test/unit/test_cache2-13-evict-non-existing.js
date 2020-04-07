"use strict";

function run_test() {
  do_get_profile();

  var storage = getCacheStorage("disk");
  storage.asyncDoomURI(
    createURI("http://non-existing/"),
    "",
    new EvictionCallback(false, function() {
      finish_cache2_test();
    })
  );

  do_test_pending();
}
