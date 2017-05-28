setExpectedFailuresForSelfTest(1);

function test() {
  waitForExplicitFinish();
  executeSoon(() => {
    ok(false, "fail");
    finish();
  });
}
