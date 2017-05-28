function test() {
  function end() {
    throw "thrown exception";
  }
  waitForExplicitFinish();
  setTimeout(end, 1000);
}
