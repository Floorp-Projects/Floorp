//
// Run test script in content process instead of chrome (xpcshell's default)
//

Cu.import("resource://gre/modules/Services.jsm");

function run_test() {
  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref("network.cookie.ipc.sync", true);
  run_test_in_child("../unit/test_cookie_header.js");
}
