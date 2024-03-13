//
// Run test script in content process instead of chrome (xpcshell's default)
//

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.http.http3.priority");
  Services.prefs.clearUserPref("network.http.priority_header.enabled");
  http3_clear_prefs();
});

// setup will be called before the child process tests
add_task(async function setup() {
  await http3_setup_tests("h3-29");
});

async function run_test() {
  // test priority urgency and incremental with priority disabled
  Services.prefs.setBoolPref("network.http.http3.priority", false);
  Services.prefs.setBoolPref("network.http.priority_header.enabled", false);
  run_test_in_child("../unit/test_http3_prio_disabled.js");
  run_next_test(); // only pumps next async task from this file
}
