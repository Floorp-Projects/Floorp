const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

//
// Run test script in content process instead of chrome (xpcshell's default)
//
//
function run_test() {
  do_await_remote_message("disable-ports").then(_ => {
    Services.prefs.setCharPref("network.security.ports.banned", "65400");
    do_send_remote_message("disable-ports-done");
  });
  run_test_in_child("../unit/test_redirect-caching_failure.js");
}
