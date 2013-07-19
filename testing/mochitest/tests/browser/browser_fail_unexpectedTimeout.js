function test() {
  function message() {
    info("This should delay timeout");
  }
  function end() {
    ok(true, "Should have not timed out, but notified long running test");
    finish();
  }
  waitForExplicitFinish();
  setTimeout(message, 20000);
  setTimeout(end, 40000);
}
