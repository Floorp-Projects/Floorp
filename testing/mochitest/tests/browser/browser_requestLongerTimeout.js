function test() {
  requestLongerTimeout(2);
  function end() {
    ok(true, "should not time out");
    finish();
  }
  waitForExplicitFinish();
  setTimeout(end, 40000);
}
