const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

//
// Run test script in content process instead of chrome (xpcshell's default)
//

function run_test() {
  Services.prefs.setCharPref("network.security.ports.banned", "65400");
  run_test_in_child("../unit/test_redirect_failure.js");
}
