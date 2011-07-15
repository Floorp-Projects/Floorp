function test() {
  waitForExplicitFinish();
  ok(true, "ok called");
  executeSoon(function() {
    expectUncaughtException();
    throw "uncaught exception";
  });
  executeSoon(function() {
    finish();
  });
}
