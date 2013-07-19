function test() {
  function end() {
    ok(false, "should have timed out");
    finish();
  }
  waitForExplicitFinish();
  setTimeout(end, 40000);
}
