const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function run_test() {
  do_get_profile();
  run_test_in_child("../unit/test_cache-entry-id.js");

  do_await_remote_message("flush").then(() => {
    let p = new Promise(resolve => {
      Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(resolve);
    });
    p.then(() => {
      do_send_remote_message("flushed");
    });
  });
}
