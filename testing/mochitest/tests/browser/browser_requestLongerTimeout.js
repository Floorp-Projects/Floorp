function test() {
  requestLongerTimeout(2);
  function end() {
    ok(true, "should not time out");
    finish();
  }
  waitForExplicitFinish();
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(end, 40000);
}
