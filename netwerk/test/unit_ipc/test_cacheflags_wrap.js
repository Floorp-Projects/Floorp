//
// Run test script in content process instead of chrome (xpcshell's default)
//

function run_test() {
  do_get_profile();
  run_test_in_child("../unit/test_cacheflags.js");
}
