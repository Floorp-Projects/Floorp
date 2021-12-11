function test() {
  waitForExplicitFinish();
  ok(true, "ok called");
  executeSoon(function() {
    expectUncaughtException();
    throw new Error("this is a deliberately thrown exception");
  });
  executeSoon(function() {
    finish();
  });
}
