function test() {
  waitForExplicitFinish();
  function done() {
    ok(true, "timeout ran");
    finish();
  }
  setTimeout(done, 500);
}
