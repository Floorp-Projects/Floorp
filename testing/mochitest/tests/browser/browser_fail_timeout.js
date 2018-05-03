function test() {
  function end() {
    ok(false, "should have timed out");
    finish();
  }
  waitForExplicitFinish();
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(end, 40000);
}
