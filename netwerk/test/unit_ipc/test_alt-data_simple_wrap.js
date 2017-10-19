Cu.import("resource://gre/modules/Services.jsm");

// needs to be rooted
var cacheFlushObserver = { observe: function() {
  cacheFlushObserver = null;
  do_send_remote_message('flushed');
}};

function run_test() {
  do_get_profile();
  do_await_remote_message('flush').then(() => {
    Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver);
  });
  run_test_in_child("../unit/test_alt-data_simple.js");
}
