function test() {
  waitForExplicitFinish();
  function done() {
    ok(true, "timeout ran");
    finish();
  }

  ok(OpenBrowserWindow(), "opened browser window");
  // and didn't close it!

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(done, 10000);
}
