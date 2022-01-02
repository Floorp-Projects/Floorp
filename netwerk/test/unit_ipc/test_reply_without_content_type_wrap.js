//
// Run test script in content process instead of chrome (xpcshell's default)
//

function run_test() {
  run_test_in_child("../unit/test_reply_without_content_type.js");
}
