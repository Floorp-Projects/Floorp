function test() {
  function message() {
    info("This should delay timeout");
  }
  function end() {
    ok(true, "Should have not timed out, but notified long running test");
    finish();
  }
  waitForExplicitFinish();
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(message, 20000);
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(end, 40000);
}
