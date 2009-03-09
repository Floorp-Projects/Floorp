function test() {
  function end() {
    ok(true, "didn't time out?");
    finish();
  }
  waitForExplicitFinish();
  setTimeout(end, 40000);
}
