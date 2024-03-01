//
// Run test script in content process instead of chrome (xpcshell's default)
//
//

function run_test() {
  Services.prefs.setCharPref(
    "network.gio.supported-protocols",
    "localtest:,recent:"
  );

  do_await_remote_message("gio-allow-test-protocols").then(() => {
    do_send_remote_message("gio-allow-test-protocols-done");
  });

  run_test_in_child("../unit/test_gio_protocol.js");
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.gio.supported-protocols");
});
