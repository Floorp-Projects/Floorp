// Tests for the test functions in head.js

function test() {
  waitForExplicitFinish();
  testWaitForAndContinue();
}

function testWaitForAndContinue() {
  waitForAndContinue(function() {
    ok(true, "continues on success")
    testWaitForAndContinue2();
  }, function() true);
}

function testWaitForAndContinue2() {
  waitForAndContinue(function() {
    ok(true, "continues on failure");
    finish();
  }, function() false);
}
