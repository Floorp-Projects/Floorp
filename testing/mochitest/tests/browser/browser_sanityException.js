function test() {
  ok(true, "ok called");
  expectUncaughtException();
  throw new Error("this is a deliberately thrown exception");
}
