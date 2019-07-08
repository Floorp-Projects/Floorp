const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function run_test() {
  // Allow all cookies.
  Services.prefs.setBoolPref(
    "network.cookieSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  run_test_in_child("../unit/test_cookiejars.js");
}
