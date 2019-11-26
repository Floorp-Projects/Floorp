const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// needs to be rooted
var cacheFlushObserver = {
  observe() {
    cacheFlushObserver = null;
    do_send_remote_message("flushed");
  },
};

var currentThread = Services.tm.currentThread;

function run_test() {
  Services.prefs.setBoolPref("browser.cache.cache_isolation", false);
  do_get_profile();
  do_await_remote_message("flush").then(() => {
    Services.cache2
      .QueryInterface(Ci.nsICacheTesting)
      .flush(cacheFlushObserver);
  });
  run_test_in_child("../unit/test_alt-data_closeWithStatus.js");
}
