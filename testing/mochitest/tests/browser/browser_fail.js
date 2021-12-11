setExpectedFailuresForSelfTest(6);

function test() {
  ok(false, "fail ok");
  is(true, false, "fail is");
  isnot(true, true, "fail isnot");
  todo(true, "fail todo");
  todo_is(true, true, "fail todo_is");
  todo_isnot(true, false, "fail todo_isnot");
}
