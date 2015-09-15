var testVar;

registerCleanupFunction(function() {
  ok(true, "I'm a cleanup function in test file");
  is(this.testVar, "I'm a var in test file", "Test cleanup function scope is correct");
});

function test() {
  is(headVar, "I'm a var in head file", "Head variables are set");
  ok(headMethod(), "Head methods are imported");
  testVar = "I'm a var in test file";
}
