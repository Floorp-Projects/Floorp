function test() {
  waitForExplicitFinish();
  function done() {
    ok(true, "timeout ran");
    finish();
  }
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(done, 500);
}
