setExpectedFailuresForSelfTest(1);

function test() {
  throw new Error("thrown exception");
}
