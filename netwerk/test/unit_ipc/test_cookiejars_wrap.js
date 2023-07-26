function run_test() {
  // Allow all cookies.
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.skip_browsing_context_check_in_parent_for_testing",
    true
  );
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  run_test_in_child("../unit/test_cookiejars.js");
}
