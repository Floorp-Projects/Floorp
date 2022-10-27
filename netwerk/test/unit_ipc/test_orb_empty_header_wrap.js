//
// Run test script in content process instead of chrome (xpcshell's default)
//
// setup will be called before the child process tests
function run_test() {
  Services.prefs.setBoolPref("browser.opaqueResponseBlocking", true);
  run_test_in_child("../unit/test_orb_empty_header.js");
}
