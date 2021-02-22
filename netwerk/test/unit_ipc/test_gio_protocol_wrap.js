//
// Run test script in content process instead of chrome (xpcshell's default)
//
//

function run_test() {
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefBranch
  );
  prefs.setCharPref("network.gio.supported-protocols", "localtest:,recent:");

  do_await_remote_message("gio-allow-test-protocols").then(port => {
    do_send_remote_message("gio-allow-test-protocols-done");
  });

  run_test_in_child("../unit/test_gio_protocol.js");
}

registerCleanupFunction(() => {
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefBranch
  );
  prefs.clearUserPref("network.gio.supported-protocols");
});
