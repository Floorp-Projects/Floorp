print("Define some functions in well defined line positions for the test");
function foo(v) {
  return bar(v + 1);
} // line 3
function bar(v) {
  return baz(v + 1);
} // line 6
function baz(v) {
  throw new Error(v + 1);
} // line 9

print("Make sure lazy constructor calling/assignment works");
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");

function run_test() {
  print(
    "Make sure functions, arguments, files are pretty printed in the trace"
  );
  let trace = "";
  try {
    foo(0);
  } catch (ex) {
    trace = Log.stackTrace(ex);
  }
  print(`Got trace: ${trace}`);
  Assert.notEqual(trace, "");

  let bazPos = trace.indexOf("baz@test_Log_stackTrace.js:9");
  let barPos = trace.indexOf("bar@test_Log_stackTrace.js:6");
  let fooPos = trace.indexOf("foo@test_Log_stackTrace.js:3");
  print(`String positions: ${bazPos} ${barPos} ${fooPos}`);

  print("Make sure the desired messages show up");
  Assert.ok(bazPos >= 0);
  Assert.ok(barPos > bazPos);
  Assert.ok(fooPos > barPos);
}
