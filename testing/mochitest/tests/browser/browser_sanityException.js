function test() {
  ok(true, "ok called");
  expectUncaughtException();
  throw "this is a deliberately thrown exception";
}
