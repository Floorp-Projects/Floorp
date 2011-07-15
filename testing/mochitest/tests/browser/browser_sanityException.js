function test() {
  ok(true, "ok called");
  expectUncaughtException();
  throw "uncaught exception";
}
