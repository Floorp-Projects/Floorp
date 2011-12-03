function test() {
  waitForExplicitFinish();
  ok(true, "ok called");
  executeSoon(function() {
    expectUncaughtException();
    throw "this is a deliberately thrown exception";
  });
  executeSoon(function() {
    finish();
  });
}
