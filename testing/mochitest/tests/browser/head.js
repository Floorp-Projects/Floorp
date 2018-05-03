var headVar = "I'm a var in head file";

function headMethod() {
  return true;
}

ok(true, "I'm a test in head file");

registerCleanupFunction(function() {
  ok(true, "I'm a cleanup function in head file");
  is(this.headVar, "I'm a var in head file", "Head cleanup function scope is correct");
});
